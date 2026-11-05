// The metric gauge: a Sim(3) from the model's camera centres onto reference
// positions in metres, with the uncertainty that says whether to trust it.
//
// Where Orient.h fixes the gauge from the poses alone -- upright, centred,
// unit-sized -- this fixes it from an outside measurement, so the written
// model is in metres and every consumer inherits that for free (D74).
//
// The scale and orientation uncertainties are REPORTED, never gated on: they
// assume uncorrelated noise, and measured against a reference whose error is
// correlated they under-state it by 3.9-4.5x (D74). What gates is geometry --
// how far the reference positions spread, and how close to a line they lie.
#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "sfm/core/Exif.h"
#include "sfm/core/Geodetic.h"
#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"
#include "sfm/map/Orient.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/optim/Ransac.h"
#include "sfm/map/Merge.h"

namespace sfm {

// Camera centres in the model's own gauge, paired by index with reference
// positions in metres. `image_ids` is what the caller needs to report; the fit
// never reads it.
struct MetricRef {
    std::vector<Vec3> centres;
    std::vector<Vec3> targets;
    std::vector<uint32_t> image_ids;
    GeoOrigin origin;   // where `targets` are centred, when they are GPS
    // A telemetry capture for every pair, or -1 where no identity exists.
    // A full GPS fit may remove one barometric altitude constant per capture.
    std::vector<int> tracks;
};

enum class MetricFail { None, Pairs, Distinct, Spread, Inliers, Collinear, Tilted };

// Which components of the reference carry the gauge. Horizontal reads the
// level pair and takes the tilt from the caller's own up axis: a GPS altitude
// biased by metres tips a full fit by degrees over a 100 m capture (D75).
enum class MetricAxes { Full, Horizontal };

enum class MetricTrackOffsets { None, Vertical };

struct MetricFit {
    bool ok = false;
    MetricFail reason = MetricFail::Pairs;
    Sim3 T;
    int n = 0;                  // paired cameras offered
    int distinct = 0;           // ... at different reference positions
    bool flat = false;          // a level fit
    int inliers = 0;
    double max_error = 0;       // metres
    double rms = 0;             // metres, over the inliers
    double scale_unc = 0;       // per cent, std(ds/s) -- advisory
    double rot_unc_deg = 0;     // worst principal axis -- advisory
    double spread = 0;          // metres, RMS radius of the reference positions
    double perp_frac = 0;       // spread across the least-resisted axis, over the whole
    // Metres per axis: the inliers' scatter about the fit, and its level and vertical
    // parts (sigma_v 0 on a level fit); `centroid` is their mean reference position.
    double sigma = 0, sigma_h = 0, sigma_v = 0;
    Vec3 centroid{0, 0, 0};
    // Horizontal only: the scale the inliers' 3D distances give (0 without enough
    // pairs), and the robust sigma of their altitude about the fitted vertical, metres.
    double scale_3d = 0;
    double vertical_sigma = 0;
    std::vector<char> inlier_mask;
    // The amount subtracted from a track's stated ENU altitude before the
    // fit. The weighted mean is zero; the global Z translation owns the datum.
    std::vector<int> track_ids;
    std::vector<double> track_vertical_offsets;

    double trackVerticalOffset(int track) const {
        for (size_t i = 0; i < track_ids.size() && i < track_vertical_offsets.size(); i++)
            if (track_ids[i] == track) return track_vertical_offsets[i];
        return 0.0;
    }
};

// Orientation error grows as 1/perp_frac against scale error, so 0.05 refuses
// a reference amplifying it beyond 20x -- a starting heuristic, not a bound:
// the derivation gives the shape, not 20 rather than 10 (D74, one flight).
inline constexpr double kMetricMinPerpFraction = 0.05;

// Distinct reference positions a scattered fit needs: twice as many equations as it has
// parameters (4 level, 7 full). A bracketing phone repeats each fix five times, and two
// fixes fit a level similarity exactly -- a zero scatter that reads as a centimetre radius.
inline int metricMinDistinct(bool flat) { return flat ? 4 : 5; }

// A scattered radius never under this fraction of the spread: drift bends a model off a
// similarity at that level, and a radius under it splits the capture at the bend (one leg
// of an L-shaped track went outlier, sfm_sensor_prior_test).
inline constexpr double kMetricMinRadiusFrac = 0.005;

// A level fit's scale past this multiple of the 3D one: the up it was levelled
// about tips the camera path, whose shortened level part inflates the scale. An
// Avata clip's IMU up ~90 deg off read 35x; a path climbing 37 deg reads 1.25x.
inline constexpr double kMetricMaxLevelScaleRatio = 1.25;

namespace detail {

inline Vec3 meanOf(const std::vector<Vec3>& v) {
    Vec3 m{0, 0, 0};
    for (const Vec3& p : v) m = m + p;
    return v.empty() ? m : m * (1.0 / (double)v.size());
}

inline double medianOf(std::vector<double> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + (long)(v.size() / 2), v.end());
    return v[v.size() / 2];
}

// Median of reference over model distance between inliers a half and a third of
// the set apart, rotation-free. Pairs under `min_dist` metres are noise; 0 under 5 pairs.
inline double pairwiseScale(const MetricRef& ref, const std::vector<int>& keep, double min_dist) {
    const size_t m = keep.size();
    std::vector<double> r;
    for (size_t a = 0; a < m; a++)
        for (size_t step : {m / 2, m / 3}) {
            if (!step) continue;
            const int i = keep[a], j = keep[(a + step) % m];
            const double dg = (ref.targets[i] - ref.targets[j]).norm();
            const double dc = (ref.centres[i] - ref.centres[j]).norm();
            if (dg > min_dist && dc > 0) r.push_back(dg / dc);
        }
    return r.size() < 5 ? 0.0 : medianOf(std::move(r));
}

// Chi-square median and 99th-percentile radius of a residual over 2 (level) or 3 axes:
// a per-axis sigma from a residual norm, and the norm an inlier stays under.
inline double chi2Median(bool flat) { return flat ? 1.3863 : 2.3660; }
inline double chi99(bool flat) { return flat ? 3.0349 : 3.3682; }

// Per-axis sigma of the least-median-of-squares fit over minimal samples, with
// Rousseeuw's finite-sample factor: a scale no threshold went into. 0 when n is minimal.
template <class Fit, class Res>
inline double lmedsSigma(int n, int sample, bool flat, Fit fit, Res res) {
    if (n <= sample) return 0;
    std::mt19937 rng(0);
    std::uniform_int_distribution<int> pick(0, n - 1);
    std::vector<int> idx;
    std::vector<double> r2((size_t)n);
    double best = std::numeric_limits<double>::infinity();
    for (int t = 0; t < 200; t++) {
        idx.clear();
        while ((int)idx.size() < sample) {
            const int i = pick(rng);
            if (std::find(idx.begin(), idx.end(), i) == idx.end()) idx.push_back(i);
        }
        const auto models = fit(idx);
        if (models.empty()) continue;
        for (int i = 0; i < n; i++) r2[(size_t)i] = res(models.front(), i);
        std::nth_element(r2.begin(), r2.begin() + n / 2, r2.end());
        best = std::min(best, r2[(size_t)(n / 2)]);
    }
    if (!std::isfinite(best)) return 0;
    return std::sqrt(best / chi2Median(flat)) * (1.0 + 5.0 / (double)(n - sample));
}

}  // namespace detail

// The similarity restricted to scale, heading and place: one rotation about
// +Z, fitted to the level components alone. Flattening the targets and calling
// estimateSim3 instead would fit a full rotation and tip the model into them.
inline bool estimateSim3Yaw(const std::vector<Vec3>& src, const std::vector<Vec3>& dst,
                            Sim3& out) {
    const size_t n = src.size();
    if (n < 2 || dst.size() != n) return false;
    Vec3 ms{0, 0, 0}, md{0, 0, 0};
    for (size_t i = 0; i < n; i++) { ms = ms + src[i]; md = md + dst[i]; }
    ms = ms * (1.0 / (double)n);
    md = md * (1.0 / (double)n);
    double var_s = 0, dot = 0, cross = 0;
    for (size_t i = 0; i < n; i++) {
        const Vec3 a = src[i] - ms, b = dst[i] - md;
        var_s += a.x * a.x + a.y * a.y;
        dot += a.x * b.x + a.y * b.y;
        cross += a.x * b.y - a.y * b.x;
    }
    const double h = std::sqrt(dot * dot + cross * cross);
    if (!(var_s > 1e-12) || !(h > 1e-12)) return false;
    const double c = dot / h, s = cross / h;
    out.scale = h / var_s;
    if (!(out.scale > 1e-12) || !std::isfinite(out.scale)) return false;
    out.R = Mat3{c, -s, 0, s, c, 0, 0, 0, 1};
    out.t = md - mul(out.R, ms) * out.scale;
    return std::isfinite(out.t.x) && std::isfinite(out.t.y) && std::isfinite(out.t.z);
}

// The similarity taking `ref.centres` onto `ref.targets`, refused with a named reason when
// the data cannot support one. `max_error` > 0 fixes the inlier radius (or `max_error_frac`
// of the spread, if larger); 0 takes it from the references' own scatter about the fit.
inline MetricFit fitMetricGaugeRaw(const MetricRef& ref, double max_error,
                                   MetricAxes axes = MetricAxes::Full,
                                   double max_error_frac = 0.0) {
    MetricFit out;
    out.max_error = max_error;
    const bool flat = axes == MetricAxes::Horizontal;
    const bool adaptive = !(max_error > 0);
    const int n = (int)ref.centres.size();
    out.n = n;
    out.flat = flat;
    if (n < 3 || (int)ref.targets.size() != n) {
        out.reason = MetricFail::Pairs;
        return out;
    }
    // Only a scattered radius trusts what the residuals say; a fixed one never reads them.
    auto distinctOf = [&](const std::vector<int>* subset) {
        std::set<std::tuple<double, double, double>> seen;
        auto add = [&](int i) {
            const Vec3& p = ref.targets[(size_t)i];
            seen.emplace(p.x, p.y, flat ? 0.0 : p.z);
        };
        if (subset) for (int i : *subset) add(i);
        else for (int i = 0; i < n; i++) add(i);
        return (int)seen.size();
    };
    out.distinct = distinctOf(nullptr);
    if (adaptive && out.distinct < metricMinDistinct(flat)) {
        out.reason = MetricFail::Distinct;
        return out;
    }

    const Vec3 pbar = detail::meanOf(ref.targets);
    double var_t = 0;
    for (const Vec3& p : ref.targets) {
        const Vec3 d = p - pbar;
        var_t += flat ? d.x * d.x + d.y * d.y : d.dot(d);
    }
    out.spread = std::sqrt(var_t / (double)n);

    auto fit_fn = [&](const std::vector<int>& idx) {
        std::vector<Sim3> models;
        std::vector<Vec3> s, d;
        s.reserve(idx.size());
        d.reserve(idx.size());
        for (int i : idx) {
            s.push_back(ref.centres[i]);
            d.push_back(ref.targets[i]);
        }
        Sim3 T;
        if (flat ? estimateSim3Yaw(s, d, T) : estimateSim3(s, d, T)) models.push_back(T);
        return models;
    };
    auto res_fn = [&](const Sim3& T, int i) {
        const Vec3 r = ref.targets[i] - transformPoint(T, ref.centres[i]);
        return flat ? r.x * r.x + r.y * r.y : r.dot(r);
    };

    // An RTK flight scatters by centimetres and a phone by metres; a radius fixed
    // for one is blind or deaf on the other.
    if (adaptive)
        max_error = std::max(detail::chi99(flat) *
                                 detail::lmedsSigma(n, flat ? 2 : 3, flat, fit_fn, res_fn),
                             kMetricMinRadiusFrac * out.spread);
    else
        max_error = std::max(max_error, max_error_frac * out.spread);
    out.max_error = max_error;
    // References spread no wider than their own noise carry no scale to fit, and
    // every fit to them is an arbitrary one that would pass an RMS gate.
    if (!(out.spread > max_error)) {
        out.reason = MetricFail::Spread;
        return out;
    }

    RansacOptions opt;
    opt.max_error = max_error;
    opt.max_num_trials = 1000;
    opt.seed = 0;
    RansacReport<Sim3> rep = loransac<Sim3>(n, flat ? 2 : 3, fit_fn, fit_fn, res_fn, opt);
    out.inlier_mask = rep.inlier_mask;
    out.inliers = std::max(rep.num_inliers, 0);
    const int need = std::max(3, (n + 1) / 2);
    if (!rep.success || out.inliers < need) {
        out.reason = MetricFail::Inliers;
        return out;
    }

    std::vector<int> keep;
    // Seven parameters come out of 3m residual components -- four out of the
    // 2m level ones -- so the noise estimate divides by what is left.
    auto noise = [&](const std::vector<int>& k) {
        double s = 0;
        for (int i : k) s += res_fn(out.T, i);
        const int dof = flat ? 2 * (int)k.size() - 4 : 3 * (int)k.size() - 7;
        return dof > 0 ? std::sqrt(s / (double)dof) : 0.0;
    };
    for (int pass = 0; pass < 2; pass++) {
        keep.clear();
        for (int i = 0; i < n; i++)
            if (out.inlier_mask[i]) keep.push_back(i);
        std::vector<Sim3> refit = fit_fn(keep);
        if (refit.empty() || (int)keep.size() < need) {
            out.reason = MetricFail::Inliers;
            return out;
        }
        out.T = refit.front();
        if (!adaptive || pass) break;
        // The LMedS scale is a median's; the inliers' own scatter sets the radius.
        max_error =
            std::max(detail::chi99(flat) * noise(keep), kMetricMinRadiusFrac * out.spread);
        out.max_error = max_error;
        if (!(out.spread > max_error)) {
            out.reason = MetricFail::Spread;
            return out;
        }
        const double r2 = max_error * max_error;
        for (int i = 0; i < n; i++) out.inlier_mask[i] = res_fn(out.T, i) < r2;
    }
    out.inliers = (int)keep.size();
    if (adaptive && distinctOf(&keep) < metricMinDistinct(flat)) {
        out.reason = MetricFail::Distinct;
        out.distinct = distinctOf(&keep);
        return out;
    }

    const int m = (int)keep.size();
    double ss = 0, ssz = 0;
    Vec3 cbar{0, 0, 0};
    for (int i : keep) {
        ss += res_fn(out.T, i);
        cbar = cbar + ref.centres[i];
        out.centroid = out.centroid + ref.targets[i];
        if (!flat) ssz += std::pow(ref.targets[i].z - transformPoint(out.T, ref.centres[i]).z, 2);
    }
    cbar = cbar * (1.0 / (double)m);
    out.centroid = out.centroid * (1.0 / (double)m);
    out.rms = std::sqrt(ss / (double)m);
    const double sigma = noise(keep);
    out.sigma = sigma;
    // Split by the share of the residual each part carries, at the fit's degrees of freedom.
    const double share = std::max(ss, 1e-300);
    out.sigma_h = flat ? sigma : sigma * std::sqrt(1.5 * std::max(ss - ssz, 0.0) / share);
    out.sigma_v = flat ? 0.0 : sigma * std::sqrt(3.0 * ssz / share);

    std::vector<double> C(9, 0.0);
    for (int i : keep) {
        const Vec3 a = ref.centres[i] - cbar;
        const double v[3] = {a.x, a.y, flat ? 0.0 : a.z};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) C[3 * r + c] += v[r] * v[c];
    }
    const double s2 = out.T.scale * out.T.scale;
    for (double& v : C) v *= s2 / (double)m;
    std::vector<double> lam, V;
    jacobiEigenSymmetric(C, 3, lam, V);
    const double tr = lam[0] + lam[1] + lam[2];

    out.scale_unc = 100.0 * sigma / (std::sqrt((double)m) * std::sqrt(tr));
    // Rotation about principal axis k is resisted only by the spread across
    // it, and none at all makes the angle unidentifiable; the horizontal fit
    // turns about the vertical alone, which the whole in-plane radius resists.
    double worst = 0, perp_min = 0;
    for (int k = 0; k < 3; k++) {
        const double perp = flat ? tr : tr - lam[k];
        worst = std::max(worst, perp > 0.0 ? sigma / std::sqrt((double)m * perp)
                                           : std::numeric_limits<double>::infinity());
        if (k == 0 || perp < perp_min) perp_min = perp;
    }
    out.rot_unc_deg = worst * 180.0 / M_PI;
    out.perp_frac = tr > 0.0 ? std::sqrt(std::max(perp_min, 0.0) / tr) : 0.0;

    // The level fit's vertical place is the median altitude offset, not
    // estimateSim3Yaw's mean, which a few fixes stated at sea level would drag.
    if (flat) {
        std::vector<double> rz;
        rz.reserve(keep.size());
        for (int i : keep) rz.push_back(ref.targets[i].z - transformPoint(out.T, ref.centres[i]).z);
        const double dz = detail::medianOf(rz);
        out.T.t.z += dz;
        for (double& r : rz) r = std::fabs(r - dz);
        out.vertical_sigma = 1.4826 * detail::medianOf(std::move(rz));
        out.scale_3d = detail::pairwiseScale(ref, keep, 2.0 * max_error);
        if (out.scale_3d > 0 && out.T.scale > kMetricMaxLevelScaleRatio * out.scale_3d) {
            out.reason = MetricFail::Tilted;
            return out;
        }
    }
    // The horizontal fit makes no rotation this can refuse.
    if (!flat && !(out.perp_frac >= kMetricMinPerpFraction)) {
        out.reason = MetricFail::Collinear;
        return out;
    }
    out.ok = true;
    out.reason = MetricFail::None;
    return out;
}

// A new recording can start a new barometric datum. With the centres levelled by a
// measured up, a level seed reads each track's step (a median, so fixes stated at sea
// level do not move it); the requested fit then runs on the corrected targets.
inline MetricFit fitMetricGauge(const MetricRef& ref, double max_error,
                                MetricAxes axes = MetricAxes::Full,
                                double max_error_frac = 0.0,
                                MetricTrackOffsets offsets = MetricTrackOffsets::None) {
    if (offsets != MetricTrackOffsets::Vertical || ref.tracks.size() != ref.centres.size())
        return fitMetricGaugeRaw(ref, max_error, axes, max_error_frac);

    std::map<int, int> offered;
    for (int track : ref.tracks)
        if (track >= 0) offered[track]++;
    if (std::count_if(offered.begin(), offered.end(), [](const auto& kv) { return kv.second >= 3; }) < 2)
        return fitMetricGaugeRaw(ref, max_error, axes, max_error_frac);

    // A full seed would trade a track's barometric step for a tilt.
    const MetricFit seed = fitMetricGaugeRaw(ref, max_error, MetricAxes::Horizontal, max_error_frac);
    if (!seed.ok) return fitMetricGaugeRaw(ref, max_error, axes, max_error_frac);
    std::map<int, std::vector<double>> residuals;
    for (size_t i = 0; i < ref.centres.size(); i++)
        if (ref.tracks[i] >= 0 && seed.inlier_mask[i])
            residuals[ref.tracks[i]].push_back(ref.targets[i].z -
                                               transformPoint(seed.T, ref.centres[i]).z);
    std::vector<int> tracks;
    std::vector<double> dz;
    double datum = 0.0;
    int n = 0;
    for (auto& kv : residuals) {
        const int count = (int)kv.second.size();
        if (count < 3) continue;
        tracks.push_back(kv.first);
        dz.push_back(detail::medianOf(std::move(kv.second)));
        datum += dz.back() * count;
        n += count;
    }
    if (tracks.size() < 2) return fitMetricGaugeRaw(ref, max_error, axes, max_error_frac);
    // Zero mean over the frames: the fit's translation owns the datum.
    for (double& z : dz) z -= datum / n;

    MetricRef corrected = ref;
    for (size_t i = 0; i < corrected.targets.size(); i++)
        for (size_t k = 0; k < tracks.size(); k++)
            if (corrected.tracks[i] == tracks[k]) corrected.targets[i].z -= dz[k];
    MetricFit fit = fitMetricGaugeRaw(corrected, max_error, axes, max_error_frac);
    if (!fit.ok) return fitMetricGaugeRaw(ref, max_error, axes, max_error_frac);
    fit.track_ids = std::move(tracks);
    fit.track_vertical_offsets = std::move(dz);
    return fit;
}

// What pairing found, so a run can say why it has fewer cameras than either
// side offered.
struct MetricPairCounts {
    int matched = 0;
    int unmatched_file = 0;    // positions naming no registered image
    int unmatched_model = 0;   // registered images with no position
};

// COLMAP's model_aligner --ref_images_path format: `image_name X Y Z` per
// line, metres, '#' comments, blank lines skipped. `err` names the line.
inline bool readMetricPositions(const std::string& path, std::map<std::string, Vec3>& out,
                                std::string& err) {
    std::ifstream f(path);
    if (!f) {
        err = "cannot open " + path;
        return false;
    }
    out.clear();
    std::string line;
    for (int lineno = 1; std::getline(f, line); lineno++) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        std::istringstream is(line);
        std::string name;
        if (!(is >> name)) continue;
        double x, y, z;
        std::string extra;
        if (!(is >> x >> y >> z) || (is >> extra)) {
            err = "line " + std::to_string(lineno) + ": expected `image_name X Y Z`";
            return false;
        }
        if (!out.emplace(name, Vec3{x, y, z}).second) {
            err = "line " + std::to_string(lineno) + ": " + name + " appears twice";
            return false;
        }
    }
    if (out.empty()) {
        err = "no positions in " + path;
        return false;
    }
    return true;
}

// Pair registered images to positions on the name the model carries, then on
// that name without its extension. Never on the basename: two folders holding
// one file name is a rig capture, not a duplicate.
inline MetricPairCounts pairMetricRef(const Reconstruction& rec,
                                      const std::map<std::string, Vec3>& positions,
                                      MetricRef& ref) {
    MetricPairCounts c;
    std::vector<char> used(positions.size(), 0);
    for (const auto& kv : rec.images) {
        if (!kv.second.registered) continue;
        auto it = positions.find(kv.second.name);
        if (it == positions.end()) {
            const size_t dot = kv.second.name.find_last_of('.');
            const size_t slash = kv.second.name.find_last_of('/');
            if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                it = positions.find(kv.second.name.substr(0, dot));
        }
        if (it == positions.end()) {
            c.unmatched_model++;
            continue;
        }
        used[(size_t)std::distance(positions.begin(), it)] = 1;
        ref.centres.push_back(cameraCenter(kv.second.pose));
        ref.targets.push_back(it->second);
        ref.image_ids.push_back(kv.first);
        c.matched++;
    }
    for (char u : used)
        if (!u) c.unmatched_file++;
    return c;
}


// What reading GPS off the images found.
struct MetricGpsCounts {
    int matched = 0;
    int no_gps = 0;
    int no_alt = 0;   // positioned, but with no altitude: treated as sea level
};

// Reference positions from each registered image's own EXIF, in a local ENU
// metre frame. Altitude is optional; a fix without one is still a fix. Images
// under a `skip` prefix count as carrying none.
inline MetricGpsCounts metricRefFromGps(const Reconstruction& rec, const std::string& image_dir,
                                        MetricRef& ref,
                                        const std::vector<std::string>& skip = {}) {
    MetricGpsCounts c;
    std::vector<Geodetic> g;
    std::vector<Vec3> centres;
    std::vector<uint32_t> ids;
    for (const auto& kv : rec.images) {
        if (!kv.second.registered) continue;
        const ExifData e =
            exifPrefixCovers(skip, kv.second.name)
                ? ExifData{}
                : readExif((std::filesystem::path(image_dir) / kv.second.name).string());
        if (!e.has_gps) {
            c.no_gps++;
            continue;
        }
        if (!e.has_alt) c.no_alt++;
        g.push_back({e.lat_deg, e.lon_deg, e.alt_m});
        centres.push_back(cameraCenter(kv.second.pose));
        ids.push_back(kv.first);
        c.matched++;
    }
    if (g.empty()) return c;
    ref.origin = {true, meanGeodetic(g), c.no_alt ? "" : "exif"};
    std::vector<Vec3> enu = enuFromGeodetic(g, ref.origin.at);
    for (size_t i = 0; i < enu.size(); i++) {
        ref.centres.push_back(centres[i]);
        ref.targets.push_back(enu[i]);
        ref.image_ids.push_back(ids[i]);
    }
    return c;
}


// Angle between the reference frame's +Z and where the cameras themselves say
// up is. A few degrees on a hand-held or gimballed capture; tens of degrees
// means a tilted reference or a tilted capture, and says which to look at.
inline double metricUpDisagreementDeg(const Reconstruction& rec) {
    const Sim3 up = uprightTransform(rec);
    return std::acos(std::max(-1.0, std::min(1.0, up.R[8]))) * 180.0 / M_PI;
}

}  // namespace sfm
