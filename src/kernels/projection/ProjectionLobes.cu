#include "kernels/projection/ProjectionLobes.cuh"

#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

#include <core/Common.cuh>
#include "shaders/screen_layout.h"

#include "generated/slang.cuh"
namespace SlangHarmonics {
#include "generated/set_namespace.cuh"
#include "generated/harmonics.cuh"
}

namespace {

template<int VALUE_BITS>
__global__ void add_sb_lobes_kernel(
    int64_t total, int64_t N, int lobes, int sb_base,
    const int32_t* __restrict__ camera_ids, const int32_t* __restrict__ gaussian_ids,
    const float* __restrict__ viewmats, const float3* __restrict__ means,
    const float* __restrict__ features_sh, int64_t row_floats,
    const uint2* __restrict__ aabb, float* __restrict__ screen, int stride, int rgb,
    const uint8_t* sh_packed, const float2* sh_bounds, uint32_t num_sh_buffer,
    int64_t sh_bounds_stride
) {
    const int64_t idx = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total) return;
    // The projection culled it and stored no row.
    const uint2 box = aabb[idx];
    if (box.x == 0u && box.y == 0u) return;
    const int64_t cid = camera_ids ? camera_ids[idx] : idx / N;
    const int64_t gid = gaussian_ids ? gaussian_ids[idx] : idx % N;
    const float* vm = viewmats + 16 * cid;
    const float3x3 R = {vm[0], vm[1], vm[2], vm[4], vm[5], vm[6], vm[8], vm[9], vm[10]};
    const float3 t = {vm[3], vm[7], vm[11]};
    float3 c;
    if constexpr (VALUE_BITS == 32) {
        c = SlangHarmonics::sb_lobes_to_color(lobes, sb_base, means[gid], R, t,
                                              (float3*)(features_sh + gid * row_floats));
    } else {
        const ShQuantAddr sha = sh_quant_addr(num_sh_buffer, sh_bounds_stride);
        if constexpr (VALUE_BITS == 8)
            c = SlangHarmonics::sb_lobes_to_color_q8(lobes, sb_base, means[gid], R, t,
                (uint8_t*)sh_packed, (float2*)sh_bounds, sha.base(gid), sha.bounds_stride, sha.pair_pitch);
        else
            c = SlangHarmonics::sb_lobes_to_color_q16(lobes, sb_base, means[gid], R, t,
                (uint16_t*)sh_packed, (float2*)sh_bounds, sha.base(gid), sha.bounds_stride, sha.pair_pitch);
    }
    float* r = screen + idx * stride + rgb;
    r[0] += c.x;
    r[1] += c.y;
    r[2] += c.z;
}

}  // namespace

int stored_sh_degree(uint32_t num_sh, int lobes) {
    const int64_t sh = (int64_t)num_sh - ColorBasis::kSbCoeffsPerLobe * lobes;
    for (int d = 0; d <= ColorBasis::kMaxShDegree; d++)
        if (ColorBasis::sh_coeffs_of(d) == sh) return d;
    throw std::runtime_error("a colour row of " + std::to_string(num_sh) +
                             " coefficients holds no SH degree ahead of " +
                             std::to_string(lobes) + " lobes");
}

void add_projected_sb_lobes(
    const ColorBasis& color, bool eval3d,
    const std::vector<DeviceTensorFloatND>& in_splats,
    const std::vector<DeviceTensorFloatND>& splats_screen,
    const uint2* aabb, int64_t total, int64_t N,
    const int32_t* camera_ids, const int32_t* gaussian_ids,
    const float* viewmats,
    const uint8_t* sh_value_packed, const float2* sh_value_bounds,
    uint32_t num_sh_buffer, int sh_value_bits, int64_t sh_bounds_stride
) {
    if (color.sb_lobes <= 0 || total <= 0) return;
    const int64_t row_floats = 3 * (int64_t)num_sh_buffer;
    const int sb_base = (int)num_sh_buffer - color.sb_coeffs();
    const int stride = eval3d ? SCRG_STRIDE : SCR2_STRIDE;
    const int rgb = eval3d ? SCRG_RGB : SCR2_RGB;
    constexpr int block = 256;
    #define _LAUNCH(VB) add_sb_lobes_kernel<VB><<<_CEIL_DIV(total, block), block>>>( \
        total, N, color.sb_lobes, sb_base, camera_ids, gaussian_ids, viewmats, \
        (const float3*)in_splats[0].data_ptr(), in_splats[5].data_ptr(), row_floats, aabb, \
        splats_screen[0].data_ptr(), stride, rgb, sh_value_packed, sh_value_bounds, \
        num_sh_buffer, sh_bounds_stride)
    if (sh_value_bits == 8) _LAUNCH(8);
    else if (sh_value_bits == 16) _LAUNCH(16);
    else _LAUNCH(32);
    #undef _LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
