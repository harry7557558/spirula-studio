// Host mirror of the bundle-adjustment device math -- the camera models of
// sfm/shaders/common/camera.slang and the losses of loss.slang -- written once
// over a scalar template, so the residual evaluates in `double` and the
// Jacobian in a dual number. Forward mode rather than the device's reverse
// mode because the widths are small (3 + intrinsics) and it needs no generated
// code; the derivative is the same up to rounding.
#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace bacpu {

using std::atan2;
using std::cos;
using std::exp;
using std::log;
using std::sin;
using std::sqrt;

// ================
// Forward-mode dual number
// ================

template <int N>
struct Jet {
    double a;
    double d[N];

    Jet() = default;
    Jet(double x) : a(x) {
        for (int i = 0; i < N; i++) d[i] = 0.0;
    }
    static Jet var(double x, int k) {
        Jet j(x);
        j.d[k] = 1.0;
        return j;
    }
};

template <int N> inline Jet<N> operator+(const Jet<N>& x, const Jet<N>& y) {
    Jet<N> r;
    r.a = x.a + y.a;
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] + y.d[i];
    return r;
}
template <int N> inline Jet<N> operator-(const Jet<N>& x, const Jet<N>& y) {
    Jet<N> r;
    r.a = x.a - y.a;
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] - y.d[i];
    return r;
}
template <int N> inline Jet<N> operator-(const Jet<N>& x) {
    Jet<N> r;
    r.a = -x.a;
    for (int i = 0; i < N; i++) r.d[i] = -x.d[i];
    return r;
}
template <int N> inline Jet<N> operator*(const Jet<N>& x, const Jet<N>& y) {
    Jet<N> r;
    r.a = x.a * y.a;
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * y.a + x.a * y.d[i];
    return r;
}
template <int N> inline Jet<N> operator/(const Jet<N>& x, const Jet<N>& y) {
    Jet<N> r;
    const double inv = 1.0 / y.a;
    r.a = x.a * inv;
    for (int i = 0; i < N; i++) r.d[i] = (x.d[i] - r.a * y.d[i]) * inv;
    return r;
}
template <int N> inline Jet<N> operator*(const Jet<N>& x, double s) {
    Jet<N> r;
    r.a = x.a * s;
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * s;
    return r;
}
template <int N> inline Jet<N> operator*(double s, const Jet<N>& x) { return x * s; }
template <int N> inline Jet<N> operator+(const Jet<N>& x, double s) {
    Jet<N> r = x;
    r.a += s;
    return r;
}
template <int N> inline Jet<N> operator+(double s, const Jet<N>& x) { return x + s; }
template <int N> inline Jet<N> operator-(const Jet<N>& x, double s) { return x + (-s); }
template <int N> inline Jet<N> operator-(double s, const Jet<N>& x) { return (-x) + s; }
template <int N> inline Jet<N> operator/(const Jet<N>& x, double s) { return x * (1.0 / s); }

template <int N> inline Jet<N> sqrt(const Jet<N>& x) {
    Jet<N> r;
    r.a = std::sqrt(x.a);
    const double k = 0.5 / r.a;
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * k;
    return r;
}
template <int N> inline Jet<N> exp(const Jet<N>& x) {
    Jet<N> r;
    r.a = std::exp(x.a);
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * r.a;
    return r;
}
template <int N> inline Jet<N> log(const Jet<N>& x) {
    Jet<N> r;
    r.a = std::log(x.a);
    const double k = 1.0 / x.a;
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * k;
    return r;
}
template <int N> inline Jet<N> sin(const Jet<N>& x) {
    Jet<N> r;
    r.a = std::sin(x.a);
    const double k = std::cos(x.a);
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * k;
    return r;
}
template <int N> inline Jet<N> cos(const Jet<N>& x) {
    Jet<N> r;
    r.a = std::cos(x.a);
    const double k = -std::sin(x.a);
    for (int i = 0; i < N; i++) r.d[i] = x.d[i] * k;
    return r;
}
// Matches rAtan2's custom derivative in common/real.slang, which is the exact
// one rather than a differentiation of its Newton loop.
template <int N> inline Jet<N> atan2(const Jet<N>& y, const Jet<N>& x) {
    Jet<N> r;
    r.a = std::atan2(y.a, x.a);
    const double inv = 1.0 / (y.a * y.a + x.a * x.a);
    const double ky = x.a * inv, kx = -y.a * inv;
    for (int i = 0; i < N; i++) r.d[i] = y.d[i] * ky + x.d[i] * kx;
    return r;
}

// ================
// Camera models -- parameter order is packIntrinsics' (sfm/core/Camera.h)
// ================

struct SnavelyModel {
    static constexpr int kNumIntr = 3;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& f_log = c[0];
        const T& k1 = c[1];
        const T& k2 = c[2];
        T xp = -p[0] / p[2];
        T yp = -p[1] / p[2];
        T r2 = xp * xp + yp * yp;
        T distortion = T(1.0) + r2 * (k1 + r2 * k2);
        T f = exp(f_log) * distortion;
        out[0] = f * xp;
        out[1] = f * yp;
    }
};

struct SnavelyFModel {
    static constexpr int kNumIntr = 3;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& f = c[0];
        const T& k1 = c[1];
        const T& k2 = c[2];
        T xp = -p[0] / p[2];
        T yp = -p[1] / p[2];
        T r2 = xp * xp + yp * yp;
        T fd = f * (T(1.0) + r2 * (k1 + r2 * k2));
        out[0] = fd * xp;
        out[1] = fd * yp;
    }
};

struct PinholeRadialModel {
    static constexpr int kNumIntr = 5;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& f = c[0];
        const T& k1 = c[1];
        const T& k2 = c[2];
        const T& cx = c[3];
        const T& cy = c[4];
        T xp = p[0] / p[2];
        T yp = p[1] / p[2];
        T r2 = xp * xp + yp * yp;
        T fd = f * (T(1.0) + r2 * (k1 + r2 * k2));
        out[0] = fd * xp + cx;
        out[1] = fd * yp + cy;
    }
};

struct SimplePinholeModel {
    static constexpr int kNumIntr = 3;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        out[0] = c[0] * (p[0] / p[2]) + c[1];
        out[1] = c[0] * (p[1] / p[2]) + c[2];
    }
};

struct PinholeModel {
    static constexpr int kNumIntr = 4;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        out[0] = c[0] * (p[0] / p[2]) + c[2];
        out[1] = c[1] * (p[1] / p[2]) + c[3];
    }
};

struct OpenCVModel {
    static constexpr int kNumIntr = 8;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& fx = c[0];
        const T& fy = c[1];
        const T& k1 = c[2];
        const T& k2 = c[3];
        const T& p1 = c[4];
        const T& p2 = c[5];
        const T& cx = c[6];
        const T& cy = c[7];
        T xp = p[0] / p[2];
        T yp = p[1] / p[2];
        T r2 = xp * xp + yp * yp;
        T radial = T(1.0) + r2 * (k1 + r2 * k2);
        T dx = xp * radial + 2.0 * p1 * xp * yp + p2 * (r2 + 2.0 * xp * xp);
        T dy = yp * radial + p1 * (r2 + 2.0 * yp * yp) + 2.0 * p2 * xp * yp;
        out[0] = fx * dx + cx;
        out[1] = fy * dy + cy;
    }
};

struct FisheyeModel {
    static constexpr int kNumIntr = 8;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& fx = c[0];
        const T& fy = c[1];
        const T& k1 = c[2];
        const T& k2 = c[3];
        const T& k3 = c[4];
        const T& k4 = c[5];
        const T& cx = c[6];
        const T& cy = c[7];
        T r = sqrt(p[0] * p[0] + p[1] * p[1] + T(1e-24));
        T theta = atan2(r, p[2]);
        T t2 = theta * theta;
        T thd = theta * (T(1.0) + t2 * (k1 + t2 * (k2 + t2 * (k3 + t2 * k4))));
        T scale = thd / r;
        out[0] = fx * scale * p[0] + cx;
        out[1] = fy * scale * p[1] + cy;
    }
};

struct FullOpenCVModel {
    static constexpr int kNumIntr = 12;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& fx = c[0];
        const T& fy = c[1];
        const T& k1 = c[2];
        const T& k2 = c[3];
        const T& p1 = c[4];
        const T& p2 = c[5];
        const T& k3 = c[6];
        const T& k4 = c[7];
        const T& k5 = c[8];
        const T& k6 = c[9];
        const T& cx = c[10];
        const T& cy = c[11];
        T xp = p[0] / p[2];
        T yp = p[1] / p[2];
        T r2 = xp * xp + yp * yp;
        T radial = (T(1.0) + r2 * (k1 + r2 * (k2 + r2 * k3))) /
                   (T(1.0) + r2 * (k4 + r2 * (k5 + r2 * k6)));
        T dx = xp * radial + 2.0 * p1 * xp * yp + p2 * (r2 + 2.0 * xp * xp);
        T dy = yp * radial + p1 * (r2 + 2.0 * yp * yp) + 2.0 * p2 * xp * yp;
        out[0] = fx * dx + cx;
        out[1] = fy * dy + cy;
    }
};

struct ThinPrismFisheyeModel {
    static constexpr int kNumIntr = 12;
    static constexpr bool kPeriodicX = false;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& fx = c[0];
        const T& fy = c[1];
        const T& k1 = c[2];
        const T& k2 = c[3];
        const T& p1 = c[4];
        const T& p2 = c[5];
        const T& k3 = c[6];
        const T& k4 = c[7];
        const T& sx1 = c[8];
        const T& sy1 = c[9];
        const T& cx = c[10];
        const T& cy = c[11];
        T r = sqrt(p[0] * p[0] + p[1] * p[1] + T(1e-24));
        T theta = atan2(r, p[2]);
        T s = theta / r;
        T uf = s * p[0];
        T vf = s * p[1];
        T rf2 = theta * theta;
        T radial = rf2 * (k1 + rf2 * (k2 + rf2 * (k3 + rf2 * k4)));
        T du = uf * radial + 2.0 * p1 * uf * vf + p2 * (rf2 + 2.0 * uf * uf) + sx1 * rf2;
        T dv = vf * radial + p1 * (rf2 + 2.0 * vf * vf) + 2.0 * p2 * uf * vf + sy1 * rf2;
        out[0] = fx * (uf + du) + cx;
        out[1] = fy * (vf + dv) + cy;
    }
};

struct EquirectModel {
    static constexpr int kNumIntr = 2;
    static constexpr bool kPeriodicX = true;
    template <class T> static void project(const T* c, const T p[3], T out[2]) {
        const T& w = c[0];
        const T& h = c[1];
        T horiz = sqrt(p[0] * p[0] + p[2] * p[2] + T(1e-24));
        T theta = atan2(p[0], p[2]);
        T phi = atan2(-p[1], horiz);
        out[0] = (theta * 0.15915494309189535 + 0.5) * w;
        out[1] = (T(0.5) - phi * 0.3183098861837907) * h;
    }
};

// Dispatch on the kModels index in sfm/ba/Problem.h.
template <class Fn> inline void withModel(uint32_t model, Fn&& fn) {
    switch (model) {
        case 0: fn(SnavelyModel{}); return;
        case 1: fn(SnavelyFModel{}); return;
        case 2: fn(PinholeRadialModel{}); return;
        case 3: fn(OpenCVModel{}); return;
        case 4: fn(SimplePinholeModel{}); return;
        case 5: fn(PinholeModel{}); return;
        case 6: fn(FisheyeModel{}); return;
        case 7: fn(FullOpenCVModel{}); return;
        case 8: fn(ThinPrismFisheyeModel{}); return;
        case 9: fn(EquirectModel{}); return;
    }
    throw std::runtime_error("camera model index outside the registry");
}

// ================
// Robust losses (s is the squared residual norm)
// ================

struct TrivialLoss {
    static double cost(double s, double) { return s; }
    static double weight(double, double) { return 1.0; }
};
struct HuberLoss {
    static double cost(double s, double p) {
        double d2 = p * p;
        return s <= d2 ? s : 2.0 * p * std::sqrt(s) - d2;
    }
    static double weight(double s, double p) {
        double d2 = p * p;
        return s <= d2 ? 1.0 : p / std::sqrt(s);
    }
};
struct CauchyLoss {
    static double cost(double s, double p) {
        double c2 = p * p;
        return c2 * std::log(1.0 + s / c2);
    }
    static double weight(double s, double p) {
        double c2 = p * p;
        return 1.0 / (1.0 + s / c2);
    }
};

template <class Fn> inline void withLoss(const std::string& loss, Fn&& fn) {
    if (loss == "huber") fn(HuberLoss{});
    else if (loss == "cauchy") fn(CauchyLoss{});
    else fn(TrivialLoss{});
}

// ================
// Residual and Jacobian
// ================

// out = R(aa) p, matching camera.slang down to its 1e-15 guard on the norm.
template <class T> inline void angleAxisRotate(const T axis[3], const T p[3], T out[3]) {
    T theta = sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    T inv = T(1.0) / (theta + T(1e-15));
    T a[3] = {axis[0] * inv, axis[1] * inv, axis[2] * inv};
    T st = sin(theta), ct = cos(theta);
    T dp = a[0] * p[0] + a[1] * p[1] + a[2] * p[2];
    T cr[3] = {a[1] * p[2] - a[2] * p[1], a[2] * p[0] - a[0] * p[2], a[0] * p[1] - a[1] * p[0]};
    T w = dp * (T(1.0) - ct);
    for (int i = 0; i < 3; i++) out[i] = p[i] * ct + cr[i] * st + a[i] * w;
}

template <class M>
inline void residualAt(const double* intr, const double p[3], const double obs[2], double r[2]) {
    double px[2];
    M::template project<double>(intr, p, px);
    r[0] = px[0] - obs[0];
    if constexpr (M::kPeriodicX) {
        const double half = 0.5 * intr[0];
        if (r[0] > half) r[0] -= intr[0];
        else if (r[0] < -half) r[0] += intr[0];
    }
    r[1] = px[1] - obs[1];
}

template <class M>
inline void residual(const double pose[6], const double* intr, const double X[3],
                     const double obs[2], double r[2]) {
    double p[3];
    angleAxisRotate<double>(pose, X, p);
    for (int i = 0; i < 3; i++) p[i] += pose[3 + i];
    residualAt<M>(intr, p, obs, r);
}

// Through a rig: `pose` is the frame's rig_from_world, `ext` the member's
// cam_from_rig (camera.slang reprojectRig).
template <class M>
inline void residualRig(const double pose[6], const double ext[6], const double* intr,
                        const double X[3], const double obs[2], double r[2]) {
    double q[3], p[3];
    angleAxisRotate<double>(pose, X, q);
    for (int i = 0; i < 3; i++) q[i] += pose[3 + i];
    angleAxisRotate<double>(ext, q, p);
    for (int i = 0; i < 3; i++) p[i] += ext[3 + i];
    residualAt<M>(intr, p, obs, r);
}

// ct I + st [a]_x + (1-ct) a a^T: what angleAxisRotate applies, and its
// derivative in the point.
inline void angleAxisMatrix(const double axis[3], double R[9]) {
    const double th = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    const double inv = 1.0 / (th + 1e-15);
    const double a0 = axis[0] * inv, a1 = axis[1] * inv, a2 = axis[2] * inv;
    const double st = std::sin(th), ct = std::cos(th), w = 1.0 - ct;
    R[0] = ct + w * a0 * a0;
    R[1] = w * a0 * a1 - st * a2;
    R[2] = w * a0 * a2 + st * a1;
    R[3] = w * a1 * a0 + st * a2;
    R[4] = ct + w * a1 * a1;
    R[5] = w * a1 * a2 - st * a0;
    R[6] = w * a2 * a0 - st * a1;
    R[7] = w * a2 * a1 + st * a0;
    R[8] = ct + w * a2 * a2;
}

// d(R(aa) p)/d aa as a 3x3 (row-major), by a dual pass over the axis alone:
// the norm has no derivative at a zero rotation (every seed pair's first image)
// and this keeps that 0/0 in the axis columns, where isfinite guards drop it.
inline void angleAxisJacobian(const double axis[3], const double p[3], double J[9], double out[3]) {
    Jet<3> aa[3], pj[3], oj[3];
    for (int i = 0; i < 3; i++) aa[i] = Jet<3>::var(axis[i], i);
    for (int i = 0; i < 3; i++) pj[i] = Jet<3>(p[i]);
    angleAxisRotate<Jet<3>>(aa, pj, oj);
    for (int k = 0; k < 3; k++) {
        out[k] = oj[k].a;
        for (int j = 0; j < 3; j++) J[3 * k + j] = oj[k].d[j];
    }
}

// The projection's derivative in the camera-frame point and the intrinsics:
// dp[row][k] = dr/dp_cam[k], di[row][i] = dr/dintr[i].
template <class M>
inline void projectJacobian(const double* intr, const double p[3], const double obs[2],
                            double r[2], double dp[2][3], double* di) {
    constexpr int NI = M::kNumIntr;
    constexpr int NP = 3 + NI;
    Jet<NP> pc[3], ic[NI], px[2];
    for (int i = 0; i < 3; i++) pc[i] = Jet<NP>::var(p[i], i);
    for (int i = 0; i < NI; i++) ic[i] = Jet<NP>::var(intr[i], 3 + i);
    M::template project<Jet<NP>>(ic, pc, px);
    Jet<NP> rr[2] = {px[0] - obs[0], px[1] - obs[1]};
    if constexpr (M::kPeriodicX) {
        const double half = 0.5 * intr[0];
        if (rr[0].a > half) rr[0] = rr[0] - ic[0];
        else if (rr[0].a < -half) rr[0] = rr[0] + ic[0];
    }
    for (int row = 0; row < 2; row++) {
        r[row] = rr[row].a;
        for (int k = 0; k < 3; k++) dp[row][k] = rr[row].d[k];
        for (int i = 0; i < NI; i++) di[row * NI + i] = rr[row].d[3 + i];
    }
}

// Jc is [row][6 pose | kNumIntr intrinsics], Jp is [row][3].
template <class M>
inline void jacobian(const double pose[6], const double* intr, const double X[3],
                     const double obs[2], double r[2], double* Jc, double* Jp) {
    constexpr int NI = M::kNumIntr;
    constexpr int DOF = 6 + NI;
    double Ja[9], R[9], p[3];
    angleAxisJacobian(pose, X, Ja, p);
    angleAxisMatrix(pose, R);
    for (int i = 0; i < 3; i++) p[i] += pose[3 + i];
    double dp[2][3], di[2 * NI];
    projectJacobian<M>(intr, p, obs, r, dp, di);
    for (int row = 0; row < 2; row++) {
        double* jc = Jc + row * DOF;
        double* jp = Jp + row * 3;
        for (int j = 0; j < 3; j++) {
            double da = 0, dx = 0;
            for (int k = 0; k < 3; k++) {
                da += dp[row][k] * Ja[3 * k + j];
                dx += dp[row][k] * R[3 * k + j];
            }
            jc[j] = da;
            jc[3 + j] = dp[row][j];
            jp[j] = dx;
        }
        for (int i = 0; i < NI; i++) jc[6 + i] = di[row * NI + i];
    }
}

// Jc is [row][6 frame | 6 extrinsic | kNumIntr intrinsics]: the frame block is
// chained through the extrinsic rotation, the point block through both.
template <class M>
inline void jacobianRig(const double pose[6], const double ext[6], const double* intr,
                        const double X[3], const double obs[2], double r[2], double* Jc,
                        double* Jp) {
    constexpr int NI = M::kNumIntr;
    constexpr int DOF = 12 + NI;
    double Jf[9], Rf[9], q[3];
    angleAxisJacobian(pose, X, Jf, q);
    angleAxisMatrix(pose, Rf);
    for (int i = 0; i < 3; i++) q[i] += pose[3 + i];
    double Je[9], Re[9], p[3];
    angleAxisJacobian(ext, q, Je, p);
    angleAxisMatrix(ext, Re);
    for (int i = 0; i < 3; i++) p[i] += ext[3 + i];
    double dp[2][3], di[2 * NI];
    projectJacobian<M>(intr, p, obs, r, dp, di);
    // dp_cam / d q = Re; dq / d frame_aa = Jf, dq / d frame_t = I, dq / dX = Rf
    double ReJf[9], ReRf[9];
    for (int k = 0; k < 3; k++)
        for (int j = 0; j < 3; j++) {
            double a = 0, b = 0;
            for (int m = 0; m < 3; m++) {
                a += Re[3 * k + m] * Jf[3 * m + j];
                b += Re[3 * k + m] * Rf[3 * m + j];
            }
            ReJf[3 * k + j] = a;
            ReRf[3 * k + j] = b;
        }
    for (int row = 0; row < 2; row++) {
        double* jc = Jc + row * DOF;
        double* jp = Jp + row * 3;
        for (int j = 0; j < 3; j++) {
            double fa = 0, ft = 0, ea = 0, dx = 0;
            for (int k = 0; k < 3; k++) {
                fa += dp[row][k] * ReJf[3 * k + j];
                ft += dp[row][k] * Re[3 * k + j];
                ea += dp[row][k] * Je[3 * k + j];
                dx += dp[row][k] * ReRf[3 * k + j];
            }
            jc[j] = fa;
            jc[3 + j] = ft;
            jc[6 + j] = ea;
            jc[9 + j] = dp[row][j];
            jp[j] = dx;
        }
        for (int i = 0; i < NI; i++) jc[12 + i] = di[row * NI + i];
    }
}

}  // namespace bacpu
