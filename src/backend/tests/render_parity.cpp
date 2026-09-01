// Backend parity tool for the phase-3 forward pipeline: background SH,
// tile intersection, and rasterization. The SAME source builds under both
// backends (see projection_parity.cpp):
//
//   CUDA build:   ./render_parity dump ref.bin
//   Vulkan build: ./render_parity compare ref.bin   (per device)
//
// Sections:
//   1. Background SH forward: SH degree x (camera model, distortion tier).
//   2. Full pipeline (projection -> intersect -> rasterize), fused + packed,
//      per primitive, with dist-loss / median variants. Rendered images,
//      transmittances, and tile offsets are compared with tolerance; near-tie
//      depth keys may sort differently across backends (last-ulp projection
//      divergence), so a small violation fraction is allowed.

#include <backend/tests/DistortionFixture.h>
#include <kernels/background/BackgroundSphericalHarmonics.cuh>
#include <kernels/tile/IntersectTile.cuh>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/raster/RasterizationEval3DFwd.cuh>
#include <kernels/raster/RasterizationFwd.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

static constexpr int64_t N = 3000;
static constexpr uint32_t C = 2;
static constexpr uint32_t W = 200, H = 150;
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

void readback(std::vector<float>& acc, const float* d, int64_t n) {
    if (n == 0) return;
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

void readback_i32(std::vector<float>& acc, const int32_t* d, int64_t n) {
    if (n == 0) return;
    std::vector<int32_t> tmp(n);
    backend::memcpy_sync(tmp.data(), d, n * sizeof(int32_t),
                         MemcpyKind::DeviceToHost);
    for (int32_t v : tmp) acc.push_back((float)v);
}

int check_error() {
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }
    return 0;
}

// static: the engine library these tools link defines the same helper
// inline (engine/EngineCommon.h), and MSVC refuses the duplicate.
static DeviceTensor2D<uint2> vec_to_2d_aabb(const DeviceVector<uint2>& vec) {
    TorchTensorView tv{(uint64_t)vec.data_ptr(), (uint32_t)sizeof(unsigned),
                       {vec.size(), 1LL, 2LL}};
    return DeviceTensor2D<uint2>(tv);
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    // --- deterministic inputs (projection_parity's distributions) ---
    std::mt19937 rng(654321u);
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
    std::vector<float> intr = {150, 152, 100, 75, 145, 146, 97, 78};
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
                   {(int64_t)C, kCameraDistortionParams});
    };

    std::vector<float> acc;

    // === 1. Background SH forward ===
    {
        std::vector<float> bg_sh(25 * 3);
        for (auto& v : bg_sh) v = uf(-0.6f, 0.6f);
        float* d_bg_sh = upload(bg_sh);
        float* d_bg_out =
            (float*)backend::device_malloc((size_t)C * H * W * 3 * 4);
        for (int si = 0; si < 3; si++)          // degrees 0, 2, 4
            for (int ci = 0; ci < 4; ci++) {
                const int shd = 2 * si;
                // PINHOLE has four compiled tiers and only three slots here,
                // so start it at OpenCV; the NONE path is covered by the
                // other models.
                const int tier =
                    dist_fixture::kTiers[ci][(ci == 0 ? si + 1 : si) %
                                             dist_fixture::kNumTiers[ci]];
                render_background_sh_forward(
                    W, H, cams[ci], dist_fixture::kTierNames[tier], shd,
                    ttv(d_vm, {(int64_t)C, 16}),
                    ttv(d_intr, {(int64_t)C, 4}), dist_tv(tier),
                    ttv(d_bg_sh, {(shd + 1) * (shd + 1), 3}),
                    ttv(d_bg_out, {(int64_t)C, H, W, 3}));
                backend::device_synchronize();
                if (check_error()) return 1;
                readback(acc, d_bg_out, (int64_t)C * H * W * 3);
            }
        backend::device_free(d_bg_sh);
        backend::device_free(d_bg_out);
    }

    // === 2. Projection -> intersect -> rasterize pipeline ===
    struct Cfg {
        int prim;      // 0 = 3dgs, 1 = mip, 2 = 3dgut
        bool packed;
        int cam;
        int dist;      // distortion tier, index into dist_fixture::kTierNames
        DistortionType dt;
        bool median;
    };
    // Only the 3dgut rasterizer reads the camera, so its rows carry the tier
    // sweep; for 3dgs/mip the tier reaches the projection alone.
    const Cfg cfgs[] = {
        {0, false, 0, 1, DistortionType::RGB_D, true},
        {1, false, 0, 0, DistortionType::RGB_D, true},
        {2, false, 0, 1, DistortionType::RGB_D, true},
        {2, false, 0, 2, DistortionType::D, false},
        {0, false, 1, 2, DistortionType::D, false},
        {2, false, 3, 0, DistortionType::None, true},
        {0, true, 3, 0, DistortionType::None, false},
        {1, true, 0, 3, DistortionType::D, false},
        {2, true, 0, 3, DistortionType::None, false},
    };

    for (const Cfg& cfg : cfgs) {
        backend::memset_sync(d_radii, 0, N * sizeof(float));

        // --- projection ---
        std::vector<DeviceTensorFloatND> splats_s;
        DeviceVector<int32_t> cam_ids, gauss_ids;
        DeviceTensor2D<uint2> aabb_2d;       // non-packed (or packed view)
        DeviceVector<uint2> aabb_vec;        // packed
        DeviceTensorFloatND depths_nd;

        if (cfg.packed) {
            auto fn = cfg.prim == 0   ? projection_3dgs_packed_forward
                      : cfg.prim == 1 ? projection_mip_packed_forward
                                      : projection_3dgut_packed_forward;
            auto out = fn(N, 3, in_splats, ttv(d_vm, {(int64_t)C, 16}),
                          ttv(d_intr, {(int64_t)C, 4}), W, H, cams[cfg.cam],
                          dist_fixture::kTierNames[cfg.dist],
                          dist_tv(cfg.dist), radii, std::nullopt,
                          std::nullopt, 0, 32, 0);
            cam_ids = std::get<0>(out);
            gauss_ids = std::get<1>(out);
            aabb_vec = std::get<2>(out);
            auto depths_vec = std::get<3>(out);
            splats_s = std::get<4>(out);
            aabb_2d = vec_to_2d_aabb(aabb_vec);
            depths_nd = DeviceTensorFloatND(depths_vec);
            acc.push_back((float)cam_ids.size());  // nnz
        } else {
            auto fn = cfg.prim == 0   ? projection_3dgs_forward
                      : cfg.prim == 1 ? projection_mip_forward
                                      : projection_3dgut_forward;
            auto out = fn(N, 3, in_splats, ttv(d_vm, {(int64_t)C, 16}),
                          ttv(d_intr, {(int64_t)C, 4}), W, H, cams[cfg.cam],
                          dist_fixture::kTierNames[cfg.dist],
                          dist_tv(cfg.dist), radii, std::nullopt,
                          std::nullopt, 0, 32, 0);
            aabb_2d = std::get<0>(out);
            auto depths_2d = std::get<1>(out);
            splats_s = std::get<2>(out);
            depths_nd = DeviceTensorFloatND(depths_2d);
        }
        backend::device_synchronize();
        if (check_error()) return 1;

        // --- tile intersection (ellipse mode) ---
        DeviceVector<int32_t>* image_ids_ptr =
            cfg.packed && cam_ids.data_ptr() ? &cam_ids : nullptr;

        int macro_log2 = kMacroLog2Default;

        auto [isect_ids, flatten_ids, tile_offsets] = do_intersect_tile_generic(
            aabb_2d, depths_nd, ellipse_view(splats_s, cfg.prim == 2), C,
            ttv(d_intr, {(int64_t)C, 4}), W, H, image_ids_ptr, /*tile_active=*/nullptr,
        macro_log2);
        backend::device_synchronize();
        if (check_error()) return 1;

        acc.push_back((float)flatten_ids.size());  // n_isects
        readback_i32(acc, tile_offsets.data_ptr(),
                     tile_offsets.size<0>() * tile_offsets.size<1>() *
                         tile_offsets.size<2>());

        // --- rasterization forward ---
        std::tuple<RenderOutput::TensorTuple, DeviceTensor3D<float>,
                   DeviceTensor3D<int32_t>, RenderOutput::TensorTuple,
                   DeviceTensor3D<float>>
            rout;
        if (cfg.prim == 2) {
            rout = rasterize_to_pixels_3dgut_fwd(
                N, in_splats, splats_s, gauss_ids,
                ttv(d_vm, {(int64_t)C, 16}), ttv(d_intr, {(int64_t)C, 4}),
                cams[cfg.cam], dist_fixture::kTierNames[cfg.dist],
                dist_tv(cfg.dist), aabb_2d, W, H,
                tile_offsets, flatten_ids, macro_log2, cfg.dt,
                cfg.median);
        } else {
            auto fn = cfg.prim == 0 ? rasterize_to_pixels_3dgs_fwd
                                    : rasterize_to_pixels_mip_fwd;
            rout = fn(N, in_splats, splats_s, gauss_ids, W, H, tile_offsets,
                      flatten_ids, macro_log2, cfg.dt, cfg.median);
        }
        backend::device_synchronize();
        if (check_error()) return 1;

        auto& renders = std::get<0>(rout);
        auto& render_Ts = std::get<1>(rout);
        auto& distortions = std::get<3>(rout);
        auto& render_median = std::get<4>(rout);

        const int64_t n_pix = (int64_t)C * H * W;
        readback(acc, (const float*)std::get<0>(renders).data_ptr(),
                 n_pix * 3);
        readback(acc, std::get<1>(renders).data_ptr(), n_pix);
        readback(acc, render_Ts.data_ptr(), n_pix);
        if (cfg.median)
            readback(acc, render_median.data_ptr(), n_pix);
        if (dist_has_rgb(cfg.dt))
            readback(acc, (const float*)std::get<0>(distortions).data_ptr(),
                     n_pix * 3);
        if (dist_has_depth(cfg.dt))
            readback(acc, std::get<1>(distortions).data_ptr(), n_pix);
        readback(acc, d_radii, N);
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t n = (int64_t)acc.size();
        f.write((const char*)&n, sizeof(n));
        f.write((const char*)acc.data(), acc.size() * sizeof(float));
        std::printf("render_parity: dumped %lld floats to %s\n",
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

    // Same tolerance model as projection_parity; the violation-fraction cap
    // additionally absorbs near-tie depth-key order flips in the sorted
    // blend (both backends see valid but different orders).
    int64_t violations = 0;
    double max_abs = 0;
    for (int64_t i = 0; i < n; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 5e-4 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) violations++;
    }
    double frac = (double)violations / (double)n;
    std::printf("render_parity: %lld floats, max_abs %.3g, "
                "violations %lld (%.5f%%)\n",
                (long long)n, max_abs, (long long)violations, 100.0 * frac);
    bool pass = frac <= 1e-3;
    std::printf(pass ? "render_parity: PASSED\n" : "render_parity: FAILED\n");
    return pass ? 0 : 1;
}
