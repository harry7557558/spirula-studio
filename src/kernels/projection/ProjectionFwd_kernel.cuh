#include <cuda_runtime.h>
#include <cstdint>

#include <core/Common.cuh>

#ifndef NO_TORCH
#define NO_TORCH
#endif

#include <cooperative_groups.h>
namespace cg = cooperative_groups;

#include "core/AabbQuant.cuh"


template<typename SplatPrimitive, CameraModelType camera_model,
         CameraDistortionType distortion, int VALUE_BITS = 32>
__global__ void projection_fused_fwd_kernel(
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // outputs
    uint2 *__restrict__ aabbs,          // [C, N] packed, core/AabbQuant.cuh
    float *__restrict__ sorting_depths,  // [C, N, 1]
    float *__restrict__ radii,  // [N, 1]
    typename SplatPrimitive::ScreenBuffer splats_screen,
    // SH VALUE-quant (active when VALUE_BITS != 32). Packed bytes + per-block
    // (min, max) bounds. The cell offset within a splat is 3 * num_sh_buffer
    // (the buffer's max SH count, NOT the runtime SH degree). `sh_bounds_stride`
    // is the number of CELLS covered by one float2 bound -- 256 for the
    // non-FPBO per-cell-block layout, or 256 * 3 * num_sh_buffer for the FPBO
    // per-splat-block layout (256 splats per block, all 3K cells/splat).
    // 0 (legacy default) is treated as per-splat-block for back-compat with FPBO.
    const uint8_t* __restrict__ sh_value_packed,
    const float2* __restrict__ sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int64_t  sh_bounds_stride
) {
    // parallelize over C * N.
    uint32_t idx = cg::this_grid().thread_rank();
    if (idx >= C * N) {
        return;
    }
    const uint32_t cid = (idx / N) % C; // camera id
    const uint32_t gid = idx % N; // gaussian id

    // Load camera
    viewmats += cid * 16;
    float4 intrin = intrins[cid];
    float3x3 R = {
        viewmats[0], viewmats[1], viewmats[2],  // 1st row
        viewmats[4], viewmats[5], viewmats[6],  // 2nd row
        viewmats[8], viewmats[9], viewmats[10],  // 3rd row
    };
    float3 t = { viewmats[3], viewmats[7], viewmats[11] };
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    ProjCameraT<distortion> cam = {
        R, t, fx, fy, cx, cy,
        image_width, image_height,
    };
    cam.dist_coeffs = dist_coeffs_buffer.load<distortion>(cid);

    // Load splat
    typename SplatPrimitive::World splat_world;
    splat_world.load(splats_world, gid);

    // Projection. When VALUE_BITS != 32, pass the codec args to project() so
    // the SH eval inside reads from the packed bytes instead of fp32. base
    // = gid * 3 * num_sh_buffer matches the layout used by the encode side
    // (engine().world.features_sh_quant{8,16}{,_fpbo}). bounds_stride selects
    // the bounds layout: 256 for per-cell-block, 256*3*num_sh_buffer for the
    // FPBO per-splat-block layout (which is what `_ensure_optim_state`
    // allocates when use_fused_proj_bwd_optim is on).
    float sorting_depth;
    float4 aabb;
    float radius = 0.0f;
    typename SplatPrimitive::Screen splat_screen;
    if constexpr (VALUE_BITS == 32) {
        splat_world.template project<camera_model, distortion, 32>(
            cam, splat_screen, aabb, sorting_depth, radius);
    } else {
        const ShQuantAddr sha = sh_quant_addr(num_sh_buffer, sh_bounds_stride);
        const int64_t sh_base = sha.base(gid);
        splat_world.template project<camera_model, distortion, VALUE_BITS>(
            cam, splat_screen, aabb, sorting_depth, radius,
            const_cast<uint8_t*>(sh_value_packed),
            const_cast<float2*>(sh_value_bounds),
            sh_base, sha.bounds_stride, sha.pair_pitch);
    }

    // Save results
    aabb.x = fminf(fmaxf(aabb.x, 0.0f), image_width-1.0f);
    aabb.y = fminf(fmaxf(aabb.y, 0.0f), image_height-1.0f);
    aabb.z = fminf(fmaxf(aabb.z, 0.0f), image_width-1.0f);
    aabb.w = fminf(fmaxf(aabb.w, 0.0f), image_height-1.0f);
    if (aabb.z - aabb.x > 1e-3f && aabb.w - aabb.y > 1e-3f) {
        splat_screen.store(splats_screen, idx);
        aabbs[idx] = aabb16_encode(aabb, image_width, image_height);
        sorting_depths[idx] = sorting_depth;
        atomicMax(&radii[idx%N], radius);
    } else {
        aabbs[idx] = make_uint2(0u, 0u);
        sorting_depths[idx] = 0.0f;
    }
}


template<typename SplatPrimitive, CameraModelType camera_model,
         CameraDistortionType distortion>
void projection_fused_fwd_kernel_wrapper(
    cudaStream_t stream,
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // outputs
    uint2 *__restrict__ aabbs,          // [C, N] packed, core/AabbQuant.cuh
    float *__restrict__ sorting_depths,  // [C, N, 1]
    float *__restrict__ radii,  // [N, 1]
    typename SplatPrimitive::ScreenBuffer splats_screen,
    // SH VALUE-quant (active when sh_value_bits != 32). Runtime dispatched
    // below to the matching VALUE_BITS template instantiation.
    const uint8_t* __restrict__ sh_value_packed,
    const float2* __restrict__ sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    // 0 -> FPBO per-splat-block layout (256 * 3 * num_sh_buffer cells/bound);
    // 256 -> non-FPBO per-cell-block layout. See projection_fused_fwd_kernel.
    const int64_t sh_bounds_stride
) {
    constexpr uint block = 128;
    #define _LAUNCH(VB) \
        projection_fused_fwd_kernel<SplatPrimitive, camera_model, distortion, VB> \
        <<<_CEIL_DIV(C*N, block), block, 0, stream>>>( \
            C, N, \
            splats_world, viewmats, intrins, dist_coeffs_buffer, \
            image_width, image_height, \
            aabbs, sorting_depths, radii, splats_screen, \
            sh_value_packed, sh_value_bounds, num_sh_buffer, sh_bounds_stride)
    if      (sh_value_bits == 8)  { _LAUNCH(8); }
    else if (sh_value_bits == 16) { _LAUNCH(16); }
    else                          { _LAUNCH(32); }
    #undef _LAUNCH
}
