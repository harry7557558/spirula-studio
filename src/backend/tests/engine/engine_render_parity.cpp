// ENGINE-level backend parity: drives the real render path an app uses —
// set_data_3dgs / set_camera_params, forward_3dgs (projection -> tile
// intersect -> rasterize -> background blend -> color space), the
// depth->normal viewer buffer, and engine_blit_view (frustum/grid overlay +
// colormap + uint8 pack). The SAME source builds under both backends:
//
//   CUDA build:   ./engine_render_parity dump ref.bin
//   Vulkan build: ./engine_render_parity compare ref.bin   (per device)
//
// Float outputs compare with the usual fast-math tolerance + tiny violation
// allowance (near-tie depth-key sort flips reorder blending on a handful of
// pixels). The blit's uint8 output allows |d| <= 2 per byte (fp rounding at
// the byte quantization boundary) with the same violation cap.

#include <backend/tests/DistortionFixture.h>
#include <engine/Engine.h>
#include <engine/EngineState.h>
#include <kernels/pixelwise/PixelWise.cuh>
#include <kernels/visualize/Visualizer.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

using backend::MemcpyKind;

static constexpr int64_t N = 4000;
static constexpr int C = 2;
static constexpr int W = 240, H = 180;
static constexpr int NUM_SH = 15;  // degree 3 buffer
static constexpr int N_CAM = 6;    // viewer dataset cameras

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
TorchTensorView ttv_null() { return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0}); }

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    // Kernel parity, not binning policy: pin the granularity so a reference
    // stays comparable across runs that would otherwise adapt it.
    engine_set_bin_tile_size(bin_tile_x(kMacroLog2Default));

    std::mt19937 rng(20260717u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    // --- splats ---
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3), sh(N * NUM_SH * 3);
    for (int64_t i = 0; i < N; i++) {
        means[3 * i + 0] = uf(-3.f, 3.f);
        means[3 * i + 1] = uf(-3.f, 3.f);
        means[3 * i + 2] = uf(-1.f, 7.f);
        float qn = 0.f;
        for (int k = 0; k < 4; k++) {
            quats[4 * i + k] = uf(-1.f, 1.f);
            qn += quats[4 * i + k] * quats[4 * i + k];
        }
        if (qn < 1e-6f) quats[4 * i] = 1.f;
        for (int k = 0; k < 3; k++) scales[3 * i + k] = uf(-5.f, -1.8f);
        opac[i] = uf(-2.f, 4.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = uf(-0.5f, 1.5f);
    }
    for (auto& v : sh) v = uf(-0.25f, 0.25f);

    set_data_3dgs(N, ttv(means.data(), {N, 3}), ttv(quats.data(), {N, 4}),
                  ttv(scales.data(), {N, 3}), ttv(opac.data(), {N, 1}),
                  ttv(dc.data(), {N, 3}),
                  ttv(sh.data(), {N, NUM_SH, 3}));

    // --- render cameras (2) ---
    const float cy_ = std::cos(0.15f), sy_ = std::sin(0.15f);
    std::vector<float> vm = {
        1, 0, 0, 0,   0, 1, 0, 0,    0, 0, 1, 3.5f,  0, 0, 0, 1,
        cy_, 0, sy_, 0.2f,  0, 1, 0, -0.1f,  -sy_, 0, cy_, 4.2f, 0, 0, 0, 1,
    };
    std::vector<float> intr = {180, 182, 120, 90, 175, 177, 118, 92};
    std::vector<float> dist = dist_fixture::distortion_rows(C);

    auto set_cams = [&](const char* model, int tier) {
        set_camera_params(W, H, model, dist_fixture::kTierNames[tier],
                          ttv(vm.data(), {C, 4, 4}), ttv(intr.data(), {C, 4}),
                          ttv(dist.data() + dist_fixture::row_offset(tier, C),
                              {C, kCameraDistortionParams}));
    };

    // --- background: SH skybox with deterministic coefficients ---
    engine_init_background_sh(/*sh_degree=*/2, /*linear=*/false);
    {
        std::vector<float> bg_sh(9 * 3);
        for (auto& v : bg_sh) v = uf(-0.4f, 0.4f);
        bg_sh[0] = 0.3f; bg_sh[1] = 0.5f; bg_sh[2] = 0.8f;  // DC sky color
        backend::memcpy_sync(engine().background.sh_coeffs.data_ptr(),
                             bg_sh.data(), bg_sh.size() * sizeof(float),
                             MemcpyKind::HostToDevice);
    }
    engine_set_background_step_params(4321u, 0.6f);

    // --- color space: mildly non-identity splat matrix (sRGB working) ---
    engine_init_color_space(
        /*splat_enabled=*/true, /*splat_is_linear=*/false,
        {0.9f, 0.08f, 0.02f, 0.05f, 0.9f, 0.05f, 0.02f, 0.08f, 0.9f},
        /*image_enabled=*/false, /*image_is_linear=*/false,
        {1, 0, 0, 0, 1, 0, 0, 0, 1});

    std::vector<float> acc;      // float outputs
    std::vector<uint8_t> accb;   // blit bytes
    auto pull = [&](int want_median, int want_dist) {
        std::vector<float> rgb(C * H * W * 3), depth(C * H * W),
            Ts(C * H * W), raw(C * H * W * 3);
        engine_copy_render_to_host(
            ttv(rgb.data(), {C, H, W, 3}), ttv(depth.data(), {C, H, W, 1}),
            ttv(Ts.data(), {C, H, W, 1}), ttv(raw.data(), {C, H, W, 3}),
            ttv_null());
        acc.insert(acc.end(), rgb.begin(), rgb.end());
        acc.insert(acc.end(), depth.begin(), depth.end());
        acc.insert(acc.end(), Ts.begin(), Ts.end());
        acc.insert(acc.end(), raw.begin(), raw.end());
        if (want_median) {
            std::vector<float> med(C * H * W);
            engine_copy_render_to_host(ttv_null(), ttv_null(), ttv_null(),
                                       ttv_null(),
                                       ttv(med.data(), {C, H, W, 1}));
            acc.insert(acc.end(), med.begin(), med.end());
        }
        if (want_dist) {
            std::vector<float> drgb(C * H * W * 3), dd(C * H * W);
            engine_copy_distortion_to_host(ttv(drgb.data(), {C, H, W, 3}),
                                           ttv(dd.data(), {C, H, W, 1}));
            acc.insert(acc.end(), drgb.begin(), drgb.end());
            acc.insert(acc.end(), dd.begin(), dd.end());
        }
    };

    struct Cfg {
        const char* prim;
        const char* cam;
        int tier;       // distortion tier, index into dist_fixture::kTierNames
        bool packed;
        bool median;
        int dist_type;  // 0 None, 2 RGB_D
    };
    // Rows 1 and 2 differ only in the tier, so the NONE fast path is read
    // against a distorted neighbour.
    const Cfg cfgs[] = {
        {"3dgs", "PINHOLE", 0, false, true, 0},
        {"3dgs", "PINHOLE", 1, false, true, 0},
        {"mip", "FISHEYE", 2, false, false, 2},
        {"3dgut", "PINHOLE", 3, false, false, 0},
        {"3dgs", "EQUIRECTANGULAR", 0, true, false, 0},
    };
    for (const Cfg& c : cfgs) {
        set_cams(c.cam, c.tier);
        forward_3dgs(c.prim, 3, c.packed, c.median, c.dist_type);
        backend::device_synchronize();
        if (const char* err = backend::last_error()) {
            std::fprintf(stderr, "backend error (%s/%s): %s\n", c.prim,
                         c.cam, err);
            return 1;
        }
        pull(c.median, c.dist_type != 0);
    }

    // --- noise background mode ---
    engine_init_background_noise(/*linear=*/false);
    set_cams("PINHOLE", 1);
    forward_3dgs("3dgs", 3, false, false, 0);
    backend::device_synchronize();
    pull(0, 0);

    // --- depth -> normal (viewer buffer path) on the last render ---
    {
        auto& depth_t = std::get<1>(engine().fwd.renders);
        float* d_normals =
            (float*)backend::device_malloc((size_t)C * H * W * 3 * 4);
        depth_to_normal_forward_tv(
            "PINHOLE", "OPENCV",
            ttv(engine().camera.intrins.data_ptr(), {C, 4}),
            ttv(engine().camera.dist_coeffs.data_ptr(),
                {C, kCameraDistortionParams}),
            /*is_ray_depth=*/true,
            ttv(depth_t.data_ptr(), {C, H, W, 1}),
            ttv(d_normals, {C, H, W, 3}));
        backend::device_synchronize();
        std::vector<float> normals(C * H * W * 3);
        backend::memcpy_sync(normals.data(), d_normals,
                             normals.size() * 4, MemcpyKind::DeviceToHost);
        acc.insert(acc.end(), normals.begin(), normals.end());
    }

    // --- viewer blit: synthetic 6-camera dataset + grid overlay ---
    {
        std::vector<float> v_intr(N_CAM * 4),
            v_dist(N_CAM * kCameraDistortionParams, 0.f), v_c2w(N_CAM * 12);
        std::vector<int32_t> v_w(N_CAM, 320), v_h(N_CAM, 240),
            v_model(N_CAM, 0), v_tier(N_CAM, 0);
        for (int i = 0; i < N_CAM; i++) {
            v_intr[4 * i + 0] = 260 + 5 * i;
            v_intr[4 * i + 1] = 262 + 5 * i;
            v_intr[4 * i + 2] = 160;
            v_intr[4 * i + 3] = 120;
            float ang = 0.9f * (float)i;
            float r = 3.0f;
            // camera at radius r around origin, z-up-ish orbit
            float cx = r * std::cos(ang), cyy = r * std::sin(ang);
            // rows of R (camera axes in world), position in col 4
            v_c2w[12 * i + 0] = -std::sin(ang);
            v_c2w[12 * i + 1] = 0;
            v_c2w[12 * i + 2] = std::cos(ang);
            v_c2w[12 * i + 3] = cx;
            v_c2w[12 * i + 4] = 0;
            v_c2w[12 * i + 5] = 1;
            v_c2w[12 * i + 6] = 0.1f * i;
            v_c2w[12 * i + 7] = cyy;
            v_c2w[12 * i + 8] = -std::cos(ang);
            v_c2w[12 * i + 9] = 0.05f;
            v_c2w[12 * i + 10] = -std::sin(ang);
            v_c2w[12 * i + 11] = 1.5f;
        }
        v_model[2] = 1;  // one fisheye frustum
        // Mixed tiers across the table: the frustum fill dispatches the tier
        // per camera, so a single-tier dataset would not exercise it (the
        // view camera's tier used to stand in, drawing every frustum
        // undistorted). Cameras 0/3 stay NONE. The fixture rows are scaled
        // up here -- a frustum is a few dozen pixels wide, so a mild lens
        // bends its edges by less than the byte-comparison tolerance.
        const std::vector<float> rows = dist_fixture::distortion_rows(1);
        for (int i : {1, 2, 4, 5}) {
            int tier = i == 2 ? 2 : (i == 5 ? 3 : 1);  // fisheye: no RATIONAL
            v_tier[i] = tier;
            for (int k = 0; k < kCameraDistortionParams; k++)
                v_dist[(size_t)i * kCameraDistortionParams + k] =
                    8.0f * rows[dist_fixture::row_offset(tier, 1) + k];
        }
        engine_viewer_init(ttv(v_model.data(), {N_CAM}),
                           ttv(v_intr.data(), {N_CAM, 4}),
                           ttv(v_dist.data(), {N_CAM, kCameraDistortionParams}),
                           ttv(v_tier.data(), {N_CAM}),
                           ttv(v_c2w.data(), {N_CAM, 3, 4}),
                           ttv(v_w.data(), {N_CAM}),
                           ttv(v_h.data(), {N_CAM}), 0.8f);
        engine_viewer_set_grid(/*radius=*/4.0f, /*view_distance=*/3.0f);

        // view camera = render camera 0 (device copies)
        std::vector<float> view_vm(vm.begin(), vm.begin() + 16);
        std::vector<float> view_intr(intr.begin(), intr.begin() + 4);
        // Visualization camera: OpenCV, so the blit's own projection runs a
        // distorted tier rather than only the fast path.
        std::vector<float> view_dist(
            dist.begin() + dist_fixture::row_offset(1, C),
            dist.begin() + dist_fixture::row_offset(1, C) +
                kCameraDistortionParams);
        float* d_vm = upload(view_vm);
        float* d_vi = upload(view_intr);
        float* d_vd = upload(view_dist);

        auto& rgb_t = std::get<0>(engine().fwd.renders);
        auto& depth_t = std::get<1>(engine().fwd.renders);
        float* d_Ts = engine().fwd.render_Ts.data_ptr();
        uint8_t* d_out = (uint8_t*)backend::device_malloc((size_t)H * W * 3);

        for (int mode = 0; mode < 2; mode++) {
            bool cams = mode == 0;
            engine_blit_view(
                "rgb", ttv(rgb_t.data_ptr(), {H, W, 3}),
                ttv(depth_t.data_ptr(), {H, W, 1}), ttv(d_Ts, {H, W, 1}), 0,
                "OPENCV", ttv(d_vi, {1, 4}), ttv(d_vm, {4, 4}),
                ttv(d_vd, {1, kCameraDistortionParams}),
                /*show_training_cameras=*/cams, /*show_overlay=*/true,
                /*grid_dist=*/3.0f, 0.2f, -0.1f, 0.0f,
                ttv(d_out, {H, W, 3}));
            backend::device_synchronize();
            if (const char* err = backend::last_error()) {
                std::fprintf(stderr, "backend error (blit %d): %s\n", mode,
                             err);
                return 1;
            }
            size_t off = accb.size();
            accb.resize(off + (size_t)H * W * 3);
            backend::memcpy_sync(accb.data() + off, d_out,
                                 (size_t)H * W * 3,
                                 MemcpyKind::DeviceToHost);
        }
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)acc.size(), nb = (int64_t)accb.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        f.write((const char*)&nb, 8);
        f.write((const char*)accb.data(), nb);
        std::printf("engine_render_parity: dumped %lld floats + %lld bytes "
                    "to %s\n",
                    (long long)nf, (long long)nb, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nb = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)acc.size()) {
        std::fprintf(stderr, "float count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, acc.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);
    f.read((char*)&nb, 8);
    if (nb != (int64_t)accb.size()) {
        std::fprintf(stderr, "byte count mismatch: ref %lld vs got %zu\n",
                     (long long)nb, accb.size());
        return 1;
    }
    std::vector<uint8_t> refb(nb);
    f.read((char*)refb.data(), nb);

    int64_t fviol = 0;
    double max_abs = 0;
    for (int64_t i = 0; i < nf; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 5e-4 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) fviol++;
    }
    int64_t bviol = 0;
    int max_b = 0;
    for (int64_t i = 0; i < nb; i++) {
        int d = std::abs((int)accb[i] - (int)refb[i]);
        max_b = std::max(max_b, d);
        if (d > 2) bviol++;
    }
    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double bfrac = nb ? (double)bviol / (double)nb : 0.0;
    std::printf("engine_render_parity: %lld floats (max_abs %.3g, "
                "violations %lld = %.5f%%), %lld blit bytes (max |d| %d, "
                "violations %lld = %.5f%%)\n",
                (long long)nf, max_abs, (long long)fviol, 100.0 * ffrac,
                (long long)nb, max_b, (long long)bviol, 100.0 * bfrac);
    bool pass = ffrac <= 1e-3 && bfrac <= 2e-3;
    std::printf(pass ? "engine_render_parity: PASSED\n"
                     : "engine_render_parity: FAILED\n");
    return pass ? 0 : 1;
}
