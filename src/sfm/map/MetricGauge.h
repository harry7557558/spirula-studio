// The metric gauge: a Sim(3) from the model's camera centres onto reference
// positions in metres, with the uncertainty that says whether to trust it.
//
// Where Orient.h fixes the gauge from the poses alone -- upright, centred,
// unit-sized -- this fixes it from an outside measurement, so the written
// model is in metres and every consumer inherits that for free (D74).
//
// The reported uncertainty assumes independent isotropic noise on the
// reference positions. Correlated error (GPS drift, SfM drift) is not in it,
// so it is a LOWER BOUND on the real uncertainty, not an estimate of it.
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "sfm/core/Pose.h"
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

enum class MetricFail { None, Pairs, Spread, Inliers, Scale, Rotation };

struct MetricFit {
    bool ok = false;
    MetricFail reason = MetricFail::Pairs;
    Sim3 T;
    int n = 0;                  // paired cameras offered
    int inliers = 0;
    double max_error = 0;       // metres
    double rms = 0;             // metres, over the inliers
    double scale_unc = 0;       // per cent, std(ds/s)
    double rot_unc_deg = 0;     // worst principal axis
    double spread = 0;          // metres, RMS radius of the reference positions
    std::vector<char> inlier_mask;
};

inline constexpr double kMetricMaxScaleUncPct = 2.0;
inline constexpr double kMetricMaxRotUncDeg = 5.0;

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
    double worst = 0;
    for (int k = 0; k < 3; k++) {
        const double perp = tr - lam[k];
        worst = std::max(worst, perp > 0.0 ? sigma / std::sqrt((double)m * perp)
                                           : std::numeric_limits<double>::infinity());
    }
    out.rot_unc_deg = worst * 180.0 / M_PI;

    if (!(out.scale_unc <= kMetricMaxScaleUncPct)) {
        out.reason = MetricFail::Scale;
        return out;
    }
    if (!(out.rot_unc_deg <= kMetricMaxRotUncDeg)) {
        out.reason = MetricFail::Rotation;
        return out;
    }
    out.ok = true;
    out.reason = MetricFail::None;
    return out;
}

}  // namespace sfm
