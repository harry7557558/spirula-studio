#pragma once

#include "backend/api/BackendTypes.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <core/Tensor.h>
#include <core/Common.cuh>


// Per-attribute block-wise quantized GRADIENT accumulator pointers (non-FPBO
// grad-quant path). Each attribute has a packed byte buffer + a per-splat-block
// (min,max) float2 bounds table. A null `*_packed` routes that attribute away
// from the quantized path -- either it stays fp32 (3dgut means/quats/scales,
// which take rasterization-backward atomicAdds and are written to the fp32
// world grad buffer) or it is unused. Passed by value into the kernel like
// NonShQuantState.
struct GradQuantBuffers {
    uint8_t* means_packed  = nullptr;  float2* means_bounds  = nullptr;  // 16-bit, 3 cells/splat
    uint8_t* quats_packed  = nullptr;  float2* quats_bounds  = nullptr;  // 16-bit, 4 cells/splat
    uint8_t* scales_packed = nullptr;  float2* scales_bounds = nullptr;  // 16-bit, 3 cells/splat
    uint8_t* opac_packed   = nullptr;  float2* opac_bounds   = nullptr;  // 16-bit, 1 cell/splat
    uint8_t* dc_packed     = nullptr;  float2* dc_bounds     = nullptr;  // 16-bit, 3 cells/splat
    uint8_t* sh_packed     = nullptr;  float2* sh_bounds     = nullptr;  // 8-bit, 3*K cells/splat
};


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void projection_3dgs_backward_quantgrad(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    const DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    GradQuantBuffers gq,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride
);


void projection_mip_backward_quantgrad(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    const DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    GradQuantBuffers gq,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride
);


void projection_3dgut_backward_quantgrad(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    const DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    GradQuantBuffers gq,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride
);
