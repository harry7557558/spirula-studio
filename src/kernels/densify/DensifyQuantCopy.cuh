#pragma once

// Quantized optimizer-state helpers shared by the relocation paths
// (Relocation.cu, DensifySplitFilter.cu): copy a source
// splat's quantized SH/non-SH optim state into a destination slot, or reset
// it to a value that decodes as (g1 = 0, g2 = 0).

#include "kernels/densify/DensifyCommon.cuh"

// Resetting a splat's quantized SH Adam state to "zero" means writing the
// (u_q, log_s_q) byte pair that decodes (with the CURRENT bounds slot for
// that cell) as closely as possible to (g1 = 0, g2 = 0). Naively writing 0
// bytes only matches that intent when the bounds happen to satisfy
// mm.x <= 0 <= mm.y AND mm.z == 0 -- otherwise decode(0,0,mm) lands at
// (mm.x, eps*expm1(mm.z)), which contaminates the new splat's first few
// optim steps. Saturation is acceptable: the next optim pass recomputes
// bounds and re-encodes.
//
// Bounds layout is one of:
//   bounds_per_splat=true  (FPBO): one float4 per 256 splats, covering all
//                                  3*num_sh cells per splat in the block.
//   bounds_per_splat=false (regular Optimizer.cu): one float4 per 256 cells,
//                                  so cells within a single splat may span
//                                  multiple bounds slots.
// Encode a single u or log_s value as the (BITS-bit) quantized nibble or byte
// that decodes back to `zero_val` within the [lo, hi] block bound. Used by
// densify to initialize dst splats' optim-state bytes to (g1 = 0, g2 = 0).
template<int BITS>
__device__ __forceinline__ uint8_t _quant_encode_zero_byte(float zero_val, float lo, float hi) {
    constexpr float qmax = (BITS == 8) ? 255.0f : 15.0f;  // BITS == 4 -> 15
    float range = fmaxf(hi - lo, 1e-30f);
    float qf = roundf(qmax * (zero_val - lo) / range);
    return (uint8_t)fminf(fmaxf(qf, 0.0f), qmax);
}

// Order-preserving u32 image of a float, so an unsigned atomicMin takes a
// float min of either sign. The bound grow below keeps max as min of -v, so
// one 0xff fill empties both halves.
__device__ __forceinline__ uint32_t _qv_key(float f) {
    uint32_t b = __float_as_uint(f);
    return (b & 0x80000000u) ? ~b : (b | 0x80000000u);
}
__device__ __forceinline__ float _qv_unkey(uint32_t k) {
    return __uint_as_float((k & 0x80000000u) ? (k & 0x7fffffffu) : ~k);
}

// SH VALUE-quant codec copy of one splat's cells, src -> dst. GROW instead
// records in `ext` how far dst's bound must widen: clipped to an empty
// block's (0, 0), a spherical-beta lobe becomes a lit lobe facing +z.
template<int BITS, bool BOUNDS_PER_SPLAT, bool GROW>
__device__ __forceinline__ void _copy_quant_sh_value_for_splat(
    uint8_t* __restrict__ packed,
    float2*  __restrict__ bounds,
    uint2*   __restrict__ ext,
    int64_t  src_splat,
    int64_t  dst_splat,
    int      num_sh,        // runtime SH count (degree-capped)
    int      num_sh_buffer  // buffer stride (model max)
) {
    using Codec = QuantizedTensor<BITS, 256>;
    // BOUNDS_PER_SPLAT is the FPBO layout, whose cells are word-paired and
    // block-transposed (docs/notes/sh-quant-layout.md).
    const ShQuantAddr sha =
        sh_quant_addr((uint32_t)num_sh_buffer, BOUNDS_PER_SPLAT ? 0 : 256);
    int64_t src_base = sha.base(src_splat);
    int64_t dst_base = sha.base(dst_splat);

    float2 src_mm{0.f, 0.f}, dst_mm{0.f, 0.f};
    if constexpr (BOUNDS_PER_SPLAT) {
        src_mm = bounds[src_splat / 256];
        dst_mm = bounds[dst_splat / 256];
    }

    int64_t slot = -1;
    uint32_t lo = 0xffffffffu, neg_hi = 0xffffffffu;
    int64_t cells = (int64_t)3 * num_sh;
    #pragma unroll 1
    for (int64_t c = 0; c < cells; ++c) {
        int64_t src_cell = sha.cell(src_base, (int)c);
        int64_t dst_cell = sha.cell(dst_base, (int)c);
        if constexpr (!BOUNDS_PER_SPLAT) {
            src_mm = bounds[src_cell / 256];
            dst_mm = bounds[dst_cell / 256];
        }
        float v = Codec::decode_v(packed, src_cell, src_mm);
        if constexpr (GROW) {
            const int64_t s = BOUNDS_PER_SPLAT ? dst_splat / 256 : dst_cell / 256;
            if (s != slot && slot >= 0) {
                atomicMin(&ext[slot].x, lo);
                atomicMin(&ext[slot].y, neg_hi);
                lo = neg_hi = 0xffffffffu;
            }
            slot = s;
            if (!isfinite(v)) continue;
            lo = min(lo, _qv_key(v));
            neg_hi = min(neg_hi, _qv_key(-v));
        } else {
            Codec::encode_v(packed, dst_cell, v, dst_mm);
        }
    }
    if (GROW && slot >= 0) {
        atomicMin(&ext[slot].x, lo);
        atomicMin(&ext[slot].y, neg_hi);
    }
}

template<int BITS, bool BOUNDS_PER_SPLAT, bool GROW>
__global__ void densify_copy_quant_sh_value_kernel(
    int64_t cur_num_splats,        // for default dst when dst_indices is null
    int64_t num_pairs,
    const int32_t* __restrict__ src_indices,
    const int32_t* __restrict__ dst_indices,  // nullptr -> dst = cur_num_splats + idx
    uint8_t* __restrict__ packed,
    float2*  __restrict__ bounds,
    uint2*   __restrict__ ext,
    int num_sh,
    int num_sh_buffer
) {
    int64_t pair = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (pair >= num_pairs) return;
    int32_t src = src_indices[pair];
    int32_t dst = (dst_indices == nullptr)
        ? (int32_t)(cur_num_splats + pair)
        : dst_indices[pair];
    _copy_quant_sh_value_for_splat<BITS, BOUNDS_PER_SPLAT, GROW>(
        packed, bounds, ext, src, dst, num_sh, num_sh_buffer);
}

// One block per bound slot: a slot whose `ext` reaches past its bound has
// every cell re-encoded to the widened bound, which it then adopts.
template<int BITS>
__global__ void densify_widen_quant_sh_value_kernel(
    int64_t slot_cells,
    uint8_t* __restrict__ packed,
    float2*  __restrict__ bounds,
    const uint2* __restrict__ ext
) {
    using Codec = QuantizedTensor<BITS, 256>;
    const int64_t slot = blockIdx.x;
    const float2 old = bounds[slot];
    const uint2 e = ext[slot];
    float2 nw = old;
    if (e.x != 0xffffffffu) nw.x = fminf(old.x, _qv_unkey(e.x));
    if (e.y != 0xffffffffu) nw.y = fmaxf(old.y, -_qv_unkey(e.y));
    if (nw.x == old.x && nw.y == old.y) return;
    for (int64_t c = threadIdx.x; c < slot_cells; c += blockDim.x) {
        const int64_t cell = slot * slot_cells + c;
        Codec::encode_v(packed, cell, Codec::decode_v(packed, cell, old), nw);
    }
    __syncthreads();
    if (threadIdx.x == 0) bounds[slot] = nw;
}

// Bound slots holding the cells of splats [0, num_splats): one per 256-splat
// block in the FPBO layout, one per 256 cells otherwise.
static inline int64_t _quant_sh_value_slots(int64_t num_splats,
                                            int num_sh_buffer,
                                            bool bounds_per_splat,
                                            int64_t* slot_cells) {
    const int64_t R = 3 * (int64_t)num_sh_buffer;
    *slot_cells = bounds_per_splat ? ((R + 1) >> 1) * 512 : 256;
    const int64_t cells = bounds_per_splat ? ((num_splats + 255) / 256) * *slot_cells
                                           : num_splats * R;
    return (cells + *slot_cells - 1) / *slot_cells;
}

static inline void _launch_widen_quant_sh_value(int64_t num_slots,
                                                int64_t slot_cells,
                                                uint8_t* packed, float2* bounds,
                                                const uint2* ext, int bits) {
    if (bits == 8)
        densify_widen_quant_sh_value_kernel<8><<<(unsigned)num_slots, 256>>>(
            slot_cells, packed, bounds, ext);
    else
        densify_widen_quant_sh_value_kernel<16><<<(unsigned)num_slots, 256>>>(
            slot_cells, packed, bounds, ext);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

// Grow, widen, copy. No-op when bits == 32 or num_pairs == 0. Every dst lies
// below cur_num_splats, or is cur_num_splats + pair without dst_indices.
static inline void _launch_densify_copy_quant_sh_value(
    int64_t cur_num_splats,
    int64_t num_pairs,
    const int32_t* src_indices,
    const int32_t* dst_indices,
    uint8_t* packed,
    float2*  bounds,
    int      num_sh,
    int      num_sh_buffer,
    int      bits,                  // 32 / 8 / 16
    bool     bounds_per_splat
) {
    if (bits == 32 || num_pairs == 0 || packed == nullptr || bounds == nullptr)
        return;
    int64_t slot_cells = 0;
    const int64_t num_slots = _quant_sh_value_slots(
        cur_num_splats + (dst_indices ? 0 : num_pairs), num_sh_buffer,
        bounds_per_splat, &slot_cells);
    DeviceVector<uint2> ext;
    ext.resize(PoolSlot::DensifyQshValueExt, num_slots);
    CHECK_DEVICE_ERROR(cudaMemsetAsync(ext.data_ptr(), 0xff, sizeof(uint2) * num_slots));
    constexpr int BLOCK = 256;
    int grid = (int)((num_pairs + BLOCK - 1) / BLOCK);
    #define _LAUNCH(BB, BPS, GROW) \
        densify_copy_quant_sh_value_kernel<BB, BPS, GROW><<<grid, BLOCK>>>( \
            cur_num_splats, num_pairs, src_indices, dst_indices, \
            packed, bounds, ext.data_ptr(), num_sh, num_sh_buffer)
    #define _LAUNCH_BITS(GROW) \
        if      (bits == 8  &&  bounds_per_splat) { _LAUNCH(8,  true,  GROW); } \
        else if (bits == 8  && !bounds_per_splat) { _LAUNCH(8,  false, GROW); } \
        else if (bits == 16 &&  bounds_per_splat) { _LAUNCH(16, true,  GROW); } \
        else if (bits == 16 && !bounds_per_splat) { _LAUNCH(16, false, GROW); }
    _LAUNCH_BITS(true)
    CHECK_DEVICE_ERROR(cudaGetLastError());
    _launch_widen_quant_sh_value(num_slots, slot_cells, packed, bounds,
                                 ext.data_ptr(), bits);
    _LAUNCH_BITS(false)
    #undef _LAUNCH_BITS
    #undef _LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// Per-cell stores for the joint QuantizedAdamState<BITS, 256> codec.
//   BITS == 8: AoS layout, 2 bytes per cell -- byte[2k]=u_q, byte[2k+1]=log_s_q
//   BITS == 4: joint nibbles, 1 byte per cell -- low nibble = u_q, high = log_s_q
// Within densify both halves of every cell get zeroed for the dst splat, so
// these stores don't race with neighbors at the cell boundary (each thread
// owns its splat's full cell range).
template<int BITS>
__device__ __forceinline__ void _zero_quant_sh_store_cell(
    uint8_t* __restrict__ packed, int64_t cell, uint8_t u_q, uint8_t log_s_q
) {
    if constexpr (BITS == 8) {
        packed[cell * 2 + 0] = u_q;
        packed[cell * 2 + 1] = log_s_q;
    } else {  // BITS == 4
        packed[cell] = (uint8_t)((u_q & 0x0Fu) | ((log_s_q & 0x0Fu) << 4));
    }
}

// Non-SH 16-bit Adam-state codec mirrors QuantizedAdamState<16, 256> in
// Tensor.h: each cell is 4 bytes -- (uint16_t u_q)(uint16_t log_s_q). One
// per-splat-block float4 bound covers all PRIMS cells per splat for the
// attribute. Used by densify to initialize a relocated splat's optim-state
// bytes so they decode to (g1 = 0, g2 = 0) against the block's CURRENT bound;
// the next FPBO step's reduce expands the bound to cover the new value.
// 0 may fall outside the current bound (e.g. mm.x > 0); the encode clamps to
// the nearest endpoint then -- accepted small transient, identical to the
// SH-value densify-copy strategy.
__device__ __forceinline__ uint16_t _quant_encode_zero_word16(
    float zero_val, float lo, float hi
) {
    constexpr float qmax = 65535.0f;
    float range = fmaxf(hi - lo, 1e-30f);
    float qf = roundf(qmax * (zero_val - lo) / range);
    return (uint16_t)fminf(fmaxf(qf, 0.0f), qmax);
}

template<int PRIMS>
__device__ __forceinline__ void _zero_quant_non_sh_for_splat(
    uint8_t* __restrict__ packed,
    const float4* __restrict__ bounds,
    int64_t splat_idx
) {
    if (packed == nullptr || bounds == nullptr) return;
    float4 mm = bounds[splat_idx / 256];
    uint16_t u_q     = _quant_encode_zero_word16(0.0f, mm.x, mm.y);
    uint16_t log_s_q = _quant_encode_zero_word16(0.0f, mm.z, mm.w);
    int64_t base_cell = (int64_t)PRIMS * splat_idx;
    uint16_t* p = reinterpret_cast<uint16_t*>(packed);
    #pragma unroll
    for (int i = 0; i < PRIMS; ++i) {
        p[(base_cell + i) * 2 + 0] = u_q;
        p[(base_cell + i) * 2 + 1] = log_s_q;
    }
}

__device__ __forceinline__ void _zero_quant_non_sh_all(
    const NonShQuantState& non_sh, int64_t splat_idx
) {
    if (!non_sh.enabled) return;
    _zero_quant_non_sh_for_splat<3>(non_sh.means_packed,       non_sh.means_bounds,       splat_idx);
    _zero_quant_non_sh_for_splat<4>(non_sh.quats_packed,       non_sh.quats_bounds,       splat_idx);
    _zero_quant_non_sh_for_splat<3>(non_sh.scales_packed,      non_sh.scales_bounds,      splat_idx);
    _zero_quant_non_sh_for_splat<1>(non_sh.opacities_packed,   non_sh.opacities_bounds,   splat_idx);
    _zero_quant_non_sh_for_splat<3>(non_sh.features_dc_packed, non_sh.features_dc_bounds, splat_idx);
}


template<int BITS>
__device__ __forceinline__ void _zero_quant_sh_for_splat(
    uint8_t* __restrict__ packed,
    int64_t splat_idx,
    int num_sh,
    const float4* __restrict__ bounds,
    bool bounds_per_splat
) {
    int64_t cells_per_splat = (int64_t)num_sh * 3;
    const ShQuantAddr sha =
        sh_quant_addr((uint32_t)num_sh, bounds_per_splat ? 0 : 256);
    int64_t base_cell = sha.base(splat_idx);
    if (bounds_per_splat) {
        float4 mm = bounds[splat_idx / 256];
        uint8_t u_q     = _quant_encode_zero_byte<BITS>(0.0f, mm.x, mm.y);
        uint8_t log_s_q = _quant_encode_zero_byte<BITS>(0.0f, mm.z, mm.w);
        #pragma unroll 1
        for (int64_t c = 0; c < cells_per_splat; ++c) {
            _zero_quant_sh_store_cell<BITS>(packed, sha.cell(base_cell, (int)c),
                                            u_q, log_s_q);
        }
    } else {
        #pragma unroll 1
        for (int64_t c = 0; c < cells_per_splat; ++c) {
            int64_t cell = base_cell + c;
            float4 mm = bounds[cell / 256];
            uint8_t u_q     = _quant_encode_zero_byte<BITS>(0.0f, mm.x, mm.y);
            uint8_t log_s_q = _quant_encode_zero_byte<BITS>(0.0f, mm.z, mm.w);
            _zero_quant_sh_store_cell<BITS>(packed, cell, u_q, log_s_q);
        }
    }
}
