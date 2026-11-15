// Engine side of the fused appearance chain (kernels/pixelwise/AppearanceChain.cu):
// one launch from the background, PPISP and color-space state, replayed by the
// backward. SS_FUSED_APPEARANCE=0 restores the per-stage kernels, which keep a
// full-resolution image per stage for their backward.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "core/Env.h"
#include "kernels/pixelwise/PixelWise.cuh"


static bool _appearance_fusion_on() {
    static const bool on = [] {
        const char* v = spirula::env("FUSED_APPEARANCE");
        return !(v && v[0] == '0');
    }();
    return on;
}

DeviceTensor3D<float3> _engine_image(PoolSlot slot, int64_t C, int64_t H,
                                     int64_t W, PixelFormat fmt) {
    uint8_t* p = DevicePool::global().acquire<uint8_t>(
        slot, pixel_buffer_bytes(fmt, C * H * W, 3));
    return DeviceTensor3D<float3>(TorchTensorView((uint64_t)p, 4, {C, H, W, 3}));
}

TorchTensorView _engine_image_view(const DeviceTensor3D<float3>& t, PixelFormat fmt) {
    if (t.data_ptr() == nullptr) return _tv_null();
    return TorchTensorView((uint64_t)t.data_ptr(), pixel_format_bytes(fmt),
                           {t.size<0>(), t.size<1>(), t.size<2>(), 3});
}

static DeviceTensor3D<float3> _arena_image(const char* key, PoolPhase phase,
                                           int64_t C, int64_t H, int64_t W) {
    void* p = DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, pixel_buffer_bytes(PixelFormat::F16, C * H * W, 3),
        phase);
    return DeviceTensor3D<float3>(TorchTensorView((uint64_t)p, 4, {C, H, W, 3}));
}

bool _engine_appearance_forward(AppearancePpisp ppisp) {
    StepTimingScope timing(StepPart::Appearance, StepScale::Pixels);
    auto& a = engine().appearance;
    const bool transient = a.transient_pending;
    a.transient_pending = false;
    a.transient_post = DeviceTensor3D<float3>();
    a.fused = false;
    if (!_appearance_fusion_on() || !a.allow) return false;

    auto& cs = engine().color_space;
    auto& pp = engine().ppisp;
    AppearanceChainParams p;
    p.cam_indices = engine().bilagrid_cur_cam_indices.data_ptr();
    _engine_background_chain_params(p);
    if (cs.splat_enabled) {
        p.cs_enabled = 1;
        p.cs_transfer = cs.splat_transfer;
        p.cs_is_linear = cs.splat_is_linear ? 1 : 0;
        p.cs_matrix = (const float*)cs.splat_color_matrix.data_ptr();
    }
    if (ppisp != AppearancePpisp::Off && pp.enabled) {
        const PpispParamSpec spec = ppisp_param_spec(pp.param_type);
        p.ppisp = ppisp;
        p.ppisp_layout = spec.layout;
        p.ppisp_clamp = spec.clamp_output ? 1 : 0;
        p.ppisp_params = pp.params.data_ptr();
        p.intrins = (const float*)engine().camera.intrins.data_ptr();
    }

    // Nothing of the per-stage path survives a fused forward for its hooks.
    engine().background.fwd_pre_blend_rgb = DeviceTensor3D<float3>();
    cs.fwd_pre = DeviceTensor3D<float3>();
    pp.fwd_pre = DeviceTensor3D<float3>();
    if (p.bg == AppearanceBg::None && !p.cs_enabled &&
        p.ppisp == AppearancePpisp::Off)
        return true;

    auto& fwd = engine().fwd;
    auto& rgb = std::get<0>(fwd.renders);
    const int64_t C = rgb.size<0>(), H = rgb.size<1>(), W = rgb.size<2>();
    const bool half = fwd.image_fmt == PixelFormat::F16;
    // The tile-intersect phase is open from the forward's intersection until
    // the loss, which the grid reading this output runs before.
    DeviceTensor3D<float3> post =
        transient && half
            ? _arena_image("eng.appearance.post.transient", PoolPhase::TileIsect, C, H, W)
            : _engine_image(PoolSlot::EngAppearancePost, C, H, W, fwd.image_fmt);
    if (transient && half) a.transient_post = post;
    if (half)
        fwd.raw_rgb16 = _engine_image(PoolSlot::EngAppearanceRaw16, C, H, W,
                                      PixelFormat::F16);
    appearance_chain_forward(p, _engine_image_view(rgb, fwd.rgb_fmt), fwd.render_Ts,
                             _engine_image_view(post, fwd.image_fmt),
                             _engine_image_view(fwd.raw_rgb16, PixelFormat::F16));
    rgb = post;
    fwd.rgb_fmt = fwd.image_fmt;
    a.params = p;
    a.fused = true;
    return true;
}

DeviceTensor3D<float3> _engine_appearance_replay() {
    auto& fwd = engine().fwd;
    const auto& raw = fwd.raw_rgb16;
    DeviceTensor3D<float3> out =
        _arena_image("eng.appearance.replay", PoolPhase::ImageBwd, raw.size<0>(),
                     raw.size<1>(), raw.size<2>());
    // The forward read the raw render rounded to this copy, so this is it again.
    appearance_chain_forward(engine().appearance.params,
                             _engine_image_view(raw, PixelFormat::F16), fwd.render_Ts,
                             _engine_image_view(out, PixelFormat::F16), _tv_null());
    return out;
}

void _engine_appearance_backward(TorchTensorView v_render_rgb,
                                 TorchTensorView v_render_Ts,
                                 float overexposure_reg_weight) {
    auto& a = engine().appearance;
    if (!a.fused) return;
    AppearanceChainParams p = a.params;
    p.overexposure_weight = overexposure_reg_weight;
    if (p.ppisp != AppearancePpisp::Off) {
        _ensure_ppisp_optim_state();
        p.v_ppisp_params = engine().ppisp.grads.data_ptr();
    }
    const bool half = engine().fwd.raw_rgb16.data_ptr() != nullptr;
    const DeviceTensor3D<float3>& raw =
        half ? engine().fwd.raw_rgb16 : engine().fwd.raw_rgb;
    float* v_bg = nullptr;
    if (p.bg == AppearanceBg::Image) {
        v_bg = DevicePool::global().acquire<float>(
            PoolSlot::EngBgSkyVBg,
            (size_t)raw.size<0>() * raw.size<1>() * raw.size<2>() * 3);
        p.v_bg_image = v_bg;
    }
    appearance_chain_backward(p,
                              _engine_image_view(raw, half ? PixelFormat::F16
                                                           : PixelFormat::F32),
                              engine().fwd.render_Ts,
                              DeviceTensor3D<float3>(v_render_rgb),
                              DeviceTensor3D<float>(v_render_Ts));
    if (v_bg) _engine_background_sh_backward(v_bg);
}
