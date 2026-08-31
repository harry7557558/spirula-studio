#include <cuda_runtime.h>
#include <cstdint>

#include <core/Common.cuh>

#include "shaders/packed_mask.h"

#ifndef NO_TORCH
#define NO_TORCH
#endif


#include <cooperative_groups.h>
namespace cg = cooperative_groups;



template<typename SplatPrimitive, CameraModelType camera_model,
         CameraDistortionType distortion, int VALUE_BITS = 32>
__global__ void projection_packed_mask_kernel(
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,  // [N, ...]
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // outputs
    uint32_t *__restrict__ mask_bits,     // [gridDim.x * SS_PMASK_WORDS]
    int32_t *__restrict__ block_counts,   // [gridDim.x]
    // SH VALUE-quant. The mask kernel discards splat_screen.rgb but still
    // calls project() which evaluates SH internally; passing the codec args
    // keeps the SH eval well-defined when fp32 features_sh is stale.
    const uint8_t* __restrict__ sh_value_packed = nullptr,
    const float2* __restrict__ sh_value_bounds  = nullptr,
    const uint32_t num_sh_buffer = 0,
    const int64_t  sh_bounds_stride = 0
) {
    // parallelize over C * N.
    __shared__ uint32_t sh_bits[SS_PMASK_WORDS];
    const uint32_t tid = threadIdx.x;
    if (tid < SS_PMASK_WORDS) sh_bits[tid] = 0u;
    __syncthreads();

    const uint32_t idx = blockIdx.x * SS_PMASK_BLOCK + tid;
    if (idx < C * N) {
        // GAUSSIAN-major, so the nnz list the prefix scan compacts comes out
        // sorted by gaussian id and the backward passes need no sort of their own
        // (see the note on projection_*_packed_forward).
        const uint32_t gid = idx / C; // gaussian id
        const uint32_t cid = idx % C; // camera id

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
        // TODO: verify that SH is not loaded after compiler optimization
        typename SplatPrimitive::World splat_world;
        splat_world.load(splats_world, gid);

        // Projection
        float sorting_depth;
        float4 aabb;
        float radius;
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
        if (aabb.z - aabb.x > 1e-3f && aabb.w - aabb.y > 1e-3f)
            atomicOr(&sh_bits[tid >> 5], 1u << (tid & 31));
    }
    __syncthreads();

    if (tid < SS_PMASK_WORDS)
        mask_bits[blockIdx.x * SS_PMASK_WORDS + tid] = sh_bits[tid];
    if (tid == 0) {
        int32_t n = 0;
        for (int k = 0; k < SS_PMASK_WORDS; ++k) n += __popc(sh_bits[k]);
        block_counts[blockIdx.x] = n;
    }
}


template<typename SplatPrimitive, CameraModelType camera_model,
         CameraDistortionType distortion, int VALUE_BITS = 32>
__global__ void projection_packed_fwd_kernel(
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,  // [N, ...]
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t* __restrict__ mask_bits,   // [gridDim.x * SS_PMASK_WORDS]
    const int32_t* __restrict__ block_scan,   // [gridDim.x], inclusive
    // outputs
    int32_t *__restrict__ camera_ids,    // [nnz]
    int32_t *__restrict__ gaussian_ids,  // [nnz]
    float4 *__restrict__ aabbs,         // [nnz, 4]
    float *__restrict__ sorting_depths,  // [nnz]
    float *__restrict__ radii,  // [N]
    typename SplatPrimitive::ScreenBuffer splats_screen,  // [nnz, ...]
    const uint8_t* __restrict__ sh_value_packed = nullptr,
    const float2* __restrict__ sh_value_bounds  = nullptr,
    const uint32_t num_sh_buffer = 0,
    const int64_t  sh_bounds_stride = 0
) {
    // parallelize over C * N.
    __shared__ uint32_t sh_bits[SS_PMASK_WORDS];
    const uint32_t tid = threadIdx.x;
    if (tid < SS_PMASK_WORDS)
        sh_bits[tid] = mask_bits[blockIdx.x * SS_PMASK_WORDS + tid];
    __syncthreads();

    const uint32_t word = sh_bits[tid >> 5];
    if (!((word >> (tid & 31)) & 1u))
        return;

    // Output slot = scanned count of the blocks before this one + the bits set
    // below this thread inside it. Out-of-range lanes never set a bit, so the
    // range test above is implicit.
    int32_t out_idx = blockIdx.x == 0 ? 0 : block_scan[blockIdx.x - 1];
    for (uint32_t k = 0; k < (tid >> 5); ++k)
        out_idx += __popc(sh_bits[k]);
    out_idx += __popc(word & ((1u << (tid & 31)) - 1u));

    const uint32_t idx = blockIdx.x * SS_PMASK_BLOCK + tid;
    const uint32_t gid = idx / C; // gaussian id (see the mask kernel)
    const uint32_t cid = idx % C; // camera id

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

    // Projection
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
    camera_ids[out_idx] = (int32_t)cid;
    gaussian_ids[out_idx] = (int32_t)gid;
    aabb.x = fminf(fmaxf(aabb.x, 0.0f), image_width-1.0f);
    aabb.y = fminf(fmaxf(aabb.y, 0.0f), image_height-1.0f);
    aabb.z = fminf(fmaxf(aabb.z, 0.0f), image_width-1.0f);
    aabb.w = fminf(fmaxf(aabb.w, 0.0f), image_height-1.0f);
    aabbs[out_idx] = aabb;
    sorting_depths[out_idx] = sorting_depth;
    splat_screen.store(splats_screen, out_idx);
    atomicMax(&radii[gid], radius);
}


template<typename SplatPrimitive, CameraModelType camera_model,
         CameraDistortionType distortion>
void projection_packed_mask_kernel_wrapper(
    cudaStream_t stream,
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,  // [N, ...]
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // outputs
    uint32_t *__restrict__ mask_bits,
    int32_t *__restrict__ block_counts,
    const uint8_t* __restrict__ sh_value_packed,
    const float2* __restrict__ sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    constexpr uint block = SS_PMASK_BLOCK;
    #define _LAUNCH(VB) \
        projection_packed_mask_kernel<SplatPrimitive, camera_model, distortion, VB> \
        <<<_CEIL_DIV(C*N, block), block, 0, stream>>>( \
            C, N, \
            splats_world, viewmats, intrins, dist_coeffs_buffer, \
            image_width, image_height, \
            mask_bits, block_counts, \
            sh_value_packed, sh_value_bounds, num_sh_buffer, sh_bounds_stride)
    if      (sh_value_bits == 8)  { _LAUNCH(8); }
    else if (sh_value_bits == 16) { _LAUNCH(16); }
    else                          { _LAUNCH(32); }
    #undef _LAUNCH
}

template<typename SplatPrimitive, CameraModelType camera_model,
         CameraDistortionType distortion>
void projection_packed_fwd_kernel_wrapper(
    cudaStream_t stream,
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,  // [N, ...]
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t* __restrict__ mask_bits,
    const int32_t* __restrict__ block_scan,
    // outputs
    int32_t *__restrict__ camera_ids,    // [nnz]
    int32_t *__restrict__ gaussian_ids,  // [nnz]
    float4 *__restrict__ aabbs,         // [nnz, 4]
    float *__restrict__ sorting_depths,         // [nnz]
    float *__restrict__ radii,  // [N]
    typename SplatPrimitive::ScreenBuffer splats_screen,  // [nnz, ...]
    const uint8_t* __restrict__ sh_value_packed,
    const float2* __restrict__ sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    constexpr uint block = SS_PMASK_BLOCK;
    #define _LAUNCH(VB) \
        projection_packed_fwd_kernel<SplatPrimitive, camera_model, distortion, VB> \
        <<<_CEIL_DIV(C*N, block), block, 0, stream>>>( \
            C, N, \
            splats_world, viewmats, intrins, dist_coeffs_buffer, \
            image_width, image_height, \
            mask_bits, block_scan, \
            camera_ids, gaussian_ids, aabbs, sorting_depths, radii, splats_screen, \
            sh_value_packed, sh_value_bounds, num_sh_buffer, sh_bounds_stride)
    if      (sh_value_bits == 8)  { _LAUNCH(8); }
    else if (sh_value_bits == 16) { _LAUNCH(16); }
    else                          { _LAUNCH(32); }
    #undef _LAUNCH
}
