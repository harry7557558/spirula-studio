#pragma once

#include "backend/api/BackendTypes.h"
#include <cstdint>

#include <core/Tensor.h>

#include "primitives/Primitive3DGS.cuh"
// #include "primitives/Primitive3DGUT.cuh"

#include "core/Common.cuh"


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



std::tuple<
    std::vector<DeviceTensorFloatND>, std::vector<DeviceTensorFloatND>,  // gradient
    DeviceVector<float>  // accum_weight
> rasterize_to_pixels_3dgs_bwd(
    // Gaussian parameters
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets, // [I, tile_height, tile_width]
    const DeviceVector<int32_t> flatten_ids,    // [n_isects]
    int macro_log2,               // binning granularity
    // forward outputs
    const DeviceTensor3D<float> render_Ts,  // [I, image_height, image_width]
    const DeviceTensor3D<int32_t> last_ids, // [I, image_height, image_width]
    RenderOutput::TensorTuple render_outputs_tuple,
    std::optional<RenderOutput::TensorTuple> distortion_fwd_outputs,  // forward D
    DistortionType dist_type,  // distortion channel set (None/D/RGB_D)
    DeviceTensor3D<float> accum_weight_map,  // [I, H, W]
    DensifyAccumMode accum_mode,
    // [num_splats] running peak of alpha * T, atomic-maxed times
    // max_weight_scale; empty to skip.
    DeviceVector<float> max_weight,
    float max_weight_scale,
    // Primitive3DGS::kernel_vis: > 0 cut and renormalized, 0 the full gaussian
    float kernel_cut,
    // gradients of outputs
    RenderOutput::TensorTuple v_render_outputs,
    const DeviceTensor3D<float> v_render_Ts,
    const DeviceTensor3D<float> v_median,  // [I, H, W], optional
    std::optional<RenderOutput::TensorTuple> v_distortion_outputs,
    float v_dist_rgb_per_depth,  // used when v_distortion_outputs has no rgb
    std::optional<std::vector<DeviceTensorFloatND>> v_splats_w,
    std::optional<std::vector<DeviceTensorFloatND>> v_splats_s
);
