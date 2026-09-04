// One bad float must not take the whole model down. Trains a healthy scene on
// the shipping quantized FPBO path, pokes ONE non-finite value into ONE splat,
// trains on, and counts the splats left non-finite.
//
// The amplifier under test is the block-quantized Adam state: one float4
// bound is shared by 256 splats, so a bound that goes non-finite decodes the
// whole block to NaN. CUDA's fminf/fmaxf drop a NaN operand but SPIR-V's
// FMin/FMax are undefined on one, so the backends would disagree without an
// explicit sanitize. Self-checking; no reference file.

#include <engine/Engine.h>
#include <engine/EngineState.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

static constexpr int64_t N = 2048;   // 8 quantization blocks of 256
static constexpr int NUM_SH = 4;     // degree 1 buffer
static constexpr int C = 2, W = 64, H = 48;

static TorchTensorView ttv(const void* p, uint32_t elem,
                           std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, elem, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

struct Scene {
    std::vector<float> vm, intr, dist;
    std::vector<uint8_t> gt_rgb, gt_normal;
    std::vector<uint16_t> gt_depth;
    EngineStepConfig cfg{};
};

static Scene build_scene(std::mt19937& rng) {
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3), sh(N * NUM_SH * 3);
    for (int64_t i = 0; i < N; i++) {
        means[3 * i + 0] = uf(-3.f, 3.f);
        means[3 * i + 1] = uf(-3.f, 3.f);
        means[3 * i + 2] = uf(-1.f, 7.f);
        quats[4 * i + 0] = 1.f;
        for (int k = 1; k < 4; k++) quats[4 * i + k] = uf(-0.2f, 0.2f);
        for (int k = 0; k < 3; k++) scales[3 * i + k] = uf(-5.f, -1.8f);
        opac[i] = uf(-2.f, 4.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = uf(-0.5f, 1.5f);
    }
    for (auto& v : sh) v = uf(-0.25f, 0.25f);

    set_data_3dgs(N, ttv(means.data(), 4, {N, 3}),
                  ttv(quats.data(), 4, {N, 4}),
                  ttv(scales.data(), 4, {N, 3}),
                  ttv(opac.data(), 4, {N, 1}), ttv(dc.data(), 4, {N, 3}),
                  ttv(sh.data(), 4, {N, NUM_SH, 3}));

    Scene s;
    auto& cfg = s.cfg;
    auto W_ = [&](LossWeightIndex i) -> float& {
        return cfg.loss.weights[(int)i];
    };
    W_(LossWeightIndex::RgbSupL1) = 0.8f;
    W_(LossWeightIndex::RgbSupL2) = 0.2f;
    W_(LossWeightIndex::DepthSup) = 0.5f;
    W_(LossWeightIndex::NormalSup) = 0.5f;
    cfg.loss.w_ssim = 0.2f;
    cfg.loss.num_loss_scales = 1;
    cfg.optim.lr_means = 1.6e-4f;
    cfg.optim.lr_quats = 1e-3f;
    cfg.optim.lr_scales = 5e-3f;
    cfg.optim.lr_opacities = 5e-2f;
    cfg.optim.lr_features_dc = 2.5e-3f;
    cfg.optim.lr_features_sh = 1.25e-4f;
    cfg.optim.sh_value_bits = 32;
    // 0 would clamp every scale to log(0) on the first step.
    cfg.optim.max_gauss_ratio = 10.0f;
    // The shipping default: the 8-bit SH / 16-bit non-SH Adam state behind
    // the fused projection backward, whose per-256-splat bound is the
    // amplifier this tool exists to bound.
    cfg.optim.quantization_level = 1;
    cfg.optim.sh_optim_bits = 8;
    cfg.optim.sh_value_bits = 16;
    cfg.optim.non_sh_optim_bits = 16;
    cfg.optim.use_per_splat_bias_correction = true;
    cfg.optim.use_fused_proj_bwd_optim = true;
    // Both default to 0, and the size clip runs every step regardless of the
    // refine gate -- 0 would clamp every scale to log(0) on step 1.
    cfg.densify.max_screen_size = std::numeric_limits<float>::infinity();
    cfg.densify.max_world_size = std::numeric_limits<float>::infinity();
    cfg.densify.refine_start_iter = 1 << 20;
    cfg.densify.refine_every = 1 << 20;

    s.vm = {
        1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 3.5f,  0, 0, 0, 1,
        1, 0, 0, 0.2f,  0, 1, 0, -0.1f,  0, 0, 1, 4.2f,  0, 0, 0, 1,
    };
    s.intr = {70, 71, 32, 24, 69, 70, 31, 25};
    s.dist.assign(C * kCameraDistortionParams, 0.f);
    s.gt_rgb.resize((size_t)C * H * W * 3);
    s.gt_normal.resize((size_t)C * H * W * 3);
    s.gt_depth.resize((size_t)C * H * W);
    for (auto& v : s.gt_rgb) v = (uint8_t)(rng() & 0xff);
    for (auto& v : s.gt_normal) v = (uint8_t)(rng() & 0xff);
    for (auto& v : s.gt_depth) v = (uint16_t)(1 + (rng() % 4000));
    return s;
}

static std::map<std::string, float> step_n(Scene& s, int from, int to) {
    std::map<std::string, float> last;
    for (int step = from; step <= to; step++)
        last = engine_train_step(
            step, 100, "3dgs", 1, /*packed=*/true, W, H, "PINHOLE", "NONE",
            ttv(s.vm.data(), 4, {C, 4, 4}), ttv(s.intr.data(), 4, {C, 4}),
            ttv(s.dist.data(), 4, {C, kCameraDistortionParams}),
            ttv(s.gt_rgb.data(), 1, {C, H, W, 3}),
            ttv(s.gt_depth.data(), 2, {C, H, W, 1}),
            ttv(s.gt_normal.data(), 1, {C, H, W, 3}),
            ttv_null(), ttv_null(), s.cfg);
    return last;
}

// Splats with at least one non-finite component, across every tensor the PLY
// export filters on (EngineCheckpoint.cpp) -- the same definition of "dead"
// that produced `element vertex 0` in the user's checkpoint.
struct Health {
    int64_t dead = 0;
    const char* first_tensor = "none";
};

static Health count_dead() {
    backend::device_synchronize();
    struct Field { const char* name; const void* ptr; int comps; };
    const Field fields[] = {
        {"means",       engine().world.means.data_ptr(),       3},
        {"quats",       engine().world.quats.data_ptr(),       4},
        {"scales",      engine().world.scales.data_ptr(),      3},
        {"opacities",   engine().world.opacities.data_ptr(),   1},
        {"features_dc", engine().world.features_dc.data_ptr(), 3},
    };
    std::vector<char> bad((size_t)N, 0);
    Health h;
    for (const Field& f : fields) {
        std::vector<float> host((size_t)N * f.comps);
        backend::memcpy_sync(host.data(), f.ptr, host.size() * sizeof(float),
                             MemcpyKind::DeviceToHost);
        for (int64_t i = 0; i < N; i++)
            for (int c = 0; c < f.comps; c++)
                if (!std::isfinite(host[(size_t)i * f.comps + c])) {
                    if (!bad[(size_t)i] && h.first_tensor == std::string("none"))
                        h.first_tensor = f.name;
                    bad[(size_t)i] = 1;
                }
    }
    for (char b : bad) h.dead += b ? 1 : 0;
    return h;
}

// Overwrite one float of one splat tensor on the device.
static void poke(const char* which, int64_t splat, float value) {
    float* p = nullptr;
    if (!std::strcmp(which, "means"))
        p = (float*)engine().world.means.data_ptr() + splat * 3;
    else if (!std::strcmp(which, "scales"))
        p = (float*)engine().world.scales.data_ptr() + splat * 3;
    else if (!std::strcmp(which, "quats"))
        p = (float*)engine().world.quats.data_ptr() + splat * 4;
    else if (!std::strcmp(which, "opacities"))
        p = (float*)engine().world.opacities.data_ptr() + splat;
    else if (!std::strcmp(which, "ppisp"))
        p = (float*)engine().ppisp.params.data_ptr();
    else
        p = (float*)engine().world.features_dc.data_ptr() + splat * 3;
    backend::memcpy_sync(p, &value, sizeof(float), MemcpyKind::HostToDevice);
    backend::device_synchronize();
}

// Train, poison one splat, train on. Returns how many splats are dead at the
// end. `splat` is deliberately mid-block so a poisoned block bound shows up as
// its 255 innocent neighbours dying with it.
static int64_t run_case(const char* which, const char* label,
                        float poison, int64_t splat, int steps_after) {
    engine_reset();
    std::mt19937 rng(20260823u);
    Scene s = build_scene(rng);
    if (!std::strcmp(which, "ppisp")) {
        set_camera_params(W, H, "PINHOLE", "NONE",
                          ttv(s.vm.data(), 4, {C, 4, 4}),
                          ttv(s.intr.data(), 4, {C, 4}),
                          ttv(s.dist.data(), 4, {C, kCameraDistortionParams}));
        engine_init_bilagrid_rgb(C, "ppisp", 8, 16, 16, 8, 16, true);
        engine_init_ppisp(C, "no_crf", true);
    }
    step_n(s, 1, 6);
    Health before = count_dead();
    poke(which, splat, poison);
    auto last = step_n(s, 7, 6 + steps_after);
    Health after = count_dead();
    float loss = 0.0f;
    for (const auto& kv : last)
        if (!std::isfinite(kv.second)) loss = kv.second;

    std::printf("  %-11s %-5s  healthy_before=%lld  dead_after=%lld / %lld"
                "  (first: %s, loss %g)\n",
                which, label, (long long)before.dead, (long long)after.dead,
                (long long)N, after.first_tensor, loss);
    return after.dead;
}

// The optimizer guard: a non-finite gradient or optimizer moment must not turn
// a finite appearance parameter non-finite -- the only path by which one can.
static bool ppisp_grad_case(float poison) {
    engine_reset();
    std::mt19937 rng(20260823u);
    Scene s = build_scene(rng);
    set_camera_params(W, H, "PINHOLE", "NONE",
                      ttv(s.vm.data(), 4, {C, 4, 4}),
                      ttv(s.intr.data(), 4, {C, 4}),
                      ttv(s.dist.data(), 4, {C, kCameraDistortionParams}));
    engine_init_ppisp(C, "no_crf", /*use_adagrad=*/true);
    step_n(s, 1, 2);

    int64_t n = engine().ppisp.params.numel();
    std::vector<float> bad((size_t)n, poison);
    backend::memcpy_sync(engine().ppisp.grads.data_ptr(), bad.data(),
                         bad.size() * sizeof(float), MemcpyKind::HostToDevice);
    backend::memcpy_sync(engine().ppisp.accum_f.data_ptr(), bad.data(),
                         bad.size() * sizeof(float), MemcpyKind::HostToDevice);
    PpispStepConfig pc{};
    pc.lr = 0.1f;
    engine_ppisp_optim_step(3, pc);
    backend::device_synchronize();

    std::vector<float> host((size_t)n);
    backend::memcpy_sync(host.data(), engine().ppisp.params.data_ptr(),
                         host.size() * sizeof(float), MemcpyKind::DeviceToHost);
    int64_t bad_n = 0;
    for (float v : host) if (!std::isfinite(v)) bad_n++;
    std::printf("  ppisp grad=%-5g -> non-finite params %lld / %lld\n",
                poison, (long long)bad_n, (long long)n);
    return bad_n == 0;
}

int main() {
    std::printf("engine_nonfinite_recovery: one poisoned float per case, "
                "%lld splats total\n", (long long)N);

    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    int64_t worst = 0;
    for (const char* which :
         {"features_dc", "means", "scales", "quats", "opacities", "ppisp"}) {
        const int after = std::getenv("LONG") ? 60 : 8;
        worst = std::max(worst, run_case(which, "+inf", inf, 300, after));
        worst = std::max(worst, run_case(which, "nan", nan, 300, after));
    }

    bool grad_ok = ppisp_grad_case(std::numeric_limits<float>::infinity());
    grad_ok &= ppisp_grad_case(std::numeric_limits<float>::quiet_NaN());

    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }

    // One poisoned splat may die. Its block-mates must not: a shared
    // quantization bound is an implementation detail, not a blast radius.
    const int64_t kMaxDead = 1;
    const bool pass = worst <= kMaxDead && grad_ok;
    std::printf("engine_nonfinite_recovery: %s (allowed %lld dead)\n",
                pass ? "PASSED" : "FAILED", (long long)kMaxDead);
    return pass ? 0 : 1;
}
