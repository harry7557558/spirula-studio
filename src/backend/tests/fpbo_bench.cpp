// Timing harness for the fused projection-backward + optimizer, the kernel
// that dominates a training step (~2/3 of GPU time at cap_max splats).
// Builds the same buffer set the engine hands it at quantization_level 1 and
// times R back-to-back calls, so a kernel change can be A/B'd in seconds
// instead of a 30k-step run. Same source on both backends.
//
//   ./fpbo_bench [num_splats] [iters]

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/optim/FusedProjectionBwdOptim.cuh>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

namespace {

constexpr int NUM_SH = 15;  // sh_degree 3
constexpr uint32_t W = 1237, H = 822;

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

int check_error(const char* where) {
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error (%s): %s\n", where, err);
        return 1;
    }
    return 0;
}

}  // namespace

static DeviceTensor2D<uint2> vec_to_2d_aabb(const DeviceVector<uint2>& vec) {
    TorchTensorView tv{(uint64_t)vec.data_ptr(), (uint32_t)sizeof(unsigned),
                       {vec.size(), 1LL, 2LL}};
    return DeviceTensor2D<uint2>(tv);
}


int main(int argc, char** argv) {
    const int64_t N = (argc > 1 ? std::atoll(argv[1]) : 5000000) / 256 * 256;
    const int iters = argc > 2 ? std::atoi(argv[2]) : 20;
    const int64_t C = 1;
    const int64_t NB = (N + 255) / 256;
    const int64_t cells = sh_fpbo_cells(N, NUM_SH);

    std::mt19937 rng(20260825u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    // A slab of splats in front of the camera; roughly a third project
    // inside the frame, which is the visible fraction a mip-NeRF 360 scene
    // sees at cap_max.
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3);
    for (int64_t i = 0; i < N; i++) {
        means[3 * i] = uf(-6.f, 6.f);
        means[3 * i + 1] = uf(-4.f, 4.f);
        means[3 * i + 2] = uf(-2.f, 14.f);
        for (int k = 0; k < 4; k++) quats[4 * i + k] = uf(-1.f, 1.f);
        for (int k = 0; k < 3; k++) scales[3 * i + k] = uf(-5.f, -2.5f);
        opac[i] = uf(-3.f, 5.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = uf(-0.5f, 1.5f);
    }
    std::vector<float> vm = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::vector<float> intr = {900.f, 900.f, W * 0.5f, H * 0.5f};
    std::vector<float> dist = dist_fixture::distortion_rows(C);

    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));
    backend::memset_sync(d_radii, 0, N * sizeof(float));
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));

    std::vector<DeviceTensorFloatND> splats = {
        DeviceTensorFloatND(ttv(upload(means), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(quats), {N, 4, 1})),
        DeviceTensorFloatND(ttv(upload(scales), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(opac), {N, 1, 1})),
        DeviceTensorFloatND(ttv(upload(dc), {N, 3, 1})),
        DeviceTensorFloatND(ttv(nullptr, {N, NUM_SH * 3, 1})),
    };

    // Quantized SH state + values (the level-1 canonical storage).
    std::vector<uint8_t> shq(cells * 2, 0);
    std::vector<uint8_t> shv(cells * 2);
    for (auto& v : shv) v = (uint8_t)(rng() & 0xff);
    std::vector<float> shq_b(4 * NB, 0.f), shv_b(2 * NB);
    for (int64_t b = 0; b < NB; b++) {
        shv_b[2 * b] = -0.3f;
        shv_b[2 * b + 1] = 0.3f;
    }
    uint8_t* d_shq = upload(shq);
    uint8_t* d_shv = upload(shv);
    float* d_shq_b = upload(shq_b);
    float* d_shv_b = upload(shv_b);
    shq.clear(); shq.shrink_to_fit();
    shv.clear(); shv.shrink_to_fit();
    std::optional<TorchTensorView> shq_tv = ttv(d_shq, {cells, 1});
    std::optional<TorchTensorView> shq_b_tv = ttv(d_shq_b, {NB, 4});
    std::optional<TorchTensorView> shv_tv = ttv(d_shv, {cells, 1});
    std::optional<TorchTensorView> shv_b_tv = ttv(d_shv_b, {NB, 2});

    auto out = projection_3dgs_packed_forward(
        N, 3, splats, ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), W, H,
        "PINHOLE", dist_fixture::kTierNames[0],
        ttv(d_dist + dist_fixture::row_offset(0, C),
            {C, kCameraDistortionParams}),
        radii, shv_tv, shv_b_tv, (uint32_t)NUM_SH, 16, 0);
    DeviceVector<int32_t> cam_ids = std::get<0>(out);
    DeviceVector<int32_t> gauss_ids = std::get<1>(out);
    DeviceTensor2D<uint2> aabb_2d = vec_to_2d_aabb(std::get<2>(out));
    const int64_t n_isect = cam_ids.size();
    backend::device_synchronize();
    if (check_error("fwd")) return 1;
    std::printf("N %lld  intersections %lld (%.1f%% visible)\n",
                (long long)N, (long long)n_isect, 100.0 * n_isect / (double)N);

    std::vector<float> vxy(n_isect * 2), vd(n_isect), vc(n_isect * 3),
        vo(n_isect), vr(n_isect * 3);
    for (auto& v : vxy) v = uf(-0.5f, 0.5f);
    for (auto& v : vd) v = uf(-0.2f, 0.2f);
    for (auto& v : vc) v = uf(-0.02f, 0.02f);
    for (auto& v : vo) v = uf(-0.5f, 0.5f);
    for (auto& v : vr) v = uf(-1.f, 1.f);
    auto rows = interleave_screen(n_isect, SCR2_STRIDE,
        {{SCR2_XY, &vxy}, {SCR2_DEPTH, &vd}, {SCR2_CONIC, &vc},
         {SCR2_OPAC, &vo}, {SCR2_RGB, &vr}});
    std::vector<DeviceTensorFloatND> v_screen = {
        DeviceTensorFloatND(ttv(upload(rows), {n_isect, SCR2_STRIDE, 1}))};

    // 3dgs leaves every v_splats_world slot null (raster_bwd never writes
    // the world buffer for it) -- as does the engine.
    std::vector<DeviceTensorFloatND> v_world(6), g1(6), g2(6);
    const int wch[6] = {3, 4, 3, 1, 3, 3 * NUM_SH};
    for (int c = 0; c < 6; c++) {
        v_world[c] = DeviceTensorFloatND(ttv(nullptr, {N, wch[c], 1}));
        g1[c] = DeviceTensorFloatND(ttv(nullptr, {N, wch[c], 1}));
        g2[c] = DeviceTensorFloatND(ttv(nullptr, {N, wch[c], 1}));
    }

    NonShQuantState non_sh{};
    const int prims[5] = {3, 4, 3, 1, 3};
    std::vector<uint8_t*> nq_packed(5);
    std::vector<float*> nq_bounds(5);
    for (int c = 0; c < 5; c++) {
        nq_packed[c] = (uint8_t*)backend::device_malloc(N * prims[c] * 4);
        backend::memset_sync(nq_packed[c], 0, N * prims[c] * 4);
        nq_bounds[c] = (float*)backend::device_malloc(4 * NB * 4);
        backend::memset_sync(nq_bounds[c], 0, 4 * NB * 4);
    }
    non_sh.enabled = true;
    non_sh.means_packed = nq_packed[0];
    non_sh.quats_packed = nq_packed[1];
    non_sh.scales_packed = nq_packed[2];
    non_sh.opacities_packed = nq_packed[3];
    non_sh.features_dc_packed = nq_packed[4];
    non_sh.means_bounds = (float4*)nq_bounds[0];
    non_sh.quats_bounds = (float4*)nq_bounds[1];
    non_sh.scales_bounds = (float4*)nq_bounds[2];
    non_sh.opacities_bounds = (float4*)nq_bounds[3];
    non_sh.features_dc_bounds = (float4*)nq_bounds[4];

    std::vector<int32_t> st(N, 500);
    int32_t* d_steps = upload(st);
    st.clear(); st.shrink_to_fit();
    float* d_ds = (float*)backend::device_malloc(N * sizeof(float));
    DeviceVector<float> densify_score(ttv(d_ds, {N, 1}));

    // The Vulkan launcher sorts gaussian_ids in place; keep a pristine copy
    // so every timed call sees the same input.
    int32_t* d_gauss_ref =
        (int32_t*)backend::device_malloc(n_isect * sizeof(int32_t));
    backend::memcpy_sync(d_gauss_ref, gauss_ids.data_ptr(),
                         n_isect * sizeof(int32_t),
                         MemcpyKind::DeviceToDevice);

    auto one_call = [&](int step) {
        backend::memcpy_sync(gauss_ids.data_ptr(), d_gauss_ref,
                             n_isect * sizeof(int32_t),
                             MemcpyKind::DeviceToDevice);
        fused_projection_bwd_optimizer_3dgs(
            N, 3, splats, ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), W, H,
            "PINHOLE", dist_fixture::kTierNames[0],
            ttv(d_dist + dist_fixture::row_offset(0, C),
                {C, kCameraDistortionParams}),
            cam_ids, gauss_ids, aabb_2d, v_world, v_screen, g1, g2, shq_tv,
            shq_b_tv, shv_tv, shv_b_tv, non_sh, radii, densify_score,
            1.28e-4f, 1.5e-3f, 0.02f, 0.025f, 5e-3f, 2.5e-4f, 10.f, 0.01f,
            0.01f, 0.01f, 0.f, 0.f, 0.f, 1e-3f, 1e-3f,
            /*max_screen_size=*/0.3f, /*max_screen_size_penalty=*/1.f,
            /*use_scale_agnostic_mean=*/true, /*color_trust_linear=*/false,
            1e-4f, ttv(d_steps, {N, 1}), /*quantization_level=*/1);
    };

    for (int i = 0; i < 3; i++) one_call(i);
    backend::device_synchronize();
    if (check_error("warmup")) return 1;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++) one_call(i);
    backend::device_synchronize();
    auto t1 = std::chrono::steady_clock::now();
    if (check_error("bench")) return 1;

    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
    std::printf("fpbo: %.3f ms/call  (%.1f Msplat/ms)\n", ms, N / ms / 1e6);
    return 0;
}
