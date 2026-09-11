// engine_reset() must leave NO state behind: the second scene in a process
// has to train exactly like the first. The GUI is the reason -- "Train again"
// after a stop reuses the process, and every buffer the previous run allocated
// has been freed by then, so any "already uploaded" flag that outlives the
// reset points at memory nobody wrote.
//
// Self-checking (no reference file): the same seeded scene is trained twice
// with engine_reset() between the runs and the splats must land in the same
// place. Only the per-pixel loss drives the optimizer here (w_ssim = 0), so a
// lost cotangent seed shows up as splats that never moved.

#include <engine/Engine.h>
#include <engine/EngineState.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using backend::MemcpyKind;

static constexpr int64_t N = 800;
static constexpr int NUM_SH = 4;   // degree 1 buffer
static constexpr int C = 2, W = 64, H = 48;

static TorchTensorView ttv(const void* p, uint32_t elem,
                           std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, elem, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

// One full run: seed the scene, train a few steps, read the splats back.
static std::vector<float> run_once() {
    std::mt19937 rng(20260815u);
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

    EngineStepConfig cfg{};
    auto W_ = [&](LossWeightIndex i) -> float& {
        return cfg.loss.weights[(int)i];
    };
    W_(LossWeightIndex::RgbSupL1) = 0.8f;
    W_(LossWeightIndex::RgbSupL2) = 0.2f;
    W_(LossWeightIndex::DepthSup) = 0.5f;
    W_(LossWeightIndex::NormalSup) = 0.5f;
    // The whole optimizer signal comes through the per-pixel loss cotangents.
    cfg.loss.w_ssim = 0.0f;
    cfg.loss.num_loss_scales = 1;
    cfg.optim.lr_means = 1.6e-3f;
    cfg.optim.lr_quats = 1e-2f;
    cfg.optim.lr_scales = 5e-2f;
    cfg.optim.lr_opacities = 5e-1f;
    cfg.optim.lr_features_dc = 2.5e-2f;
    cfg.optim.lr_features_sh = 1.25e-3f;
    cfg.optim.sh_value_bits = 32;
    cfg.densify.refine_start_iter = 1 << 20;
    cfg.densify.refine_every = 1 << 20;

    std::vector<float> vm = {
        1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 3.5f,  0, 0, 0, 1,
        1, 0, 0, 0.2f,  0, 1, 0, -0.1f,  0, 0, 1, 4.2f,  0, 0, 0, 1,
    };
    std::vector<float> intr = {70, 71, 32, 24, 69, 70, 31, 25};
    std::vector<float> dist(C * kCameraDistortionParams, 0.f);

    std::vector<uint8_t> gt_rgb((size_t)C * H * W * 3),
                         gt_normal((size_t)C * H * W * 3);
    std::vector<uint16_t> gt_depth((size_t)C * H * W);
    for (auto& v : gt_rgb) v = (uint8_t)(rng() & 0xff);
    for (auto& v : gt_normal) v = (uint8_t)(rng() & 0xff);
    for (auto& v : gt_depth) v = (uint16_t)(1 + (rng() % 4000));

    for (int step = 1; step <= 8; step++)
        engine_train_step(
            step, 100, "3dgs", 1, /*packed=*/false, W, H, "PINHOLE", "NONE",
            ttv(vm.data(), 4, {C, 4, 4}), ttv(intr.data(), 4, {C, 4}),
            ttv(dist.data(), 4, {C, kCameraDistortionParams}),
            ttv(gt_rgb.data(), 1, {C, H, W, 3}),
            ttv(gt_depth.data(), 2, {C, H, W, 1}),
            ttv(gt_normal.data(), 1, {C, H, W, 3}),
            ttv_null(), ttv_null(), cfg);

    backend::device_synchronize();
    std::vector<float> out((size_t)N * 11);
    float* p = out.data();
    auto grab = [&](const void* d, int64_t n) {
        backend::memcpy_sync(p, d, n * sizeof(float), MemcpyKind::DeviceToHost);
        p += n;
    };
    grab(engine().world.means.data_ptr(), N * 3);
    grab(engine().world.quats.data_ptr(), N * 4);
    grab(engine().world.opacities.data_ptr(), N);
    grab(engine().world.features_dc.data_ptr(), N * 3);
    return out;
}

int main() {
    std::vector<float> first = run_once();
    backend::BudgetSnapshot live = backend::budget_snapshot();
    if (live.status != backend::BudgetStatus::Available) {
        std::fprintf(stderr, "engine_reset_state: memory telemetry unavailable\n");
        return 1;
    }
    backend::training_budget_begin(0, live.process_bytes);
    bool refused = false;
    try {
        backend::device_malloc_checked(1ull << 20, "forced_after_setup");
    } catch (const backend::BudgetError& e) {
        refused =
            e.failure.kind == backend::BudgetFailureKind::ApplicationLimit;
    }
    backend::training_budget_end();
    if (!refused) {
        std::fprintf(stderr, "engine_reset_state: low budget was not refused\n");
        return 1;
    }
    engine_reset();
    std::vector<float> second = run_once();
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }

    double max_abs = 0.0;
    int64_t viol = 0;
    for (size_t i = 0; i < first.size(); i++) {
        double d = std::fabs((double)first[i] - (double)second[i]);
        max_abs = std::max(max_abs, d);
        // Atomic accumulation order is the only legitimate difference, and it
        // lands around 1e-4; a lost cotangent seed moves splats by whole units.
        if (d > 5e-3 + 1e-3 * std::fabs((double)first[i])) viol++;
    }
    const bool pass = viol == 0;
    std::printf("engine_reset_state: %zu floats (max_abs %.3g, violations "
                "%lld)\nengine_reset_state: %s\n",
                first.size(), max_abs, (long long)viol,
                pass ? "PASSED" : "FAILED");
    return pass ? 0 : 1;
}
