// Self-checking: the eval3d binning and culls must not change the image
// (docs/notes/footprint.md). Every camera model renders random splats twice
// through the real forward rasterizer: binned and culled as the engine does,
// and with every splat in every tile under bounds that pass everything. The
// reference takes its visible set from the host, so a splat the projection
// wrongly culls shows up too.
//
//   ./footprint_exact          (either backend)
//   ./footprint_exact noslab   bins and culls on the boxes alone, and
//   ./footprint_exact nobox    on the footprint test alone, to tell which bound
//                              a failure comes from

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/raster/RasterizationEval3DFwd.cuh>
#include <kernels/tile/IntersectTile.cuh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "backend/tests/ScreenRows.h"
#include "shaders/aabb16.h"

using backend::MemcpyKind;

static constexpr int64_t N = 1500;
static constexpr uint32_t W = 256, H = 192;
// One 8-bit level: a fringe pixel at the alpha threshold moves less than this,
// a dropped tile row far more.
static constexpr float kTolerance = 1.0f / 255.0f;

template <typename T>
static T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T), MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
static std::vector<T> download(const T* d, size_t n) {
    std::vector<T> host(n);
    if (n) backend::memcpy_sync(host.data(), d, n * sizeof(T), MemcpyKind::DeviceToHost);
    return host;
}

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

struct View {
    const char* model;
    int tier;       // dist_fixture::kTierNames
    float fx, fy;
};

int main(int argc, char** argv) {
    const bool nobox = argc > 1 && std::string(argv[1]) == "nobox";
    const bool noslab = nobox || (argc > 1 && std::string(argv[1]) == "noslab");
    std::mt19937 rng(20261120u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    // Splats all around the camera, a tenth of them large enough to cross the
    // camera plane or to hold the camera, a fifth thin.
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N), dc(N * 3);
    for (int64_t i = 0; i < N; i++) {
        for (int k = 0; k < 3; k++) means[3 * i + k] = uf(-5.f, 5.f);
        for (int k = 0; k < 4; k++) quats[4 * i + k] = uf(-1.f, 1.f);
        const float size = i % 10 == 0 ? uf(-0.5f, 1.2f) : uf(-5.f, -1.f);
        for (int k = 0; k < 3; k++) scales[3 * i + k] = size + uf(-1.5f, 0.f);
        if (i % 5 == 1) scales[3 * i + (int)(i % 3)] = size - uf(3.f, 8.f);
        opac[i] = uf(-4.f, 6.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = uf(-0.5f, 1.5f);
    }
    for (int64_t i = 0; i < N; i++) {
        float n = 0.f;
        for (int k = 0; k < 4; k++) n += quats[4 * i + k] * quats[4 * i + k];
        n = n > 1e-6f ? 1.f / std::sqrt(n) : 0.f;
        for (int k = 0; k < 4; k++) quats[4 * i + k] = n > 0.f ? quats[4 * i + k] * n : (k == 0);
    }

    const float c20 = std::cos(0.2f), s20 = std::sin(0.2f);
    const std::vector<float> vm = {c20, 0, s20, 0.3f,  0, 1, 0, -0.2f,
                                   -s20, 0, c20, 0.5f,  0, 0, 0, 1};
    std::vector<DeviceTensorFloatND> in_splats = {
        DeviceTensorFloatND(ttv(upload(means), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(quats), {N, 4, 1})),
        DeviceTensorFloatND(ttv(upload(scales), {N, 3, 1})),
        DeviceTensorFloatND(ttv(upload(opac), {N, 1, 1})),
        DeviceTensorFloatND(ttv(upload(dc), {N, 3, 1})),
        DeviceTensorFloatND(ttv(nullptr, {N, 3, 1})),
    };
    float* d_vm = upload(vm);
    float* d_dist = upload(dist_fixture::distortion_rows(1));
    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));

    const float pi = 3.14159265f;
    const View views[] = {
        {"PINHOLE", 0, 150, 152},
        {"PINHOLE", 1, 150, 152},
        {"PINHOLE", 2, 150, 152},
        {"FISHEYE", 0, 75, 75},          // 230 degrees across the diagonal
        {"FISHEYE", 1, 75, 75},
        {"EQUISOLID", 0, 95, 95},
        {"EQUISOLID", 2, 95, 95},
        {"EQUIRECTANGULAR", 0, W / (2 * pi), H / pi},
        {"ORTHOGRAPHIC", 0, 30, 30},
    };

    const int macro_log2s[2] = {1, 3};
    bool ok = true;
    for (const View& view : views) for (int surfel = 0; surfel < 2; surfel++) {
        const std::vector<float> intr = {view.fx, view.fy, W * 0.5f + 1.5f, H * 0.5f - 2.5f};
        float* d_intr = upload(intr);
        const char* tier = dist_fixture::kTierNames[view.tier];
        auto dist_tv = ttv(d_dist + dist_fixture::row_offset(view.tier, 1),
                           {1, kCameraDistortionParams});
        auto vm_tv = ttv(d_vm, {1, 16});
        auto intr_tv = ttv(d_intr, {1, 4});
        const std::string model = view.model;

        backend::memset_sync(d_radii, 0, N * sizeof(float));
        auto proj = (surfel ? projection_2dgs_forward : projection_3dgut_forward)(
            N, ColorBasis{}, in_splats, vm_tv, intr_tv, W, H, model, tier, dist_tv, radii,
            std::nullopt, std::nullopt, 0, 32, 0);
        DeviceTensor2D<uint2> aabb = std::get<0>(proj);
        DeviceTensorFloatND depths(std::get<1>(proj));
        std::vector<DeviceTensorFloatND> splats_s = std::get<2>(proj);
        backend::device_synchronize();

        // --- the reference's rows, order and bounds ---
        std::vector<float> rows = download(splats_s[0].data_ptr(), (size_t)N * SCRG_STRIDE);
        if (noslab) {
            for (int64_t i = 0; i < N; i++)
                for (int k = 0; k < 3; k++) rows[i * SCRG_STRIDE + SCRG_SCALE + k] = 0.0f;
            backend::memcpy_sync((float*)splats_s[0].data_ptr(), rows.data(),
                                 rows.size() * sizeof(float), MemcpyKind::HostToDevice);
        }
        std::vector<uint2> box = download(aabb.data_ptr(), (size_t)N);
        if (nobox) {
            for (uint2& b : box)
                if (SS_AABB16_HI(b.x) > SS_AABB16_LO(b.x) && SS_AABB16_HI(b.y) > SS_AABB16_LO(b.y))
                    b = uint2{SS_AABB16_PACK(0u, 65535u), SS_AABB16_PACK(0u, 65535u)};
            backend::memcpy_sync((uint2*)aabb.data_ptr(), box.data(), box.size() * sizeof(uint2),
                                 MemcpyKind::HostToDevice);
        }
        std::vector<float> depth = download(depths.data_ptr(), (size_t)N);
        const bool planar = model == "PINHOLE" || model == "ORTHOGRAPHIC";
        std::vector<int32_t> order;
        int culled = 0;
        for (int64_t i = 0; i < N; i++) {
            const float* m = &means[3 * i];
            float p[3];
            for (int r = 0; r < 3; r++)
                p[r] = vm[4 * r] * m[0] + vm[4 * r + 1] * m[1] + vm[4 * r + 2] * m[2] + vm[4 * r + 3];
            const float o = 1.0f / (1.0f + std::exp(-opac[i]));
            // The projection culls on the centre alone, as every renderer does.
            if (o <= 1.0f / 255.0f || (planar && p[2] <= 0.0f)) continue;
            float* row = &rows[i * SCRG_STRIDE];
            const bool projected = SS_AABB16_HI(box[i].x) > SS_AABB16_LO(box[i].x) &&
                                   SS_AABB16_HI(box[i].y) > SS_AABB16_LO(box[i].y);
            if (!projected) {
                culled++;
                row[SCRG_OPAC] = o;
                row[SCRG_RGB] = row[SCRG_RGB + 1] = row[SCRG_RGB + 2] = 0.5f;
                depth[i] = planar ? p[2] : p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
            }
            row[SCRG_SCALE] = row[SCRG_SCALE + 1] = row[SCRG_SCALE + 2] = 0.0f;
            order.push_back((int32_t)i);
        }
        // The intersector's own order: depth, then splat index.
        std::stable_sort(order.begin(), order.end(), [&](int32_t a, int32_t b) {
            return std::fabs(depth[a]) < std::fabs(depth[b]);
        });
        std::vector<DeviceTensorFloatND> rows_all = {
            DeviceTensorFloatND(ttv(upload(rows), {N, SCRG_STRIDE, 1}))};
        const std::vector<uint2> box_all(
            (size_t)N, uint2{SS_AABB16_PACK(0u, 65535u), SS_AABB16_PACK(0u, 65535u)});
        uint2* d_box_all = upload(box_all);
        DeviceTensor2D<uint2> aabb_all(
            TorchTensorView{(uint64_t)d_box_all, (uint32_t)sizeof(unsigned), {1LL, N, 2LL}});

        auto raster = surfel ? rasterize_to_pixels_2dgs_fwd : rasterize_to_pixels_3dgut_fwd;
        auto render = [&](std::vector<DeviceTensorFloatND>& screen, DeviceTensor2D<uint2>& bounds,
                          const DeviceTensor3D<int32_t>& offsets,
                          const DeviceVector<int32_t>& ids, int macro) {
            auto out = raster(N, in_splats, screen, DeviceVector<int32_t>(), vm_tv, intr_tv,
                              model, tier, dist_tv, bounds, W, H, offsets, ids, macro,
                              DistortionType::None, false, true, SplatScoreRender{});
            backend::device_synchronize();
            std::vector<float> img = download(
                (const float*)std::get<0>(std::get<0>(out)).data_ptr(), (size_t)W * H * 3);
            const std::vector<float> T = download(std::get<1>(out).data_ptr(), (size_t)W * H);
            img.insert(img.end(), T.begin(), T.end());
            return img;
        };

        for (int macro_log2 : macro_log2s)
        for (bool skip : {false, true}) {
            // A tile-active map (every third tile off, as a mask would) binned
            // against the reference with those tiles emptied.
            const uint32_t tw = _CEIL_DIV(W, (uint32_t)bin_tile_x(macro_log2));
            const uint32_t th = _CEIL_DIV(H, (uint32_t)bin_tile_y(macro_log2));
            std::vector<int32_t> live(tw * th);
            for (uint32_t t = 0; t < tw * th; t++) live[t] = skip && t % 3 == 1 ? 0 : 1;
            int32_t* d_live = upload(live);

            // --- as the engine bins and culls ---
            int macro = macro_log2;
            auto isect = do_intersect_tile_generic(
                aabb, depths, ellipse_view(splats_s, true),
                footprint_view(in_splats, nullptr, vm_tv, dist_tv, model, tier, surfel != 0),
                1, intr_tv, W, H, nullptr, skip ? d_live : nullptr, macro);
            const std::vector<float> binned =
                render(splats_s, aabb, std::get<2>(isect), std::get<1>(isect), macro);
            const int64_t pairs = std::get<1>(isect).size();
            backend::device_free(d_live);
            if (macro != macro_log2) {
                std::fprintf(stderr, "binning coarsened from %d to %d\n", macro_log2, macro);
                return 1;
            }

            // --- every splat in every live tile ---
            std::vector<int32_t> ids, offsets(tw * th);
            for (uint32_t t = 0; t < tw * th; t++) {
                offsets[t] = (int32_t)ids.size();
                if (live[t]) ids.insert(ids.end(), order.begin(), order.end());
            }
            int32_t* d_ids = upload(ids);
            int32_t* d_offsets = upload(offsets);
            const std::vector<float> brute = render(
                rows_all, aabb_all, DeviceTensor3D<int32_t>(ttv(d_offsets, {1, th, tw})),
                DeviceVector<int32_t>(ttv(d_ids, {(int64_t)ids.size()})), macro);
            backend::device_free(d_ids);
            backend::device_free(d_offsets);
            if (const char* err = backend::last_error()) {
                std::fprintf(stderr, "backend error: %s\n", err);
                return 1;
            }

            float worst = 0.0f;
            int64_t bad = 0;
            for (size_t i = 0; i < brute.size(); i++) {
                const float d = std::fabs(brute[i] - binned[i]);
                if (!(d <= kTolerance)) bad++;
                if (!(d <= worst)) worst = d;
            }
            std::printf("%-16s %-10s %s  %2d px tiles%s: %6lld pairs, %4zu drawn, %3d culled "
                        "by the projection, max |diff| %.2e%s\n",
                        view.model, tier, surfel ? "2dgs " : "3dgut", bin_tile_x(macro),
                        skip ? ", a third off" : "", (long long)pairs, order.size(), culled,
                        (double)worst, bad ? "  FAIL" : "");
            ok = ok && bad == 0;
        }
        backend::device_free(d_box_all);
        backend::device_free(d_intr);
    }
    std::printf(ok ? "footprint_exact: OK\n" : "footprint_exact: FAILED\n");
    return ok ? 0 : 1;
}
