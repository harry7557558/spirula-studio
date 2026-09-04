// Engine background blending (none / random noise / SH skybox).
//
// Forward runs inside forward_3dgs (so viewer renders blend too), out of place
// on fwd.renders.rgb, keeping the pre-blend buffer for the backward.
// Backward order is PPISP -> bilagrid -> background -> raster; the hook here
// rewrites v_render_rgb (post-blend -> pre-blend), ADDS into v_render_Ts, and
// folds in overexposure_reg. SH mode also accumulates per-camera gradient into
// the SH coefficient table; the rotation gradient is dropped (cameras are
// fixed here).

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "kernels/background/BackgroundSphericalHarmonics.cuh"
#include "core/ColorSpace.h"
#include "kernels/pixelwise/PixelWise.cuh"
#include "kernels/optim/Optimizer.cuh"

#include <stdexcept>


// ============================================================================
// Init / setters
// ============================================================================

void engine_init_background_noise(int splat_transfer, bool splat_is_linear) {
    auto& bg = engine().background;
    bg.mode    = EngineBackground::Mode::Noise;
    bg.enabled = true;
    bg.splat_transfer = splat_transfer;
    bg.splat_is_linear = splat_is_linear;
}

void engine_init_background_pseudorandom(int splat_transfer, bool splat_is_linear) {
    auto& bg = engine().background;
    bg.mode    = EngineBackground::Mode::Pseudorandom;
    bg.enabled = true;
    bg.splat_transfer  = splat_transfer;
    bg.splat_is_linear = splat_is_linear;
}

// Allocates the SH parameter table; slot 0 is the DC colour.
void engine_init_background_sh(int sh_degree, int splat_transfer,
                               bool splat_is_linear) {
    if (sh_degree < 0 || sh_degree > 4)
        throw std::runtime_error("engine_init_background_sh: sh_degree must be in [0, 4]");
    auto& bg = engine().background;
    bg.mode           = EngineBackground::Mode::Sh;
    bg.enabled        = true;
    bg.sh_degree      = sh_degree;
    bg.splat_transfer = splat_transfer;
    bg.splat_is_linear = splat_is_linear;

    // Layout: slot 0 = DC color, slots 1..(sh_degree+1)^2-1 = higher SH bands.
    // The kernel reads `sh_coeffs[0]` as DC and `sh_coeffs + 1` as the L1+
    // table, so we must allocate the full (sh_degree+1)^2 elements (NOT
    // sh_degree*(sh_degree+2), which is one short and produces an OOB read
    // on the last band).
    int64_t n = (int64_t)(sh_degree + 1) * (sh_degree + 1);
    bg.sh_coeffs.resize(PoolSlot::EngBgSkyShCoeffs, n);
    bg.sh_coeffs.zero();
    bg.sh_optim_initialized = false;
}


// ============================================================================
// Helpers
// ============================================================================

static void _ensure_bg_sh_optim_state() {
    auto& bg = engine().background;
    if (bg.sh_optim_initialized) return;
    if (bg.mode != EngineBackground::Mode::Sh) return;
    int64_t n = bg.sh_coeffs.size();
    bg.sh_g1.resize(PoolSlot::EngBgSkyG1, n); bg.sh_g1.zero();
    bg.sh_g2.resize(PoolSlot::EngBgSkyG2, n); bg.sh_g2.zero();
    bg.sh_optim_initialized = true;
}

static inline DeviceTensorFloatND _sh_slice_fnd(float3* base, int64_t n) {
    // TorchTensorView convention: trailing dim is the per-cell element shape.
    // T=float so we need s.back()*element_size == sizeof(float) -> trailing 1.
    TorchTensorView tv((uint64_t)base, 4, {n, 3LL, 1LL});
    return DeviceTensorFloatND(tv);
}

// Build the (viewmats, intrins, dist_coeffs, sh_coeffs) TorchTensorView pack
// used by render_background_sh_{forward,backward}. The engine's camera state
// already holds per-batch data (set_camera_params copies in one row per batch
// image), so we pass it as a flat [B, ...] tensor with no extra gather — the
// SH kernel does NOT use cam_indices, because that's a global-camera-id
// lookup that would index past the [B] buffer.
struct BgShViews {
    TorchTensorView viewmats, intrins, dist_coeffs, sh_coeffs;
};
static BgShViews _engine_bg_sh_views(int C_batch) {
    auto& bg = engine().background;
    BgShViews v;
    v.viewmats = TorchTensorView(
        (uint64_t)engine().camera.viewmats.data_ptr(), 4,
        {(int64_t)C_batch, 4LL, 4LL});
    v.intrins = TorchTensorView(
        (uint64_t)engine().camera.intrins.data_ptr(), 4,
        {(int64_t)C_batch, 4LL});
    if (engine().camera.dist_coeffs.data_ptr() != nullptr) {
        v.dist_coeffs = TorchTensorView(
            (uint64_t)engine().camera.dist_coeffs.data_ptr(), 4,
            {(int64_t)C_batch, (int64_t)kCameraDistortionParams});
    } else {
        v.dist_coeffs = TorchTensorView(0, 4, {0LL, (int64_t)kCameraDistortionParams});
    }
    v.sh_coeffs = _dv_tv(bg.sh_coeffs);
    return v;
}


// ============================================================================
// Setter: stash per-iter (seed, randomize_weight) so the next forward_3dgs
// blend + later backward + optim see the same values. Training calls this each
// step; the viewer/eval path can ignore it (defaults of 0 give a uniform-gray
// noise blend, which avoids per-frame flicker in noise-mode viewer renders).
// ============================================================================
void engine_set_background_step_params(uint32_t seed, float randomize_weight) {
    auto& bg = engine().background;
    bg.cur_seed             = seed;
    bg.cur_randomize_weight = randomize_weight;
}


// ============================================================================
// Forward: blend background into engine().fwd.renders.rgb.
// Called from forward_3dgs so both training and viewer renders see the blend.
// Reads (seed, randomize_weight) from state.
// ============================================================================
void _engine_background_forward() {
    auto& bg = engine().background;
    if (!bg.enabled || bg.mode == EngineBackground::Mode::None) return;

    int C_batch = (int)engine().camera.num;
    int H       = engine().camera.height;
    int W       = engine().camera.width;
    if (C_batch <= 0) return;

    auto& fwd_rgb_tensor = std::get<0>(engine().fwd.renders);
    auto& fwd_Ts_tensor  = engine().fwd.render_Ts;
    if (fwd_rgb_tensor.data_ptr() == nullptr || fwd_Ts_tensor.data_ptr() == nullptr)
        throw std::runtime_error("engine_background_forward: forward_3dgs must run first");

    DeviceTensor3D<float>  Ts_in(fwd_Ts_tensor);            // [C, H, W]

    // Both modes blend out-of-place: pre-blend rgb is a pointer alias of the
    // current renders.rgb (no copy), post goes to a fresh buffer.
    bg.fwd_pre_blend_rgb = fwd_rgb_tensor;
    DeviceTensor3D<float3> post_rgb;
    post_rgb.resize(PoolSlot::EngBgSkyRgbPost, C_batch, H, W);

    if (bg.mode == EngineBackground::Mode::Noise ||
        bg.mode == EngineBackground::Mode::Pseudorandom) {
        blend_background_noise_forward(
            bg.splat_transfer, bg.splat_is_linear,
            bg.mode == EngineBackground::Mode::Pseudorandom,
            bg.fwd_pre_blend_rgb, Ts_in,
            bg.cur_randomize_weight, bg.cur_seed,
            post_rgb);
        fwd_rgb_tensor = post_rgb;
        return;
    }

    // ---- SH mode ----
    // Skybox SH into a per-batch image from engine state buffers directly (no
    // R_wc / dist_coeffs gather); the kernel does the cam_indices indirection.
    bg.fwd_background.resize(PoolSlot::EngBgSkyImage, C_batch, H, W);
    TorchTensorView bg_image_tv = _dt3d_tv(bg.fwd_background);
    BgShViews vs = _engine_bg_sh_views(C_batch);
    render_background_sh_forward(
        W, H, engine().camera.model_str,
        engine().camera.distortion_str, bg.sh_degree,
        vs.viewmats, vs.intrins, vs.dist_coeffs,
        vs.sh_coeffs, bg_image_tv);

    DeviceTensor3D<float3> bg_image(bg_image_tv);
    blend_background_forward(bg.fwd_pre_blend_rgb, Ts_in, bg_image, post_rgb);
    fwd_rgb_tensor = post_rgb;
}


// ============================================================================
// Backward hook
// ============================================================================
void _engine_background_backward_hook(
    TorchTensorView v_render_rgb,   // [C, H, W, 3] in/out (post-blend in; pre-blend out)
    TorchTensorView v_render_Ts,    // [C, H, W, 1] in/out (accumulate blend's v_T)
    float overexposure_reg_weight   // fused here: needs the blend composite
) {
    auto& bg = engine().background;
    if (!bg.enabled || bg.mode == EngineBackground::Mode::None) return;
    if (std::get<0>(v_render_rgb) == 0 || std::get<0>(v_render_Ts) == 0) return;

    int C_batch = (int)engine().camera.num;
    int H       = engine().camera.height;
    int W       = engine().camera.width;
    if (C_batch <= 0) return;

    auto& fwd_rgb_tensor = std::get<0>(engine().fwd.renders);
    auto& fwd_Ts_tensor  = engine().fwd.render_Ts;

    // Defensive: the forward hook must have run earlier this step. Skip if the
    // expected per-iter buffers aren't populated (e.g. caller invoked
    // engine_compute_loss_backward directly without going through engine_train_step).
    if (bg.fwd_pre_blend_rgb.data_ptr() == nullptr) return;
    if (fwd_rgb_tensor.data_ptr() == nullptr || fwd_Ts_tensor.data_ptr() == nullptr) return;

    // Scratch v_Ts; blend backward writes here, then we accumulate into the
    // existing v_render_Ts (already populated by the per-pixel loss).
    float* v_Ts_scratch = DevicePool::global().acquire<float>(
        PoolSlot::EngBgSkyVTsScratch, (size_t)C_batch * H * W);
    TorchTensorView v_Ts_scratch_tv((uint64_t)v_Ts_scratch, 4,
        {(int64_t)C_batch, H, W, 1LL});

    DeviceTensor3D<float3> v_rgb(v_render_rgb);
    DeviceTensor3D<float>  v_Ts_scratch_dt(v_Ts_scratch_tv);

    if (bg.mode == EngineBackground::Mode::Noise ||
        bg.mode == EngineBackground::Mode::Pseudorandom) {
        // Pre-blend, not post: the overexposure term needs the unclamped
        // composite, which the blend output cannot recover. Passing post-blend
        // was harmless only while the affine derivative was the sole reader.
        DeviceTensor3D<float>  Ts_in(fwd_Ts_tensor);
        DeviceTensor3D<float3> v_out(v_render_rgb);
        blend_background_noise_backward(
            bg.splat_transfer, bg.splat_is_linear,
            bg.mode == EngineBackground::Mode::Pseudorandom,
            bg.fwd_pre_blend_rgb, Ts_in,
            bg.cur_randomize_weight, bg.cur_seed,
            overexposure_reg_weight,
            v_out, v_rgb, v_Ts_scratch_dt);
    } else {
        // SH mode: full blend backward gives v_background; propagate to v_sh.
        DeviceTensor3D<float3> rgb_pre (bg.fwd_pre_blend_rgb);
        DeviceTensor3D<float>  Ts_in   (fwd_Ts_tensor);
        DeviceTensor3D<float3> bg_image(bg.fwd_background);
        DeviceTensor3D<float3> v_out   (v_render_rgb);

        // v_background scratch.
        float* v_bg_dev = DevicePool::global().acquire<float>(
            PoolSlot::EngBgSkyVBg, (size_t)C_batch * H * W * 3);
        TorchTensorView v_bg_tv((uint64_t)v_bg_dev, 4,
            {(int64_t)C_batch, H, W, 3LL});
        DeviceTensor3D<float3> v_bg(v_bg_tv);

        blend_background_backward(
            rgb_pre, Ts_in, bg_image, overexposure_reg_weight,
            v_out, v_rgb, v_Ts_scratch_dt, v_bg);

        // v_sh_coeffs: zero per-iter, persists past hook for the optim step.
        int64_t sh_n = bg.sh_coeffs.size();
        float* v_sh_dev = DevicePool::global().acquire<float>(
            PoolSlot::EngBgSkyVSh, (size_t)sh_n * 3);
        backend::memset_async(v_sh_dev, 0, sh_n * 3 * sizeof(float),
                              backend::kDefaultStream);
        TorchTensorView v_sh_tv((uint64_t)v_sh_dev, 4, {sh_n, 3LL});

        TorchTensorView bg_image_tv = _dt3d_tv(bg.fwd_background);
        TorchTensorView v_bg_tv2    = _dt3d_tv(v_bg);
        BgShViews vs = _engine_bg_sh_views(C_batch);

        render_background_sh_backward(
            W, H, engine().camera.model_str,
        engine().camera.distortion_str, bg.sh_degree,
            vs.viewmats, vs.intrins, vs.dist_coeffs,
            vs.sh_coeffs,
            bg_image_tv, v_bg_tv2,
            v_sh_tv);
    }

    // v_render_Ts += v_Ts_scratch
    {
        int64_t numel = (int64_t)C_batch * H * W;
        DeviceVector<float> dst(TorchTensorView(std::get<0>(v_render_Ts), 4,
                                                {numel, 1LL}));
        DeviceVector<float> src(TorchTensorView((uint64_t)v_Ts_scratch, 4,
                                                {numel, 1LL}));
        float_add_into(dst, src, numel);
    }
}


// ============================================================================
// Optimizer step: Adam over SH coeffs (DC slot uses lr_dc, slots 1+ use lr_sh).
// Reads `eng.bg_sky.v_sh` pool buffer populated by the backward hook.
// ============================================================================
void engine_background_optim_step(int step, const BackgroundStepConfig& cfg) {
    auto& bg = engine().background;
    if (!bg.enabled) return;
    if (bg.mode != EngineBackground::Mode::Sh) return;

    _ensure_bg_sh_optim_state();
    int64_t n = bg.sh_coeffs.size();
    if (n == 0) return;

    float* v_sh_dev = DevicePool::global().acquire<float>(
        PoolSlot::EngBgSkyVSh, (size_t)n * 3);

    DeviceVector<int32_t> no_per_splat_steps;
    int32_t adam_step = step + 1;

    if (cfg.lr_dc > 0.0f) {
        fused_adam_step(
            1,
            _sh_slice_fnd(bg.sh_coeffs.data_ptr(),       1),
            _sh_slice_fnd((float3*)v_sh_dev,             1),
            _sh_slice_fnd(bg.sh_g1.data_ptr(),           1),
            _sh_slice_fnd(bg.sh_g2.data_ptr(),           1),
            cfg.lr_dc, adam_step, no_per_splat_steps,
            /*l2_reg=*/0.0f, /*l2_reg_offset=*/0.0f,
            /*grad_scale=*/1.0f, /*zero_grad=*/false);
    }
    if (n > 1 && cfg.lr_sh > 0.0f) {
        fused_adam_step(
            n - 1,
            _sh_slice_fnd(bg.sh_coeffs.data_ptr() + 1,   n - 1),
            _sh_slice_fnd((float3*)v_sh_dev + 1,         n - 1),
            _sh_slice_fnd(bg.sh_g1.data_ptr() + 1,       n - 1),
            _sh_slice_fnd(bg.sh_g2.data_ptr() + 1,       n - 1),
            cfg.lr_sh, adam_step, no_per_splat_steps,
            /*l2_reg=*/0.0f, /*l2_reg_offset=*/0.0f,
            /*grad_scale=*/1.0f, /*zero_grad=*/false);
    }
}


// ============================================================================
// Copy the current background image to a host [C, H, W, 3] buffer.
//
// SH mode: returns the skybox rendered by the most recent forward_3dgs (so
//          the viewer / eval gets a per-camera skybox matching the rendered rgb).
// Noise mode: fills with the mean blend color (0.5 in sRGB-space, optionally
//          gamma-corrected) so the "background" dropdown shows a sensible
//          uniform color rather than uninitialized memory or random per-frame
//          noise. We don't materialize the per-pixel noise image because the
//          noise is applied without an intermediate buffer.
//
// Returns 1 on success (image written), 0 if no engine background is active or
// the SH-mode skybox hasn't been rendered yet.
// ============================================================================
int engine_copy_background_to_host(TorchTensorView out_image) {
    auto& bg = engine().background;
    if (!bg.enabled || bg.mode == EngineBackground::Mode::None) return 0;

    int C_batch = (int)engine().camera.num;
    int H       = engine().camera.height;
    int W       = engine().camera.width;
    if (C_batch <= 0 || H <= 0 || W <= 0) return 0;

    // Validate the output shape: expect (..., H, W, 3) and at least C_batch
    // batch items worth of data.
    const auto& s = std::get<2>(out_image);
    if (s.size() < 3 || s[s.size() - 1] != 3 ||
        s[s.size() - 2] != (int64_t)W || s[s.size() - 3] != (int64_t)H) return 0;
    if (std::get<0>(out_image) == 0) return 0;
    size_t nbytes = (size_t)C_batch * H * W * sizeof(float3);

    auto& cs = engine().color_space;

    if (bg.mode == EngineBackground::Mode::Sh) {
        if (bg.fwd_background.data_ptr() == nullptr) return 0;

        // When the splat works in a non-sRGB color space, the SH-rendered
        // skybox is in that working color space. Convert through a scratch
        // device buffer so the host gets sRGB without mutating fwd_background.
        const float3* src = bg.fwd_background.data_ptr();
        if (cs.splat_enabled) {
            float3* scratch = DevicePool::global().acquire<float3>(
                PoolSlot::ColorSpaceBgSrgb, (size_t)C_batch * H * W);
            DeviceTensor3D<float3> in_view(TorchTensorView(
                (uint64_t)src, 4,
                {(int64_t)C_batch, (int64_t)H, (int64_t)W, 3LL}));
            DeviceTensor3D<float3> out_view(TorchTensorView(
                (uint64_t)scratch, 4,
                {(int64_t)C_batch, (int64_t)H, (int64_t)W, 3LL}));
            working_to_display_forward(cs.splat_transfer, cs.splat_is_linear,
                                       in_view, cs.splat_color_matrix, out_view);
            src = scratch;
        }
        backend::memcpy_sync((void*)std::get<0>(out_image), src, nbytes,
                             backend::MemcpyKind::DeviceToHost);
        return 1;
    }

    // Noise mode: pre-fill host buffer with the mean blend color. When the
    // color space is configured we still return a uniform mid-gray; the host
    // value here is what shows up under regions blended toward random noise.
    float* h = (float*)std::get<0>(out_image);
    float fill = colorspace::display_to_working(
        0.5f, (colorspace::Transfer)bg.splat_transfer, bg.splat_is_linear);
    int64_t n = (int64_t)C_batch * H * W * 3;
    for (int64_t i = 0; i < n; ++i) h[i] = fill;
    return 1;
}
