// FPBO against the non-fused optimizer path (fused_optim_3dgs_geometry + the
// DC/SH kernels): one step from identical state and identical world gradients
// must move every splat parameter the same way, or a dataset large enough to
// pick the split-batch path trains under a different update law. Self-checking
// on either backend; `v_splats_screen` is zeroed so the fused
// projection-backward half contributes nothing and only the optimizers differ.
//
//   ./fpbo_split_parity

#include <backend/tests/DistortionFixture.h>
#include <kernels/optim/FusedProjectionBwdOptim.cuh>
#include <kernels/optim/Optimizer.cuh>
#include <kernels/projection/ProjectionFwd.cuh>

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

static constexpr int64_t N = 3072;
static constexpr int64_t C = 2;
static constexpr uint32_t W = 200, H = 150;
static constexpr int NUM_SH = 15;
static constexpr int MAX_DEG = 3;
static constexpr int64_t NB = (N + 255) / 256;
static constexpr int64_t SH_CELLS = N * NUM_SH * 3;

static constexpr float kLrMeans = 1.6e-4f, kLrQuats = 1e-3f;
static constexpr float kLrScales = 5e-3f, kLrOpacs = 5e-2f;
// Colour rates far above the presets': the trust-region rails have to
// bind, or a path that never clips still matches one that does.
static constexpr float kLrDc = 2.5e-3f, kLrSh = 5e-2f;
static constexpr float kDcReg = 0.02f, kShReg = 0.05f;
static constexpr float kEpsTr = 1e-8f;  // small enough that the rails bind
static constexpr int32_t kStep = 10;

template <typename T>
static T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

static std::vector<float> download(const float* d, int64_t n) {
    std::vector<float> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * sizeof(float),
                         MemcpyKind::DeviceToHost);
    return h;
}

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

template <typename T>
static DeviceVector<T> dv(const void* p, int64_t n) {
    return DeviceVector<T>(std::make_tuple((uint64_t)p, (uint32_t)sizeof(T),
                                           std::vector<int64_t>{n, 1}));
}

// One 16-bit quantized SH value, from whichever block layout wrote it
// (bounds_stride 0 = FPBO per-splat-block, 256 = per-cell-block).
static float decode_sh16(const std::vector<uint16_t>& packed,
                         const std::vector<float>& bounds,
                         const ShQuantAddr& addr, int64_t splat, int cell) {
    const int64_t c = addr.cell(addr.base(splat), cell);
    const float2 mm = {bounds[2 * (c / addr.bounds_stride)],
                       bounds[2 * (c / addr.bounds_stride) + 1]};
    return mm.x + (mm.y - mm.x) * ((float)packed[(size_t)c] * (1.0f / 65535.0f));
}

int main() {
    std::mt19937 rng(4242u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3), sh(SH_CELLS);
    for (int64_t i = 0; i < N; i++) {
        for (int k = 0; k < 3; k++) means[3 * i + k] = uf(-4.f, 4.f);
        means[3 * i + 2] = uf(-2.f, 8.f);
        float qn = 0.f;
        for (int k = 0; k < 4; k++) {
            quats[4 * i + k] = uf(-1.f, 1.f);
            qn += quats[4 * i + k] * quats[4 * i + k];
        }
        if (qn < 1e-6f) quats[4 * i] = 1.f;
        for (int k = 0; k < 3; k++) scales[3 * i + k] = uf(-5.f, -1.5f);
        opac[i] = uf(-3.f, 5.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = uf(-0.5f, 1.5f);
    }
    for (auto& v : sh) v = uf(-0.3f, 0.3f);

    // Gradients and Adam state, shared by both paths. The SH data gradient
    // stays zero: FPBO derives it in-kernel from the camera loop and never
    // reads v_splats_world[5], so only momentum drives the SH step here.
    const int wch[6] = {3, 4, 3, 1, 3, 3 * NUM_SH};
    std::vector<std::vector<float>> vw(6), g1(6), g2(6);
    for (int c = 0; c < 6; c++) {
        vw[c].resize(N * wch[c]);
        g1[c].resize(N * wch[c]);
        g2[c].resize(N * wch[c]);
        for (auto& v : vw[c]) v = uf(-0.05f, 0.05f);
        for (auto& v : g1[c]) v = uf(-0.02f, 0.02f);
        for (auto& v : g2[c]) v = uf(0.f, 4e-4f);
    }
    for (auto& v : vw[5]) v = 0.f;

    const float cy_ = std::cos(0.2f), sy_ = std::sin(0.2f);
    std::vector<float> vm = {
        1, 0, 0, 0,   0, 1, 0, 0,    0, 0, 1, 4,     0, 0, 0, 1,
        cy_, 0, sy_, 0.3f,  0, 1, 0, -0.2f,  -sy_, 0, cy_, 5.f,  0, 0, 0, 1,
    };
    std::vector<float> intr = {150, 152, 100, 75, 145, 146, 97, 78};
    std::vector<float> dist(C * kCameraDistortionParams, 0.f);
    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));

    // Screen-space gradients stay zero: the fused kernel's projection-backward
    // half is a VJP, so it adds nothing and both paths see the same v_world.
    const int64_t n_isect = C * N;
    std::vector<float> zero_screen((size_t)n_isect * SCR2_STRIDE, 0.f);
    std::vector<DeviceTensorFloatND> v_screen = {DeviceTensorFloatND(
        ttv(upload(zero_screen), {n_isect, SCR2_STRIDE, 1}))};

    int failures = 0;
    auto compare = [&](const char* what, const std::vector<float>& a,
                       const std::vector<float>& b, float tol) {
        double worst = 0.0;
        int64_t worst_i = 0;
        for (size_t i = 0; i < a.size(); i++) {
            const double d = std::abs((double)a[i] - (double)b[i]);
            const double rel = d / (1e-6 + std::abs((double)a[i]));
            if (std::min(d, rel) > worst) {
                worst = std::min(d, rel);
                worst_i = (int64_t)i;
            }
        }
        const bool ok = worst <= tol;
        if (!ok) {
            failures++;
            int shown = 0;
            for (size_t i = 0; i < a.size() && shown < 6; i++) {
                const double d = std::abs((double)a[i] - (double)b[i]);
                if (d / (1e-6 + std::abs((double)a[i])) > tol && d > tol) {
                    std::printf("      [%lld] fused %.9g  split %.9g\n",
                                (long long)i, a[i], b[i]);
                    shown++;
                }
            }
        }
        std::printf("    %-10s max err %.3e @ %lld  %s\n", what, worst,
                    (long long)worst_i, ok ? "ok" : "MISMATCH");
    };

    struct Cfg { int level; bool ctl; };
    const Cfg cfgs[] = {{0, false}, {0, true}, {1, false}, {1, true}};

    for (const Cfg& cfg : cfgs) {
        const bool level1 = cfg.level == 1;
        std::printf("level %d, color_trust_linear %d\n", cfg.level,
                    (int)cfg.ctl);
        backend::memset_sync(d_radii, 0, N * sizeof(float));

        // ---- fused path ----
        float* f_means = upload(means);
        float* f_quats = upload(quats);
        float* f_scales = upload(scales);
        float* f_opac = upload(opac);
        float* f_dc = upload(dc);
        float* f_sh = upload(sh);
        std::vector<DeviceTensorFloatND> splats = {
            DeviceTensorFloatND(ttv(f_means, {N, 3, 1})),
            DeviceTensorFloatND(ttv(f_quats, {N, 4, 1})),
            DeviceTensorFloatND(ttv(f_scales, {N, 3, 1})),
            DeviceTensorFloatND(ttv(f_opac, {N, 1, 1})),
            DeviceTensorFloatND(ttv(f_dc, {N, 3, 1})),
            DeviceTensorFloatND(ttv(f_sh, {N, NUM_SH * 3, 1})),
        };

        auto fwd = projection_3dgs_forward(
            N, MAX_DEG, splats, ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), W, H,
            "PINHOLE", dist_fixture::kTierNames[0],
            ttv(d_dist, {C, kCameraDistortionParams}), radii, std::nullopt,
            std::nullopt, (uint32_t)NUM_SH, 32, 256);
        DeviceTensor2D<uint2> aabb = std::get<0>(fwd);
        backend::device_synchronize();

        std::vector<DeviceTensorFloatND> v_world, g1_world, g2_world;
        for (int c = 0; c < 6; c++) {
            v_world.push_back(
                DeviceTensorFloatND(ttv(upload(vw[c]), {N, wch[c], 1})));
            g1_world.push_back(
                DeviceTensorFloatND(ttv(upload(g1[c]), {N, wch[c], 1})));
            g2_world.push_back(
                DeviceTensorFloatND(ttv(upload(g2[c]), {N, wch[c], 1})));
        }

        // Level 1 quantizes the Adam state (and, for SH, the values too).
        // Zero packed + zero bounds is the codec's valid initial state.
        const int64_t fpbo_cells = sh_fpbo_cells(N, NUM_SH);
        NonShQuantState f_non_sh{};
        std::optional<TorchTensorView> shq, shq_b, shv, shv_b;
        uint8_t* f_shv = nullptr;
        float* f_shv_b = nullptr;
        if (level1) {
            const int prims[5] = {3, 4, 3, 1, 3};
            uint8_t* pk[5];
            float* bd[5];
            for (int c = 0; c < 5; c++) {
                pk[c] = upload(std::vector<uint8_t>(N * prims[c] * 4, 0));
                bd[c] = upload(std::vector<float>(4 * NB, 0.f));
            }
            f_non_sh = {true, pk[0], pk[1], pk[2],  pk[3],
                        pk[4], (float4*)bd[0], (float4*)bd[1],
                        (float4*)bd[2], (float4*)bd[3], (float4*)bd[4]};
            uint8_t* f_shq = upload(std::vector<uint8_t>(fpbo_cells * 2, 0));
            float* f_shq_b = upload(std::vector<float>(4 * NB, 0.f));
            // Values start from the fp32 SH above, encoded against a bound
            // that covers their range; both paths then re-encode in place.
            std::vector<uint16_t> v16((size_t)fpbo_cells, 0);
            const ShQuantAddr fa = sh_quant_addr(NUM_SH, 0);
            for (int64_t i = 0; i < N; i++)
                for (int c = 0; c < 3 * NUM_SH; c++)
                    v16[(size_t)fa.cell(fa.base(i), c)] = (uint16_t)std::lround(
                        (sh[i * 3 * NUM_SH + c] + 0.5f) * 65535.0f);
            f_shv = (uint8_t*)upload(v16);
            std::vector<float> vb(2 * NB);
            for (int64_t b = 0; b < NB; b++) {
                vb[2 * b] = -0.5f;
                vb[2 * b + 1] = 0.5f;
            }
            f_shv_b = upload(vb);
            shq = ttv(f_shq, {fpbo_cells, 1});
            shq_b = ttv(f_shq_b, {NB, 4});
            shv = ttv(f_shv, {fpbo_cells, 1});
            shv_b = ttv(f_shv_b, {NB, 2});
        }

        float* f_score = upload(std::vector<float>(N, 0.f));
        fused_projection_bwd_optimizer_3dgs(
            N, MAX_DEG, splats, ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), W, H,
            "PINHOLE", dist_fixture::kTierNames[0],
            ttv(d_dist, {C, kCameraDistortionParams}), DeviceVector<int32_t>{},
            DeviceVector<int32_t>{}, aabb, v_world, v_screen, g1_world,
            g2_world, shq, shq_b, shv, shv_b, f_non_sh, SplatVisitState{}, radii,
            dv<float>(f_score, N), kLrMeans, kLrQuats,
            kLrScales, kLrOpacs, kLrDc, kLrSh, /*max_gauss_ratio=*/10.f,
            /*scale_reg=*/0.1f, /*mcmc_op=*/0.01f, /*mcmc_scale=*/0.01f,
            /*erank=*/0.1f, /*erank_s3=*/0.02f, /*quat_norm=*/0.01f, kDcReg,
            kShReg, /*max_screen_size=*/0.02f, /*max_screen_size_penalty=*/1.f,
            /*use_scale_agnostic_mean=*/false, cfg.ctl, kEpsTr, kStep,
            cfg.level);
        backend::device_synchronize();
        if (const char* err = backend::last_error()) {
            std::fprintf(stderr, "fpbo: %s\n", err);
            return 1;
        }

        // ---- non-fused path ----
        float* s_means = upload(means);
        float* s_quats = upload(quats);
        float* s_scales = upload(scales);
        float* s_opac = upload(opac);
        float* s_dc = upload(dc);
        float* s_sh = upload(sh);
        float* s_v[6];
        float* s_g1[6];
        float* s_g2[6];
        for (int c = 0; c < 6; c++) {
            s_v[c] = upload(vw[c]);
            s_g1[c] = upload(g1[c]);
            s_g2[c] = upload(g2[c]);
        }

        ColorTrustState color_trust{};
        color_trust.enabled = cfg.ctl;
        color_trust.eps_tr = kEpsTr;
        color_trust.features_dc = s_dc;
        color_trust.opacities = s_opac;

        NonShQuantState s_non_sh{};
        uint8_t* s_shq = nullptr;
        float* s_shq_b = nullptr;
        uint8_t* s_shv = nullptr;
        float* s_shv_b = nullptr;
        const int64_t cell_blocks = (SH_CELLS + 255) / 256;
        if (level1) {
            const int prims[5] = {3, 4, 3, 1, 3};
            uint8_t* pk[5];
            float* bd[5];
            for (int c = 0; c < 5; c++) {
                pk[c] = upload(std::vector<uint8_t>(N * prims[c] * 4, 0));
                bd[c] = upload(std::vector<float>(4 * NB, 0.f));
            }
            s_non_sh = {true, pk[0], pk[1], pk[2],  pk[3],
                        pk[4], (float4*)bd[0], (float4*)bd[1],
                        (float4*)bd[2], (float4*)bd[3], (float4*)bd[4]};
            s_shq = upload(std::vector<uint8_t>(SH_CELLS * 2, 0));
            s_shq_b = upload(std::vector<float>(4 * cell_blocks, 0.f));
            std::vector<uint16_t> v16((size_t)SH_CELLS);
            for (int64_t i = 0; i < SH_CELLS; i++)
                v16[(size_t)i] =
                    (uint16_t)std::lround((sh[i] + 0.5f) * 65535.0f);
            s_shv = (uint8_t*)upload(v16);
            std::vector<float> vb(2 * cell_blocks);
            for (int64_t b = 0; b < cell_blocks; b++) {
                vb[2 * b] = -0.5f;
                vb[2 * b + 1] = 0.5f;
            }
            s_shv_b = upload(vb);
        }

        float* s_score = upload(std::vector<float>(N, 0.f));
        fused_optim_3dgs_geometry(
            N, dv<float3>(s_means, N), dv<float3>(s_v[0], N),
            dv<float3>(s_g1[0], N), dv<float3>(s_g2[0], N),
            dv<float4>(s_quats, N), dv<float4>(s_v[1], N),
            dv<float4>(s_g1[1], N), dv<float4>(s_g2[1], N),
            dv<float3>(s_scales, N), dv<float3>(s_v[2], N),
            dv<float3>(s_g1[2], N), dv<float3>(s_g2[2], N),
            dv<float>(s_opac, N), dv<float>(s_v[3], N), dv<float>(s_g1[3], N),
            dv<float>(s_g2[3], N), dv<float3>(s_dc, N), dv<float3>(s_v[4], N),
            radii, dv<float>(s_score, N), kLrMeans, kLrQuats, kLrScales,
            kLrOpacs, kLrDc, /*max_gauss_ratio=*/10.f, /*scale_reg=*/0.1f,
            /*mcmc_op=*/0.01f, /*mcmc_scale=*/0.01f, /*erank=*/0.1f,
            /*erank_s3=*/0.02f, /*quat_norm=*/0.01f, kDcReg, kShReg,
            /*max_screen_size=*/0.02f, /*max_screen_size_penalty=*/1.f,
            /*use_scale_agnostic_mean=*/false, color_trust, s_non_sh,
            SplatVisitState{},
            GradQuantBuffers{}, kStep, DeviceVector<int32_t>(),
            /*grad_scale=*/1.f, /*zero_grad=*/false);

        // The engine's dispatch: quantized state folds DC into the geometry
        // kernel, fp32 splits it into the trust-region or plain Adam kernel.
        if (!level1) {
            if (cfg.ctl) {
                fused_adamtr_linear_rgb_optim(
                    ttv(s_dc, {N, 3}), ttv(s_v[4], {N, 3}),
                    ttv(s_g1[4], {N, 3}), ttv(s_g2[4], {N, 3}),
                    ttv(s_opac, {N, 1}), kLrDc, 0.9f, 0.999f, 1e-15f, kEpsTr,
                    kDcReg, kShReg, kStep, 1.f, false);
                fused_adamtr_linear_rgb_sh_optim(
                    ttv(s_sh, {N, NUM_SH, 3}), ttv(s_v[5], {N, NUM_SH, 3}),
                    ttv(s_g1[5], {N, NUM_SH, 3}), ttv(s_g2[5], {N, NUM_SH, 3}),
                    ttv(s_dc, {N, 3}), ttv(s_opac, {N, 1}), kLrSh, 0.9f,
                    0.999f, 1e-15f, kEpsTr, kShReg, kStep, 1.f, false);
            } else {
                // FPBO applies 2*dc_reg/3 per splat; fused_adam_step divides
                // l2_reg by the parameter count, so pre-multiply by N.
                fused_adam_step(N, DeviceTensorFloatND(ttv(s_dc, {N, 3, 1})),
                                DeviceTensorFloatND(ttv(s_v[4], {N, 3, 1})),
                                DeviceTensorFloatND(ttv(s_g1[4], {N, 3, 1})),
                                DeviceTensorFloatND(ttv(s_g2[4], {N, 3, 1})),
                                kLrDc, kStep, DeviceVector<int32_t>(),
                                kDcReg * (float)N, 0.5f / 0.28209479177387814f,
                                1.f, false);
                fused_adam_step(
                    N, DeviceTensorFloatND(ttv(s_sh, {N, NUM_SH * 3, 1})),
                    DeviceTensorFloatND(ttv(s_v[5], {N, NUM_SH * 3, 1})),
                    DeviceTensorFloatND(ttv(s_g1[5], {N, NUM_SH * 3, 1})),
                    DeviceTensorFloatND(ttv(s_g2[5], {N, NUM_SH * 3, 1})),
                    kLrSh, kStep, DeviceVector<int32_t>(), kShReg, 0.f, 1.f,
                    false);
            }
        } else {
            fused_adam_step_quantized_value(
                N, SH_CELLS,
                DeviceTensorFloatND(ttv(s_v[5], {N, NUM_SH * 3, 1})), nullptr,
                nullptr, s_shq, (float4*)s_shq_b, s_shv, (float2*)s_shv_b,
                kLrSh, kStep, DeviceVector<int32_t>(), kShReg, 0.f, 8, 16,
                color_trust, 1.f, false);
        }
        backend::device_synchronize();
        if (const char* err = backend::last_error()) {
            std::fprintf(stderr, "split: %s\n", err);
            return 1;
        }

        // ---- compare ----
        // Level 1 quantizes SH values against two different block layouts;
        // everything else is one exact step from the zero quantized state.
        const float sh_tol = level1 ? 1e-3f : 1e-4f;
        const float tol = 1e-4f;
        compare("means", download(f_means, N * 3), download(s_means, N * 3),
                tol);
        compare("quats", download(f_quats, N * 4), download(s_quats, N * 4),
                tol);
        compare("scales", download(f_scales, N * 3), download(s_scales, N * 3),
                tol);
        compare("opacities", download(f_opac, N), download(s_opac, N), tol);
        compare("features_dc", download(f_dc, N * 3), download(s_dc, N * 3),
                tol);
        compare("score", download(f_score, N), download(s_score, N), tol);
        if (!level1) {
            compare("features_sh", download(f_sh, SH_CELLS),
                    download(s_sh, SH_CELLS), sh_tol);
        } else {
            std::vector<uint16_t> fp((size_t)fpbo_cells), sp((size_t)SH_CELLS);
            backend::memcpy_sync(fp.data(), f_shv, fp.size() * 2,
                                 MemcpyKind::DeviceToHost);
            backend::memcpy_sync(sp.data(), s_shv, sp.size() * 2,
                                 MemcpyKind::DeviceToHost);
            std::vector<float> fb = download(f_shv_b, 2 * NB);
            std::vector<float> sb = download(s_shv_b, 2 * cell_blocks);
            const ShQuantAddr fa = sh_quant_addr(NUM_SH, 0);
            const ShQuantAddr sa = sh_quant_addr(NUM_SH, 256);
            std::vector<float> fv(SH_CELLS), sv(SH_CELLS);
            for (int64_t i = 0; i < N; i++)
                for (int c = 0; c < 3 * NUM_SH; c++) {
                    fv[i * 3 * NUM_SH + c] = decode_sh16(fp, fb, fa, i, c);
                    sv[i * 3 * NUM_SH + c] = decode_sh16(sp, sb, sa, i, c);
                }
            compare("features_sh", fv, sv, sh_tol);
        }
    }

    std::printf("fpbo_split_parity: %s\n", failures ? "FAILED" : "PASSED");
    return failures ? 1 : 0;
}
