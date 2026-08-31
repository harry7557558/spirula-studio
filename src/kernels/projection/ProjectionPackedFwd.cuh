#pragma once

#include "backend/api/BackendTypes.h"
#include <cstdint>

#include <core/Tensor.h>

#include <core/Common.cuh>

#include "primitives/Primitive3DGS.cuh"
#include "primitives/Primitive3DGUT.cuh"
#include "shaders/packed_mask.h"

#include <stdexcept>
#include <string>


// The compaction addresses its output with int32 (camera_ids, gaussian_ids and
// every downstream flat index are int32), so the pair count is the hard limit.
inline void packed_check_pair_count(uint32_t C, uint32_t N) {
    const int64_t pairs = (int64_t)C * (int64_t)N;
    if (pairs > (int64_t)INT32_MAX)
        throw std::runtime_error(
            "packed projection: " + std::to_string(C) + " cameras x " +
            std::to_string(N) + " gaussians exceeds the 2^31 pair limit; "
            "lower --cap-max or the sub-batch size");
}


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



std::tuple<
    DeviceVector<int32_t>, DeviceVector<int32_t>, DeviceVector<uint2>,
    DeviceVector<float>, std::vector<DeviceTensorFloatND>
> projection_3dgs_packed_forward(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &in_splats,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceVector<float> radii,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);


std::tuple<
    DeviceVector<int32_t>, DeviceVector<int32_t>, DeviceVector<uint2>,
    DeviceVector<float>, std::vector<DeviceTensorFloatND>
> projection_mip_packed_forward(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &in_splats,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceVector<float> radii,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);


std::tuple<
    DeviceVector<int32_t>, DeviceVector<int32_t>, DeviceVector<uint2>,
    DeviceVector<float>, std::vector<DeviceTensorFloatND>
> projection_3dgut_packed_forward(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &in_splats,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceVector<float> radii,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);
