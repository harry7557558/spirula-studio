// Block-wise quantized-state codecs (Vulkan backend) mirroring the CUDA
// device codecs in core/Tensor.h (QuantizedAdamState, QuantizedTensor) and
// core/GradQuant.cuh (gradq::Codec<8>). Packed storage is read and written
// as u32 WORDS; cell indices are u32, plus a gindex_t overload on the
// helpers the SH buffers reach (past 2^32 cells).
//
// Sub-word cells are assembled in groupshared (_oq_pack) and stored whole by
// the lowest thread of each word group; a 256-cell block starts word-aligned
// and the tail bytes fall in the allocation's 4-byte rounding, written as
// zero where CUDA leaves them (nothing reads them).
//
// log1p/expm1 are compensated forms: parity allows +-1 step per cell.

#include "int8_compat.slang"

static const float kAdamEps = 1e-15f;

[ForceInline]
bool _oq_finite(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

// SPIR-V leaves FMin/FMax undefined on a NaN operand, so a driver may let one
// through where CUDA's fminf/fmaxf drop it -- and one NaN in a block bound
// decodes all 256 of its cells to NaN. Pick the finite side explicitly.
[ForceInline]
float _oq_min(float a, float b) {
    if (!_oq_finite(a)) return b;
    if (!_oq_finite(b)) return a;
    return min(a, b);
}
[ForceInline]
float _oq_max(float a, float b) {
    if (!_oq_finite(a)) return b;
    if (!_oq_finite(b)) return a;
    return max(a, b);
}

// CUDA's fminf(fmaxf(x, 0), qmax) provably sends NaN to 0; SPIR-V's FClamp
// does not, and OpConvertFToU of a NaN or out-of-range float is undefined --
// a garbage code corrupts the neighbouring cells sharing its word.
[ForceInline]
uint _oq_code(float x, float qmax) {
    if (!_oq_finite(x)) return 0u;
    return uint(clamp(x, 0.0f, qmax));
}

[ForceInline]
float _oq_log1p(float x) {
    float u = 1.0f + x;
    if (u == 1.0f) return x;
    if (!_oq_finite(u)) return u;   // log1pf(+inf) = +inf, not inf*(inf/inf)
    return log(u) * (x / (u - 1.0f));
}

[ForceInline]
float _oq_expm1(float x) {
    float u = exp(x);
    if (u == 1.0f) return x;
    if (u == 0.0f) return -1.0f;    // expm1f(-inf) = -1
    if (!_oq_finite(u)) return u;   // expm1f(+inf) = +inf
    return (u - 1.0f) * (x / log(u));
}

// floor(x + 0.5) matches CUDA roundf everywhere the callers' subsequent
// clamp-to-[0, qmax] doesn't already coalesce the difference (the only
// mismatch is exactly -0.5, which both paths clamp to 0).
[ForceInline]
float _oq_round(float x) { return floor(x + 0.5f); }

// Scratch shared by the word-assembly stores and the block reductions.
// Every helper both begins and ends its groupshared use with a barrier, so
// they are safely callable in sequence within one kernel.
groupshared uint _oq_pack[256];
groupshared float4 _oq_red4[256];

// Block-wide (min, max, min, max) reduction over all 256 threads,
// broadcast to every thread. Groupshared tree combine (no subgroup-id
// assumptions, and min/max is exactly associative so the result matches a
// serial fold bit-for-bit). Pass sentinels (+1e30, -1e30, ...) for threads
// that must not contribute.
[ForceInline]
float4 oq_block_reduce_mm4(float4 v, uint tid) {
    GroupMemoryBarrierWithGroupSync();
    _oq_red4[tid] = v;
    GroupMemoryBarrierWithGroupSync();
    [ForceUnroll]
    for (uint st = 128; st > 0; st >>= 1) {
        if (tid < st) {
            float4 a = _oq_red4[tid];
            float4 b = _oq_red4[tid + st];
            _oq_red4[tid] = float4(_oq_min(a.x, b.x), _oq_max(a.y, b.y),
                                   _oq_min(a.z, b.z), _oq_max(a.w, b.w));
        }
        GroupMemoryBarrierWithGroupSync();
    }
    return _oq_red4[0];
}

// float2 (min, max) variant over the same scratch.
[ForceInline]
float2 oq_block_reduce_mm2(float2 v, uint tid) {
    float4 r = oq_block_reduce_mm4(float4(v.x, v.y, 1e30f, -1e30f), tid);
    return float2(r.x, r.y);
}

// ---------------------------------------------------------------------------
// QuantizedAdamState<BITS, 256> — joint (u, log_s) Adam state, AoS.
//   BITS=16: u32 word/cell   BITS=8: halfword/cell   BITS=4: byte/cell
// ---------------------------------------------------------------------------

[ForceInline]
float qadam_qmax<let BITS : int>() { return float((1 << BITS) - 1); }

[ForceInline]
uint2 qadam_read_q<let BITS : int>(uint* packed, uint idx) {
    if (BITS == 16) {
        uint w = packed[idx];
        return uint2(w & 0xffffu, w >> 16);
    } else if (BITS == 8) {
        uint w = packed[idx >> 1];
        uint h = (w >> (16u * (idx & 1u))) & 0xffffu;
        return uint2(h & 0xffu, h >> 8);
    } else {  // BITS == 4
        uint w = packed[idx >> 2];
        uint b = (w >> (8u * (idx & 3u))) & 0xffu;
        return uint2(b & 0xfu, b >> 4);
    }
}

[ForceInline]
uint2 qadam_read_q<let BITS : int>(uint* packed, gindex_t idx) {
    if (BITS == 16) {
        uint w = elem_at(packed, idx)[0];
        return uint2(w & 0xffffu, w >> 16);
    } else if (BITS == 8) {
        uint w = elem_at(packed, idx >> 1)[0];
        uint h = (w >> (16u * (uint(idx) & 1u))) & 0xffffu;
        return uint2(h & 0xffu, h >> 8);
    } else {  // BITS == 4
        uint w = elem_at(packed, idx >> 2)[0];
        uint b = (w >> (8u * (uint(idx) & 3u))) & 0xffu;
        return uint2(b & 0xfu, b >> 4);
    }
}

[ForceInline]
float2 _qadam_us_of<let BITS : int>(uint2 q, float4 mm) {
    float inv = 1.0f / qadam_qmax<BITS>();
    // See core/Tensor.h decode_us: a non-finite bound would decode all 256 of
    // the block's cells to NaN, and nothing later undoes that.
    if (!_oq_finite(mm.x) || !_oq_finite(mm.y)) { mm.x = 0.0f; mm.y = 0.0f; }
    if (!_oq_finite(mm.z) || !_oq_finite(mm.w)) { mm.z = 0.0f; mm.w = 0.0f; }
    return float2(mm.x + (mm.y - mm.x) * (float(q.x) * inv),
                  mm.z + (mm.w - mm.z) * (float(q.y) * inv));
}

[ForceInline]
float2 _qadam_g1g2_of(float2 prim) {
    float sqrt_g2 = kAdamEps * _oq_expm1(prim.y);
    return float2(prim.x * (sqrt_g2 + kAdamEps), sqrt_g2 * sqrt_g2);
}

[ForceInline]
float2 qadam_decode_us<let BITS : int>(uint* packed, uint idx, float4 mm) {
    return _qadam_us_of<BITS>(qadam_read_q<BITS>(packed, idx), mm);
}

[ForceInline]
float2 qadam_decode_g1g2<let BITS : int>(uint* packed, uint idx, float4 mm) {
    return _qadam_g1g2_of(qadam_decode_us<BITS>(packed, idx, mm));
}

[ForceInline]
float2 qadam_decode_g1g2<let BITS : int>(uint* packed, gindex_t idx,
                                         float4 mm) {
    return _qadam_g1g2_of(
        _qadam_us_of<BITS>(qadam_read_q<BITS>(packed, idx), mm));
}

// (g1, g2) -> linearly-quantized primitives (u, log_s); the block bounds
// reduction operates on these.
[ForceInline]
float2 qadam_g1g2_to_us(float g1, float g2) {
    float sqrt_g2 = sqrt(max(g2, 0.0f));
    float u = g1 / (sqrt_g2 + kAdamEps);
    return float2(u, _oq_log1p(max(sqrt_g2, 0.0f) * (1.0f / kAdamEps)));
}

[ForceInline]
uint2 qadam_quantize_us<let BITS : int>(float u_val, float log_s_val,
                                        float4 mm) {
    float qmax = qadam_qmax<BITS>();
    float u_range = max(mm.y - mm.x, kAdamEps);
    float s_range = max(mm.w - mm.z, kAdamEps);
    return uint2(_oq_code(_oq_round(qmax * (u_val - mm.x) / u_range), qmax),
                 _oq_code(_oq_round(qmax * (log_s_val - mm.z) / s_range),
                          qmax));
}

// Cooperative packed store of one cell's (u_q, s_q). Call from ALL 256
// threads (barriers inside); `inside` gates the actual memory writes.
[ForceInline]
void qadam_store<let BITS : int>(uint* packed, uint idx, uint tid,
                                 bool inside, uint2 q) {
    if (BITS == 16) {
        if (inside) packed[idx] = q.x | (q.y << 16);
    } else if (BITS == 8) {
        GroupMemoryBarrierWithGroupSync();
        _oq_pack[tid] = inside ? (q.x | (q.y << 8)) : 0u;
        GroupMemoryBarrierWithGroupSync();
        if ((tid & 1u) == 0u && inside)
            packed[idx >> 1] = _oq_pack[tid] | (_oq_pack[tid + 1] << 16);
        GroupMemoryBarrierWithGroupSync();
    } else {  // BITS == 4
        GroupMemoryBarrierWithGroupSync();
        _oq_pack[tid] = inside ? ((q.x & 0xfu) | ((q.y & 0xfu) << 4)) : 0u;
        GroupMemoryBarrierWithGroupSync();
        if ((tid & 3u) == 0u && inside)
            packed[idx >> 2] = _oq_pack[tid] | (_oq_pack[tid + 1] << 8) |
                               (_oq_pack[tid + 2] << 16) |
                               (_oq_pack[tid + 3] << 24);
        GroupMemoryBarrierWithGroupSync();
    }
}

// ---------------------------------------------------------------------------
// QuantizedTensor<BITS, 256> — linear value codec.
//   BITS=16: halfword/cell   BITS=8: byte/cell
// (Same codec the SH value-quant render path decodes via sh_quant.slang.)
// ---------------------------------------------------------------------------

[ForceInline]
float qtensor_decode_v<let BITS : int>(uint* packed, uint idx, float2 mm) {
    float q;
    if (BITS == 16) {
        uint w = packed[idx >> 1];
        q = float((w >> (16u * (idx & 1u))) & 0xffffu);
    } else {  // BITS == 8
        q = float(u8_load(packed, idx));
    }
    return mm.x + (mm.y - mm.x) * (q / float((1 << BITS) - 1));
}

[ForceInline]
float qtensor_decode_v<let BITS : int>(uint* packed, gindex_t idx, float2 mm) {
    float q;
    if (BITS == 16) {
        uint w = elem_at(packed, idx >> 1)[0];
        q = float((w >> (16u * (uint(idx) & 1u))) & 0xffffu);
    } else {  // BITS == 8
        q = float(u8_load(packed, idx));
    }
    return mm.x + (mm.y - mm.x) * (q / float((1 << BITS) - 1));
}

[ForceInline]
uint qtensor_quantize_v<let BITS : int>(float v, float2 mm) {
    float qmax = float((1 << BITS) - 1);
    float range = max(mm.y - mm.x, 1e-30f);
    return _oq_code(_oq_round(qmax * (v - mm.x) / range), qmax);
}

// Cooperative packed store of one cell's value code. Call from ALL 256
// threads (barriers inside); `inside` gates the actual memory writes.
[ForceInline]
void qtensor_store<let BITS : int>(uint* packed, uint idx, uint tid,
                                   bool inside, uint q) {
    GroupMemoryBarrierWithGroupSync();
    _oq_pack[tid] = inside ? q : 0u;
    GroupMemoryBarrierWithGroupSync();
    if (BITS == 16) {
        if ((tid & 1u) == 0u && inside)
            packed[idx >> 1] = _oq_pack[tid] | (_oq_pack[tid + 1] << 16);
    } else {  // BITS == 8
        if ((tid & 3u) == 0u && inside)
            packed[idx >> 2] = _oq_pack[tid] | (_oq_pack[tid + 1] << 8) |
                               (_oq_pack[tid + 2] << 16) |
                               (_oq_pack[tid + 3] << 24);
    }
    GroupMemoryBarrierWithGroupSync();
}

// ---------------------------------------------------------------------------
// QuantizedTensorLog<BITS, 256> — non-negative scalar per cell, stored as a
// linearly-quantized log1p(v / eps) with per-block (log-min, log-max) float2
// bounds. 0 <-> 0 fixed point (all-zero packed + bounds decode to v = 0).
//   BITS=8: byte/cell   BITS=4: nibble/cell (two cells per byte)
// Mirrors core/Tensor.h QuantizedTensorLog (bilagrid AdaGrad accumulator).
// ---------------------------------------------------------------------------

[ForceInline]
float qtlog_forward_v(float v) {
    return _oq_log1p(max(v, 0.0f) * (1.0f / kAdamEps));
}

[ForceInline]
float qtlog_inverse_v(float log_v) {
    return kAdamEps * _oq_expm1(log_v);
}

[ForceInline]
float qtlog_decode_log<let BITS : int>(uint* packed, uint idx, float2 mm) {
    float q;
    if (BITS == 8) {
        q = float(u8_load(packed, idx));
    } else {  // BITS == 4
        uint w = packed[idx >> 3];
        q = float((w >> (4u * (idx & 7u))) & 0xfu);
    }
    return mm.x + (mm.y - mm.x) * (q / float((1 << BITS) - 1));
}

[ForceInline]
float qtlog_decode_v<let BITS : int>(uint* packed, uint idx, float2 mm) {
    return qtlog_inverse_v(qtlog_decode_log<BITS>(packed, idx, mm));
}

[ForceInline]
uint qtlog_quantize_log<let BITS : int>(float log_v, float2 mm) {
    float qmax = float((1 << BITS) - 1);
    float range = max(mm.y - mm.x, kAdamEps);
    return _oq_code(_oq_round(qmax * (log_v - mm.x) / range), qmax);
}

// Cooperative packed store of one cell's log-code. Call from ALL 256 threads
// (barriers inside); `inside` gates the actual memory writes. Subsumes the
// CUDA 4-bit odd/even two-phase serialization: whole words are assembled in
// groupshared and stored once.
[ForceInline]
void qtlog_store<let BITS : int>(uint* packed, uint idx, uint tid,
                                 bool inside, uint q) {
    GroupMemoryBarrierWithGroupSync();
    _oq_pack[tid] = inside ? q : 0u;
    GroupMemoryBarrierWithGroupSync();
    if (BITS == 8) {
        if ((tid & 3u) == 0u && inside)
            packed[idx >> 2] = _oq_pack[tid] | (_oq_pack[tid + 1] << 8) |
                               (_oq_pack[tid + 2] << 16) |
                               (_oq_pack[tid + 3] << 24);
    } else {  // BITS == 4
        if ((tid & 7u) == 0u && inside) {
            uint w = 0u;
            [ForceUnroll]
            for (uint k = 0; k < 8; k++)
                w |= (_oq_pack[tid + k] & 0xfu) << (4u * k);
            packed[idx >> 3] = w;
        }
    }
    GroupMemoryBarrierWithGroupSync();
}

// ---------------------------------------------------------------------------
// Masked sub-word atomic stores. For packed layouts where one thread owns a
// CONTIGUOUS cell run that need not be word-aligned (per-splat runs in the
// grad-quant / FPBO writebacks), neighbor threads can share a word. And+Or
// with disjoint masks is correct under any interleaving: each atomic only
// touches the owner's bits, so the final word is
// (old & ~maskA & ~maskB) | bitsA | bitsB regardless of order.
// ---------------------------------------------------------------------------

[ForceInline]
void vkq_store_u16(uint* packed, uint idx, uint h) {  // idx in halfwords
    uint shift = 16u * (idx & 1u);
    InterlockedAnd(packed[idx >> 1], ~(0xffffu << shift));
    InterlockedOr(packed[idx >> 1], (h & 0xffffu) << shift);
}

[ForceInline]
void vkq_store_u8(uint* packed, uint idx, uint b) {  // idx in bytes
    u8_store(packed, idx, b);
}

[ForceInline]
void vkq_store_u16(uint* packed, gindex_t idx, uint h) {
    uint shift = 16u * (uint(idx) & 1u);
    uint* w = elem_at(packed, idx >> 1);
    InterlockedAnd(w[0], ~(0xffffu << shift));
    InterlockedOr(w[0], (h & 0xffffu) << shift);
}

[ForceInline]
void vkq_store_u8(uint* packed, gindex_t idx, uint b) {
    u8_store(packed, idx, b);
}

// Streaming store of one thread's CONTIGUOUS ascending halfword-cell run
// [c0, c1) (the grad-quant per-splat writebacks): present each cell in
// order via vkq_run_put_u16, then call vkq_run_flush_u16 with the run end.
// Fully-owned words are assembled in `acc` and written as plain u32 stores;
// only the up-to-two boundary words shared with a neighboring owner fall
// back to the masked And+Or atomics — two atomics per RUN edge instead of
// two per CELL. (For large runs prefer the FPBO kernel's groupshared
// staging: per-lane runs are lane-strided, so even plain per-thread word
// stores defeat write coalescing.)
[ForceInline]
void vkq_run_put_u16(uint* packed, uint c0, uint c, uint h, inout uint acc) {
    if ((c & 1u) == 0u)
        acc = h & 0xffffu;            // lower half: hold for the word store
    else if (c == c0)
        vkq_store_u16(packed, c, h);  // head cell of a word we don't own
    else
        packed[c >> 1] = acc | ((h & 0xffffu) << 16);
}

[ForceInline]
void vkq_run_flush_u16(uint* packed, uint c1, uint acc) {
    if ((c1 & 1u) == 1u)
        vkq_store_u16(packed, c1 - 1u, acc);  // tail cell of a shared word
}

// ---------------------------------------------------------------------------
// gradq::Codec<BITS> — signed symmetric gradient codec (int8/int16 cells,
// codes in [-qmax, qmax] scaled by the block amplitude; code 0 decodes to
// exactly 0). Mirrors core/GradQuant.cuh.
// ---------------------------------------------------------------------------

[ForceInline]
float gradq_amp(float2 mm) { return max(abs(mm.x), abs(mm.y)); }

[ForceInline]
float gradq_qmax<let BITS : int>() { return BITS == 16 ? 32767.0f : 127.0f; }

[ForceInline]
float gradq_decode1<let BITS : int>(uint* packed, uint cell, float2 mm) {
    int s;
    if (BITS == 16) {
        uint w = packed[cell >> 1];
        uint h = (w >> (16u * (cell & 1u))) & 0xffffu;
        s = (int(h) << 16) >> 16;  // sign-extend int16
    } else {
        s = s8_load(packed, cell);
    }
    return float(s) * (gradq_amp(mm) * (1.0f / gradq_qmax<BITS>()));
}

[ForceInline]
float gradq8_decode1(uint* packed, uint cell, float2 mm) {
    return gradq_decode1<8>(packed, cell, mm);
}

// Round-half-even, matching CUDA rintf in gradq::Codec::encode1.
[ForceInline]
float _gq_rne(float x) {
    float f = floor(x);
    float d = x - f;
    if (d < 0.5f) return f;
    if (d > 0.5f) return f + 1.0f;
    return (f - 2.0f * floor(0.5f * f)) == 0.0f ? f : f + 1.0f;
}

// Encode one value to its signed code bits (caller stores via vkq_store_*).
[ForceInline]
uint gradq_encode1<let BITS : int>(float v, float2 mm) {
    float a = gradq_amp(mm);
    float qmax = gradq_qmax<BITS>();
    float qf = (a > 0.0f) ? _gq_rne(v * (qmax / a)) : 0.0f;
    qf = clamp(qf, -qmax, qmax);
    int q = int(qf);
    return uint(q) & (BITS == 16 ? 0xffffu : 0xffu);
}

// ---------------------------------------------------------------------------
// Non-SH attribute Adam-state IO shared by the FPBO and fused-3DGS-geometry
// optimizer kernels: fp32 g1/g2 buffers or the 16-bit qadam codec
// (word/cell, per-splat run at base PRIMS*gid, one bound per 256 splats).
// ---------------------------------------------------------------------------

[ForceInline]
float3 oq_sqrt3(float3 v) {
    return float3(sqrt(max(v.x, 0.0f)), sqrt(max(v.y, 0.0f)),
                  sqrt(max(v.z, 0.0f)));
}
[ForceInline]
float4 oq_sqrt4(float4 v) {
    return float4(sqrt(max(v.x, 0.0f)), sqrt(max(v.y, 0.0f)),
                  sqrt(max(v.z, 0.0f)), sqrt(max(v.w, 0.0f)));
}

[ForceInline]
void oq_state_read<let PRIMS : int>(bool quant, uint* packed, float4* bounds,
                                    float* g1_buf, float* g2_buf,
                                    uint block_id, uint gid,
                                    out float g1[PRIMS],
                                    out float g2[PRIMS]) {
    if (quant) {
        float4 mm_old = bounds[block_id];
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++) {
            float2 ab = qadam_decode_g1g2<16>(packed,
                                              uint(PRIMS) * gid + uint(i),
                                              mm_old);
            g1[i] = ab.x;
            g2[i] = ab.y;
        }
    } else {
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++) {
            g1[i] = g1_buf[PRIMS * gid + i];
            g2[i] = g2_buf[PRIMS * gid + i];
        }
    }
}

[ForceInline]
void oq_accum_us<let PRIMS : int>(float g1[PRIMS], float g2[PRIMS],
                                  inout float4 mm) {
    [ForceUnroll]
    for (int i = 0; i < PRIMS; i++) {
        float2 us = qadam_g1g2_to_us(g1[i], g2[i]);
        mm.x = min(mm.x, us.x);
        mm.y = max(mm.y, us.x);
        mm.z = min(mm.z, us.y);
        mm.w = max(mm.w, us.y);
    }
}

[ForceInline]
void oq_state_encode<let PRIMS : int>(uint* packed, uint gid, float g1[PRIMS],
                                      float g2[PRIMS], float4 mm) {
    [ForceUnroll]
    for (int i = 0; i < PRIMS; i++) {
        float2 us = qadam_g1g2_to_us(g1[i], g2[i]);
        uint2 q = qadam_quantize_us<16>(us.x, us.y, mm);
        packed[uint(PRIMS) * gid + uint(i)] = q.x | (q.y << 16);
    }
}
