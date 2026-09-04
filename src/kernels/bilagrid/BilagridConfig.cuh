#pragma once

// Ported from fused-bilagrid/fused_bilagrid/config.h
// Self-contained to avoid pulling in common_utils.cuh, which would conflict
// with vector ops/dot/cross defined in BilagridPpispMath.cuh.

// RGB to gray constants
#define kC2G_r 0.299f
#define kC2G_g 0.587f
#define kC2G_b 0.114f

#ifdef __CUDACC__

#include <stdio.h>
#include <cuda_runtime.h>
#include "core/SourcePath.h"

// Function-call-style macro (different from the original statement-style),
// matching the form used by the rest of core/Common.cuh.
#ifndef CHECK_DEVICE_ERROR
#define CHECK_DEVICE_ERROR(call)                                    \
do {                                                                \
    cudaError_t err = call;                                         \
    if (err != cudaSuccess) {                                       \
        fprintf(stderr, "\033[41mCUDA Error at %s:%d: %s\033[m\n",  \
                SS_FILE, __LINE__, cudaGetErrorString(err));        \
        exit(EXIT_FAILURE);                                         \
    }                                                               \
} while (0)
#endif

#include "kernels/bilagrid/BilagridReader.cuh"

// Block dimensions baked into every bilagrid *_sample_backward_v1 kernel
// (affine RGB, PPISP, log-linear, depth, normal). Threaded as compile-time
// constants so blockDim.x/y/z folds inside the kernels (saves register
// pressure on x/y/z index math and lets `mult_x % BX == 0` style checks
// disappear at compile time when call sites pick block-multiple tile sizes).
// Host launchers use these to set `dim3 block`; do NOT change one without
// the other. 8x8x1 = 64 threads per block matches `sharedData[64]` reduction
// buffer in the bilagrid-grad kernels.
constexpr unsigned kBilagridBwdV1BlockX = 8;
constexpr unsigned kBilagridBwdV1BlockY = 8;
constexpr unsigned kBilagridBwdV1BlockZ = 1;

// 1D block dim for the companion *_v1_kernel_rgb (image-side rgb-grad) pass
// in the same 5 v1 variants. Launcher and kernel must agree.
constexpr unsigned kBilagridBwdV1RgbThreads = 256;


// ===========================================================================
// Grid folding for the whole-table kernels (TV loss + channel mean, fwd/bwd)
//
// These four kernels sweep the entire [N, L, H, W, C] table, so their block
// count along the fused (ni, li) axis is N*L/4 -- 80000 blocks for a 40k-image
// dataset at L=8, past the 65535 cap that CUDA puts on gridDim.y/z and that
// Vulkan puts on *every* dispatch dimension. So the (ni, li) block count is
// folded across two grid dimensions (x = column, y = row) and the two small
// spatial block counts share the third (z = h_block * w_blocks + w_block).
// The launcher passes `nl_per_row` so the kernel can undo the fold; the
// Vulkan launcher + bilagrid_tv.slang use the identical decomposition.
// ===========================================================================

// Per-axis block extent of the 4x4x4 blocks these kernels launch with.
constexpr int kBilagridTvBlock = 4;

struct BilagridTvGrid {
    unsigned gx, gy, gz;
    int nl_per_row;     // (ni, li) blocks per grid row; pass to the kernel
};

inline BilagridTvGrid bilagrid_tv_grid(int N, int L, int H, int W) {
    constexpr int kB = kBilagridTvBlock;
    const int64_t nl_blocks = ((int64_t)N * L + kB - 1) / kB;
    int64_t per_row = nl_blocks < 1 ? 1 : nl_blocks;
    if (per_row > 65535) per_row = 65535;
    const int64_t rows = (nl_blocks + per_row - 1) / per_row;
    const int64_t wb = (W + kB - 1) / kB;
    const int64_t hb = (H + kB - 1) / kB;
    BilagridTvGrid g;
    g.gx = (unsigned)per_row;
    g.gy = (unsigned)rows;
    g.gz = (unsigned)(wb * hb);
    g.nl_per_row = (int)per_row;
    return g;
}

// Undo the fold: recover the (ni, li), wi and hi block indices. `W` is the
// grid width, from which the w-block count is re-derived.
__device__ __forceinline__ void bilagrid_tv_block_ids(
    int nl_per_row, int W, int& nl_block, int& w_block, int& h_block
) {
    nl_block = (int)blockIdx.y * nl_per_row + (int)blockIdx.x;
    const int wb = (W + kBilagridTvBlock - 1) / kBilagridTvBlock;
    w_block = (int)blockIdx.z % wb;
    h_block = (int)blockIdx.z / wb;
}


// Block-wide reduce + single global atomicAdd per call, used by the 5
// bilagrid sample backward v1 kernels (affine RGB, PPISP, log-linear, depth,
// normal) for their channel-loop writeback. Functionally equivalent to
// `atomicAddFVec<64>` from Common.cuh but declared here so the v1 bwd kernel
// headers can avoid pulling in Common.cuh -- which provides global vector
// `dot`/`operator*` overloads that collide with bilagrid_ppisp::dot inside
// BilagridPpispMath{,Bwd}.cuh.
//
// Hardcoded for the 8x8x1 = 64-thread (2-warp) block geometry used by all 5
// v1 bwd kernels (see kBilagridBwdV1Block{X,Y,Z} in BilagridConfig.cuh).
//
// Caller requirements:
//   - threadblock is exactly 2 warps (8x8x1, 64 threads).
//   - All threads must call together.
//   - A __syncthreads() must separate successive calls within the same
//     kernel invocation: the static __shared__ scratch is reused, and
//     warp 0's reads of scratch[1] from the previous call can race with
//     warp 1's writes of scratch[1] for the next call otherwise.

#include <cuda_runtime.h>

inline __device__ void bilagrid_bwd_v1_block_atomic_add(
    float* addr, float val
) {
    constexpr unsigned kBlockSize = kBilagridBwdV1BlockX * kBilagridBwdV1BlockY * kBilagridBwdV1BlockZ;
    constexpr unsigned kWarpSize = 32;

    val = isfinite(val) ? val : 0.0f;

    // Intra-warp shuffle reduce. Each warp ends with its sum in lane 0.
    #pragma unroll
    for (int stride = kWarpSize >> 1; stride > 0; stride >>= 1) {
        val += __shfl_down_sync(0xFFFFFFFFu, val, stride);
    }

    // Lane 0 of each warp hands off its partial sum to a 2-slot scratch.
    static __shared__ float scratch[kBlockSize / kWarpSize];

    uint32_t lane_id;
    asm("mov.u32 %0, %%laneid;" : "=r"(lane_id));
    // 2D block, linear order: warpId = linear_tid / 32. blockDim is the
    // launch-time (constexpr) 8x8x1, so this collapses to a constant 0 or 1.
    const uint32_t linear_tid =
        (threadIdx.z * kBilagridBwdV1BlockY + threadIdx.y) * kBilagridBwdV1BlockX + threadIdx.x;
    const uint32_t warp_id = linear_tid / kWarpSize;

    if (lane_id == 0u) scratch[warp_id] = val;
    __syncthreads();

    // Warp 0 reduces the 2 partials and emits a single global atomicAdd.
    if (warp_id == 0u) {
        float sum = (lane_id < (kBlockSize / kWarpSize)) ? scratch[lane_id] : 0.0f;
        sum += __shfl_down_sync(0xFFFFFFFFu, sum, 1);
        if (lane_id == 0u && sum != 0.0f) atomicAdd(addr, sum);
    }
}

#endif  // __CUDACC__
