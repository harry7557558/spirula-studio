// Reconstruction merging -- COLMAP's `model_merger` (docs/notes/sfm-design.md D43).
//
// A capture that is not one connected view graph reconstructs as several models
// (D41), each in its own gauge. Two models that registered some of the same
// images can still be fused: those shared images give the similarity transform
// (rotation, translation, scale) between the two worlds, and everything else
// follows -- poses are transformed, tracks that meet at a shared feature are
// spliced, tracks that do not become new points.
//
// Three pieces, deliberately separable because a GUI will drive them
// individually (the model viewer, where a user says "merge this
// one into that one" or supplies the transform themselves):
//
//   alignReconstructions()  shared images -> Sim3, by RANSAC over image
//                           centers scored in *pixels* (a candidate transform
//                           predicts each shared image's pose; the residual is
//                           how far the other model's points then reproject).
//   mergeInto()             mechanical: apply a Sim3, splice tracks, filter.
//   MergeSession            the policy: which pairs to try, in what order,
//                           and what to do when one turns out badly.
//
// Failure handling mirrors the mapper's (D36): a merge is attempted on a copy,
// validated, and committed only if the result is sound -- a bad alignment
// leaves the models exactly as they were, the pair is remembered as failed, and
// the search moves on. A later successful merge revives those pairs, because a
// model that has grown may now share enough with the one it could not reach.
//
// Requirement inherited from COLMAP: both models must come from the same
// features, so that in a shared image `point2D_idx` means the same keypoint in
// both. Violations are detected (name / keypoint-count mismatch on a shared
// id) and refused rather than merged into garbage.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/geometry/Triangulation.h"
#include "sfm/optim/Ransac.h"
#include "sfm/core/Log.h"

namespace sfm {

// ---- duplicate structure (D45) -------------------------------------------
//
// The failure no other test here catches. A capture that walks past two
// similar-looking parts of a building -- two wings of a symmetric facade, two
// identical corridors -- produces matches between them that are perfectly
// self-consistent, so every check based on the model's own agreement passes.
// The result is a *fold*: two physically different places written on top of
// each other, one of them typically rotated.
//
// What gives it away is what is *missing*. If the model puts two images in the
// same place looking the same way, they are looking at the same thing, and a
// reconstruction that believed that would have triangulated the same points
// from both. Measured over co-located, co-oriented image pairs of that model:
// pairs on the same side of the fold share a median of 454 points, pairs
// across it share 0 (73% share fewer than 5). A model with no fold has no such
// pairs at all.
//
// This is the "missing correspondences" signal of the duplicate-structure
// literature (Heinly et al.), computed from the reconstruction alone.

// "Have these two images been matched to each other, with real support?" --
// supplied by whoever holds the correspondence graph (the mapper does; the
// merger does not). Without it the test below misfires: a densely captured
// room has plenty of co-located images that share no *point ids* simply
// because their tracks were split, and calling those a fold cut drjohnson's
// sound 261-image model into 144+96+20. Two images that really are the same
// place were matched and verified long ago; two copies of a similar place
// were not.
struct DuplicateOptions {
    // A pair counts as co-located when the camera centres are within this many
    // median nearest-neighbour distances -- a scale the model itself defines,
    // so no world units are assumed.
    double radius_nn = 2.0;
    double min_view_cos = 0.5;      // 60 deg between principal axes
    int min_image_points = 50;      // an image with less structure cannot testify
    double max_shared_frac = 0.02;  // shared / min(observed) below this = conflict
    // How many conflicting pairs it takes to consider a model folded at all. A
    // stray one or two happen in sound models, and three cannot-link constraints
    // are also the least that can force a meaningful cut.
    int min_conflicts = 3;
    // The conflict *rate* is deliberately not a criterion (0 = off). It fails
    // in both directions. What decides is max_cut_fraction below. Kept as a
    // parameter because it is worth being able to re-measure, not because a
    // value above 0 is recommended.
    double min_conflict_ratio = 0.0;
    // How much co-visibility the split may sever, as a fraction of the model's
    // total (DuplicateCut::fraction). *This* is the test that separates a fold
    // from a dense capture. Two places written on top of each other share
    // nothing to begin with, so pulling them apart costs almost no real
    // co-visibility; cutting a sound model always runs through structure it
    // genuinely shares. Note this is a veto, not a trigger: the conflicts still
    // have to force a cut before there is anything to measure, which is what
    // stops it from lopping the end off a linear capture (a cheap cut, but no
    // co-located pair to demand it).
    //
    // "Almost no" has to be taken literally. A real fold's cut severs *zero*
    // co-visibility -- that is what the synthetic case in sfm_merge_test
    // measures, and it is what the argument above predicts. The one sound model
    // in this corpus that the conflicts talked into a cut severed 1.30%, and
    // splitting it cost 568 of its 1243 images and 65 points of AUC. So the
    // veto sits where the two are separated by two orders of magnitude, not by
    // a factor of 1.5.
    double max_cut_fraction = 0.005;
    // A group the cut produced is a fold only if it is written *on top of*
    // something: this fraction of its images must sit where an image outside it
    // also sits (the same co-location test as above). A group that fails this
    // is not a duplicate of anywhere -- it is a part of the capture the
    // conflicts happened to sever -- and it is put back rather than written out
    // as its own model (D67).
    //
    // The two tests above are both about the *cut*, and neither asks whether
    // the pieces coincide. On a 6112-image capture six conflicting pairs out of
    // 557 tore 358 images off a settled 5500-image model through a cut costing
    // 0.08% of its co-visibility -- passing both -- and those 358 images were
    // the difference between 90% and 84% coverage. Six pairs can only put six
    // images on top of anything.
    double min_fold_overlap = 0.5;
};

struct DuplicateReport {
    size_t colocated = 0;   // co-located, co-oriented pairs examined
    size_t conflicts = 0;   // ... that share (almost) no structure and never matched
    size_t unmatched_but_seen = 0;  // ... that share none but *were* matched
    std::vector<std::pair<uint32_t, uint32_t>> pairs;  // the conflicting ones
    // Every co-located pair, conflicting or not. This is what says whether a
    // group the cut produced sits on top of anything (min_fold_overlap); the
    // conflicts alone cannot, since the cut is derived from them and they
    // straddle it by construction. Small in practice -- the radius and
    // orientation tests are tight, and a 5441-image model yielded 789.
    std::vector<std::pair<uint32_t, uint32_t>> colocated_pairs;
    double ratio() const { return colocated ? (double)conflicts / (double)colocated : 0.0; }
    bool duplicated(const DuplicateOptions& o) const {
        return (int)conflicts >= o.min_conflicts && ratio() >= o.min_conflict_ratio;
    }
};

inline DuplicateReport findDuplicateStructure(const Reconstruction& m,
                                              const DuplicateOptions& opt = {},
                                              const MatchedFn& matched = nullptr) {
    DuplicateReport rep;
    std::vector<uint32_t> ids;
    std::vector<Vec3> C, fwd;
    std::vector<std::vector<uint64_t>> pts;
    for (const auto& kv : m.images) {
        if (!kv.second.registered) continue;
        std::vector<uint64_t> p = observedPoints(kv.second);
        if ((int)p.size() < opt.min_image_points) continue;
        ids.push_back(kv.first);
        C.push_back(cameraCenter(kv.second.pose));
        // third row of R is the principal axis in world coordinates
        const Mat3& R = kv.second.pose.R;
        fwd.push_back({R[6], R[7], R[8]});
        pts.push_back(std::move(p));
    }
    const size_t n = ids.size();
    if (n < 4) return rep;

    // The model's own length scale: the median distance to a nearest neighbour.
    std::vector<double> nn(n, 1e300);
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            double d2 = (C[i] - C[j]).dot(C[i] - C[j]);
            nn[i] = std::min(nn[i], d2);
            nn[j] = std::min(nn[j], d2);
        }
    std::vector<double> sorted = nn;
    std::sort(sorted.begin(), sorted.end());
    const double med = std::sqrt(std::max(1e-24, sorted[sorted.size() / 2]));
    const double r2 = (opt.radius_nn * med) * (opt.radius_nn * med);

    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            Vec3 d = C[i] - C[j];
            if (d.dot(d) > r2) continue;
            if (fwd[i].dot(fwd[j]) < opt.min_view_cos) continue;
            rep.colocated++;
            rep.colocated_pairs.push_back({ids[i], ids[j]});
            size_t sh = sharedPoints(pts[i], pts[j]);
            double frac = (double)sh / (double)std::min(pts[i].size(), pts[j].size());
            if (frac >= opt.max_shared_frac) continue;
            // No shared structure. Only a conflict if they were never matched
            // either -- otherwise the model is right and its tracks are split.
            if (matched && matched(ids[i], ids[j])) {
                rep.unmatched_but_seen++;
                continue;
            }
            rep.conflicts++;
            rep.pairs.push_back({ids[i], ids[j]});
        }
    return rep;
}

// Pull a folded model apart along the conflicts.
//
// The conflicting pairs say "these two images cannot be the same place". The
// co-visibility graph -- images weighted by the 3D points they share -- says
// how strongly everything else belongs together, and the false glue that
// caused the fold is by construction its *weakest* link across the seam. So:
// grow components by co-visibility, strongest edge first, and refuse any union
// that would put two conflicting images together (Kruskal with constraints).
// The copies form first, and the glue is rejected last.
//
// Groups smaller than `min_group` are dropped; their images go back to being
// unregistered, where a later growth pass can offer them a place again.
//
// `cut_out` reports what the split cost in co-visibility, which is how the
// caller decides whether to believe it (DuplicateOptions::max_cut_fraction).
struct DuplicateCut {
    size_t groups = 0;   // components Kruskal produced
    uint64_t severed = 0;  // co-visibility weight between different components
    uint64_t kept = 0;     // ... within one component
    // Of all the co-visibility in the model, the share the split throws away.
    double fraction() const {
        const uint64_t t = severed + kept;
        return t ? (double)severed / (double)t : 0.0;
    }
    // Of the groups the cut proposed, how many were put back for sitting on top
    // of nothing, and the weakest overlap among those that survived
    // (min_fold_overlap). 1.0 with nothing to report.
    size_t reattached = 0;
    double min_overlap = 1.0;
};

// The verdict: this model is folded and the split it implies is worth making.
// Both halves matter -- the conflicts say a fold is *possible*, the cut says
// the split is *cheap*, and only a genuine fold is both.
inline bool foldSplitAccepted(const DuplicateReport& rep, const DuplicateCut& cut,
                              const DuplicateOptions& opt) {
    return rep.duplicated(opt) && cut.groups > 1 && cut.fraction() <= opt.max_cut_fraction;
}

inline std::vector<Reconstruction> splitDuplicateStructure(const Reconstruction& m,
                                                           const DuplicateReport& rep,
                                                           size_t min_group,
                                                           size_t* dropped_out = nullptr,
                                                           DuplicateCut* cut_out = nullptr,
                                                           double min_fold_overlap = 0.0) {
    if (rep.pairs.empty()) return {m};
    std::vector<uint32_t> ids;
    std::map<uint32_t, size_t> pos;
    for (const auto& kv : m.images)
        if (kv.second.registered) { pos[kv.first] = ids.size(); ids.push_back(kv.first); }
    if (ids.size() < 2) return {m};

    // Co-visibility from the tracks: cost is sum of |track|^2, not n^2.
    std::map<std::pair<size_t, size_t>, uint32_t> covis;
    for (const auto& kv : m.points3D) {
        const std::vector<TrackElement>& t = kv.second.track;
        if (t.size() > 64) continue;  // a point seen by everything says nothing
        for (size_t a = 0; a < t.size(); a++)
            for (size_t b = a + 1; b < t.size(); b++) {
                auto ia = pos.find(t[a].image_id), ib = pos.find(t[b].image_id);
                if (ia == pos.end() || ib == pos.end()) continue;
                size_t x = ia->second, y = ib->second;
                if (x > y) std::swap(x, y);
                covis[{x, y}]++;
            }
    }
    struct Edge { uint32_t w; size_t a, b; };
    std::vector<Edge> edges;
    edges.reserve(covis.size());
    for (const auto& kv : covis) edges.push_back({kv.second, kv.first.first, kv.first.second});
    std::sort(edges.begin(), edges.end(),
              [](const Edge& x, const Edge& y) { return x.w > y.w; });

    std::vector<size_t> parent(ids.size());
    for (size_t i = 0; i < parent.size(); i++) parent[i] = i;
    std::function<size_t(size_t)> find = [&](size_t x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    std::vector<std::pair<size_t, size_t>> conflict;
    for (const auto& p : rep.pairs) {
        auto a = pos.find(p.first), b = pos.find(p.second);
        if (a != pos.end() && b != pos.end()) conflict.push_back({a->second, b->second});
    }
    for (const Edge& e : edges) {
        size_t ra = find(e.a), rb = find(e.b);
        if (ra == rb) continue;
        bool forbidden = false;
        for (const auto& c : conflict) {
            size_t rc = find(c.first), rd = find(c.second);
            if ((rc == ra && rd == rb) || (rc == rb && rd == ra)) { forbidden = true; break; }
        }
        if (forbidden) continue;
        parent[ra] = rb;
    }

    std::map<size_t, std::vector<uint32_t>> groups;
    for (size_t i = 0; i < ids.size(); i++) groups[find(i)].push_back(ids[i]);
    std::vector<std::vector<uint32_t>> gs;
    for (auto& kv : groups) gs.push_back(std::move(kv.second));
    std::sort(gs.begin(), gs.end(),
              [](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
                  return a.size() > b.size();
              });
    if (gs.size() <= 1) return {m};

    // Is each group written on top of something? A fold's two copies occupy the
    // same space, so nearly every image of the smaller copy stands where an
    // image of the other one stands. A group that does not -- a stretch of the
    // capture the conflicts happened to sever -- goes back into the largest
    // group instead of becoming a model of its own (D67).
    if (min_fold_overlap > 0.0 && !rep.colocated_pairs.empty()) {
        std::map<uint32_t, size_t> group_of;
        for (size_t g = 0; g < gs.size(); g++)
            for (uint32_t id : gs[g]) group_of[id] = g;
        std::vector<std::set<uint32_t>> covered(gs.size());
        for (const auto& p : rep.colocated_pairs) {
            auto a = group_of.find(p.first), b = group_of.find(p.second);
            if (a == group_of.end() || b == group_of.end() || a->second == b->second) continue;
            covered[a->second].insert(p.first);
            covered[b->second].insert(p.second);
        }
        std::vector<std::vector<uint32_t>> keep;
        keep.push_back(std::move(gs[0]));  // the largest is what the rest fall back into
        for (size_t g = 1; g < gs.size(); g++) {
            const double ov = (double)covered[g].size() / (double)gs[g].size();
            if (ov >= min_fold_overlap) {
                if (cut_out) cut_out->min_overlap = std::min(cut_out->min_overlap, ov);
                keep.push_back(std::move(gs[g]));
            } else {
                if (cut_out) cut_out->reattached++;
                keep[0].insert(keep[0].end(), gs[g].begin(), gs[g].end());
            }
        }
        gs = std::move(keep);
        std::sort(gs.begin(), gs.end(),
                  [](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
                      return a.size() > b.size();
                  });
        if (gs.size() <= 1) return {m};
    }

    // What the cut costs, measured on the split as it now stands rather than on
    // the one Kruskal proposed -- an edge into a re-attached group is not
    // severed any more, and the caller's veto (max_cut_fraction) has to see the
    // split it is actually vetoing. Every co-visibility edge is either inside a
    // group or across the cut; a genuine fold severs almost nothing, because
    // the two copies never shared structure in the first place.
    if (cut_out) {
        std::vector<size_t> gid(ids.size(), (size_t)-1);
        for (size_t g = 0; g < gs.size(); g++)
            for (uint32_t id : gs[g]) gid[pos.at(id)] = g;
        for (const auto& kv : covis) {
            if (gid[kv.first.first] == gid[kv.first.second]) cut_out->kept += kv.second;
            else cut_out->severed += kv.second;
        }
        cut_out->groups = gs.size();
    }

    std::vector<Reconstruction> parts;
    size_t dropped = 0;
    for (const std::vector<uint32_t>& g : gs) {
        if (g.size() < min_group) { dropped += g.size(); continue; }
        parts.push_back(subsetModel(m, std::set<uint32_t>(g.begin(), g.end())));
    }
    if (dropped_out) *dropped_out = dropped;
    if (parts.empty()) return {m};
    return parts;
}

// Declared here for MergeOptions::validate; defined with mergeInto below.
struct MergeCounts;

struct MergeOptions {
    // Alignment. `max_reproj_error` is the RANSAC inlier threshold in pixels
    // (COLMAP's model_merger default is 8 px -- looser than the mapper's 4,
    // because the two models were optimized independently and their shared
    // poses genuinely differ by more than a well-converged BA residual).
    int min_common_images = 3;         // 2 shared poses determine a Sim3; 3 gives a vote
    double max_reproj_error = 8.0;
    double min_inlier_ratio = 0.3;     // COLMAP's kMinInlierObservations
    int max_alignment_points = 100;    // 3D points sampled per shared image
    int ransac_max_trials = 1000;
    unsigned seed = 0;
    // Post-merge filtering, matching the mapper's defaults: the spliced tracks
    // have never seen a bundle adjustment across the seam, so the observations
    // that do not survive the mapper's own criteria should not be kept.
    double filter_reproj_error = 4.0;
    double min_tri_angle_deg = 1.5;
    // Acceptance of the merged result (the "detect failed merges" half). An
    // alignment can pass RANSAC on a repeated structure -- two facades of the
    // same building -- and place the incoming images somewhere plausible but
    // wrong. What that looks like afterwards is images arriving with nothing
    // that survives filtering, or the anchor model losing observations it had
    // before. Both are checked; either one undoes the merge.
    int min_image_points = 5;          // an image below this is "hollow"
    double max_hollow_ratio = 0.34;    // of the images the merge added
    double max_anchor_obs_loss = 0.2;  // of the anchor's observations
    double max_splice_conflict_ratio = 0.5;  // of the points the two models share
    // ... unless the alignment itself is this well determined, in which case
    // the disagreement is arbitrated rather than fatal (D64).
    //
    // A shared image is a pose correspondence: both models place *that* image,
    // and the transform has to satisfy all of them at once. When dozens of them
    // agree, the two models are looking at the same place from the same spots --
    // whatever else is wrong, it is not that one of them is somewhere else. What
    // the spliced points then disagree about is the *shape*: two long walks that
    // each drifted their own way cannot both be right, and one similarity cannot
    // straighten either. That is a bundle adjustment's job, so the verdict is
    // handed to `validate`, which can refine the result and judge it on evidence
    // the alignment never used. Measured on a 5356-image capture: two components
    // of 3459 and 2974 images sharing 1291 of them, refused here for disagreeing
    // about 578129 of the 900671 points they both triangulated.
    //
    // 0 restores the unconditional refusal.
    int splice_arbitrate_inliers = 20;
    // Folded results (findDuplicateStructure). A merge that puts two parts of
    // the capture on top of each other passes every test above -- the fold is
    // self-consistent -- but leaves pairs of images the merged model places in
    // the same spot with no structure in common. Only a fold the *merge*
    // introduced counts: an anchor that already had some keeps whatever it had.
    // Off by default based on real-world dataset measurements. A fold is cheaper
    // to cut once at the end than to legislate against at every merge;
    // ModelManager does exactly that.
    bool check_duplicate = false;
    DuplicateOptions duplicate;
    // How far apart, in pixels, the two models' versions of one point may
    // reproject before they count as disagreeing. Deliberately NOT tied to
    // max_reproj_error: loosening the alignment threshold to get a hard pair
    // to align must not also switch off the check that catches it when the
    // result is wrong.
    double splice_tolerance = 8.0;
    // An outside opinion on a candidate result, run after every built-in test
    // passes (D45). The merger only ever sees the two models, so everything it
    // can check is computed from the evidence the alignment already used; a
    // caller that also holds the correspondence graph can ask a question the
    // merger cannot -- see Mapper::checkSeam, which tests the merged model
    // against the verified two-view geometries that cross the seam. Returns an
    // empty string to accept, or the reason to refuse. Also the natural place
    // for a GUI to hang "ask the user" on.
    //
    // `merged` is mutable, and a validator that replaces it commits what it
    // left behind. A judge that can repair what it judges is worth more than
    // one that can only refuse: the cross-seam test is a pixel measurement on a
    // model no bundle adjustment has seen, so its way of confirming a doubt is
    // to optimize the seam and look again -- and then the optimized model is
    // the one that should be kept (D64).
    std::function<std::string(Reconstruction& merged, const Reconstruction& src,
                              const Sim3& transform, const MergeCounts& counts)> validate;
    bool verbose = true;
    // Rigs (sfm/core/Rig.h): two models holding different lenses of one frame
    // share a pose correspondence through the calibration, so they align and
    // count as overlapping without sharing an image.
    const RigTable* rigs = nullptr;
};

// ---- least-squares similarity (Umeyama 1991) ----------------------------
// The transform taking `src` onto `dst`, minimizing sum |dst_i - (s R src_i + t)|^2.
inline bool estimateSim3(const std::vector<Vec3>& src, const std::vector<Vec3>& dst, Sim3& out) {
    const size_t n = src.size();
    if (n < 3 || dst.size() != n) return false;
    Vec3 ms{0, 0, 0}, md{0, 0, 0};
    for (size_t i = 0; i < n; i++) { ms = ms + src[i]; md = md + dst[i]; }
    ms = ms * (1.0 / n);
    md = md * (1.0 / n);
    Mat3 sigma{};
    double var_s = 0;
    for (size_t i = 0; i < n; i++) {
        Vec3 a = src[i] - ms, b = dst[i] - md;
        var_s += a.dot(a);
        const double av[3] = {a.x, a.y, a.z}, bv[3] = {b.x, b.y, b.z};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) sigma[3 * r + c] += bv[r] * av[c];
    }
    var_s /= (double)n;
    for (double& v : sigma) v /= (double)n;
    if (!(var_s > 1e-12)) return false;  // all source centers coincide

    Svd3 s = svd3(sigma);
    // det(U)det(V) < 0 means the plain U V^T is a reflection; flipping the
    // smallest singular direction gives the best proper rotation.
    Mat3 D = mat3Identity();
    const double flip = det3(s.U) * det3(s.V) < 0 ? -1.0 : 1.0;
    D[8] = flip;
    out.R = mul(mul(s.U, D), transpose(s.V));
    const double trace = s.s.x + s.s.y + flip * s.s.z;
    out.scale = trace / var_s;
    if (!(out.scale > 1e-12) || !std::isfinite(out.scale)) return false;
    out.t = md - mul(out.R, ms) * out.scale;
    for (double v : out.R)
        if (!std::isfinite(v)) return false;
    return std::isfinite(out.t.x) && std::isfinite(out.t.y) && std::isfinite(out.t.z);
}

// The similarity taking the `src` poses onto the `dst` poses.
//
// A shared image is a *pose* correspondence, not just a point, and using the
// rotations is what makes this work where the point version does not. From
// transformPose, the transformed pose has rotation R_src * T.R^T, so matching
// it to R_dst gives T.R = R_dst^T R_src from one image alone -- no geometric
// configuration required. Scale and translation then come from the centers
// with the rotation already fixed, so two images suffice.
//
// The alternative -- Umeyama over three camera centers -- is degenerate
// exactly where captures fragment: a corridor or a facade walked in sequence
// puts the shared images on a line, where three centers leave the rotation
// about that line free and the estimate is noise.
inline bool estimateSim3FromPoses(const std::vector<Pose>& src, const std::vector<Pose>& dst,
                                  Sim3& out) {
    const size_t n = src.size();
    if (n < 2 || dst.size() != n) return false;
    // Chordal rotation average of the per-image estimates: sum them and
    // project back onto SO(3).
    Mat3 acc{};
    for (size_t i = 0; i < n; i++) {
        Mat3 Ri = mul(transpose(dst[i].R), src[i].R);
        for (int k = 0; k < 9; k++) acc[k] += Ri[k];
    }
    Svd3 s = svd3(acc);
    Mat3 D = mat3Identity();
    D[8] = det3(s.U) * det3(s.V) < 0 ? -1.0 : 1.0;
    out.R = mul(mul(s.U, D), transpose(s.V));

    // With the rotation fixed, scale and translation are a 1-D least squares
    // over the centers.
    std::vector<Vec3> cs(n), cd(n);
    Vec3 ms{0, 0, 0}, md{0, 0, 0};
    for (size_t i = 0; i < n; i++) {
        cs[i] = cameraCenter(src[i]);
        cd[i] = cameraCenter(dst[i]);
        ms = ms + cs[i];
        md = md + cd[i];
    }
    ms = ms * (1.0 / n);
    md = md * (1.0 / n);
    double num = 0, den = 0;
    for (size_t i = 0; i < n; i++) {
        Vec3 a = mul(out.R, cs[i] - ms), b = cd[i] - md;
        num += a.dot(b);
        den += a.dot(a);
    }
    if (!(den > 1e-12)) return false;  // every shared image at the same place
    out.scale = num / den;
    if (!(out.scale > 1e-12) || !std::isfinite(out.scale)) return false;
    out.t = md - mul(out.R, ms) * out.scale;
    return std::isfinite(out.t.x) && std::isfinite(out.t.y) && std::isfinite(out.t.z);
}

// ---- reprojection helpers on a standalone Reconstruction ----------------
// The mapper computes these against its FeatureSets; a merge only has the
// models, whose `points2D` carry the same keypoint coordinates.
inline double reprojErrorAt(const Camera& cam, const Pose& pose, const Vec2& obs, const Vec3& X) {
    Vec3 pc = mul(pose.R, X) + pose.t;
    // Cheirality, as the mapper does it (D33): the pinhole family tests z, a
    // wide-FOV camera (which sees past 90 deg) tests the sign along the ray.
    if (cam.wideFov()) {
        if (pc.dot(cam.bearing(obs)) <= 0) return 1e30;
    } else if (pc.z < 1e-8) {
        return 1e30;
    }
    Vec2 px = cam.project(pc);
    return std::hypot(px.x - obs.x, px.y - obs.y);
}

inline size_t countObservations(const Reconstruction& rec) {
    size_t n = 0;
    for (const auto& kv : rec.points3D) n += kv.second.track.size();
    return n;
}

// Drop observations that reproject badly and points whose track lost its
// parallax -- Mapper::filterPoints, against a standalone model.
// `max_err` is in extraction pixels (Camera::pixel_scale, D47).
inline void filterModel(Reconstruction& rec, double max_err, double min_ang_deg,
                        size_t& removed_obs, size_t& removed_pts) {
    removed_obs = removed_pts = 0;
    std::map<uint32_t, Vec3> centers;
    for (const auto& kv : rec.images)
        if (kv.second.registered) centers[kv.first] = cameraCenter(kv.second.pose);
    const double min_ang = min_ang_deg * M_PI / 180.0;
    std::vector<uint64_t> drop;
    for (auto& kv : rec.points3D) {
        Point3D& pt = kv.second;
        std::vector<TrackElement> keep;
        for (const TrackElement& e : pt.track) {
            auto im = rec.images.find(e.image_id);
            if (im == rec.images.end() || !im->second.registered ||
                e.point2D_idx >= im->second.points2D.size()) {
                removed_obs++;
                continue;
            }
            const Camera& cam = rec.cameras.at(im->second.camera_id);
            // `max_err` is in extraction pixels, like every other threshold
            // (D47); the camera converts it to its own.
            if (reprojErrorAt(cam, im->second.pose, im->second.points2D[e.point2D_idx], pt.xyz) <=
                cam.errPx(max_err)) {
                keep.push_back(e);
            } else {
                im->second.point3D_ids[e.point2D_idx] = kInvalidPoint3D;
                removed_obs++;
            }
        }
        pt.track = keep;
        bool degenerate = pt.track.size() < 2;
        if (!degenerate) {
            double best = 0;
            for (size_t i = 0; i + 1 < pt.track.size() && best < min_ang; i++)
                for (size_t j = i + 1; j < pt.track.size() && best < min_ang; j++)
                    best = std::max(best, triangulationAngle(pt.xyz, centers[pt.track[i].image_id],
                                                             centers[pt.track[j].image_id]));
            degenerate = best < min_ang;
        }
        if (degenerate) {
            for (const TrackElement& e : pt.track) {
                auto im = rec.images.find(e.image_id);
                if (im != rec.images.end() && e.point2D_idx < im->second.point3D_ids.size())
                    im->second.point3D_ids[e.point2D_idx] = kInvalidPoint3D;
                removed_obs++;
            }
            drop.push_back(kv.first);
        }
    }
    for (uint64_t id : drop) {
        rec.points3D.erase(id);
        removed_pts++;
    }
}

// ---- alignment ----------------------------------------------------------

struct AlignmentResult {
    Sim3 transform;                // src world -> dst world
    size_t common_images = 0;      // shared images usable for alignment
    size_t inliers = 0;
    double mean_error = 0;         // px, over the inliers
    bool success = false;
    std::string reason;
    // Set when the transform came from structure the two models triangulated
    // in common rather than from images they both registered (D70), with the
    // number of 3D-3D correspondences it was fitted to. Two models can be
    // aligned this way while sharing no camera at all.
    size_t structure_pairs = 0;
    bool from_structure = false;
    size_t rig_views = 0;   // ... of common_images, the ones a rig supplied
};

// ---- rigs ----------------------------------------------------------------

// A key that is the image id for a plain image and one value per rig frame
// for a rig image, so two lenses of one frame hash to the same overlap.
inline uint64_t overlapKey(const RigTable* rigs, uint32_t img) {
    if (!rigs) return img;
    const RigSlot sl = rigs->slot(img);
    if (!sl.valid()) return img;
    uint64_t k = 1ull << 40;
    for (uint32_t r = 0; r < sl.rig; r++) k += rigs->rigs[r].frames.size();
    return k + sl.frame;
}

// The pose `dst_image` would have in `src`'s gauge, from a registered
// rig-mate `src` holds and has calibrated. False without one.
inline bool rigPoseInSrc(const Reconstruction& src, const RigTable& rigs, uint32_t dst_image,
                         Pose& out) {
    const RigSlot sl = rigs.slot(dst_image);
    if (!sl.valid() || sl.rig >= src.rigs.size()) return false;
    if (src.rig_detached.count(dst_image)) return false;
    const RigCalib& c = src.rigs[sl.rig];
    if (!c.usable(sl.member)) return false;
    const std::vector<uint32_t>& fr = rigs.frameOf(sl);
    int best = -1;
    uint32_t best_pts = 0;
    for (uint32_t m = 0; m < fr.size(); m++) {
        if (m == sl.member || fr[m] == kNoImage || !c.usable(m)) continue;
        if (src.rig_detached.count(fr[m])) continue;
        auto it = src.images.find(fr[m]);
        if (it == src.images.end() || !it->second.registered) continue;
        const uint32_t n = it->second.numPoint3D();
        if (best < 0 || n > best_pts) { best = (int)m; best_pts = n; }
    }
    if (best < 0) return false;
    out = c.predict((uint32_t)best, sl.member, src.images.at(fr[best]).pose);
    return true;
}

// An image the rig places from a rig-mate that carries structure of its own:
// hollow by its observations, sound by construction.
inline bool rigCovered(const Reconstruction& m, const RigTable* rigs, uint32_t img, int min_pts) {
    if (!rigs || m.rig_detached.count(img)) return false;
    const RigSlot sl = rigs->slot(img);
    if (!sl.valid() || sl.rig >= m.rigs.size() || !m.rigs[sl.rig].usable(sl.member)) return false;
    const std::vector<uint32_t>& fr = rigs->frameOf(sl);
    for (uint32_t k = 0; k < fr.size(); k++) {
        if (k == sl.member || fr[k] == kNoImage || !m.rigs[sl.rig].usable(k)) continue;
        auto it = m.images.find(fr[k]);
        if (it != m.images.end() && it->second.registered &&
            (int)it->second.numPoint3D() >= min_pts)
            return true;
    }
    return false;
}

// The calibration a merged model keeps: its own where it has one, the
// incoming model's (rescaled into the anchor's gauge) where it has not.
inline void mergeRigCalibs(std::vector<RigCalib>& dst, const std::vector<RigCalib>& src,
                           double scale) {
    if (dst.size() < src.size()) dst.resize(src.size());
    for (size_t r = 0; r < src.size(); r++) {
        const RigCalib& s = src[r];
        RigCalib& d = dst[r];
        if (s.ref < 0) continue;
        if (d.ref < 0) {
            d = s;
            for (Pose& p : d.cam_from_rig) p.t = p.t * scale;
            continue;
        }
        if (d.ref != s.ref) continue;  // two frames of reference: a solve reconciles them
        for (size_t m = 0; m < s.cam_from_rig.size() && m < d.cam_from_rig.size(); m++) {
            if (d.established[m] || !s.established[m]) continue;
            d.cam_from_rig[m] = s.cam_from_rig[m];
            d.cam_from_rig[m].t = d.cam_from_rig[m].t * scale;
            d.established[m] = 1;
            d.fixed[m] = s.fixed[m];
            d.support[m] = s.support[m];
            d.spread_deg[m] = s.spread_deg[m];
        }
    }
}

// A pose correspondence between two models: where `src` puts `dst_image`.
struct PoseCorr {
    Pose src_pose;
    uint32_t dst_image = 0;
    bool by_rig = false;
};

// Every correspondence the two models offer: the images both registered, and
// the rig frames where each holds a different lens (src's calibration, which
// is the gauge the transform starts in).
inline std::vector<PoseCorr> poseCorrespondences(const Reconstruction& src,
                                                 const Reconstruction& dst,
                                                 const RigTable* rigs) {
    std::vector<PoseCorr> out;
    std::set<uint32_t> covered;
    for (const auto& kv : src.images) {
        if (!kv.second.registered) continue;
        auto it = dst.images.find(kv.first);
        if (it == dst.images.end() || !it->second.registered) continue;
        out.push_back({kv.second.pose, kv.first, false});
        covered.insert(kv.first);
    }
    if (!rigs) return out;
    for (const auto& kv : dst.images) {
        if (!kv.second.registered || covered.count(kv.first)) continue;
        Pose p;
        if (rigPoseInSrc(src, *rigs, kv.first, p)) out.push_back({p, kv.first, true});
    }
    return out;
}

// Images both models registered, in `src`-id order. Ids are the identity here:
// merging needs `point2D_idx` to mean the same keypoint in both models, which
// only holds for models built from the same features (COLMAP assumes the same
// database). `mismatched` counts shared ids whose name or keypoint count
// disagree -- evidence that assumption is broken.
inline std::vector<uint32_t> sharedImages(const Reconstruction& a, const Reconstruction& b,
                                          size_t* mismatched = nullptr) {
    std::vector<uint32_t> out;
    size_t bad = 0;
    for (const auto& kv : a.images) {
        if (!kv.second.registered) continue;
        auto it = b.images.find(kv.first);
        if (it == b.images.end() || !it->second.registered) continue;
        if (kv.second.points2D.size() != it->second.points2D.size() ||
            (!kv.second.name.empty() && !it->second.name.empty() &&
             kv.second.name != it->second.name)) {
            bad++;
            continue;
        }
        out.push_back(kv.first);
    }
    if (mismatched) *mismatched = bad;
    return out;
}

// Estimate the similarity taking `src` coordinates into `dst`'s, from the
// images both registered.
//
// The minimal solve is 2 shared poses (estimateSim3FromPoses), and the
// *scoring* is in pixels rather than in world units, which is what makes one
// threshold work across scenes of unknown scale (COLMAP's
// ReconstructionAlignmentEstimator scores the same way): a
// candidate transform predicts where each shared image sits in the destination
// frame, and the residual is the mean reprojection error of that model's own
// points, seen from the predicted pose. A wrong scale, rotation or translation
// all show up there; a per-center distance threshold would need a scene-size
// guess to be meaningful.
inline AlignmentResult alignReconstructions(const Reconstruction& src, const Reconstruction& dst,
                                            const MergeOptions& opt) {
    AlignmentResult r;
    size_t mismatched = 0;
    std::vector<uint32_t> shared = sharedImages(src, dst, &mismatched);
    if (mismatched) {
        r.reason = std::to_string(mismatched) +
                   " shared image id(s) disagree on name or keypoint count: the models "
                   "were not built from the same features";
        return r;
    }

    // Per correspondence: the two poses, and the destination model's points
    // seen there (sampled, so the residual stays cheap on long tracks).
    struct View {
        Pose src_pose, dst_pose;
        const Camera* cam = nullptr;
        std::vector<Vec3> pts;
        std::vector<Vec2> obs;
    };
    std::vector<View> views;
    const std::vector<PoseCorr> corr = poseCorrespondences(src, dst, opt.rigs);
    views.reserve(corr.size());
    for (const PoseCorr& pc : corr) {
        const uint32_t id = pc.dst_image;
        const Image& di = dst.images.at(id);
        auto cam = dst.cameras.find(di.camera_id);
        if (cam == dst.cameras.end()) continue;
        if (pc.by_rig) r.rig_views++;
        View v;
        v.src_pose = pc.src_pose;
        v.dst_pose = di.pose;
        v.cam = &cam->second;
        std::vector<uint32_t> feats;
        for (uint32_t f = 0; f < (uint32_t)di.point3D_ids.size(); f++)
            if (di.point3D_ids[f] != kInvalidPoint3D && dst.points3D.count(di.point3D_ids[f]))
                feats.push_back(f);
        if (feats.empty()) continue;  // nothing to score this image with
        const size_t stride =
            std::max<size_t>(1, feats.size() / std::max(1, opt.max_alignment_points));
        for (size_t k = 0; k < feats.size(); k += stride) {
            v.pts.push_back(dst.points3D.at(di.point3D_ids[feats[k]]).xyz);
            v.obs.push_back(di.points2D[feats[k]]);
        }
        views.push_back(std::move(v));
    }
    r.common_images = views.size();
    if ((int)views.size() < std::max(2, opt.min_common_images)) {
        r.reason = std::to_string(views.size()) + " usable shared image(s), need " +
                   std::to_string(std::max(2, opt.min_common_images));
        return r;
    }

    // Mean reprojection error, in pixels, of the destination points seen from
    // where `T` says this image is. Points that land behind the predicted
    // camera are capped rather than infinite, so one bad point cannot decide
    // an otherwise good view.
    const double cap = 10.0 * opt.max_reproj_error;
    auto viewError = [&](const Sim3& T, const View& v) {
        Pose pred = transformPose(T, v.src_pose);
        double sum = 0;
        for (size_t k = 0; k < v.pts.size(); k++)
            // In extraction pixels, so views from cameras the extractor
            // downscaled by different amounts are judged on one scale (D47).
            sum += std::min(cap, reprojErrorAt(*v.cam, pred, v.obs[k], v.pts[k]) /
                                     v.cam->pixel_scale);
        return sum / (double)v.pts.size();
    };

    auto fit = [&](const std::vector<int>& idx) {
        std::vector<Pose> s, d;
        s.reserve(idx.size());
        d.reserve(idx.size());
        for (int i : idx) {
            s.push_back(views[i].src_pose);
            d.push_back(views[i].dst_pose);
        }
        Sim3 T;
        std::vector<Sim3> out;
        if (estimateSim3FromPoses(s, d, T)) out.push_back(T);
        return out;
    };
    auto res = [&](const Sim3& T, int i) {
        double e = viewError(T, views[i]);
        return e * e;
    };
    RansacOptions ro;
    ro.max_error = opt.max_reproj_error;
    ro.max_num_trials = opt.ransac_max_trials;
    ro.seed = opt.seed;
    // Two pose correspondences per hypothesis, so a pair sharing only a handful
    // of images still gets a real RANSAC rather than a single fit.
    RansacReport<Sim3> rep = loransac<Sim3>((int)views.size(), 2, fit, fit, res, ro);

    if (!rep.success || (int)rep.num_inliers < std::max(2, opt.min_common_images)) {
        r.reason = "alignment found only " + std::to_string(rep.num_inliers) + "/" +
                   std::to_string(views.size()) + " consistent shared image(s)";
        return r;
    }
    const double ratio = (double)rep.num_inliers / (double)views.size();
    if (ratio < opt.min_inlier_ratio) {
        char buf[128];
        snprintf(buf, sizeof buf, "only %d/%zu shared images agree (%.0f%%, need %.0f%%)",
                 rep.num_inliers, views.size(), 100 * ratio, 100 * opt.min_inlier_ratio);
        r.reason = buf;
        return r;
    }
    double sum = 0;
    for (size_t i = 0; i < views.size(); i++)
        if (rep.inlier_mask[i]) sum += viewError(rep.model, views[i]);
    r.transform = rep.model;
    r.inliers = rep.num_inliers;
    r.mean_error = sum / (double)rep.num_inliers;
    r.success = true;
    return r;
}

// ---- the merge itself ---------------------------------------------------

struct MergeCounts {
    size_t images_added = 0;
    size_t points_added = 0;    // src points that became new dst points
    size_t points_spliced = 0;  // src points that joined an existing dst point
    size_t points_dropped = 0;  // src points with too little left to place
    size_t obs_added = 0;
    size_t obs_filtered = 0;
    size_t points_filtered = 0;
    size_t hollow_images = 0;   // added images left under min_image_points
    // Spliced points where the two models disagree about where the point is
    // (see mergeInto). The sharpest signal that an alignment is wrong.
    size_t splice_conflicts = 0;
};

// Apply `T` to `src` and fold it into `dst`, then filter. Mechanical: every
// decision about whether this *should* happen belongs to the caller.
//
// Track splicing follows COLMAP's Reconstruction::Merge: a source point whose
// observations land on features that already carry a destination point joins
// that point (position averaged by track length, as MergePoints3D does);
// otherwise, if at least two observations are free, it becomes a new point.
// A source point that would join two different destination points is ambiguous
// -- one of the two models has a mistake -- and is dropped.
inline MergeCounts mergeInto(Reconstruction& dst, const Reconstruction& src, const Sim3& T,
                             const MergeOptions& opt) {
    MergeCounts c;
    // A camera id present in both models was optimized twice, independently;
    // the destination's copy wins (as COLMAP's Merge does), so the incoming
    // images change intrinsics slightly. That is one of the things the bundle
    // adjustment after a merge is for.
    for (const auto& kv : src.cameras)
        if (!dst.cameras.count(kv.first)) dst.cameras[kv.first] = kv.second;
    mergeRigCalibs(dst.rigs, src.rigs, T.scale);
    dst.rig_detached.insert(src.rig_detached.begin(), src.rig_detached.end());

    std::vector<uint32_t> added;
    for (const auto& kv : src.images) {
        if (!kv.second.registered || dst.images.count(kv.first)) continue;
        Image im = kv.second;
        im.pose = transformPose(T, im.pose);
        im.point3D_ids.assign(im.points2D.size(), kInvalidPoint3D);
        dst.images[kv.first] = std::move(im);
        added.push_back(kv.first);
    }
    c.images_added = added.size();

    for (const auto& kv : src.points3D) {
        const Point3D& sp = kv.second;
        std::vector<TrackElement> free_track;  // observations with no dst point yet
        std::set<uint64_t> existing;
        size_t bound = 0;
        for (const TrackElement& e : sp.track) {
            auto di = dst.images.find(e.image_id);
            if (di == dst.images.end() || !di->second.registered ||
                e.point2D_idx >= di->second.point3D_ids.size())
                continue;
            uint64_t id = di->second.point3D_ids[e.point2D_idx];
            if (id == kInvalidPoint3D) free_track.push_back(e);
            else { existing.insert(id); bound++; }
        }
        const Vec3 X = transformPoint(T, sp.xyz);
        if (existing.size() == 1 && free_track.size() + bound >= 2) {
            Point3D& tp = dst.points3D.at(*existing.begin());
            // Both models triangulated this feature, so both positions describe
            // the same physical point and the transformed one must project
            // where the destination's observations already say it is. When the
            // alignment is wrong this is where it shows: the tracks still meet
            // (they meet by feature index, not by geometry) but they disagree
            // about where they meet. A few of the existing observations are
            // enough to tell.
            for (size_t k = 0, tested = 0; k < tp.track.size() && tested < 3; k++) {
                const TrackElement& e = tp.track[k];
                auto di = dst.images.find(e.image_id);
                if (di == dst.images.end() || e.point2D_idx >= di->second.points2D.size()) continue;
                auto cam = dst.cameras.find(di->second.camera_id);
                if (cam == dst.cameras.end()) continue;
                tested++;
                if (reprojErrorAt(cam->second, di->second.pose,
                                  di->second.points2D[e.point2D_idx], X) >
                    cam->second.errPx(opt.splice_tolerance)) {
                    c.splice_conflicts++;
                    break;
                }
            }
            // Two features of one image on a single track is the ambiguity the
            // mapper guards against; keep the observation that is already there.
            std::set<uint32_t> in_track;
            for (const TrackElement& e : tp.track) in_track.insert(e.image_id);
            size_t attached = 0;
            for (const TrackElement& e : free_track) {
                if (!in_track.insert(e.image_id).second) continue;
                tp.track.push_back(e);
                dst.images[e.image_id].point3D_ids[e.point2D_idx] = *existing.begin();
                attached++;
            }
            // COLMAP's MergePoints3D: both positions are estimates of the same
            // point, weighted by how many observations back them.
            const double wo = (double)(tp.track.size() - attached), wn = (double)sp.track.size();
            if (wo + wn > 0) {
                tp.xyz = (tp.xyz * wo + X * wn) * (1.0 / (wo + wn));
                for (int k = 0; k < 3; k++)
                    tp.rgb[k] = (uint8_t)std::lround((tp.rgb[k] * wo + sp.rgb[k] * wn) / (wo + wn));
            }
            c.obs_added += attached;
            c.points_spliced++;
        } else if (existing.empty() && free_track.size() >= 2) {
            uint64_t id = dst.addPoint3D(X, free_track);
            dst.points3D[id].rgb[0] = sp.rgb[0];
            dst.points3D[id].rgb[1] = sp.rgb[1];
            dst.points3D[id].rgb[2] = sp.rgb[2];
            c.obs_added += free_track.size();
            c.points_added++;
        } else {
            c.points_dropped++;
        }
    }

    filterModel(dst, opt.filter_reproj_error, opt.min_tri_angle_deg, c.obs_filtered,
                c.points_filtered);
    for (uint32_t id : added)
        if ((int)dst.images.at(id).numPoint3D() < opt.min_image_points &&
            !rigCovered(dst, opt.rigs, id, opt.min_image_points))
            c.hollow_images++;
    return c;
}

// ---- the policy ---------------------------------------------------------

// One merge the session could perform, as ranked for the automatic order. A
// GUI shows this list and lets the user pick; `MergeSession::tryMerge` takes
// the pick, so both callers go through the same path.
struct MergeCandidate {
    size_t dst = 0, src = 0;     // merge `src` into `dst`
    size_t common_images = 0;
};

struct MergeAttempt {
    size_t dst = 0, src = 0;
    AlignmentResult alignment;
    MergeCounts counts;
    bool merged = false;
    std::string reason;          // why not, when !merged
};

// Holds the models and decides what to merge. `runAuto()` is the automatic
// policy; a GUI drives the same object one step at a time instead:
//
//     MergeSession s(std::move(models));
//     for (const MergeCandidate& c : s.candidates())   // what is possible
//         show(c, s.commonImages(c.dst, c.src));
//     AlignmentResult a = alignReconstructions(s.model(src), s.model(dst), s.options());
//     ... preview a.transform / a.mean_error, or let the user place one ...
//     s.tryMerge(dst, src, user_transform_or_null);    // validated and undone
//     std::vector<Reconstruction> out = s.take();
//
// Model indices are stable: a merged-away model leaves its slot behind (with
// `alive()` false), so a UI can keep referring to what the user selected.
// A committed merge cannot be undone from here -- the session frees the
// absorbed model -- so a GUI offering undo keeps its own copy, or re-reads
// from disk.
class MergeSession {
public:
    explicit MergeSession(std::vector<Reconstruction> models, MergeOptions opt = {})
        : models_(std::move(models)), alive_(models_.size(), 1), opt_(opt) {}

    size_t numModels() const { return models_.size(); }
    bool alive(size_t i) const { return i < alive_.size() && alive_[i]; }
    const Reconstruction& model(size_t i) const { return models_.at(i); }
    const std::vector<MergeAttempt>& log() const { return log_; }
    const MergeOptions& options() const { return opt_; }
    MergeOptions& options() { return opt_; }

    size_t commonImages(size_t a, size_t b) const {
        if (!alive(a) || !alive(b) || a == b) return 0;
        if (!opt_.rigs) return sharedImages(models_[a], models_[b]).size();
        std::set<uint64_t> keys;
        for (const auto& kv : models_[a].images)
            if (kv.second.registered) keys.insert(overlapKey(opt_.rigs, kv.first));
        size_t n = 0;
        std::set<uint64_t> seen;
        for (const auto& kv : models_[b].images)
            if (kv.second.registered) {
                const uint64_t k = overlapKey(opt_.rigs, kv.first);
                if (keys.count(k) && seen.insert(k).second) n++;
            }
        return n;
    }

    // Every pair that could be merged, best first. "Best" is the most shared
    // images (the alignment is then most over-determined, so a wrong one is
    // most likely to be caught), then the largest anchor: the bigger model
    // keeps its gauge and its intrinsics, and the smaller one moves.
    std::vector<MergeCandidate> candidates() const {
        // Counted through an overlap-key -> models index rather than by
        // intersecting every pair of models (quadratic, and a bottom-up run
        // holds hundreds). The key is the rig frame where there is one.
        std::unordered_map<uint64_t, std::vector<uint32_t>> holders;
        for (size_t i = 0; i < models_.size(); i++) {
            if (!alive_[i]) continue;
            for (const auto& kv : models_[i].images) {
                if (!kv.second.registered) continue;
                std::vector<uint32_t>& h = holders[overlapKey(opt_.rigs, kv.first)];
                if (h.empty() || h.back() != (uint32_t)i) h.push_back((uint32_t)i);
            }
        }
        std::unordered_map<uint64_t, size_t> shared;  // (lo << 32 | hi) -> images
        for (const auto& kv : holders) {
            const std::vector<uint32_t>& v = kv.second;  // ascending by construction
            for (size_t a = 0; a + 1 < v.size(); a++)
                for (size_t b = a + 1; b < v.size(); b++)
                    shared[((uint64_t)v[a] << 32) | v[b]]++;
        }
        std::vector<MergeCandidate> out;
        for (const auto& kv : shared) {
            if ((int)kv.second < std::max(3, opt_.min_common_images)) continue;
            const size_t i = (size_t)(kv.first >> 32), j = (size_t)(kv.first & 0xffffffffu);
            MergeCandidate c;
            // Anchor = more registered images; ties keep the earlier index,
            // which is the one with more 3D points (models arrive sorted).
            const bool i_first = models_[i].numRegistered() >= models_[j].numRegistered();
            c.dst = i_first ? i : j;
            c.src = i_first ? j : i;
            c.common_images = kv.second;
            out.push_back(c);
        }
        // The hash map's order is unspecified; sort to a total order so the
        // pass is reproducible, then by the ranking below.
        std::sort(out.begin(), out.end(), [](const MergeCandidate& a, const MergeCandidate& b) {
            return a.dst != b.dst ? a.dst < b.dst : a.src < b.src;
        });
        std::stable_sort(out.begin(), out.end(),
                         [&](const MergeCandidate& a, const MergeCandidate& b) {
                             if (a.common_images != b.common_images)
                                 return a.common_images > b.common_images;
                             const uint32_t ra = models_[a.dst].numRegistered();
                             const uint32_t rb = models_[b.dst].numRegistered();
                             if (ra != rb) return ra > rb;
                             return a.dst < b.dst;
                         });
        return out;
    }

    // Merge `src` into `dst`, undoing the whole thing if the result does not
    // hold up. `alignment` non-null skips estimation and uses the caller's
    // transform (the GUI path: a user-placed or externally computed one); the
    // acceptance checks still apply.
    // With an alignment computed elsewhere -- a user's placement, or one fitted
    // to the structure the two models share rather than to shared images
    // (Mapper::alignByStructure, D70). Everything after the transform is
    // identical, so such a merge faces every acceptance test the ordinary path
    // does, and the inlier count it carries is what the splice arbitration
    // reads.
    MergeAttempt tryMerge(size_t dst, size_t src, const AlignmentResult& alignment) {
        return tryMergeImpl(dst, src, &alignment, nullptr);
    }
    MergeAttempt tryMerge(size_t dst, size_t src, const Sim3* alignment = nullptr) {
        return tryMergeImpl(dst, src, nullptr, alignment);
    }

private:
    MergeAttempt tryMergeImpl(size_t dst, size_t src, const AlignmentResult* full,
                              const Sim3* alignment) {
        MergeAttempt a;
        a.dst = dst;
        a.src = src;
        if (dst == src || !alive(dst) || !alive(src)) {
            a.reason = "not two live models";
            log_.push_back(a);
            return log_.back();
        }
        if (full) {
            a.alignment = *full;
        } else if (alignment) {
            a.alignment.transform = *alignment;
            a.alignment.common_images = commonImages(dst, src);
            a.alignment.success = true;
        } else {
            a.alignment = alignReconstructions(models_[src], models_[dst], opt_);
            if (!a.alignment.success) {
                a.reason = a.alignment.reason;
                log_.push_back(a);
                return log_.back();
            }
        }

        // The undo: merge into a copy and keep it only if it survives. Copying
        // the anchor is the whole cost of being able to reject -- worth it,
        // since a merge rewrites poses, tracks and points at once and has no
        // cheaper inverse.
        const size_t anchor_obs = countObservations(models_[dst]);
        const uint32_t anchor_imgs = models_[dst].numRegistered();
        Reconstruction merged = models_[dst];
        MergeCounts c = mergeInto(merged, models_[src], a.alignment.transform, opt_);
        a.counts = c;

        char buf[192];
        // Structure the two models share is the strongest evidence available:
        // a point both triangulated has to be in the same place in both after
        // the transform. A model that arrives with its own self-consistent
        // tracks passes every other test even when it is placed completely
        // wrongly -- this is the test that does not.
        const bool contested =
            c.points_spliced &&
            c.splice_conflicts > opt_.max_splice_conflict_ratio * (double)c.points_spliced;
        if (contested && !(opt_.validate && opt_.splice_arbitrate_inliers > 0 &&
                           (int)a.alignment.inliers >= opt_.splice_arbitrate_inliers)) {
            snprintf(buf, sizeof buf,
                     "the models disagree about %zu of the %zu points they both triangulated",
                     c.splice_conflicts, c.points_spliced);
            a.reason = buf;
            log_.push_back(a);
            return log_.back();
        }
        if (c.images_added && c.hollow_images > opt_.max_hollow_ratio * (double)c.images_added) {
            snprintf(buf, sizeof buf,
                     "%zu of the %zu images it added kept fewer than %d points", c.hollow_images,
                     c.images_added, opt_.min_image_points);
            a.reason = buf;
            log_.push_back(a);
            return log_.back();
        }
        const size_t after = countObservations(merged);
        // The anchor's own observations must survive: the merged model is
        // strictly larger, so a net loss means the incoming geometry is
        // fighting what was already there.
        if (anchor_obs && after + (size_t)(opt_.max_anchor_obs_loss * anchor_obs) < anchor_obs) {
            snprintf(buf, sizeof buf, "the merged model lost %.0f%% of the anchor's observations",
                     100.0 * (double)(anchor_obs - after) / (double)anchor_obs);
            a.reason = buf;
            log_.push_back(a);
            return log_.back();
        }

        if (opt_.check_duplicate) {
            DuplicateReport before = findDuplicateStructure(models_[dst], opt_.duplicate);
            DuplicateReport after = findDuplicateStructure(merged, opt_.duplicate);
            if (after.duplicated(opt_.duplicate) &&
                after.conflicts > before.conflicts + (size_t)opt_.duplicate.min_conflicts) {
                snprintf(buf, sizeof buf,
                         "the merged model puts %zu pairs of images in the same place with no "
                         "structure in common (%zu before the merge, of %zu co-located pairs)",
                         after.conflicts, before.conflicts, after.colocated);
                a.reason = buf;
                log_.push_back(a);
                return log_.back();
            }
        }
        if (opt_.validate) {
            std::string why = opt_.validate(merged, models_[src], a.alignment.transform, c);
            if (!why.empty()) {
                a.reason = why;
                log_.push_back(a);
                return log_.back();
            }
        }

        models_[dst] = std::move(merged);
        models_[src] = Reconstruction();  // free it; the slot stays for the log
        alive_[src] = 0;
        a.merged = true;
        if (opt_.verbose)
            slog::diag(slog::Tag::Merge,
                       "[merge] model %zu <- model %zu: %zu %s (%zu inliers, %.2f px%s), "
                       "+%zu images, %u -> %u, +%zu points, %zu spliced (%zu disagreed)",
                       dst, src,
                       a.alignment.from_structure ? a.alignment.structure_pairs
                       : a.alignment.common_images,
                       a.alignment.from_structure ? "shared points" : "shared images",
                       a.alignment.inliers, a.alignment.mean_error,
                       a.alignment.rig_views ? ", some through the rig" : "",
                       c.images_added, anchor_imgs,
                       models_[dst].numRegistered(), c.points_added, c.points_spliced,
                       c.splice_conflicts);
        log_.push_back(a);
        return log_.back();
    }

public:
    // Merge until nothing else can be. Failed pairs are remembered so they are
    // not retried on identical inputs, and forgotten again for any model that a
    // later merge changed -- the same "undo and come back to it" shape as the
    // mapper's registration retries (D36).
    size_t runAuto() {
        size_t merges = 0;
        while (true) {
            bool progressed = false;
            for (const MergeCandidate& c : candidates()) {
                if (failed_.count({c.dst, c.src}) || failed_.count({c.src, c.dst})) continue;
                MergeAttempt a = tryMerge(c.dst, c.src);
                if (a.merged) {
                    merges++;
                    progressed = true;
                    // Everything that failed against the model that just grew
                    // deserves another look: it now covers more images, so a
                    // pair that shared too few may not any more.
                    for (auto it = failed_.begin(); it != failed_.end();)
                        it = (it->first == c.dst || it->second == c.dst) ? failed_.erase(it)
                                                                         : std::next(it);
                    break;
                }
                failed_.insert({c.dst, c.src});
                if (opt_.verbose)
                    slog::diag(slog::Tag::Merge, "[merge] model %zu <- model %zu refused: %s",
                               c.dst, c.src,
                               a.reason.c_str());
            }
            if (!progressed) break;
        }
        return merges;
    }

    // Models that absorbed at least one other. A caller that bundle-adjusts
    // across the seams only has to touch these; everything else is untouched
    // and must stay bit-identical to what the mapper produced.
    std::set<size_t> changed() const {
        std::set<size_t> s;
        for (const MergeAttempt& a : log_)
            if (a.merged && alive_[a.dst]) s.insert(a.dst);
        return s;
    }
    Reconstruction& modelMut(size_t i) { return models_.at(i); }

    // The surviving models, ordered by 3D point count as COLMAP writes them.
    std::vector<Reconstruction> take() {
        std::vector<Reconstruction> out;
        for (size_t i = 0; i < models_.size(); i++)
            if (alive_[i]) out.push_back(std::move(models_[i]));
        std::stable_sort(out.begin(), out.end(),
                         [](const Reconstruction& a, const Reconstruction& b) {
                             return a.points3D.size() > b.points3D.size();
                         });
        return out;
    }

private:
    std::vector<Reconstruction> models_;
    std::vector<char> alive_;
    MergeOptions opt_;
    std::vector<MergeAttempt> log_;
    std::set<std::pair<size_t, size_t>> failed_;
};

}  // namespace sfm
