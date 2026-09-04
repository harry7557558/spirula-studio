// Bundle adjustment for the mapper: build a BAProblem from a Reconstruction and
// run the existing GPU solver (src/sfm/README.md "BA integration").
//
// The reconstruction's camera convention (+z forward, angle-axis pose) maps 1:1
// to the solver's per-group camera models (pinhole_radial for RADIAL, opencv for
// OPENCV; D29), so no coordinate juggling is needed. Gauge is left free -- the solver's LM damping
// regularizes it, exactly as it does for the (also gauge-free) BAL problems.
//
// Known MVP limitation: each call constructs a fresh BundleSolver (hence a fresh
// VkContext) and pays the device init every time. The context tears down fully at
// scope exit (VRAM is returned -- before that, a 1363-image run OOMed on the
// accumulated leaks), but a persistent, reusable solver belongs with the
// phase-0 shared GPU primitives.
#pragma once

#include <algorithm>
#include <atomic>
#include <map>
#include <vector>

#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Model.h"
#include "sfm/map/Profile.h"

namespace sfm {

struct BundleOptions {
    RealCfg real = RealCfg::F64;
    int max_iters = 25;
    bool verbose = false;
    int device = -1;
    // Robust loss for mapping-time BA (D36). COLMAP's global BA is trivial
    // because local BA cleans each registration first; without local BA, a
    // single bad registration's residuals bend a small model before the
    // filters can catch it. Huber keeps the quadratic basin for well-fit
    // observations and grows linearly past `loss_param` pixels.
    std::string loss = "huber";
    float loss_param = 2.0f;
    // Convergence overrides (D38): growth-phase BAs pass a looser tolerance so
    // iteration count adapts to actual convergence instead of a fixed cap.
    // 0 keeps the solver defaults (the final refinement passes do).
    double rtol = 0;
    int patience = 0;
    // Refine each camera's principal point, or hold it where the setup put it
    // (the image centre, unless something measured otherwise). COLMAP's
    // refine_principal_point, false there and here.
    //
    // Shifting the principal point by d is almost exactly a rotation of the
    // camera by d/f -- for an equidistant fisheye it is exactly that to first
    // order across the whole field, since a rotation moves every angle by the
    // same amount. So the parameter buys nothing and costs plenty: with a
    // single camera group its drift is a pure gauge (every camera turns the
    // same way, which the alignment absorbs), but with two or more groups each
    // drifts its own way and the difference is a real error in their relative
    // orientation. On the dual-fisheye 360 rigs that error was 1.0-1.8 deg
    // of inter-lens rotation, matching the drift difference to within 25% (D50).
    //
    // A *finished* model is a different situation, which is why this is an
    // option and not a constant: COLMAP's own documentation says to hold the
    // principal point during reconstruction and then "try to refine [it] in
    // global bundle adjustment" once every image is in, "especially when
    // sharing intrinsic parameters between multiple images". Hence the
    // qualifier below -- sharing is what makes it observable (D51).
    bool refine_principal_point = false;
    // Refine the distortion coefficients, or hold them at the setup's value.
    // COLMAP's refine_extra_params, true there and here; holding them pins the
    // principal point too, since the free set is a prefix (D72).
    bool refine_extra_params = true;
    // Refuse a solve that does not fit the device rather than attempting it --
    // for a caller that can split the problem and retry (Mapper::jointRefine).
    bool over_budget_throws = false;
    // ... and only for camera groups with at least this many images behind
    // them. A group of one image has no sharing at all: moving its principal
    // point is exactly a rotation of that one camera, with nothing to
    // contradict it. 0 refines every group.
    size_t pp_min_images = 20;
    // Which linear solver the reduced camera system gets: "auto" is the
    // solver's own n_dim threshold, "dense" its Cholesky, "cg" the
    // implicit-Schur conjugate gradient. The threshold was measured on one GPU
    // and the crossover moves with the hardware, so it is worth being able to
    // name (`--ba-solver`).
    std::string solver = "auto";
    // Persistent context (D38): device, pipelines and descriptor machinery
    // outlive one solve. The caller owns it and must keep (real, loss) fixed
    // across calls on the same context. Null = scoped context per call.
    VkContext* shared_ctx = nullptr;
    // Host worker threads, for the `cpu` scalar; 0 = hardware_concurrency.
    int threads = 0;
};

// The problem built from a reconstruction, plus what writing the solution back
// needs: which reconstruction entity each BA index belongs to. Kept apart from
// `runGlobalBA` so a caller that wants to drive the solver itself (the `ba`
// subcommand, on a model directory) does not have to rebuild any of this.
struct BundleLayout {
    BAProblem P;
    std::vector<Image*> imgOf;      // by BA image index
    std::vector<Point3D*> ptOf;     // by BA point index
    std::vector<uint32_t> camIds;   // by group
};

// Pack `rec` into a BAProblem. Empty layout (num_images < 2) if there is
// nothing to optimize.
inline BundleLayout buildBundle(Reconstruction& rec, const BundleOptions& bopt) {
    // Index registered images and 3D points.
    //
    // Everything downstream addresses them by their dense BA index, so the
    // id -> index maps are flat arrays rather than std::map: assembly walks a
    // few million observations and a tree lookup per observation was the whole
    // reason "BA build" showed up next to "BA solve" in the profile.
    BundleLayout L;
    std::vector<uint32_t> imgIds;
    std::vector<Image*>& imgOf = L.imgOf;  // by BA index
    uint32_t max_img_id = 0;
    for (auto& kv : rec.images) max_img_id = std::max(max_img_id, kv.first);
    std::vector<uint32_t> imgBA(max_img_id + 1, UINT32_MAX);
    for (auto& kv : rec.images)
        if (kv.second.registered) {
            imgBA[kv.first] = (uint32_t)imgIds.size();
            imgIds.push_back(kv.first);
            imgOf.push_back(&kv.second);
        }
    std::vector<uint64_t> ptIds;
    std::vector<Point3D*>& ptOf = L.ptOf;  // by BA index
    for (auto& kv : rec.points3D) {
        if (kv.second.track.size() < 2) continue;
        ptIds.push_back(kv.first);
        ptOf.push_back(&kv.second);
    }
    if (imgIds.size() < 2 || ptIds.empty()) return BundleLayout{};

    // Camera groups: one per distinct camera used (usually a single shared one).
    std::vector<uint32_t>& camIds = L.camIds;
    std::map<uint32_t, uint32_t> camGroup;
    for (Image* im : imgOf) {
        uint32_t cid = im->camera_id;
        if (!camGroup.count(cid)) {
            camGroup[cid] = (uint32_t)camIds.size();
            camIds.push_back(cid);
        }
    }

    BAProblem& P = L.P;
    P.num_images = (uint32_t)imgIds.size();
    P.num_points = (uint32_t)ptIds.size();

    // Observations, emitted point-major (which is the order the solver's tables
    // want) so the only sorting left is by image *within* one point's track --
    // a handful of elements each, instead of one global sort of millions.
    struct Obs { uint32_t img, pt; double x, y; };
    std::vector<Obs> obs;
    obs.reserve((size_t)P.num_points * 3);
    P.obs_ranges.assign(P.num_points + 1, 0);
    for (uint32_t p = 0; p < P.num_points; p++) {
        const size_t start = obs.size();
        for (const TrackElement& e : ptOf[p]->track) {
            if (e.image_id > max_img_id) continue;
            const uint32_t bi = imgBA[e.image_id];
            if (bi == UINT32_MAX) continue;
            const Vec2& xy = imgOf[bi]->points2D[e.point2D_idx];
            obs.push_back({bi, p, xy.x, xy.y});
        }
        std::sort(obs.begin() + start, obs.end(),
                  [](const Obs& a, const Obs& b) { return a.img < b.img; });
        P.obs_ranges[p + 1] = (uint32_t)obs.size();
    }
    P.num_obs = (uint32_t)obs.size();
    P.obs_image.resize(P.num_obs);
    P.obs_point.resize(P.num_obs);
    P.obs_xy.resize(2 * P.num_obs);
    for (uint32_t i = 0; i < P.num_obs; i++) {
        P.obs_image[i] = obs[i].img;
        P.obs_point[i] = obs[i].pt;
        P.obs_xy[2 * i] = obs[i].x;
        P.obs_xy[2 * i + 1] = obs[i].y;
    }

    // Poses (angle-axis + t).
    P.poses.resize(6 * P.num_images);
    for (uint32_t i = 0; i < P.num_images; i++) {
        const Image& im = *imgOf[i];
        Vec3 aa = rotationToAngleAxis(im.pose.R);
        P.poses[6 * i + 0] = aa.x; P.poses[6 * i + 1] = aa.y; P.poses[6 * i + 2] = aa.z;
        P.poses[6 * i + 3] = im.pose.t.x; P.poses[6 * i + 4] = im.pose.t.y;
        P.poses[6 * i + 5] = im.pose.t.z;
    }

    // Intrinsics groups. Model + parameter count are per group (each camera may
    // pick its own distortion model), so the flat `intr` array is packed with
    // per-group offsets rather than a single stride (D29 ended the old uniform
    // pinhole_radial-only assumption). The model index, count and parameter
    // layout all come from sfm/core/Camera.h -- one source of truth (D30).
    //
    // Every group stores all of its model's parameters; how many of them BA may
    // *change* is the group's n_intr, and only those own columns of the reduced
    // system (D50). Storage and columns are therefore two different packings --
    // a group holding its principal point fixed stores 8 and owns 6 -- and the
    // solver's intr_update walks the group table rather than assuming they
    // coincide.
    std::vector<size_t> group_images(camIds.size(), 0);
    std::vector<uint32_t> img_group(P.num_images);
    for (uint32_t i = 0; i < P.num_images; i++) {
        img_group[i] = camGroup[imgOf[i]->camera_id];
        group_images[img_group[i]]++;
    }
    P.groups.resize(camIds.size());
    P.intr.clear();
    for (size_t g = 0; g < camIds.size(); g++) {
        const Camera& c = rec.cameras[camIds[g]];
        const bool pp = bopt.refine_principal_point && bopt.refine_extra_params &&
                        group_images[g] >= bopt.pp_min_images;
        uint32_t nf = (uint32_t)camNumFreeParams(c.model, pp, bopt.refine_extra_params);
        uint32_t off = (uint32_t)P.intr.size();
        uint32_t ni = (uint32_t)camNumParams(c.model);
        double ps[12];
        packIntrinsics(c, ps);
        for (uint32_t i = 0; i < ni; i++) P.intr.push_back(ps[i]);
        P.groups[g] = {off, P.free_intr, nf, (uint32_t)camBaModel(c.model)};
        P.free_intr += nf;
    }
    P.image_group = std::move(img_group);

    // Points.
    P.points.resize(3 * P.num_points);
    for (uint32_t i = 0; i < P.num_points; i++) {
        const Vec3& X = ptOf[i]->xyz;
        P.points[3 * i] = X.x; P.points[3 * i + 1] = X.y; P.points[3 * i + 2] = X.z;
    }

    P.pose_dim = 6 * P.num_images;
    P.total_intr = (uint32_t)P.intr.size();
    P.n_dim = P.pose_dim + P.free_intr;
    for (auto& g : P.groups) g.intr_col += P.pose_dim;
    finalizeTables(P);
    return L;
}

// Solver options for a mapper-driven bundle adjustment.
inline SolverOptions bundleSolverOptions(const BundleOptions& bopt) {
    SolverOptions sopt;
    sopt.real = bopt.real;
    sopt.max_iters = bopt.max_iters;
    sopt.verbose = bopt.verbose;
    sopt.device = bopt.device;
    sopt.loss = bopt.loss;
    sopt.loss_param = bopt.loss_param;
    if (bopt.rtol > 0) sopt.rtol = bopt.rtol;
    if (bopt.patience > 0) sopt.patience = bopt.patience;
    if (bopt.solver == "dense") sopt.solver = SolverSel::Dense;
    else if (bopt.solver == "cg") sopt.solver = SolverSel::CG;
    sopt.over_budget_throws = bopt.over_budget_throws;
    sopt.threads = bopt.threads;
    return sopt;
}

// Copy a solved problem's parameters back into the reconstruction it came from.
// `P` is the layout's own problem unless the caller moved it out to hand to a
// solver, which `spirula-sfm ba` does.
inline void writeBundle(Reconstruction& rec, const BundleLayout& L, const BAProblem& P) {
    for (uint32_t i = 0; i < P.num_images; i++) {
        Image& im = *L.imgOf[i];
        im.pose.R = angleAxisToRotation({P.poses[6 * i], P.poses[6 * i + 1], P.poses[6 * i + 2]});
        im.pose.t = {P.poses[6 * i + 3], P.poses[6 * i + 4], P.poses[6 * i + 5]};
    }
    for (size_t g = 0; g < L.camIds.size(); g++) {
        Camera& c = rec.cameras[L.camIds[g]];
        unpackIntrinsics(c, &P.intr[P.groups[g].intr_offset]);
    }
    for (uint32_t i = 0; i < P.num_points; i++)
        L.ptOf[i]->xyz = {P.points[3 * i], P.points[3 * i + 1], P.points[3 * i + 2]};
}

// ---- host fallback after a device failure ---------------------------------

// A solve the device could not finish -- a lost device (what a Windows TDR
// reset looks like from here), or a refused allocation -- re-runs on the host,
// and every later solve that big goes straight there: the problems only grow.
inline std::atomic<uint64_t>& baHostObsThreshold() {
    static std::atomic<uint64_t> n{UINT64_MAX};
    return n;
}

inline void noteBaDeviceFailure(const VkError& e, uint64_t num_obs) {
    uint64_t from = e.result == VK_ERROR_DEVICE_LOST ? 0 : num_obs;
    uint64_t was = baHostObsThreshold().load();
    while (from < was && !baHostObsThreshold().compare_exchange_weak(was, from)) {}
    static std::atomic<bool> said{false};
    if (!said.exchange(true))
        slog::warn(slog::Tag::Map, spirula::i18n::msg::sfm::ba_host_fallback, {e.what()});
}

// Global BA over all registered images and all 3D points. Overwrites poses,
// point positions, and intrinsics in `rec`. Returns the final RMS reprojection
// cost reported by the solver (0 if nothing to optimize).
inline double runGlobalBA(Reconstruction& rec, const BundleOptions& bopt) {
    auto prof_t0 = std::chrono::steady_clock::now();
    auto prof_lap = [&prof_t0] {
        auto t1 = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(t1 - prof_t0).count();
        prof_t0 = t1;
        return dt;
    };
    BundleLayout L = buildBundle(rec, bopt);
    BAProblem& P = L.P;
    if (P.num_images < 2) return 0;

    SolverOptions sopt = bundleSolverOptions(bopt);
    if (P.num_obs >= baHostObsThreshold().load()) sopt.real = RealCfg::CPU;
    double t_build = prof_lap();

    double t_init = 0, t_solve = 0;
    SolverStats stats;
    // The solver reads the problem's host parameters at init() and writes them
    // only in downloadParams(), so a failed attempt leaves `P` where it
    // started and the host solver can restart from the same model.
    auto attempt = [&] {
        BundleSolver solver(P, sopt, bopt.shared_ctx);
        solver.init();
        t_init = prof_lap();
        solver.solve();
        t_solve = prof_lap();
        solver.downloadParams();
        stats = solver.stats();
    };
    try {
        attempt();
    } catch (const VkError& e) {
        if (!vkErrorIsResourceFailure(e.result)) throw;
        noteBaDeviceFailure(e, P.num_obs);
        sopt.real = RealCfg::CPU;
        attempt();
    }
    writeBundle(rec, L, P);

    double t_write = prof_lap();
    g_map_prof.ba_build += t_build;
    g_map_prof.ba_init += t_init;
    g_map_prof.ba_solve += t_solve;
    g_map_prof.ba_write += t_write;
    g_map_prof.n_ba++;
    g_map_prof.n_ba_iters += stats.iterations;
    if (MapProf::enabled())
        fprintf(stderr,
                "[prof] BA #%ld: %u img %u pt %u obs | build %.3f init %.3f solve %.3f "
                "write %.3f s | %d LM iters\n",
                (long)g_map_prof.n_ba, P.num_images, P.num_points, P.num_obs, t_build, t_init,
                t_solve, t_write, stats.iterations);
    return stats.final_cost;
}

// ---- joint refinement of several components (D45) -------------------------
//
// Components are separate reconstructions but they are not separate cameras:
// the same lens took all of them, so its intrinsics are one set of unknowns
// that every component's observations constrain. Refining each component alone
// splits that evidence -- a 20-image component fits its own focal to its own
// noise, drifts, and then aligns with nothing.
//
// This packs every component into one BAProblem by giving each its own id
// range for images and points while leaving camera ids shared, so the solver
// sees one intrinsics group per lens fed by every component at once. The
// components stay geometrically independent (no observation links them, and
// each keeps its own gauge freedom, which the solver's damping handles exactly
// as it does for a single free-floating model).
//
// Intrinsics start from the best-constrained component's values -- averaging
// distortion coefficients across models that disagree is not meaningful, and
// the largest model's are the ones with evidence behind them.
inline double runJointBA(std::vector<Reconstruction*> models, const BundleOptions& bopt) {
    if (models.empty()) return 0;
    size_t live = 0;
    for (const Reconstruction* m : models)
        if (m->numRegistered() >= 2) live++;
    if (live == 0) return 0;
    if (live == 1 && models.size() == 1) return runGlobalBA(*models[0], bopt);

    // Id strides, so a merged view can be split apart again unambiguously.
    uint32_t img_stride = 0;
    uint64_t pt_stride = 0;
    for (const Reconstruction* m : models) {
        for (const auto& kv : m->images) img_stride = std::max(img_stride, kv.first + 1);
        for (const auto& kv : m->points3D) pt_stride = std::max(pt_stride, kv.first + 1);
    }
    if (img_stride == 0 || pt_stride == 0) return 0;

    Reconstruction all;
    // Cameras: shared by id, taken from the component with the most images.
    std::map<uint32_t, double> cam_weight;
    for (const Reconstruction* m : models) {
        std::map<uint32_t, double> w;
        for (const auto& kv : m->images)
            if (kv.second.registered) w[kv.second.camera_id] += 1.0;
        for (const auto& kv : w) {
            auto it = m->cameras.find(kv.first);
            if (it == m->cameras.end()) continue;
            if (!cam_weight.count(kv.first) || kv.second > cam_weight[kv.first]) {
                cam_weight[kv.first] = kv.second;
                all.cameras[kv.first] = it->second;
            }
        }
    }
    for (size_t mi = 0; mi < models.size(); mi++) {
        const Reconstruction& m = *models[mi];
        if (m.numRegistered() < 2) continue;
        const uint32_t io = (uint32_t)mi * img_stride;
        const uint64_t po = (uint64_t)mi * pt_stride;
        for (const auto& kv : m.images) {
            if (!kv.second.registered) continue;
            Image im = kv.second;
            im.id = kv.first + io;
            for (uint64_t& p : im.point3D_ids)
                if (p != kInvalidPoint3D) p += po;
            all.images[im.id] = std::move(im);
        }
        for (const auto& kv : m.points3D) {
            Point3D pt = kv.second;
            for (TrackElement& e : pt.track) e.image_id += io;
            all.points3D[kv.first + po] = std::move(pt);
        }
    }
    if (all.images.size() < 2 || all.points3D.empty()) return 0;

    double cost = runGlobalBA(all, bopt);

    // Scatter back. Intrinsics land in every component, which is the point.
    for (size_t mi = 0; mi < models.size(); mi++) {
        Reconstruction& m = *models[mi];
        if (m.numRegistered() < 2) {
            for (auto& kv : m.cameras) {
                auto it = all.cameras.find(kv.first);
                if (it != all.cameras.end()) kv.second = it->second;
            }
            continue;
        }
        const uint32_t io = (uint32_t)mi * img_stride;
        const uint64_t po = (uint64_t)mi * pt_stride;
        for (auto& kv : m.images) {
            if (!kv.second.registered) continue;
            auto it = all.images.find(kv.first + io);
            if (it != all.images.end()) kv.second.pose = it->second.pose;
        }
        for (auto& kv : m.points3D) {
            auto it = all.points3D.find(kv.first + po);
            if (it != all.points3D.end()) kv.second.xyz = it->second.xyz;
        }
        for (auto& kv : m.cameras) {
            auto it = all.cameras.find(kv.first);
            if (it != all.cameras.end()) kv.second = it->second;
        }
    }
    return cost;
}

inline double runJointBA(std::vector<Reconstruction>& models, const BundleOptions& bopt) {
    std::vector<Reconstruction*> p;
    p.reserve(models.size());
    for (Reconstruction& m : models) p.push_back(&m);
    return runJointBA(std::move(p), bopt);
}

}  // namespace sfm
