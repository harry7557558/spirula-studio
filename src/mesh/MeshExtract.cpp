// MeshExtract.cpp -- a surface from the opacity field or a TSDF (MeshStages.h).

#include "mesh/MeshStages.h"
#include "mesh/Delaunay3D.h"
#include "mesh/MarchingTets.h"
#include "mesh/MeshLog.h"
#include "mesh/Tsdf.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <limits>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

namespace mmsg = spirula::i18n::msg::mesh;

namespace {

// Free space is what a chain of all-free tetrahedra joins to an observer: the
// vertices from `first_obs` on, or without any the cloud's extreme vertices.
// Every other free vertex is made solid (docs/notes/mesh-field.md); how many.
long seal_pockets(const uint32_t* cv, long M, const float* pts, int P, int first_obs,
                  float iso, std::vector<float>& occ, std::vector<uint8_t>& sealed) {
    // Lock-free union-find: a root only ever hooks under a smaller index, so
    // the roots do not depend on the thread count.
    std::vector<std::atomic<int>> parent((size_t)P);
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < P; ++i) parent[i].store(i, std::memory_order_relaxed);
    auto find = [&](int x) {
        for (;;) {
            int p = parent[x].load(std::memory_order_relaxed);
            if (p == x) return x;
            const int g = parent[p].load(std::memory_order_relaxed);
            parent[x].compare_exchange_weak(p, g, std::memory_order_relaxed);
            x = g;
        }
    };
    auto unite = [&](int a, int b) {
        for (;;) {
            a = find(a); b = find(b);
            if (a == b) return;
            if (a < b) std::swap(a, b);
            int expect = a;
            if (parent[a].compare_exchange_strong(expect, b, std::memory_order_relaxed)) return;
        }
    };
    #pragma omp parallel for schedule(static)
    for (long t = 0; t < M; ++t) {
        const uint32_t* c = cv + 4 * t;
        if (occ[c[0]] < iso && occ[c[1]] < iso && occ[c[2]] < iso && occ[c[3]] < iso) {
            unite((int)c[0], (int)c[1]); unite((int)c[0], (int)c[2]); unite((int)c[0], (int)c[3]);
        }
    }

    std::vector<uint8_t> open((size_t)P, 0);
    for (int v = first_obs; v < P; ++v) open[find(v)] = 1;
    if (first_obs >= P) {
        // the farthest free vertex along each of 26 directions is on the hull
        for (int d = 1; d < 27; ++d) {
            if (d == 13) continue;
            const float dir[3] = {(float)(d % 3 - 1), (float)(d / 3 % 3 - 1), (float)(d / 9 - 1)};
            int best = -1;
            float most = -std::numeric_limits<float>::infinity();
            for (int v = 0; v < P; ++v) {
                const float s = dir[0] * pts[3*v] + dir[1] * pts[3*v+1] + dir[2] * pts[3*v+2];
                if (s > most) { most = s; best = v; }
            }
            if (best >= 0 && occ[best] < iso) open[find(best)] = 1;
        }
    }
    long reached = 0, n = 0;
    #pragma omp parallel for schedule(static) reduction(+:reached)
    for (int v = 0; v < first_obs; ++v)
        if (occ[v] < iso && open[find(v)]) ++reached;
    if (reached == 0) return 0;   // no observer in free space: nothing to tell pockets by
    sealed.assign((size_t)P, 0);
    #pragma omp parallel for schedule(static) reduction(+:n)
    for (int v = 0; v < first_obs; ++v)
        if (occ[v] < iso && !open[find(v)]) { sealed[v] = 1; ++n; }
    for (int v = 0; v < first_obs; ++v) if (sealed[v]) occ[v] = 1.0f;
    return n;
}

}  // namespace

bool extract_opacity_field(OccupancyEvaluator& ev, const MeshingConfig& cfg,
                           MeshProgress& prog, Clock::time_point t0, Mesh& mesh,
                           const float* cam_pos, int num_cameras) {
    const float iso = cfg.iso;
    // ---- 1. point cloud (float everywhere except the Delaunay call) ----
    prog.enter(MeshPhase::PointCloud);
    if (cfg.verbose)
        mlog::out(mlog::Stage::PointCloud, mmsg::point_cloud_sampling,
                  {ev.num_kept()});
    std::vector<float> pts;
    ev.generate_point_cloud(pts);
    // The cameras join the triangulation as free vertices, for seal_pockets.
    const int P0 = ev.num_points();
    const int P = P0 + (cam_pos ? num_cameras : 0);
    if (P > P0) pts.insert(pts.end(), cam_pos, cam_pos + 3 * (size_t)num_cameras);
    if (cfg.verbose)
        mlog::out(mlog::Stage::PointCloud, mmsg::point_cloud_done,
                  {P0, mlog::num(secs_since(t0), 2)});

    // ---- 2. Delaunay tetrahedralization (needs double precision) ----
    auto t1 = Clock::now();
    prog.enter(MeshPhase::Delaunay);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Delaunay, mmsg::delaunay_start, {P});
    std::vector<double> pts_d(pts.begin(), pts.end());
    // The float copy is dead until step 3 and float(double(f)) == f, so hand its
    // pages back before the solver claims ~260 bytes per point.
    pts.clear(); pts.shrink_to_fit();
    auto tri = delaunay3d::compute_delaunay_3d(pts_d.data(), P, cfg.num_threads, false);
    pts.resize(pts_d.size());
    std::transform(pts_d.begin(), pts_d.end(), pts.begin(),
                   [](double v) { return static_cast<float>(v); });
    pts_d.clear(); pts_d.shrink_to_fit();
    const int M = tri.nb_cells;
    if (cfg.verbose)
        mlog::out(mlog::Stage::Delaunay, mmsg::delaunay_done,
                  {M, tri.num_threads, mlog::num(secs_since(t1), 2)});
    if (M == 0) {
        mlog::fail(mlog::Stage::Delaunay, mmsg::no_tetrahedra);
        return false;
    }

    // ---- 3. occupancy at all vertices ----
    auto t2 = Clock::now();
    prog.enter(MeshPhase::Occupancy);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Occupancy, mmsg::occupancy_start, {P0});
    std::vector<float> occ(P, 0.0f);
    ev.evaluate(pts.data(), P0, occ.data());
    if (cfg.verbose)
        mlog::out(mlog::Stage::Occupancy, mmsg::seconds,
                  {mlog::num(secs_since(t2), 2)});
    std::vector<uint8_t> sealed;
    {
        const auto ts = Clock::now();
        const long n = seal_pockets(tri.cell_vertices.data(), M, pts.data(), P, P0, iso, occ, sealed);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Occupancy, mmsg::pockets_sealed,
                      {n, mlog::num(secs_since(ts), 2)});
    }


    // ---- 4. collect cut edges ----
    auto t3 = Clock::now();
    prog.enter(MeshPhase::CutEdges);
    if (cfg.verbose)
        mlog::out(mlog::Stage::CutEdges, mmsg::cut_edges_start, {M});
    const int* cv = (const int*)tri.cell_vertices.data();
    // One marching-tetrahedra case index per tet, so the two serial passes below
    // read a byte instead of gathering four occupancies each.
    std::vector<uint8_t> code((size_t)M);
    int64_t n_mt_tri = 0;
    #pragma omp parallel for schedule(static) reduction(+:n_mt_tri)
    for (long t = 0; t < (long)M; ++t) {
        const int* c = cv + 4 * t;
        int k = (occ[c[0]] < iso ? 1 : 0) | (occ[c[1]] < iso ? 2 : 0) |
                (occ[c[2]] < iso ? 4 : 0) | (occ[c[3]] < iso ? 8 : 0);
        code[t] = (uint8_t)k;
        n_mt_tri += kMT[k].ntri;
    }
    // Every mesh vertex is one cut edge, and the isosurface is a triangle mesh,
    // so V ~ F/2; 1.4x of that keeps the table's load factor around half.
    const size_t e_est = (size_t)(n_mt_tri / 2) + 1;
    EdgeTable edge_id(e_est + e_est / 2 + e_est / 10);
    std::vector<int32_t> ea, eb;
    std::vector<float> oa, ob;
    ea.reserve(e_est); eb.reserve(e_est);
    oa.reserve(e_est); ob.reserve(e_est);
    auto get_edge = [&](int va, int vb) {
        const int id = (int)ea.size();
        if (edge_id.insert(edge_key(va, vb, P), id) >= 0) return;
        ea.push_back(va); eb.push_back(vb);
        oa.push_back(occ[va]); ob.push_back(occ[vb]);
    };
    for (long t = 0; t < (long)M; ++t) {
        const int k = code[t];
        if (k == 0 || k == 15) continue;
        const int* c = cv + 4 * t;
        for (int e = 0; e < 6; ++e) {
            const int ia = kTetEdge[e][0], ib = kTetEdge[e][1];
            if (((k >> ia) & 1) != ((k >> ib) & 1)) get_edge(c[ia], c[ib]);
        }
    }
    const int E = (int)ea.size();
    if (cfg.verbose)
        mlog::out(mlog::Stage::CutEdges, mmsg::cut_edges_done,
                  {E, mlog::num(secs_since(t3), 2)});
    if (E == 0) {
        mlog::fail(mlog::Stage::CutEdges, mmsg::isosurface_empty,
                   {mlog::num(iso, 3)});
        return false;
    }

    // ---- 5. bisection: one mesh vertex per cut edge ----
    auto t4 = Clock::now();
    prog.edges(E);
    prog.enter(MeshPhase::Bisection);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bisection, mmsg::bisection_start,
                  {E, cfg.bisection_iters});
    mesh.V.resize(E);
    {
        std::vector<float> xyz((size_t)3 * E);
        ev.bisect_edges(pts.data(), P, ea.data(), eb.data(), oa.data(), ob.data(), E, xyz.data());
        for (size_t i = 0; i < (size_t)E; ++i)
            mesh.V[i] = {xyz[3*i], xyz[3*i+1], xyz[3*i+2]};
        // the field has no crossing on an edge into a sealed pocket: its middle
        if (!sealed.empty()) {
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < E; ++i)
                if (sealed[ea[i]] || sealed[eb[i]])
                    for (int a = 0; a < 3; ++a)
                        mesh.V[i][a] = 0.5f * (pts[3 * (size_t)ea[i] + a] + pts[3 * (size_t)eb[i] + a]);
        }
    }
    std::vector<int32_t>().swap(ea); std::vector<int32_t>().swap(eb);
    std::vector<float>().swap(oa);   std::vector<float>().swap(ob);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bisection, mmsg::seconds,
                  {mlog::num(secs_since(t4), 2)});

    // ---- 6. emit triangles ----
    // Delaunay tets have no consistent orientation, so each triangle is wound
    // by the field: its normal toward the tet's empty corners.
    auto t5 = Clock::now();
    prog.enter(MeshPhase::Marching);
    mesh.F.reserve((size_t)n_mt_tri);
    for (long t = 0; t < (long)M; ++t) {
        const MTCase& mc = kMT[code[t]];
        if (mc.ntri == 0) continue;
        const int* c = cv + 4 * t;

        // outward direction = (centroid of empty corners) - (centroid of solid)
        float cin[3] = {0,0,0}, cout[3] = {0,0,0};
        int nin = 0, nout = 0;
        for (int k = 0; k < 4; ++k) {
            const float* pc = &pts[3 * c[k]];
            if (occ[c[k]] < iso) { cout[0]+=pc[0]; cout[1]+=pc[1]; cout[2]+=pc[2]; ++nout; }
            else                 { cin[0]+=pc[0];  cin[1]+=pc[1];  cin[2]+=pc[2];  ++nin; }
        }
        float dout[3];
        for (int a = 0; a < 3; ++a)
            dout[a] = (nout ? cout[a]/nout : 0.f) - (nin ? cin[a]/nin : 0.f);

        for (int ti = 0; ti < mc.ntri; ++ti) {
            int vid[3];
            bool ok = true;
            for (int k = 0; k < 3; ++k) {
                int ca = c[mc.e[ti][k][0]], cb = c[mc.e[ti][k][1]];
                vid[k] = edge_id.find(edge_key(ca, cb, P));
                if (vid[k] < 0) { ok = false; break; }
            }
            if (!ok) continue;
            // flip winding if the triangle normal faces the solid side
            const auto& P0 = mesh.V[vid[0]];
            const auto& P1 = mesh.V[vid[1]];
            const auto& P2 = mesh.V[vid[2]];
            float e1[3] = {P1[0]-P0[0], P1[1]-P0[1], P1[2]-P0[2]};
            float e2[3] = {P2[0]-P0[0], P2[1]-P0[1], P2[2]-P0[2]};
            float nx = e1[1]*e2[2] - e1[2]*e2[1];
            float ny = e1[2]*e2[0] - e1[0]*e2[2];
            float nz = e1[0]*e2[1] - e1[1]*e2[0];
            if (nx*dout[0] + ny*dout[1] + nz*dout[2] < 0.0f)
                std::swap(vid[1], vid[2]);
            mesh.F.push_back({vid[0], vid[1], vid[2]});
        }
    }
    if (cfg.verbose)
        mlog::out(mlog::Stage::Marching, mmsg::marching_done,
                  {(long long)mesh.F.size(), mlog::num(secs_since(t5), 2)});

    edge_id.release();
    std::vector<uint8_t>().swap(code);
    std::vector<float>().swap(occ);
    pts.clear(); pts.shrink_to_fit();
    tri.cell_adjacents.clear(); tri.cell_adjacents.shrink_to_fit();
    tri.cell_vertices.clear(); tri.cell_vertices.shrink_to_fit();
    return true;
}

bool extract_tsdf(OccupancyEvaluator& ev, const CameraParams& cams, int num_cameras,
                  const MeshingConfig& cfg, MeshProgress& prog, Mesh& mesh) {
    const float voxel = cfg.tsdf_voxel;
    const float trunc = std::max(cfg.tsdf_trunc, 1.0f) * voxel;
    TsdfVolume vol(voxel, trunc);
    const std::vector<int>& sel = ev.selected_cameras();
    const int n_cams = sel.empty() ? num_cameras : (int)sel.size();
    auto t = Clock::now();
    prog.enter(MeshPhase::Fuse);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Tsdf, mmsg::tsdf_start,
                  {n_cams, mlog::num(voxel, 4), mlog::num(trunc, 4)});
    std::vector<float> moments, depth;
    for (int ci = 0; ci < n_cams; ++ci) {
        const int c = sel.empty() ? ci : sel[ci];
        int W = 0, H = 0;
        if (!ev.render_moments(c, moments, W, H)) {
            mlog::fail(mlog::Stage::Tsdf, mmsg::tsdf_needs_cameras);
            return false;
        }
        depth.resize((size_t)W * H);
        moments_to_depth(moments.data(), depth.size(), depth.data());
        TsdfCamera tc{};
        std::copy(cams.viewmats + 16 * c, cams.viewmats + 16 * (c + 1), tc.viewmat);
        tc.fx = cams.intrins[4 * c];
        tc.fy = cams.intrins[4 * c + 1];
        tc.cx = cams.intrins[4 * c + 2];
        tc.cy = cams.intrins[4 * c + 3];
        tc.width = W;
        tc.height = H;
        tc.model = cams.camera_models[c];
        tc.distortion = cams.distortions ? cams.distortions[c] : 0;
        if (cams.dist_coeffs) std::copy(cams.dist_coeffs + 8 * c, cams.dist_coeffs + 8 * (c + 1), tc.dist);
        vol.integrate(tc, depth.data(), cfg.tsdf_max_depth);
    }
    if (cfg.verbose)
        mlog::out(mlog::Stage::Tsdf, mmsg::tsdf_fused,
                  {(long long)vol.num_blocks(), mlog::num((double)vol.bytes() / 1048576.0, 0),
                   mlog::num(secs_since(t), 2)});

    t = Clock::now();
    prog.enter(MeshPhase::Extract);
    vol.extract(mesh);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Tsdf, mmsg::marching_done,
                  {(long long)mesh.F.size(), mlog::num(secs_since(t), 2)});
    if (mesh.F.empty()) {
        mlog::fail(mlog::Stage::Tsdf, mmsg::tsdf_empty);
        return false;
    }
    return true;
}

} // namespace meshing
