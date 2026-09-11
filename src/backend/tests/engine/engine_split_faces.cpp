#include <engine/Engine.h>
#include <engine/EngineInternal.h>
#include <engine/EngineState.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using backend::MemcpyKind;

static constexpr int64_t N = 2048;
static constexpr int C = 6, W = 64, H = 64;
static constexpr int IW = 128, IH = 64, NIW = 64, NIH = 32;
static constexpr int NUM_SH = 4;

static TorchTensorView tv(const void* p, uint32_t elem,
                          std::vector<int64_t> shape) {
    return {(uint64_t)p, elem, std::move(shape)};
}

static TorchTensorView null_tv() {
    return {0, 4, {0}};
}

static std::vector<float> make_axes() {
    struct V3 { float x, y, z; };
    auto norm = [](V3 a) {
        float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
        return V3{a.x / l, a.y / l, a.z / l};
    };
    auto cross = [](V3 a, V3 b) {
        return V3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
    };
    const V3 directions[C] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
        {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    };
    std::vector<float> axes;
    for (V3 z : directions) {
        V3 up = std::fabs(z.y) > 0.9f ? V3{0, 0, 1} : V3{0, 1, 0};
        V3 x = norm(cross(up, z));
        V3 y = cross(z, x);
        const float frame[9] = {
            x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z,
        };
        axes.insert(axes.end(), frame, frame + 9);
    }
    return axes;
}

struct Inputs {
    std::vector<float> means, quats, scales, opacities, dc, sh;
    std::vector<float> viewmats, intrins, distortion, axes;
    std::vector<float> input_intrins, input_distortion;
    std::vector<uint8_t> rgb, normal;
    std::vector<uint16_t> depth;
    std::vector<int32_t> camera_indices;
};

static Inputs make_inputs() {
    std::mt19937 rng(918273u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    Inputs x;
    x.means.resize(N * 3);
    x.quats.resize(N * 4);
    x.scales.resize(N * 3);
    x.opacities.resize(N);
    x.dc.resize(N * 3);
    x.sh.resize(N * NUM_SH * 3);
    for (int64_t i = 0; i < N; ++i) {
        x.means[3 * i] = uf(-2.0f, 2.0f);
        x.means[3 * i + 1] = uf(-2.0f, 2.0f);
        x.means[3 * i + 2] = uf(-0.5f, 5.0f);
        x.quats[4 * i] = 1.0f;
        x.quats[4 * i + 1] = uf(-0.1f, 0.1f);
        x.quats[4 * i + 2] = uf(-0.1f, 0.1f);
        x.quats[4 * i + 3] = uf(-0.1f, 0.1f);
        for (int j = 0; j < 3; ++j) {
            x.scales[3 * i + j] = uf(-4.5f, -2.0f);
            x.dc[3 * i + j] = uf(-0.25f, 0.75f);
        }
        x.opacities[i] = uf(-1.0f, 3.0f);
    }
    for (float& v : x.sh) v = uf(-0.1f, 0.1f);

    x.viewmats.resize(C * 16, 0.0f);
    x.intrins.resize(C * 4);
    x.distortion.resize(C * kCameraDistortionParams, 0.0f);
    x.camera_indices.resize(C);
    x.axes = make_axes();
    x.input_intrins = {(float)IW, (float)IH, 0.5f * IW, 0.5f * IH};
    x.input_distortion.assign(kCameraDistortionParams, 0.0f);
    for (int c = 0; c < C; ++c) {
        float* m = x.viewmats.data() + c * 16;
        for (int r = 0; r < 3; ++r)
            for (int j = 0; j < 3; ++j)
                m[4 * r + j] = x.axes[9 * c + 3 * r + j];
        m[15] = 1.0f;
        m[11] = 3.5f;
        x.intrins[4 * c] = 50.0f;
        x.intrins[4 * c + 1] = 50.0f;
        x.intrins[4 * c + 2] = 0.5f * W;
        x.intrins[4 * c + 3] = 0.5f * H;
        x.camera_indices[c] = c;
    }
    x.rgb.resize((size_t)IH * IW * 3);
    x.normal.resize((size_t)NIH * NIW * 3);
    for (int y = 0; y < IH; ++y)
        for (int x0 = 0; x0 < IW; ++x0) {
            size_t p = ((size_t)y * IW + x0) * 3;
            x.rgb[p] = (uint8_t)(255 * x0 / (IW - 1));
            x.rgb[p + 1] = (uint8_t)(255 * y / (IH - 1));
            x.rgb[p + 2] = (uint8_t)((13 * x0 + 7 * y) & 255);
        }
    for (size_t i = 0; i < x.normal.size(); i += 3) {
        x.normal[i] = 128;
        x.normal[i + 1] = 128;
        x.normal[i + 2] = 255;
    }
    x.depth.resize((size_t)IH * IW);
    for (uint16_t& v : x.depth) v = (uint16_t)(1 + rng() % 4000);
    return x;
}

static void append_device(std::vector<float>& out, const float* ptr,
                          int64_t count) {
    size_t at = out.size();
    out.resize(at + (size_t)count);
    backend::memcpy_sync(out.data() + at, ptr, (size_t)count * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

struct Result {
    std::vector<float> splats, ppisp, rgb_grid, normal_grid, background;
};

static Result run(const Inputs& x, bool split, int steps, bool fail = false) {
    engine_reset();
    set_data_3dgs(N, tv(x.means.data(), 4, {N, 3}),
                  tv(x.quats.data(), 4, {N, 4}),
                  tv(x.scales.data(), 4, {N, 3}),
                  tv(x.opacities.data(), 4, {N, 1}),
                  tv(x.dc.data(), 4, {N, 3}),
                  tv(x.sh.data(), 4, {N, NUM_SH, 3}));
    set_camera_params(W, H, "PINHOLE", "NONE",
                      tv(x.viewmats.data(), 4, {C, 4, 4}),
                      tv(x.intrins.data(), 4, {C, 4}),
                      tv(x.distortion.data(), 4,
                         {C, kCameraDistortionParams}));
    engine_init_bilagrid_rgb(C, "affine", 4, 4, 4, 32, 32, false);
    engine_init_bilagrid_normal(C, 4, 4, 4, 32, 32, false);
    engine_init_ppisp(C, "no_crf", false);
    engine_init_background_sh(1, 0, false);

    EngineStepConfig cfg{};
    cfg.loss.weights[(int)LossWeightIndex::RgbSupL1] = 1.0f;
    cfg.loss.weights[(int)LossWeightIndex::NormalSup] = 0.5f;
    cfg.loss.weights[(int)LossWeightIndex::DepthSup] = 0.5f;
    cfg.loss.w_ssim = 0.2f;
    cfg.optim.split_batch = split;
    cfg.optim.lr_means = 1.6e-4f;
    cfg.optim.lr_quats = 1e-3f;
    cfg.optim.lr_scales = 5e-3f;
    cfg.optim.lr_opacities = 5e-2f;
    cfg.optim.lr_features_dc = 2.5e-3f;
    cfg.optim.lr_features_sh = 1.25e-4f;
    cfg.optim.max_gauss_ratio = 10.0f;
    cfg.optim.quantization_level = 0;
    cfg.optim.sh_optim_bits = 32;
    cfg.optim.sh_value_bits = 32;
    cfg.optim.non_sh_optim_bits = 32;
    cfg.optim.use_per_splat_bias_correction = false;
    cfg.optim.use_fused_proj_bwd_optim = false;
    cfg.densify.max_screen_size = 1e6f;
    cfg.densify.max_world_size = 1e6f;
    cfg.densify.refine_start_iter = 1 << 20;
    cfg.densify.refine_every = 1 << 20;
    cfg.loss.color_shift_reg_weight = 0.0f;
    cfg.loss.color_shift_reg_beta = 0.0f;
    cfg.loss.overexposure_reg_weight = 0.0f;
    cfg.bilagrid.lr_rgb = 1e-3f;
    cfg.bilagrid.lr_normal = 1e-3f;
    cfg.bilagrid.tv_weight_rgb = 0.0f;
    cfg.bilagrid.tv_weight_normal = 0.0f;
    cfg.ppisp.lr = 1e-3f;
    cfg.ppisp.reg_weights[0] = 0.0f;
    cfg.background.lr_dc = 1e-3f;
    cfg.background.lr_sh = 1e-3f;

    auto train = [&](int step, bool do_split) {
        std::vector<WarpFacePass> passes;
        if (do_split)
            for (int k = 0; k < C; ++k)
                passes.push_back({k, k + 1, W, H});
        EngineStepConfig step_cfg = cfg;
        step_cfg.optim.split_batch = do_split;
        engine_train_step_warped(
            step, 100, "3dgs", 1, false, W, H,
            tv(x.viewmats.data(), 4, {C, 4, 4}),
            tv(x.intrins.data(), 4, {C, 4}),
            tv(x.distortion.data(), 4, {C, kCameraDistortionParams}),
            "EQUIRECTANGULAR", "NONE", 1, IH, IW, C,
            tv(x.input_intrins.data(), 4, {1, 4}),
            tv(x.input_distortion.data(), 4, {1, kCameraDistortionParams}),
            null_tv(), null_tv(),
            tv(x.rgb.data(), 1, {1, IH, IW, 3}), null_tv(), 0, 0,
            tv(x.depth.data(), 2, {1, IH, IW, 1}), IH, IW,
            tv(x.normal.data(), 1, {1, NIH, NIW, 3}), NIH, NIW,
            tv(x.axes.data(), 4, {C, 3, 3}), passes,
            tv(x.camera_indices.data(), 4, {C}), step_cfg);
    };
    if (fail) {
        std::string failure;
        _engine_test_fail_split_after_pass(1);
        try {
            train(1, true);
        } catch (const std::runtime_error& e) {
            failure = e.what();
        } catch (...) {
            _engine_test_fail_split_after_pass(-1);
            throw;
        }
        _engine_test_fail_split_after_pass(-1);
        if (failure != "injected split-pass failure")
            throw std::runtime_error("unexpected split failure: " + failure);
        const auto& optim = engine().optim;
        if (engine().bilagrid_split_grad_active ||
            optim.skip_grad_zero || optim.zero_grad_in_optim ||
            optim.grad_scale != 1.0f || optim.loss_grad_scale != 1.0f)
            throw std::runtime_error("split state was not restored");
    }
    for (int step = 1; step <= steps; ++step) train(step, split);
    backend::device_synchronize();

    Result r;
    append_device(r.splats, (const float*)engine().world.means.data_ptr(), N * 3);
    append_device(r.splats, (const float*)engine().world.features_dc.data_ptr(), N * 3);
    append_device(r.ppisp, engine().ppisp.params.data_ptr(),
                  engine().ppisp.params.numel());
    append_device(r.rgb_grid, engine().bilagrid_rgb.grids.data_ptr(),
                  engine().bilagrid_rgb.grids.numel());
    append_device(r.normal_grid, engine().bilagrid_normal.grids.data_ptr(),
                  engine().bilagrid_normal.grids.numel());
    append_device(r.background,
                  (const float*)engine().background.sh_coeffs.data_ptr(),
                  engine().background.sh_coeffs.size() * 3);
    return r;
}

static bool compare(const char* name, const std::vector<float>& a,
                    const std::vector<float>& b, float tolerance,
                    double max_fraction, double max_rel_rms) {
    float worst = 0.0f;
    size_t worst_i = 0, violations = 0;
    double se = 0.0, sr = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        float d = std::fabs(a[i] - b[i]);
        if (d > worst) { worst = d; worst_i = i; }
        if (d > tolerance) ++violations;
        se += (double)d * d;
        sr += (double)a[i] * a[i];
    }
    const double fraction = a.empty() ? 0.0 : (double)violations / a.size();
    const double rel_rms = sr == 0.0 ? 0.0 : std::sqrt(se / sr);
    std::printf("  %-12s max_abs %.6g @ %zu, violations %.3f%%, rel_rms %.4g\n",
                name, worst, worst_i, 100.0 * fraction, rel_rms);
    return a.size() == b.size() && fraction <= max_fraction &&
           rel_rms <= max_rel_rms;
}
static bool check_exception_restore(const Inputs& inputs) {
    bool ok = false;
    try {
        Result recovered = run(inputs, true, 2, true);
        Result fresh = run(inputs, true, 2);
        Result fresh_repeat = run(inputs, true, 2);
        ok = compare("fresh splats", fresh.splats, fresh_repeat.splats,
                     1e-6, 0.0, 1e-6);
        ok = compare("fresh ppisp", fresh.ppisp, fresh_repeat.ppisp,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("fresh rgb", fresh.rgb_grid, fresh_repeat.rgb_grid,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("fresh normal", fresh.normal_grid,
                     fresh_repeat.normal_grid, 1e-6, 0.0, 1e-6) && ok;
        ok = compare("fresh background", fresh.background,
                     fresh_repeat.background, 1e-6, 0.0, 1e-6) && ok;
        ok = compare("recovered splats", recovered.splats, fresh.splats,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("recovered ppisp", recovered.ppisp, fresh.ppisp,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("recovered rgb", recovered.rgb_grid, fresh.rgb_grid,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("recovered normal", recovered.normal_grid,
                     fresh.normal_grid, 1e-6, 0.0, 1e-6) && ok;
        ok = compare("recovered background", recovered.background,
                     fresh.background, 1e-6, 0.0, 1e-6) && ok;
    } catch (const std::exception& e) {
        std::printf("engine_split_faces: exception recovery FAILED: %s\n",
                    e.what());
    }
    engine_reset();
    std::printf("engine_split_faces: exception recovery %s\n",
                ok ? "ok" : "FAILED");
    return ok;
}

static bool check_steps(const Inputs& inputs, int steps) {
    std::printf("engine_split_faces: %d-step unsplit\n", steps);
    Result unsplit = run(inputs, false, steps);
    std::printf("engine_split_faces: %d-step split\n", steps);
    Result split = run(inputs, true, steps);
    bool ok = compare("splats", unsplit.splats, split.splats,
                      5e-3f, 0.08, 2e-3);
    ok = compare("ppisp", unsplit.ppisp, split.ppisp,
                 1e-5f, 0.0, 1e-3) && ok;
    // Two Adam steps amplify split-order roundoff; 1e-4 stays 50x below the engine parity gate.
    ok = compare("rgb grid", unsplit.rgb_grid, split.rgb_grid,
                 1e-4f, 0.0, 2e-3) && ok;
    ok = compare("normal grid", unsplit.normal_grid, split.normal_grid,
                 1e-4f, 0.0, 5e-3) && ok;
    ok = compare("background", unsplit.background, split.background,
                 1e-5f, 0.0, 2e-3) && ok;
    return ok;
}

int main() {
    Inputs inputs = make_inputs();
    bool ok = check_exception_restore(inputs);
    ok = check_steps(inputs, 1) && ok;
    ok = check_steps(inputs, 2) && ok;
    engine_reset();
    std::printf(ok ? "engine_split_faces: PASSED\n" : "engine_split_faces: FAILED\n");
    return ok ? 0 : 1;
}
