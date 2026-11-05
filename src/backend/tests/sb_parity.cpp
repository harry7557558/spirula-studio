// Spherical-beta lobes on the Vulkan backend, held to an independent host
// model (core/SphericalBeta.h, in double): the projection forward's colour in
// fp32 and through both value-codec layouts, the projection backward's
// gradients against central differences, and the grad-quant and fused (FPBO)
// backwards against the plain one. Self-checking. The CUDA kernels carry no
// lobes, so a CUDA build only reports that.
//
//   ./sb_parity

#include <backend/tests/DistortionFixture.h>
#include <core/AabbQuant.cuh>
#include <core/SphericalBeta.h>
#include <kernels/optim/FusedProjectionBwdOptim.cuh>
#include <kernels/projection/ProjectionBwd.cuh>
#include <kernels/projection/ProjectionBwdQuantGrad.cuh>
#include <kernels/projection/ProjectionFwd.cuh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include "backend/tests/ScreenRows.h"

#ifdef SS_BACKEND_VULKAN

using backend::MemcpyKind;
namespace sb = spirula::sb;

static constexpr int64_t N = 512;
static constexpr int64_t C = 3;
static constexpr uint32_t W = 160, H = 120;
static constexpr double kC0 = 0.28209479177387814;

template <typename T>
static T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(std::max<size_t>(host.size(), 1) * sizeof(T));
    if (!host.empty())
        backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                             MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
static std::vector<T> download(const void* d, int64_t n) {
    std::vector<T> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * sizeof(T),
                         MemcpyKind::DeviceToHost);
    return h;
}

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

static int g_failures = 0;

// Worst of min(abs, rel) error over `got` vs `want`, failing above `tol`.
static void check(const std::string& what, const std::vector<double>& got,
                  const std::vector<double>& want, double tol) {
    double worst = 0.0, scale = 0.0;
    size_t at = 0;
    for (double w : want) scale = std::max(scale, std::fabs(w));
    for (size_t i = 0; i < got.size(); i++) {
        const double d = std::fabs(got[i] - want[i]);
        const double e = std::min(d / (1e-3 * scale + 1e-12),
                                  d / (std::fabs(want[i]) + 1e-12));
        if (e > worst) { worst = e; at = i; }
    }
    const bool ok = worst <= tol && !got.empty();
    if (!ok) g_failures++;
    std::printf("  %-44s n %6zu  worst %.2e @ %zu (got %.6g want %.6g)  %s\n",
                what.c_str(), got.size(), worst, at,
                got.empty() ? 0.0 : got[at], want.empty() ? 0.0 : want[at],
                ok ? "ok" : "FAIL");
}

struct Scene {
    ColorBasis basis;
    int K = 0;  // float3 per colour row
    std::vector<float> means, quats, scales, opac, dc, rows;
    std::vector<float> vm, intr, dist;
};

static Scene make_scene(ColorBasis basis, uint32_t seed) {
    std::mt19937 rng(seed);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    Scene s;
    s.basis = basis;
    s.K = basis.coeffs();
    s.means.resize(N * 3); s.quats.resize(N * 4); s.scales.resize(N * 3);
    s.opac.resize(N); s.dc.resize(N * 3); s.rows.resize(N * s.K * 3);
    for (int64_t i = 0; i < N; i++) {
        s.means[3 * i + 0] = uf(-1.5f, 1.5f);
        s.means[3 * i + 1] = uf(-1.1f, 1.1f);
        s.means[3 * i + 2] = uf(-1.0f, 1.0f);
        for (int k = 0; k < 4; k++) s.quats[4 * i + k] = uf(-1.f, 1.f);
        for (int k = 0; k < 3; k++) s.scales[3 * i + k] = uf(-4.f, -2.5f);
        s.opac[i] = uf(-1.f, 3.f);
        for (int k = 0; k < 3; k++) s.dc[3 * i + k] = uf(-2.2f, 1.5f);
        float* row = &s.rows[(size_t)(i * s.K * 3)];
        for (int c = 0; c < 3 * basis.sh_coeffs(); c++) row[c] = uf(-0.4f, 0.4f);
        float* lobe = row + 3 * basis.sh_coeffs();
        for (int l = 0; l < basis.sb_lobes; l++, lobe += sb::kLobeFloats) {
            for (int k = 0; k < 3; k++) lobe[k] = uf(-0.6f, 0.8f);
            lobe[3] = uf(0.f, 3.1416f);
            lobe[4] = uf(-3.1416f, 6.2832f);
            // n = 4 exp(beta) > 1: the weight is C1 across the hemisphere's
            // edge, so central differences hold there too.
            lobe[5] = uf(-0.6f, 1.5f);
        }
    }
    // Three cameras 4.5 units out, turned so the view directions spread.
    for (int c = 0; c < C; c++) {
        const float a = -0.5f + 0.5f * c, ca = std::cos(a), sa = std::sin(a);
        const float v[16] = {ca, 0, sa, 0.1f * c,  0, 1, 0, -0.05f,
                             -sa, 0, ca, 4.5f,      0, 0, 0, 1};
        s.vm.insert(s.vm.end(), v, v + 16);
        const float in[4] = {120.f + 5.f * c, 121.f, W * 0.5f, H * 0.5f};
        s.intr.insert(s.intr.end(), in, in + 4);
    }
    s.dist.assign(C * kCameraDistortionParams, 0.f);
    return s;
}

// The unit direction from camera c to splat i, as the kernels form it.
static void view_dir(const Scene& s, int c, int64_t i, const double* mean,
                     double dir[3]) {
    const float* v = &s.vm[(size_t)c * 16];
    double d[3];
    for (int k = 0; k < 3; k++)
        d[k] = mean[k] + v[0 * 4 + k] * v[3] + v[1 * 4 + k] * v[7] +
               v[2 * 4 + k] * v[11];
    const double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (int k = 0; k < 3; k++) dir[k] = d[k] / n;
    (void)i;
}

// Sum of splat i's lobes seen from camera c, with `lobes` overriding the row.
static void lobes_color(const Scene& s, int c, int64_t i, const double* mean,
                        const float* lobes, double out[3]) {
    double dir[3];
    view_dir(s, c, i, mean, dir);
    out[0] = out[1] = out[2] = 0.0;
    for (int l = 0; l < s.basis.sb_lobes; l++) {
        double one[3];
        sb::lobe_color(lobes + sb::kLobeFloats * l, dir, one);
        for (int k = 0; k < 3; k++) out[k] += one[k];
    }
}

struct Device {
    std::vector<DeviceTensorFloatND> splats;
    float* rows = nullptr;
    float *vm = nullptr, *intr = nullptr, *dist = nullptr, *radii = nullptr;
};

static Device to_device(const Scene& s, const std::vector<float>& rows, int K) {
    Device d;
    d.rows = upload(rows);
    d.splats = {
        DeviceTensorFloatND(ttv(upload(s.means), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(s.quats), {N, 4, 1})),
        DeviceTensorFloatND(ttv(upload(s.scales), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(s.opac), {N, 1, 1})),
        DeviceTensorFloatND(ttv(upload(s.dc), {N, 3, 1})),
        DeviceTensorFloatND(ttv(d.rows, {N, (int64_t)K * 3, 1})),
    };
    d.vm = upload(s.vm);
    d.intr = upload(s.intr);
    d.dist = upload(s.dist);
    d.radii = (float*)backend::device_malloc(N * sizeof(float));
    return d;
}

struct Fwd {
    std::vector<uint2> aabb;
    std::vector<float> screen;  // [C*N, stride]
    DeviceTensor2D<uint2> aabb_dev;
};

static int stride_of(bool eval3d) { return eval3d ? SCRG_STRIDE : SCR2_STRIDE; }
static int rgb_of(bool eval3d) { return eval3d ? SCRG_RGB : SCR2_RGB; }

static Fwd forward(const Device& d, ColorBasis color, bool eval3d,
                   const char* cam, std::optional<TorchTensorView> vp = {},
                   std::optional<TorchTensorView> vb = {}, uint32_t num_sh = 0,
                   int bits = 32, int64_t bstride = 256) {
    backend::memset_sync(d.radii, 0, N * sizeof(float));
    DeviceVector<float> radii(ttv(d.radii, {N, 1}));
    auto fn = eval3d ? projection_3dgut_forward : projection_3dgs_forward;
    auto out = fn(N, color, d.splats, ttv(d.vm, {C, 16}), ttv(d.intr, {C, 4}),
                  W, H, cam, "NONE", ttv(d.dist, {C, kCameraDistortionParams}),
                  radii, vp, vb, num_sh, bits, bstride);
    backend::device_synchronize();
    Fwd f;
    f.aabb_dev = std::get<0>(out);
    f.aabb = download<uint2>(f.aabb_dev.data_ptr(), C * N);
    f.screen = download<float>(std::get<2>(out)[0].data_ptr(),
                               C * N * stride_of(eval3d));
    return f;
}

// The colour rows without their lobes (at least one coefficient wide, so the
// tensor is never empty), which the SH-only launch reads.
static std::vector<float> strip_lobes(const Scene& s, const std::vector<float>& rows,
                                      int* K_out) {
    const int Ksh = std::max(s.basis.sh_coeffs(), 1);
    std::vector<float> out((size_t)(N * Ksh * 3), 0.f);
    for (int64_t i = 0; i < N; i++)
        std::copy(&rows[(size_t)(i * s.K * 3)],
                  &rows[(size_t)(i * s.K * 3)] + 3 * s.basis.sh_coeffs(),
                  &out[(size_t)(i * Ksh * 3)]);
    *K_out = Ksh;
    return out;
}

// What the lobes add on top of the same model without them, against the host.
static void check_forward(const Scene& s, const std::vector<float>& rows,
                          const Fwd& got, bool eval3d, const char* cam,
                          const std::string& tag) {
    int Ksh;
    const std::vector<float> sh_rows = strip_lobes(s, rows, &Ksh);
    Device bare = to_device(s, sh_rows, Ksh);
    const Fwd base = forward(bare, ColorBasis{s.basis.sh_degree, 0}, eval3d, cam);
    std::vector<double> g, w;
    const int st = stride_of(eval3d), ro = rgb_of(eval3d);
    for (int c = 0; c < C; c++)
        for (int64_t i = 0; i < N; i++) {
            const int64_t idx = c * N + i;
            if (aabb16_is_empty(got.aabb[idx])) continue;
            const double mean[3] = {s.means[3 * i], s.means[3 * i + 1],
                                    s.means[3 * i + 2]};
            double add[3];
            lobes_color(s, c, i, mean,
                        &rows[(size_t)((i * s.K + s.basis.sh_coeffs()) * 3)], add);
            for (int k = 0; k < 3; k++) {
                g.push_back(got.screen[(size_t)(idx * st + ro + k)]);
                w.push_back(base.screen[(size_t)(idx * st + ro + k)] + add[k]);
            }
        }
    check(tag, g, w, 2e-4);
}

// The packed value codec in either layout (docs/notes/sh-quant-layout.md),
// and the rows it decodes back to.
template <int BITS>
static void encode_rows(const std::vector<float>& rows, int K, bool fpbo,
                        std::vector<uint8_t>& packed, std::vector<float>& bounds,
                        std::vector<float>& decoded) {
    using Q = QuantizedTensor<BITS, 256>;
    const ShQuantAddr a = sh_quant_addr((uint32_t)K, fpbo ? 0 : 256);
    const int64_t R = 3 * (int64_t)K;
    const int64_t cells = fpbo ? sh_fpbo_cells(N, (uint32_t)K) : N * R;
    const int64_t nb = fpbo ? (N + 255) / 256 : (cells + 255) / 256;
    std::vector<float2> mm((size_t)nb, float2{1e30f, -1e30f});
    for (int64_t i = 0; i < N; i++)
        for (int c = 0; c < R; c++) {
            const int64_t b = a.cell(a.base(i), c) / a.bounds_stride;
            mm[(size_t)b].x = std::min(mm[(size_t)b].x, rows[(size_t)(i * R + c)]);
            mm[(size_t)b].y = std::max(mm[(size_t)b].y, rows[(size_t)(i * R + c)]);
        }
    packed.assign((size_t)(cells * Q::kBytesPerCell), 0);
    decoded.resize(rows.size());
    for (int64_t i = 0; i < N; i++)
        for (int c = 0; c < R; c++) {
            const int64_t cell = a.cell(a.base(i), c);
            const float2 m = mm[(size_t)(cell / a.bounds_stride)];
            Q::encode_v(packed.data(), cell, rows[(size_t)(i * R + c)], m);
            decoded[(size_t)(i * R + c)] = Q::decode_v(packed.data(), cell, m);
        }
    bounds.resize(2 * (size_t)nb);
    for (int64_t b = 0; b < nb; b++) {
        bounds[2 * b] = mm[(size_t)b].x;
        bounds[2 * b + 1] = mm[(size_t)b].y;
    }
}

// Per splat, the gradient of sum over its visible pairs of v_rgb . colour
// with respect to its lobes and (SH degree 0 only) its mean and DC, by
// central differences of the host model.
struct FdGrads {
    std::vector<double> lobes, means, dc;
};

static FdGrads finite_differences(const Scene& s, const Fwd& f,
                                  const std::vector<float>& v_rgb) {
    FdGrads out;
    const int L = s.basis.sb_lobes, Ksh = s.basis.sh_coeffs();
    out.lobes.assign((size_t)(N * L * sb::kLobeFloats), 0.0);
    out.means.assign((size_t)(N * 3), 0.0);
    out.dc.assign((size_t)(N * 3), 0.0);
    for (int64_t i = 0; i < N; i++) {
        std::vector<float> lobes(&s.rows[(size_t)((i * s.K + Ksh) * 3)],
                                 &s.rows[(size_t)((i * s.K + Ksh) * 3)] +
                                     L * sb::kLobeFloats);
        double mean[3] = {s.means[3 * i], s.means[3 * i + 1], s.means[3 * i + 2]};
        auto loss = [&]() {
            double sum = 0.0;
            for (int c = 0; c < C; c++) {
                const int64_t idx = c * N + i;
                if (aabb16_is_empty(f.aabb[idx])) continue;
                double col[3];
                lobes_color(s, c, i, mean, lobes.data(), col);
                for (int k = 0; k < 3; k++) sum += v_rgb[(size_t)(idx * 3 + k)] * col[k];
            }
            return sum;
        };
        auto central = [&](auto&& set, double x0, double h) {
            set(x0 + h);
            const double up = loss();
            set(x0 - h);
            const double dn = loss();
            set(x0);
            return (up - dn) / (2.0 * h);
        };
        for (int p = 0; p < L * sb::kLobeFloats; p++) {
            const float x0 = lobes[(size_t)p];
            // Float steps: the kernel reads float parameters.
            out.lobes[(size_t)(i * L * sb::kLobeFloats + p)] =
                central([&](double x) { lobes[(size_t)p] = (float)x; }, x0, 1e-3);
        }
        for (int k = 0; k < 3; k++)
            out.means[(size_t)(3 * i + k)] =
                central([&](double x) { mean[k] = x; }, mean[k], 1e-4);
        for (int k = 0; k < 3; k++) {
            double g = 0.0;
            for (int c = 0; c < C; c++) {
                const int64_t idx = c * N + i;
                if (aabb16_is_empty(f.aabb[idx])) continue;
                if (kC0 * s.dc[3 * i + k] >= -0.5)
                    g += kC0 * v_rgb[(size_t)(idx * 3 + k)];
            }
            out.dc[(size_t)(3 * i + k)] = g;
        }
    }
    return out;
}

static void run(ColorBasis basis, bool eval3d, const char* cam) {
    const std::string tag = std::string(eval3d ? "3dgut " : "3dgs ") + cam +
                            " SH" + std::to_string(basis.sh_degree) + " SB" +
                            std::to_string(basis.sb_lobes);
    std::printf("%s\n", tag.c_str());
    const Scene s = make_scene(basis, 1234u + 17u * basis.sb_lobes + basis.sh_degree);
    const Device d = to_device(s, s.rows, s.K);

    // ---- forward: fp32, then both codec layouts ----
    const Fwd f = forward(d, basis, eval3d, cam);
    check_forward(s, s.rows, f, eval3d, cam, "forward fp32");
    for (int fpbo = 0; fpbo < 2; fpbo++) {
        std::vector<uint8_t> packed;
        std::vector<float> bounds, decoded;
        encode_rows<16>(s.rows, s.K, fpbo, packed, bounds, decoded);
        uint8_t* dp = upload(packed);
        float* db = upload(bounds);
        const Fwd fq = forward(d, basis, eval3d, cam,
                               ttv(dp, {(int64_t)packed.size(), 1}),
                               ttv(db, {(int64_t)bounds.size() / 2, 2}),
                               (uint32_t)s.K, 16, fpbo ? 0 : 256);
        check_forward(s, decoded, fq, eval3d, cam,
                      fpbo ? "forward q16, FPBO layout" : "forward q16, cell blocks");
        encode_rows<8>(s.rows, s.K, fpbo, packed, bounds, decoded);
        dp = upload(packed);
        db = upload(bounds);
        const Fwd f8 = forward(d, basis, eval3d, cam,
                               ttv(dp, {(int64_t)packed.size(), 1}),
                               ttv(db, {(int64_t)bounds.size() / 2, 2}),
                               (uint32_t)s.K, 8, fpbo ? 0 : 256);
        check_forward(s, decoded, f8, eval3d, cam,
                      fpbo ? "forward q8, FPBO layout" : "forward q8, cell blocks");
    }

    // ---- plain backward against central differences ----
    std::mt19937 rng(99u);
    std::uniform_real_distribution<float> uv(-1.f, 1.f);
    std::vector<float> v_rgb((size_t)(C * N * 3));
    for (float& v : v_rgb) v = uv(rng);
    const int st = stride_of(eval3d), ro = rgb_of(eval3d);
    std::vector<float> v_rows((size_t)(C * N * st), 0.f);
    for (int64_t idx = 0; idx < C * N; idx++)
        for (int k = 0; k < 3; k++)
            v_rows[(size_t)(idx * st + ro + k)] = v_rgb[(size_t)(idx * 3 + k)];
    std::vector<DeviceTensorFloatND> v_screen = {
        DeviceTensorFloatND(ttv(upload(v_rows), {C * N, st, 1}))};
    const int wch[6] = {3, 4, 3, 1, 3, 3 * s.K};
    std::vector<DeviceTensorFloatND> v_world;
    for (int c = 0; c < 6; c++)
        v_world.push_back(DeviceTensorFloatND(
            ttv(upload(std::vector<float>((size_t)(N * wch[c]), 0.f)), {N, wch[c], 1})));
    auto bwd = eval3d ? projection_3dgut_backward : projection_3dgs_backward;
    bwd(N, basis, d.splats, ttv(d.vm, {C, 16}), ttv(d.intr, {C, 4}), W, H, cam,
        "NONE", ttv(d.dist, {C, kCameraDistortionParams}), DeviceVector<int32_t>{},
        DeviceVector<int32_t>{}, f.aabb_dev, v_screen, v_world, nullptr,
        std::nullopt, std::nullopt, (uint32_t)s.K, 32, 256);
    backend::device_synchronize();
    const std::vector<float> g_rows = download<float>(v_world[5].data_ptr(), N * 3 * s.K);
    const std::vector<float> g_means = download<float>(v_world[0].data_ptr(), N * 3);
    const std::vector<float> g_dc = download<float>(v_world[4].data_ptr(), N * 3);

    const FdGrads fd = finite_differences(s, f, v_rgb);
    const int L = basis.sb_lobes, Ksh = basis.sh_coeffs();
    std::vector<double> got_l, want_l;
    for (int64_t i = 0; i < N; i++)
        for (int p = 0; p < L * sb::kLobeFloats; p++) {
            got_l.push_back(g_rows[(size_t)((i * s.K + Ksh) * 3 + p)]);
            want_l.push_back(fd.lobes[(size_t)(i * L * sb::kLobeFloats + p)]);
        }
    check("backward lobes vs central differences", got_l, want_l, 2e-2);
    if (basis.sh_degree == 0) {
        std::vector<double> gm(g_means.begin(), g_means.end());
        std::vector<double> gd(g_dc.begin(), g_dc.end());
        check("backward means vs central differences", gm, fd.means, 2e-2);
        check("backward dc", gd, fd.dc, 1e-4);
    }

    // ---- grad-quant backward: the colour row's 8-bit accumulator ----
    {
        const int64_t nb = (N + 255) / 256;
        uint8_t* gq_sh = upload(std::vector<uint8_t>((size_t)(N * 3 * s.K), 0));
        float* gq_b = upload(std::vector<float>((size_t)(2 * nb), 0.f));
        GradQuantBuffers gq;
        gq.sh_packed = gq_sh;
        gq.sh_bounds = (float2*)gq_b;
        std::vector<DeviceTensorFloatND> vw0;
        for (int c = 0; c < 6; c++)
            vw0.push_back(c < 3 && eval3d
                ? DeviceTensorFloatND(ttv(upload(std::vector<float>(
                                              (size_t)(N * wch[c]), 0.f)),
                                          {N, wch[c], 1}))
                : DeviceTensorFloatND());
        auto qg = eval3d ? projection_3dgut_backward_quantgrad
                         : projection_3dgs_backward_quantgrad;
        qg(N, basis, d.splats, ttv(d.vm, {C, 16}), ttv(d.intr, {C, 4}), W, H,
           cam, "NONE", ttv(d.dist, {C, kCameraDistortionParams}),
           DeviceVector<int32_t>{}, DeviceVector<int32_t>{}, f.aabb_dev, v_screen,
           vw0, gq, std::nullopt, std::nullopt, (uint32_t)s.K, 32, 256);
        backend::device_synchronize();
        const std::vector<int8_t> codes = download<int8_t>(gq_sh, N * 3 * s.K);
        const std::vector<float> bd = download<float>(gq_b, 2 * nb);
        std::vector<double> got, want;
        for (int64_t i = 0; i < N; i++) {
            const int64_t b = i / 256;
            const double amp = std::max(std::fabs(bd[2 * b]), std::fabs(bd[2 * b + 1]));
            for (int p = 3 * Ksh; p < 3 * s.K; p++) {
                const double v = g_rows[(size_t)(i * 3 * s.K + p)];
                const double q = codes[(size_t)(i * 3 * s.K + p)] * amp / 127.0;
                // Within half a quantum is exact for this codec.
                got.push_back(std::fabs(q - v) <= 0.5 * amp / 127.0 * 1.0001 ? v : q);
                want.push_back(v);
            }
        }
        check("grad-quant lobes vs plain backward", got, want, 1e-6);
    }

    // ---- FPBO: its first-moment update from zero state is 0.1 * gradient ----
    {
        std::vector<DeviceTensorFloatND> splats_f = to_device(s, s.rows, s.K).splats;
        std::vector<DeviceTensorFloatND> zw, g1, g2;
        float* g1_rows = nullptr;
        for (int c = 0; c < 6; c++) {
            const std::vector<float> z((size_t)(N * wch[c]), 0.f);
            zw.push_back(DeviceTensorFloatND());
            float* p1 = upload(z);
            if (c == 5) g1_rows = p1;
            g1.push_back(DeviceTensorFloatND(ttv(p1, {N, wch[c], 1})));
            g2.push_back(DeviceTensorFloatND(ttv(upload(z), {N, wch[c], 1})));
        }
        float* radii = upload(std::vector<float>((size_t)N, 0.f));
        auto fn = eval3d ? fused_projection_bwd_optimizer_3dgut
                         : fused_projection_bwd_optimizer_3dgs;
        fn(N, basis, splats_f, ttv(d.vm, {C, 16}), ttv(d.intr, {C, 4}), W, H, cam,
           "NONE", ttv(d.dist, {C, kCameraDistortionParams}),
           DeviceVector<int32_t>{}, DeviceVector<int32_t>{}, f.aabb_dev, zw,
           v_screen, g1, g2, std::nullopt, std::nullopt, std::nullopt,
           std::nullopt, NonShQuantState{}, SplatVisitState{}, {},
           DeviceVector<float>(ttv(radii, {N, 1})), DeviceVector<float>{},
           1e-4f, 1e-3f, 5e-3f, 5e-2f, 2.5e-3f, 1e-4f, 2.5e-3f,
           /*max_gauss_ratio=*/10.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
           /*dc_reg=*/0.f, /*sh_reg=*/0.f, /*max_screen_size=*/1e9f, 0.f,
           false, false, 1e-6f, (int32_t)1, /*quantization_level=*/0);
        backend::device_synchronize();
        const std::vector<float> m1 = download<float>(g1_rows, N * 3 * s.K);
        std::vector<double> got, want;
        for (int64_t i = 0; i < N; i++)
            for (int p = 3 * Ksh; p < 3 * s.K; p++) {
                got.push_back(10.0 * m1[(size_t)(i * 3 * s.K + p)]);
                want.push_back(g_rows[(size_t)(i * 3 * s.K + p)]);
            }
        check("FPBO lobes vs plain backward", got, want, 1e-3);
    }
    if (const char* err = backend::last_error()) {
        std::printf("  backend error: %s\n", err);
        g_failures++;
    }
}

int main() {
    const ColorBasis bases[] = {{0, 1}, {0, 2}, {2, 2}, {3, 1}, {0, 4}};
    for (const ColorBasis& b : bases) {
        run(b, false, "PINHOLE");
        run(b, false, "FISHEYE");
        run(b, true, "PINHOLE");
    }
    std::printf("sb_parity: %s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

#else

int main() {
    std::printf("sb_parity: skipped (spherical-beta lobes are Vulkan-only)\n");
    return 0;
}

#endif
