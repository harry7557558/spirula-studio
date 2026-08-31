#include <cuda_runtime.h>
#include <cstdint>

#include <core/Common.cuh>

#ifndef NO_TORCH
#define NO_TORCH
#endif

#include "core/GradQuant.cuh"
#include "kernels/projection/ProjectionBwdQuantGrad.cuh"   // GradQuantBuffers struct

#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include "core/AabbQuant.cuh"
namespace cg = cooperative_groups;


// ============================================================================
// projection_bwd_quantgrad_kernel -- non-atomic, block-wise QUANTIZED
// gradient-accumulation projection backward (non-FPBO grad-quant path).
//
// One thread per splat (256/block); loops that splat's cameras for THIS
// sub-batch in registers (no atomics), then folds the register gradient onto
// the running quantized grad accumulator via decode -> add -> block-reduce
// bounds -> encode. Called once per sub-batch; the persistent quantized grad
// buffers carry state across sub-batches (split_batch). Mirrors the front half
// of the FPBO kernel (FusedProjectionBwdOptim_kernel.cuh:232-332) but writes a
// materialized quantized gradient the separate optim step later decodes.
//
// Per-attribute output routing (via GradQuantBuffers pointers):
//   3dgs / mip : all six attributes quantized (fp32 world buffer is empty, so
//                its slot pointers are null -> the fp32 add is skipped).
//   3dgut      : means/quats/scales come from rasterization-backward atomicAdds
//                into the fp32 world buffer, so their quant pointers are null
//                and the projection contribution is added (non-atomically, one
//                owner thread) into that fp32 buffer; opacity/features_dc/
//                features_sh are quantized.
//
// Layout: per-SPLAT-block. Non-SH: PRIMS cells/splat at base PRIMS*gid, one
// 16-bit codec, one float2 bound per 256 splats (index blockIdx.x). SH: 3*K
// cells/splat at base 3*num_sh_buffer*gid, 8-bit codec, one float2 bound per
// 256 splats. Zero-init (bounds == 0) decodes every cell to 0 (zero grad).
// ============================================================================

template<
    typename SplatPrimitive,
    CameraModelType camera_model,
    CameraDistortionType distortion,
    int VALUE_BITS = 32,
    int BLOCK_SIZE = 256
>
__global__ void __launch_bounds__(BLOCK_SIZE) projection_bwd_quantgrad_kernel(
    // fwd inputs
    const uint32_t C,
    const uint32_t N,
    const uint32_t num_sh_buffer,   // SH stride (= model max K); may exceed template num_sh during warmup
    typename SplatPrimitive::WorldBuffer splats_world,
    const float *__restrict__ viewmats,   // [C, 4, 4]
    const float4 *__restrict__ intrins,    // [C, 4]
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // fwd outputs (packed intersection ordering; nulls => non-packed C*N)
    const int32_t *__restrict__ camera_id_bounds,   // [N+1]
    const int32_t *__restrict__ camera_ids,         // [nnz] ORIGINAL order
    const uint2 *__restrict__ aabb,                 // [C, N] or [nnz], packed
    // fp32 world grad (means/quats/scales for 3dgut; empty for 3dgs/mip)
    typename SplatPrimitive::WorldBuffer v_splats_world,
    // screen grad from rasterization backward
    typename SplatPrimitive::ScreenBuffer v_splats_screen,
    // quantized grad accumulators (packed + per-splat-block bounds)
    GradQuantBuffers gq,
    // SH VALUE quant (source SH read for the v_dir chain), active VALUE_BITS!=32.
    // sh_value_bounds_stride = cells-per-bound of the ACTUAL value-quant layout
    // (256 for the non-FPBO per-cell-block features_sh_quant{8,16}); the slang
    // codec indexes bounds[cell / stride].
    const uint8_t *__restrict__ sh_value_packed,
    const float2 *__restrict__ sh_value_bounds,
    const int64_t sh_value_bounds_stride
) {
    const uint32_t gid = blockIdx.x * BLOCK_SIZE + threadIdx.x;
    const bool inside = (gid < N);

    const bool packed = (camera_id_bounds != nullptr && camera_ids != nullptr);
    int cid_0 = 0, cid_1 = (int)C;
    if (packed) {
        cid_0 = inside ? camera_id_bounds[gid]   : 0;
        cid_1 = inside ? camera_id_bounds[gid+1] : 0;
    }

    typename SplatPrimitive::World splat_world;
    if (inside) splat_world.load(splats_world, gid);

    constexpr int num_sh = SplatPrimitive::World::num_sh();
    float v_sh_coeffs[num_sh == 0 ? 1 : 3 * num_sh];

    // Register accumulator for THIS sub-batch's projection contribution.
    typename SplatPrimitive::World v = SplatPrimitive::World::zero();
    v.features_sh = &v_sh_coeffs[0];
    #pragma unroll
    for (int i = 0; i < 3 * num_sh; ++i) v.features_sh[i] = 0.0f;

    [[maybe_unused]] float3x3 v_R = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    [[maybe_unused]] float3 v_t = {0.f, 0.f, 0.f};

    // ---- camera loop (register accumulation, no atomics) ----
    for (int cid_t = cid_0; cid_t < cid_1; ++cid_t) {
        // Packed: the forward emits (gaussian, camera) order, so cid_t is
        // the out_idx that indexes aabb / camera_ids / v_splats_screen.
        int idx = packed ? cid_t : cid_t * (int)N + (int)gid;
        int cid = packed ? camera_ids[idx] : cid_t;
        if (aabb16_is_empty(aabb[idx]))
            continue;

        float4 intrin = intrins[cid];
        float3x3 R = {
            viewmats[cid*16+0], viewmats[cid*16+1], viewmats[cid*16+2],
            viewmats[cid*16+4], viewmats[cid*16+5], viewmats[cid*16+6],
            viewmats[cid*16+8], viewmats[cid*16+9], viewmats[cid*16+10],
        };
        float3 t = { viewmats[cid*16+3], viewmats[cid*16+7], viewmats[cid*16+11] };
        ProjCameraT<distortion> cam = {
            R, t, intrin.x, intrin.y, intrin.z, intrin.w,
            image_width, image_height,
        };
        cam.dist_coeffs = dist_coeffs_buffer.load<distortion>(cid);

        typename SplatPrimitive::Screen v_screen;
        v_screen.load(v_splats_screen, idx);

        if constexpr (VALUE_BITS == 32) {
            splat_world.template project_vjp<camera_model, distortion, false, 32>(
                cam, v_screen, v, v_R, v_t);
        } else {
            const ShQuantAddr sha =
                sh_quant_addr(num_sh_buffer, sh_value_bounds_stride);
            splat_world.template project_vjp<camera_model, distortion, false, VALUE_BITS>(
                cam, v_screen, v, v_R, v_t,
                const_cast<uint8_t*>(sh_value_packed),
                const_cast<float2*>(sh_value_bounds),
                sha.base(gid), sha.bounds_stride, sha.pair_pitch);
        }
    }

    // ---- fp32 geometry writeback (3dgut: add proj contrib onto raster-bwd's
    // world buffer; one owner thread per gid so a plain add is race-free) ----
    if (inside) {
        if (&v_splats_world.means(0))  v_splats_world.means(gid)  = v_splats_world.means(gid)  + v.mean;
        if (&v_splats_world.quats(0))  v_splats_world.quats(gid)  = v_splats_world.quats(gid)  + v.quat;
        if (&v_splats_world.scales(0)) v_splats_world.scales(gid) = v_splats_world.scales(gid) + v.scale;
    }

    // ---- quantized attribute writeback: decode running grad, add this
    // sub-batch, block-reduce fresh bounds, re-encode. All threads participate
    // in every block reduce (pointers are grid-uniform, so the branch is too).
    #define QGRAD_NONSH(PACKED, BOUNDS, PRIMS, VREG)                              \
        if ((PACKED) != nullptr) {                                               \
            float _vals[PRIMS];                                                   \
            float2 _mm = make_float2(1e30f, -1e30f);                             \
            if (inside) {                                                         \
                float2 _old = (BOUNDS)[blockIdx.x];                              \
                gradq::Codec<16>::decode((PACKED), (int64_t)(PRIMS)*gid,          \
                                         (PRIMS), _old, _vals);                   \
                const float* _c = (const float*)&(VREG);                          \
                _Pragma("unroll") for (int _i = 0; _i < (PRIMS); ++_i)           \
                    _vals[_i] += _c[_i];                                          \
                gradq::Codec<16>::accumulate(_vals, (PRIMS), _mm);              \
            }                                                                    \
            _mm = gradq::block_reduce_minmax_f2<BLOCK_SIZE>(_mm);                \
            if (threadIdx.x == 0) (BOUNDS)[blockIdx.x] = _mm;                     \
            if (inside)                                                           \
                gradq::Codec<16>::encode((PACKED), (int64_t)(PRIMS)*gid,          \
                                         (PRIMS), _mm, _vals);                    \
        }

    QGRAD_NONSH(gq.means_packed,  gq.means_bounds,  3, v.mean)
    QGRAD_NONSH(gq.quats_packed,  gq.quats_bounds,  4, v.quat)
    QGRAD_NONSH(gq.scales_packed, gq.scales_bounds, 3, v.scale)
    QGRAD_NONSH(gq.opac_packed,   gq.opac_bounds,   1, v.opacity)
    QGRAD_NONSH(gq.dc_packed,     gq.dc_bounds,     3, v.features_dc)
    #undef QGRAD_NONSH

    // ---- SH grad (8-bit): two passes over the full 3*K cells so inactive
    // warmup bands (contrib 0) stay re-encoded against the fresh bound. ----
    if (gq.sh_packed != nullptr) {
        const int64_t sh_base = (int64_t)3 * (int64_t)num_sh_buffer * (int64_t)gid;
        const int total = 3 * (int)num_sh_buffer;
        const float2 sh_old = inside ? gq.sh_bounds[blockIdx.x] : make_float2(0.f, 0.f);
        float2 sh_mm = make_float2(1e30f, -1e30f);
        if (inside) {
            for (int c = 0; c < total; ++c) {
                float old = gradq::Codec<8>::decode1(gq.sh_packed, sh_base + c, sh_old);
                float contrib = (c < 3 * num_sh) ? v_sh_coeffs[c] : 0.0f;
                float nv = old + contrib;
                sh_mm.x = fminf(sh_mm.x, nv);
                sh_mm.y = fmaxf(sh_mm.y, nv);
            }
        }
        sh_mm = gradq::block_reduce_minmax_f2<BLOCK_SIZE>(sh_mm);
        if (threadIdx.x == 0) gq.sh_bounds[blockIdx.x] = sh_mm;

        // Whole-word writeback of this thread's contiguous byte-cell run. The
        // per-cell stores it replaces make every warp store touch 32 sectors;
        // here only the up-to-two edge words are shared with a neighbour.
        if (inside) {
            uint32_t* words = reinterpret_cast<uint32_t*>(gq.sh_packed);
            const int64_t c0 = sh_base, c1 = sh_base + total;
            uint32_t acc = 0;
            for (int64_t cell = c0; cell < c1; ++cell) {
                const int c = (int)(cell - c0);
                float old = gradq::Codec<8>::decode1(gq.sh_packed, cell, sh_old);
                float contrib = (c < 3 * num_sh) ? v_sh_coeffs[c] : 0.0f;
                const uint32_t code = (uint32_t)(uint8_t)(int8_t)
                    gradq::Codec<8>::encode1_code(old + contrib, sh_mm);
                const uint32_t b = (uint32_t)(cell & 3);
                acc |= code << (8u * b);
                if (b == 3u || cell + 1 == c1) {
                    const int64_t w = cell >> 2;
                    const uint32_t fb = (w == (c0 >> 2)) ? (uint32_t)(c0 & 3) : 0u;
                    if (fb == 0u && b == 3u) {
                        words[w] = acc;
                    } else {
                        uint32_t mask = 0;
                        for (uint32_t k = fb; k <= b; ++k) mask |= 0xffu << (8u * k);
                        atomicAnd(&words[w], ~mask);
                        atomicOr(&words[w], acc & mask);
                    }
                    acc = 0;
                }
            }
        }
    }
}


// Thin per-(primitive, camera model, distortion tier) launcher; dispatches
// VALUE_BITS at runtime (mirrors projection_fused_bwd_kernel_wrapper in
// ProjectionBwd_kernel.cuh).
template<
    typename SplatPrimitive,
    CameraModelType camera_model,
    CameraDistortionType distortion
>
void projection_bwd_quantgrad_kernel_wrapper(
    cudaStream_t stream,
    const uint32_t C,
    const uint32_t N,
    const uint32_t num_sh_buffer,
    typename SplatPrimitive::WorldBuffer splats_world,
    const float * viewmats,
    const float4 * intrins,
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    const int32_t * camera_id_bounds,
    const int32_t * camera_ids,
    const uint2 * aabb,
    typename SplatPrimitive::WorldBuffer v_splats_world,
    typename SplatPrimitive::ScreenBuffer v_splats_screen,
    GradQuantBuffers gq,
    const uint8_t* sh_value_packed,
    const float2* sh_value_bounds,
    const int64_t sh_value_bounds_stride,
    const int sh_value_bits
) {
    constexpr int BLOCK = 256;
    const uint32_t grid = (N + BLOCK - 1) / BLOCK;
    if (grid == 0) return;
    #define _QG_LAUNCH(VB)                                                        \
        projection_bwd_quantgrad_kernel<SplatPrimitive, camera_model,             \
                                       distortion, VB, BLOCK>                    \
            <<<grid, BLOCK, 0, stream>>>(                                         \
                C, N, num_sh_buffer, splats_world, viewmats, intrins,             \
                dist_coeffs_buffer, image_width, image_height,                    \
                camera_id_bounds, camera_ids, aabb,                               \
                v_splats_world, v_splats_screen, gq,                              \
                sh_value_packed, sh_value_bounds, sh_value_bounds_stride)
    if      (sh_value_bits == 8)  { _QG_LAUNCH(8); }
    else if (sh_value_bits == 16) { _QG_LAUNCH(16); }
    else                          { _QG_LAUNCH(32); }
    #undef _QG_LAUNCH
}
