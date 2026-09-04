#pragma once

#include "backend/api/BackendTypes.h"
#include <cstdint>

#include <core/Tensor.h>

#include "primitives/Primitive3DGS.cuh"
// #include "primitives/Primitive3DGUT.cuh"

#include "core/Common.cuh"


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_3dgs_fwd(
    // Gaussian parameters
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,               // binning granularity
    DistortionType dist_type,
    bool output_median
);


std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_mip_fwd(
    // Gaussian parameters
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,               // binning granularity
    DistortionType dist_type,
    bool output_median
);
