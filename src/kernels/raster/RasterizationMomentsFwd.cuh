#pragma once

/*
 * RasterizationMomentsFwd.cuh
 *
 * Occupancy-moment rasterization for the meshing pipeline. Mimics the 3DGUT
 * forward rasterizer (RasterizationEval3DFwd) over the SAME projected
 * splats_w/splats_s + tile intersection, but instead of compositing color it
 * accumulates, per pixel, the first three moments of the alpha*T weight over
 * the per-Gaussian ray depth:
 *
 *     m0 = sum_i alpha_i T_i           ( = 1 - T_final, the accumulated alpha )
 *     mean = (1/m0) sum_i alpha_i T_i z_i
 *     var  = (1/m0) sum_i alpha_i T_i z_i^2 - mean^2
 *
 * where z_i is the Gaussian's ray depth (FragmentFwd::evaluate_color().depth).
 * The occupancy 1 - T(z) at depth z is then modeled as m0 * Phi((z-mean)/std).
 *
 * Only 3DGUT (Vanilla3DGUT<0>) with no distortion output is instantiated, over
 * the compiled (camera model, distortion tier) pairs -- all in this translation
 * unit, so no generated ins/ entry is needed.
 */

#include <cstdint>
#include <string>
#include <vector>

#include <core/Tensor.h>

// Per-pixel moments [I*image_height*image_width], packed as
// (m0, mean_depth, std_depth). Caller allocates the device buffer.
void rasterize_moments_3dgut_fwd(
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    TorchTensorView viewmats,            // [..., C, 4, 4]
    TorchTensorView intrins,             // [..., C, 4], fx, fy, cx, cy
    const std::string& camera_model,
    const std::string& distortion,
    TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,          // [..., N] packed AABB
    uint32_t image_width,
    uint32_t image_height,
    const DeviceTensor3D<int32_t>& tile_offsets,  // [I, tile_h, tile_w]
    const DeviceVector<int32_t>& flatten_ids,     // [n_isects]
    float3* render_moments,              // [I*H*W] device, (m0, z_lo, z_hi)
    float3* render_rgb = nullptr         // [I*H*W] device full-ray DC color, or null
);
