// ProgressClock.cpp -- see ProgressClock.h.

#include "app/gui/ProgressClock.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace gui {

namespace {

// Recent enough to follow a change of pace (cached pairs, then inference), long
// enough to ride out one slow item.
constexpr double kWindow = 90.0;
constexpr size_t kMaxSamples = 512;
// Cost per unit of work may rise as the job goes (a model that gets dearer to
// solve as it grows) or fall (a fixed start-up cost); the fitted exponent stays inside these.
constexpr double kMinExponent = 0.5, kMaxExponent = 3.0;
// A stall counts down to this share of the last estimate and holds there rather than reading as done.
constexpr double kStallFloor = 0.2;

}  // namespace

double progress_now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void ProgressClock::start(double now) {
    *this = ProgressClock{};
    _t0 = now;
}

void ProgressClock::work(double now, double w) { work(now, w, -1.0); }

void ProgressClock::work(double now, double w, double remaining_s) {
    if (!started()) start(now);
    if (finished()) return;
    w = std::clamp(std::isfinite(w) ? w : 0.0, 0.0, 1.0);
    if (w < _w && _w > 0.0) {
        const double scale = w / _w;
        for (Sample& s : _s) s.w *= scale;
    } else if (w == _w && !_s.empty()) {
        // No progress, so the bar holds; a fresh estimate of the time left still counts.
        if (remaining_s >= 0.0) {
            _rem = remaining_s;
            _rem_at = now;
        }
        return;
    }
    _w = w;
    _s.push_back({now, w});
    if (_s.size() > kMaxSamples) {
        std::vector<Sample> thin;
        thin.reserve(kMaxSamples);
        for (size_t i = 0; i < _s.size(); ++i)
            if (i >= kMaxSamples / 2 || i % 2 == 0) thin.push_back(_s[i]);
        _s.swap(thin);
    }
    estimate(now, w, remaining_s);
}

void ProgressClock::finish(double now) {
    if (!started()) start(now);
    if (finished()) return;
    _t_end = now;
    _frac = 1.0f;
    _rem = 0.0;
    _rem_at = now;
}

void ProgressClock::estimate(double now, double w, double external) {
    const double T = now - _t0;
    double rem = external;
    if (rem < 0.0) {
        if (w <= 0.0 || T < 1.0 || _s.size() < 2) return;
        // t = T_total * w^p over the recent part of the run, as a line in log-log.
        double p = 1.0;
        {
            double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0, w_min = w;
            for (const Sample& s : _s) {
                const double dt = s.t - _t0;
                if (s.w < 0.25 * w || s.w <= 0.0 || dt <= 0.05) continue;
                const double x = std::log(s.w), y = std::log(dt);
                n += 1; sx += x; sy += y; sxx += x * x; sxy += x * y;
                w_min = std::min(w_min, s.w);
            }
            const double det = n * sxx - sx * sx;
            if (n >= 4 && w >= 1.5 * w_min && det > 0.0)
                p = std::clamp((n * sxy - sx * sy) / det, kMinExponent, kMaxExponent);
        }
        rem = T * (std::pow(w, -p) - 1.0);
        // The current pace, which the fit lags: a phase that slowed down or a start-up cost.
        size_t j = 0;
        while (j + 1 < _s.size() && _s[j + 1].t < now - kWindow) ++j;
        const double dt = _s.back().t - _s[j].t, dw = w - _s[j].w;
        if (dt >= 2.0 && dw > 0.0) rem = std::max(rem, (1.0 - w) * dt / dw);
    }
    rem = std::max(0.0, rem);
    _rem = rem;
    _rem_at = now;
    if (T + rem > 0.0)
        _frac = std::max(_frac, (float)std::min(0.999, T / (T + rem)));
}

double ProgressClock::elapsed(double now) const {
    if (!started()) return 0.0;
    return std::max(0.0, (finished() ? _t_end : now) - _t0);
}

double ProgressClock::remaining(double now) const {
    if (finished()) return 0.0;
    if (_rem < 0.0) return -1.0;
    const double since = std::max(0.0, now - _rem_at);
    return _rem - std::min(since, (1.0 - kStallFloor) * _rem);
}

}  // namespace gui
