// Geometric verification of a whole pair list (src/sfm/README.md).
//
// `estimateTwoView` verifies one pair; this drives it over every pair produced
// by `sfm/feature/Pairing.h` and is where the parallelism lives: host RANSAC on
// every putative pair is the largest single cost in the `match` stage (it was
// 83-85% of it, and ~70% of end-to-end wall clock, when this pool was written;
// it is a third to two thirds of the stage now that the other stages have been
// worked on, and still the reason the stage is CPU-bound).
//
// Shape: the GPU matcher owns one VkContext and one queue, so descriptor matching
// stays on the calling thread; it acts as the producer and a pool of worker
// threads consumes the putative matches. That overlaps host RANSAC with GPU
// matching *and* spreads it across cores, which the serial loop did neither of.
//
// Results are collected by pair index and returned in the pair list's order, so
// the output does not depend on thread count or scheduling. `estimateTwoView`
// is reentrant (no globals; its RNG is seeded per call from RansacOptions), so
// each pair's geometry is likewise thread-count-independent.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "sfm/core/Progress.h"
#include "sfm/core/Camera.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Events.h"
#include "sfm/core/Log.h"
#include "sfm/core/Resume.h"
#include "sfm/geometry/TwoView.h"
#include "i18n/catalog/Sfm.h"

namespace sfm {

// Unit bearings for every feature of every image, plus the cameras they were
// produced with (D45). Verifying a fisheye pair on pixels asks a pinhole model
// to explain rays it cannot represent; verifying it on bearings asks the
// question the geometry actually answers. bearing() is a Newton inversion per
// keypoint and each image takes part in dozens of pairs, so this is computed
// once per image rather than once per pair -- which is the difference between
// seconds and hours on a 2000-image capture.
//
// An empty per-image entry means "verify this image's pairs on pixels", so a
// mixed dataset degrades per image rather than wholesale.
struct BearingCache {
    std::vector<Camera> cameras;              // per image
    std::vector<std::vector<Vec3>> bearings;  // per image, per feature
    bool has(uint32_t i) const { return i < bearings.size() && !bearings[i].empty(); }
    size_t bytes() const {
        size_t n = 0;
        for (const auto& b : bearings) n += b.size() * sizeof(Vec3);
        return n;
    }
};

// `cams[i]` is image i's camera; entries whose model cannot see past 90 degrees
// are skipped, because the pixel path is already exact for them and reproducing
// it through bearings would only perturb long-settled results.
inline BearingCache precomputeBearings(const std::vector<FeatureSet>& feats,
                                       const std::vector<Camera>& cams,
                                       bool wide_only = true, int threads = 0) {
    BearingCache bc;
    bc.cameras = cams;
    bc.bearings.resize(feats.size());
    const size_t n = std::min(feats.size(), cams.size());
    // Millions of independent Newton inversions, one per keypoint; images are
    // written to disjoint slots, so this is a straight fan-out.
    const unsigned hc = std::thread::hardware_concurrency();
    int nt = threads > 0 ? threads : (hc > 0 ? (int)hc : 1);
    nt = std::max(1, std::min<int>(nt, (int)std::max<size_t>(n, 1)));
    std::atomic<size_t> next{0};
    auto worker = [&] {
        for (size_t i = next++; i < n; i = next++) {
            if (wide_only && !cams[i].wideFov()) continue;
            std::vector<Vec3>& out = bc.bearings[i];
            out.resize(feats[i].keypoints.size());
            for (size_t k = 0; k < out.size(); k++)
                out[k] = cams[i].bearing({feats[i].keypoints[k].x, feats[i].keypoints[k].y});
        }
    };
    if (nt == 1) {
        worker();
        return bc;
    }
    std::vector<std::thread> pool;
    pool.reserve(nt);
    for (int t = 0; t < nt; t++) pool.emplace_back(worker);
    for (std::thread& t : pool) t.join();
    return bc;
}

// How many correspondences a focal hypothesis lets *some* epipolar geometry
// explain, over `sample`. Only the epipolar half of two-view verification runs
// (the homography answers a different question), on the matched features only,
// so the whole search costs a couple of seconds rather than a full extra
// verification pass.
//
// Two counts, because they are not equally informative. Near the optical axis
// any plausible focal produces nearly the same ray, so central correspondences
// agree with every hypothesis and flatten the curve; it is the periphery of a
// fisheye frame -- where a wrong focal bends the ray by tens of degrees -- that
// actually knows the answer. `peripheral` counts only inliers whose keypoints
// are both beyond kPeripheralFrac of the half-diagonal in both images, and that
// is what the search maximizes.
struct FocalScore {
    int total = 0;
    int peripheral = 0;
};
inline constexpr double kPeripheralFrac = 0.45;

inline FocalScore focalInliers(const std::vector<FeatureSet>& feats,
                               const std::vector<std::pair<uint32_t, uint32_t>>& sample,
                               const std::vector<std::vector<FeatureMatch>>& matches,
                               const std::vector<Camera>& cams, double focal,
                               const TwoViewOptions& tvopt, int threads) {
    std::atomic<int> total{0}, periph{0};
    std::atomic<size_t> next{0};
    auto worker = [&] {
        for (;;) {
            size_t s = next++;
            if (s >= sample.size()) return;
            const std::vector<FeatureMatch>& m = matches[s];
            if ((int)m.size() < tvopt.min_num_inliers) continue;
            uint32_t i = sample[s].first, j = sample[s].second;
            Camera c1 = cams[i], c2 = cams[j];
            c1.setFocal(focal);
            c2.setFocal(focal);
            const double r1 = kPeripheralFrac * 0.5 * std::hypot((double)feats[i].width,
                                                                 (double)feats[i].height);
            const double r2 = kPeripheralFrac * 0.5 * std::hypot((double)feats[j].width,
                                                                 (double)feats[j].height);
            int n = (int)m.size();
            std::vector<Vec3> b1(n), b2(n);
            std::vector<char> wide(n, 0);
            for (int k = 0; k < n; k++) {
                const Keypoint& a = feats[i].keypoints[m[k].idx1];
                const Keypoint& b = feats[j].keypoints[m[k].idx2];
                b1[k] = c1.bearing({a.x, a.y});
                b2[k] = c2.bearing({b.x, b.y});
                wide[k] = std::hypot(a.x - c1.cx, a.y - c1.cy) > r1 &&
                          std::hypot(b.x - c2.cx, b.y - c2.cy) > r2;
            }
            RansacOptions ro = tvopt.ransac;
            ro.max_error = tvopt.ransac.max_error *
                           (0.5 * (feats[i].pixelScale() + feats[j].pixelScale())) / focal;
            ro.max_num_trials = std::min(ro.max_num_trials, 2000);
            auto fit = [&](const std::vector<int>& s2) {
                return estimateEpipolar7Bearing(b1, b2, s2);
            };
            auto refit = [&](const std::vector<int>& s2) {
                return estimateEpipolar8Bearing(b1, b2, s2);
            };
            auto res = [&](const Mat3& E, int k) {
                return sampsonSqBearing(E, b1[k], b2[k]);
            };
            RansacReport<Mat3> rep = loransac<Mat3>(n, 7, fit, refit, res, ro);
            if (rep.num_inliers < tvopt.min_num_inliers) continue;
            int w = 0;
            for (int k = 0; k < n; k++)
                if (rep.inlier_mask[k] && wide[k]) w++;
            total += rep.num_inliers;
            periph += w;
        }
    };
    int nt = threads > 0 ? threads : (int)std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (int t = 0; t < nt; t++) pool.emplace_back(worker);
    for (std::thread& t : pool) t.join();
    return {total, periph};
}

// Search the focal length that makes the most peripheral correspondences agree
// with *some* epipolar geometry. A too-long focal flattens the rays and a
// too-short one splays them; either way no single epipolar geometry fits a
// whole fisheye frame afterwards, so the peripheral inlier count peaks at the
// truth. The grid is in half-diagonal field of view, the physically meaningful
// axis: 55 deg (an ordinary lens) through 175 deg (well past any real fisheye,
// so the true value is interior and the parabola below has something to work
// with).
inline double bootstrapFocal(const std::vector<FeatureSet>& feats,
                             const std::vector<std::pair<uint32_t, uint32_t>>& sample,
                             const std::vector<std::vector<FeatureMatch>>& matches,
                             const std::vector<Camera>& cams, const TwoViewOptions& tvopt,
                             int threads, bool verbose) {
    if (sample.empty()) return 0;
    double diag = std::hypot((double)feats[sample[0].first].width,
                             (double)feats[sample[0].first].height);
    double best_f = 0;
    int best_n = -1;
    std::vector<std::pair<double, int>> curve;
    std::vector<int> totals;
    for (int step = 0; step <= 24; step++) {
        double half_fov = (55.0 + step * 5.0) * M_PI / 180.0;
        double f = 0.5 * diag / half_fov;  // equidistant: r = f * theta
        FocalScore sc = focalInliers(feats, sample, matches, cams, f, tvopt, threads);
        curve.push_back({f, sc.peripheral});
        totals.push_back(sc.total);
        if (sc.peripheral > best_n) { best_n = sc.peripheral; best_f = f; }
    }
    // Parabolic interpolation through the peak and its neighbours, so the
    // answer is not quantized to the grid (the mapper refines it anyway, but a
    // better start is a better set of verified pairs).
    for (size_t i = 1; i + 1 < curve.size(); i++) {
        if (curve[i].first != best_f) continue;
        double y0 = curve[i - 1].second, y1 = curve[i].second, y2 = curve[i + 1].second;
        double den = y0 - 2 * y1 + y2;
        if (den < 0) {
            double d = 0.5 * (y0 - y2) / den;
            if (std::fabs(d) < 1.0) {
                double lf = std::log(curve[i - 1].first), rf = std::log(curve[i + 1].first);
                best_f = std::exp(std::log(best_f) + d * 0.5 * (rf - lf));
            }
        }
        break;
    }
    if (verbose) {
        slog::out(slog::Tag::Match, spirula::i18n::msg::sfm::focal_search,
                 {(long long)sample.size(), slog::num(best_f, 1),
                  (long long)((0.5 * diag / best_f) * 180.0 / M_PI)});
        // The search curve itself is a debug histogram -- whoever reads it is
        // calibrating the focal search, not waiting on a reconstruction.
        std::string line = "[focal] curve (f:peripheral/total):";
        for (size_t i = 0; i < curve.size(); i++) {
            char buf[64];
            snprintf(buf, sizeof buf, " %.0f:%d/%d", curve[i].first,
                     curve[i].second, totals[i]);
            line += buf;
        }
        slog::diag(slog::Tag::Match, "%s", line.c_str());
    }
    return best_f;
}

// Bootstrap a focal for every fisheye camera group that has no prior, from the
// pairs whose two images are *both* in that group. One focal per group, not one
// per dataset (D46): a rig carries two lenses, and a cross-group pair cannot
// tell which of the two is wrong.
//
// Groups already in `given` (any --focal, or EXIF -- CameraSetup::focal_given)
// are left alone, and so is every non-fisheye group: a wrong focal warps a pinhole
// image's bearings by a fixed linear map, which leaves the epipolar relation
// rank-2 and the inlier set unchanged, whereas a wrong *fisheye* focal warps
// them nonlinearly and no epipolar geometry fits afterwards. That asymmetry is
// the whole reason this search exists.
//
// `pairs[i]` has `matches[i]`; both come from the caller's sample.
inline void bootstrapGroupFocals(const std::vector<FeatureSet>& feats,
                                 const std::vector<uint32_t>& cam_ids,
                                 const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                                 const std::vector<std::vector<FeatureMatch>>& matches,
                                 std::map<uint32_t, Camera>& cams,
                                 const std::set<uint32_t>& given, std::set<uint32_t>& measured,
                                 const TwoViewOptions& tvopt,
                                 size_t max_sample, int threads, bool verbose) {
    std::map<uint32_t, std::vector<size_t>> by_group;
    for (size_t p = 0; p < pairs.size() && p < matches.size(); p++) {
        uint32_t a = pairs[p].first, b = pairs[p].second;
        if (a >= cam_ids.size() || b >= cam_ids.size() || cam_ids[a] != cam_ids[b]) continue;
        by_group[cam_ids[a]].push_back(p);
    }
    // focalInliers wants a camera per image; the group's camera stands in for
    // every image of that group.
    std::vector<Camera> percam(feats.size());
    for (size_t i = 0; i < feats.size() && i < cam_ids.size(); i++) {
        auto it = cams.find(cam_ids[i]);
        if (it != cams.end()) percam[i] = it->second;
    }
    for (auto& kv : cams) {
        const uint32_t id = kv.first;
        if (!kv.second.isFisheye() || given.count(id)) continue;
        auto g = by_group.find(id);
        if (g == by_group.end() || g->second.size() < 8) {
            if (verbose)
                slog::diag(slog::Tag::Match,
                           "[focal] camera %u: %zu in-group pair(s), too few to search; "
                           "keeping f=%.1f",
                           id, g == by_group.end() ? 0 : g->second.size(), kv.second.focal());
            continue;
        }
        // Spread the sample over the group's pairs; a prefix would sample one
        // part of the capture, since pair lists are ordered.
        std::vector<std::pair<uint32_t, uint32_t>> sp;
        std::vector<std::vector<FeatureMatch>> sm;
        const size_t stride = std::max<size_t>(1, g->second.size() / std::max<size_t>(1, max_sample));
        for (size_t k = 0; k < g->second.size() && sp.size() < max_sample; k += stride) {
            sp.push_back(pairs[g->second[k]]);
            sm.push_back(matches[g->second[k]]);
        }
        if (verbose) slog::diag(slog::Tag::Match, "[focal] camera %u:", id);
        double f = bootstrapFocal(feats, sp, sm, percam, tvopt, threads, verbose);
        if (f > 0) {
            kv.second.setFocal(f);
            measured.insert(id);
            for (size_t i = 0; i < percam.size() && i < cam_ids.size(); i++)
                if (cam_ids[i] == id) percam[i].setFocal(f);
        }
    }
}

// ---- focal from the fundamental matrix, for rectilinear groups (D53) ------
//
// A wrong focal leaves a *pinhole* pair's inlier count alone -- that is the
// asymmetry bootstrapFocal above is built on, and the reason it only helps a
// fisheye. It does not leave the pair's fundamental matrix alone. Two views
// that share one K satisfy E = K^T F K, and an essential matrix has singular
// values (s, s, 0): sweep f, watch the two leading singular values of K^T F K
// converge, and the focal falls out of geometry the pipeline already computes.
//
// The criterion is sharp only when the motion says something. A pair whose
// baseline points down the optical axis, or whose scene is a plane, is nearly
// flat in f and its argmin is noise. So each pair votes with its own argmin,
// votes are kept only when the minimum is a real dip, and the group takes the
// median. Too few votes, or votes that disagree, and the geometric guess
// stands -- an unmeasured focal is bad, a confidently wrong one is worse.
//
// This matters because the guess is 1.2x the long edge, i.e. a ~45 deg
// diagonal field of view. A wide lens is off by 3x, and everything downstream
// -- seed choice, PnP, the model's shape -- is built on it.
//
// Accuracy, measured against the focal a full reconstruction's bundle
// adjustment settles on: within 1.5-6%, biased low, which is what a
// centre-assumed principal point and near-degenerate motion do to any
// estimator of this family. That is why the answer is a *starting point* the
// mapper's probe still refines (MapperOptions::measured_focal_cameras) rather
// than a prior, and why the search only claims a group whose votes agree.
struct FocalVoteOptions {
    double min_ratio = 0.25, max_ratio = 3.0;  // search window around the guess
    int samples = 41;                          // log-spaced points in it
    // A vote counts when the dip at the argmin is this much below the window's
    // median asymmetry: a flat curve has nothing to say.
    double min_contrast = 0.35;
    int min_votes = 8;
    // ... and the votes have to agree: median absolute deviation over median.
    double max_spread = 0.25;
};

struct FocalVoteReport {
    double focal = 0;     // 0 = no confident answer
    int votes = 0, pairs = 0;
    double spread = 1.0;  // MAD / median of the accepted votes
};

// |s1 - s2| / (s1 + s2) of K^T F K -- zero exactly when F is an essential
// matrix for this K, and scale-free so it can be compared across focals.
inline double essentialAsymmetry(const Mat3& F, double f, double cx, double cy) {
    // K = [[f,0,cx],[0,f,cy],[0,0,1]]; K^T F K written out.
    Mat3 K = {f, 0, cx, 0, f, cy, 0, 0, 1};
    Svd3 s = svd3(mul(transpose(K), mul(F, K)));
    const double d = s.s.x + s.s.y;
    return d > 0 ? (s.s.x - s.s.y) / d : 1.0;
}

// The argmin of a sampled curve, refined by a parabola through the three
// points around it, in the curve's own (log-focal) coordinate. Returns the
// index offset; the caller converts. Without the parabola the answer is
// quantized to the grid, which at 41 points over a factor of twelve is 6% --
// the size of the error actually observed against bundle-adjusted focals.
inline bool refineMinimum(const std::vector<double>& v, int& best_i, double& frac) {
    best_i = -1;
    double best = 1e30;
    for (int i = 0; i < (int)v.size(); i++)
        if (v[i] < best) { best = v[i]; best_i = i; }
    // An argmin pinned to an end of the window is an extrapolation, not a dip.
    if (best_i <= 0 || best_i + 1 >= (int)v.size()) return false;
    const double y0 = v[best_i - 1], y1 = v[best_i], y2 = v[best_i + 1];
    const double den = y0 - 2 * y1 + y2;
    frac = 0.0;
    if (den > 0) {
        const double d = 0.5 * (y0 - y2) / den;
        if (std::fabs(d) < 1.0) frac = d;
    }
    return true;
}

// One pair's vote as a (fractional) index into the log-focal window, or -1 when
// its curve is too flat to have an opinion. `F` is the pixel fundamental matrix
// of the pair, `guess` the focal the window is centred on.
inline double focalVote(const Mat3& F, double guess, double cx, double cy,
                        const FocalVoteOptions& o) {
    const double lo = std::log(guess * o.min_ratio), hi = std::log(guess * o.max_ratio);
    const double step = (hi - lo) / (o.samples - 1);
    std::vector<double> vals;
    vals.reserve(o.samples);
    for (int i = 0; i < o.samples; i++)
        vals.push_back(essentialAsymmetry(F, std::exp(lo + step * i), cx, cy));
    int best_i = 0;
    double frac = 0;
    if (!refineMinimum(vals, best_i, frac)) return -1;
    std::vector<double> sorted(vals);
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double med = sorted[sorted.size() / 2];
    if (med <= 0 || vals[best_i] > (1.0 - o.min_contrast) * med) return -1;
    return best_i + frac;
}

// Estimate one rectilinear group's focal from the pairs whose two images are
// both in it. `pairs[i]` has `matches[i]`, as for bootstrapGroupFocals.
inline FocalVoteReport focalFromEpipolar(const std::vector<FeatureSet>& feats,
                                         const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                                         const std::vector<std::vector<FeatureMatch>>& matches,
                                         const Camera& cam, const TwoViewOptions& tvopt,
                                         const FocalVoteOptions& fo, int threads) {
    FocalVoteReport rep;
    rep.pairs = (int)pairs.size();
    std::mutex mtx;
    std::vector<double> votes;
    std::atomic<size_t> next{0};
    auto worker = [&] {
        std::vector<double> local;
        for (;;) {
            const size_t s = next++;
            if (s >= pairs.size()) break;
            const std::vector<FeatureMatch>& m = matches[s];
            if ((int)m.size() < tvopt.min_num_inliers) continue;
            const uint32_t i = pairs[s].first, j = pairs[s].second;
            const int n = (int)m.size();
            std::vector<Vec2> x1(n), x2(n);
            for (int k = 0; k < n; k++) {
                const Keypoint& a = feats[i].keypoints[m[k].idx1];
                const Keypoint& b = feats[j].keypoints[m[k].idx2];
                x1[k] = {a.x, a.y};
                x2[k] = {b.x, b.y};
            }
            RansacOptions ro = tvopt.ransac;
            ro.max_error = tvopt.ransac.max_error *
                           0.5 * (feats[i].pixelScale() + feats[j].pixelScale());
            ro.max_num_trials = std::min(ro.max_num_trials, 2000);
            auto fit = [&](const std::vector<int>& s2) { return estimateFundamental7(x1, x2, s2); };
            auto refit = [&](const std::vector<int>& s2) {
                return estimateFundamental8(x1, x2, s2);
            };
            auto res = [&](const Mat3& F, int k) { return sampsonSq(F, x1[k], x2[k]); };
            RansacReport<Mat3> r = loransac<Mat3>(n, 7, fit, refit, res, ro);
            if (r.num_inliers < std::max(30, tvopt.min_num_inliers)) continue;
            const double v = focalVote(r.model, cam.focal(), cam.cx, cam.cy, fo);
            if (v >= 0) local.push_back(v);
        }
        std::lock_guard<std::mutex> lk(mtx);
        votes.insert(votes.end(), local.begin(), local.end());
    };
    const int nt = threads > 0 ? threads : (int)std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (int t = 0; t < std::max(1, nt); t++) pool.emplace_back(worker);
    for (std::thread& t : pool) t.join();

    rep.votes = (int)votes.size();
    if (rep.votes < fo.min_votes) return rep;
    // Median vote, and the median absolute deviation as the confidence. Both
    // are computed on the grid index, which *is* the log focal, so the
    // deviation is a relative spread with no further conversion.
    std::nth_element(votes.begin(), votes.begin() + votes.size() / 2, votes.end());
    const double med = votes[votes.size() / 2];
    std::vector<double> dev;
    dev.reserve(votes.size());
    for (double v : votes) dev.push_back(std::fabs(v - med));
    std::nth_element(dev.begin(), dev.begin() + dev.size() / 2, dev.end());
    const double lo = std::log(cam.focal() * fo.min_ratio);
    const double step = (std::log(cam.focal() * fo.max_ratio) - lo) / (fo.samples - 1);
    rep.spread = std::exp(dev[dev.size() / 2] * step) - 1.0;
    if (rep.spread > fo.max_spread) return rep;
    rep.focal = std::exp(lo + step * med);
    return rep;
}

// Run the above for every rectilinear group that has no focal prior, and
// record the ones that answered confidently in `measured`.
inline void bootstrapRectilinearFocals(const std::vector<FeatureSet>& feats,
                                       const std::vector<uint32_t>& cam_ids,
                                       const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                                       const std::vector<std::vector<FeatureMatch>>& matches,
                                       std::map<uint32_t, Camera>& cams,
                                       const std::set<uint32_t>& given,
                                       std::set<uint32_t>& measured,
                                       const TwoViewOptions& tvopt, size_t max_sample,
                                       int threads, bool verbose) {
    std::map<uint32_t, std::vector<size_t>> by_group;
    for (size_t p = 0; p < pairs.size() && p < matches.size(); p++) {
        const uint32_t a = pairs[p].first, b = pairs[p].second;
        if (a >= cam_ids.size() || b >= cam_ids.size() || cam_ids[a] != cam_ids[b]) continue;
        by_group[cam_ids[a]].push_back(p);
    }
    const FocalVoteOptions fo;
    for (auto& kv : cams) {
        const uint32_t id = kv.first;
        if (kv.second.isFisheye() || kv.second.isSpherical() || given.count(id)) continue;
        auto g = by_group.find(id);
        if (g == by_group.end() || (int)g->second.size() < fo.min_votes) continue;
        std::vector<std::pair<uint32_t, uint32_t>> sp;
        std::vector<std::vector<FeatureMatch>> sm;
        const size_t stride =
            std::max<size_t>(1, g->second.size() / std::max<size_t>(1, max_sample));
        for (size_t k = 0; k < g->second.size() && sp.size() < max_sample; k += stride) {
            sp.push_back(pairs[g->second[k]]);
            sm.push_back(matches[g->second[k]]);
        }
        FocalVoteReport r =
            focalFromEpipolar(feats, sp, sm, kv.second, tvopt, fo, threads);
        if (r.focal > 0) {
            if (verbose)
                slog::diag(slog::Tag::Match,
                           "[focal] camera %u: %.0f -> %.0f px from %d/%d epipolar vote(s), "
                           "spread %.0f%%",
                           id, kv.second.focal(), r.focal, r.votes, r.pairs, 100.0 * r.spread);
            kv.second.setFocal(r.focal);
            measured.insert(id);
        } else if (verbose) {
            slog::diag(slog::Tag::Match,
                       "[focal] camera %u: %d/%d epipolar vote(s)%s; keeping the f=%.0f guess",
                       id, r.votes, r.pairs,
                       r.votes >= fo.min_votes ? " disagree" : " -- too few", kv.second.focal());
        }
    }
}

struct VerificationOptions {
    TwoViewOptions two_view;
    // When set, pairs whose two images both have bearings are verified on the
    // sphere; `two_view.ransac.max_error` stays in pixels and is converted per
    // pair with the two cameras' focal lengths.
    const BearingCache* bearings = nullptr;
    // Worker threads for the RANSAC. 0 = hardware_concurrency, 1 = verify
    // inline on the calling thread (no pool, no queue).
    int num_threads = 0;
    // Producer backpressure: the queue holds at most num_threads * this many
    // pairs of putative matches, so a slow verifier cannot make the matcher
    // buffer the whole dataset.
    int queue_depth_per_thread = 4;
    // Pairs requested from the matcher per call. Must match the matcher's own
    // batch size to get the benefit; see MatchOptions::batch_pairs.
    int match_batch_pairs = 64;
    // Where every finished pair is recorded so an interrupted run need not
    // verify it again (sfm/core/Resume.h). Null writes nothing.
    resume::MatchJournal* journal = nullptr;
    // Where this call sits in a larger list: a resumed run is handed only the
    // pairs its journal has not got, and the bar still counts the whole stage.
    size_t progress_done_base = 0;
    size_t progress_total = 0;   // 0 = pairs.size()
};

inline int verificationThreadCount(const VerificationOptions& opt) {
    if (opt.num_threads > 0) return opt.num_threads;
    unsigned hc = std::thread::hardware_concurrency();
    return hc > 0 ? (int)hc : 1;
}

// Match and verify every pair. `matchFn(begin, end, out)` fills `out` with the
// putative matches for pairs[begin..end) in order, and is only ever called on
// the calling thread (GPU matchers are not thread-safe). Batching is the
// matcher's business, not ours -- it lets a GPU matcher amortize uploads and
// fence round trips over many pairs (D22) -- so we hand it a run and push the
// results into the queue one at a time, keeping the backpressure per pair.
// `progress(done, total)` is optional and also called only on the calling
// thread.
//
// Returned entries keep the pair list's order and only cover pairs that
// survived: fewer putatives than `min_num_inliers`, or a degenerate/undefined
// two-view config, drops the pair entirely. `putative_out` accumulates the
// pre-verification match count.
inline std::vector<TwoViewMatches> verifyPairs(
    const std::vector<FeatureSet>& feats,
    const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
    const std::function<void(size_t, size_t, std::vector<std::vector<FeatureMatch>>&)>& matchFn,
    const VerificationOptions& opt, uint64_t* putative_out = nullptr,
    const std::function<void(size_t, size_t)>& progress = nullptr) {

    const int nthreads = verificationThreadCount(opt);
    const TwoViewOptions& tv = opt.two_view;

    // Per-pair slot, filled by whichever worker takes the job.
    std::vector<TwoViewMatches> results(pairs.size());
    std::vector<char> kept(pairs.size(), 0);
    uint64_t putative = 0;

    // Verify one pair in place, returning the inliers it kept; the only work a
    // worker does.
    auto verifyBody = [&](size_t p, std::vector<FeatureMatch>& m,
                          uint32_t i, uint32_t j) -> uint32_t {
        if ((int)m.size() < tv.min_num_inliers) return 0;  // below the inlier floor
        // The threshold is in extraction pixels; each image converts it to its
        // own (D47). They differ whenever the extractor downscaled one image
        // more than the other, which a mixed-resolution capture does routinely.
        const double sc = 0.5 * (feats[i].pixelScale() + feats[j].pixelScale());
        TwoViewGeometry g;
        if (opt.bearings && opt.bearings->has(i) && opt.bearings->has(j)) {
            const BearingCache& bc = *opt.bearings;
            std::vector<Vec3> b1(m.size()), b2(m.size());
            for (size_t k = 0; k < m.size(); k++) {
                b1[k] = bc.bearings[i][m[k].idx1];
                b2[k] = bc.bearings[j][m[k].idx2];
            }
            TwoViewOptions tvb = tv;
            double f = 0.5 * (bc.cameras[i].focal() + bc.cameras[j].focal());
            tvb.ransac.max_error = tv.ransac.max_error * sc / std::max(1.0, f);  // px -> rad
            g = estimateTwoViewBearing(b1, b2, tvb);
        } else {
            std::vector<Vec2> q1(m.size()), q2(m.size());
            for (size_t k = 0; k < m.size(); k++) {
                const Keypoint& a = feats[i].keypoints[m[k].idx1];
                const Keypoint& b = feats[j].keypoints[m[k].idx2];
                q1[k] = {a.x, a.y};
                q2[k] = {b.x, b.y};
            }
            TwoViewOptions tvp = tv;
            tvp.ransac.max_error = tv.ransac.max_error * sc;  // extraction px -> source px
            g = estimateTwoView(q1, q2, tvp);
        }
        if (g.config == TwoViewConfig::Degenerate || g.config == TwoViewConfig::Undefined) return 0;
        std::vector<FeatureMatch> inl;
        inl.reserve(g.num_inliers);
        for (size_t k = 0; k < m.size(); k++)
            if (g.inlier_mask[k]) inl.push_back(m[k]);
        if (inl.empty()) return 0;
        const uint32_t n_inl = (uint32_t)inl.size();
        results[p] = {i, j, (int)g.config, std::move(inl)};
        kept[p] = 1;
        return n_inl;
    };

    auto verifyOne = [&](size_t p, std::vector<FeatureMatch>& m) {
        const uint32_t i = pairs[p].first, j = pairs[p].second;
        // Every pair, kept or not: a screen watching the matrix cannot tell a
        // pair that failed from one nothing has reached yet unless both are
        // reported. A no-op without --progress-dir, and called from the
        // workers, which is what the lock inside it is for.
        const uint32_t inl = verifyBody(p, m, i, j);
        progress::pair(i, j, inl);
        if (inl) {
            const std::vector<FeatureMatch>& kept = results[p].matches;
            progress::live_pair(i, j, results[p].config,
                                &kept[0].idx1, &kept[0].idx2,
                                sizeof(FeatureMatch), (uint32_t)kept.size());
            if (opt.journal)
                opt.journal->record(i, j, results[p].config, (uint32_t)m.size(),
                                    &kept[0].idx1, &kept[0].idx2,
                                    sizeof(FeatureMatch), (uint32_t)kept.size());
        } else if (opt.journal) {
            // A pair that failed is recorded too: without it a resumed run
            // cannot tell one nothing reached from one already answered, and
            // the pairs that fail are the expensive half.
            opt.journal->record(i, j, 0, (uint32_t)m.size(), nullptr, nullptr, 4, 0);
        }
        Event e;
        e.kind = Event::Kind::PairVerified;
        e.stage = Stage::Match;
        e.image_a = i;
        e.image_b = j;
        e.inliers = inl;
        events::emit(e);
    };

    // How many pairs to ask the matcher for at once. Independent of the worker
    // count: it only bounds how far the GPU may run ahead of the verifiers.
    const size_t batch = std::max<size_t>(1, (size_t)opt.match_batch_pairs);
    std::vector<std::vector<FeatureMatch>> batch_out;

    // The stage's fraction, at most a few hundred steps over the whole of it.
    // Per pair it is a global lock the workers are already contending for, and
    // a screen cannot show 700k of anything.
    const size_t step = std::max<size_t>(1, pairs.size() / 400);
    const size_t ptotal = opt.progress_total ? opt.progress_total : pairs.size();
    auto tick = [&](size_t p) {
        if ((p + 1) % step == 0 || p + 1 == pairs.size())
            events::progress(Stage::Match, (int64_t)(opt.progress_done_base + p + 1),
                             (int64_t)ptotal);
    };

    if (nthreads <= 1) {
        for (size_t b = 0; b < pairs.size(); b += batch) {
            size_t e = std::min(b + batch, pairs.size());
            matchFn(b, e, batch_out);
            for (size_t p = b; p < e; p++) {
                std::vector<FeatureMatch>& m = batch_out[p - b];
                putative += m.size();
                verifyOne(p, m);
                if (progress) progress(p + 1, pairs.size());
                tick(p);
                cancel::check();
            }
        }
    } else {
        struct Job {
            size_t index;
            std::vector<FeatureMatch> matches;
        };
        std::queue<Job> queue;
        std::mutex mtx;
        std::condition_variable cv_job, cv_space;
        bool done_producing = false;
        const size_t max_queued = (size_t)nthreads * std::max(1, opt.queue_depth_per_thread);

        std::vector<std::thread> workers;
        workers.reserve(nthreads);
        for (int t = 0; t < nthreads; t++) {
            workers.emplace_back([&] {
                for (;;) {
                    Job job;
                    {
                        std::unique_lock<std::mutex> lk(mtx);
                        cv_job.wait(lk, [&] { return !queue.empty() || done_producing; });
                        if (queue.empty()) return;  // drained and the producer is finished
                        job = std::move(queue.front());
                        queue.pop();
                    }
                    cv_space.notify_one();
                    verifyOne(job.index, job.matches);
                }
            });
        }

        // A cancel stops the producer and lets the queue drain; it must not
        // throw from here, because the workers are still waiting on `cv_job`
        // and an unwind past their join() would terminate the process.
        bool stop = false;
        for (size_t b = 0; b < pairs.size() && !stop; b += batch) {
            size_t e = std::min(b + batch, pairs.size());
            matchFn(b, e, batch_out);
            for (size_t p = b; p < e; p++) {
                std::vector<FeatureMatch>& m = batch_out[p - b];
                putative += m.size();
                {
                    std::unique_lock<std::mutex> lk(mtx);
                    cv_space.wait(lk, [&] { return queue.size() < max_queued; });
                    queue.push({p, std::move(m)});
                }
                cv_job.notify_one();
                if (progress) progress(p + 1, pairs.size());
                tick(p);
                if (cancel::requested()) { stop = true; break; }
            }
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            done_producing = true;
        }
        cv_job.notify_all();
        for (std::thread& w : workers) w.join();
    }
    cancel::check();   // safe now: every worker has been joined

    if (putative_out) *putative_out = putative;

    std::vector<TwoViewMatches> out;
    for (size_t p = 0; p < pairs.size(); p++)
        if (kept[p]) out.push_back(std::move(results[p]));
    return out;
}

}  // namespace sfm
