#pragma once

#include "backend/api/BackendTypes.h"
#include <cstdint>

#include <core/Tensor.h>
#include <core/Common.cuh>

// #include "primitives/Primitive3DGS.cuh"
#include "primitives/Primitive3DGUT.cuh"


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



std::tuple<
    std::vector<DeviceTensorFloatND>, std::vector<DeviceTensorFloatND>,  // gradient
    DeviceTensor2D<float>,  // v_viewmats
    DeviceVector<float>  // accum_weight
> rasterize_to_pixels_3dgut_bwd(
    // Gaussian parameters
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    TorchTensorView viewmats,  // [..., C, 4, 4]
    TorchTensorView intrins,  // [..., C, 4], fx, fy, cx, cy
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,  // [..., N] projected 2D AABB, for sub-tile culling
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets, // [I, tile_height, tile_width]
    const DeviceVector<int32_t> flatten_ids,    // [n_isects]
    // forward outputs
    const DeviceTensor3D<float> render_Ts,  // [I, image_height, image_width]
    const DeviceTensor3D<int32_t> last_ids, // [I, image_height, image_width]
    RenderOutput::TensorTuple render_outputs,
    std::optional<RenderOutput::TensorTuple> distortion_fwd_outputs,
    DistortionType dist_type,  // distortion channel set (None/D/RGB_D; DN/RGB_DN need a normal primitive)
    DeviceTensor3D<float> loss_map,  // [..., image_height, image_width, 1]
    DeviceTensor3D<float> accum_weight_map,  // [I, H, W]
    DensifyAccumMode accum_mode,
    // gradients of outputs
    RenderOutput::TensorTuple v_render_outputs,
    const DeviceTensor3D<float> v_render_Ts, // [..., image_height, image_width, 1]
    const DeviceTensor3D<float> v_median, // [..., image_height, image_width], optional
    std::optional<RenderOutput::TensorTuple> v_distortion_outputs,
    std::optional<std::vector<DeviceTensorFloatND>> v_splats_w,
    std::optional<std::vector<DeviceTensorFloatND>> v_splats_s,
    bool need_viewmat_grad
);
