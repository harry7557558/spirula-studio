// ENGINE-level end-to-end TRAINING parity: drives the fused train-step
// entrypoints an app uses — engine_train_step (plain pinhole batches with
// byte GT: u8 rgb + u16 depth + u8 normal + u8 mask, exercising the raw
// byte->float conversion kernels inside set_training_data) and
// engine_train_step_warped (fisheye and equirectangular inputs split to
// pinhole faces, exercising the fused byte->float GT warp kernels) — each
// running the full forward + multi-scale loss/backward + Adam pipeline.
// The SAME source builds under both backends:
//
//   CUDA build:   ./engine_train_parity dump ref.bin
//   Vulkan build: ./engine_train_parity compare ref.bin   (per device)
//
// Ref format: [nt tight floats] [nl loose floats].
//
// Tight: the engine's GT buffers after the last (equirectangular) warped
// upload — deterministic per-pixel warp output read straight from
// engine().gt (isolated boundary-flip pixels only). Loose: the per-step
// loss maps returned by the train steps (atomic accumulation order, and
// one-iteration-behind on both backends) and the splat parameters after
// all optimizer steps (fed by atomically accumulated gradients).
// Densification is disabled so the splat count stays fixed.

#include <backend/tests/DistortionFixture.h>
#include <engine/Engine.h>
#include <engine/EngineState.h>
#include <kernels/pixelwise/PixelWise.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

static constexpr int64_t N = 3000;
static constexpr int NUM_SH = 15;  // degree 3 buffer

static std::vector<float> g_tight, g_loose;

TorchTensorView ttv(const void* p, uint32_t elem,
                    std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, elem, std::move(shape));
}
TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

struct Rng {
    std::mt19937 rng;
    explicit Rng(uint32_t seed) : rng(seed) {}
    float uf(float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    }
    std::vector<uint8_t> bytes(int64_t n) {
        std::vector<uint8_t> v(n);
        for (auto& x : v) x = (uint8_t)(rng() & 0xff);
        return v;
    }
    std::vector<uint16_t> words(int64_t n, uint16_t max) {
        std::vector<uint16_t> v(n);
        for (auto& x : v) x = (uint16_t)(rng() % (uint32_t)(max + 1));
        return v;
    }
};

static void readback_dev_f(std::vector<float>& acc, const void* d,
                           int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

static void push_losses(const std::map<std::string, float>& m) {
    for (const auto& kv : m) g_loose.push_back(kv.second);
}

// [K, 3, 3] unit face frames (see warp_parity.cpp).
static std::vector<float> make_axes(int K) {
    struct V3 { float x, y, z; };
    auto norm3 = [](V3 a) {
        float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
        return V3{a.x / l, a.y / l, a.z / l};
    };
    auto cross3 = [](V3 a, V3 b) {
        return V3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
    };
    const V3 zs[4] = {{0, 0, 1}, {1, 0, 0}, {0.577f, 0.577f, 0.577f},
                      {0, -1, 0}};
    std::vector<float> axes;
    for (int k = 0; k < K; k++) {
        V3 z = norm3(zs[k % 4]);
        V3 up = std::fabs(z.y) > 0.9f ? V3{0, 0, 1} : V3{0, 1, 0};
        V3 x = norm3(cross3(up, z));
        V3 y = cross3(z, x);
        float row[9] = {x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z};
        axes.insert(axes.end(), row, row + 9);
    }
    return axes;
}

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

    Rng r(20260718u);

    // --- splats ---
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3), sh(N * NUM_SH * 3);
    for (int64_t i = 0; i < N; i++) {
        means[3 * i + 0] = r.uf(-3.f, 3.f);
        means[3 * i + 1] = r.uf(-3.f, 3.f);
        means[3 * i + 2] = r.uf(-1.f, 7.f);
        float qn = 0.f;
        for (int k = 0; k < 4; k++) {
            quats[4 * i + k] = r.uf(-1.f, 1.f);
            qn += quats[4 * i + k] * quats[4 * i + k];
        }
        if (qn < 1e-6f) quats[4 * i] = 1.f;
        for (int k = 0; k < 3; k++) scales[3 * i + k] = r.uf(-5.f, -1.8f);
        opac[i] = r.uf(-2.f, 4.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = r.uf(-0.5f, 1.5f);
    }
    for (auto& v : sh) v = r.uf(-0.25f, 0.25f);

    set_data_3dgs(N, ttv(means.data(), 4, {N, 3}),
                  ttv(quats.data(), 4, {N, 4}),
                  ttv(scales.data(), 4, {N, 3}),
                  ttv(opac.data(), 4, {N, 1}), ttv(dc.data(), 4, {N, 3}),
                  ttv(sh.data(), 4, {N, NUM_SH, 3}));

    // --- shared step config: multi-scale loss + SSIM + Adam, no densify ---
    EngineStepConfig cfg{};
    auto W_ = [&](LossWeightIndex i) -> float& {
        return cfg.loss.weights[(int)i];
    };
    W_(LossWeightIndex::RgbSupL1) = 0.8f;
    W_(LossWeightIndex::RgbSupL2) = 0.2f;
    W_(LossWeightIndex::DepthSup) = 0.02f;
    W_(LossWeightIndex::NormalSup) = 0.05f;
    W_(LossWeightIndex::AlphaSup) = 0.05f;
    W_(LossWeightIndex::NormalReg) = 0.02f;
    W_(LossWeightIndex::AlphaReg) = 0.01f;
    cfg.loss.w_ssim = 0.2f;
    cfg.loss.num_loss_scales = 2;
    cfg.loss.compute_loss_map = true;
    cfg.loss.loss_map_mode = 1;  // LossFull
    cfg.optim.lr_means = 1.6e-4f;
    cfg.optim.lr_quats = 1e-3f;
    cfg.optim.lr_scales = 5e-3f;
    cfg.optim.lr_opacities = 5e-2f;
    cfg.optim.lr_features_dc = 2.5e-3f;
    cfg.optim.lr_features_sh = 1.25e-4f;
    cfg.optim.sh_value_bits = 32;
    // Disable densification via refine_start_iter: the step gate evaluates
    // `step % refine_every` whenever step > refine_start_iter, so a zero
    // refine_every alone SIGFPEs. Keep both fields out of reach.
    cfg.densify.refine_start_iter = 1 << 20;
    cfg.densify.refine_every = 1 << 20;

    const int max_steps = 100;
    int step = 1;

    // --- plain pinhole steps with byte GT (conversion kernels) -------------
    {
        const int C = 2, W = 96, H = 72;
        const float cy_ = std::cos(0.15f), sy_ = std::sin(0.15f);
        std::vector<float> vm = {
            1, 0, 0, 0,   0, 1, 0, 0,   0, 0, 1, 3.5f,  0, 0, 0, 1,
            cy_, 0, sy_, 0.2f,  0, 1, 0, -0.1f,  -sy_, 0, cy_, 4.2f,
            0, 0, 0, 1,
        };
        std::vector<float> intr = {70, 71, 48, 36, 69, 70, 47, 37};
        std::vector<float> dist = dist_fixture::distortion_rows(C);

        auto gt_rgb = r.bytes((int64_t)C * H * W * 3);
        auto gt_depth = r.words((int64_t)C * H * W, 12);
        auto gt_normal = r.bytes((int64_t)C * H * W * 3);
        auto gt_alpha = r.bytes((int64_t)C * H * W);
        for (auto& v : gt_alpha) v = v < 220 ? 1 : 0;

        // One step per tier: PINHOLE compiles all four, and the tier is
        // orthogonal to everything the step does with the GT.
        for (int s = 0; s < 4; s++, step++) {
            auto losses = engine_train_step(
                step, max_steps, "3dgs", 3, /*packed=*/false, W, H,
                "PINHOLE", dist_fixture::kTierNames[s],
                ttv(vm.data(), 4, {C, 4, 4}), ttv(intr.data(), 4, {C, 4}),
                ttv(dist.data() + dist_fixture::row_offset(s, C), 4,
                    {C, kCameraDistortionParams}),
                ttv(gt_rgb.data(), 1, {C, H, W, 3}),
                ttv(gt_depth.data(), 2, {C, H, W, 1}),
                ttv(gt_normal.data(), 1, {C, H, W, 3}),
                ttv(gt_alpha.data(), 1, {C, H, W, 1}), ttv_null(), cfg);
            push_losses(losses);
        }
    }

    // --- warped steps (fisheye wide + equirectangular) ---------------------
    const int K = 2, out_W = 64, out_H = 64;
    std::vector<float> axes = make_axes(K);

    // Post-split pinhole cameras: the warp samples each face through this
    // very table, so an off-centre crop here is an off-centre crop there.
    const int B_post = K;  // B_in = 1
    std::vector<float> post_vm = {
        1, 0, 0, 0,   0, 1, 0, 0,   0, 0, 1, 3.0f,  0, 0, 0, 1,
        0, 0, -1, 0.1f,  0, 1, 0, 0,   1, 0, 0, 3.8f,  0, 0, 0, 1,
    };
    std::vector<float> post_intr;
    for (int k = 0; k < B_post; k++) {
        post_intr.push_back(0.5f * out_W / 0.7f);
        post_intr.push_back(0.5f * out_H / 0.7f);
        post_intr.push_back(0.5f * out_W + 4.0f * k);
        post_intr.push_back(0.5f * out_H - 3.0f * k);
    }
    // engine_train_step_warped always sets the post-split table to
    // PINHOLE / NONE.
    std::vector<float> post_dist(B_post * kCameraDistortionParams, 0.f);

    struct WarpCase {
        const char* model;
        int tier;      // distortion tier, index into dist_fixture::kTierNames
        int in_H, in_W;
        float fx, fy;
    };
    const WarpCase wcases[2] = {
        {"FISHEYE", 2, 48, 64, 20.0f, 20.5f},
        {"EQUIRECTANGULAR", 0, 32, 64, 10.2f, 10.2f},
    };
    const std::vector<float> warp_dist = dist_fixture::distortion_rows(1);
    for (const WarpCase& wc : wcases) {
        std::vector<float> in_intr = {wc.fx, wc.fy, 0.5f * wc.in_W,
                                      0.5f * wc.in_H};
        const float* in_dist =
            warp_dist.data() + dist_fixture::row_offset(wc.tier, 1);

        auto gt_rgb = r.bytes((int64_t)wc.in_H * wc.in_W * 3);
        auto gt_alpha = r.bytes((int64_t)wc.in_H * wc.in_W);
        for (auto& v : gt_alpha) v = v < 220 ? 1 : 0;
        // A solid dead band, not just speckle: whole tiles have to die for the
        // skip path to differ from rendering everything.
        for (int y = 0; y < wc.in_H; y++)
            for (int x = 0; x < wc.in_W / 2; x++) gt_alpha[(int64_t)y * wc.in_W + x] = 0;
        auto gt_depth = r.words((int64_t)wc.in_H * wc.in_W, 12);
        auto gt_normal = r.bytes((int64_t)wc.in_H * wc.in_W * 3);

        // Steps 2-3 drop alpha supervision, which is what lets the engine
        // leave fully-masked tiles unrendered; SS_NO_TILE_SKIP=1 renders them
        // and must land in the same place.
        for (int s = 0; s < 4; s++, step++) {
            EngineStepConfig cfg_s = cfg;
            if (s >= 2) {
                cfg_s.loss.weights[(int)LossWeightIndex::AlphaSup] = 0.0f;
                cfg_s.loss.weights[(int)LossWeightIndex::AlphaSupUnder] = 0.0f;
            }
            auto losses = engine_train_step_warped(
                step, max_steps, "3dgs", 3, /*packed=*/false, out_W, out_H,
                ttv(post_vm.data(), 4, {B_post, 4, 4}),
                ttv(post_intr.data(), 4, {B_post, 4}),
                ttv(post_dist.data(), 4, {B_post, kCameraDistortionParams}),
                wc.model, dist_fixture::kTierNames[wc.tier],
                /*B_in=*/1, wc.in_H, wc.in_W, K,
                ttv(in_intr.data(), 4, {1, 4}),
                ttv(in_dist, 4, {1, kCameraDistortionParams}),
                ttv_null(), ttv_null(),
                ttv(gt_rgb.data(), 1, {1, wc.in_H, wc.in_W, 3}),
                ttv(gt_alpha.data(), 1, {1, wc.in_H, wc.in_W, 1}),
                wc.in_H, wc.in_W,
                ttv(gt_depth.data(), 2, {1, wc.in_H, wc.in_W, 1}),
                wc.in_H, wc.in_W,
                ttv(gt_normal.data(), 1, {1, wc.in_H, wc.in_W, 3}),
                wc.in_H, wc.in_W, ttv(axes.data(), 4, {K, 3, 3}),
                /*face_passes=*/{}, ttv_null(), cfg_s);
            push_losses(losses);
        }
    }

    backend::device_synchronize();
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error (train steps): %s\n", err);
        return 1;
    }

    // --- tight: engine GT buffers after the last (equirect) warped upload --
    {
        auto& gt = engine().gt;
        readback_dev_f(g_tight, gt.rgb.data_ptr(),
                       (int64_t)B_post * out_H * out_W * 3);
        readback_dev_f(g_tight, gt.depth.data_ptr(),
                       (int64_t)B_post * out_H * out_W);
        readback_dev_f(g_tight, gt.normal.data_ptr(),
                       (int64_t)B_post * out_H * out_W * 3);
        std::vector<uint8_t> alpha(B_post * out_H * out_W);
        backend::memcpy_sync(alpha.data(), gt.alpha.data_ptr(),
                             alpha.size(), MemcpyKind::DeviceToHost);
        for (uint8_t v : alpha) g_tight.push_back((float)v);
    }

    // --- loose: splat parameters after all optimizer steps -----------------
    if (engine().cur_num_splats != N) {
        std::fprintf(stderr, "unexpected splat count %lld (densify ran?)\n",
                     (long long)engine().cur_num_splats);
        return 1;
    }
    readback_dev_f(g_loose, engine().world.means.data_ptr(), N * 3);
    readback_dev_f(g_loose, engine().world.quats.data_ptr(), N * 4);
    readback_dev_f(g_loose, engine().world.scales.data_ptr(), N * 3);
    readback_dev_f(g_loose, engine().world.opacities.data_ptr(), N);
    readback_dev_f(g_loose, engine().world.features_dc.data_ptr(), N * 3);

    const int64_t nt = (int64_t)g_tight.size(),
                  nl = (int64_t)g_loose.size();
    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        f.write((const char*)&nt, 8);
        f.write((const char*)g_tight.data(), nt * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)g_loose.data(), nl * 4);
        std::printf("engine_train_parity: dumped %lld tight + %lld loose "
                    "floats to %s\n",
                    (long long)nt, (long long)nl, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t rnt = 0, rnl = 0;
    std::vector<float> reft, refl;
    f.read((char*)&rnt, 8);
    if (rnt != nt) {
        std::fprintf(stderr, "tight count mismatch: ref %lld vs got %lld\n",
                     (long long)rnt, (long long)nt);
        return 1;
    }
    reft.resize(nt);
    f.read((char*)reft.data(), nt * 4);
    f.read((char*)&rnl, 8);
    if (rnl != nl) {
        std::fprintf(stderr, "loose count mismatch: ref %lld vs got %lld\n",
                     (long long)rnl, (long long)nl);
        return 1;
    }
    refl.resize(nl);
    f.read((char*)refl.data(), nl * 4);

    // Two gates: the element one absorbs the trajectory drift a 12-step run
    // accumulates on any architecture, the relative-RMS one is what still
    // catches a real break. Measured numbers: docs/testing.md.
    auto check = [](const char* name, const std::vector<float>& got,
                    const std::vector<float>& ref, double allow,
                    double allow_rms) {
        int64_t viol = 0, n_fin = 0;
        double max_abs = 0, se = 0, sr = 0;
        for (size_t i = 0; i < got.size(); i++) {
            double g = (double)got[i], r = (double)ref[i];
            if (!std::isfinite(g) || !std::isfinite(r)) continue;
            double d = std::fabs(g - r);
            double tol = 5e-3 + 1e-3 * std::fabs(r);
            max_abs = std::max(max_abs, d);
            if (d > tol) viol++;
            se += d * d;
            sr += r * r;
            n_fin++;
        }
        double frac =
            got.empty() ? 0.0 : (double)viol / (double)got.size();
        double rms = sr > 0 ? std::sqrt(se / sr) : 0.0;
        std::printf("engine_train_parity: %s %zu floats (max_abs %.3g, "
                    "violations %lld = %.5f%%, rel_rms %.3g)\n",
                    name, got.size(), max_abs, (long long)viol,
                    100.0 * frac, rms);
        (void)n_fin;
        return frac <= allow && rms <= allow_rms;
    };
    bool pass = check("tight", g_tight, reft, 2e-3, 1e-3);
    pass = check("loose", g_loose, refl, 8e-2, 2e-3) && pass;
    std::printf(pass ? "engine_train_parity: PASSED\n"
                     : "engine_train_parity: FAILED\n");
    return pass ? 0 : 1;
}
