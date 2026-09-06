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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "sfm/core/Exif.h"
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
};

enum class MetricFail { None, Pairs, Spread, Inliers, Collinear };

struct MetricFit {
    bool ok = false;
    MetricFail reason = MetricFail::Pairs;
    Sim3 T;
    int n = 0;                  // paired cameras offered
    int inliers = 0;
    double max_error = 0;       // metres
    double rms = 0;             // metres, over the inliers
    double scale_unc = 0;       // per cent, std(ds/s) -- advisory
    double rot_unc_deg = 0;     // worst principal axis -- advisory
    double spread = 0;          // metres, RMS radius of the reference positions
    double perp_frac = 0;       // RMS spread across the long axis, over the whole
    std::vector<char> inlier_mask;
};

// Orientation error grows as 1/perp_frac against scale error, so 0.05 refuses
// a reference amplifying it beyond 20x -- a starting heuristic, not a bound:
// the derivation gives the shape, not 20 rather than 10 (D74, one flight).
inline constexpr double kMetricMinPerpFraction = 0.05;

namespace detail {

inline Vec3 meanOf(const std::vector<Vec3>& v) {
    Vec3 m{0, 0, 0};
    for (const Vec3& p : v) m = m + p;
    return v.empty() ? m : m * (1.0 / (double)v.size());
}

}  // namespace detail

// The similarity taking `ref.centres` onto `ref.targets`, refused with a named
// reason when the data cannot support one. `max_error` is the RANSAC inlier
// radius in metres.
inline MetricFit fitMetricGauge(const MetricRef& ref, double max_error) {
    MetricFit out;
    out.max_error = max_error;
    const int n = (int)ref.centres.size();
    out.n = n;
    if (n < 3 || (int)ref.targets.size() != n) {
        out.reason = MetricFail::Pairs;
        return out;
    }

    // Reference positions that do not spread out carry no scale to fit, and
    // every fit to them is an arbitrary one that would pass an RMS gate.
    const Vec3 pbar = detail::meanOf(ref.targets);
    double var_t = 0;
    for (const Vec3& p : ref.targets) var_t += (p - pbar).dot(p - pbar);
    out.spread = std::sqrt(var_t / (double)n);
    if (!(out.spread > max_error)) {
        out.reason = MetricFail::Spread;
        return out;
    }

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
        if (estimateSim3(s, d, T)) models.push_back(T);
        return models;
    };
    auto res_fn = [&](const Sim3& T, int i) {
        const Vec3 r = ref.targets[i] - transformPoint(T, ref.centres[i]);
        return r.dot(r);
    };

    RansacOptions opt;
    opt.max_error = max_error;
    opt.max_num_trials = 1000;
    opt.seed = 0;
    RansacReport<Sim3> rep = loransac<Sim3>(n, 3, fit_fn, fit_fn, res_fn, opt);
    out.inlier_mask = rep.inlier_mask;
    out.inliers = std::max(rep.num_inliers, 0);
    const int need = std::max(3, (n + 1) / 2);
    if (!rep.success || out.inliers < need) {
        out.reason = MetricFail::Inliers;
        return out;
    }

    std::vector<int> keep;
    keep.reserve(out.inliers);
    for (int i = 0; i < n; i++)
        if (out.inlier_mask[i]) keep.push_back(i);
    std::vector<Sim3> refit = fit_fn(keep);
    if (refit.empty()) {
        out.reason = MetricFail::Inliers;
        return out;
    }
    out.T = refit.front();

    const int m = (int)keep.size();
    double ss = 0;
    Vec3 cbar{0, 0, 0};
    for (int i : keep) {
        ss += res_fn(out.T, i);
        cbar = cbar + ref.centres[i];
    }
    cbar = cbar * (1.0 / (double)m);
    out.rms = std::sqrt(ss / (double)m);
    // Seven parameters come out of 3m residual components, so the noise
    // estimate divides by what is left.
    const double sigma = std::sqrt(ss / (double)(3 * m - 7));

    std::vector<double> C(9, 0.0);
    for (int i : keep) {
        const Vec3 a = ref.centres[i] - cbar;
        const double v[3] = {a.x, a.y, a.z};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) C[3 * r + c] += v[r] * v[c];
    }
    const double s2 = out.T.scale * out.T.scale;
    for (double& v : C) v *= s2 / (double)m;
    std::vector<double> lam, V;
    jacobiEigenSymmetric(C, 3, lam, V);
    const double tr = lam[0] + lam[1] + lam[2];

    out.scale_unc = 100.0 * sigma / (std::sqrt((double)m) * std::sqrt(tr));
    // Rotation about principal axis k is resisted only by the spread
    // perpendicular to it; where there is none the angle is unidentifiable
    // whatever the residual says.
    double worst = 0, perp_min = 0;
    for (int k = 0; k < 3; k++) {
        const double perp = tr - lam[k];
        worst = std::max(worst, perp > 0.0 ? sigma / std::sqrt((double)m * perp)
                                           : std::numeric_limits<double>::infinity());
        if (k == 0 || perp < perp_min) perp_min = perp;
    }
    out.rot_unc_deg = worst * 180.0 / M_PI;
    out.perp_frac = tr > 0.0 ? std::sqrt(std::max(perp_min, 0.0) / tr) : 0.0;

    if (!(out.perp_frac >= kMetricMinPerpFraction)) {
        out.reason = MetricFail::Collinear;
        return out;
    }
    out.ok = true;
    out.reason = MetricFail::None;
    return out;
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


struct Geodetic {
    double lat_deg = 0, lon_deg = 0, alt_m = 0;
};

// WGS-84 geodetic to earth-centred earth-fixed, in double: ECEF magnitudes are
// ~6.4e6 m, where a float's half-ulp is 0.25 m.
inline Vec3 ecefFromGeodetic(double lat_deg, double lon_deg, double h) {
    constexpr double a = 6378137.0, f = 1.0 / 298.257223563;
    constexpr double e2 = f * (2.0 - f);
    const double p = lat_deg * M_PI / 180.0, l = lon_deg * M_PI / 180.0;
    const double sp = std::sin(p), cp = std::cos(p);
    const double N = a / std::sqrt(1.0 - e2 * sp * sp);
    return {(N + h) * cp * std::cos(l), (N + h) * cp * std::sin(l), (N * (1.0 - e2) + h) * sp};
}

// A local east-north-up metre frame about `origin`. Right-handed, north
// positive. Not valid across the antimeridian or over ~100 km.
inline std::vector<Vec3> enuFromGeodetic(const std::vector<Geodetic>& g,
                                         const Geodetic& origin) {
    std::vector<Vec3> out;
    if (g.empty()) return out;
    const double lat0 = origin.lat_deg, lon0 = origin.lon_deg, h0 = origin.alt_m;
    const Vec3 o = ecefFromGeodetic(lat0, lon0, h0);
    const double p = lat0 * M_PI / 180.0, l = lon0 * M_PI / 180.0;
    const double sp = std::sin(p), cp = std::cos(p), sl = std::sin(l), cl = std::cos(l);
    out.reserve(g.size());
    for (const Geodetic& q : g) {
        const Vec3 d = ecefFromGeodetic(q.lat_deg, q.lon_deg, q.alt_m) - o;
        out.push_back({-sl * d.x + cl * d.y,
                       -sp * cl * d.x - sp * sl * d.y + cp * d.z,
                       cp * cl * d.x + cp * sl * d.y + sp * d.z});
    }
    return out;
}

// The same, about the mean of the fixes. The rotation depends on where the
// origin is, so two origins do not differ by a translation alone.
inline std::vector<Vec3> enuFromGeodetic(const std::vector<Geodetic>& g) {
    if (g.empty()) return {};
    Geodetic o;
    for (const Geodetic& q : g) {
        o.lat_deg += q.lat_deg;
        o.lon_deg += q.lon_deg;
        o.alt_m += q.alt_m;
    }
    const double inv = 1.0 / (double)g.size();
    o.lat_deg *= inv;
    o.lon_deg *= inv;
    o.alt_m *= inv;
    return enuFromGeodetic(g, o);
}


// What reading GPS off the images found.
struct MetricGpsCounts {
    int matched = 0;
    int no_gps = 0;
    int no_alt = 0;   // positioned, but with no altitude: treated as sea level
};

// Reference positions from each registered image's own EXIF, in a local ENU
// metre frame. Altitude is optional; a fix without one is still a fix.
inline MetricGpsCounts metricRefFromGps(const Reconstruction& rec, const std::string& image_dir,
                                        MetricRef& ref) {
    MetricGpsCounts c;
    std::vector<Geodetic> g;
    std::vector<Vec3> centres;
    std::vector<uint32_t> ids;
    for (const auto& kv : rec.images) {
        if (!kv.second.registered) continue;
        const ExifData e =
            readExif((std::filesystem::path(image_dir) / kv.second.name).string());
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
    std::vector<Vec3> enu = enuFromGeodetic(g);
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
