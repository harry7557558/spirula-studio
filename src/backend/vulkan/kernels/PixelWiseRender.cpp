// Vulkan implementations of the PixelWise launch APIs needed by the RENDER
// path (kernels/pixelwise/PixelWise.cuh): background blends, the display encode,
// depth->normal. Device work: shaders/pixel_wise_render.slang. The
// training-time PixelWise kernels (losses, backwards, warps) are not here —
// they land with the training phase.

#include <kernels/pixelwise/PixelWise.cuh>
#include <core/Common.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors BlendBgParams in shaders/pixel_wise_render.slang.
struct BlendBgParams {
    uint64_t rgb, transmittance, background, out_rgb;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BlendBgParams) == 4 * 8 + 2 * 4, "layout");

// Mirrors BlendBgNoiseParams.
struct BlendBgNoiseParams {
    uint64_t rgb, transmittance, out_rgb;
    float randomize_weight;
    uint32_t seed, HW, total, wgs_per_row;
};
static_assert(sizeof(BlendBgNoiseParams) == 3 * 8 + 5 * 4 + 4 /*pad*/,
              "layout");

// Mirrors RgbToSrgbParams.
struct RgbToSrgbParams {
    uint64_t rgb, color_matrix, out_rgb;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(RgbToSrgbParams) == 3 * 8 + 2 * 4, "layout");

// Mirrors DepthToNormalParams.
struct DepthToNormalParams {
    uint64_t intrins, dist_coeffs, depths, normals;
    uint32_t W, H, B, is_ray_depth;
    int32_t camera_model;
    uint32_t _pad0;
};
static_assert(sizeof(DepthToNormalParams) == 4 * 8 + 6 * 4, "layout");

}  // namespace

/* API definitions matching kernels/pixelwise/PixelWise.cuh (render-path subset) */

void blend_background_forward(
    DeviceTensor3D<float3> rgb,
    DeviceTensor3D<float> transmittance,
    DeviceTensor3D<float3> background,
    DeviceTensor3D<float3> out_rgb
) {
    const int64_t total = rgb.size<0>() * rgb.size<1>() * rgb.size<2>();
    BlendBgParams p{};
    p.rgb = (uint64_t)rgb.data_ptr();
    p.transmittance = (uint64_t)transmittance.data_ptr();
    p.background = (uint64_t)background.data_ptr();
    p.out_rgb = (uint64_t)out_rgb.data_ptr();
    p.total = (uint32_t)total;
    vkk::dispatch_flat("pixel_wise_render.blend_background_fwd",
                       backend::vk::SpecList{}, total, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void blend_background_noise_forward(
    int transfer,
    bool is_linear,
    DeviceTensor3D<float3> rgb,
    DeviceTensor3D<float> transmittance,
    float randomize_weight,
    uint32_t seed,
    DeviceTensor3D<float3> out_rgb
) {
    const int64_t hw = rgb.size<1>() * rgb.size<2>();
    const int64_t total = rgb.size<0>() * hw;
    BlendBgNoiseParams p{};
    p.rgb = (uint64_t)rgb.data_ptr();
    p.transmittance = (uint64_t)transmittance.data_ptr();
    p.out_rgb = (uint64_t)out_rgb.data_ptr();
    p.randomize_weight = randomize_weight;
    p.seed = seed;
    p.HW = (uint32_t)hw;
    p.total = (uint32_t)total;
    vkk::dispatch_flat("pixel_wise_render.blend_background_noise_fwd",
                       backend::vk::SpecList{(uint32_t)transfer,
                                             is_linear ? 1u : 0u},
                       total, 128, &p, sizeof(p), &p.wgs_per_row);
}

void working_to_display_forward(
    int transfer,
    bool is_linear,
    DeviceTensor3D<float3> rgb,
    DeviceTensor2D<float3> color_matrix,
    DeviceTensor3D<float3> out_rgb
) {
    const int64_t total = rgb.size<0>() * rgb.size<1>() * rgb.size<2>();
    RgbToSrgbParams p{};
    p.rgb = (uint64_t)rgb.data_ptr();
    p.color_matrix = (uint64_t)color_matrix.data_ptr();
    p.out_rgb = (uint64_t)out_rgb.data_ptr();
    p.total = (uint32_t)total;
    vkk::dispatch_flat("pixel_wise_render.working_to_display_fwd",
                       backend::vk::SpecList{(uint32_t)transfer,
                                             is_linear ? 1u : 0u},
                       total, 128, &p, sizeof(p), &p.wgs_per_row);
}

void depth_to_normal_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    DeviceTensor3D<float> depths,
    DeviceTensor3D<float3> normals
) {
    const uint32_t B = (uint32_t)depths.size<0>();
    const uint32_t H = (uint32_t)depths.size<1>();
    const uint32_t W = (uint32_t)depths.size<2>();
    if (B * H * W == 0) return;
    DepthToNormalParams p{};
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.depths = (uint64_t)depths.data_ptr();
    p.normals = (uint64_t)normals.data_ptr();
    p.W = W;
    p.H = H;
    p.B = B;
    p.is_ray_depth = is_ray_depth ? 1u : 0u;
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    p.camera_model = (int32_t)cd.cam;
    vkk::dispatch("pixel_wise_render.depth_to_normal_fwd",
                  backend::vk::SpecList{0u, 0u, cd.dist}, (W + 15) / 16,
                  (H + 15) / 16, B, &p, sizeof(p));
}

void depth_to_normal_forward_tv(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    TorchTensorView depths,
    TorchTensorView normals
) {
    depth_to_normal_forward(camera_model, distortion, intrins, dist_coeffs,
                            is_ray_depth,
                            DeviceTensor3D<float>(depths),
                            DeviceTensor3D<float3>(normals));
}
