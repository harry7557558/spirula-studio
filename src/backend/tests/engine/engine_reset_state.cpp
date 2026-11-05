// A second run in the same process must reproduce both the trained splats
// and the metric readbacks, including the first step after engine_reset().

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

struct RunResult {
    std::vector<float> splats;
    std::vector<float> metrics;
};

static RunResult run_once() {
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
                  ttv(sh.data(), 4, {N, NUM_SH, 3}), 0);

    EngineStepConfig cfg{};
    auto W_ = [&](LossWeightIndex i) -> float& {
        return cfg.loss.weights[(int)i];
    };
    W_(LossWeightIndex::RgbSupL1) = 0.8f;
    W_(LossWeightIndex::RgbSupL2) = 0.2f;
    W_(LossWeightIndex::DepthSup) = 0.5f;
    W_(LossWeightIndex::NormalSup) = 0.5f;
    cfg.loss.w_ssim = 0.2f;
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

    RunResult result;
    for (int step = 1; step <= 8; step++) {
        const auto losses = engine_train_step(
            step, 100, "3dgs", 1, /*packed=*/false, W, H, "PINHOLE", "NONE",
            ttv(vm.data(), 4, {C, 4, 4}), ttv(intr.data(), 4, {C, 4}),
            ttv(dist.data(), 4, {C, kCameraDistortionParams}),
            ttv(gt_rgb.data(), 1, {C, H, W, 3}),
            ttv(gt_depth.data(), 2, {C, H, W, 1}),
            ttv(gt_normal.data(), 1, {C, H, W, 3}),
            ttv_null(), ttv_null(), cfg);
        for (const char* key : {"psnr", "ssim", "rgb_loss"})
            result.metrics.push_back(losses.at(key));
    }

    backend::device_synchronize();
    result.splats.resize((size_t)N * 11);
    float* p = result.splats.data();
    auto grab = [&](const void* d, int64_t n) {
        backend::memcpy_sync(p, d, n * sizeof(float), MemcpyKind::DeviceToHost);
        p += n;
    };
    grab(engine().world.means.data_ptr(), N * 3);
    grab(engine().world.quats.data_ptr(), N * 4);
    grab(engine().world.opacities.data_ptr(), N);
    grab(engine().world.features_dc.data_ptr(), N * 3);
    return result;
}

int main() {
    RunResult first = run_once();
    engine_reset();
    RunResult second = run_once();

    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }

    double max_abs = 0.0;
    int64_t viol = 0;
    for (size_t i = 0; i < first.splats.size(); i++) {
        double d = std::fabs((double)first.splats[i] - (double)second.splats[i]);
        max_abs = std::max(max_abs, d);
        // Atomic accumulation order is the only legitimate difference, and it
        // lands around 1e-4; a lost cotangent seed moves splats by whole units.
        if (d > 5e-3 + 1e-3 * std::fabs((double)first.splats[i])) viol++;
    }
    int metric_viol = 0;
    for (size_t i = 0; i < first.metrics.size(); i++) {
        const float a = first.metrics[i], b = second.metrics[i];
        if (!std::isfinite(a) || !std::isfinite(b) ||
            std::fabs(a - b) > 5e-4f + 1e-4f * std::fabs(a)) {
            std::fprintf(stderr, "metric %zu: first %.7g, second %.7g\n", i, a, b);
            metric_viol++;
        }
    }
    const bool pass = viol == 0 && metric_viol == 0;
    std::printf("engine_reset_state: %zu floats (max_abs %.3g, violations "
                "%lld), metric violations %d\nengine_reset_state: %s\n",
                first.splats.size(), max_abs, (long long)viol, metric_viol,
                pass ? "PASSED" : "FAILED");
    return pass ? 0 : 1;
}
