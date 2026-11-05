// ResourceForecast.cpp -- see ResourceForecast.h.

#include "app/ResourceForecast.h"

#include <algorithm>
#include <cmath>

namespace spirula {

namespace {

// Sigma per e-fold of work still to extrapolate over, as a share of the growth
// projected. The trainer's is 0.3 (docs/notes/train-forecast.md); a model whose
// solves grow faster than its image count gets more.
constexpr double kReach = 0.5;
constexpr double kFloor = 0.05;    // of the high-water: allocator and driver noise
// Before a tenth of the work there is no slope worth fitting: the rest of the
// job may cost as much again as what it holds now.
constexpr double kEarlyWork = 0.1;

}  // namespace

void assess_oom(ResourceForecast& f) {
    if (!f.projected || f.limit <= 0.0) {
        f.p_oom = 0.0;
        f.risk = OomRisk::Unknown;
        return;
    }
    const double others_sd = f.commit_limited ? 0.0 : f.others_sigma;
    const double sd = std::sqrt(f.peak_sigma * f.peak_sigma + others_sd * others_sd);
    f.p_oom = sd > 0.0 ? 0.5 * std::erfc((f.limit - f.peak_mean) / (sd * std::sqrt(2.0)))
                       : (f.peak_mean > f.limit ? 1.0 : 0.0);
    f.risk = f.p_oom < 0.1 ? OomRisk::Low : f.p_oom < 0.5 ? OomRisk::Medium : OomRisk::High;
}

void GrowthForecast::add(double work, double bytes) {
    if (!(bytes > 0.0)) return;
    work = std::clamp(work, 0.0, 1.0);
    if (bytes < _hw) {
        const double d = _hw - bytes;
        _jitter_sq += 0.05 * (d * d - _jitter_sq);
    }
    _hw = std::max(_hw, bytes);
    if (_pts.empty() || work > _w) {
        _pts.push_back({work, _hw});
        if (_pts.size() > 512) {
            std::vector<Point> half;
            for (size_t i = 0; i < _pts.size(); i += 2) half.push_back(_pts[i]);
            _pts.swap(half);
        }
    } else {
        _pts.back().hw = _hw;
    }
    _w = std::max(_w, work);
}

void GrowthForecast::peak(double& mean, double& sigma) const {
    const double left = 1.0 - _w;
    const double jitter = std::sqrt(std::max(0.0, _jitter_sq));
    if (_pts.size() < 3 || _w < kEarlyWork) {
        mean = _hw;
        sigma = std::sqrt(_hw * left * _hw * left + jitter * jitter);
        return;
    }
    // hw = a + b w over the last three quarters of the work so far: the job's present slope.
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (const Point& p : _pts) {
        if (p.w < 0.25 * _w) continue;
        n += 1; sx += p.w; sy += p.hw; sxx += p.w * p.w; sxy += p.w * p.hw;
    }
    double b = 0.0, var_b = 0.0;
    const double det = n * sxx - sx * sx;
    if (n >= 3 && det > 0.0) {
        b = std::max(0.0, (n * sxy - sx * sy) / det);
        const double a = (sy - b * sx) / n;
        double rss = 0;
        for (const Point& p : _pts)
            if (p.w >= 0.25 * _w) rss += (p.hw - a - b * p.w) * (p.hw - a - b * p.w);
        var_b = n > 2 ? rss / (n - 2) * n / det : 0.0;
    }
    const double growth = b * left;
    const double reach = kReach * growth * std::log(1.0 / std::max(_w, 1e-3));
    mean = _hw + growth;
    sigma = std::sqrt(var_b * left * left + reach * reach + jitter * jitter +
                      (kFloor * _hw) * (kFloor * _hw));
}

}  // namespace spirula
