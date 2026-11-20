// The 2DGS surfel evaluation (shaders/projection_utils.slang) on the Vulkan
// backend against a host model: forward values in double, and the Slang VJPs
// against the model's exact (dual-number) gradients. Self-checking.
//
//   ./surfel_grad_test

#include "backend/api/BackendRuntime.h"
#include "backend/vulkan/VulkanPipelines.h"
#include "core/SourcePath.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", SS_FILE, __LINE__, #cond);    \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

// Must match surfel_test.slang.
constexpr int kStride = 32;
constexpr int kNumParams = 20;  // mean 3, quat 4, scale 3, opacity, rgb 3, ray_o 3, ray_d 3
constexpr int kInVAlpha = 20, kInVRgb = 21, kInVDepth = 24, kInVNormal = 25;
constexpr int kOutGrad = 8;  // the parameter gradients, in input order from here

// Forward-mode dual number: exact gradients of the host model with no step
// size, and in float, the error fp32 arithmetic alone makes.
template <typename R>
struct Dual {
    R v = 0;
    R g[kNumParams] = {};
    Dual() = default;
    Dual(double c) : v((R)c) {}
};
template <typename R>
Dual<R> var(double x, int k) { Dual<R> d(x); d.g[k] = 1; return d; }

#define SS_DUAL_BINOP(OP, DV, DG)                                             \
    template <typename R> Dual<R> operator OP(Dual<R> a, Dual<R> b) {         \
        Dual<R> r; r.v = DV;                                                  \
        for (int k = 0; k < kNumParams; ++k) r.g[k] = DG;                     \
        return r;                                                             \
    }                                                                         \
    template <typename R> Dual<R> operator OP(Dual<R> a, double c) { return a OP Dual<R>(c); } \
    template <typename R> Dual<R> operator OP(double c, Dual<R> b) { return Dual<R>(c) OP b; }
SS_DUAL_BINOP(+, a.v + b.v, a.g[k] + b.g[k])
SS_DUAL_BINOP(-, a.v - b.v, a.g[k] - b.g[k])
SS_DUAL_BINOP(*, a.v * b.v, a.g[k] * b.v + a.v * b.g[k])
SS_DUAL_BINOP(/, a.v / b.v, (a.g[k] * b.v - a.v * b.g[k]) / (b.v * b.v))
#undef SS_DUAL_BINOP
template <typename R> Dual<R> operator-(Dual<R> a) { return Dual<R>(0.0) - a; }
template <typename R> Dual<R> dexp(Dual<R> a) {
    Dual<R> r; r.v = std::exp(a.v);
    for (int k = 0; k < kNumParams; ++k) r.g[k] = r.v * a.g[k];
    return r;
}
inline double dexp(double a) { return std::exp(a); }
template <typename R> double val(Dual<R> a) { return (double)a.v; }
inline double val(double a) { return a; }

template <typename T> using V3 = std::array<T, 3>;
template <typename T> V3<T> sub(V3<T> a, V3<T> b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
template <typename T> T dot(V3<T> a, V3<T> b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

template <typename T>
struct Eval {
    T alpha, depth, loss;
    V3<T> rgb, normal;
    int branch = 0;  // grazing | normal sign, as bits
};

// The same expressions as the Slang. p holds the parameters as T (variables
// for a Dual), q the whole input record.
template <typename T>
Eval<T> eval(const T* p, const double* q) {
    V3<T> mean = {p[0], p[1], p[2]};
    T w = p[3], x = p[4], y = p[5], z = p[6];
    V3<T> col[3] = {
        V3<T>{1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)},
        V3<T>{2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)},
        V3<T>{2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)},
    };
    const T s[3] = {dexp(-p[7]), dexp(-p[8]), T(1.0)};
    V3<T> o = {p[14], p[15], p[16]}, d = {p[17], p[18], p[19]};
    V3<T> om = sub(o, mean);
    V3<T> gro, grd;
    for (int i = 0; i < 3; ++i) {
        gro[i] = s[i] * dot(col[i], om);
        grd[i] = s[i] * dot(col[i], d);
    }
    Eval<T> e{};
    T g(0.0), t_hit(0.0);
    if (val(grd[2] * grd[2]) > 1e-6 * val(dot(d, d))) {
        t_hit = -gro[2] / grd[2];
        T u = gro[0] + t_hit * grd[0], v = gro[1] + t_hit * grd[1];
        g = dexp(-0.5 * (u * u + v * v));
    } else {
        e.branch |= 1;
    }
    e.alpha = p[10] * g;
    e.depth = t_hit;
    e.rgb = {p[11], p[12], p[13]};
    const bool flip = val(dot(col[2], d)) > 0;
    e.branch |= flip ? 2 : 0;
    for (int i = 0; i < 3; ++i) e.normal[i] = flip ? -col[2][i] : col[2][i];
    e.loss = q[kInVAlpha] * e.alpha + q[kInVDepth] * e.depth;
    for (int i = 0; i < 3; ++i)
        e.loss = e.loss + q[kInVRgb + i] * e.rgb[i] + q[kInVNormal + i] * e.normal[i];
    return e;
}

Eval<double> eval(const double* q) { return eval<double>(q, q); }

template <typename R>
Eval<Dual<R>> eval_dual(const double* q) {
    Dual<R> p[kNumParams];
    for (int k = 0; k < kNumParams; ++k) p[k] = var<R>(q[k], k);
    return eval<Dual<R>>(p, q);
}

// A ray through or near the disc of a random surfel; tiny: discs of 0.002-0.02,
// under a pixel to a few at f = 1000 from 2-5 units, where fp32 cancels most.
std::vector<double> make_case(std::mt19937& rng, bool tiny) {
    std::uniform_real_distribution<double> U(-1, 1);
    std::normal_distribution<double> N01(0, 1);
    std::vector<double> p(kStride, 0.0);
    double q[4] = {N01(rng), N01(rng), N01(rng), N01(rng)};
    double qn = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int i = 0; i < 4; ++i) p[3 + i] = q[i] / qn;
    for (int i = 0; i < 3; ++i) p[i] = U(rng);
    p[7] = tiny ? -5 + U(rng) : -1.5 + U(rng);
    p[8] = tiny ? -5 + U(rng) : -1.5 + U(rng);
    p[9] = U(rng);
    p[10] = 0.5 + 0.45 * U(rng);
    for (int i = 0; i < 3; ++i) p[11 + i] = 0.5 + 0.5 * U(rng);

    double w = p[3], x = p[4], y = p[5], z = p[6];
    V3<double> tu = {1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)};
    V3<double> tv = {2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)};
    const double su = std::exp(p[7]), sv = std::exp(p[8]);
    const double a = 1.2 * N01(rng), b = 1.2 * N01(rng);
    V3<double> target;
    for (int i = 0; i < 3; ++i)
        target[i] = p[i] + a * su * tu[i] + b * sv * tv[i];
    V3<double> dir = {N01(rng), N01(rng), N01(rng)};
    double dl = std::sqrt(dot(dir, dir)), dist = 2 + 3 * (0.5 + 0.5 * U(rng));
    double dscale = 0.5 + 0.75 * (1 + U(rng));
    for (int i = 0; i < 3; ++i) {
        p[14 + i] = target[i] - dist * dir[i] / dl;
        p[17 + i] = dscale * dir[i] / dl;
    }
    p[kInVAlpha] = U(rng);
    for (int i = 0; i < 3; ++i) {
        p[kInVRgb + i] = U(rng);
        p[kInVNormal + i] = U(rng);
    }
    p[kInVDepth] = U(rng);
    return p;
}

}  // namespace

int main() {
    using namespace backend;
    constexpr int kCases = 4096;
    std::mt19937 rng(7);
    std::vector<std::vector<double>> cases;
    std::vector<float> in((size_t)kCases * kStride);
    for (int c = 0; c < kCases; ++c) {
        cases.push_back(make_case(rng, c % 2 == 1));
        for (int i = 0; i < kStride; ++i)
            in[(size_t)c * kStride + i] = (float)cases.back()[i];
        // The host model reads exactly what the device reads.
        for (int i = 0; i < kStride; ++i)
            cases.back()[i] = (double)in[(size_t)c * kStride + i];
    }

    void* d_in = device_malloc(in.size() * sizeof(float));
    void* d_out = device_malloc(in.size() * sizeof(float));
    memcpy_sync(d_in, in.data(), in.size() * sizeof(float), MemcpyKind::HostToDevice);
    memset_sync(d_out, 0, in.size() * sizeof(float));
    struct {
        uint64_t in, out;
        uint32_t n;
    } params{(uint64_t)d_in, (uint64_t)d_out, (uint32_t)kCases};
    CHECK(vk::dispatch(kDefaultStream, "surfel_test.surfel_eval", vk::SpecList{},
                       (kCases + 63) / 64, 1, 1, &params, sizeof(params)));
    device_synchronize();
    if (const char* err = last_error()) {
        std::printf("backend error: %s\n", err);
        return 1;
    }
    std::vector<float> out(in.size());
    memcpy_sync(out.data(), d_out, out.size() * sizeof(float), MemcpyKind::DeviceToHost);
    device_free(d_in);
    device_free(d_out);

    auto rel = [](const auto& got, const double* want) {
        double num = 0, den = 0;
        for (int k = 0; k < kNumParams; ++k) {
            num += ((double)got[k] - want[k]) * ((double)got[k] - want[k]);
            den += want[k] * want[k];
        }
        return std::sqrt(num) / std::max(std::sqrt(den), 1e-2);
    };
    int n_hit = 0, n_checked = 0;
    int n_fwd_bad = 0, n_fd_bad = 0;
    double worst_fwd = 0, worst_fd = 0;
    std::vector<double> err_dev, err_f32;
    for (int c = 0; c < kCases; ++c) {
        const double* p = cases[c].data();
        const float* o = &out[(size_t)c * kStride];
        Eval<double> e = eval(p);

        double fwd_err = std::abs(o[0] - e.alpha);
        for (int i = 0; i < 3; ++i) {
            fwd_err = std::max(fwd_err, std::abs(o[1 + i] - e.rgb[i]));
            fwd_err = std::max(fwd_err, std::abs(o[5 + i] - e.normal[i]));
        }
        fwd_err = std::max(fwd_err, std::abs(o[4] - e.depth) / std::max(1.0, std::abs(e.depth)));
        worst_fwd = std::max(worst_fwd, fwd_err);
        // fp32 rounds the disc-frame coordinates, up to |o - mean| / scale, so
        // a small disc is held to a few ulps of that.
        const V3<double> om = {p[14] - p[0], p[15] - p[1], p[16] - p[2]};
        const V3<double> d = {p[17], p[18], p[19]};
        const double frame = std::exp(-std::min(p[7], p[8])) *
                             (std::sqrt(dot(om, om)) + std::abs(e.depth) * std::sqrt(dot(d, d)));
        if (fwd_err > 1e-4 + 16 * FLT_EPSILON * frame && ++n_fwd_bad <= 5)
            std::printf("forward mismatch case %d: err %.3g\n", c, fwd_err);

        // Central differences (one Richardson step) hold the dual model to
        // the forward it differentiates; a step crossing a branch skips them.
        // The step shrinks with the disc, whose size sets the curvature.
        const double step = 1e-6 * std::min(1.0, 10 * std::exp(std::min(p[7], p[8])));
        std::vector<double> pp(cases[c]);
        double fd[kNumParams];
        bool smooth = true;
        auto central = [&](int k, double h) {
            const double x0 = pp[k];
            pp[k] = x0 + h;
            const Eval<double> e1 = eval(pp.data());
            pp[k] = x0 - h;
            const Eval<double> e2 = eval(pp.data());
            pp[k] = x0;
            smooth = smooth && e1.branch == e.branch && e2.branch == e.branch;
            return (e1.loss - e2.loss) / (2 * h);
        };
        for (int k = 0; k < kNumParams && smooth; ++k) {
            const double h = step * std::max(1.0, std::abs(pp[k]));
            fd[k] = (4 * central(k, h / 2) - central(k, h)) / 3;
        }
        if (!smooth) continue;
        ++n_checked;
        n_hit += val(e.alpha) > 1e-3;

        const Eval<Dual<double>> exact = eval_dual<double>(p);
        const Eval<Dual<float>> f32 = eval_dual<float>(p);
        const double r_fd = rel(fd, exact.loss.g);
        worst_fd = std::max(worst_fd, r_fd);
        if (r_fd > 1e-6 && ++n_fd_bad <= 5)
            std::printf("dual model vs central differences, case %d: rel %.3g\n", c, r_fd);

        // Near grazing, and for tiny discs seen from metres, fp32 itself loses
        // up to ~1%; the device is held to the error distribution of the same
        // model evaluated in fp32.
        err_dev.push_back(rel(&o[kOutGrad], exact.loss.g));
        err_f32.push_back(rel(f32.loss.g, exact.loss.g));
    }

    auto pct = [](std::vector<double> v, double q) {
        std::sort(v.begin(), v.end());
        return v.empty() ? 0.0 : v[(size_t)(q * (double)(v.size() - 1))];
    };
    const double dev50 = pct(err_dev, 0.5), dev999 = pct(err_dev, 0.999), dev_max = pct(err_dev, 1.0);
    const double f32_999 = pct(err_f32, 0.999), f32_max = pct(err_f32, 1.0);
    std::printf("cases %d, gradient-checked %d (%d with alpha > 1e-3)\n",
                kCases, n_checked, n_hit);
    std::printf("worst forward %.3g, worst dual vs differences %.3g\n", worst_fwd, worst_fd);
    std::printf("relative gradient error  device: median %.3g, 99.9%% %.3g, max %.3g\n"
                "                          fp32 model:          99.9%% %.3g, max %.3g\n",
                dev50, dev999, dev_max, f32_999, f32_max);
    CHECK(n_fwd_bad == 0);
    CHECK(n_fd_bad == 0);
    CHECK(dev50 < 1e-5);
    CHECK(dev999 <= 4 * f32_999 + 1e-5);
    CHECK(dev_max <= 2 * f32_max + 1e-5);
    CHECK(n_hit > kCases / 4);

    if (g_failures == 0) std::printf("surfel_grad_test: PASS\n");
    return g_failures == 0 ? 0 : 1;
}
