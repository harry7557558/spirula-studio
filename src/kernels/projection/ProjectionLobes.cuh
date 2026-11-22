#pragma once

// CUDA only: the spherical-beta lobes the projection forward leaves out, added
// to its screen rows afterwards. Vulkan evaluates them inside the projection
// (view_color.slang); docs/notes/spherical-beta.md.

#include <cstdint>
#include <vector>

#include <core/Tensor.h>
#include "core/ColorBasis.h"

// The SH degree a row of num_sh float3 coefficients stores ahead of its lobes.
int stored_sh_degree(uint32_t num_sh, int lobes);

// Adds the lobes of every projected splat: screen row i is splat
// gaussian_ids[i] seen by camera camera_ids[i] (packed), else i % N by i / N.
void add_projected_sb_lobes(
    const ColorBasis& color, bool eval3d,
    const std::vector<DeviceTensorFloatND>& in_splats,
    const std::vector<DeviceTensorFloatND>& splats_screen,
    const uint2* aabb, int64_t total, int64_t N,
    const int32_t* camera_ids, const int32_t* gaussian_ids,
    const float* viewmats,
    const uint8_t* sh_value_packed, const float2* sh_value_bounds,
    uint32_t num_sh_buffer, int sh_value_bits, int64_t sh_bounds_stride);
