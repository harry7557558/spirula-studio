#pragma once

#include "backend/api/BackendTypes.h"
#include <cstdint>

#include <core/Tensor.h>

#include <core/Common.cuh>

#include "primitives/Primitive3DGS.cuh"
#include "primitives/Primitive3DGUT.cuh"


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void projection_3dgs_backward(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &splats_world,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> &v_splats_screen,
    // returns
    const std::vector<DeviceTensorFloatND> &v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant. Nulls + sh_value_bits=32 leaves the bwd on the fp32
    // SH path (callers without value-quant active pass std::nullopt + 32).
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);


void projection_mip_backward(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &splats_world,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> &v_splats_screen,
    // returns
    const std::vector<DeviceTensorFloatND> &v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant. Nulls + sh_value_bits=32 leaves the bwd on the fp32
    // SH path (callers without value-quant active pass std::nullopt + 32).
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);


void projection_3dgut_backward(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &splats_world,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> &v_splats_screen,
    // returns
    const std::vector<DeviceTensorFloatND> &v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant. Nulls + sh_value_bits=32 leaves the bwd on the fp32
    // SH path (callers without value-quant active pass std::nullopt + 32).
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);
