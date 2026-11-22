#pragma once

/*
 * RasterizationMomentsFwd.cuh
 *
 * Occupancy-moment rasterization for meshing: the 3DGUT forward rasterizer's
 * walk over the same projected splats, writing per pixel (m0, c0, c1) -- the
 * accumulated alpha and a line c0 + c1 z for sqrt(-log T) at ray depth z,
 * fitted to the middle half of the light the ray loses: through its mean
 * depth, with the slope its spread gives (docs/notes/mesh-field.md). Only
 * Vanilla3DGUT<0>, over the compiled (camera model, distortion tier) pairs.
 */

#include <cstdint>
#include <string>
#include <vector>

#include <core/Tensor.h>

// Per-pixel moments [I*image_height*image_width] as (m0, c0, c1). Caller
// allocates the device buffer.
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
    int macro_log2,                      // binning granularity, Common.cuh
    float3* render_moments,              // [I*H*W] device, (m0, z_lo, z_hi)
    float3* render_rgb = nullptr         // [I*H*W] device full-ray DC color, or null
);
