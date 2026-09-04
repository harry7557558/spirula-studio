// Does the densification loss map reach the right splats? Cross-backend
// parity cannot answer that -- both backends share the indexing -- so this
// runs the real forward chain, plants known loss maps, and checks the
// per-splat accum_weight the raster backward hands to densify.
//
// Exit code 0 = all checks pass, 1 = a check failed.

#include <backend/tests/DistortionFixture.h>
#include <kernels/tile/IntersectTile.cuh>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/raster/RasterizationFwd.cuh>
#include <kernels/raster/RasterizationBwd.cuh>
#include <kernels/densify/Densify.cuh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

namespace {

constexpr int64_t N = 3000;
constexpr uint32_t C = 2;
constexpr uint32_t W = 200, H = 150;
constexpr int NUM_SH = 15;
// The asymmetric probe rectangle: a square one would not separate a
// transposed index from a correct one.
constexpr int RX0 = 10, RX1 = 45, RY0 = 100, RY1 = 140;

int g_fail = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); g_fail++; }
}

template <typename T> T* upload(const std::vector<T>& h) {
    T* d = (T*)backend::device_malloc(h.size() * sizeof(T));
    backend::memcpy_sync(d, h.data(), h.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}
TorchTensorView ttv(const void* p, std::vector<int64_t> s) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(s));
}

const char* mode_name(DensifyAccumMode m) {
    switch (m) {
        case DensifyAccumMode::Max: return "max";
        case DensifyAccumMode::Sum: return "sum";
        default:                    return "avg";
    }
}

static DeviceTensor2D<uint2> vec_to_2d_aabb(const DeviceVector<uint2>& vec) {
    TorchTensorView tv{(uint64_t)vec.data_ptr(), (uint32_t)sizeof(unsigned),
                       {vec.size(), 1LL, 2LL}};
    return DeviceTensor2D<uint2>(tv);
}


void run_mode(bool packed, DensifyAccumMode accum_mode) {
    std::printf("-- %s projection, accum=%s --\n",
                packed ? "packed" : "non-packed", mode_name(accum_mode));
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
    const float cy = std::cos(0.2f), sy = std::sin(0.2f);
    std::vector<float> vm = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 4, 0, 0, 0, 1,
                             cy, 0, sy, 0.3f, 0, 1, 0, -0.2f, -sy, 0, cy, 5.f,
                             0, 0, 0, 1};
    std::vector<float> intr = {150, 152, 100, 75, 145, 146, 97, 78};
    std::vector<float> dist = dist_fixture::distortion_rows(C);

    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));
    backend::memset_sync(d_radii, 0, N * sizeof(float));
    std::vector<DeviceTensorFloatND> in_splats = {
        DeviceTensorFloatND(ttv(upload(means), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(quats), {N, 4, 1})),
        DeviceTensorFloatND(ttv(upload(scales), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(opac), {N, 1, 1})),
        DeviceTensorFloatND(ttv(upload(dc), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(sh), {N, NUM_SH * 3, 1}))};
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));
    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    auto dist_tv = ttv(d_dist + dist_fixture::row_offset(0, C),
                       {(int64_t)C, kCameraDistortionParams});

    const int64_t n_pix = (int64_t)C * H * W;
    float* d_v_rgb = upload(std::vector<float>(n_pix * 3, 0.0f));
    std::vector<float> zeros1((size_t)n_pix, 0.0f);
    float* d_v_depth = upload(zeros1);
    float* d_v_T = upload(zeros1);
    float* d_awmap = upload(zeros1);
    auto t3f1 = [&](float* p) {
        return DeviceTensor3D<float>(ttv(p, {(int64_t)C, H, W, 1}));
    };
    RenderOutput::TensorTuple v_renders{
        DeviceTensor3D<float3>(ttv(d_v_rgb, {(int64_t)C, H, W, 3})),
        t3f1(d_v_depth), DeviceTensor3D<float3>{}};

    DeviceTensor2D<uint2> aabb_2d;
    DeviceTensorFloatND depths_nd;
    std::vector<DeviceTensorFloatND> splats_s;
    DeviceVector<int32_t> cam_ids, gauss_ids;
    DeviceVector<uint2> aabb_vec;
    if (packed) {
        auto out = projection_3dgs_packed_forward(
            N, 3, in_splats, ttv(d_vm, {(int64_t)C, 16}),
            ttv(d_intr, {(int64_t)C, 4}), W, H, "PINHOLE",
            dist_fixture::kTierNames[0], dist_tv, radii, std::nullopt,
            std::nullopt, 0, 32, 0);
        cam_ids = std::get<0>(out);
        gauss_ids = std::get<1>(out);
        aabb_vec = std::get<2>(out);
        aabb_2d = vec_to_2d_aabb(aabb_vec);
        splats_s = std::get<4>(out);
        depths_nd = DeviceTensorFloatND(std::get<3>(out));
    } else {
        auto out = projection_3dgs_forward(
            N, 3, in_splats, ttv(d_vm, {(int64_t)C, 16}),
            ttv(d_intr, {(int64_t)C, 4}), W, H, "PINHOLE",
            dist_fixture::kTierNames[0], dist_tv, radii, std::nullopt,
            std::nullopt, 0, 32, 0);
        aabb_2d = std::get<0>(out);
        splats_s = std::get<2>(out);
        depths_nd = DeviceTensorFloatND(std::get<1>(out));
    }
    DeviceVector<int32_t>* img_ids =
        (packed && cam_ids.data_ptr()) ? &cam_ids : nullptr;
    int macro_log2 = kMacroLog2Default;
    auto [isect_ids, flatten_ids, tile_offsets] = do_intersect_tile_generic(
        aabb_2d, depths_nd, ellipse_view(splats_s, false), C,
        ttv(d_intr, {(int64_t)C, 4}), W, H, img_ids, /*tile_active=*/nullptr,
        macro_log2);
    auto rout = rasterize_to_pixels_3dgs_fwd(N, in_splats, splats_s, gauss_ids,
                                             W, H, tile_offsets, flatten_ids,
                                             macro_log2,
                                             DistortionType::None, false);
    backend::device_synchronize();
    auto& renders = std::get<0>(rout);
    auto& render_Ts = std::get<1>(rout);
    auto& last_ids = std::get<2>(rout);

    // Screen centre per (camera, WORLD splat id): packed rows are scattered
    // back through gaussian_ids, which is the mapping under test.
    std::vector<float> xy((size_t)C * N * 2, -1e9f);
    {
        int64_t rows = packed ? (int64_t)gauss_ids.size() : (int64_t)C * N;
        std::vector<float> raw;
        readback_screen(raw, splats_s[0], SCR2_STRIDE, {{SCR2_XY, 2}});
        if (!packed) {
            xy = raw;
        } else {
            std::vector<int32_t> gid(rows), cid(rows);
            backend::memcpy_sync(gid.data(), gauss_ids.data_ptr(), rows * 4,
                                 MemcpyKind::DeviceToHost);
            backend::memcpy_sync(cid.data(), cam_ids.data_ptr(), rows * 4,
                                 MemcpyKind::DeviceToHost);
            for (int64_t j = 0; j < rows; j++) {
                int64_t k = (int64_t)cid[j] * N + gid[j];
                xy[2 * k] = raw[2 * j];
                xy[2 * k + 1] = raw[2 * j + 1];
            }
        }
    }

    auto run = [&](const std::vector<float>& map) {
        backend::memcpy_sync(d_awmap, map.data(), map.size() * 4,
                             MemcpyKind::HostToDevice);
        auto [vw, vs, aw] = rasterize_to_pixels_3dgs_bwd(
            N, in_splats, splats_s, gauss_ids, W, H, tile_offsets, flatten_ids,
            macro_log2,
            render_Ts, last_ids, renders, std::nullopt, DistortionType::None,
            t3f1(d_awmap), accum_mode, v_renders, t3f1(d_v_T),
            DeviceTensor3D<float>{}, std::nullopt, std::nullopt, std::nullopt);
        backend::device_synchronize();
        // Avg lands as [numerator, denominator] planar; the engine divides it
        // down with densify_accum_finalize_tensor once every camera is in.
        if (accum_mode == DensifyAccumMode::Avg)
            densify_accum_finalize_tensor(N, aw);
        backend::device_synchronize();
        std::vector<float> h(N);
        backend::memcpy_sync(h.data(), aw.data_ptr(), N * 4,
                             MemcpyKind::DeviceToHost);
        return h;
    };

    // 1) A rectangle in one image scores the splats that sit in it, under
    // that image's projection and no other -- the check that catches a
    // transposed pixel index or a dropped image offset.
    for (int cam = 0; cam < (int)C; cam++) {
        std::vector<float> map(n_pix, 0.f);
        for (int y = RY0; y < RY1; y++)
            for (int x = RX0; x < RX1; x++)
                map[(int64_t)cam * H * W + (int64_t)y * W + x] = 1.0f;
        auto aw = run(map);
        int hit[2][2] = {};  // [camera][swapped]
        int scored = 0;
        for (int64_t i = 0; i < N; i++) {
            if (!(aw[i] > 0.0f)) continue;
            scored++;
            for (int c = 0; c < 2; c++)
                for (int sw = 0; sw < 2; sw++) {
                    float sx = xy[2 * ((size_t)c * N + i)];
                    float sy = xy[2 * ((size_t)c * N + i) + 1];
                    if (sw) std::swap(sx, sy);
                    // A splat larger than the rectangle can be scored from
                    // outside it, so this is a majority test, not a bound.
                    if (sx >= RX0 - 8 && sx <= RX1 + 8 && sy >= RY0 - 8 &&
                        sy <= RY1 + 8)
                        hit[c][sw]++;
                }
        }
        std::printf("   image %d: %d scored; explained by cam0/cam1 = %d/%d,"
                    " transposed = %d/%d\n",
                    cam, scored, hit[0][0], hit[1][0], hit[0][1], hit[1][1]);
        check(scored > 20, "rectangle scored too few splats");
        check(hit[cam][0] > 0.8 * scored, "map did not reach the splats under it");
        check(hit[1 - cam][0] < 0.5 * scored, "map reached the wrong image");
        check(hit[cam][1] == 0, "pixel index looks transposed");
    }

    // 2) The value carried is the map value where the splat sits: an
    // all-ones map gives each splat its peak alpha*T, so the ratio against a
    // smooth map recovers the map at the splat centre.
    auto peak = run(std::vector<float>(n_pix, 1.0f));
    auto f = [&](float x, float y) {
        return 0.5f + 0.45f * std::sin(6.2831853f * x / (float)W) *
                          std::sin(6.2831853f * y / (float)H);
    };
    std::vector<float> smooth(n_pix);
    for (uint32_t c = 0; c < C; c++)
        for (uint32_t y = 0; y < H; y++)
            for (uint32_t x = 0; x < W; x++)
                smooth[((int64_t)c * H + y) * W + x] = f((float)x, (float)y);
    auto sm = run(smooth);
    int n = 0;
    double se = 0;
    for (int64_t i = 0; i < N; i++) {
        if (!(peak[i] > 1e-3f)) continue;
        float sx = xy[2 * i], sy = xy[2 * i + 1];
        if (sx < 4 || sy < 4 || sx > W - 5 || sy > H - 5) continue;
        double d = sm[i] / peak[i] - f(std::round(sx), std::round(sy));
        se += d * d;
        n++;
    }
    double rms = n ? std::sqrt(se / n) : 1.0;
    std::printf("   score/peak vs map at splat centre: n=%d rms=%.4f\n", n, rms);
    check(n > 500, "too few splats to judge the carried value");
    // 0.087 measured on both backends; the residual is splats wide enough
    // that their peak-weight pixel is not the centre.
    check(rms < 0.15, "score does not track the map value at the splat");

    // 3) Every mode is linear in the map: a constant c must give exactly c
    // times the all-ones score. Avg's all-ones score is 1 for any covered
    // splat, which is what makes it size-free.
    const float kConst = 0.7f;
    auto flat = run(std::vector<float>(n_pix, kConst));
    double worst_lin = 0.0, worst_one = 0.0;
    int checked = 0;
    for (int64_t i = 0; i < N; i++) {
        if (!(peak[i] > 1e-3f)) continue;
        checked++;
        worst_lin = std::max(worst_lin,
                             std::fabs(flat[i] - kConst * peak[i]) /
                                 (double)(kConst * peak[i]));
        worst_one = std::max(worst_one, (double)std::fabs(peak[i] - 1.0f));
    }
    std::printf("   constant map: n=%d worst |score - c*ones|/(c*ones) = %.2e\n",
                checked, worst_lin);
    check(checked > 500, "too few splats to judge linearity");
    check(worst_lin < 1e-4, "score is not linear in the map");
    if (accum_mode == DensifyAccumMode::Avg) {
        std::printf("   avg normalization: worst |ones_score - 1| = %.2e\n",
                    worst_one);
        check(worst_one < 1e-5, "avg is not normalized by the covered weight");
    }
}

}  // namespace

int main() {
    for (DensifyAccumMode m : {DensifyAccumMode::Max, DensifyAccumMode::Sum,
                               DensifyAccumMode::Avg}) {
        run_mode(false, m);
        run_mode(true, m);
    }
    std::printf(g_fail ? "accum_weight_map: FAILED (%d)\n"
                       : "accum_weight_map: PASSED (%d)\n", g_fail);
    return g_fail ? 1 : 0;
}
