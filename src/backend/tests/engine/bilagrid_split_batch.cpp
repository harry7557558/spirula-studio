// A split-batch step must give every camera's bilateral grid and PPISP row the update the same batch gives them unsplit.
#include "engine/Engine.h"
#include "engine/EngineState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
constexpr int kGrids = 5, kL = 4, kGH = 6, kGW = 6, kC = 12;
constexpr int kCells = kL * kGH * kGW * kC;

TorchTensorView view(const void* p, std::vector<int64_t> shape) {
    return TorchTensorView((uint64_t)p, 4u, std::move(shape));
}

enum class Mode { Whole, Split, Mixed };
struct Trained { std::vector<float> grids, ppisp; };

// Three steps of three cameras each; returns the per-camera tables afterwards.
// Mixed hands the batch over as the sub-batches of a mixed-resolution step.
Trained train(Mode mode, bool adagrad) {
    engine_reset();
    constexpr int n = 12, width = 40, height = 32, batch = 3;
    std::vector<float> means, quats, scales, opacities(n, 1.0f), dc(n * 3), sh(n * 9, 0);
    for (int i = 0; i < n; i++) {
        means.insert(means.end(), {0.5f * (i % 4) - 0.75f, 0.5f * (i / 4) - 0.5f, 3.0f});
        quats.insert(quats.end(), {1, 0, 0, 0});
        scales.insert(scales.end(), {-1.4f, -1.2f, -1.6f});
        for (int c = 0; c < 3; c++) dc[i * 3 + c] = 0.2f * ((i + c) % 5) - 0.3f;
    }
    set_data_3dgs(n, view(means.data(), {n, 3}), view(quats.data(), {n, 4}),
        view(scales.data(), {n, 3}), view(opacities.data(), {n, 1}), view(dc.data(), {n, 3}),
        view(sh.data(), {n, 3, 3}), 0);
    engine_init_bilagrid_rgb(kGrids, "affine", kL, kGH, kGW, 32, 32, adagrad);
    // Not the response curve: its gradient here is rounding noise, which the terms amplify.
    engine_init_ppisp(kGrids, "no_crf", adagrad, true);

    EngineStepConfig cfg{};
    cfg.optim.split_batch = mode != Mode::Whole;
    cfg.optim.use_fused_proj_bwd_optim = false;
    cfg.optim.keep_fp32_geometry_grad = true;
    cfg.optim.sh_value_bits = 32;
    cfg.optim.sh_optim_bits = 32;
    cfg.optim.non_sh_optim_bits = 32;
    cfg.loss.weights[(int)LossWeightIndex::RgbSupL1] = 1.0f;
    cfg.bilagrid.lr_rgb = 0.01f;
    cfg.bilagrid.tv_weight_rgb = 200.0f;
    // Terms strong enough that a wrong image-to-term balance shows in three steps.
    cfg.ppisp.lr = 0.01f;
    cfg.ppisp.reg_weights.fill(0.5f);
    cfg.densify.refine_start_iter = 10000;
    cfg.densify.refine_every = 100;
    cfg.densify.max_screen_size = std::numeric_limits<float>::infinity();
    cfg.densify.max_world_size = std::numeric_limits<float>::infinity();

    std::vector<float> cameras, intrinsics, distortion(batch * kCameraDistortionParams, 0);
    for (int b = 0; b < batch; b++) {
        const float shift = 0.3f * (b - 1);
        cameras.insert(cameras.end(), {1,0,0,shift, 0,1,0,0, 0,0,1,0, 0,0,0,1});
        intrinsics.insert(intrinsics.end(), {40, 40, 20, 16});
    }
    const int32_t picks[3][batch] = {{4, 1, 2}, {0, 3, 1}, {2, 4, 0}};
    const auto empty = view(nullptr, {0});
    for (int step = 1; step <= 3; step++) {
        std::vector<float> rgb((size_t)batch * height * width * 3);
        for (int b = 0; b < batch; b++)
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    for (int c = 0; c < 3; c++)
                        rgb[(((size_t)b * height + y) * width + x) * 3 + c] =
                            0.5f + 0.4f * std::sin(0.2f * x * (b + 1) + 0.3f * y + c + step);
        // Cameras [first, first + count) of the batch.
        auto part = [&](int first, int count) {
            HeteroSubBatch sub;
            sub.width = width; sub.height = height; sub.num = count;
            sub.camera_model = "PINHOLE"; sub.distortion = "NONE";
            sub.viewmats = view(cameras.data() + first * 16, {count, 4, 4});
            sub.intrins = view(intrinsics.data() + first * 4, {count, 4});
            sub.dist_coeffs = view(distortion.data() + first * kCameraDistortionParams,
                                   {count, kCameraDistortionParams});
            sub.gt_rgb = view(rgb.data() + (size_t)first * height * width * 3,
                              {count, height, width, 3});
            sub.gt_depth = sub.gt_normal = sub.gt_alpha = empty;
            sub.bilagrid_cam_indices = view(picks[step - 1] + first, {count, 1});
            return sub;
        };
        if (mode == Mode::Mixed) {
            engine_train_step_hetero(step, 100, "3dgs", 0, false, {part(0, 2), part(2, 1)}, cfg);
            continue;
        }
        const HeteroSubBatch all = part(0, batch);
        engine_train_step(step, 100, "3dgs", 0, false, width, height, "PINHOLE", "NONE",
            all.viewmats, all.intrins, all.dist_coeffs, all.gt_rgb, empty, empty, empty,
            all.bilagrid_cam_indices, cfg);
    }
    backend::device_synchronize();
    Trained out;
    out.grids.resize((size_t)kGrids * kCells);
    backend::memcpy_sync(out.grids.data(), engine().bilagrid_rgb.grids.data_ptr(),
                         out.grids.size() * sizeof(float), backend::MemcpyKind::DeviceToHost);
    out.ppisp.resize((size_t)kGrids * engine().ppisp.num_params);
    backend::memcpy_sync(out.ppisp.data(), engine().ppisp.params.data_ptr(),
                         out.ppisp.size() * sizeof(float), backend::MemcpyKind::DeviceToHost);
    return out;
}

bool run(Mode mode, bool adagrad) {
    const Trained whole_run = train(Mode::Whole, adagrad), split_run = train(mode, adagrad);
    const std::vector<float>& whole = whole_run.grids;
    const std::vector<float>& split = split_run.grids;
    // What the least-moved camera's grid moved by, and how far apart the runs are.
    float moved = std::numeric_limits<float>::infinity(), apart = 0;
    for (int g = 0; g < kGrids; g++) {
        float most = 0;
        for (int i = 0; i < kCells; i++) {
            const float identity = (i % kC) % 5 == 0 ? 1.0f : 0.0f;
            const float a = whole[(size_t)g * kCells + i], b = split[(size_t)g * kCells + i];
            most = std::max(most, std::fabs(b - identity));
            apart = std::max(apart, std::fabs(a - b));
        }
        moved = std::min(moved, most);
    }
    float ppisp_apart = 0;
    for (size_t i = 0; i < whole_run.ppisp.size(); i++)
        ppisp_apart = std::max(ppisp_apart, std::fabs(whole_run.ppisp[i] - split_run.ppisp[i]));
    const bool ok = std::isfinite(apart) && moved > 5e-3f && apart < 2e-4f &&
                    std::isfinite(ppisp_apart) && ppisp_apart < 2e-4f;
    std::printf("%s %s %s: every grid moved (least %.4f), grids within %.2g and PPISP "
                "within %.2g of unsplit\n",
                ok ? "ok" : "FAIL", mode == Mode::Split ? "split" : "mixed",
                adagrad ? "adagrad" : "adam", moved, apart, ppisp_apart);
    return ok;
}
}  // namespace

int main() {
    bool ok = true;
    for (Mode mode : {Mode::Split, Mode::Mixed}) {
        ok &= run(mode, false);
        ok &= run(mode, true);
    }
    engine_reset();
    if (const char* error = backend::last_error()) { std::fprintf(stderr, "%s\n", error); ok = false; }
    return ok ? 0 : 1;
}
