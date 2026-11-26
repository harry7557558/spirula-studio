// Vulkan implementation of the fused appearance chain
// (kernels/pixelwise/PixelWise.cuh appearance_chain_*). Device work:
// shaders/appearance_chain.slang.

#include <kernels/pixelwise/PixelWise.cuh>
#include <core/PixelFormat.h>
#include <core/Tensor.h>

#include <algorithm>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors AppParams in shaders/appearance_chain.slang.
struct AppParams {
    uint64_t rgb, transmittance, out_rgb, fwd_out, raw16, v_transmittance,
        bg_image, v_bg_image, exponent_by_cam, cam_indices, cs_matrix,
        ppisp_params, v_ppisp_params, intrins, bg_mask;
    float bg_r, bg_g, bg_b;
    float randomize_weight;
    uint32_t seed, blocky, block_px;
    float oe_scale;
    int32_t B, H, W;
    int32_t has_cam_indices;
    uint32_t wgs_per_row;
    uint32_t rgb_fmt, out_fmt, raw16_on;
    int32_t mask_h, mask_w;
    uint32_t _pad0;
};
static_assert(sizeof(AppParams) == 15 * 8 + 19 * 4 + 4, "layout");

// In appearance_chain.slang declaration order.
backend::vk::SpecList spec_list(const AppearanceChainParams& c) {
    const bool ppisp = c.ppisp != AppearancePpisp::Off;
    return backend::vk::SpecList{
        (uint32_t)c.bg,
        (uint32_t)c.ppisp,
        ppisp ? (uint32_t)c.ppisp_layout : 0u,
        ppisp && c.ppisp_clamp ? 1u : 0u,
        c.cs_enabled ? 1u : 0u,
        c.cs_enabled ? (uint32_t)c.cs_transfer : 0u,
        c.cs_enabled && c.cs_is_linear ? 1u : 0u,
        c.bg_noise ? (uint32_t)c.bg_transfer : 0u,
        c.bg_noise && c.bg_is_linear ? 1u : 0u,
        c.bg_noise && c.bg_exponent_by_cam ? 1u : 0u,
        c.bg_noise ? 1u : 0u,
        c.bg_noise && c.bg_mask ? 1u : 0u};
}

AppParams make_params(const AppearanceChainParams& c, const TorchTensorView& rgb,
                      const DeviceTensor3D<float>& transmittance) {
    AppParams p{};
    p.rgb = std::get<0>(rgb);
    p.rgb_fmt = (uint32_t)pixel_format(rgb);
    p.fwd_out = vkk::or_fallback(nullptr);
    p.raw16 = vkk::or_fallback(nullptr);
    p.transmittance = vkk::or_fallback(transmittance.data_ptr());
    p.bg_image = vkk::or_fallback(c.bg_image);
    p.v_bg_image = vkk::or_fallback(c.v_bg_image);
    p.exponent_by_cam = vkk::or_fallback(c.bg_exponent_by_cam);
    p.cam_indices = vkk::or_fallback(c.cam_indices);
    p.cs_matrix = vkk::or_fallback(c.cs_matrix);
    p.ppisp_params = vkk::or_fallback(c.ppisp_params);
    p.v_ppisp_params = vkk::or_fallback(c.v_ppisp_params);
    p.intrins = vkk::or_fallback(c.intrins);
    p.bg_mask = vkk::or_fallback(c.bg_mask);
    p.mask_h = c.bg_mask_h;
    p.mask_w = c.bg_mask_w;
    p.bg_r = c.bg_color[0];
    p.bg_g = c.bg_color[1];
    p.bg_b = c.bg_color[2];
    p.randomize_weight = c.bg_randomize_weight;
    p.seed = c.bg_seed;
    p.blocky = c.bg_blocky ? 1u : 0u;
    p.block_px = c.bg_block_px;
    const auto& s = std::get<2>(rgb);
    p.B = (int32_t)s[0];
    p.H = (int32_t)s[1];
    p.W = (int32_t)s[2];
    p.has_cam_indices = c.cam_indices ? 1 : 0;
    return p;
}

// The backward's workgroups stride over the image: with one per 256 pixels,
// thousands of them add to the same few ppisp parameters, and RDNA 4 executes
// same-address float atomics one at a time (~60 ns each).
constexpr int64_t kBwdGroupsPerImage = 256;

void dispatch_image(const char* entry, const AppearanceChainParams& c,
                    AppParams& p) {
    const int64_t pixels = (int64_t)p.H * p.W;
    if (pixels <= 0 || p.B <= 0) return;
    if (p.B > 65535)
        throw std::runtime_error("appearance: image grid dimension exceeds 65535");
    p.wgs_per_row = (uint32_t)std::min((pixels + 255) / 256, kBwdGroupsPerImage);
    vkk::dispatch_ring(entry, spec_list(c), p.wgs_per_row, (uint32_t)p.B, 1,
                       &p, sizeof(p));
}

}  // namespace

void appearance_chain_forward(const AppearanceChainParams& c,
                              TorchTensorView rgb,
                              DeviceTensor3D<float> transmittance,
                              TorchTensorView out_rgb, TorchTensorView raw16) {
    AppParams p = make_params(c, rgb, transmittance);
    p.out_rgb = vkk::or_fallback(nullptr);
    p.fwd_out = std::get<0>(out_rgb);
    p.out_fmt = (uint32_t)pixel_format(out_rgb);
    p.raw16_on = std::get<0>(raw16) ? 1u : 0u;
    if (p.raw16_on) p.raw16 = std::get<0>(raw16);
    p.v_transmittance = vkk::or_fallback(nullptr);
    // Flat over B*H*W, which rgb_store3's lane pairs need.
    vkk::dispatch_ring_flat("appearance_chain.appearance_fwd", spec_list(c),
                            (int64_t)p.B * p.H * p.W, 256, &p, sizeof(p),
                            &p.wgs_per_row);
}

void appearance_chain_backward(const AppearanceChainParams& c,
                               TorchTensorView rgb,
                               DeviceTensor3D<float> transmittance,
                               DeviceTensor3D<float3> v_rgb,
                               DeviceTensor3D<float> v_transmittance) {
    AppParams p = make_params(c, rgb, transmittance);
    p.out_rgb = (uint64_t)v_rgb.data_ptr();
    p.v_transmittance = vkk::or_fallback(v_transmittance.data_ptr());
    const double n = (double)p.B * p.H * p.W * 3.0;
    p.oe_scale = c.overexposure_weight == 0.0f
                     ? 0.0f
                     : (float)(2.0 * (double)c.overexposure_weight / n);
    dispatch_image("appearance_chain.appearance_bwd", c, p);
}
