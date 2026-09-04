#pragma once

// GradQuant -- block-wise SIGNED SYMMETRIC quantization helpers for the
// per-splat world GRADIENT accumulator (non-FPBO grad-quant path). Sibling of
// the Adam-state helpers `_NonShQ` (FusedProjectionBwdOptim_kernel.cuh) and
// `_OptimNonShQ` (FusedGeometryOptim.cu), but with its own codec (NOT the min-max
// QuantizedTensor codec): int codes in [-QMax, QMax] scaled by the block
// amplitude a = max(|min|,|max|), so code 0 decodes to EXACTLY 0.0f.
//
// Zero-preservation is a correctness requirement, not a nicety: with the
// min-max codec a zero gradient (splat out of view) decodes to a persistent
// half-quantum pseudo-gradient, and Adam's m/sqrt(v) normalizes any constant
// pseudo-gradient to a full-learning-rate step -- untouched splats then random
// -walk into floaters. The symmetric codec snaps |v| < a/(2*QMax) to exactly 0.
//
// Storage reuses QuantizedTensor<BITS,256> as a dumb byte/bounds holder
// (16-bit non-SH, 8-bit SH); bounds keep the block-reduced (min,max) float2
// and the amplitude is derived on the fly at decode/encode.
//
// Layout: per-SPLAT-block. Each attribute stores PRIMS contiguous cells per
// splat at base PRIMS*gid; one float2 (min,max) bound per 256 splats. The
// projection-backward kernel launches 256 splats/block so the block-reduced
// bound index is blockIdx.x; a per-cell reader (SH optim) recovers it as
// (cell / cells_per_splat) / 256.
//
// The includer must provide Common.cuh (WARP_SIZE) and Tensor.h before this
// header; both grad-quant TUs (ProjectionBwdQuantGrad_kernel.cuh, FusedGeometryOptim.cu)
// already do.

#include <cstdint>
#include <type_traits>

#include "core/Tensor.h"

#ifdef __CUDACC__
#include <cuda_runtime.h>
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>

namespace gradq {

// Signed symmetric codec over QuantizedTensor<BITS,256> storage. All indices
// are in CELLS. Codes are int8/int16 in [-kQMax, kQMax]; scale is derived from
// the stored (min,max) bound. decode(0) == 0.0f exactly, for any bound.
template<int BITS>
struct Codec {
    static_assert(BITS == 8 || BITS == 16, "gradq::Codec: 8/16-bit only");
    using SignedT = std::conditional_t<BITS == 16, int16_t, int8_t>;
    static constexpr float kQMax    = (BITS == 16) ? 32767.0f : 127.0f;
    static constexpr float kInvQMax = 1.0f / kQMax;

    __device__ static inline float amplitude(float2 mm) {
        return fmaxf(fabsf(mm.x), fabsf(mm.y));
    }

    __device__ static inline float decode1(
        const uint8_t* __restrict__ packed, int64_t cell, float2 mm
    ) {
        float q = (float)reinterpret_cast<const SignedT*>(packed)[cell];
        return q * (amplitude(mm) * kInvQMax);
    }
    // One cell's code as a value, for callers that assemble whole words
    // themselves (the SH grad writeback's staged stores).
    __device__ static inline SignedT encode1_code(float v, float2 mm) {
        float a = amplitude(mm);
        float qf = (a > 0.0f) ? rintf(v * (kQMax / a)) : 0.0f;
        qf = fminf(fmaxf(qf, -kQMax), kQMax);
        return (SignedT)qf;
    }
    __device__ static inline void encode1(
        uint8_t* __restrict__ packed, int64_t cell, float v, float2 mm
    ) {
        reinterpret_cast<SignedT*>(packed)[cell] = encode1_code(v, mm);
    }

    // Decode `count` contiguous cells starting at `base` into out[0..count).
    __device__ static inline void decode(
        const uint8_t* __restrict__ packed, int64_t base, int count,
        float2 mm, float* __restrict__ out
    ) {
        float scale = amplitude(mm) * kInvQMax;
        for (int i = 0; i < count; ++i)
            out[i] = scale * (float)reinterpret_cast<const SignedT*>(packed)[base + i];
    }
    // Encode `count` contiguous cells starting at `base`.
    __device__ static inline void encode(
        uint8_t* __restrict__ packed, int64_t base, int count,
        float2 mm, const float* __restrict__ v
    ) {
        for (int i = 0; i < count; ++i)
            encode1(packed, base + i, v[i], mm);
    }
    // Fold `count` values into a running per-thread (min,max).
    __device__ static inline void accumulate(
        const float* __restrict__ v, int count, float2& mm
    ) {
        for (int i = 0; i < count; ++i) {
            mm.x = fminf(mm.x, v[i]);
            mm.y = fmaxf(mm.y, v[i]);
        }
    }
};

// Block-wide (min,max) reduction of a float2, broadcast to every thread.
// Two-stage warp reduction mirroring _block_reduce_minmax_f4. Starts and ends
// with __syncthreads() so it is re-callable in sequence within a kernel.
template<int BLOCK_SIZE>
__device__ inline float2 block_reduce_minmax_f2(float2 mm) {
    // See _block_reduce_minmax_f4: a NaN must not reach a block bound, or all
    // 256 of the block's cells decode to NaN.
    if (!isfinite(mm.x)) mm.x =  1e30f;
    if (!isfinite(mm.y)) mm.y = -1e30f;
    static_assert(BLOCK_SIZE > 0 && BLOCK_SIZE % WARP_SIZE == 0,
                  "block_reduce_minmax_f2: BLOCK_SIZE must be a multiple of WARP_SIZE");
    namespace cg = cooperative_groups;
    auto warp = cg::tiled_partition<WARP_SIZE>(cg::this_thread_block());
    mm.x = cg::reduce(warp, mm.x, cg::less<float>());
    mm.y = cg::reduce(warp, mm.y, cg::greater<float>());
    __shared__ float2 stage[BLOCK_SIZE / WARP_SIZE];
    __syncthreads();  // prior call's broadcast read (if any) finished
    if (threadIdx.x % WARP_SIZE == 0)
        stage[threadIdx.x / WARP_SIZE] = mm;
    __syncthreads();
    mm = (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        ? stage[threadIdx.x] : float2{1e30f, -1e30f};
    mm.x = cg::reduce(warp, mm.x, cg::less<float>());
    mm.y = cg::reduce(warp, mm.y, cg::greater<float>());
    __syncthreads();
    if (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        stage[threadIdx.x] = mm;
    __syncthreads();
    return stage[threadIdx.x / WARP_SIZE];
}

}  // namespace gradq
#endif  // __CUDACC__
