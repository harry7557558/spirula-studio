#pragma once

// Stochastic (sort-free) rasterization for the viewer: one Monte Carlo frame of
// the sorted composite, summed into a running average. docs/notes/stochastic-splats.md.

#include "backend/api/BackendTypes.h"
#include <cstdint>
#include <string>

#include <core/Tensor.h>

#include "primitives/Primitive.cuh"

#include "core/Common.cuh"

inline constexpr uint32_t kStochasticMaxSpp = 8;
// Floats a pixel keeps over the frames: weighted rgb, depth, camera normal, the
// weight, and the share of samples that kept a splat. kStochasticSum in the Slang.
inline constexpr int kStochasticSum = 9;

struct StochasticFrame {
    float* sum = nullptr;  // [I, H, W, kStochasticSum], summed over frames
    uint32_t frame = 0;    // frames already in `sum`; 0 starts it over
    uint32_t spp = 4;      // samples a pixel this frame, 1..kStochasticMaxSpp
    bool winner = false;   // the paper's estimator: a sample shows its nearest accepted splat
    // Each tile's list is in sort-key order: a walk ends at a splat behind every sample.
    bool depth_sorted = true;
    // A pixel's samples split one draw a splat (u + s/spp) rather than drawing S.
    bool stratified = true;
};


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



std::tuple<
    RenderOutput::TensorTuple,  // renders: rgb, depth, normal (3dgut on request, 2dgs)
    DeviceTensor3D<float>       // transmittances
> rasterize_stochastic_fwd(
    int primitive,            // 0: 3dgs / mip, 1: 3dgut, 2: 2dgs
    bool output_normal,       // 3dgut renders its normal too
    bool output_depth,
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    DeviceTensorFloatND depths,       // what the tile intersector sorted by
    TorchTensorView viewmats,         // [I, 4, 4]
    TorchTensorView intrins,          // [I, 4]
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,
    StochasticFrame frame,
    float kernel_cut
);
