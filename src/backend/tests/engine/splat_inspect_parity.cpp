// The per-splat developer view (engine/EngineInspect.cpp, shaders/
// splat_expr.slang) read through every layout a run can store an attribute
// in. One seeded scene is trained one step three ways -- fp32, quantized
// (level 1), and quantized through the fused projection-backward optimizer.
// A plain buffer must read back exactly what the engine holds; an attribute a
// quantized run decodes must match the fp32 run's plain floats to the codec's
// precision, which a table row with the wrong stride, offset or bound layout
// misses by the attribute's whole range. Compound expressions are then held
// to eval_expr (core/ExprProgram.h) over the device's own single-attribute
// reads. Self-checking; Vulkan only.

#include <engine/Engine.h>
#include <engine/EngineState.h>

#include "core/ExprProgram.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

#ifndef SS_BACKEND_VULKAN
int main() {
    std::printf("splat_inspect_parity: skipped, the evaluator is Vulkan-only\n");
    return 0;
}
#else

static constexpr int64_t N = 800;   // three whole 256-splat blocks and a partial one
// Degree 1, so no band sits out the warmup (the fused path zero-encodes
// those), and 9 cells a row: the odd count leaves FPBO's SH layout a pad cell.
static constexpr int SH_DEGREE = 1;
static constexpr int NUM_SH = 3;
static constexpr int C = 2, W = 64, H = 48;

static TorchTensorView ttv(const void* p, uint32_t elem, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, elem, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

enum class Run { Fp32, Quant, QuantFused };

// Read through a codec in the quantized runs: compared across runs.
static const char* kDecoded[] = {
    "sh_norm", "v_mean_y", "v_quat_x", "v_scale_z", "v_opacity", "v_dc_b", "v_sh_norm",
    "g1_mean_x", "g2_mean_z", "g1_quat_y", "g2_quat_w", "g1_scale_x", "g2_scale_y",
    "g1_opacity", "g2_opacity", "g1_dc_r", "g2_dc_b", "g1_sh_norm", "g2_sh_norm",
};

struct Result {
    std::map<std::string, std::vector<float>> values;   // engine_eval_splat_expr
    std::map<std::string, std::vector<float>> plain;    // the same, from the buffer
    std::map<std::string, SplatAttribute> attrs;
};

static std::vector<float> download(const void* d, int64_t n) {
    std::vector<float> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * 4, backend::MemcpyKind::DeviceToHost);
    return h;
}

// Component `c` of a [N, stride] buffer, through `f`.
static std::vector<float> column(const std::vector<float>& rows, int stride, int c,
                                 const std::function<float(float)>& f = nullptr) {
    std::vector<float> out((size_t)N);
    for (int64_t i = 0; i < N; i++) {
        const float v = rows[(size_t)(i * stride + c)];
        out[(size_t)i] = f ? f(v) : v;
    }
    return out;
}

static std::vector<float> row_norm(const std::vector<float>& rows, int stride) {
    std::vector<float> out((size_t)N);
    for (int64_t i = 0; i < N; i++) {
        double s = 0.0;
        for (int c = 0; c < stride; c++) s += (double)rows[(size_t)(i * stride + c)] * rows[(size_t)(i * stride + c)];
        out[(size_t)i] = (float)std::sqrt(s);
    }
    return out;
}

static float bits_lo16(float v) { uint32_t u; std::memcpy(&u, &v, 4); return (float)(u & 0xffffu); }
static float bits_hi16(float v) { uint32_t u; std::memcpy(&u, &v, 4); return (float)(u >> 16); }

static Result run_once(Run run) {
    std::mt19937 rng(20261018u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N), dc(N * 3),
        sh(N * NUM_SH * 3);
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
    set_data_3dgs(N, ttv(means.data(), 4, {N, 3}), ttv(quats.data(), 4, {N, 4}),
                  ttv(scales.data(), 4, {N, 3}), ttv(opac.data(), 4, {N, 1}),
                  ttv(dc.data(), 4, {N, 3}), ttv(sh.data(), 4, {N, NUM_SH, 3}), 0);

    EngineStepConfig cfg{};
    cfg.loss.weights[(int)LossWeightIndex::RgbSupL1] = 0.8f;
    cfg.loss.weights[(int)LossWeightIndex::RgbSupL2] = 0.2f;
    cfg.loss.w_ssim = 0.0f;
    cfg.loss.num_loss_scales = 1;
    cfg.optim.lr_means = 1.6e-3f;
    cfg.optim.lr_quats = 1e-2f;
    cfg.optim.lr_scales = 5e-2f;
    cfg.optim.lr_opacities = 5e-2f;
    cfg.optim.lr_features_dc = 2.5e-2f;
    cfg.optim.lr_features_sh = 1.25e-3f;
    cfg.optim.max_gauss_ratio = 10.0f;
    const bool quant = run != Run::Fp32;
    cfg.optim.quantization_level = quant ? 1 : 0;
    cfg.optim.sh_optim_bits = quant ? 8 : 32;
    cfg.optim.sh_value_bits = quant ? 16 : 32;
    cfg.optim.non_sh_optim_bits = quant ? 16 : 32;
    cfg.optim.image_bits = 32;
    cfg.optim.use_fused_proj_bwd_optim = run == Run::QuantFused;
    cfg.densify.refine_start_iter = 1 << 20;
    cfg.densify.refine_every = 1 << 20;

    std::vector<float> vm = {
        1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 3.5f,  0, 0, 0, 1,
        1, 0, 0, 0.2f,  0, 1, 0, -0.1f,  0, 0, 1, 4.2f,  0, 0, 0, 1,
    };
    std::vector<float> intr = {70, 71, 32, 24, 69, 70, 31, 25};
    std::vector<float> dist(C * kCameraDistortionParams, 0.f);
    std::vector<uint8_t> gt_rgb((size_t)C * H * W * 3);
    for (auto& v : gt_rgb) v = (uint8_t)(rng() & 0xff);

    engine_train_step(1, 100, "3dgs", SH_DEGREE, /*packed=*/false, W, H, "PINHOLE", "NONE",
                      ttv(vm.data(), 4, {C, 4, 4}), ttv(intr.data(), 4, {C, 4}),
                      ttv(dist.data(), 4, {C, kCameraDistortionParams}),
                      ttv(gt_rgb.data(), 1, {C, H, W, 3}), ttv_null(), ttv_null(),
                      ttv_null(), ttv_null(), cfg);
    backend::device_synchronize();

    Result r;
    const EngineState& e = engine();
    const std::vector<float> w_means = download(e.world.means.data_ptr(), N * 3);
    const std::vector<float> w_quats = download(e.world.quats.data_ptr(), N * 4);
    const std::vector<float> w_scales = download(e.world.scales.data_ptr(), N * 3);
    const std::vector<float> w_dc = download(e.world.features_dc.data_ptr(), N * 3);
    const std::vector<float> visits = download(e.optim.visit_counters.data_ptr(), N);
    const std::vector<float> accum = download(e.optim.accum_buffer.data_ptr(), N * 2);
    r.plain["mean_x"] = column(w_means, 3, 0);
    r.plain["quat_w"] = column(w_quats, 4, 0);
    r.plain["quat_z"] = column(w_quats, 4, 3);
    r.plain["scale_y"] = column(w_scales, 3, 1);
    r.plain["opacity"] = download(e.world.opacities.data_ptr(), N);
    r.plain["dc_g"] = column(w_dc, 3, 1);
    r.plain["radii"] = download(e.optim.radii.data_ptr(), N);
    r.plain["accum_score"] = column(accum, 2, 0);
    r.plain["accum_count"] = column(accum, 2, 1);
    r.plain["visit_renders"] = column(visits, 1, 0, bits_lo16);
    r.plain["visit_idle"] = column(visits, 1, 0, bits_hi16);
    if (run == Run::Fp32) {
        r.plain["sh_norm"] = row_norm(download(e.world.features_sh.data_ptr(), N * NUM_SH * 3),
                                      NUM_SH * 3);
        r.plain["g1_mean_x"] = column(download(e.optim.g1_means.data_ptr(), N * 3), 3, 0);
        r.plain["g2_mean_z"] = column(download(e.optim.g2_means.data_ptr(), N * 3), 3, 2);
        r.plain["v_mean_y"] = column(download(e.grad.means.data_ptr(), N * 3), 3, 1);
        r.plain["g2_sh_norm"] = row_norm(
            download(e.optim.g2_features_sh.data_ptr(), N * NUM_SH * 3), NUM_SH * 3);
    }

    for (const SplatAttribute& a : engine_splat_attributes()) r.attrs[a.code] = a;
    for (const auto& [code, want] : r.plain) {
        (void)want;
        r.values[code] = engine_eval_splat_expr(code);
    }
    for (const char* code : kDecoded)
        if (r.attrs[code].live && !r.values.count(code))
            r.values[code] = engine_eval_splat_expr(code);
    return r;
}

static void expect_exact(const char* run, const Result& r) {
    for (const auto& [code, want] : r.plain) {
        const std::vector<float>& got = r.values.at(code);
        int64_t bad = 0;
        for (int64_t i = 0; i < N; i++) {
            const float a = want[(size_t)i], b = got[(size_t)i];
            // sh_norm's host sum is in double.
            const float tol = code.find("_norm") != std::string::npos ? 1e-6f * std::fabs(a) : 0.0f;
            if (!(a == b || std::fabs(a - b) <= tol || (std::isnan(a) && std::isnan(b)))) bad++;
        }
        check(bad == 0, std::string(run) + " " + code + " reads the engine's own " +
                            r.attrs.at(code).pool + " (" + std::to_string(bad) + " off)");
    }
}

// Within `rtol` of the reference or `atol` of its largest magnitude, splat by
// splat: a codec's error is relative to its block, a misread is not.
static void compare(const char* run, const Result& ref, const Result& got,
                    const std::string& code, double rtol, double atol) {
    auto a = ref.values.find(code), b = got.values.find(code);
    if (a == ref.values.end() || b == got.values.end()) {
        check(false, std::string(run) + " " + code + ": not read in both runs");
        return;
    }
    double peak = 0.0;
    for (float v : a->second) peak = std::max(peak, (double)std::fabs(v));
    int64_t bad = 0;
    double worst = 0.0;
    for (int64_t i = 0; i < N; i++) {
        const double x = a->second[(size_t)i], y = b->second[(size_t)i];
        const double d = std::fabs(x - y);
        worst = std::max(worst, d);
        if (!(d <= rtol * std::fabs(x) + atol * peak)) bad++;
    }
    char line[256];
    std::snprintf(line, sizeof line, "%s %s via %s: %lld of %lld off (max |d| %.3g, peak %.3g)",
                  run, code.c_str(), got.attrs.at(code).pool.c_str(), (long long)bad,
                  (long long)N, worst, peak);
    check(bad == 0 && peak > 0.0, line);
}

static void expect_pool(const char* run, const Result& r, const char* code, const char* pool) {
    auto it = r.attrs.find(code);
    const bool ok = it != r.attrs.end() && it->second.live && it->second.pool == pool;
    check(ok, std::string(run) + " reads " + code + " from " + pool +
                  (it == r.attrs.end() ? "" : " (got " + it->second.pool + ")"));
}

// Device evaluation of `expr` against eval_expr over the device's reads of
// each of its variables.
static void parity(const Result& r, const std::string& expr) {
    spirula::ExprProgram p;
    std::string err;
    const bool ok = spirula::compile_expr(
        expr, [&](const std::string& n) { return r.values.count(n) > 0; }, p, err);
    if (!ok) {
        check(false, expr + ": " + err);
        return;
    }
    const std::vector<float> dev = engine_eval_splat_expr(expr);
    int64_t bad = 0;
    std::vector<float> vars(p.vars.size());
    for (int64_t i = 0; i < N; i++) {
        for (size_t k = 0; k < p.vars.size(); k++)
            vars[k] = r.values.at(p.vars[k])[(size_t)i];
        const float host = spirula::eval_expr(p, vars.data());
        const float d = dev[(size_t)i];
        const bool same = (std::isnan(host) && std::isnan(d)) ||
                          std::fabs(host - d) <= 1e-4f * std::max(1.0f, std::fabs(host));
        if (!same) bad++;
    }
    check(bad == 0 && (int64_t)dev.size() == N,
          "device = host: " + expr + " (" + std::to_string(bad) + " off)");
}

int main() {
    const Result fp32 = run_once(Run::Fp32);
    engine_reset();
    const Result fused = run_once(Run::QuantFused);
    engine_reset();
    const Result quant = run_once(Run::Quant);   // last: parity() reads the engine

    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }

    // The layouts each run is supposed to exercise, or the comparisons below
    // would pass by reading plain floats three times.
    expect_pool("fp32", fp32, "g1_mean_x", "eng.g1_means");
    expect_pool("fp32", fp32, "sh_norm", "world.features_sh");
    expect_pool("quant", quant, "sh_norm", "eng.world.sh_vq16");
    expect_pool("quant", quant, "g1_sh_norm", "eng.sh_quant");
    expect_pool("quant", quant, "v_mean_x", "eng.v_means_q");
    expect_pool("quant", quant, "v_sh_norm", "eng.v_features_sh_q");
    expect_pool("fused", fused, "sh_norm", "eng.world.sh_vq16_fpbo");
    expect_pool("fused", fused, "g1_sh_norm", "eng.sh_quant_fpbo");
    expect_pool("fused", fused, "g1_mean_x", "eng.means_qfpbo");
    check(!fused.attrs.at("v_mean_x").live, "fused keeps no gradient buffer");

    expect_exact("fp32", fp32);
    expect_exact("quant", quant);
    expect_exact("fused", fused);

    for (const char* code : kDecoded) {
        const std::string c = code;
        // 8-bit Adam state: log-domain sqrt(g2) at 1/255 of its block's span.
        const bool coarse = c.find("sh_norm") != std::string::npos;
        const double rtol = coarse ? 0.05 : 0.02, atol = coarse ? 0.02 : 2e-3;
        if (quant.values.count(c)) compare("quant", fp32, quant, c, rtol, atol);
        if (fused.values.count(c)) compare("fused", fp32, fused, c, rtol, atol);
    }

    for (const char* expr : {
             "ln((abs(g1_mean_x)+abs(g1_quat_y)+abs(g1_dc_r))/sqrt(max(g2_mean_z,g2_opacity,1e-18)))",
             "mean_x^2 + (-mean_x)^3 - mean_x**0.5",
             "where(opacity > 0, dc_g, -dc_g) * clamp(quat_z, -0.1, 0.1)",
             "sigmoid(opacity) + tanh(v_opacity * 1e3) + sign(v_dc_b) + floor(visit_renders)",
             "min(g1_opacity, g2_opacity, 0) / max(sh_norm, 1e-6) + log2(radii + 1)",
         })
        parity(quant, expr);

    std::printf("splat_inspect_parity: %s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

#endif
