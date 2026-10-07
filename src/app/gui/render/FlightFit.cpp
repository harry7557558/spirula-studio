// FlightFit.cpp -- see FlightFit.h.

#include "app/gui/render/FlightFit.h"

#include "app/gui/render/Trajectory.h"

#include <cstddef>
#include <algorithm>
#include <cmath>

namespace gui::render {

namespace {

constexpr double kRate = 30.0;          // samples a second, after resampling

double angle_between(const double a[4], const double b[4]) {
    const double d = std::fabs(a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3]);
    return 2.0 * std::acos(std::min(d, 1.0));
}

double dist(const double a[3], const double b[3]) {
    return std::sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]));
}

void normalize(double q[4]) {
    const double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    for (int k = 0; k < 4; k++) q[k] = n > 1e-12 ? q[k] / n : (k == 0 ? 1.0 : 0.0);
}

// The flight cleaned up: even in time, the jitter of a hand filtered out,
// and the stillness at either end -- before the first move, after the last
// -- cut off. Empty when nothing moved.
std::vector<FlightSample> prepare(const std::vector<FlightSample>& raw, double unit) {
    std::vector<FlightSample> in;
    for (const FlightSample& s : raw)
        if (in.empty() || s.t > in.back().t + 1e-6) in.push_back(s);
    if (in.size() < 2) return {};
    for (size_t i = 1; i < in.size(); i++) {
        double d = 0.0;
        for (int k = 0; k < 4; k++) d += in[i].rot[k] * in[i - 1].rot[k];
        if (d < 0.0) for (double& v : in[i].rot) v = -v;
    }
    // Still: within half a percent of the scene, or a third of a degree, of
    // where it rests -- a hand on the mouse is never quite still.
    const double still = 0.005 * unit;
    auto moved = [&](const FlightSample& a, const FlightSample& b) {
        return dist(a.pos, b.pos) + unit * angle_between(a.rot, b.rot) > still;
    };
    size_t a = 0, b = in.size() - 1;
    while (a + 1 < in.size() && !moved(in[0], in[a + 1])) a++;
    while (b > a && !moved(in.back(), in[b - 1])) b--;
    if (b <= a) return {};

    const double t0 = in[a].t, t1 = in[b].t;
    const int n = std::max(2, (int)std::ceil((t1 - t0) * kRate) + 1);
    std::vector<FlightSample> even((size_t)n);
    size_t at = a;
    for (int i = 0; i < n; i++) {
        const double t = t0 + (t1 - t0) * i / (n - 1);
        while (at + 1 < b && in[at + 1].t < t) at++;
        const FlightSample& p = in[at];
        const FlightSample& q = in[std::min(at + 1, b)];
        const double f = q.t > p.t ? std::clamp((t - p.t) / (q.t - p.t), 0.0, 1.0) : 0.0;
        FlightSample& s = even[(size_t)i];
        s.t = t - t0;
        for (int k = 0; k < 3; k++) s.pos[k] = p.pos[k] + (q.pos[k] - p.pos[k]) * f;
        for (int k = 0; k < 4; k++) s.rot[k] = p.rot[k] + (q.rot[k] - p.rot[k]) * f;
        normalize(s.rot);
    }
    // A Gaussian a twentieth of a second wide, narrowing to nothing at the
    // ends so the flight still starts and stops where it did.
    std::vector<FlightSample> out = even;
    constexpr int kHalf = 4;
    constexpr double kSigma = 1.5;
    for (int i = 1; i + 1 < n; i++) {
        const int h = std::min({kHalf, i, n - 1 - i});
        double w = 0.0, p[3] = {0, 0, 0}, q[4] = {0, 0, 0, 0};
        for (int j = -h; j <= h; j++) {
            const double g = std::exp(-0.5 * j * j / (kSigma * kSigma));
            const FlightSample& s = even[(size_t)(i + j)];
            for (int k = 0; k < 3; k++) p[k] += g * s.pos[k];
            for (int k = 0; k < 4; k++) q[k] += g * s.rot[k];
            w += g;
        }
        for (int k = 0; k < 3; k++) out[(size_t)i].pos[k] = p[k] / w;
        for (int k = 0; k < 4; k++) out[(size_t)i].rot[k] = q[k] / w;
        normalize(out[(size_t)i].rot);
    }
    return out;
}

// Each sample's time in the move: a step takes dt^timing (ds / v)^(1 - timing),
// ds the way it covers, turning included, and v the pace the flight mostly
// kept. As flown at 1, one pace at 0; a stop costs nothing below 1.
std::vector<double> retime(const std::vector<FlightSample>& s, double timing, double unit) {
    const size_t n = s.size();
    std::vector<double> ds(n, 0.0), speed;
    for (size_t i = 1; i < n; i++) {
        ds[i] = dist(s[i].pos, s[i - 1].pos) + unit * angle_between(s[i].rot, s[i - 1].rot);
        speed.push_back(ds[i] * kRate);
    }
    // The median of the moving part: what a typical stretch was flown at.
    std::vector<double> moving;
    const double top = *std::max_element(speed.begin(), speed.end());
    for (double v : speed)
        if (v > 0.05 * top) moving.push_back(v);
    double pace = 1e-12;
    if (!moving.empty()) {
        std::nth_element(moving.begin(), moving.begin() + (ptrdiff_t)(moving.size() / 2),
                         moving.end());
        pace = std::max(moving[moving.size() / 2], pace);
    }
    const double g = std::clamp(timing, 0.0, 1.0), dt = 1.0 / kRate;
    std::vector<double> u(n, 0.0);
    for (size_t i = 1; i < n; i++)
        u[i] = u[i - 1] + std::pow(dt, g) * std::pow(ds[i] / pace, 1.0 - g);
    return u;
}

}  // namespace

double flight_length(const std::vector<FlightSample>& flight, const FlightFit& fit, double unit) {
    const std::vector<FlightSample> s = prepare(flight, unit);
    if (s.size() < 2) return 0.0;
    return retime(s, fit.timing, unit).back();
}

int fit_flight(const std::vector<FlightSample>& flight, const FlightFit& fit, double unit,
               const Lens& lens, RenderProject& p) {
    const std::vector<FlightSample> s = prepare(flight, unit);
    const int n = (int)s.size();
    if (n < 2) return 0;
    std::vector<double> u = retime(s, fit.timing, unit);
    const double own = u.back();
    if (!(own > 1e-9)) return 0;
    const double length = fit.length > 0.0 ? fit.length : own;
    for (double& v : u) v *= length / own;

    // Within a share of the scene and a few degrees: loose to close.
    const double d = std::clamp(fit.detail, 0.0, 1.0);
    const double tol_pos = unit * 0.03 * std::pow(0.002 / 0.03, d);
    const double tol_ang = 4.0 * std::pow(0.3 / 4.0, d) * 3.14159265358979 / 180.0;

    RenderProject fp = p;
    fp.motion.curve = Curve::Spline;
    fp.motion.ease = false;
    fp.motion.constant_speed = false;
    fp.motion.loop = false;
    fp.end = 0.0;
    fp.shots.clear();
    auto build = [&](const std::vector<int>& at) {
        fp.keys.clear();
        for (int i : at) {
            Keyframe k;
            k.time = u[(size_t)i];
            for (int c = 0; c < 3; c++) k.pos[c] = s[(size_t)i].pos[c];
            for (int c = 0; c < 4; c++) k.rot[c] = s[(size_t)i].rot[c];
            fp.keys.push_back(k);
        }
        fp.keys[0].own_lens = true;
        fp.keys[0].lens = lens;
    };
    // Ends first; every stretch between two keys that strays further than
    // the tolerance gets a key where it strays most, until none does.
    std::vector<int> at = {0, n - 1};
    constexpr int kMaxKeys = 240;
    for (int round = 0; round < 32; round++) {
        build(at);
        const Trajectory tr(fp, false);
        std::vector<int> add;
        for (size_t k = 0; k + 1 < at.size(); k++) {
            int worst = -1;
            double worst_e = 1.0;
            for (int i = at[k] + 1; i < at[k + 1]; i++) {
                // A key needs a time of its own.
                if (u[(size_t)i] - u[(size_t)at[k]] < 1e-4 || u[(size_t)at[k + 1]] - u[(size_t)i] < 1e-4)
                    continue;
                const CameraState c = tr.at(u[(size_t)i]);
                const double e = std::max(dist(c.pos, s[(size_t)i].pos) / tol_pos,
                                          angle_between(c.rot, s[(size_t)i].rot) / tol_ang);
                if (e > worst_e) { worst_e = e; worst = i; }
            }
            if (worst >= 0) add.push_back(worst);
        }
        if (add.empty() || (int)(at.size() + add.size()) > kMaxKeys) break;
        at.insert(at.end(), add.begin(), add.end());
        std::sort(at.begin(), at.end());
    }
    build(at);
    p.keys = fp.keys;
    p.motion = fp.motion;
    p.end = 0.0;
    return (int)p.keys.size();
}

}  // namespace gui::render
