// Raster backward parity from real projection/intersection/forward state.
// CUDA: raster_bwd_parity dump ref.bin; Vulkan: compare ref.bin.
// Atomic accumulation order varies; comparisons cap tolerance violations.

#include <backend/tests/DistortionFixture.h>
#include <kernels/tile/IntersectTile.cuh>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/raster/RasterizationEval3DFwd.cuh>
#include <kernels/raster/RasterizationFwd.cuh>
#include <kernels/raster/RasterizationBwd.cuh>
#include <kernels/raster/RasterizationEval3DBwd.cuh>

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
static constexpr int NUM_SH = 15;

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
    if (n == 0 || d == nullptr) return;
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
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

    std::mt19937 rng(112233u);
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

    // Deterministic per-pixel output gradients (shared across configs).
    const int64_t n_pix = (int64_t)C * H * W;
    std::vector<float> h_v_rgb(n_pix * 3), h_v_depth(n_pix), h_v_T(n_pix),
        h_v_med(n_pix), h_v_drgb(n_pix * 3), h_v_dd(n_pix), h_awmap(n_pix);
    for (auto& v : h_v_rgb) v = uf(-1.f, 1.f);
    for (auto& v : h_v_depth) v = uf(-0.2f, 0.2f);
    for (auto& v : h_v_T) v = uf(-0.5f, 0.5f);
    for (auto& v : h_v_med) v = uf(-0.1f, 0.1f);
    for (auto& v : h_v_drgb) v = uf(-0.05f, 0.05f);
    for (auto& v : h_v_dd) v = uf(-0.05f, 0.05f);
    for (auto& v : h_awmap) v = uf(0.f, 1.f);
    float* d_v_rgb = upload(h_v_rgb);
    float* d_v_depth = upload(h_v_depth);
    float* d_v_T = upload(h_v_T);
    float* d_v_med = upload(h_v_med);
    float* d_v_drgb = upload(h_v_drgb);
    float* d_v_dd = upload(h_v_dd);
    float* d_awmap = upload(h_awmap);
    // Peak-weight seed: some splats keep it, the rest are raised past it.
    std::vector<float> h_mw(N);
    for (auto& v : h_mw) v = uf(0.f, 0.3f);
    float* d_mw = upload(h_mw);
    const float kMwScale = 1.25f;

    auto t3f3 = [&](float* p) {
        return DeviceTensor3D<float3>(ttv(p, {(int64_t)C, H, W, 3}));
    };
    auto t3f1 = [&](float* p) {
        return DeviceTensor3D<float>(ttv(p, {(int64_t)C, H, W, 1}));
    };
    RenderOutput::TensorTuple v_renders{t3f3(d_v_rgb), t3f1(d_v_depth),
                                        DeviceTensor3D<float3>{}};
    RenderOutput::TensorTuple v_dists{t3f3(d_v_drgb), t3f1(d_v_dd),
                                      DeviceTensor3D<float3>{}};
    // The RGB distortion gradient as the loss hands it over: a fixed multiple
    // of the depth one, either spelled out or left for the kernel to derive.
    const float kDistRatio = 0.6f;
    std::vector<float> h_v_drgb_tied(n_pix * 3);
    for (int64_t i = 0; i < n_pix; i++)
        for (int c = 0; c < 3; c++) h_v_drgb_tied[i * 3 + c] = h_v_dd[i] * kDistRatio;
    float* d_v_drgb_tied = upload(h_v_drgb_tied);
    RenderOutput::TensorTuple v_dists_tied{t3f3(d_v_drgb_tied), t3f1(d_v_dd),
                                           DeviceTensor3D<float3>{}};
    RenderOutput::TensorTuple v_dists_derived{DeviceTensor3D<float3>{},
                                              t3f1(d_v_dd),
                                              DeviceTensor3D<float3>{}};
    int derived_failures = 0;

    std::vector<float> acc;

    struct Cfg {
        int prim;  // 0 = 3dgs, 2 = 3dgut
        bool packed;
        int cam;
        int dist;  // distortion tier, index into dist_fixture::kTierNames
        DistortionType dt;
        bool median;
        DensifyAccumMode aw;
        bool vmg;  // 3dgut viewmat grad
        bool mw;   // peak-weight output
        float cut; // 3dgs kernel_cut: Primitive3DGS::kernel_vis
        bool depth_grad = true;
    };
    // The 3dgs backward never sees the camera, so the tier sweep lives on the
    // 3dgut rows; the 3dgs rows carry it through the projection only.
    const Cfg cfgs[] = {
        {0, false, 0, 0, DistortionType::None, false, DensifyAccumMode::None, false, true, 0.0f},
        {0, false, 0, 1, DistortionType::RGB_D, true, DensifyAccumMode::Max, false, false, 0.0f},
        {0, true, 1, 2, DistortionType::D, false, DensifyAccumMode::Sum, false, true, 0.0f},
        {0, true, 1, 2, DistortionType::D, false, DensifyAccumMode::Avg, false, false, 0.0f},
        {0, false, 0, 0, DistortionType::None, false, DensifyAccumMode::Sum, false, true, 4.0f},
        {0, true, 1, 1, DistortionType::D, false, DensifyAccumMode::Avg, false, false, 4.0f},
        {2, false, 0, 0, DistortionType::None, false, DensifyAccumMode::None, true, false, 0.0f},
        {2, false, 1, 2, DistortionType::D, true, DensifyAccumMode::Max, false, true, 0.0f},
        {2, false, 0, 1, DistortionType::RGB_D, false, DensifyAccumMode::Sum, false, false, 0.0f},
        {2, true, 0, 2, DistortionType::D, false, DensifyAccumMode::Avg, false, true, 0.0f},
        {0, false, 0, 0, DistortionType::None, false, DensifyAccumMode::None, false, true, 0.0f, false},
        {0, true, 1, 1, DistortionType::None, false, DensifyAccumMode::Avg, false, true, 4.0f, false},
        {2, false, 0, 0, DistortionType::None, false, DensifyAccumMode::None, true, false, 0.0f, false},
        {2, true, 1, 2, DistortionType::None, false, DensifyAccumMode::Avg, false, true, 0.0f, false},
        {0, true, 1, 1, DistortionType::RGB_D, true, DensifyAccumMode::Max, false, false, 0.0f, false},
        {2, false, 0, 1, DistortionType::D, true, DensifyAccumMode::Sum, true, true, 0.0f, false},
        {0, false, 0, 0, DistortionType::None, true, DensifyAccumMode::None, false, false, 0.0f, false},
        {2, false, 0, 0, DistortionType::None, true, DensifyAccumMode::None, true, false, 0.0f, false},
    };
    const char* cams[4] = {"PINHOLE", "FISHEYE", "EQUISOLID",
                           "EQUIRECTANGULAR"};
    auto dist_tv = [&](int tier) {
        return ttv(d_dist + dist_fixture::row_offset(tier, C),
                   {(int64_t)C, kCameraDistortionParams});
    };

    for (const Cfg& cfg : cfgs) {
        auto v_render_cfg = v_renders;
        if (!cfg.depth_grad) std::get<1>(v_render_cfg) = DeviceTensor3D<float>{};
        backend::memset_sync(d_radii, 0, N * sizeof(float));

        // --- projection ---
        std::vector<DeviceTensorFloatND> splats_s;
        DeviceVector<int32_t> cam_ids, gauss_ids;
        DeviceTensor2D<uint2> aabb_2d;
        DeviceVector<uint2> aabb_vec;
        DeviceTensorFloatND depths_nd;
        if (cfg.packed) {
            auto fn = cfg.prim == 0 ? projection_3dgs_packed_forward
                                    : projection_3dgut_packed_forward;
            auto out = fn(N, ColorBasis{3, 0}, in_splats, ttv(d_vm, {(int64_t)C, 16}),
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
        } else {
            auto fn = cfg.prim == 0 ? projection_3dgs_forward
                                    : projection_3dgut_forward;
            auto out = fn(N, ColorBasis{3, 0}, in_splats, ttv(d_vm, {(int64_t)C, 16}),
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

        // --- tile intersection ---
        DeviceVector<int32_t>* image_ids_ptr =
            cfg.packed && cam_ids.data_ptr() ? &cam_ids : nullptr;
        int macro_log2 = kMacroLog2Default;
        auto [isect_ids, flatten_ids, tile_offsets] =
            do_intersect_tile_generic(aabb_2d, depths_nd,
                                      ellipse_view(splats_s, cfg.prim == 2, cfg.cut), C,
                                      ttv(d_intr, {(int64_t)C, 4}), W, H,
                                      image_ids_ptr, /*tile_active=*/nullptr,
                                      macro_log2);
        backend::device_synchronize();
        if (check_error()) return 1;

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
                cfg.median, true, SplatScoreRender{});
        } else {
            rout = rasterize_to_pixels_3dgs_fwd(N, in_splats, splats_s,
                                                gauss_ids, W, H, tile_offsets,
                                                flatten_ids,
                                                macro_log2, cfg.dt,
                                                cfg.median, true,
                                                SplatScoreRender{}, cfg.cut);
        }
        backend::device_synchronize();
        if (check_error()) return 1;

        auto& renders = std::get<0>(rout);
        auto& render_Ts = std::get<1>(rout);
        auto& last_ids = std::get<2>(rout);
        auto& distortions = std::get<3>(rout);

        std::optional<RenderOutput::TensorTuple> dist_fwd_opt, v_dist_opt;
        if (dist_any(cfg.dt)) {
            dist_fwd_opt = distortions;
            v_dist_opt = v_dists;
        }
        DeviceTensor3D<float> awmap_t = accum_any(cfg.aw)
            ? t3f1(d_awmap) : DeviceTensor3D<float>{};
        DeviceTensor3D<float> v_med_t =
            cfg.median ? t3f1(d_v_med) : DeviceTensor3D<float>{};
        DeviceVector<float> mw_t;
        if (cfg.mw) {
            backend::memcpy_sync(d_mw, h_mw.data(), N * sizeof(float),
                                 MemcpyKind::HostToDevice);
            mw_t = DeviceVector<float>(ttv(d_mw, {N, 1}));
        }

        // --- rasterization backward ---
        if (cfg.prim == 2) {
            auto [v_w, v_s, v_viewmats, aw_out] =
                rasterize_to_pixels_3dgut_bwd(
                    N, in_splats, splats_s, gauss_ids,
                    ttv(d_vm, {(int64_t)C, 16}),
                    ttv(d_intr, {(int64_t)C, 4}), cams[cfg.cam],
                    dist_fixture::kTierNames[cfg.dist], dist_tv(cfg.dist),
                    aabb_2d, W, H,
                    tile_offsets, flatten_ids, macro_log2,
                    render_Ts, last_ids, renders,
                    dist_fwd_opt, cfg.dt, DeviceTensor3D<float>{}, awmap_t,
                    cfg.aw, mw_t, kMwScale, v_render_cfg, t3f1(d_v_T), v_med_t,
                    v_dist_opt, 0.0f, std::nullopt, std::nullopt, cfg.vmg);
            backend::device_synchronize();
            if (check_error()) return 1;
            for (int c = 0; c < 3; c++)  // v means/quats/scales
                readback(acc, v_w[c].data_ptr(), v_w[c].numel());
            readback_screen(acc, v_s[0], SCRG_STRIDE,  // opac, rgb
                            {{SCRG_OPAC, 1}, {SCRG_RGB, 3}});
            if (cfg.vmg)
                readback(acc, v_viewmats.data_ptr(), (int64_t)C * 16);
            if (accum_any(cfg.aw))
                readback(acc, aw_out.data_ptr(), N * accum_lanes(cfg.aw));
            if (cfg.mw) readback(acc, d_mw, N);
        } else {
            auto [v_w, v_s, aw_out] = rasterize_to_pixels_3dgs_bwd(
                N, in_splats, splats_s, gauss_ids, W, H, tile_offsets,
                flatten_ids, macro_log2, render_Ts, last_ids,
                renders, dist_fwd_opt,
                cfg.dt, awmap_t, cfg.aw, mw_t, kMwScale, cfg.cut, v_render_cfg,
                t3f1(d_v_T), v_med_t, v_dist_opt, 0.0f,
                std::nullopt, std::nullopt);
            backend::device_synchronize();
            if (check_error()) return 1;
            readback_screen(acc, v_s[0], SCR2_STRIDE,
                            {{SCR2_XY, 2}, {SCR2_DEPTH, 1}, {SCR2_CONIC, 3},
                             {SCR2_OPAC, 1}, {SCR2_RGB, 3}});
            if (accum_any(cfg.aw))
                readback(acc, aw_out.data_ptr(), N * accum_lanes(cfg.aw));
            if (cfg.mw) readback(acc, d_mw, N);

            if (cfg.dt == DistortionType::RGB_D) {
                std::vector<float> got[2];
                for (int k = 0; k < 2; k++) {
                    auto [w2, s2, a2] = rasterize_to_pixels_3dgs_bwd(
                        N, in_splats, splats_s, gauss_ids, W, H, tile_offsets,
                        flatten_ids, macro_log2, render_Ts, last_ids,
                        renders, dist_fwd_opt, cfg.dt, awmap_t, cfg.aw,
                        DeviceVector<float>{}, 1.0f, cfg.cut,
                        v_render_cfg, t3f1(d_v_T), v_med_t,
                        k == 0 ? v_dists_tied : v_dists_derived,
                        k == 0 ? 0.0f : kDistRatio, std::nullopt, std::nullopt);
                    backend::device_synchronize();
                    readback_screen(got[k], s2[0], SCR2_STRIDE,
                                    {{SCR2_XY, 2}, {SCR2_CONIC, 3},
                                     {SCR2_OPAC, 1}, {SCR2_RGB, 3}});
                }
                double worst = 0.0;
                for (size_t i = 0; i < got[0].size(); i++)
                    worst = std::max(worst, std::fabs((double)got[0][i] - got[1][i]) /
                                                (1e-6 + 1e-4 * std::fabs((double)got[0][i])));
                if (worst > 1.0) {
                    std::fprintf(stderr, "derived RGB distortion gradient differs "
                                 "(%.3g of tolerance)\n", worst);
                    derived_failures++;
                }
            }
        }
    }
    if (derived_failures) return 1;

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t n = (int64_t)acc.size();
        f.write((const char*)&n, sizeof(n));
        f.write((const char*)acc.data(), acc.size() * sizeof(float));
        std::printf("raster_bwd_parity: dumped %lld floats to %s\n",
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

    // Gradients are atomic accumulations over many tiles: base/relative
    // tolerance plus a violation cap absorbing accumulation-order noise and
    // near-tie sort flips upstream.
    int64_t violations = 0;
    double max_abs = 0;
    for (int64_t i = 0; i < n; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 1e-3 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) violations++;
    }
    double frac = (double)violations / (double)n;
    std::printf("raster_bwd_parity: %lld floats, max_abs %.3g, "
                "violations %lld (%.5f%%)\n",
                (long long)n, max_abs, (long long)violations, 100.0 * frac);
    bool pass = frac <= 2e-3;
    std::printf(pass ? "raster_bwd_parity: PASSED\n"
                     : "raster_bwd_parity: FAILED\n");
    return pass ? 0 : 1;
}
