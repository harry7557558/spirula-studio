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
#include <limits>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "sfm/ba/Priors.h"
#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Model.h"
#include "sfm/map/Profile.h"
#include "sfm/core/Log.h"

namespace sfm {

struct BundleOptions {
    RealCfg real = RealCfg::F64;
    int max_iters = 25;
    bool verbose = false;
    // Canonical uuid:<hex> of the device this solve runs on; "" = the shared
    // precedence. The int below is the CLI/API input boundary only.
    std::string device_selector;
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
    // SolverOptions::gradient_tol and metres_per_unit; 0 = no gradient stop.
    double gradient_tol = 0;
    double metres_per_unit = 1;
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
    bool refine_intrinsics = true;
    // Refine the distortion coefficients, or hold them at the setup's value.
    // COLMAP's refine_extra_params, true there and here; holding them pins the
    // principal point too, since the free set is a prefix (D72).
    bool refine_extra_params = true;
    // Refuse a solve that does not fit the device rather than attempting it --
    // for a caller that can split the problem and retry (Mapper::jointRefine).
    bool over_budget_throws = false;
    size_t host_budget_bytes = 0;
    uint64_t index_limit = UINT32_MAX;
    // ... and only for camera groups with at least this many images behind
    // them. A group of one image has no sharing at all: moving its principal
    // point is exactly a rotation of that one camera, with nothing to
    // contradict it. 0 refines every group.
    size_t pp_min_images = 20;
    // The dense/CG crossover depends on hardware; an explicit selection overrides it.
    std::string solver = "auto";
    // Persistent context (D38): device, pipelines and descriptor machinery
    // outlive one solve. The caller owns it and must keep (real, loss) fixed
    // across calls on the same context. Null = scoped context per call.
    VkContext* shared_ctx = nullptr;
    // Host worker threads, for the `cpu` scalar; 0 = hardware_concurrency.
    int threads = 0;
    // Rigs (sfm/core/Rig.h): with a table, every frame whose members have an
    // established calibration is one pose block and the member extrinsics are
    // refined; `use_rigs` off treats every image as its own frame.
    const RigTable* rigs = nullptr;
    bool use_rigs = true;
    bool refine_rigs = true;
    // Frames holding a member together with another member of its rig before
    // its extrinsic is refined rather than held; below that, the two would
    // trade off against each other.
    int rig_min_frames = 3;
    // ... and observations of the member's images in the problem.
    int rig_min_obs = 100;
    // Pose priors on the reconstruction's image ids (sfm/ba/Priors.h);
    // factors naming an image the problem lacks are dropped.
    const PosePriors* priors = nullptr;
    const std::set<uint64_t>* fixed_points = nullptr;
    const std::set<uint32_t>* fixed_images = nullptr;
    SolverStats* stats = nullptr;
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
    std::vector<std::pair<uint32_t, uint32_t>> memberOf;  // by BA member: (rig, member)
    // The priors on BA indices. P.priors points here once the layout has its
    // final address (attachPriors), never before: the struct is returned by value.
    PosePriors priors;
    void attachPriors() { P.priors = priors.empty() ? nullptr : &priors; }
};

namespace bundle_detail {

// The frame a registered image's pose block belongs to: (rig, frame) for a rig
// image whose member is calibrated, (kNoRig, image id) otherwise.
struct FrameKey {
    uint32_t rig, frame;
    bool operator<(const FrameKey& o) const {
        return rig != o.rig ? rig < o.rig : frame < o.frame;
    }
    bool operator==(const FrameKey& o) const { return rig == o.rig && frame == o.frame; }
};

inline FrameKey frameKeyOf(const Reconstruction& rec, const RigTable* rigs, bool use,
                           uint32_t image_id) {
    if (rigs && use && !rec.rig_detached.count(image_id)) {
        const RigSlot sl = rigs->slot(image_id);
        if (sl.valid() && sl.rig < rec.rigs.size() && rec.rigs[sl.rig].usable(sl.member))
            return {sl.rig, sl.frame};
    }
    return {kNoRig, image_id};
}

inline void packPose(const Pose& p, double* out) {
    const Vec3 aa = rotationToAngleAxis(p.R);
    out[0] = aa.x; out[1] = aa.y; out[2] = aa.z;
    out[3] = p.t.x; out[4] = p.t.y; out[5] = p.t.z;
}

inline Pose unpackPose(const double* v) {
    return {angleAxisToRotation({v[0], v[1], v[2]}), {v[3], v[4], v[5]}};
}

struct HostShape {
    long double images = 0, points = 0, observations = 0, copy_bytes = 0;
    long double pair_entries = 0, max_track = 0, image_span = 0;
    uint32_t max_dof = 6;
};

inline void addHostShape(HostShape& s, const Reconstruction& rec, const BundleOptions& opt) {
    uint32_t max_dof = 6;
    for (const auto& kv : rec.images) {
        s.image_span = std::max(s.image_span, (long double)kv.first + 1);
        const Image& im = kv.second;
        if (!im.registered) continue;
        s.images++;
        s.copy_bytes += 256 + im.name.size() + 16.L * im.points2D.size() +
                        8.L * im.point3D_ids.size();
    }
    for (const auto& kv : rec.points3D) {
        const size_t track = kv.second.track.size();
        s.copy_bytes += 128 + 8.L * track;
        if (track < 2 && !(opt.fixed_points && opt.fixed_points->count(kv.first) && track)) continue;
        s.points++;
        s.observations += track;
        s.pair_entries += (long double)track * (track + 1.L) / 2;
        s.max_track = std::max(s.max_track, (long double)track);
    }
    for (const auto& kv : rec.cameras) {
        const uint32_t free = (uint32_t)camNumFreeParams(kv.second.model,
            opt.refine_principal_point && opt.refine_extra_params, opt.refine_extra_params);
        max_dof = std::max(max_dof, 6 + free);
        s.copy_bytes += 512;
    }
    if (opt.rigs && opt.use_rigs && !opt.rigs->empty()) {
        if (opt.refine_rigs) max_dof = std::min(kMaxCamDof, max_dof + 6);
        for (const RigSpec& r : opt.rigs->rigs) {
            s.copy_bytes += 256 + 64.L * r.members.size() + 32.L * r.frames.size();
            for (const auto& frame : r.frames) s.copy_bytes += 4.L * frame.size();
        }
    }
    s.max_dof = std::max(s.max_dof, max_dof);
}

inline HostShape jointHostShape(const std::vector<Reconstruction*>& models,
                                const BundleOptions& opt) {
    HostShape shape;
    long double stride = 0;
    for (const Reconstruction* model : models) {
        for (const auto& kv : model->images)
            stride = std::max(stride, (long double)kv.first + 1);
        if (model->numRegistered() >= 2) addHostShape(shape, *model, opt);
    }
    if (opt.rigs && opt.use_rigs)
        stride = std::max(stride, (long double)opt.rigs->of_image.size());
    shape.image_span = stride * models.size();
    if (opt.rigs && opt.use_rigs && !opt.rigs->empty())
        shape.copy_bytes += sizeof(RigSlot) * shape.image_span;
    return shape;
}

inline size_t hostBytes(long double bytes) {
    const long double maximum = (long double)std::numeric_limits<size_t>::max();
    return bytes >= maximum ? std::numeric_limits<size_t>::max() : (size_t)std::ceil(bytes);
}

inline size_t estimateHostBytes(const HostShape& s, const BundleOptions& opt, bool copy) {
    if (s.images < 2 || s.points == 0) return 0;
    long double bytes = 65536 + 32 * s.observations + 60 * s.points +
                        384 * s.images + 4 * s.image_span + 24 * s.max_track;
    if (copy) bytes += s.copy_bytes;
    if (opt.fixed_points) bytes += 16 * s.points;
    if (opt.fixed_images) bytes += 16 * s.images;
    const long double dim = s.max_dof * s.images;
    const bool dense = opt.solver == "dense" || (opt.solver == "auto" && dim <= 8192);
    if (opt.real == RealCfg::CPU) {
        bytes += (2 * s.max_dof + 8) * 8 * s.observations + 256 * s.points +
                 8192 * s.images;
        if (dense) bytes += 4 * dim * (dim + 1);
    } else {
        bytes += 16 * s.observations + 48 * s.points;
        if (dense) bytes += 8 * s.pair_entries + 16 * s.observations;
    }
    if (opt.priors) {
        bytes += 2.L * (opt.priors->rotations.size() * sizeof(PriorRotation) +
                        opt.priors->ups.size() * sizeof(PriorUp) +
                        opt.priors->centres.size() * sizeof(PriorCentre));
    }
    return hostBytes(bytes);
}

inline void checkHostBytes(size_t need, size_t budget) {
    if (budget && need > budget)
        throw BAOverBudget((double)need / (1024 * 1024),
                           (double)budget / (1024 * 1024), "host");
}

inline size_t remainingHostBytes(size_t resident, size_t budget) {
    if (resident >= budget)
        throw BAOverBudget(((double)resident + 1) / (1024 * 1024),
                           (double)budget / (1024 * 1024), "host");
    return budget - resident;
}

}  // namespace bundle_detail

inline size_t estimateBundleHostBytes(const Reconstruction& rec, const BundleOptions& opt,
                                     bool include_copy = false) {
    bundle_detail::HostShape shape;
    bundle_detail::addHostShape(shape, rec, opt);
    return bundle_detail::estimateHostBytes(shape, opt, include_copy);
}

inline size_t estimateJointBundleHostBytes(const std::vector<Reconstruction*>& models,
                                          const BundleOptions& opt,
                                          const std::vector<const PosePriors*>* priors = nullptr) {
    const auto shape = bundle_detail::jointHostShape(models, opt);
    long double bytes = bundle_detail::estimateHostBytes(shape, opt, models.size() > 1);
    if (priors)
        for (const PosePriors* p : *priors)
            if (p) bytes += 2.L * (p->rotations.size() * sizeof(PriorRotation) +
                                  p->ups.size() * sizeof(PriorUp) +
                                  p->centres.size() * sizeof(PriorCentre));
    return bundle_detail::hostBytes(bytes);
}

// Pack `rec` into a BAProblem. Empty layout (num_images < 2) if there is
// nothing to optimize.
inline BundleLayout buildBundle(Reconstruction& rec, const BundleOptions& bopt) {
    if (rec.numRegistered() < 2 ||
        std::none_of(rec.points3D.begin(), rec.points3D.end(),
                     [&](const auto& point) { return point.second.track.size() >= 2 ||
                         (bopt.fixed_points && bopt.fixed_points->count(point.first) && !point.second.track.empty()); }))
        return BundleLayout{};
    if (bopt.host_budget_bytes)
        bundle_detail::checkHostBytes(estimateBundleHostBytes(rec, bopt), bopt.host_budget_bytes);
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
    checkBAIndexCapacity((uint64_t)max_img_id + 1, UINT32_MAX, "BA image-id span");
    std::vector<uint32_t> imgBA(max_img_id + 1, UINT32_MAX);
    // Images ordered by frame, so a rig frame's images are one contiguous pose
    // block (the host solver relies on it; sfm/ba/Problem.h).
    using bundle_detail::FrameKey;
    const RigTable* rigs = bopt.use_rigs ? bopt.rigs : nullptr;
    std::vector<std::pair<FrameKey, uint32_t>> order;
    for (auto& kv : rec.images)
        if (kv.second.registered)
            order.push_back({bundle_detail::frameKeyOf(rec, rigs, true, kv.first), kv.first});
    std::stable_sort(order.begin(), order.end(),
                     [](const std::pair<FrameKey, uint32_t>& a,
                        const std::pair<FrameKey, uint32_t>& b) { return a.first < b.first; });
    for (const auto& o : order) {
        imgBA[o.second] = (uint32_t)imgIds.size();
        imgIds.push_back(o.second);
        imgOf.push_back(&rec.images.at(o.second));
    }
    std::vector<uint64_t> ptIds;
    std::vector<Point3D*>& ptOf = L.ptOf;  // by BA index
    for (auto& kv : rec.points3D) {
        if (kv.second.track.size() < 2 &&
            !(bopt.fixed_points && bopt.fixed_points->count(kv.first) && !kv.second.track.empty())) continue;
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
    P.index_limit = bopt.index_limit;
    checkBAIndexCapacity(imgIds.size(), UINT32_MAX, "BA images");
    checkBAIndexCapacity(ptIds.size(), UINT32_MAX - 1ull, "BA points");
    P.num_images = (uint32_t)imgIds.size();
    P.num_points = (uint32_t)ptIds.size();

    // Observations, emitted point-major (which is the order the solver's tables
    // want) so the only sorting left is by image *within* one point's track --
    // a handful of elements each, instead of one global sort of millions.
    P.obs_ranges.assign(P.num_points + 1, 0);
    std::vector<uint32_t> image_obs(P.num_images, 0);
    uint64_t total_obs = 0;
    for (uint32_t p = 0; p < P.num_points; p++) {
        for (const TrackElement& e : ptOf[p]->track) {
            if (e.image_id > max_img_id) continue;
            if (imgBA[e.image_id] != UINT32_MAX) {
                total_obs++;
                checkBAIndexCapacity(total_obs, UINT32_MAX, "BA observations");
                image_obs[imgBA[e.image_id]]++;
            }
        }
        P.obs_ranges[p + 1] = (uint32_t)total_obs;
    }
    P.num_obs = (uint32_t)total_obs;

    // Frames and members. A rig frame's pose is taken from the image with the
    // most observations (its rig-mates are snapped to the calibration; the
    // solve reconciles them); a plain image is its own frame.
    P.image_frame.assign(P.num_images, 0);
    P.image_member.assign(P.num_images, kNoMember);
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> memberBA;  // (rig, member) -> index
    std::vector<uint32_t> memberCo;  // per BA member, frames shared with another member
    std::vector<std::vector<uint32_t>> frameImgs;  // per BA frame, its BA images
    for (uint32_t i = 0; i < P.num_images; i++) {
        const FrameKey key = order[i].first;
        if (i == 0 || !(order[i - 1].first == key)) frameImgs.emplace_back();
        P.image_frame[i] = (uint32_t)frameImgs.size() - 1;
        frameImgs.back().push_back(i);
        if (key.rig == kNoRig) continue;
        const RigSlot sl = rigs->slot(imgIds[i]);
        auto it = memberBA.find({sl.rig, sl.member});
        if (it == memberBA.end()) {
            it = memberBA.emplace(std::make_pair(sl.rig, sl.member), (uint32_t)L.memberOf.size()).first;
            L.memberOf.push_back({sl.rig, sl.member});
            memberCo.push_back(0);
        }
        P.image_member[i] = it->second;
    }
    P.num_frames = (uint32_t)frameImgs.size();
    if (bopt.fixed_images && !bopt.fixed_images->empty()) {
        P.fixed_frames.assign(P.num_frames, 0);
        for (uint32_t i = 0; i < P.num_images; ++i)
            if (bopt.fixed_images->count(imgIds[i])) P.fixed_frames[P.image_frame[i]] = 1;
    }
    for (const std::vector<uint32_t>& fi : frameImgs)
        if (fi.size() > 1)
            for (uint32_t i : fi) memberCo[P.image_member[i]]++;

    P.poses.resize(6 * P.num_frames);
    for (uint32_t f = 0; f < P.num_frames; f++) {
        uint32_t best = frameImgs[f][0];
        for (uint32_t i : frameImgs[f])
            if (imgOf[i]->numPoint3D() > imgOf[best]->numPoint3D()) best = i;
        const Image& im = *imgOf[best];
        Pose fp = im.pose;
        if (P.image_member[best] != kNoMember) {
            const auto& rm = L.memberOf[P.image_member[best]];
            fp = rec.rigs[rm.first].rigFromWorld(rm.second, im.pose);
        }
        bundle_detail::packPose(fp, &P.poses[6 * f]);
    }
    // Members: cam_from_rig from the calibration, refined when asked, when
    // enough frames tie the member to its rig, and when it sees enough: a known
    // member is established before it has observed anything (a lens on the sky).
    std::vector<uint32_t> memberObs(L.memberOf.size(), 0);
    std::vector<bool> memberHeld(L.memberOf.size(), false);
    for (uint32_t i = 0; i < P.num_images; i++) {
        const uint32_t m = P.image_member[i];
        if (m != kNoMember) {
            memberObs[m] += image_obs[i];
            memberHeld[m] = memberHeld[m] || P.frameFixed(P.image_frame[i]);
        }
    }
    P.members.resize(L.memberOf.size());
    P.exts.resize(6 * L.memberOf.size());
    P.ext_dim = 0;
    for (uint32_t m = 0; m < L.memberOf.size(); m++) {
        const RigCalib& c = rec.rigs[L.memberOf[m].first];
        const uint32_t member = L.memberOf[m].second;
        bundle_detail::packPose(c.cam_from_rig[member], &P.exts[6 * m]);
        const uint32_t mask = rigs->rigs[L.memberOf[m].first].members[member].dof;
        const bool held = memberHeld[m] || !bopt.refine_rigs || (int)member == c.ref ||
                          (member < c.fixed.size() && c.fixed[member]) || mask == 0 ||
                          (int)memberCo[m] < bopt.rig_min_frames ||
                          (int)memberObs[m] < bopt.rig_min_obs;
        const uint32_t nf = held ? 0u : extFreeCount(mask);
        P.members[m] = {6 * m, P.ext_dim, nf, mask};
        P.ext_dim += nf;
    }

    // Stored intrinsics include held parameters; only free parameters own columns.
    std::vector<size_t> group_images(camIds.size(), 0);
    std::vector<bool> group_active(camIds.size(), false);
    std::vector<uint32_t> img_group(P.num_images);
    for (uint32_t i = 0; i < P.num_images; i++) {
        img_group[i] = camGroup[imgOf[i]->camera_id];
        group_images[img_group[i]]++;
        group_active[img_group[i]] = group_active[img_group[i]] || !P.frameFixed(P.image_frame[i]);
    }
    P.groups.resize(camIds.size());
    P.intr.clear();
    for (size_t g = 0; g < camIds.size(); g++) {
        const Camera& c = rec.cameras[camIds[g]];
        const bool pp = bopt.refine_principal_point && bopt.refine_extra_params &&
                        group_images[g] >= bopt.pp_min_images;
        uint32_t nf = bopt.refine_intrinsics && group_active[g]
            ? (uint32_t)camNumFreeParams(c.model, pp, bopt.refine_extra_params) : 0;
        uint32_t off = (uint32_t)P.intr.size();
        uint32_t ni = (uint32_t)camNumParams(c.model);
        double ps[12];
        packIntrinsics(c, ps);
        for (uint32_t i = 0; i < ni; i++) P.intr.push_back(ps[i]);
        P.groups[g] = {off, P.free_intr, nf, (uint32_t)camBaModel(c.model)};
        P.free_intr += nf;
    }
    P.image_group = std::move(img_group);

    uint64_t jc_elements = 0;
    for (uint32_t i = 0; i < P.num_images; i++)
        jc_elements += 2ull * image_obs[i] *
                       (6 + P.memberFree(i) + P.groups[P.image_group[i]].n_intr);
    checkBAIndexCapacity(jc_elements, bopt.index_limit, "Jc pool");
    P.obs_image.resize(P.num_obs);
    P.obs_point.resize(P.num_obs);
    P.obs_xy.resize(2 * (size_t)P.num_obs);
    std::vector<std::pair<uint32_t, uint32_t>> track;
    for (uint32_t p = 0; p < P.num_points; p++) {
        track.clear();
        for (const TrackElement& e : ptOf[p]->track) {
            if (e.image_id > max_img_id || imgBA[e.image_id] == UINT32_MAX) continue;
            track.emplace_back(imgBA[e.image_id], e.point2D_idx);
        }
        std::sort(track.begin(), track.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        size_t o = P.obs_ranges[p];
        for (const auto& e : track) {
            const Vec2& xy = imgOf[e.first]->points2D[e.second];
            P.obs_image[o] = e.first;
            P.obs_point[o] = p;
            P.obs_xy[2 * o] = xy.x;
            P.obs_xy[2 * o + 1] = xy.y;
            o++;
        }
    }

    // Points.
    P.points.resize(3 * P.num_points);
    if (bopt.fixed_points) P.fixed_points.assign(P.num_points, 0);
    for (uint32_t i = 0; i < P.num_points; i++) {
        if (bopt.fixed_points) P.fixed_points[i] = uint32_t(bopt.fixed_points->count(ptIds[i]) != 0);
        const Vec3& X = ptOf[i]->xyz;
        P.points[3 * i] = X.x; P.points[3 * i + 1] = X.y; P.points[3 * i + 2] = X.z;
    }

    P.pose_dim = 6 * P.num_frames;
    P.total_intr = (uint32_t)P.intr.size();
    P.n_dim = P.pose_dim + P.ext_dim + P.free_intr;
    for (auto& m : P.members) m.ext_col += P.pose_dim;
    for (auto& g : P.groups) g.intr_col += P.pose_dim + P.ext_dim;
    finalizeTables(P);

    // Priors onto BA indices. One rotation factor per frame pair: a rig's
    // lenses each carry the chain, and both name the same two pose blocks.
    if (bopt.priors && !bopt.priors->empty()) {
        const PosePriors& in = *bopt.priors;
        PosePriors& out = L.priors;
        out.up_w = in.up_w;
        out.huber = in.huber;
        auto ba = [&](uint32_t id, uint32_t& idx) {
            if (id > max_img_id || imgBA[id] == UINT32_MAX) return false;
            idx = imgBA[id];
            return true;
        };
        std::set<std::pair<uint32_t, uint32_t>> seen;
        for (PriorRotation r : in.rotations) {
            if (!ba(r.i, r.i) || !ba(r.j, r.j)) continue;
            const uint32_t fi = P.image_frame[r.i], fj = P.image_frame[r.j];
            if (fi == fj || !seen.insert({std::min(fi, fj), std::max(fi, fj)}).second) continue;
            out.rotations.push_back(r);
        }
        for (PriorUp u : in.ups)
            if (ba(u.i, u.i)) out.ups.push_back(u);
        for (PriorCentre c : in.centres) {
            bool ok = true;
            for (int k = 0; k < c.n; k++) ok = ok && ba(c.img[k], c.img[k]);
            if (ok) out.centres.push_back(c);
        }
    }
    return L;
}

// Solver options for a mapper-driven bundle adjustment.
inline SolverOptions bundleSolverOptions(const BundleOptions& bopt) {
    SolverOptions sopt;
    sopt.real = bopt.real;
    sopt.max_iters = bopt.max_iters;
    sopt.verbose = bopt.verbose;
    sopt.device_selector = bopt.device_selector;
    sopt.device = bopt.device;
    sopt.loss = bopt.loss;
    sopt.loss_param = bopt.loss_param;
    if (bopt.rtol > 0) sopt.rtol = bopt.rtol;
    if (bopt.patience > 0) sopt.patience = bopt.patience;
    sopt.gradient_tol = bopt.gradient_tol;
    sopt.metres_per_unit = bopt.metres_per_unit;
    if (bopt.solver == "dense") sopt.solver = SolverSel::Dense;
    else if (bopt.solver == "cg") sopt.solver = SolverSel::CG;
    sopt.over_budget_throws = bopt.over_budget_throws;
    sopt.host_budget_bytes = bopt.host_budget_bytes;
    sopt.threads = bopt.threads;
    return sopt;
}

// Copy a solved problem's parameters back into the reconstruction it came from.
// `P` is the layout's own problem unless the caller moved it out to hand to a
// solver, which `spirula-sfm ba` does.
inline void writeBundle(Reconstruction& rec, const BundleLayout& L, const BAProblem& P) {
    for (uint32_t m = 0; m < P.members.size(); m++) {
        if (P.members[m].n_free == 0) continue;
        const auto& rm = L.memberOf[m];
        rec.rigs[rm.first].cam_from_rig[rm.second] = bundle_detail::unpackPose(&P.exts[6 * m]);
    }
    for (uint32_t i = 0; i < P.num_images; i++) {
        Image& im = *L.imgOf[i];
        if (P.frameFixed(P.image_frame[i])) continue;
        const Pose fp = bundle_detail::unpackPose(&P.poses[6 * P.image_frame[i]]);
        const uint32_t m = P.image_member[i];
        im.pose = m == kNoMember ? fp
                                 : rec.rigs[L.memberOf[m].first].camFromWorld(L.memberOf[m].second, fp);
    }
    for (size_t g = 0; g < L.camIds.size(); g++) {
        Camera& c = rec.cameras[L.camIds[g]];
        unpackIntrinsics(c, &P.intr[P.groups[g].intr_offset]);
    }
    for (uint32_t i = 0; i < P.num_points; i++)
        if (P.fixed_points.empty() || !P.fixed_points[i])
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

struct BundleRun {
    SolverStats stats;
    RealCfg real = RealCfg::F64;  // what the solve that finished ran in
    double t_init = 0, t_solve = 0;
};

inline size_t bundleProblemHostBytes(const BAProblem& P) {
    long double bytes = 0;
    auto add = [&](const auto& values) {
        bytes += (long double)values.capacity() * sizeof(*values.data());
    };
    add(P.image_frame); add(P.image_member); add(P.members);
    add(P.obs_image); add(P.obs_point); add(P.obs_xy); add(P.obs_ranges);
    add(P.image_group); add(P.groups); add(P.poses); add(P.exts); add(P.intr); add(P.points); add(P.fixed_points); add(P.fixed_frames);
    add(P.model_obs); add(P.model_ranges); add(P.jc_off);
    add(P.pair_entries); add(P.pair_chunks);
    add(P.cam_obs_ranges); add(P.cam_obs); add(P.cam_chunks); add(P.prec_blocks);
    return bundle_detail::hostBytes(bytes);
}

// Solve `P` in place, on the host from the device's last checkpoint if the
// device fails. With `rebuild` (makes `P` anew) the device solve frees P's
// tables once uploaded, and a host solve takes a rebuilt copy.
inline BundleRun solveBundle(BAProblem& P, SolverOptions sopt, VkContext* shared,
                             const std::function<BAProblem()>& rebuild = {}) {
    BundleRun r;
    if (sopt.host_budget_bytes) {
        const size_t resident = bundleProblemHostBytes(P);
        sopt.host_budget_bytes = bundle_detail::remainingHostBytes(resident,
                                                                 sopt.host_budget_bytes);
    }
    if (P.num_obs >= baHostObsThreshold().load()) sopt.real = RealCfg::CPU;
    SolverCheckpoint ck;
    sopt.checkpoint = &ck;
    bool released = false;
    auto attempt = [&] {
        auto t0 = std::chrono::steady_clock::now();
        BundleSolver solver(P, sopt, shared);
        solver.init();
        if (rebuild) released = solver.releaseHostTables() || released;
        auto t1 = std::chrono::steady_clock::now();
        solver.solve();
        auto t2 = std::chrono::steady_clock::now();
        solver.downloadParams();
        r.stats = solver.stats();
        r.real = solver.real();
        r.t_init = std::chrono::duration<double>(t1 - t0).count();
        r.t_solve = std::chrono::duration<double>(t2 - t1).count();
    };
    try {
        attempt();
    } catch (const VkError& e) {
        if (!vkErrorIsResourceFailure(e.result)) throw;
        noteBaDeviceFailure(e, P.num_obs);
        if (released) {
            BAProblem fresh = rebuild();
            fresh.poses = std::move(P.poses);
            fresh.exts = std::move(P.exts);
            fresh.intr = std::move(P.intr);
            fresh.points = std::move(P.points);
            fresh.priors = P.priors;
            P = std::move(fresh);
        }
        sopt.real = RealCfg::CPU;
        sopt.checkpoint = nullptr;
        if (ck.iterations > 0) {
            sopt.init_damping = ck.damping;
            sopt.max_iters = std::max(1, sopt.max_iters - ck.iterations);
            slog::diag(slog::Tag::Map, "[ba] resuming on the host at iteration %d, cost %.6e",
                       ck.iterations, ck.cost);
        }
        attempt();
        r.stats.iterations += ck.iterations;
    }
    return r;
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
    L.attachPriors();
    BAProblem& P = L.P;
    if (P.num_images < 2) return 0;
    double t_build = prof_lap();

    const BundleRun run = solveBundle(P, bundleSolverOptions(bopt), bopt.shared_ctx,
                                      [&] { return buildBundle(rec, bopt).P; });
    const SolverStats& stats = run.stats;
    if (bopt.stats) *bopt.stats = stats;
    const double t_init = run.t_init, t_solve = run.t_solve;
    prof_lap();
    writeBundle(rec, L, P);

    double t_write = prof_lap();
    g_map_prof.ba_build += t_build;
    g_map_prof.ba_init += t_init;
    g_map_prof.ba_solve += t_solve;
    g_map_prof.ba_write += t_write;
    g_map_prof.n_ba++;
    g_map_prof.n_ba_iters += stats.iterations;
    char grad[64] = "";
    if (!stats.gradient_norms.empty())
        std::snprintf(grad, sizeof grad, ", gradient %.3e%s", stats.gradient_norms.back(),
                      stats.gradient_stop ? " (stop)" : "");
    if (MapProf::enabled())
        slog::diag(slog::Tag::Map,
                   "[prof] BA #%ld: %u img %u pt %u obs | build %.3f init %.3f solve %.3f "
                   "write %.3f s | %d LM iters, %s%s%s | prior %.3f -> %.3f, %d prior-driven, "
                   "final damping %.1e, cost %.6e -> %.6e%s",
                   (long)g_map_prof.n_ba, P.num_images, P.num_points, P.num_obs, t_build, t_init,
                   t_solve, t_write, stats.iterations, stats.solver, stats.jac32 ? " fp32-J" : "",
                   stats.cg_solves ? (" " + std::to_string((int)std::lround(
                                                 stats.cg_iters_total / stats.cg_solves)) +
                                      " its/solve").c_str()
                                   : "",
                   stats.prior_initial, stats.prior_final, stats.prior_steps,
                   stats.final_damping, stats.initial_cost, stats.final_cost, grad);
    return stats.final_cost;
}

// ---- joint refinement of several components (D45) -------------------------

// One lens took every component, so its intrinsics are one set of unknowns:
// every model goes into one BAProblem under its own id range, the camera ids
// shared. `priors[k]`, when given, are model k's factors on its own image ids.
inline double runJointBA(std::vector<Reconstruction*> models, const BundleOptions& bopt,
                         const std::vector<const PosePriors*>* priors = nullptr) {
    if (models.empty()) return 0;
    size_t live = 0;
    for (const Reconstruction* m : models)
        if (m->numRegistered() >= 2) live++;
    if (live == 0) return 0;
    if (live == 1 && models.size() == 1) {
        BundleOptions one = bopt;
        one.priors = priors && !priors->empty() ? (*priors)[0] : nullptr;
        return runGlobalBA(*models[0], one);
    }
    if (bopt.host_budget_bytes)
        bundle_detail::checkHostBytes(estimateJointBundleHostBytes(models, bopt, priors),
                                     bopt.host_budget_bytes);

    // Id strides, so a merged view can be split apart again unambiguously.
    uint64_t wide_img_stride = 0;
    uint64_t pt_stride = 0;
    for (const Reconstruction* m : models) {
        for (const auto& kv : m->images)
            wide_img_stride = std::max(wide_img_stride, (uint64_t)kv.first + 1);
        for (const auto& kv : m->points3D) pt_stride = std::max(pt_stride, kv.first + 1);
    }
    if (wide_img_stride == 0 || pt_stride == 0) return 0;
    // A rig frame names images a component may not hold; the stride has to
    // clear every id the table can produce, not only the ones present.
    if (bopt.rigs && bopt.use_rigs)
        wide_img_stride = std::max(wide_img_stride, (uint64_t)bopt.rigs->of_image.size());
    checkBAIndexCapacity(wide_img_stride * models.size(), UINT32_MAX, "joint image-id span");
    const uint32_t img_stride = (uint32_t)wide_img_stride;

    Reconstruction all;
    // Each model's priors travel with its shifted image ids; the up axis is
    // per model too, so the stacked problem takes it from the first model
    // that has up factors and drops the others' (their gauges differ).
    PosePriors joint_priors;
    bool joint_up = false;
    // Rigs: each component keeps its own calibration (its own scale), so the
    // stacked problem gets one copy of the table per component, image ids
    // shifted with the component, and one calibration set per copy.
    RigTable joint_rigs;
    const bool rigs = bopt.rigs && bopt.use_rigs && !bopt.rigs->empty();
    std::vector<uint32_t> rig_base(models.size(), 0);
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
        if (rigs) {
            rig_base[mi] = (uint32_t)joint_rigs.rigs.size();
            for (const RigSpec& r : bopt.rigs->rigs) {
                RigSpec c = r;
                for (auto& fr : c.frames)
                    for (uint32_t& img : fr) {
                        if (img == kNoImage) continue;
                        auto it = m.images.find(img);
                        img = it != m.images.end() && it->second.registered ? img + io : kNoImage;
                    }
                joint_rigs.rigs.push_back(std::move(c));
            }
            std::vector<RigCalib> calib = m.rigs;
            calib.resize(bopt.rigs->rigs.size());
            all.rigs.insert(all.rigs.end(), calib.begin(), calib.end());
        }
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
        if (priors && mi < priors->size() && (*priors)[mi] && !(*priors)[mi]->empty()) {
            const PosePriors& pr = *(*priors)[mi];
            joint_priors.huber = pr.huber;
            for (PriorRotation r : pr.rotations) {
                r.i += io;
                r.j += io;
                joint_priors.rotations.push_back(r);
            }
            if (!pr.ups.empty() && (!joint_up || pr.up_w.dot(joint_priors.up_w) > 0.9999)) {
                if (!joint_up) joint_priors.up_w = pr.up_w;
                joint_up = true;
                for (PriorUp u : pr.ups) {
                    u.i += io;
                    joint_priors.ups.push_back(u);
                }
            }
            for (PriorCentre c : pr.centres) {
                for (int k = 0; k < c.n; k++) c.img[k] += io;
                joint_priors.centres.push_back(c);
            }
        }
    }
    if (all.images.size() < 2 || all.points3D.empty()) return 0;

    BundleOptions jopt = bopt;
    std::set<uint32_t> joint_fixed_images;
    std::set<uint64_t> joint_fixed_points;
    for (size_t mi = 0; mi < models.size(); ++mi) {
        if (bopt.fixed_images) for (uint32_t id : *bopt.fixed_images)
            if (models[mi]->images.count(id)) joint_fixed_images.insert(id + uint32_t(mi) * img_stride);
        if (bopt.fixed_points) for (uint64_t id : *bopt.fixed_points)
            if (models[mi]->points3D.count(id)) joint_fixed_points.insert(id + uint64_t(mi) * pt_stride);
    }
    if (bopt.fixed_images) jopt.fixed_images = &joint_fixed_images;
    if (bopt.fixed_points) jopt.fixed_points = &joint_fixed_points;
    if (jopt.host_budget_bytes) {
        const auto shape = bundle_detail::jointHostShape(models, bopt);
        const size_t copy = bundle_detail::hostBytes(shape.copy_bytes);
        jopt.host_budget_bytes = bundle_detail::remainingHostBytes(copy, jopt.host_budget_bytes);
    }
    jopt.priors = joint_priors.empty() ? nullptr : &joint_priors;
    if (rigs) {
        joint_rigs.index((size_t)models.size() * img_stride);
        jopt.rigs = &joint_rigs;
    }
    double cost = runGlobalBA(all, jopt);

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
        if (rigs)
            for (size_t r = 0; r < bopt.rigs->rigs.size() && r < m.rigs.size(); r++)
                m.rigs[r] = all.rigs[rig_base[mi] + r];
    }
    return cost;
}

inline double runJointBA(std::vector<Reconstruction>& models, const BundleOptions& bopt,
                         const std::vector<const PosePriors*>* priors = nullptr) {
    std::vector<Reconstruction*> p;
    p.reserve(models.size());
    for (Reconstruction& m : models) p.push_back(&m);
    return runJointBA(std::move(p), bopt, priors);
}

}  // namespace sfm
