#pragma once
// Time-indexed queries over one video's telemetry: the IMU rotation between
// two instants, the up direction at an instant, a pre-integration over an
// interval, a GPS position at an instant. Every query takes video time
// (seconds from the first frame) and adds `time_offset` to reach the IMU
// clock. Gyro and attitude are interchangeable rotation sources, so a DJI
// file with no raw gyro answers the same questions as an Insta360.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "sfm/core/Pose.h"
#include "sfm/core/Preintegration.h"
#include "sfm/core/Telemetry.h"

namespace sfm {

namespace timeline_detail {

inline Quat quatMul(const Quat& a, const Quat& b) {
    return {a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
            a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
            a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
            a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0]};
}
inline Quat quatConj(const Quat& q) { return {q[0], -q[1], -q[2], -q[3]}; }
inline Quat quatNormalized(Quat q) {
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n > 0) for (double& v : q) v /= n;
    return q;
}
inline Quat quatExp(const Vec3& phi) {
    const double th = phi.norm();
    if (th < 1e-12) return {1, 0.5 * phi.x, 0.5 * phi.y, 0.5 * phi.z};
    const double s = std::sin(0.5 * th) / th;
    return {std::cos(0.5 * th), phi.x * s, phi.y * s, phi.z * s};
}
inline Quat quatNlerp(const Quat& a, Quat b, double u) {
    double d = 0;
    for (int i = 0; i < 4; i++) d += a[i] * b[i];
    if (d < 0) for (double& v : b) v = -v;
    Quat q;
    for (int i = 0; i < 4; i++) q[i] = a[i] * (1 - u) + b[i] * u;
    return quatNormalized(q);
}

}  // namespace timeline_detail

struct UpVote {
    bool ok = false;
    Vec3 up;          // unit, IMU frame: the direction the specific force averages to
    double motion = 0;   // RMS deviation of the window's samples from that average, m/s^2
    int samples = 0;
};

class SensorTimeline {
public:
    double time_offset = 0;   // seconds: IMU clock = video clock + time_offset
    ImuNoise noise;

    // False with `error` when the file carries nothing this can answer with.
    bool init(const Telemetry& t, const TelemetryCheck& c, std::string& error) {
        using namespace timeline_detail;
        _gyro.clear(); _accel.clear(); _att.clear();
        _q_plus.clear(); _q_minus.clear(); _gps.clear();
        _P = mat3Identity();
        _up_declared = false;
        _gyro_rate = c.gyro.rate_hz;
        _accel_rate = c.accel.rate_hz;
        for (const TelemetryVec& v : t.gyro) _gyro.push_back({v.t, {v.x, v.y, v.z}});
        for (const TelemetryVec& v : t.accel) _accel.push_back({v.t, {v.x, v.y, v.z}});
        auto by_t = [](const Stamped& a, const Stamped& b) { return a.t < b.t; };
        std::stable_sort(_gyro.begin(), _gyro.end(), by_t);
        std::stable_sort(_accel.begin(), _accel.end(), by_t);

        if (t.orientation_axes.size() == 3) {
            _P = {0, 0, 0, 0, 0, 0, 0, 0, 0};
            for (int k = 0; k < 3; k++) {
                const int axis = std::tolower(t.orientation_axes[(size_t)k]) - 'x';
                if (axis >= 0 && axis < 3) _P[3 * k + axis] = 1;
            }
        }
        for (const TelemetryQuat& q : t.orientation) {
            Quat v = quatNormalized({q.w, q.x, q.y, q.z});
            if (!c.attitude_is_sensor_to_world) v = quatConj(v);
            _att.push_back({q.t, v});
        }
        std::stable_sort(_att.begin(), _att.end(),
                         [](const StampedQuat& a, const StampedQuat& b) { return a.t < b.t; });

        noise.accel = std::max(ImuNoise().accel, accelNoiseDensity());
        _use_gyro = _gyro.size() >= 2 && _gyro_rate >= 50;
        _use_att = !_use_gyro && _att.size() >= 2;
        if (_use_gyro) buildCumulative(+1.0, _q_plus);
        if (_use_att) {
            Vec3 u{0, 0, 0};
            for (const Stamped& a : _accel) {
                Quat q;
                if (!attitudeAt(a.t, q)) continue;
                u = u + mul(quaternionToRotation(q), mul(_P, a.v));
            }
            _world_up = u.norm() > 0 ? u.normalized() : Vec3{0, 0, 1};
            const Vec3 w{t.attitude_world_up[0], t.attitude_world_up[1], t.attitude_world_up[2]};
            if (_accel.empty() && w.norm() > 0) {
                _world_up = w.normalized();
                _up_declared = true;
            }
        }

        std::vector<TelemetryGps> kept;
        telemetry_gps_filter(t, kept);
        for (const TelemetryGps& g : kept)
            if (_gps.empty() || g.lat != _gps.back().lat || g.lon != _gps.back().lon)
                _gps.push_back(g);
        _gps_usable = c.gps_usable && _gps.size() >= 5;

        if (!hasRotation() && _accel.empty() && _gps.empty()) {
            error = "no gyro, attitude, accelerometer or GPS";
            return false;
        }
        return true;
    }

    bool hasRotation() const { return _use_gyro || _use_att; }
    bool hasGyro() const { return _use_gyro; }
    bool hasUp() const { return !_accel.empty() || _up_declared; }
    // No accelerometer settled which way the attitude maps, so `sign` -1
    // conjugates it: a hypothesis map/ImuExtrinsic.h tests like the gyro's.
    bool attitudeSenseOpen() const { return _use_att && _accel.empty(); }
    // 20 Hz admits a camera writing one accelerometer reading per frame (the
    // DJI's 30 Hz); the position integral over a 0.1-1 s pair still has
    // samples to work with, and the fit's own sigma says when it does not.
    bool canPreintegrate() const {
        return hasRotation() && _accel_rate >= 20 && _accel.size() >= 2;
    }
    bool hasGps() const { return _gps_usable; }
    double imuFirst() const { return _use_gyro ? _gyro.front().t : _use_att ? _att.front().t : 0; }
    double imuLast() const { return _use_gyro ? _gyro.back().t : _use_att ? _att.back().t : 0; }
    bool coversImu(double t) const {
        const double ti = t + time_offset;
        return hasRotation() && ti >= imuFirst() && ti <= imuLast();
    }
    const std::vector<TelemetryGps>& gpsFixes() const { return _gps; }

    // R_i(t0) <- i(t1): the rotation taking IMU-frame vectors at video time
    // t1 into the IMU frame at t0. `sign` -1 integrates the gyro negated,
    // the left-handed-axes hypothesis, or conjugates an open-sense attitude.
    bool rotationBetween(double t0, double t1, Mat3& R, double sign = 1.0) const {
        return rotationBetweenImu(t0 + time_offset, t1 + time_offset, R, sign);
    }

    // The specific force averaged over +-half_window around t, each sample
    // rotated into the IMU frame at t first so a turning camera does not
    // smear it. A camera at rest reads +g up, so this points UP.
    UpVote upAt(double t, double half_window = 0.25, double sign = 1.0) const {
        UpVote v;
        const double ti = t + time_offset;
        if (_use_att && _accel_rate < 50) {
            Quat q;
            if (!hasUp() || !orientationAt(ti, q, sign)) return v;
            v.up = mul(transpose(_P), mul(transpose(quaternionToRotation(q)), _world_up)).normalized();
            v.ok = true;
            v.samples = 1;
            return v;
        }
        if (_accel.empty()) return v;
        auto lo = std::lower_bound(_accel.begin(), _accel.end(), ti - half_window,
                                   [](const Stamped& s, double x) { return s.t < x; });
        auto hi = std::upper_bound(_accel.begin(), _accel.end(), ti + half_window,
                                   [](double x, const Stamped& s) { return x < s.t; });
        if (hi - lo < 1) return v;
        std::vector<Vec3> xs;
        xs.reserve((size_t)(hi - lo));
        Vec3 sum{0, 0, 0};
        for (auto it = lo; it != hi; ++it) {
            Vec3 x = it->v;
            if (hasRotation()) {
                Mat3 R;
                if (!rotationBetween(t, it->t - time_offset, R, sign)) continue;
                x = mul(R, x);
            }
            xs.push_back(x);
            sum = sum + x;
        }
        if (xs.empty()) return v;
        const Vec3 mean = sum * (1.0 / (double)xs.size());
        double dev = 0;
        for (const Vec3& x : xs) dev += (x - mean).dot(x - mean);
        v.motion = std::sqrt(dev / (double)xs.size());
        v.samples = (int)xs.size();
        if (!(mean.norm() > 1.0)) return v;
        v.up = mean.normalized();
        v.ok = true;
        return v;
    }

    Preintegration preintegrate(double t0, double t1, const Vec3& bg, const Vec3& ba,
                                double sign = 1.0) const {
        Preintegration P;
        if (!canPreintegrate()) return P;
        const double a = t0 + time_offset, b = t1 + time_offset;
        if (!(b > a)) return P;
        if (!_use_gyro) return preintegrateFromAttitude(a, b, ba);
        if (a < _gyro.front().t || b > _gyro.back().t) return P;
        std::vector<ImuSample> s;
        auto push = [&](double t, const Vec3& w) {
            Vec3 acc;
            if (!interpolate(_accel, t, acc)) return;
            s.push_back({t, w * sign, acc});
        };
        Vec3 w;
        if (interpolate(_gyro, a, w)) push(a, w);
        auto it = std::upper_bound(_gyro.begin(), _gyro.end(), a,
                                   [](double x, const Stamped& g) { return x < g.t; });
        for (; it != _gyro.end() && it->t < b; ++it) push(it->t, it->v);
        if (interpolate(_gyro, b, w)) push(b, w);
        return sfm::preintegrate(s, bg, ba, noise);
    }

    // The log's position at t, interpolated between the first appearances of
    // consecutive distinct fixes. False beyond the log or across a gap over
    // `max_gap` seconds.
    bool gpsAt(double t, TelemetryGps& out, double max_gap = 10.0) const {
        if (_gps.size() < 2) return false;
        auto it = std::lower_bound(_gps.begin(), _gps.end(), t,
                                   [](const TelemetryGps& g, double x) { return g.t < x; });
        if (it == _gps.begin() || it == _gps.end()) return false;
        const TelemetryGps& b = *it;
        const TelemetryGps& a = *(it - 1);
        if (b.t - a.t > max_gap) return false;
        const double u = (t - a.t) / (b.t - a.t);
        out = a;
        out.t = t;
        out.lat = a.lat + (b.lat - a.lat) * u;
        out.lon = a.lon + (b.lon - a.lon) * u;
        out.alt = a.alt + (b.alt - a.alt) * u;
        out.has_alt = a.has_alt && b.has_alt;
        return true;
    }

private:
    struct Stamped {
        double t;
        Vec3 v;
    };
    struct StampedQuat {
        double t;
        Quat q;
    };
    std::vector<Stamped> _gyro, _accel;
    std::vector<StampedQuat> _att;
    mutable std::vector<Quat> _q_plus, _q_minus;   // cumulative gyro orientation per sample
    std::vector<TelemetryGps> _gps;
    Mat3 _P = mat3Identity();   // accel frame -> the frame the attitude acts on
    Vec3 _world_up{0, 0, 1};
    double _gyro_rate = 0, _accel_rate = 0;
    bool _use_gyro = false, _use_att = false, _gps_usable = false;
    bool _up_declared = false;

    // What the double integral cannot recover: a per-frame accelerometer
    // aliases the vibration a 1 kHz stream resolves and integrates away. The
    // second difference of a smooth signal is that content and nothing else.
    double accelNoiseDensity() const {
        if (_accel.size() < 32 || !(_accel_rate > 0)) return 0;
        std::vector<double> d;
        d.reserve(3 * (_accel.size() - 2));
        for (size_t k = 1; k + 1 < _accel.size(); k++) {
            const Vec3 v = _accel[k - 1].v - _accel[k].v * 2.0 + _accel[k + 1].v;
            d.push_back(std::fabs(v.x));
            d.push_back(std::fabs(v.y));
            d.push_back(std::fabs(v.z));
        }
        std::nth_element(d.begin(), d.begin() + (long)d.size() / 2, d.end());
        const double sigma = d[d.size() / 2] / (0.6745 * std::sqrt(6.0));
        return sigma / std::sqrt(_accel_rate);
    }

    bool rotationBetweenImu(double ta, double tb, Mat3& R, double sign) const {
        using namespace timeline_detail;
        Quat q0, q1;
        if (!orientationAt(ta, q0, sign) || !orientationAt(tb, q1, sign)) return false;
        const Mat3 Rb = quaternionToRotation(quatMul(quatConj(q0), q1));
        R = _use_att ? mul(mul(transpose(_P), Rb), _P) : Rb;
        return true;
    }

    // Both ends must be covered by the accelerometer: a shortened interval
    // would leave `dt` disagreeing with the frame spacing the caller pairs it
    // with.
    Preintegration preintegrateFromAttitude(double a, double b, const Vec3& ba) const {
        Preintegration P;
        if (a < _att.front().t || b > _att.back().t) return P;
        if (a < _accel.front().t || b > _accel.back().t) return P;
        std::vector<AttitudeSample> s;
        auto push = [&](double t) {
            AttitudeSample x;
            x.t = t;
            if (!interpolate(_accel, t, x.a) || !rotationBetweenImu(a, t, x.R, 1.0)) return;
            s.push_back(x);
        };
        push(a);
        auto it = std::upper_bound(_accel.begin(), _accel.end(), a,
                                   [](double x, const Stamped& g) { return x < g.t; });
        for (; it != _accel.end() && it->t < b; ++it) push(it->t);
        push(b);
        return preintegrateAttitude(s, ba, noise);
    }

    void buildCumulative(double sign, std::vector<Quat>& q) const {
        using namespace timeline_detail;
        q.assign(_gyro.size(), Quat{1, 0, 0, 0});
        for (size_t k = 0; k + 1 < _gyro.size(); k++) {
            const double dt = _gyro[k + 1].t - _gyro[k].t;
            const Vec3 w = (_gyro[k].v + _gyro[k + 1].v) * (0.5 * sign);
            q[k + 1] = quatNormalized(quatMul(q[k], quatExp(w * dt)));
        }
    }

    static bool interpolate(const std::vector<Stamped>& s, double t, Vec3& out) {
        if (s.size() < 2 || t < s.front().t || t > s.back().t) return false;
        auto it = std::upper_bound(s.begin(), s.end(), t,
                                   [](double x, const Stamped& g) { return x < g.t; });
        if (it == s.begin()) { out = s.front().v; return true; }
        if (it == s.end()) { out = s.back().v; return true; }
        const Stamped& b = *it;
        const Stamped& a = *(it - 1);
        const double u = b.t > a.t ? (t - a.t) / (b.t - a.t) : 0.0;
        out = a.v + (b.v - a.v) * u;
        return true;
    }

    bool attitudeAt(double ti, Quat& q) const {
        using namespace timeline_detail;
        if (_att.size() < 2 || ti < _att.front().t || ti > _att.back().t) return false;
        auto it = std::upper_bound(_att.begin(), _att.end(), ti,
                                   [](double x, const StampedQuat& g) { return x < g.t; });
        if (it == _att.begin()) { q = _att.front().q; return true; }
        if (it == _att.end()) { q = _att.back().q; return true; }
        const StampedQuat& b = *it;
        const StampedQuat& a = *(it - 1);
        const double u = b.t > a.t ? (ti - a.t) / (b.t - a.t) : 0.0;
        q = quatNlerp(a.q, b.q, u);
        return true;
    }

    // Orientation of the IMU frame at IMU time `ti` relative to the table's
    // origin, R_origin <- i(ti), as a quaternion.
    bool orientationAt(double ti, Quat& q, double sign) const {
        using namespace timeline_detail;
        if (_use_att) {
            if (!attitudeAt(ti, q)) return false;
            if (sign < 0) q = quatConj(q);
            return true;
        }
        if (!_use_gyro || ti < _gyro.front().t || ti > _gyro.back().t) return false;
        std::vector<Quat>& tab = sign < 0 ? _q_minus : _q_plus;
        if (tab.empty()) buildCumulative(sign, tab);
        auto it = std::upper_bound(_gyro.begin(), _gyro.end(), ti,
                                   [](double x, const Stamped& g) { return x < g.t; });
        size_t k = it == _gyro.begin() ? 0 : (size_t)(it - _gyro.begin()) - 1;
        if (k + 1 >= _gyro.size()) k = _gyro.size() - 2;
        const double dt = ti - _gyro[k].t;
        q = quatNormalized(quatMul(tab[k], quatExp(_gyro[k].v * (dt * sign))));
        return true;
    }
};

}  // namespace sfm
