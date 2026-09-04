// Backend parity tool for the projection-forward launch API. The SAME
// source builds under both backends (it only touches backend:: + the launch
// functions + Tensor.h):
//
//   CUDA build:   ./projection_parity dump ref.bin
//   Vulkan build: ./projection_parity compare ref.bin   (per device)
//
// Deterministic inputs; the configs are {3DGS, MIP, 3DGUT} x 4 camera models
// x SH degree {0, 3} x the distortion tiers compiled for that model.
// Comparison is tolerance-based (fast-math exp/sqrt chains differ across
// compilers) with a small allowance for borderline-cull flips, which change
// entire rows.

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

static constexpr int64_t N = 5000;
static constexpr uint32_t C = 2;
static constexpr uint32_t W = 800, H = 600;
static constexpr int NUM_SH = 15;  // degree 3

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

// The AABB rides as 16-bit fixed point (core/AabbQuant.cuh); compare the
// quantized edges themselves, so a mismatch reads as whole steps.
void readback_aabb(std::vector<float>& acc, const uint2* d, int64_t n) {
    if (d == nullptr || n == 0) return;
    std::vector<uint2> q((size_t)n);
    backend::memcpy_sync(q.data(), d, q.size() * sizeof(uint2),
                         backend::MemcpyKind::DeviceToHost);
    size_t o = acc.size();
    acc.resize(o + (size_t)n * 4);
    for (int64_t i = 0; i < n; i++) {
        acc[o + i * 4 + 0] = (float)(q[(size_t)i].x & 0xffffu);
        acc[o + i * 4 + 1] = (float)(q[(size_t)i].y & 0xffffu);
        acc[o + i * 4 + 2] = (float)(q[(size_t)i].x >> 16);
        acc[o + i * 4 + 3] = (float)(q[(size_t)i].y >> 16);
    }
}

void readback(std::vector<float>& acc, const float* d, int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    // --- deterministic inputs ---
    std::mt19937 rng(123456u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3), sh(N * NUM_SH * 3);
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

    // Cameras: identity+z-offset and a mild y-rotation. Row-major [R|t].
    const float cy_ = std::cos(0.2f), sy_ = std::sin(0.2f);
    std::vector<float> vm = {
        1, 0, 0, 0,   0, 1, 0, 0,    0, 0, 1, 4,     0, 0, 0, 1,
        cy_, 0, sy_, 0.3f,  0, 1, 0, -0.2f,  -sy_, 0, cy_, 5.f,  0, 0, 0, 1,
    };
    std::vector<float> intr = {600, 610, 400, 300, 580, 585, 390, 310};
    std::vector<float> dist = dist_fixture::distortion_rows(C);

    float* d_means = upload(means);
    float* d_quats = upload(quats);
    float* d_scales = upload(scales);
    float* d_opac = upload(opac);
    float* d_dc = upload(dc);
    float* d_sh = upload(sh);
    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));

    // TorchTensorView convention: trailing dim x element_size == sizeof(T).
    std::vector<DeviceTensorFloatND> in_splats = {
        DeviceTensorFloatND(ttv(d_means, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_quats, {N, 4, 1})),
        DeviceTensorFloatND(ttv(d_scales, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_opac, {N, 1, 1})),
        DeviceTensorFloatND(ttv(d_dc, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_sh, {N, NUM_SH * 3, 1})),
    };
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));

    const char* cams[4] = {"PINHOLE", "FISHEYE", "EQUISOLID",
                           "EQUIRECTANGULAR"};

    auto dist_tv = [&](int tier) {
        return ttv(d_dist + dist_fixture::row_offset(tier, C),
                   {C, kCameraDistortionParams});
    };

    std::vector<float> acc;
    for (int prim = 0; prim < 3; prim++)
        for (int ci = 0; ci < 4; ci++)
            for (int shd = 0; shd <= 3; shd += 3)
                // The tier is orthogonal to SH degree: sweep the compiled
                // tiers at degree 3 and keep degree 0 on the NONE fast path,
                // which also runs with a null coefficient tensor.
                for (int t = 0; t < (shd ? dist_fixture::kNumTiers[ci] : 1);
                     t++) {
                    const int tier = dist_fixture::kTiers[ci][t];
                    backend::memset_sync(d_radii, 0, N * sizeof(float));
                    auto fn = prim == 0   ? projection_3dgs_forward
                              : prim == 1 ? projection_mip_forward
                                          : projection_3dgut_forward;
                    auto out = fn(
                        N, shd, in_splats, ttv(d_vm, {C, 16}),
                        ttv(d_intr, {C, 4}), W, H, cams[ci],
                        dist_fixture::kTierNames[tier],
                        shd ? dist_tv(tier)
                            : ttv(nullptr, {C, kCameraDistortionParams}),
                        radii, std::nullopt, std::nullopt, 0, 32, 0);
                    backend::device_synchronize();
                    if (const char* err = backend::last_error()) {
                        std::fprintf(stderr, "backend error: %s\n", err);
                        return 1;
                    }
                    auto& aabb = std::get<0>(out);
                    auto& depths = std::get<1>(out);
                    auto& screen = std::get<2>(out);
                    readback_aabb(acc, aabb.data_ptr(), (int64_t)C * N);
                    readback(acc, depths.data_ptr(), C * N);
                    readback(acc, d_radii, N);
                    if (prim == 2)
                        readback_screen(acc, screen[0], SCRG_STRIDE,
                                        SCREEN_ROWS_GUT_COMPS);
                    else
                        readback_screen(acc, screen[0], SCR2_STRIDE,
                                        SCREEN_ROWS_2D_COMPS);
                }

    // --- SH value-quant (q8/q16) fused configs ---
    // Packed cells + bounds for both layouts (docs/notes/sh-quant-layout.md);
    // features_sh becomes a shape-only null descriptor, as in the engine.
    const int64_t cells = sh_fpbo_cells(N, NUM_SH);
    std::vector<uint8_t> q8(cells);
    std::vector<uint16_t> q16(cells);
    for (auto& v : q8) v = (uint8_t)(rng() & 0xff);
    for (auto& v : q16) v = (uint16_t)(rng() & 0xffff);
    auto gen_bounds = [&](int64_t stride) {
        int64_t nb = (cells + stride - 1) / stride;
        std::vector<float> b(2 * nb);
        for (int64_t i = 0; i < nb; i++) {
            b[2 * i] = uf(-0.5f, 0.1f);
            b[2 * i + 1] = b[2 * i] + uf(0.05f, 0.8f);
        }
        return b;
    };
    std::vector<float> bounds_cell = gen_bounds(256);
    std::vector<float> bounds_fpbo =
        gen_bounds(sh_quant_addr(NUM_SH, 0).bounds_stride);
    uint8_t* d_q8 = upload(q8);
    uint16_t* d_q16 = upload(q16);
    float* d_bcell = upload(bounds_cell);
    float* d_bfpbo = upload(bounds_fpbo);

    std::vector<DeviceTensorFloatND> in_splats_q = in_splats;
    in_splats_q[5] = DeviceTensorFloatND(ttv(nullptr, {N, NUM_SH * 3, 1}));

    auto quant_args = [&](int bits, int fpbo) {
        TorchTensorView packed_tv =
            ttv(bits == 8 ? (const void*)d_q8 : (const void*)d_q16,
                {cells, 1});
        TorchTensorView bounds_tv = ttv(
            fpbo ? d_bfpbo : d_bcell,
            {(int64_t)(fpbo ? bounds_fpbo : bounds_cell).size() / 2, 2});
        return std::make_pair(packed_tv, bounds_tv);
    };

    int qcfg = 0;
    for (int prim = 0; prim < 3; prim++)
        for (int bits = 8; bits <= 16; bits += 8)
            for (int fpbo = 0; fpbo < 2; fpbo++, qcfg++) {
                // Value-quant and distortion are independent axes, so rotate
                // the tier through the codec configs instead of crossing them.
                const int tier = dist_fixture::kTiers[prim]
                                     [qcfg % dist_fixture::kNumTiers[prim]];
                backend::memset_sync(d_radii, 0, N * sizeof(float));
                auto fn = prim == 0   ? projection_3dgs_forward
                          : prim == 1 ? projection_mip_forward
                                      : projection_3dgut_forward;
                auto [packed_tv, bounds_tv] = quant_args(bits, fpbo);
                auto out = fn(
                    N, 3, in_splats_q, ttv(d_vm, {C, 16}),
                    ttv(d_intr, {C, 4}), W, H, cams[prim],
                    dist_fixture::kTierNames[tier], dist_tv(tier), radii,
                    packed_tv, bounds_tv,
                    (uint32_t)NUM_SH, bits, fpbo ? 0 : 256);
                backend::device_synchronize();
                if (const char* err = backend::last_error()) {
                    std::fprintf(stderr, "backend error (quant): %s\n", err);
                    return 1;
                }
                auto& aabb = std::get<0>(out);
                auto& depths = std::get<1>(out);
                auto& screen = std::get<2>(out);
                readback_aabb(acc, aabb.data_ptr(), (int64_t)C * N);
                readback(acc, depths.data_ptr(), C * N);
                readback(acc, d_radii, N);
                if (prim == 2)
                    readback_screen(acc, screen[0], SCRG_STRIDE,
                                    SCREEN_ROWS_GUT_COMPS);
                else
                    readback_screen(acc, screen[0], SCR2_STRIDE,
                                    SCREEN_ROWS_2D_COMPS);
            }

    // --- packed projection: nnz-compacted outputs (ids exact via float) ---
    for (int prim = 0; prim < 3; prim++)
        for (int ci = 0; ci < 4; ci += 3) {  // PINHOLE + EQUIRECTANGULAR
            // PINHOLE walks the three distorted tiers across the primitives;
            // EQUIRECTANGULAR has no lens distortion.
            const int tier = ci == 0 ? 1 + prim : 0;
            backend::memset_sync(d_radii, 0, N * sizeof(float));
            auto fn = prim == 0   ? projection_3dgs_packed_forward
                      : prim == 1 ? projection_mip_packed_forward
                                  : projection_3dgut_packed_forward;
            auto out = fn(N, 3, in_splats, ttv(d_vm, {C, 16}),
                          ttv(d_intr, {C, 4}), W, H, cams[ci],
                          dist_fixture::kTierNames[tier], dist_tv(tier),
                          radii, std::nullopt, std::nullopt, 0, 32, 0);
            backend::device_synchronize();
            if (const char* err = backend::last_error()) {
                std::fprintf(stderr, "backend error: %s\n", err);
                return 1;
            }
            auto& cam_ids = std::get<0>(out);
            auto& gauss_ids = std::get<1>(out);
            auto& aabb = std::get<2>(out);
            auto& depths = std::get<3>(out);
            auto& screen = std::get<4>(out);
            int64_t nnz = cam_ids.size();
            acc.push_back((float)nnz);
            std::vector<int32_t> ids(2 * nnz);
            backend::memcpy_sync(ids.data(), cam_ids.data_ptr(),
                                 nnz * 4, MemcpyKind::DeviceToHost);
            backend::memcpy_sync(ids.data() + nnz, gauss_ids.data_ptr(),
                                 nnz * 4, MemcpyKind::DeviceToHost);
            for (int32_t v : ids) acc.push_back((float)v);
            readback_aabb(acc, aabb.data_ptr(), nnz);
            readback(acc, depths.data_ptr(), nnz);
            readback(acc, d_radii, N);
            if (prim == 2)
                readback_screen(acc, screen[0], SCRG_STRIDE,
                                SCREEN_ROWS_GUT_COMPS);
            else
                readback_screen(acc, screen[0], SCR2_STRIDE,
                                SCREEN_ROWS_2D_COMPS);
        }

    // --- packed + quant: one q8 cell-block and one q16 FPBO config ---
    for (int k = 0; k < 2; k++) {
        const int prim = k == 0 ? 0 : 2;
        const int bits = k == 0 ? 8 : 16;
        const int fpbo = k;
        // PINHOLE rational, then FISHEYE thin-prism.
        const int tier = k == 0 ? 3 : 2;
        backend::memset_sync(d_radii, 0, N * sizeof(float));
        auto fn = prim == 0 ? projection_3dgs_packed_forward
                            : projection_3dgut_packed_forward;
        auto [packed_tv, bounds_tv] = quant_args(bits, fpbo);
        auto out = fn(N, 3, in_splats_q, ttv(d_vm, {C, 16}),
                      ttv(d_intr, {C, 4}), W, H, cams[k],
                      dist_fixture::kTierNames[tier], dist_tv(tier), radii,
                      packed_tv, bounds_tv,
                      (uint32_t)NUM_SH, bits, fpbo ? 0 : 256);
        backend::device_synchronize();
        if (const char* err = backend::last_error()) {
            std::fprintf(stderr, "backend error (packed quant): %s\n", err);
            return 1;
        }
        auto& cam_ids = std::get<0>(out);
        auto& gauss_ids = std::get<1>(out);
        auto& aabb = std::get<2>(out);
        auto& depths = std::get<3>(out);
        auto& screen = std::get<4>(out);
        int64_t nnz = cam_ids.size();
        acc.push_back((float)nnz);
        std::vector<int32_t> ids(2 * nnz);
        backend::memcpy_sync(ids.data(), cam_ids.data_ptr(), nnz * 4,
                             MemcpyKind::DeviceToHost);
        backend::memcpy_sync(ids.data() + nnz, gauss_ids.data_ptr(), nnz * 4,
                             MemcpyKind::DeviceToHost);
        for (int32_t v : ids) acc.push_back((float)v);
        readback_aabb(acc, aabb.data_ptr(), nnz);
        readback(acc, depths.data_ptr(), nnz);
        readback(acc, d_radii, N);
        if (prim == 2)
            readback_screen(acc, screen[0], SCRG_STRIDE,
                            SCREEN_ROWS_GUT_COMPS);
        else
            readback_screen(acc, screen[0], SCR2_STRIDE,
                            SCREEN_ROWS_2D_COMPS);
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t n = (int64_t)acc.size();
        f.write((const char*)&n, sizeof(n));
        f.write((const char*)acc.data(), acc.size() * sizeof(float));
        std::printf("projection_parity: dumped %lld floats to %s\n",
                    (long long)n, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t n = 0;
    f.read((char*)&n, sizeof(n));
    if (n != (int64_t)acc.size()) {
        std::fprintf(stderr, "size mismatch: ref %lld vs got %zu\n",
                     (long long)n, acc.size());
        return 1;
    }
    std::vector<float> ref(n);
    f.read((char*)ref.data(), n * sizeof(float));

    // Tolerances: |d| <= 5e-3 + 5e-4 |ref| covers fast-math divergence on
    // pixel-scale values; borderline-cull flips (one side culls, the other
    // doesn't) show up as gross row differences and are capped by count.
    int64_t violations = 0;
    double max_abs = 0, max_rel = 0;
    for (int64_t i = 0; i < n; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 5e-4 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (ref[i] != 0.f)
            max_rel = std::max(max_rel, d / std::fabs((double)ref[i]));
        if (d > tol) violations++;
    }
    double frac = (double)violations / (double)n;
    std::printf("projection_parity: %lld floats, max_abs %.3g, "
                "violations %lld (%.5f%%)\n",
                (long long)n, max_abs, (long long)violations, 100.0 * frac);
    bool pass = frac <= 2e-4;  // borderline-cull allowance
    std::printf(pass ? "projection_parity: PASSED\n"
                     : "projection_parity: FAILED\n");
    return pass ? 0 : 1;
}
