// Fixed-input raster timing across footprint and tile-list distributions.
// raster_bench [N] [iters] [macro_log2] [cut] [scene] [2d|3dgut] [W] [H] [dist] [dump|-] [depth_grad]

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/tile/IntersectTile.cuh>
#include <kernels/raster/RasterizationFwd.cuh>
#include <kernels/raster/RasterizationBwd.cuh>
#include <kernels/raster/RasterizationEval3DFwd.cuh>
#include <kernels/raster/RasterizationEval3DBwd.cuh>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

namespace {

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

DeviceTensor2D<uint2> vec_to_2d_aabb(const DeviceVector<uint2>& vec) {
    TorchTensorView tv{(uint64_t)vec.data_ptr(), (uint32_t)sizeof(unsigned),
                       {vec.size(), 1LL, 2LL}};
    return DeviceTensor2D<uint2>(tv);
}

template <typename F>
double time_ms(int iters, F body) {
    for (int i = 0; i < 3; i++) body();
    backend::device_synchronize();
    auto warm = std::chrono::steady_clock::now();
    do {
        for (int i = 0; i < 8; i++) body();
        backend::device_synchronize();
    } while (std::chrono::steady_clock::now() - warm < std::chrono::milliseconds(100));
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++) body();
    backend::device_synchronize();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
}

}  // namespace

int main(int argc, char** argv) {
    const int64_t N = argc > 1 ? std::atoll(argv[1]) : 2000000;
    const int iters = argc > 2 ? std::atoi(argv[2]) : 20;
    int macro_log2 = argc > 3 ? std::atoi(argv[3]) : kMacroLog2Default;
    const float cut = argc > 4 ? (float)std::atof(argv[4]) : 0.0f;
    const char* scene = argc > 5 ? argv[5] : "mixed";
    const bool eval3d = argc > 6 && std::strcmp(argv[6], "3dgut") == 0;
    const uint32_t W = argc > 7 ? (uint32_t)std::atoi(argv[7]) : 1237u;
    const uint32_t H = argc > 8 ? (uint32_t)std::atoi(argv[8]) : 822u;
    const DistortionType dt = argc > 9 ? (DistortionType)std::atoi(argv[9]) : DistortionType::None;
    const bool depth_grad = argc <= 11 || std::atoi(argv[11]) != 0;
    const int64_t C = 1;

    std::mt19937 rng(20260903u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    // A slab in front of the camera, scales spread so the tile lists carry the
    // mix of large and small splats a real scene does.
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
        const bool tiny = std::strcmp(scene, "tiny") == 0;
        const bool large = std::strcmp(scene, "large") == 0;
        if (tiny || large) {
            means[3 * i + 2] = uf(large ? 8.f : 3.f, large ? 24.f : 16.f);
            for (int k = 0; k < 3; ++k)
                scales[3 * i + k] = uf(large ? -3.f : -8.f, large ? -1.5f : -5.5f);
        } else if (std::strcmp(scene, "heavy_tail") == 0) {
            means[3 * i + 2] = uf(3.f, 24.f);
            const float u = uf(0.f, 1.f);
            for (int k = 0; k < 3; ++k)
                scales[3 * i + k] = u < 0.8f ? uf(-8.f, -5.5f)
                    : u < 0.98f ? uf(-5.f, -3.f) : uf(-3.f, -1.5f);
        } else if (std::strcmp(scene, "clustered") == 0 && i % 10 != 0) {
            means[3 * i] = uf(-0.7f, 0.7f);
            means[3 * i + 1] = uf(-0.5f, 0.5f);
            means[3 * i + 2] = uf(3.f, 16.f);
        } else if (std::strcmp(scene, "transparent") == 0) {
            means[3 * i + 2] = uf(3.f, 16.f);
            opac[i] = uf(-7.f, -3.f);
        }
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
        DeviceTensorFloatND(ttv(nullptr, {N, 3, 1})),
    };
    means.clear(); means.shrink_to_fit();
    quats.clear(); quats.shrink_to_fit();
    scales.clear(); scales.shrink_to_fit();
    dc.clear(); dc.shrink_to_fit();

    auto dist_tv = ttv(d_dist + dist_fixture::row_offset(0, C),
                       {C, kCameraDistortionParams});
    auto project = eval3d ? projection_3dgut_packed_forward : projection_3dgs_packed_forward;
    auto proj = project(
        N, ColorBasis{}, splats, ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), W, H, "PINHOLE",
        dist_fixture::kTierNames[0], dist_tv, radii, std::nullopt,
        std::nullopt, 0, 32, 0);
    DeviceVector<int32_t> cam_ids = std::get<0>(proj);
    DeviceVector<int32_t> gauss_ids = std::get<1>(proj);
    DeviceTensor2D<uint2> aabb_2d = vec_to_2d_aabb(std::get<2>(proj));
    DeviceTensorFloatND depths(std::get<3>(proj));
    std::vector<DeviceTensorFloatND> splats_s = std::get<4>(proj);
    const int64_t n_visible = cam_ids.size();
    backend::device_synchronize();
    if (check_error("projection")) return 1;

    auto [isect_ids, flatten_ids, tile_offsets] = do_intersect_tile_generic(
        aabb_2d, depths, ellipse_view(splats_s, eval3d, cut), (uint32_t)C,
        ttv(d_intr, {C, 4}), W, H, &cam_ids, nullptr, macro_log2);
    const int64_t n_isects = flatten_ids.size();
    backend::device_synchronize();
    if (check_error("intersect")) return 1;
    std::printf("N %lld  visible %lld  intersections %lld  (%.1f per pixel, "
                "macro_log2 %d, kernel cut %g)\n",
                (long long)N, (long long)n_visible, (long long)n_isects,
                (double)n_isects / (double)(W * H), macro_log2, cut);
    const size_t tiles = (size_t)tile_offsets.numel();
    std::vector<int32_t> lists(tiles);
    backend::memcpy_sync(lists.data(), tile_offsets.data_ptr(), tiles * sizeof(int32_t), MemcpyKind::DeviceToHost);
    for (size_t i = 0; i < tiles; ++i)
        lists[i] = (i + 1 < tiles ? lists[i + 1] : (int32_t)n_isects) - lists[i];
    std::sort(lists.begin(), lists.end());
    std::printf("scene %s primitive %s dist %d tile-list p50/p90/p99/max %d/%d/%d/%d\n",
        scene, eval3d ? "3dgut" : "2d", (int)dt,
        lists[tiles / 2], lists[tiles * 9 / 10], lists[tiles * 99 / 100], lists.back());

    const int64_t n_pix = (int64_t)C * H * W;
    std::vector<float> h(n_pix * 3);
    for (auto& v : h) v = uf(-1.f, 1.f);
    float* d_v_rgb = upload(h);
    h.resize(n_pix);
    for (auto& v : h) v = uf(-0.2f, 0.2f);
    float* d_v_depth = upload(h);
    for (auto& v : h) v = uf(-0.5f, 0.5f);
    float* d_v_T = upload(h);
    for (auto& v : h) v = uf(0.f, 1.f);
    float* d_awmap = upload(h);
    h.clear(); h.shrink_to_fit();

    auto t3f3 = [&](float* p) {
        return DeviceTensor3D<float3>(ttv(p, {C, H, W, 3}));
    };
    auto t3f1 = [&](float* p) {
        return DeviceTensor3D<float>(ttv(p, {C, H, W, 1}));
    };
    RenderOutput::TensorTuple v_renders{t3f3(d_v_rgb), depth_grad ? t3f1(d_v_depth) : DeviceTensor3D<float>{},
                                        DeviceTensor3D<float3>{}};

    const DensifyAccumMode aw = DensifyAccumMode::Avg;

    auto fwd = [&] {
        if (eval3d)
            return rasterize_to_pixels_3dgut_fwd(N, splats, splats_s, gauss_ids,
                ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), "PINHOLE",
                dist_fixture::kTierNames[0], dist_tv, aabb_2d, W, H,
                tile_offsets, flatten_ids, macro_log2, dt, false, true, SplatScoreRender{});
        return rasterize_to_pixels_3dgs_fwd(N, splats, splats_s, gauss_ids, W,
                                            H, tile_offsets, flatten_ids,
                                            macro_log2, dt, false, true,
                                            SplatScoreRender{}, cut);
    };
    auto rout = fwd();
    backend::device_synchronize();
    if (check_error("raster fwd")) return 1;

    const double fwd_ms = time_ms(iters, [&] { (void)fwd(); });
    if (check_error("fwd bench")) return 1;

    // Binning: key generation, the radix sort over (tile, depth) and the
    // per-tile offsets. Timed here because the sort is the other kernel whose
    // cost the training loop cannot measure reproducibly.
    const double isect_ms = time_ms(iters, [&] {
        int m = macro_log2;
        (void)do_intersect_tile_generic(aabb_2d, depths,
                                        ellipse_view(splats_s, eval3d, cut),
                                        (uint32_t)C, ttv(d_intr, {C, 4}), W, H,
                                        &cam_ids, nullptr, m);
    });
    if (check_error("isect bench")) return 1;

    // --dead-criterion max_weight adds the per-splat peak alpha * T output.
    float* d_mw = (float*)backend::device_malloc(N * sizeof(float));
    backend::memset_sync(d_mw, 0, N * sizeof(float));
    std::vector<DeviceTensorFloatND> v_world, v_screen;
    DeviceVector<float> v_weight;
    auto bwd = [&](DeviceVector<float> max_weight) {
        std::optional<RenderOutput::TensorTuple> dist_fwd, v_dist;
        if (dist_any(dt)) { dist_fwd = std::get<3>(rout); v_dist = RenderOutput::TensorTuple{t3f3(d_v_rgb), t3f1(d_v_depth), DeviceTensor3D<float3>{}}; }
        if (eval3d) {
            auto result = rasterize_to_pixels_3dgut_bwd(N, splats, splats_s, gauss_ids,
                ttv(d_vm, {C, 16}), ttv(d_intr, {C, 4}), "PINHOLE",
                dist_fixture::kTierNames[0], dist_tv, aabb_2d, W, H,
                tile_offsets, flatten_ids, macro_log2, std::get<1>(rout), std::get<2>(rout),
                std::get<0>(rout), dist_fwd, dt, DeviceTensor3D<float>{}, t3f1(d_awmap), aw,
                max_weight, 1.0f, v_renders, t3f1(d_v_T), DeviceTensor3D<float>{},
                v_dist, 0.f, std::nullopt, std::nullopt, false);
            v_world = std::get<0>(result);
            v_screen = std::get<1>(result);
            v_weight = std::get<3>(result);
            return;
        }
        auto result = rasterize_to_pixels_3dgs_bwd(
            N, splats, splats_s, gauss_ids, W, H, tile_offsets, flatten_ids,
            macro_log2, std::get<1>(rout), std::get<2>(rout),
            std::get<0>(rout), dist_fwd, dt, t3f1(d_awmap), aw,
            max_weight, 1.0f, cut, v_renders,
            t3f1(d_v_T), DeviceTensor3D<float>{}, v_dist, 0.0f,
            std::nullopt, std::nullopt);
        v_world = std::get<0>(result);
        v_screen = std::get<1>(result);
        v_weight = std::get<2>(result);
    };
    const double bwd_ms = time_ms(iters, [&] { bwd({}); });
    const double bwd_mw_ms =
        time_ms(iters, [&] { bwd(DeviceVector<float>(ttv(d_mw, {N, 1}))); });
    if (check_error("bwd bench")) return 1;

    std::printf("raster fwd: %8.3f ms/call\nraster bwd: %8.3f ms/call\n"
                "  + max wt: %8.3f ms/call\ntile isect: %8.3f ms/call\n",
                fwd_ms, bwd_ms, bwd_mw_ms, isect_ms);
    if (argc > 10 && std::strcmp(argv[10], "-") != 0) {
        std::ofstream f(argv[10], std::ios::binary);
        auto dump = [&](const float* ptr, size_t n) {
            std::vector<float> values(n);
            backend::memcpy_sync(values.data(), ptr, n * sizeof(float), MemcpyKind::DeviceToHost);
            f.write(reinterpret_cast<const char*>(values.data()), n * sizeof(float));
        };
        for (const auto& t : v_world) if (t.data_ptr()) dump(t.data_ptr(), t.numel());
        for (const auto& t : v_screen) if (t.data_ptr()) dump(t.data_ptr(), t.numel());
        dump(v_weight.data_ptr(), v_weight.size());
        dump(d_mw, N);
        if (!f || check_error("dump")) return 1;
    }
    return 0;
}
