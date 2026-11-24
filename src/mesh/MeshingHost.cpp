/*
 * MeshingHost.cpp
 *
 * The meshing pipeline in order (see Meshing.h): extraction, then the
 * finishing chain of MeshStages.h -- merge, culling, cleanup, repair,
 * orientation, colour -- and export. The GPU work is behind
 * OccupancyEvaluator; everything here is plain multithreaded host C++.
 */

#include "mesh/Meshing.h"
#include "mesh/GroundSplit.h"
#include "mesh/MeshCheck.h"
#include "mesh/MeshExport.h"
#include "mesh/MeshStages.h"
#include "mesh/MeshUV.h"
#include "mesh/Delaunay3D.h"
#include "mesh/MeshLog.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <string>

#include "core/Env.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

namespace mmsg = spirula::i18n::msg::mesh;

namespace {

// The mean length of the edges around each vertex.
static std::vector<float> mean_edge_length(const Mesh& mesh) {
    const size_t nv = mesh.V.size();
    std::vector<double> sum(nv, 0.0);
    std::vector<int> cnt(nv, 0);
    for (const auto& f : mesh.F)
        for (int k = 0; k < 3; ++k) {
            const auto& a = mesh.V[f[k]];
            const auto& b = mesh.V[f[(k + 1) % 3]];
            const double l = std::sqrt(double(a[0]-b[0])*(a[0]-b[0]) +
                                       double(a[1]-b[1])*(a[1]-b[1]) +
                                       double(a[2]-b[2])*(a[2]-b[2]));
            sum[f[k]] += l; sum[f[(k + 1) % 3]] += l;
            ++cnt[f[k]]; ++cnt[f[(k + 1) % 3]];
        }
    std::vector<float> out(nv, 0.0f);
    for (size_t v = 0; v < nv; ++v) if (cnt[v]) out[v] = (float)(sum[v] / cnt[v]);
    return out;
}

// The first merge only thins the raw marching-tetrahedra mesh before culling:
// edges stay within twice the local length, which bounds its rounds, and the
// surface moves at most a quarter of it, so thin parts need no probes yet.
constexpr float kRawMergeEdgeCap = 2.0f;
constexpr float kRawMergeTol = 0.25f;

// Probe depths along both normal directions, in pixel footprints (or edge
// lengths, if longer); the longer occupied run is the thickness, solid past it.
constexpr float kThicknessProbes[] = {0.5f, 1.5f, 4.0f, 10.0f};
constexpr int kNumProbes = (int)(sizeof(kThicknessProbes) / sizeof(kThicknessProbes[0]));

// Probe points of every vertex: [vertex][side][probe] xyz.
static std::vector<float> thickness_probes(const Mesh& mesh,
                                          const std::vector<float>& depth_unit) {
    const int nv = (int)mesh.V.size();
    std::vector<std::array<float,3>> nrm(nv, {0.0f, 0.0f, 0.0f});
    for (const auto& f : mesh.F) {
        const auto &a = mesh.V[f[0]], &b = mesh.V[f[1]], &c = mesh.V[f[2]];
        const float e1[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]};
        const float e2[3] = {c[0]-a[0], c[1]-a[1], c[2]-a[2]};
        const float n[3] = {e1[1]*e2[2] - e1[2]*e2[1], e1[2]*e2[0] - e1[0]*e2[2],
                            e1[0]*e2[1] - e1[1]*e2[0]};
        for (int k = 0; k < 3; ++k)
            for (int d = 0; d < 3; ++d) nrm[f[k]][d] += n[d];
    }
    std::vector<float> probes((size_t)nv * 2 * kNumProbes * 3);
    #pragma omp parallel for schedule(static)
    for (int v = 0; v < nv; ++v) {
        auto n = nrm[v];
        const float l = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (l > 0.0f) for (float& x : n) x /= l;
        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < kNumProbes; ++k)
                for (int d = 0; d < 3; ++d)
                    probes[(((size_t)v * 2 + side) * kNumProbes + k) * 3 + d] =
                        mesh.V[v][d] + (side ? 1.0f : -1.0f) * kThicknessProbes[k] *
                                           depth_unit[v] * n[d];
    }
    return probes;
}

// Per-vertex limits. After the raw pass, thickness is the longest run of
// occupied probes on either side; without camera renders it is not probed, as
// a 2dgs disc's bare density is empty off its plane.
static MergeLimits merge_limits(const Mesh& mesh, OccupancyEvaluator& ev,
                                const MeshingConfig& cfg, bool raw) {
    MergeLimits lim;
    const int nv = (int)mesh.V.size();
    const std::vector<float> L = mean_edge_length(mesh);
    std::vector<float> dens(nv, 0.0f), occ;
    std::vector<int> run(nv, kNumProbes);
    const bool probe = !raw && ev.has_render_cameras() && nv > 0;
    const bool colors = !raw && cfg.merge_color > 0.0f && nv > 0;
    if (colors) lim.rgb.resize(nv);
    if (probe) {
        // probes are spaced by the footprint: density gets a render pass of
        // its own, colour rides with the probes
        ev.view_texel_density(&mesh.V[0][0], nv, dens.data());
    } else if (nv > 0) {
        ev.merge_fields(&mesh.V[0][0], nv, dens.data(), colors ? &lim.rgb[0][0] : nullptr,
                        nullptr, 0, nullptr);
    }
    std::vector<float> scale(nv), depth_unit(nv);
    for (int v = 0; v < nv; ++v) {
        scale[v] = dens[v] > 0.0f ? 1.0f / dens[v] : L[v];
        depth_unit[v] = std::max(scale[v], L[v]);
    }
    if (probe) {
        const std::vector<float> probes = thickness_probes(mesh, depth_unit);
        occ.resize((size_t)nv * 2 * kNumProbes);
        std::vector<float> unused(nv);
        ev.merge_fields(&mesh.V[0][0], nv, unused.data(), colors ? &lim.rgb[0][0] : nullptr,
                        probes.data(), nv * 2 * kNumProbes, occ.data());
        for (int v = 0; v < nv; ++v) {
            int best = 0;
            for (int side = 0; side < 2; ++side) {
                const float* o = &occ[((size_t)v * 2 + side) * kNumProbes];
                int k = 0;
                while (k < kNumProbes && !(o[k] < cfg.iso)) ++k;
                best = std::max(best, k);
            }
            run[v] = best;
        }
    }

    lim.tol.resize(nv);
    lim.max_edge.resize(nv);
    for (int v = 0; v < nv; ++v) {
        const float thick = run[v] == kNumProbes ? std::numeric_limits<float>::infinity()
                                                 : kThicknessProbes[run[v]] * depth_unit[v];
        float tol = std::min(cfg.merge_flat_px * scale[v], cfg.merge_thin * thick);
        lim.max_edge[v] = cfg.merge_factor * cfg.merge_max_edge_px * scale[v];
        if (raw) {
            tol = std::min(tol, kRawMergeTol * L[v]);
            lim.max_edge[v] = std::min(lim.max_edge[v], kRawMergeEdgeCap * L[v]);
        }
        lim.tol[v] = cfg.merge_factor * tol;
    }
    if (colors) lim.color_tol = cfg.merge_color;
    return lim;
}

// The MeshCheck report for `mesh`; true when it found nothing.
static bool check_stage(const Mesh& mesh, const MeshingConfig& cfg, const std::string& tag) {
    MeshCheckLimits lim;
    lim.min_part_faces = cfg.floater_min_faces;
    lim.num_threads = cfg.num_threads;
    const MeshCheckReport r = check_mesh(mesh, lim);
    for (const std::string& line : format_mesh_check(r, lim, tag))
        mlog::out_raw(mlog::Stage::Debug, line);
    return r.defects() == 0;
}

// Visibility patches under this many vertices flip, per pass: the second
// pass's mesh is about a tenth as dense.
constexpr int kMinCullPatch[2] = {64, 8};
constexpr int kRepairRounds = 4;
constexpr float kEarlyFixDeg = 5.0f;

// Every camera's ground labels voted onto the mesh's vertices, then per face.
// Empty when no camera had labels.
std::vector<uint8_t> label_ground(const Mesh& mesh, OccupancyEvaluator& ev,
                                  const CameraParams& cams, int num_cameras,
                                  const MeshingConfig& cfg) {
    auto t = Clock::now();
    const std::vector<int>& sel = ev.selected_cameras();
    const int n_cams = sel.empty() ? num_cameras : (int)sel.size();
    std::vector<float> ground(mesh.V.size(), 0.0f), seen(mesh.V.size(), 0.0f);
    std::vector<float> moments, depth;
    std::vector<uint8_t> labels;
    int masked = 0;
    for (int ci = 0; ci < n_cams; ++ci) {
        const int c = sel.empty() ? ci : sel[ci];
        int W = 0, H = 0;
        if (!ev.render_moments(c, moments, W, H)) break;
        if (!cfg.ground_labels(c, W, H, labels) || labels.size() != (size_t)W * H) continue;
        depth.resize((size_t)W * H);
        moments_to_depth(moments.data(), depth.size(), depth.data());
        vote_ground(tsdf_camera(cams, c, W, H), depth.data(), labels.data(), mesh,
                    ground.data(), seen.data());
        ++masked;
    }
    if (masked == 0) return {};
    std::vector<uint8_t> faces = ground_faces(mesh, ground, seen);
    if (cfg.verbose) {
        long long n = 0;
        for (uint8_t f : faces) n += f;
        mlog::out(mlog::Stage::Ground, mmsg::ground_result,
                  {n, (long long)faces.size(), masked, mlog::num(secs_since(t), 2)});
    }
    return faces;
}

// `mesh` as `r` asks, and with `ground` its two parts beside it.
void write_with_parts(const Mesh& mesh, const MeshOutputRequest& r,
                      const std::vector<uint8_t>& ground) {
    write_mesh(mesh, r.mode, r.spec, r.base, /*verbose=*/true);
    if (ground.size() != mesh.F.size()) return;
    write_mesh(mesh_subset(mesh, ground, 1), r.mode, r.spec, r.base + "_ground", true);
    write_mesh(mesh_subset(mesh, ground, 0), r.mode, r.spec, r.base + "_noground", true);
}

// Everything after extraction, whatever made the mesh: merge, culling,
// cleanup, orientation, colour, export. False when --check found defects.
bool finish_mesh(Mesh& mesh, OccupancyEvaluator& ev, MeshProgress& prog,
                 const MeshingConfig& cfg, const CameraParams& cams,
                 const float* cam_pos, int num_cameras,
                 const std::vector<MeshOutputRequest>& outputs,
                 bool want_vertex, bool want_texture, Clock::time_point t0) {
    auto stage_check = [&](const std::string& tag) {
        if (cfg.check >= 2) check_stage(mesh, cfg, tag);
    };
    stage_check("extracted");
    // ---- 7. merge (do this once to reduce chance of GPU OOM during 6b, but risk CPU OOM) ----
    auto t6 = Clock::now();
    prog.raw_faces((double)mesh.F.size());
    prog.enter(MeshPhase::Merge);
    if (cfg.merge_factor > 0.0f)
        merge_vertices(mesh, merge_limits(mesh, ev, cfg, true),
                       cfg.merge_max_flip_deg, false, cfg.verbose, cfg.num_threads);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Merge, mmsg::seconds,
                  {mlog::num(secs_since(t6), 2)});
    stage_check("raw merge");

  for (int iter = 0; iter < 2; ++iter) {
    const std::string pass = " " + std::to_string(iter + 1);
    prog.faces((double)mesh.F.size());
    prog.enter(iter == 0 ? MeshPhase::Pass1 : MeshPhase::Pass2);

    // ---- 6b. cull vertices no camera sees (occlusion by the mesh itself) ----
    // Only faces go, so the mesh stays manifold with boundary.
    if (cfg.cull_unseen && ev.has_render_cameras() && !mesh.V.empty()) {
        auto tc = Clock::now();
        const int nv = (int)mesh.V.size();
        const int nf = (int)mesh.F.size();
        std::vector<unsigned char> vis(nv, 1);
        ev.cull_unseen_vertices(
            &mesh.V[0][0], nv, nf > 0 ? &mesh.F[0][0] : nullptr, nf, vis.data());
        despeckle_visibility(mesh, vis, kMinCullPatch[iter]);

        std::vector<int> remap(nv, -1);
        std::vector<std::array<float,3>> newV; newV.reserve(nv);
        for (int v = 0; v < nv; ++v)
            if (vis[v]) { remap[v] = (int)newV.size(); newV.push_back(mesh.V[v]); }
        std::vector<std::array<int,3>> newF; newF.reserve(mesh.F.size());
        long dropped_f = 0;
        for (int t = 0; t < nf; ++t) {
            const auto& f = mesh.F[t];
            if (!vis[f[0]] || !vis[f[1]] || !vis[f[2]]) { ++dropped_f; continue; }
            newF.push_back({remap[f[0]], remap[f[1]], remap[f[2]]});
        }
        size_t removed_v = (size_t)nv - newV.size();
        mesh.V.swap(newV);
        mesh.F.swap(newF);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Cull, mmsg::cull_result,
                      {(long long)removed_v, nv, dropped_f,
                       (long long)mesh.V.size(), (long long)mesh.F.size(),
                       mlog::num(secs_since(tc), 2)});
        stage_check("cull" + pass);
    }

    // ---- 7. feature-aware merge ----
    auto t6 = Clock::now();
    if (cfg.merge_factor > 0.0f)
        merge_vertices(mesh, merge_limits(mesh, ev, cfg, false), cfg.merge_max_flip_deg,
                       true, cfg.verbose, cfg.num_threads);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Merge, mmsg::seconds,
                  {mlog::num(secs_since(t6), 2)});
    stage_check("merge" + pass);

    // ---- 7a. post-merge cleanup: drop floaters, then fill small holes ----
    // Run after merge (final topology) and before orient so the orient pass
    // gives the new fill faces a consistent, outward winding. Floaters first so
    // we do not waste work filling holes in components about to be deleted.
    // Order matters: split_nonmanifold_vertices runs BEFORE fill_holes.
    //   - Culling deletes faces, which can split a vertex's fan and leave
    //     non-manifold (bowtie) vertices; at those the boundary has >2 edges, so
    //     a "butterfly" hole pinches through one point. Splitting first makes
    //     every boundary vertex have exactly two boundary edges, so the boundary
    //     becomes a set of disjoint simple loops -- a butterfly becomes two
    //     separate simple loops that fill_holes can then fill (instead of being
    //     skipped), and every filled loop is a true boundary circle. Filling a
    //     disk onto a true boundary circle preserves orientability; filling the
    //     bogus loops produced by ambiguous chaining at a bowtie did NOT (it was
    //     the sole source of non-orientable defects -- MT+merge+cull alone are
    //     orientable).
    //   - The split also isolates vertex-only-attached doublets into their own
    //     tiny components, which the final remove_floaters deletes.
    // A second split runs AFTER fill: fill can occasionally close a loop in a way
    // that leaves a single bowtie vertex, so re-splitting guarantees a fully
    // vertex-manifold result (split preserves orientability). The final floater
    // then removes whole isolated components (manifold-safe) left by either split.
    auto tcl = Clock::now();
    dedup_faces(mesh, cfg.verbose);
    remove_floaters(mesh, cfg.floater_min_faces, cfg.verbose);
    split_nonmanifold_vertices(mesh, cfg.verbose);
    stage_check("split" + pass);
    fill_holes(mesh, cfg.fill_hole_ratio, cfg.fill_hole_max_edges, cfg.verbose);
    stage_check("fill" + pass);
    split_nonmanifold_vertices(mesh, cfg.verbose);
    remove_floaters(mesh, cfg.floater_min_faces, cfg.verbose);
    // the next merge reshapes most of the mesh: before it, only the worst
    fix_degenerate_faces(mesh, iter == 0 ? std::min(cfg.degenerate_angle_deg, kEarlyFixDeg)
                                         : cfg.degenerate_angle_deg, cfg.verbose, cfg.num_threads);
    dedup_faces(mesh, cfg.verbose);
    stage_check("cleanup" + pass);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::seconds,
                  {mlog::num(secs_since(tcl), 2)});
  }

    prog.faces((double)mesh.F.size());
    // ---- 7a2. valence flips + tangential relaxation ----
    if (cfg.quality_iters > 0) {
        prog.enter(MeshPhase::Quality);
        auto tq = Clock::now();
        improve_mesh_quality(mesh, cfg.quality_iters, cfg.verbose, cfg.num_threads);
        stage_check("quality");
        // the relaxation moved vertices: one more sweep for the slivers it made
        fix_degenerate_faces(mesh, cfg.degenerate_angle_deg, cfg.verbose, cfg.num_threads);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Quality, mmsg::seconds,
                      {mlog::num(secs_since(tq), 2)});
    }

    // ---- 7a3. cut out what is still wrong; refill, except in the last round ----
    for (int round = 0; round < kRepairRounds; ++round) {
        std::vector<char> cut;
        const long n = mark_defects(mesh, cut);
        if (n == 0) break;
        std::vector<char> was_boundary = boundary_vertices(mesh);
        const std::vector<int> kept = cut_faces(mesh, cut);
        for (size_t i = 0; i < kept.size(); ++i) was_boundary[i] = was_boundary[kept[i]];
        was_boundary.resize(kept.size());
        if (cfg.verbose)
            mlog::out(mlog::Stage::Cleanup, mmsg::defects_removed,
                      {n, (long long)mesh.V.size(), (long long)mesh.F.size()});
        if (round + 1 < kRepairRounds) {
            split_nonmanifold_vertices(mesh, cfg.verbose);
            was_boundary.resize(mesh.V.size(), 1);   // split copies: assume the worst
            fill_holes(mesh, cfg.fill_hole_ratio, cfg.fill_hole_max_edges, cfg.verbose,
                       &was_boundary);
            split_nonmanifold_vertices(mesh, cfg.verbose);
        } else {
            drop_minor_fans(mesh);   // a split would move vertices after the last look
        }
        remove_floaters(mesh, cfg.floater_min_faces, cfg.verbose);
    }
    stage_check("repair");

    // ---- 7b. globally consistent, outward-facing winding ----
    auto t7 = Clock::now();
    prog.enter(MeshPhase::Orient);
    orient_mesh(mesh, cam_pos, num_cameras);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Orient, mmsg::seconds,
                  {mlog::num(secs_since(t7), 2)});

    // ---- 7c. smooth vertex normals (before any seam split, so normals stay
    //      continuous across UV seams) ----
    compute_vertex_normals(mesh);
    // the check's topology and quality counts are the stats line too
    MeshCheckLimits lim;
    lim.min_part_faces = cfg.floater_min_faces;
    lim.num_threads = cfg.num_threads;
    lim.intersections = cfg.check > 0;
    const MeshCheckReport report = check_mesh(mesh, lim);
    if (cfg.check > 0)
        for (const std::string& line : format_mesh_check(report, lim, "final"))
            mlog::out_raw(mlog::Stage::Debug, line);
    const bool check_ok = cfg.check <= 0 || report.defects() == 0;

    // ---- 7d. color: per-vertex, or a baked texture atlas ----
    auto t8 = Clock::now();
    if (want_vertex && !mesh.V.empty()) {
        prog.enter(MeshPhase::Color);
        const int nv = (int)mesh.V.size();
        std::vector<float> rgb(3 * nv);
        ev.colorize(&mesh.V[0][0], nv, rgb.data());
        mesh.C.resize(nv);
        for (int i = 0; i < nv; ++i)
            for (int a = 0; a < 3; ++a)
                mesh.C[i][a] = (unsigned char)std::min(std::max(
                    (int)std::lround(rgb[3*i+a] * 255.0f), 0), 255);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Color, mmsg::seconds,
                      {mlog::num(secs_since(t8), 2)});
    }

    // ---- 7e. ground split: face labels survive the atlas, which keeps face order ----
    std::vector<uint8_t> ground;
    if (cfg.ground_labels && ev.has_render_cameras() && !mesh.F.empty()) {
        prog.enter(MeshPhase::Ground);
        ground = label_ground(mesh, ev, cams, num_cameras, cfg);
    }

    // ---- 8. write everything the atlas does not touch ----
    // Before it, not after: baking splits the seam vertices, so V/F/C stop
    // describing the mesh a vertex-colored or plain file is meant to hold.
    prog.enter(MeshPhase::Write);
    if (cfg.verbose && mesh.F.empty())
        mlog::out(mlog::Stage::Stats, mmsg::stats_empty);
    else if (cfg.verbose)
        mlog::out(mlog::Stage::Stats, mmsg::stats_line,
                  {report.vertices, report.faces, report.parts, report.boundary_edges,
                   report.nonmanifold_edges, report.misoriented_edges, report.duplicate_faces,
                   mlog::num(report.min_angle_deg, 2), report.under_5_deg, report.under_15_deg});
    for (const MeshOutputRequest& r : outputs)
        if (r.mode != MeshColorMode::Texture) write_with_parts(mesh, r, ground);

    if (want_texture && !mesh.V.empty()) {
        t8 = Clock::now();
        prog.enter(MeshPhase::Atlas);
        UVAtlasConfig uvcfg;
        uvcfg.texture_size = cfg.texture_size;
        uvcfg.gutter_px = cfg.tex_gutter_px;
        uvcfg.chart_angle_deg = cfg.chart_angle_deg;
        uvcfg.num_threads = cfg.num_threads;
        uvcfg.verbose = cfg.verbose;
        // Texel budget per face = its projected pixel area in the best
        // unoccluded training view (3D area x (focal/z)^2): texels go where
        // the views actually captured detail. Face count alone starves
        // smooth-but-prominent surfaces -- big splats mean big faces, yet the
        // cameras may have seen them close-up -- and world area wastes the
        // atlas on huge distant background faces.
        if (ev.has_render_cameras()) {
            const size_t nvv = mesh.V.size(), nff = mesh.F.size();
            std::vector<float> nu(nvv, 0.0f);
            ev.view_texel_density(&mesh.V[0][0], (int)nvv, nu.data());
            // cap at the 99th percentile: a camera nearly touching a surface
            // must not hog the whole atlas
            std::vector<float> pos;
            pos.reserve(nvv);
            for (float v : nu) if (v > 0.0f) pos.push_back(v);
            if (!pos.empty()) {
                size_t q = (size_t)(0.99 * (double)(pos.size() - 1));
                std::nth_element(pos.begin(), pos.begin() + q, pos.end());
                const float nu_cap = std::max(pos[q], 1e-12f);
                std::vector<float> w(nff, 0.0f);
                std::vector<float> wpos;
                wpos.reserve(nff);
                for (size_t t = 0; t < nff; ++t) {
                    const auto& f = mesh.F[t];
                    const auto &A = mesh.V[f[0]], &B = mesh.V[f[1]], &C = mesh.V[f[2]];
                    float e1[3] = {B[0]-A[0], B[1]-A[1], B[2]-A[2]};
                    float e2[3] = {C[0]-A[0], C[1]-A[1], C[2]-A[2]};
                    float cx = e1[1]*e2[2] - e1[2]*e2[1];
                    float cy = e1[2]*e2[0] - e1[0]*e2[2];
                    float cz = e1[0]*e2[1] - e1[1]*e2[0];
                    float a3d = 0.5f * std::sqrt(cx*cx + cy*cy + cz*cz);
                    float nf_ = std::min(std::max({nu[f[0]], nu[f[1]], nu[f[2]]}),
                                         nu_cap);
                    w[t] = a3d * nf_ * nf_;
                    if (w[t] > 0.0f) wpos.push_back(w[t]);
                }
                // unseen faces (interior, undercarriage) still get a small
                // share so they don't collapse to a single texel
                if (!wpos.empty()) {
                    size_t m = wpos.size() / 2;
                    std::nth_element(wpos.begin(), wpos.begin() + m, wpos.end());
                    const float floor_w = 0.05f * std::max(wpos[m], 1e-12f);
                    for (auto& v : w) v = std::max(v, floor_w);
                    uvcfg.face_weight = std::move(w);
                }
            }
        }
        std::vector<int> face_chart = build_uv_atlas(mesh, uvcfg);
        prog.texels((double)uvcfg.texture_size * (double)uvcfg.texture_size);
        prog.enter(MeshPhase::Bake);
        bake_texture(mesh, face_chart, uvcfg,
                     [&](const float* xyz, int n, float* rgb) {
                         ev.colorize(xyz, n, rgb);
                     });
        if (cfg.verbose)
            mlog::out(mlog::Stage::Texture, mmsg::seconds,
                      {mlog::num(secs_since(t8), 2)});
        for (const MeshOutputRequest& r : outputs)
            if (r.mode == MeshColorMode::Texture) write_with_parts(mesh, r, ground);
    }
    prog.finish();
    mlog::out(mlog::Stage::Done, mmsg::done_summary,
              {(long long)mesh.V.size(), (long long)mesh.F.size(),
               mlog::num(secs_since(t0), 2)});
    return check_ok;
}

} // namespace

bool generate_mesh(
    const float* means, const float* quats,
    const float* log_scales, const float* logit_opac, const float* features_dc,
    int num_splats,
    const float* cam_pos, int num_cameras,
    const CameraParams& cams,
    const MeshingConfig& cfg,
    const std::string& output_path
) {
    auto t0 = Clock::now();

    // ---- validate the export request BEFORE any heavy work ----
    std::string formats_csv;
    for (const auto& f : cfg.formats)
        formats_csv += (formats_csv.empty() ? "" : ",") + f;
    if (formats_csv.empty()) formats_csv = "ply";
    std::vector<MeshFormatSpec> formats = parse_mesh_formats(formats_csv);
    const std::string out_base = mesh_output_strip_ext(output_path);
    std::vector<MeshColorMode> modes = cfg.colors;
    if (modes.empty()) modes.push_back(MeshColorMode::Vertex);
    std::vector<std::string> dropped;
    const std::vector<MeshOutputRequest> outputs =
        plan_mesh_outputs(formats, modes, out_base, &dropped);
    for (const std::string& d : dropped)
        mlog::out_raw(mlog::Stage::Wrote, "skipping " + d);
    if (outputs.empty()) {
        mlog::err_raw(mlog::Stage::Wrote,
                      dropped.empty() ? std::string("nothing to write")
                                      : dropped.front());
        return false;
    }
    bool want_vertex = false, want_texture = false;
    for (const MeshOutputRequest& r : outputs) {
        want_vertex = want_vertex || r.mode == MeshColorMode::Vertex;
        want_texture = want_texture || r.mode == MeshColorMode::Texture;
    }

    OccupancyEvaluator ev(means, quats, log_scales, logit_opac, features_dc, num_splats,
                          cam_pos, num_cameras, cams, cfg);

    // ---- debug: dump one camera's occupancy moments and bail out ----
    // SS_MESH_DEBUG_RENDER=<cam_idx> -> /tmp/ss_moments.f32 (float32 [H,W,3]).
    if (const char* dbg = spirula::env("MESH_DEBUG_RENDER")) {
        int cam = std::atoi(dbg);
        std::vector<float> mom; int W = 0, H = 0;
        if (!ev.render_moments(cam, mom, W, H)) {
            mlog::out(mlog::Stage::Debug, mmsg::debug_render_unavailable);
            return false;
        }
        double m0sum = 0; float m0max = 0, mn = 1e30f, mx = -1e30f;
        size_t seen = 0;
        for (size_t p = 0; p < (size_t)W * H; ++p) {
            float m0 = mom[3*p], me = mom[3*p+1];
            m0sum += m0; m0max = std::max(m0max, m0);
            if (m0 > 0.5f) { mn = std::min(mn, me); mx = std::max(mx, me); ++seen; }
        }
        // A dump of numbers behind SS_MESH_DEBUG_RENDER, read by whoever is
        // debugging the occupancy field rather than by whoever is waiting for
        // a mesh. It stays as it is.
        {
            char buf[256];
            std::snprintf(buf, sizeof buf,
                          "cam %d: %dx%d, mean m0=%.4f max m0=%.4f; "
                          "depth(m0>0.5) in [%.3f, %.3f] over %zu px",
                          cam, W, H, m0sum / ((double)W * H), m0max, mn, mx, seen);
            mlog::out_raw(mlog::Stage::Debug, buf);
        }
        FILE* f = std::fopen("/tmp/ss_moments.f32", "wb");
        if (f) {
            int hdr[3] = {H, W, 3};
            std::fwrite(hdr, sizeof(int), 3, f);
            std::fwrite(mom.data(), sizeof(float), mom.size(), f);
            std::fclose(f);
            mlog::out_raw(mlog::Stage::Debug,
                          "/tmp/ss_moments.f32 (header: H W 3, then floats)");
        }
        return true;
    }

    const bool tsdf = cfg.method == MeshMethod::Tsdf;
    const bool grid = tsdf && cfg.tsdf_voxel > 0.0f;   // else the field over the splat samples
    const int cloud_cams = cam_pos ? num_cameras : 0;
    const int spg = cfg.samples_per_gaussian <= 1 ? 1 : 7;
    const int64_t max_pts = ev.max_delaunay_points();
    // Refused here rather than inside the solver, which can only report an
    // empty result as "no tetrahedra"; OccupancyEvaluator already skipped LBVH
    // and RenderContext when over limit.
    if (!grid && ev.num_points() + cloud_cams > max_pts) {
        if (tsdf && (!cams.valid() || num_cameras <= 0)) {
            mlog::fail(mlog::Stage::Tsdf, mmsg::tsdf_needs_cameras);
            return false;
        }
        const long long max_gaussians =
            std::max(0LL, (long long)(max_pts - cloud_cams) / spg);
        mlog::fail(mlog::Stage::PointCloud, mmsg::too_many_gaussians,
                   {(long long)ev.num_kept(), (long long)max_pts, max_gaussians, (long long)spg});
        return false;
    }
    if (tsdf && !ev.has_render_cameras()) {
        mlog::fail(mlog::Stage::Tsdf, mmsg::tsdf_needs_cameras);
        return false;
    }

    // Each long phase announces itself BEFORE it runs: one that prints only on
    // completion looks like a hang and leaves a progress display nothing to show.
    MeshProgress prog(cfg, cfg.on_progress, (double)(ev.num_points() + cloud_cams), want_vertex,
                      want_texture);
    Mesh mesh;
    if (grid) {
        const std::vector<int>& sel = ev.selected_cameras();
        long long pixels = 0;
        if (sel.empty()) {
            for (int c = 0; c < num_cameras; ++c)
                pixels += (long long)cams.widths[c] * cams.heights[c];
        } else {
            for (int c : sel)
                pixels += (long long)cams.widths[c] * cams.heights[c];
        }
        prog.tsdf((double)pixels);
        if (!extract_tsdf(ev, cams, num_cameras, cfg, prog, mesh)) return false;
    } else if (!extract_opacity_field(ev, cfg, prog, t0, mesh, cam_pos, cloud_cams)) {
        return false;
    }
    return finish_mesh(mesh, ev, prog, cfg, cams, cam_pos, cam_pos ? num_cameras : 0, outputs,
                       want_vertex, want_texture, t0);
}

} // namespace meshing
