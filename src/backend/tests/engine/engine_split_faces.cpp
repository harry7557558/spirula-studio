#include <engine/Engine.h>
#include <engine/EngineInternal.h>
#include <engine/EngineState.h>

#include <app/TrainerCore.h>
#include <core/Camera.h>
#include <data/CameraMath.h>
#include <data/DatasetParser.h>
#include <data/ImageProbe.h>
#include <external/stb_image.h>
#include <external/stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
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
    if (a.size() != b.size() || a.empty()) {
        std::printf("  %-12s size mismatch (%zu vs %zu)\n", name, a.size(), b.size());
        return false;
    }
    float worst = 0.0f;
    size_t worst_i = 0, violations = 0;
    double se = 0.0, sr = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!std::isfinite(a[i]) || !std::isfinite(b[i])) {
            std::printf("  %-12s non-finite value at %zu\n", name, i);
            return false;
        }
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

static bool all_finite(const std::vector<float>& v, const char* what) {
    for (size_t i = 0; i < v.size(); ++i)
        if (!std::isfinite(v[i])) {
            std::printf("engine_split_faces: %s[%zu] is not finite\n", what, i);
            return false;
        }
    return true;
}

static bool check_exception_restore(const Inputs& inputs) {
    // Repeated Arc runs varied by 1.35e-6; retain the 1e-6 RMS gate.
    constexpr float splat_atol = 1e-5f;
    bool ok = false;
    try {
        Result recovered = run(inputs, true, 2, true);
        Result fresh = run(inputs, true, 2);
        Result fresh_repeat = run(inputs, true, 2);
        ok = compare("fresh splats", fresh.splats, fresh_repeat.splats,
                     splat_atol, 0.0, 1e-6);
        ok = compare("fresh ppisp", fresh.ppisp, fresh_repeat.ppisp,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("fresh rgb", fresh.rgb_grid, fresh_repeat.rgb_grid,
                     1e-6, 0.0, 1e-6) && ok;
        ok = compare("fresh normal", fresh.normal_grid,
                     fresh_repeat.normal_grid, 1e-6, 0.0, 1e-6) && ok;
        ok = compare("fresh background", fresh.background,
                     fresh_repeat.background, 1e-6, 0.0, 1e-6) && ok;
        ok = compare("recovered splats", recovered.splats, fresh.splats,
                     splat_atol, 0.0, 1e-6) && ok;
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

// ===========================================================================
// On-disk equirectangular control fixture (CPU only)
// ===========================================================================

namespace fs = std::filesystem;

namespace {

constexpr double kFixturePi = 3.14159265358979323846;
constexpr int kFixtureDefaultWidth = 256;
constexpr int kFixtureMaxWidth     = 4096;
constexpr int kFixtureInputs       = 3;    // 1 train + 2 consecutive held-out
constexpr int kFixtureSeedPoints   = 64;
constexpr int kFixtureCapMax       = 512;
constexpr int kFixtureFaces        = 6;    // cube faces of a 2:1 panorama
constexpr int kFixtureEvalInputs   = kFixtureInputs - 1;   // the held-out pair
constexpr int kFixtureNormalDiv    = 4;    // normal PNG = 1/16 the source area
constexpr int kControlSteps        = 6;
constexpr int kControlDensifyStep  = 2;    // refine_start_iter 1, refine_every 2

// Six well-separated face colours. The per-input channel rotation keeps them
// distinct while making consecutive inputs differ on every face.
const uint8_t kFacePalette[6][3] = {
    {240, 40, 40}, {40, 240, 40}, {40, 40, 240},
    {240, 240, 40}, {40, 240, 240}, {240, 40, 240},
};

camhost::Camera fixture_camera(int width, int height) {
    camhost::Camera c;
    c.model = (int)camera_model_from_name("EQUIRECTANGULAR");
    c.tier = (int)CameraDistortionType::None;
    c.width = width;
    c.height = height;
    c.fx = c.fy = (double)width / (2.0 * kFixturePi);
    c.cx = 0.5 * width;
    c.cy = 0.5 * height;
    return c;
}

void face_color(int face, int input, uint8_t out[3]) {
    for (int j = 0; j < 3; ++j) {
        const float v = (float)kFacePalette[face][(j + input) % 3] *
                        (1.0f - 0.12f * (float)input);
        out[j] = (uint8_t)std::lround(std::min(255.0f, std::max(0.0f, v)));
    }
}

// Colour a camera-frame direction from the face cell that holds it; the value
// at a face's centre is its palette entry, so the six faces stay separable.
bool face_pixel_color(const camhost::SplitFace& sf, const double* ax,
                      const double r[3], int input, uint8_t out[3]) {
    const double z = r[0]*ax[6] + r[1]*ax[7] + r[2]*ax[8];
    if (z <= 1e-9) return false;
    const double x = (r[0]*ax[0] + r[1]*ax[1] + r[2]*ax[2]) / z;
    const double y = (r[0]*ax[3] + r[1]*ax[4] + r[2]*ax[5]) / z;
    const double px = sf.fx * x + sf.cx, py = sf.fy * y + sf.cy;
    if (px < 0.0 || px >= sf.width || py < 0.0 || py >= sf.height) return false;
    const double t = 0.75 + 0.25 * std::min(1.0, std::max(std::fabs(x), std::fabs(y)));
    uint8_t base[3];
    face_color(sf.face, input, base);
    for (int j = 0; j < 3; ++j)
        out[j] = (uint8_t)std::lround(std::min(255.0, base[j] * t));
    return true;
}

std::vector<uint8_t> paint_equirect(const camhost::Camera& cam,
                                    const std::vector<camhost::SplitFace>& faces,
                                    int input) {
    const double* table = camhost::equirect_face_axes();
    std::vector<uint8_t> rgb((size_t)cam.width * cam.height * 3, 0);
    for (int y = 0; y < cam.height; ++y)
        for (int x = 0; x < cam.width; ++x) {
            const double u = ((double)x + 0.5 - cam.cx) / cam.fx;
            const double v = ((double)y + 0.5 - cam.cy) / cam.fy;
            double r[3];
            if (!camhost::generate_ray(u, v, cam.model, cam.tier, cam.dist, r))
                continue;
            uint8_t c[3];
            for (const camhost::SplitFace& sf : faces)
                if (face_pixel_color(sf, table + 9 * sf.face, r, input, c)) {
                    uint8_t* p = rgb.data() + ((size_t)y * cam.width + x) * 3;
                    p[0] = c[0]; p[1] = c[1]; p[2] = c[2];
                    break;
                }
        }
    return rgb;
}

std::vector<uint8_t> paint_normals(int w, int h, int input) {
    std::vector<uint8_t> rgb((size_t)w * h * 3, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double n[3] = {(2.0 * (x + 0.5) / w - 1.0) * 0.5,
                           (2.0 * (y + 0.5) / h - 1.0) * 0.5,
                           1.0 + 0.25 * (double)input};
            const double l = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
            uint8_t* p = rgb.data() + ((size_t)y * w + x) * 3;
            for (int j = 0; j < 3; ++j)
                p[j] = (uint8_t)std::lround(127.5 * (n[j] / l + 1.0));
        }
    return rgb;
}

bool write_png(const fs::path& path, int w, int h,
               const std::vector<uint8_t>& rgb) {
    return stbi_write_png(path.string().c_str(), w, h, 3, rgb.data(), w * 3) != 0;
}

void write_seed_ply(const fs::path& path, int count) {
    std::mt19937 rng(20260911u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path.string());
    f << "ply\nformat ascii 1.0\ncomment spirula engine_split_faces control\n"
      << "element vertex " << count << "\n"
      << "property float x\nproperty float y\nproperty float z\n"
      << "property uchar red\nproperty uchar green\nproperty uchar blue\n"
      << "end_header\n";
    for (int i = 0; i < count; ++i)
        f << uf(-0.4f, 0.4f) << ' ' << uf(-0.4f, 0.4f) << ' ' << uf(-0.4f, 0.4f)
          << ' ' << (int)(rng() % 256) << ' ' << (int)(rng() % 256) << ' '
          << (int)(rng() % 256) << '\n';
    if (!f) throw std::runtime_error("cannot write " + path.string());
}

// OpenGL c2w on a unit ring about the origin, so the normalized-frame scale is
// exactly 1 and the up-vector mean stays +Y.
void fixture_c2w(int input, double out[12]) {
    const double az = 2.0 * kFixturePi * (double)input / (double)kFixtureInputs;
    const double px = std::cos(az), pz = std::sin(az);
    const double back[3]  = {px, 0.0, pz};
    const double right[3] = {pz, 0.0, -px};
    for (int r = 0; r < 3; ++r) {
        out[r*4 + 0] = right[r];
        out[r*4 + 1] = r == 1 ? 1.0 : 0.0;
        out[r*4 + 2] = back[r];
        out[r*4 + 3] = r == 0 ? px : (r == 1 ? 0.0 : pz);
    }
}

void fixture_input_name(int input, char* out, size_t n) {
    std::snprintf(out, n, "%03d_%s.png", input, input == 0 ? "train" : "eval");
}

void write_transforms_json(const fs::path& path, int w, int h) {
    const double f = (double)w / (2.0 * kFixturePi);
    std::ofstream o(path, std::ios::binary);
    if (!o) throw std::runtime_error("cannot write " + path.string());
    o << "{\n  \"camera_model\": \"EQUIRECTANGULAR\",\n"
      << "  \"ply_file_path\": \"sparse_pc.ply\",\n  \"frames\": [\n";
    for (int i = 0; i < kFixtureInputs; ++i) {
        char name[32];
        fixture_input_name(i, name, sizeof name);
        double m[12];
        fixture_c2w(i, m);
        o << "    {\"file_path\": \"images/" << name << "\""
          << ", \"fl_x\": " << f << ", \"fl_y\": " << f
          << ", \"cx\": " << (w / 2.0) << ", \"cy\": " << (h / 2.0)
          << ", \"w\": " << w << ", \"h\": " << h
          << ", \"transform_matrix\": [\n";
        for (int r = 0; r < 3; ++r)
            o << "      [" << m[r*4+0] << ", " << m[r*4+1] << ", " << m[r*4+2]
              << ", " << m[r*4+3] << "],\n";
        o << "      [0, 0, 0, 1]]}" << (i + 1 < kFixtureInputs ? "," : "") << "\n";
    }
    o << "  ]\n}\n";
    if (!o) throw std::runtime_error("cannot write " + path.string());
}

bool export_fixture(const fs::path& dir, int width) {
    if (width < 16 || width > kFixtureMaxWidth)
        throw std::runtime_error("export-fixture: width must be 16.." +
                                 std::to_string(kFixtureMaxWidth));
    // 2:1 is what the panorama's canonical intrinsics assume, and an odd width
    // cannot halve into it.
    if (width % 2 != 0)
        throw std::runtime_error("export-fixture: width must be even (2:1 source)");
    if (fs::exists(dir) && !fs::is_directory(dir))
        throw std::runtime_error("export-fixture: " + dir.string() +
                                 " is not a directory");
    if (fs::is_directory(dir) && !fs::is_empty(dir))
        throw std::runtime_error("export-fixture: refusing to overwrite "
                                 "nonempty directory " + dir.string());
    const int height = width / 2;
    const camhost::Camera cam = fixture_camera(width, height);
    const std::vector<camhost::SplitFace> faces =
        camhost::plan_split_faces(cam, camhost::FaceFit::Uniform, true);
    if (faces.size() != 6)
        throw std::runtime_error("export-fixture: expected six faces for a panorama");

    fs::create_directories(dir / "images");
    fs::create_directories(dir / "normals");
    const int nw = std::max(1, width / kFixtureNormalDiv);
    const int nh = std::max(1, height / kFixtureNormalDiv);
    for (int i = 0; i < kFixtureInputs; ++i) {
        char name[32];
        fixture_input_name(i, name, sizeof name);
        if (!write_png(dir / "images" / name, width, height,
                       paint_equirect(cam, faces, i)))
            throw std::runtime_error("export-fixture: cannot write images/" +
                                     std::string(name));
        if (!write_png(dir / "normals" / name, nw, nh, paint_normals(nw, nh, i)))
            throw std::runtime_error("export-fixture: cannot write normals/" +
                                     std::string(name));
    }
    write_seed_ply(dir / "sparse_pc.ply", kFixtureSeedPoints);
    write_transforms_json(dir / "transforms.json", width, height);

    std::printf("engine_split_faces: exported fixture %s\n"
                "  source %dx%d (2:1), %d inputs (1 train, %d held-out), "
                "normal %dx%d, seeds %d, %zu faces of %dx%d\n",
                fs::absolute(dir).string().c_str(), width, height,
                kFixtureInputs, kFixtureInputs - 1, nw, nh, kFixtureSeedPoints,
                faces.size(), faces.front().width, faces.front().height);
    return true;
}

bool sample_face_center(const camhost::Camera& cam, const camhost::SplitFace& sf,
                        const double* ax, const std::vector<uint8_t>& rgb,
                        uint8_t out[3]) {
    const double nx = (sf.width / 2 + 0.5 - sf.cx) / sf.fx;
    const double ny = (sf.height / 2 + 0.5 - sf.cy) / sf.fy;
    const double r[3] = {ax[6] + nx*ax[0] + ny*ax[3],
                         ax[7] + nx*ax[1] + ny*ax[4],
                         ax[8] + nx*ax[2] + ny*ax[5]};
    double uv[2];
    if (!camhost::project_ray(r, cam.model, cam.tier, cam.dist, uv)) return false;
    const int x = (int)std::floor(uv[0] * cam.fx + cam.cx);
    const int y = (int)std::floor(uv[1] * cam.fy + cam.cy);
    if (x < -1 || y < 0 || x > cam.width || y >= cam.height) return false;
    const int cx = std::min(cam.width - 1, std::max(0, x));
    const uint8_t* p = rgb.data() + ((size_t)y * cam.width + cx) * 3;
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
    return true;
}

bool verify_fixture(const fs::path& dir, int width) {
    const int height = width / 2;
    const camhost::Camera cam = fixture_camera(width, height);
    const std::vector<camhost::SplitFace> faces =
        camhost::plan_split_faces(cam, camhost::FaceFit::Uniform, true);
    const double* table = camhost::equirect_face_axes();
    bool ok = faces.size() == 6;
    uint8_t got[kFixtureInputs][6][3] = {};
    for (int i = 0; i < kFixtureInputs && ok; ++i) {
        char name[32];
        fixture_input_name(i, name, sizeof name);
        int w = 0, h = 0, ch = 0;
        stbi_uc* img = stbi_load((dir / "images" / name).string().c_str(),
                                 &w, &h, &ch, 3);
        if (!img || w != width || h != height) {
            if (img) stbi_image_free(img);
            std::printf("engine_split_faces: fixture input %d unreadable or "
                        "wrong size\n", i);
            return false;
        }
        const std::vector<uint8_t> rgb(img, img + (size_t)w * h * 3);
        stbi_image_free(img);
        for (size_t k = 0; k < faces.size() && ok; ++k) {
            if (!sample_face_center(cam, faces[k], table + 9 * faces[k].face,
                                    rgb, got[i][k])) {
                ok = false;
                break;
            }
            uint8_t want[3];
            face_color(faces[k].face, i, want);
            for (int j = 0; j < 3; ++j)
                if (std::abs((int)got[i][k][j] -
                             (int)std::lround(want[j] * 0.75)) > 30)
                    ok = false;
        }
        for (size_t a = 0; a < faces.size() && ok; ++a)
            for (size_t b = a + 1; b < faces.size() && ok; ++b) {
                int worst = 0;
                for (int j = 0; j < 3; ++j)
                    worst = std::max(worst, std::abs((int)got[i][a][j] -
                                                     (int)got[i][b][j]));
                if (worst < 40) ok = false;
            }
        for (size_t k = 0; k < faces.size() && ok && i > 0; ++k) {
            int worst = 0;
            for (int j = 0; j < 3; ++j)
                worst = std::max(worst, std::abs((int)got[i][k][j] -
                                                 (int)got[0][k][j]));
            if (worst < 24) ok = false;
        }
    }
    std::printf("engine_split_faces: fixture face signal %s (%d inputs x %zu "
                "faces distinct)\n", ok ? "ok" : "FAILED", kFixtureInputs,
                faces.size());
    if (!ok) return false;
    // Low-resolution normal PNGs, at the size the parser and DataManager probe.
    const int nw = std::max(1, width / kFixtureNormalDiv);
    const int nh = std::max(1, height / kFixtureNormalDiv);
    for (int i = 0; i < kFixtureInputs; ++i) {
        char name[32];
        fixture_input_name(i, name, sizeof name);
        int w = 0, h = 0, ch = 0;
        if (!stbi_info((dir / "normals" / name).string().c_str(), &w, &h, &ch) ||
            w != nw || h != nh) {
            std::printf("engine_split_faces: fixture normal %s is not %dx%d\n",
                        name, nw, nh);
            return false;
        }
    }
    return true;
}

// ===========================================================================
// Managed TrainerSession control (real scheduler, <= 1 GiB app limit)
// ===========================================================================

double since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
}

// Retained pool capacities are a high-water mark; the driver figures beside
// them are an instantaneous snapshot. Never read the two as one number.
bool stage_report(const char* stage, const spirula::TrainerSession& s,
                  double seconds) {
    const backend::BudgetSnapshot b = backend::budget_snapshot();
    const backend::MemoryUsage m = backend::memory_usage();
    std::map<std::string, std::pair<size_t, size_t>> cats;   // used, cap
    for (const auto& r : engine_get_pool_breakdown_categorized()) {
        auto& c = cats[std::get<1>(r)];
        c.first  += std::get<2>(r);
        c.second += std::get<3>(r);
    }
    const double mib = 1.0 / (1024.0 * 1024.0);
    std::printf("engine_split_faces: [%s] wall %.3fs src %dx%d face %dx%d x%lld "
                "splats live %lld cap %lld\n",
                stage, seconds,
                (int)(s.ds.widths.empty() ? 0 : s.ds.widths[0]),
                (int)(s.ds.heights.empty() ? 0 : s.ds.heights[0]),
                (int)(s.post.post_widths.empty() ? 0 : s.post.post_widths[0]),
                (int)(s.post.post_heights.empty() ? 0 : s.post.post_heights[0]),
                (long long)s.post.n_post,
                (long long)engine_get_cur_num_splats(),
                (long long)engine_get_max_num_splats());
    std::printf("  budget: app_limit %.1f MiB reserve %.1f MiB process %.1f MiB "
                "reserved %.1f MiB available %.1f MiB\n",
                b.app_limit_bytes * mib, b.reserve_bytes * mib,
                b.process_bytes * mib, b.reserved_bytes * mib,
                b.available_bytes * mib);
    std::printf("  pool retained capacity (high-water, not a driver peak):");
    for (const auto& [name, v] : cats)
        std::printf(" %s %.2f/%.2f MiB", name.c_str(), v.first * mib,
                    v.second * mib);
    std::printf("; scratch %.2f MiB\n", engine_get_scratch_bytes() * mib);
    std::printf("  driver snapshot (instantaneous): process %.1f MiB, in use "
                "%.1f / %.1f MiB\n", m.process_bytes * mib, m.used_bytes * mib,
                m.total_bytes * mib);
    if (b.app_limit_bytes > 0 && b.process_bytes > b.app_limit_bytes) {
        std::printf("  FAIL: process bytes exceed the application limit\n");
        return false;
    }
    return true;
}

int metrics_view_count(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return -1;
    const std::string s((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    const std::string key = "\"num_eval_images\":";
    const size_t at = s.find(key);
    return at == std::string::npos ? -1 : std::atoi(s.c_str() + at + key.size());
}

void configure_control_session(spirula::TrainerSession& session,
                               const fs::path& fixture, const fs::path& out_dir) {
    session.cfg.data = fixture.string();
    session.cfg.eval_mode = "filename";       // 1 train + 2 consecutive held-out
    session.cfg.memory_limit_gib = 1.0f;      // <= 1 GiB application limit
    session.cfg.cap_max = kFixtureCapMax;
    session.cfg.preallocate_splat_tensors = true;
    session.cfg.num_iterations = kControlSteps;
    session.cfg.steps_per_save = 0;
    session.cfg.save_eval_images = true;
    session.cfg.refine_start_iter = 1;
    session.cfg.refine_every = 2;
    session.cfg.growth_factor = 1.25f;
    session.out_dir_override = out_dir.string();
}

bool run_managed_control(const fs::path& fixture, const fs::path& out_dir) {
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    spirula::TrainerSession session;
    configure_control_session(session, fixture, out_dir);

    auto t0 = std::chrono::steady_clock::now();
    session.check_config();
    session.load_dataset();
    if (session.ds.num_cameras != 1 || session.ds.points.num() != kFixtureSeedPoints)
        throw std::runtime_error("control: expected one training input and " +
                                 std::to_string(kFixtureSeedPoints) + " seed points");
    if (session.post.n_post != 6 || session.post.K_per_camera[0] != 6)
        throw std::runtime_error("control: expected six faces for the training input");
    std::printf("engine_split_faces: control dataset %dx%d, %lld train inputs, "
                "%lld seed points\n",
                (int)session.ds.widths[0], (int)session.ds.heights[0],
                (long long)session.ds.train_indices.size(),
                (long long)session.ds.points.num());

    session.setup_engine();
    bool ok = stage_report("setup", session, since(t0));

    int64_t first = -1, warmed = -1, densified = -1;
    spirula::TrainerCallbacks cb;
    cb.on_step = [&](const spirula::TrainerProgress& p) {
        if (p.step == 0) {
            first = p.num_splats;
            ok = stage_report("first step", session, p.step_latency) && ok;
        } else if (p.step == 1) {
            warmed = p.num_splats;
            ok = stage_report("warmed step", session, p.step_latency) && ok;
        } else if (p.step == kControlDensifyStep) {
            densified = p.num_splats;
            ok = stage_report("densification step", session, p.step_latency) && ok;
        } else if (p.step == kControlSteps - 1) {
            ok = stage_report("last step", session, p.step_latency) && ok;
        }
    };
    t0 = std::chrono::steady_clock::now();
    session.train(cb);
    ok = stage_report("training complete", session, since(t0)) && ok;

    t0 = std::chrono::steady_clock::now();
    session.eval();
    ok = stage_report("held-out eval", session, since(t0)) && ok;
    session.reset_engine();

    // The real densification scheduler must move the count by exactly the
    // growth it was given, not merely produce something nonempty.
    const int64_t expected_dense =
        (int64_t)std::floor(1.25 * (double)kFixtureSeedPoints);
    if (first != kFixtureSeedPoints || warmed != kFixtureSeedPoints) {
        std::printf("engine_split_faces: control count before densification "
                    "%lld/%lld, expected %d\n", (long long)first,
                    (long long)warmed, kFixtureSeedPoints);
        ok = false;
    }
    if (densified != expected_dense) {
        std::printf("engine_split_faces: control densification moved %lld -> "
                    "%lld, expected %lld\n", (long long)warmed,
                    (long long)densified, (long long)expected_dense);
        ok = false;
    }
    const int views = metrics_view_count(out_dir / "metrics.json");
    if (views != 2 * 6) {
        std::printf("engine_split_faces: control held-out eval produced %d views, "
                    "expected 12 (2 inputs x 6 faces)\n", views);
        ok = false;
    }
    std::printf("engine_split_faces: control %s (control leg complete)\n",
                ok ? "ok" : "FAILED");
    std::fflush(stdout);
    return ok;
}

// Removes the internally created temp trees on every exit path; an explicit
// export-fixture directory is the caller's to keep.
struct TempCleanup {
    std::vector<fs::path> paths;
    ~TempCleanup() {
        std::error_code ec;
        for (const fs::path& p : paths) fs::remove_all(p, ec);
    }
};

// ===========================================================================
// Bounded all-faces vs pass-wise evaluation reference (real engine + session)
// ===========================================================================

struct EvalCall {
    int64_t px = 0;                  // floats per view
    int64_t H = 0, W = 0;
    std::vector<float> gt, render;   // [want_views * px]
};

// Whole-batch readback ignores the supplied shape, so validate before copying.
bool read_eval_call(EvalCall& out, int64_t want_views, const char* what) {
    auto rshape = engine_get_render_rgb_shape();
    auto gshape = engine_get_gt_rgb_shape();
    const int64_t rb = std::get<0>(rshape), rh = std::get<1>(rshape);
    const int64_t rw = std::get<2>(rshape), rc = std::get<3>(rshape);
    const int64_t gb = std::get<0>(gshape), gh = std::get<1>(gshape);
    const int64_t gw = std::get<2>(gshape), gc = std::get<3>(gshape);
    if (rb != want_views || gb != want_views || rc != 3 || gc != 3 ||
        rh != gh || rw != gw || rh <= 0 || rw <= 0) {
        std::printf("engine_split_faces: %s: render [%lld,%lld,%lld,%lld] / GT "
                    "[%lld,%lld,%lld,%lld], expected %lld views\n",
                    what, (long long)rb, (long long)rh, (long long)rw,
                    (long long)rc, (long long)gb, (long long)gh, (long long)gw,
                    (long long)gc, (long long)want_views);
        return false;
    }
    out.px = rh * rw * rc;
    out.H = rh;
    out.W = rw;
    out.gt.resize((size_t)(rb * out.px));
    out.render.resize((size_t)(rb * out.px));
    engine_copy_gt_rgb_to_host(tv(out.gt.data(), 4, {rb, rh, rw, rc}));
    engine_copy_render_to_host(tv(out.render.data(), 4, {rb, rh, rw, rc}),
                               null_tv(), null_tv(), null_tv(), null_tv());
    return all_finite(out.gt, "eval GT") &&
           all_finite(out.render, "eval render");
}

struct FaceSet {
    int64_t faces = 0;
    int64_t px    = 0;
    int64_t H = 0, W = 0;
    std::vector<std::vector<float>> gt, render;   // [face][px]
};

double max_abs_diff(const std::vector<float>& a, const std::vector<float>& b) {
    double worst = 0.0;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
        worst = std::max(worst, (double)std::fabs(a[i] - b[i]));
    return worst;
}

bool collect_face_sets(const fs::path& fixture, const TrainConfig& cfg,
                       int max_faces_per_pass,
                       const std::vector<int32_t>& indices,
                       int64_t faces_per_input, const char* what,
                       std::vector<FaceSet>& out) {
    const int64_t inputs = (int64_t)indices.size();
    DatasetParserConfig pcfg;
    pcfg.recon_dir                = cfg.colmap_recon_dir;
    pcfg.image_dir                = cfg.image_dir;
    pcfg.mask_dir                 = cfg.mask_dir;
    pcfg.depth_dir                = cfg.depth_dir;
    pcfg.normal_dir               = cfg.normal_dir;
    pcfg.validation_fraction      = 0.0f;   // no early-stop holdout inside eval
    pcfg.eval_mode                = cfg.eval_mode;
    pcfg.eval_interval            = cfg.eval_interval;
    pcfg.train_split_fraction     = cfg.train_split_fraction;
    pcfg.outlier_threshold        = cfg.outlier_threshold;
    pcfg.center_mode              = cfg.scene_center;
    pcfg.probe_image_size         = probe_image_size;
    pcfg.train_resolution_divisor = cfg.train_resolution_divisor;
    pcfg.downscale_rounding_mode  = cfg.downscale_rounding_mode;
    pcfg.split                    = "eval";

    ParsedDataset eds = parse_dataset(fixture.string(), pcfg, cfg.data_format);
    if (eds.num_cameras != kFixtureEvalInputs) {
        std::printf("engine_split_faces: %s: eval split holds %lld inputs, "
                    "expected %lld\n", what, (long long)eds.num_cameras,
                    (long long)kFixtureEvalInputs);
        return false;
    }
    PostSplitCameras epost = bake_post_split(
        eds, cfg.warp_to_pinhole, cfg.warp_spherical_to_pinhole,
        WarpFaceFit::Uniform, cfg.warp_back_face);
    bool uniform = epost.any_warp && !epost.post_offsets.empty();
    for (int32_t i : indices)
        if ((int64_t)epost.K_per_camera[(size_t)i] != faces_per_input)
            uniform = false;
    if (!uniform) {
        std::printf("engine_split_faces: %s: expected %lld faces per input, got "
                    "%lld post cameras across %lld inputs\n", what,
                    (long long)faces_per_input, (long long)epost.n_post,
                    (long long)eds.num_cameras);
        return false;
    }

    const bool eval_masks = !eds.mask_filenames.empty() && cfg.load_masks;
    DataManagerConfig dm;
    dm.cache_mode   = CacheMode::CPU;   // the cursor is what this leg exercises
    dm.load_masks   = eval_masks || epost.any_fov_mask;
    dm.load_depths  = false;
    dm.load_normals = false;
    dm.train_batch_size = 1;            // one sub-batch per eval step
    dm.val_batch_size   = 1;
    dm.max_faces_per_pass = max_faces_per_pass;
    dm.flip_mask = cfg.flip_mask;
    dm.mask_boundary_offset = cfg.mask_boundary_offset;

    engine_setup_data_manager(
        dm, eds.camera_models, eds.camera_distortions, eds.image_filenames,
        eval_masks ? eds.mask_filenames : std::vector<std::string>{}, {}, {},
        eds.widths, eds.heights,
        epost.K_per_camera, epost.post_offsets,
        epost.viewmats, epost.intrins, epost.dist_coeffs,
        epost.post_widths, epost.post_heights, epost.face_axes,
        epost.input_intrins, epost.input_dist_coeffs,
        epost.redistort_models, epost.redistort_params, indices, {});

    const int64_t per_call = max_faces_per_pass > 0 ? max_faces_per_pass
                                                    : faces_per_input;
    const int64_t calls_per_input = faces_per_input / per_call;
    const int64_t total = inputs * calls_per_input;
    out.assign((size_t)inputs, {});
    for (int64_t c = 0; c < total; ++c) {
        const int64_t input = c / calls_per_input;
        const int64_t k0    = (c % calls_per_input) * per_call;
        const int n = engine_eval_forward(cfg.primitive, cfg.sh_degree,
                                          cfg.packed);
        if (n != per_call) {
            std::printf("engine_split_faces: %s: call %lld returned %d views, "
                        "expected %lld\n", what, (long long)c, n,
                        (long long)per_call);
            return false;
        }
        EvalCall call;
        if (!read_eval_call(call, per_call, what)) return false;
        FaceSet& set = out[(size_t)input];
        if (set.faces == 0) {
            set.faces = faces_per_input;
            set.px    = call.px;
            set.H = call.H;
            set.W = call.W;
            set.gt.assign((size_t)faces_per_input, {});
            set.render.assign((size_t)faces_per_input, {});
        } else if (set.H != call.H || set.W != call.W) {
            std::printf("engine_split_faces: %s: input %lld changed face shape "
                        "between calls\n", what, (long long)input);
            return false;
        }
        for (int64_t v = 0; v < per_call; ++v) {
            const size_t off = (size_t)(v * call.px);
            set.gt[(size_t)(k0 + v)].assign(call.gt.begin() + off,
                                            call.gt.begin() + off + call.px);
            set.render[(size_t)(k0 + v)].assign(
                call.render.begin() + off, call.render.begin() + off + call.px);
        }
    }
    // One exhausted call would not catch a cursor that restarts afterwards.
    for (int extra = 0; extra < 3; ++extra) {
        const int n = engine_eval_forward(cfg.primitive, cfg.sh_degree,
                                          cfg.packed);
        if (n != 0) {
            std::printf("engine_split_faces: %s: call %d past exhaustion "
                        "returned %d views, expected 0\n", what, extra, n);
            return false;
        }
    }
    return true;
}

// Distinct GT catches an ordering bug shared by both traversal modes.
bool faces_are_distinct(const char* what, const std::vector<FaceSet>& sets) {
    const double distinct = 0.01;   // one 8-bit step is ~0.004
    bool ok = true;
    for (size_t i = 0; i < sets.size(); ++i)
        for (int64_t a = 0; a < sets[i].faces && ok; ++a)
            for (int64_t b = a + 1; b < sets[i].faces && ok; ++b)
                if (max_abs_diff(sets[i].gt[(size_t)a],
                                 sets[i].gt[(size_t)b]) < distinct) {
                    std::printf("engine_split_faces: %s: input %zu faces %lld "
                                "and %lld carry the same GT\n", what, i,
                                (long long)a, (long long)b);
                    ok = false;
                }
    for (size_t i = 0; i < sets.size() && ok; ++i)
        for (size_t j = i + 1; j < sets.size() && ok; ++j)
            for (int64_t f = 0; f < sets[i].faces && ok; ++f)
                if (max_abs_diff(sets[i].gt[(size_t)f],
                                 sets[j].gt[(size_t)f]) < distinct) {
                    std::printf("engine_split_faces: %s: inputs %zu and %zu "
                                "face %lld carry the same GT\n", what, i, j,
                                (long long)f);
                    ok = false;
                }
    return ok;
}

// Render accumulation can reorder; the GT warp has no such tolerance.
bool compare_face_sets(const char* what, const std::vector<FaceSet>& a,
                       const std::vector<FaceSet>& b) {
    if (a.size() != b.size()) {
        std::printf("engine_split_faces: %s: %zu inputs vs %zu\n", what, a.size(),
                    b.size());
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].faces != b[i].faces) {
            std::printf("engine_split_faces: %s: input %zu has %lld faces vs "
                        "%lld\n", what, i, (long long)a[i].faces,
                        (long long)b[i].faces);
            return false;
        }
        if (a[i].H != b[i].H || a[i].W != b[i].W) {
            std::printf("engine_split_faces: %s: input %zu face dimensions differ\n", what, i);
            return false;
        }
        for (int64_t f = 0; f < a[i].faces; ++f) {
            char name[64];
            std::snprintf(name, sizeof name, "in%zu.f%lld gt", i, (long long)f);
            ok = compare(name, a[i].gt[(size_t)f],
                                   b[i].gt[(size_t)f], 1e-5f, 0.0, 1e-6) && ok;
            std::snprintf(name, sizeof name, "in%zu.f%lld rgb", i, (long long)f);
            ok = compare(name, a[i].render[(size_t)f],
                                   b[i].render[(size_t)f], 1e-4f, 5e-3, 1e-4) && ok;
        }
    }
    return ok;
}

// A failed attempt's files must not satisfy the recovery check.
void clear_eval_outputs(const fs::path& dir) {
    for (const auto& entry : fs::directory_iterator(dir)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("eval-", 0) == 0 || name == "metrics.json")
            fs::remove_all(entry.path());
    }
}

int count_files_with_prefix(const fs::path& dir, const std::string& prefix) {
    std::error_code ec;
    int n = 0;
    for (fs::directory_iterator it(dir, ec), end; it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (it->is_regular_file() && name.rfind(prefix, 0) == 0) ++n;
    }
    return n;
}

bool check_eval_slots(const fs::path& out_dir, int expected, const char* what) {
    const int views = metrics_view_count(out_dir / "metrics.json");
    const int gt = count_files_with_prefix(out_dir, "eval-gt-");
    const int render = count_files_with_prefix(out_dir, "eval-render-");
    if (views != expected || gt != expected || render != expected) {
        std::printf("engine_split_faces: %s: %d metric slots, %d GT PNGs, %d "
                    "render PNGs; expected %d of each\n", what, views, gt, render,
                    expected);
        return false;
    }
    const auto [B, H, W, C] = engine_get_render_rgb_shape();
    for (int i = 0; i < expected; ++i) {
        for (const char* kind : {"gt", "render"}) {
            char name[64];
            std::snprintf(name, sizeof name, "eval-%s-%05d.png", kind, i);
            int width = 0, height = 0;
            const std::string path = (out_dir / name).string();
            if (!probe_image_size(path.c_str(), &width, &height) || width != W || height != H) {
                std::printf("engine_split_faces: %s: missing or wrong-sized %s\n", what, name);
                return false;
            }
        }
    }
    return true;
}

bool run_all_faces_reference(const fs::path& fixture, const fs::path& out_dir) {
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    std::printf("engine_split_faces: all-faces reference (2 inputs x %d faces)\n",
                kFixtureFaces);
    spirula::TrainerSession session;
    configure_control_session(session, fixture, out_dir);
    auto t0 = std::chrono::steady_clock::now();
    session.check_config();
    session.load_dataset();
    session.setup_engine();
    session.train({});
    bool ok = stage_report("all-faces trained model", session, since(t0));

    std::vector<int32_t> order((size_t)kFixtureEvalInputs);
    std::iota(order.begin(), order.end(), 0);

    std::vector<FaceSet> whole, per_face;
    ok = collect_face_sets(fixture, session.cfg, 0, order, kFixtureFaces,
                           "cap0", whole) && ok;
    ok = collect_face_sets(fixture, session.cfg, 1, order, kFixtureFaces,
                           "cap1", per_face) && ok;
    if (ok) {
        ok = faces_are_distinct("all-faces reference", whole) && ok;
        ok = compare_face_sets("cap0 vs cap1", whole, per_face) && ok;
    }

    session.reset_engine();
    std::printf("engine_split_faces: all-faces reference %s\n",
                ok ? "ok" : "FAILED");
    std::fflush(stdout);
    return ok;
}

// A producer exception must join scoring workers instead of terminating.
bool run_eval_refusal_recovery(const fs::path& fixture, const fs::path& out_dir,
                               const fs::path& collision_dir) {
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    std::printf("engine_split_faces: eval refusal and recovery\n");
    spirula::TrainerSession session;
    configure_control_session(session, fixture, out_dir);
    session.check_config();
    session.load_dataset();
    session.setup_engine();

    const backend::BudgetSnapshot live = backend::budget_snapshot();
    if (live.status != backend::BudgetStatus::Available) {
        std::printf("engine_split_faces: eval refusal needs memory telemetry, "
                    "got status %d\n", (int)live.status);
        session.reset_engine();
        return false;
    }
    backend::training_budget_begin(live.reserve_bytes, live.process_bytes);
    bool refused = false;
    try {
        session.eval();
    } catch (const backend::BudgetError& e) {
        refused = e.failure.kind == backend::BudgetFailureKind::ApplicationLimit &&
                  e.failure.requested_bytes > 0;
        std::printf("engine_split_faces: eval refusal kind %d, requested %.1f "
                    "MiB\n", (int)e.failure.kind,
                    (double)e.failure.requested_bytes / (1024.0 * 1024.0));
    } catch (const std::exception& e) {
        std::printf("engine_split_faces: eval failed with a non-budget error "
                    "instead of refusing: %s\n", e.what());
    }
    bool ok = refused;
    if (!refused)
        std::printf("engine_split_faces: eval with no application headroom was "
                    "not refused\n");

    session.reset_engine();
    session.setup_engine();
    clear_eval_outputs(session.out_dir);
    auto t0 = std::chrono::steady_clock::now();
    session.eval();
    ok = stage_report("eval after refusal", session, since(t0)) && ok;
    ok = check_eval_slots(session.out_dir, kFixtureEvalInputs * kFixtureFaces,
                          "eval after refusal") && ok;
    session.reset_engine();

    // A directory at the PNG path forces a real scoring-worker write failure.
    fs::remove_all(collision_dir, ec);
    spirula::TrainerSession colliding;
    configure_control_session(colliding, fixture, collision_dir);
    colliding.check_config();
    colliding.load_dataset();
    colliding.setup_engine();
    fs::create_directories(colliding.out_dir / "eval-gt-00000.png");
    std::printf("engine_split_faces: eval worker-failure leg\n");
    std::fflush(stdout);
    bool threw = false;
    try {
        colliding.eval();
    } catch (const std::exception& e) {
        threw = true;
        std::printf("engine_split_faces: eval worker failure raised: %s\n",
                    e.what());
    }
    colliding.reset_engine();
    fs::remove_all(colliding.out_dir / "eval-gt-00000.png", ec);
    clear_eval_outputs(colliding.out_dir);
    bool recovered = false;
    try {
        colliding.setup_engine();
        colliding.eval();
        recovered = check_eval_slots(colliding.out_dir,
                                     kFixtureEvalInputs * kFixtureFaces,
                                     "eval after worker failure");
    } catch (const std::exception& e) {
        std::printf("engine_split_faces: eval after worker failure raised: %s\n",
                    e.what());
    }
    colliding.reset_engine();
    if (!threw)
        std::printf("engine_split_faces: eval worker failure did not raise\n");
    ok = threw && recovered && ok;

    std::printf("engine_split_faces: eval refusal/recovery %s\n",
                ok ? "ok" : "FAILED");
    std::fflush(stdout);
    return ok;
}

bool run_export_fixture(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: engine_split_faces export-fixture <directory> [width]\n"
                    "  width default %d, max %d; the directory must be empty\n",
                    kFixtureDefaultWidth, kFixtureMaxWidth);
        return false;
    }
    int width = kFixtureDefaultWidth;
    if (argc >= 4) {
        try {
            width = std::stoi(argv[3]);
        } catch (...) {
            std::printf("engine_split_faces: bad width '%s'\n", argv[3]);
            return false;
        }
    }
    try {
        const fs::path dir = argv[2];
        return export_fixture(dir, width) && verify_fixture(dir, width);
    } catch (const std::exception& e) {
        std::printf("engine_split_faces: export-fixture FAILED: %s\n", e.what());
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "export-fixture")
        return run_export_fixture(argc, argv) ? 0 : 1;

    // CPU fixture first: no device work has happened yet, and the control
    // session below consumes the files this writes.
    const fs::path fixture_dir =
        fs::temp_directory_path() / "spirula_engine_split_faces_fixture";
    const fs::path control_out =
        fs::temp_directory_path() / "spirula_engine_split_faces_control";
    const fs::path faces_out =
        fs::temp_directory_path() / "spirula_engine_split_faces_allfaces";
    const fs::path refusal_out =
        fs::temp_directory_path() / "spirula_engine_split_faces_refusal";
    const fs::path collision_out =
        fs::temp_directory_path() / "spirula_engine_split_faces_collision";
    TempCleanup cleanup{{fixture_dir, control_out, faces_out, refusal_out,
                         collision_out}};

    bool ok = false;
    bool fixture_ok = false;
    try {
        std::error_code ec;
        fs::remove_all(fixture_dir, ec);
        fixture_ok = export_fixture(fixture_dir, kFixtureDefaultWidth) &&
                     verify_fixture(fixture_dir, kFixtureDefaultWidth);
    } catch (const std::exception& e) {
        std::printf("engine_split_faces: fixture export FAILED: %s\n", e.what());
    }

    Inputs inputs = make_inputs();
    ok = check_exception_restore(inputs);
    ok = check_steps(inputs, 1) && ok;
    ok = check_steps(inputs, 2) && ok;
    engine_reset();

    bool control_ok = false;
    if (fixture_ok) {
        try {
            control_ok = run_managed_control(fixture_dir, control_out);
        } catch (const std::exception& e) {
            std::printf("engine_split_faces: managed control FAILED: %s\n", e.what());
        }
    } else {
        std::printf("engine_split_faces: managed control skipped (no fixture)\n");
    }

    // After the control leg: these replace the engine's DataManager and move
    // the pool's retained capacities, which the control's stage reports read.
    bool faces_ok = false;
    if (fixture_ok) {
        try {
            faces_ok = run_all_faces_reference(fixture_dir, faces_out);
        } catch (const std::exception& e) {
            std::printf("engine_split_faces: all-faces reference FAILED: %s\n",
                        e.what());
        }
        try {
            faces_ok = run_eval_refusal_recovery(fixture_dir, refusal_out,
                                                 collision_out) && faces_ok;
        } catch (const std::exception& e) {
            std::printf("engine_split_faces: eval refusal/recovery FAILED: %s\n",
                        e.what());
        }
    } else {
        std::printf("engine_split_faces: all-faces reference skipped (no fixture)\n");
    }
    engine_reset();

    ok = fixture_ok && control_ok && faces_ok && ok;
    std::printf(ok ? "engine_split_faces: PASSED\n" : "engine_split_faces: FAILED\n");
    return ok ? 0 : 1;
}
