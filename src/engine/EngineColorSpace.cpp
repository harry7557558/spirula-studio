// Engine working-space -> display conversion (gamut + output transfer).
//
// When the splat (model) works in a non-sRGB color space, the rendered RGB
// is converted to display code values before any downstream stage (bilagrid /
// PPISP / loss). GT imagery in a non-sRGB space is converted once at upload
// time and kept that way, after decoding a log curve off it when one is set.
// The 3x3 matrix maps the source gamut to Rec.709; the transfer is
// colorspace::Transfer. See docs/notes/color-transfer.md and dlog-m.md.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "core/ColorSpace.h"
#include "core/DlogM.h"
#include "kernels/pixelwise/PixelWise.cuh"

#include <algorithm>
#include <stdexcept>
#include <vector>


static void _alloc_and_upload_matrix(
    DeviceTensor2D<float3>& dst, const float* host9, PoolSlot key
) {
    dst.resize(key, 3, 3);
    backend::memcpy_sync(dst.data_ptr(), host9, 9 * sizeof(float), backend::MemcpyKind::HostToDevice);
}


/*[AutoHeaderGeneratorExport]*/
void engine_init_color_space(
    // Splat side
    bool splat_enabled,
    int splat_transfer,
    bool splat_is_linear,
    std::vector<float> splat_color_matrix,   // [9], row-major
    // Image side
    bool image_enabled,
    int image_transfer,
    bool image_is_linear,
    std::vector<float> image_color_matrix    // [9], row-major
) {
    auto& cs = engine().color_space;
    cs.splat_enabled   = splat_enabled;
    cs.splat_transfer  = splat_transfer;
    cs.splat_is_linear = splat_is_linear;
    if (splat_enabled) {
        if (splat_color_matrix.size() != 9)
            throw std::runtime_error("engine_init_color_space: splat_color_matrix must have 9 elements");
        _alloc_and_upload_matrix(cs.splat_color_matrix, splat_color_matrix.data(),
                                 PoolSlot::ColorSpaceSplatMatrix);
    }

    cs.image_enabled   = image_enabled;
    cs.image_transfer  = image_transfer;
    cs.image_is_linear = image_is_linear;
    if (image_enabled) {
        if (image_color_matrix.size() != 9)
            throw std::runtime_error("engine_init_color_space: image_color_matrix must have 9 elements");
        _alloc_and_upload_matrix(cs.image_color_matrix, image_color_matrix.data(),
                                 PoolSlot::ColorSpaceImageMatrix);
        std::copy_n(image_color_matrix.begin(), 9,
                    cs.image_color_matrix_host.begin());
    }
}


/*[AutoHeaderGeneratorExport]*/
void engine_init_image_decode(int curve) {
    auto& cs = engine().color_space;
    // The decode's output is linear Rec.2020; anything else would be read
    // through the sRGB EOTF or the wrong primaries without an error.
    if (curve != 0 && !(cs.image_enabled && cs.image_is_linear))
        throw std::runtime_error("engine_init_image_decode: the image side must be "
                                 "enabled and linear (engine_init_color_space first)");
    cs.image_curve = curve;
}


void _engine_color_space_gt_pixel(float c[3]) {
    const auto& cs = engine().color_space;
    if (!cs.image_enabled) return;
    colorspace::input_curve_to_rec2020((colorspace::InputCurve)cs.image_curve, c);
    if (!cs.image_is_linear)
        for (int k = 0; k < 3; k++) c[k] = colorspace::srgb_to_linear(c[k]);
    colorspace::apply3x3(cs.image_color_matrix_host, c);
    const auto transfer = (colorspace::Transfer)cs.image_transfer;
    for (int k = 0; k < 3; k++) c[k] = colorspace::tone_encode(c[k], transfer);
}


void _engine_color_space_forward() {
    auto& cs = engine().color_space;
    if (!cs.splat_enabled) return;

    auto& fwd_rgb_tensor = std::get<0>(engine().fwd.renders);
    if (fwd_rgb_tensor.data_ptr() == nullptr) return;

    // Mirror the background-blend pattern: alias the current rgb buffer as
    // "pre" (no D2D copy), allocate a fresh "post" buffer, run the
    // conversion out-of-place, then re-point fwd.renders.rgb at post. The
    // pre buffer (held only by cs.fwd_pre and the previous fwd_rgb_tensor
    // pool allocation) survives until the next forward.
    cs.fwd_pre = fwd_rgb_tensor;

    int64_t B = cs.fwd_pre.size<0>(), H = cs.fwd_pre.size<1>(), W = cs.fwd_pre.size<2>();
    DeviceTensor3D<float3> post_rgb;
    post_rgb.resize(PoolSlot::ColorSpaceFwdPost, B, H, W);
    working_to_display_forward(cs.splat_transfer, cs.splat_is_linear,
                               cs.fwd_pre, cs.splat_color_matrix, post_rgb);
    fwd_rgb_tensor = post_rgb;
}


void _engine_color_space_backward_hook(TorchTensorView v_render_rgb) {
    auto& cs = engine().color_space;
    if (!cs.splat_enabled) return;
    if (cs.fwd_pre.data_ptr() == nullptr) return;

    DeviceTensor3D<float3> v_rgb_t = DeviceTensor3D<float3>(v_render_rgb);
    if (v_rgb_t.data_ptr() == nullptr) return;

    // In-place is safe: the vjp is per-pixel local. The background backward
    // hook reads bg.fwd_pre_blend_rgb, its own alias to the pre-bg rgb, so
    // engine().fwd.renders.rgb does not need swapping back.
    working_to_display_backward(cs.splat_transfer, cs.splat_is_linear,
                                cs.fwd_pre, cs.splat_color_matrix,
                                v_rgb_t, v_rgb_t);
}


void _engine_color_space_apply_to_gt() {
    auto& cs = engine().color_space;
    if (!cs.image_enabled) return;

    DeviceTensor3D<float3> rgb = engine().gt.rgb;
    if (rgb.data_ptr() == nullptr) return;

    // Before the conversion, which expects the linear light the decode yields.
    if (cs.image_curve != 0) input_curve_decode_forward(cs.image_curve, rgb, rgb);
    working_to_display_forward(cs.image_transfer, cs.image_is_linear, rgb,
                               cs.image_color_matrix, rgb);
}
