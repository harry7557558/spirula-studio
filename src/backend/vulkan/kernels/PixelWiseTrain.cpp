// Vulkan implementations of the PixelWise TRAINING launch APIs
// (kernels/pixelwise/PixelWise.cuh backward subset + linear->ray depth remap) and the
// color-shift regularizer (engine/EngineInternal.h / ColorShiftReg.cu).
// Device work: shaders/pixel_wise_train.slang. The render-path
// forwards live in PixelWiseRender.cpp.

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Common.cuh>
#include <core/PixelFormat.h>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors BlendBgBwdParams in shaders/pixel_wise_train.slang.
struct BlendBgBwdParams {
    uint64_t rgb, transmittance, background, v_out_rgb, v_rgb,
        v_transmittance, v_background;
    float overexposure_scale;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BlendBgBwdParams) == 7 * 8 + 3 * 4 + 4 /*pad*/,
              "layout");

// Mirrors BlendBgNoiseBwdParams.
struct BlendBgNoiseBwdParams {
    uint64_t rgb, transmittance, v_out_rgb, v_rgb, v_transmittance,
             exponent_by_cam, cam_indices;
    float overexposure_scale, randomize_weight;
    uint32_t seed, HW, total, wgs_per_row, W, blocky, block_px, match_luma;
};
static_assert(sizeof(BlendBgNoiseBwdParams) == 7 * 8 + 10 * 4, "layout");

// Mirrors BlendBgColorBwdParams.
struct BlendBgColorBwdParams {
    uint64_t rgb, transmittance, v_out_rgb, v_rgb, v_transmittance;
    float bg_r, bg_g, bg_b, overexposure_scale;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BlendBgColorBwdParams) == 5 * 8 + 6 * 4, "layout");

// Mirrors RgbToSrgbBwdParams.
struct RgbToSrgbBwdParams {
    uint64_t rgb, color_matrix, v_out_rgb, v_rgb;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(RgbToSrgbBwdParams) == 4 * 8 + 2 * 4, "layout");

// Mirrors SplitMaskWeightParams.
struct SplitMaskWeightParams {
    uint64_t weight, mask;
    uint32_t n, words, wgs_per_row, _pad;
};
static_assert(sizeof(SplitMaskWeightParams) == 2 * 8 + 4 * 4, "layout");

// Mirrors ScaleByMaskWeightParams.
struct ScaleByMaskWeightParams {
    uint64_t data, weight;
    uint32_t H, W, C, Hm, Wm, total, wgs_per_row, _pad;
};
static_assert(sizeof(ScaleByMaskWeightParams) == 2 * 8 + 8 * 4, "layout");

// Mirrors OverexposureParams.
struct OverexposureParams {
    uint64_t rgb, v_rgb;
    float scale;
    uint32_t total, wgs_per_row, rgb_fmt;
};
static_assert(sizeof(OverexposureParams) == 2 * 8 + 4 * 4, "layout");

// Mirrors DepthToNormalBwdParams.
struct DepthToNormalBwdParams {
    uint64_t intrins, dist_coeffs, depths, v_normals, v_depths;
    uint32_t W, H, B, is_ray_depth;
    int32_t camera_model;
};
static_assert(sizeof(DepthToNormalBwdParams) == 5 * 8 + 5 * 4 + 4 /*pad*/,
              "layout");

// Mirrors LinToRayDepthParams.
struct LinToRayDepthParams {
    uint64_t intrins, dist_coeffs, depths;
    float sx, sy;
    uint32_t W, H, B;
    int32_t camera_model;
    uint32_t _pad0;
};
static_assert(sizeof(LinToRayDepthParams) == 3 * 8 + 7 * 4 + 4 /*pad*/,
              "layout");

// Mirrors ColorShiftInjectParams.
struct ColorShiftInjectParams {
    uint64_t v_render_rgb, post_rgb, pre_rgb, ema, batch_sum;
    float reg_coef;
    uint32_t N, wgs_per_row;
};
static_assert(sizeof(ColorShiftInjectParams) == 5 * 8 + 3 * 4 + 4 /*pad*/,
              "layout");

// Mirrors ColorShiftUpdateParams.
struct ColorShiftUpdateParams {
    uint64_t ema, batch_sum;
    float beta, inv_n_pixels;
};
static_assert(sizeof(ColorShiftUpdateParams) == 2 * 8 + 2 * 4, "layout");

// 2 * weight / N for L = weight * mean(max(-x, x-1, 0)^2) over N = B*H*W*3.
float overexposure_scale(int64_t b, int64_t h, int64_t w, float weight) {
    if (weight == 0.0f) return 0.0f;
    double n = (double)b * (double)h * (double)w * 3.0;
    return (float)(2.0 * (double)weight / n);
}

}  // namespace

/* API definitions matching kernels/pixelwise/PixelWise.cuh (training subset) */

void blend_background_backward(
    DeviceTensor3D<float3> rgb,
    DeviceTensor3D<float> transmittance,
    DeviceTensor3D<float3> background,
    float overexposure_weight,
    DeviceTensor3D<float3> v_out_rgb,
    DeviceTensor3D<float3> v_rgb,
    DeviceTensor3D<float> v_transmittance,
    DeviceTensor3D<float3> v_background
) {
    const int64_t total = rgb.size<0>() * rgb.size<1>() * rgb.size<2>();
    BlendBgBwdParams p{};
    p.overexposure_scale = overexposure_scale(
        rgb.size<0>(), rgb.size<1>(), rgb.size<2>(), overexposure_weight);
    p.rgb = (uint64_t)rgb.data_ptr();
    p.transmittance = (uint64_t)transmittance.data_ptr();
    p.background = (uint64_t)background.data_ptr();
    p.v_out_rgb = (uint64_t)v_out_rgb.data_ptr();
    p.v_rgb = (uint64_t)v_rgb.data_ptr();
    p.v_transmittance = (uint64_t)v_transmittance.data_ptr();
    p.v_background = (uint64_t)v_background.data_ptr();
    p.total = (uint32_t)total;
    vkk::dispatch_flat("pixel_wise_train.blend_bg_bwd",
                       backend::vk::SpecList{}, total, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void blend_background_noise_backward(
    int transfer,
    bool is_linear,
    bool blocky,
    unsigned block_px,
    DeviceTensor3D<float3> rgb,
    DeviceTensor3D<float> transmittance,
    float randomize_weight,
    uint32_t seed,
    const float* exponent_by_cam,
    const int32_t* cam_indices,
    float overexposure_weight,
    DeviceTensor3D<float3> v_out_rgb,
    DeviceTensor3D<float3> v_rgb,
    DeviceTensor3D<float> v_transmittance
) {
    const int64_t hw = rgb.size<1>() * rgb.size<2>();
    const int64_t total = rgb.size<0>() * hw;
    BlendBgNoiseBwdParams p{};
    p.exponent_by_cam = (uint64_t)exponent_by_cam;
    p.cam_indices = (uint64_t)cam_indices;
    p.match_luma = exponent_by_cam ? 1u : 0u;
    p.overexposure_scale = overexposure_scale(
        rgb.size<0>(), rgb.size<1>(), rgb.size<2>(), overexposure_weight);
    p.rgb = (uint64_t)rgb.data_ptr();
    p.transmittance = (uint64_t)transmittance.data_ptr();
    p.v_out_rgb = (uint64_t)v_out_rgb.data_ptr();
    p.v_rgb = (uint64_t)v_rgb.data_ptr();
    p.v_transmittance = (uint64_t)v_transmittance.data_ptr();
    p.randomize_weight = randomize_weight;
    p.seed = seed;
    p.HW = (uint32_t)hw;
    p.total = (uint32_t)total;
    p.W = (uint32_t)rgb.size<2>();
    p.blocky = blocky ? 1u : 0u;
    p.block_px = (uint32_t)block_px;
    vkk::dispatch_flat("pixel_wise_train.blend_bg_noise_bwd",
                       backend::vk::SpecList{(uint32_t)transfer,
                                             is_linear ? 1u : 0u},
                       total, 128, &p, sizeof(p), &p.wgs_per_row);
}

void blend_background_color_backward(
    DeviceTensor3D<float3> rgb,
    DeviceTensor3D<float> transmittance,
    float3 background,
    float overexposure_weight,
    DeviceTensor3D<float3> v_out_rgb,
    DeviceTensor3D<float3> v_rgb,
    DeviceTensor3D<float> v_transmittance
) {
    const int64_t total = rgb.size<0>() * rgb.size<1>() * rgb.size<2>();
    BlendBgColorBwdParams p{};
    p.overexposure_scale = overexposure_scale(
        rgb.size<0>(), rgb.size<1>(), rgb.size<2>(), overexposure_weight);
    p.rgb = (uint64_t)rgb.data_ptr();
    p.transmittance = (uint64_t)transmittance.data_ptr();
    p.v_out_rgb = (uint64_t)v_out_rgb.data_ptr();
    p.v_rgb = (uint64_t)v_rgb.data_ptr();
    p.v_transmittance = (uint64_t)v_transmittance.data_ptr();
    p.bg_r = background.x;
    p.bg_g = background.y;
    p.bg_b = background.z;
    p.total = (uint32_t)total;
    vkk::dispatch_flat("pixel_wise_train.blend_bg_color_bwd",
                       backend::vk::SpecList{}, total, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void working_to_display_backward(
    int transfer,
    bool is_linear,
    DeviceTensor3D<float3> rgb,
    DeviceTensor2D<float3> color_matrix,
    DeviceTensor3D<float3> v_out_rgb,
    DeviceTensor3D<float3> v_rgb
) {
    const int64_t total = rgb.size<0>() * rgb.size<1>() * rgb.size<2>();
    RgbToSrgbBwdParams p{};
    p.rgb = (uint64_t)rgb.data_ptr();
    p.color_matrix = (uint64_t)color_matrix.data_ptr();
    p.v_out_rgb = (uint64_t)v_out_rgb.data_ptr();
    p.v_rgb = (uint64_t)v_rgb.data_ptr();
    p.total = (uint32_t)total;
    vkk::dispatch_flat("pixel_wise_train.working_to_display_bwd_k",
                       backend::vk::SpecList{(uint32_t)transfer,
                                             is_linear ? 1u : 0u},
                       total, 128, &p, sizeof(p), &p.wgs_per_row);
}

void split_mask_weight(TorchTensorView weight, TorchTensorView mask) {
    const auto& s = std::get<2>(weight);
    const int64_t n = s[0] * s[1] * s[2];
    if (n <= 0) return;
    SplitMaskWeightParams p{};
    p.weight = (uint64_t)std::get<0>(weight);
    p.mask = (uint64_t)std::get<0>(mask);
    p.n = (uint32_t)n;
    p.words = (uint32_t)((n + 3) / 4);
    vkk::dispatch_flat("pixel_wise_train.split_mask_weight", backend::vk::SpecList{},
                       p.words, 256, &p, sizeof(p), &p.wgs_per_row);
}

void scale_by_mask_weight(TorchTensorView data, TorchTensorView weight) {
    const auto& s = std::get<2>(data);
    const auto& m = std::get<2>(weight);
    const int64_t B = s[0], H = s[1], W = s[2];
    if (B <= 0 || H <= 0 || W <= 0) return;
    ScaleByMaskWeightParams p{};
    p.data = (uint64_t)std::get<0>(data);
    p.weight = (uint64_t)std::get<0>(weight);
    p.H = (uint32_t)H;
    p.W = (uint32_t)W;
    p.C = s.size() > 3 ? (uint32_t)s[3] : 1u;
    p.Hm = (uint32_t)m[1];
    p.Wm = (uint32_t)m[2];
    p.total = (uint32_t)(B * H * W);
    vkk::dispatch_flat("pixel_wise_train.scale_by_mask_weight", backend::vk::SpecList{},
                       p.total, 256, &p, sizeof(p), &p.wgs_per_row);
}

void overexposure_grad_add(
    TorchTensorView rgb,
    float weight,
    DeviceTensor3D<float3> v_rgb
) {
    int64_t b = v_rgb.size<0>(), h = v_rgb.size<1>(), w = v_rgb.size<2>();
    if (b <= 0 || h <= 0 || w <= 0 || weight == 0.0f) return;
    OverexposureParams p{};
    p.rgb = std::get<0>(rgb);
    p.rgb_fmt = (uint32_t)pixel_format(rgb);
    p.v_rgb = (uint64_t)v_rgb.data_ptr();
    p.scale = overexposure_scale(b, h, w, weight);
    p.total = (uint32_t)(b * h * w);
    vkk::dispatch_flat("pixel_wise_train.overexposure_add",
                       backend::vk::SpecList{}, b * h * w, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void depth_to_normal_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    DeviceTensor3D<float> depths,
    DeviceTensor3D<float3> v_normals,
    DeviceTensor3D<float> v_depths
) {
    const uint32_t B = (uint32_t)depths.size<0>();
    const uint32_t H = (uint32_t)depths.size<1>();
    const uint32_t W = (uint32_t)depths.size<2>();
    if (B * H * W == 0) return;
    DepthToNormalBwdParams p{};
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.depths = (uint64_t)depths.data_ptr();
    p.v_normals = (uint64_t)v_normals.data_ptr();
    p.v_depths = (uint64_t)v_depths.data_ptr();
    p.W = W;
    p.H = H;
    p.B = B;
    p.is_ray_depth = is_ray_depth ? 1u : 0u;
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    p.camera_model = (int32_t)cd.cam;
    vkk::dispatch("pixel_wise_train.d2n_bwd",
                  backend::vk::SpecList{0u, 0u, cd.dist}, (W + 15) / 16,
                  (H + 15) / 16, B, &p, sizeof(p));
}

void depth_to_normal_backward_tv(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    TorchTensorView depths,
    TorchTensorView v_normals,
    TorchTensorView v_depths
) {
    depth_to_normal_backward(camera_model, distortion, intrins, dist_coeffs,
                             is_ray_depth,
                             DeviceTensor3D<float>(depths),
                             DeviceTensor3D<float3>(v_normals),
                             DeviceTensor3D<float>(v_depths));
}

void linear_depth_to_ray_depth_inplace(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    int image_width, int image_height,
    DeviceTensor3D<float> depths
) {
    int64_t b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();
    if (b <= 0 || h <= 0 || w <= 0) return;
    LinToRayDepthParams p{};
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.depths = (uint64_t)depths.data_ptr();
    p.sx = (image_width > 0) ? (float)w / (float)image_width : 1.0f;
    p.sy = (image_height > 0) ? (float)h / (float)image_height : 1.0f;
    p.W = (uint32_t)w;
    p.H = (uint32_t)h;
    p.B = (uint32_t)b;
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    p.camera_model = (int32_t)cd.cam;
    vkk::dispatch("pixel_wise_train.lin_to_ray_depth",
                  backend::vk::SpecList{0u, 0u, cd.dist},
                  (uint32_t)((w + 127) / 128), (uint32_t)h, (uint32_t)b, &p,
                  sizeof(p));
}

/* color_shift_reg_step (engine/EngineInternal.h / ColorShiftReg.cu) */

void color_shift_reg_step(
    float* v_render_rgb,
    const float* post_rgb,
    const float* pre_rgb,
    float* shift_reg_ema,
    float* shift_reg_batch_sum,
    int N_pixels,
    float weight,
    float beta,
    int64_t step,
    backend::Stream stream
) {
    (void)stream;  // single queue; default-stream submission order suffices
    if (weight <= 0.0f || N_pixels <= 0) return;
    if (!shift_reg_ema || !shift_reg_batch_sum) return;

    float bc = 1.0f - std::pow(beta, (float)(step + 1));
    if (bc < 1e-30f) bc = 1.0f;
    float reg_coef = 2.0f * weight * bc / ((float)N_pixels);  // warmup

    ColorShiftInjectParams p{};
    p.v_render_rgb = (uint64_t)v_render_rgb;
    p.post_rgb = (uint64_t)post_rgb;
    p.pre_rgb = (uint64_t)pre_rgb;
    p.ema = (uint64_t)shift_reg_ema;
    p.batch_sum = (uint64_t)shift_reg_batch_sum;
    p.reg_coef = reg_coef;
    p.N = (uint32_t)N_pixels;
    vkk::dispatch_flat("pixel_wise_train.color_shift_inject",
                       backend::vk::SpecList{}, N_pixels, 256, &p, sizeof(p),
                       &p.wgs_per_row);

    ColorShiftUpdateParams u{};
    u.ema = (uint64_t)shift_reg_ema;
    u.batch_sum = (uint64_t)shift_reg_batch_sum;
    u.beta = beta;
    u.inv_n_pixels = 1.0f / (float)N_pixels;
    vkk::dispatch("pixel_wise_train.color_shift_update",
                  backend::vk::SpecList{}, 1, 1, 1, &u, sizeof(u));
}
