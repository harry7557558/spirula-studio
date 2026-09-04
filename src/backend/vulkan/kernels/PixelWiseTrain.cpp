// Vulkan implementations of the PixelWise TRAINING launch APIs
// (kernels/pixelwise/PixelWise.cuh backward subset + linear->ray depth remap) and the
// color-shift regularizer (engine/EngineInternal.h / ColorShiftReg.cu).
// Device work: shaders/pixel_wise_train.slang. The render-path
// forwards live in PixelWiseRender.cpp.

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Common.cuh>

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
    uint64_t rgb, transmittance, v_out_rgb, v_rgb, v_transmittance;
    float overexposure_scale, randomize_weight;
    uint32_t seed, HW, total, wgs_per_row, W, blocky;
};
// 40 + 32 lands on an 8 boundary, so unlike the branch's 7-field version this
// one needs no tail padding.
static_assert(sizeof(BlendBgNoiseBwdParams) == 5 * 8 + 8 * 4, "layout");

// Mirrors RgbToSrgbBwdParams.
struct RgbToSrgbBwdParams {
    uint64_t rgb, color_matrix, v_out_rgb, v_rgb;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(RgbToSrgbBwdParams) == 4 * 8 + 2 * 4, "layout");

// Mirrors OverexposureParams.
struct OverexposureParams {
    uint64_t rgb, v_rgb;
    float scale;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(OverexposureParams) == 2 * 8 + 3 * 4 + 4 /*pad*/,
              "layout");

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
    DeviceTensor3D<float3> rgb,
    DeviceTensor3D<float> transmittance,
    float randomize_weight,
    uint32_t seed,
    float overexposure_weight,
    DeviceTensor3D<float3> v_out_rgb,
    DeviceTensor3D<float3> v_rgb,
    DeviceTensor3D<float> v_transmittance
) {
    const int64_t hw = rgb.size<1>() * rgb.size<2>();
    const int64_t total = rgb.size<0>() * hw;
    BlendBgNoiseBwdParams p{};
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
    vkk::dispatch_flat("pixel_wise_train.blend_bg_noise_bwd",
                       backend::vk::SpecList{(uint32_t)transfer,
                                             is_linear ? 1u : 0u},
                       total, 128, &p, sizeof(p), &p.wgs_per_row);
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

void overexposure_grad_add(
    DeviceTensor3D<float3> rgb,
    float weight,
    DeviceTensor3D<float3> v_rgb
) {
    int64_t b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();
    if (b <= 0 || h <= 0 || w <= 0 || weight == 0.0f) return;
    OverexposureParams p{};
    p.rgb = (uint64_t)rgb.data_ptr();
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
