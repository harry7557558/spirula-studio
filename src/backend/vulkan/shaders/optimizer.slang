// Optimizer kernels (Vulkan backend), mirroring the csrc optimizer family (AdamOptim/Newton/Fused*/TrustRegion*/ColorOptim): fused
// Adam (fp32, quantized-state, doubly-quantized state+value), AdaGrad, and
// the small utility kernels (float_add_into, increment_int32). The
// quantized kernels keep CUDA's launch geometry — one 256-thread workgroup
// per 256-cell quant block, bounds indexed by the flat workgroup id — with
// the usual wgs_per_row grid fold. CUDA's warp-shuffle block reductions
// become the deterministic groupshared reduction in optim_quant.slang, and
// sub-word packed cells are stored via its cooperative word assembly.
//
// The quantized kernels contain barriers, so out-of-range workgroups mask
// their memory accesses with `inside`/`blk_ok` instead of returning early —
// every thread reaches every barrier.

#include "optim_quant.slang"

[SpecializationConstant]
const int kOptimBits = 8;   // QuantizedAdamState bits: 4 or 8
[SpecializationConstant]
const int kValueBits = 8;   // QuantizedTensor value bits: 8 or 16

static const float kBeta1 = 0.9f;
static const float kBeta2 = 0.999f;

[ForceInline]
bool _finitef(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

// ---------------------------------------------------------------------------
// fused_adam_step (fp32 state)
// ---------------------------------------------------------------------------

struct FusedAdamParams {  // 80 bytes: pushed directly
    float* param;
    float* grad;
    float* exp_avg;
    float* exp_avg_sq;
    int* steps;          // per-splat steps (has_steps gates; fallback-backed)
    float lr;
    float decay;         // 2 * l2_reg / numel (host-computed)
    float decay_offset;
    float grad_scale;
    int scalar_step;
    uint has_steps;
    uint numel;
    uint stride;         // param stride per splat (steps index = idx/stride)
    uint zero_grad;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void fused_adam_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform FusedAdamParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.numel) return;
    float step = float(p.has_steps == 0 ? p.scalar_step
                                         : p.steps[idx / p.stride]);
    float ibc1 = 1.0f / (1.0f - pow(kBeta1, step));
    float ibc2 = 1.0f / (1.0f - pow(kBeta2, step));

    float x = p.param[idx];
    float v = p.grad[idx];
    if (!_finitef(v)) v = 0.0f;
    v *= p.grad_scale;
    v += p.decay *
         (max(x - p.decay_offset, 0.0f) + min(x + p.decay_offset, 0.0f));
    float g1 = p.exp_avg[idx];
    float g2 = p.exp_avg_sq[idx];
    // Self-heal: a moment that is already non-finite would otherwise pin this
    // parameter for the rest of the run.
    if (!_finitef(g1)) g1 = 0.0f;
    if (!_finitef(g2)) g2 = 0.0f;

    g1 = kBeta1 * g1 + (1.0f - kBeta1) * v;
    g2 = kBeta2 * g2 + (1.0f - kBeta2) * v * v;

    float x_new = x - p.lr * ibc1 * g1 / (sqrt(g2 * ibc2) + kAdamEps);

    // A finite parameter must not become non-finite: for the per-photo
    // appearance tables (PPISP, bilagrid) one such entry poisons every pixel
    // of that photo for the rest of the run, and nothing ever restores it.
    if (_finitef(x_new)) p.param[idx] = x_new;
    if (_finitef(g1)) p.exp_avg[idx] = g1;
    if (_finitef(g2)) p.exp_avg_sq[idx] = g2;

    if (p.zero_grad != 0) p.grad[idx] = 0.0f;
}

// ---------------------------------------------------------------------------
// fused_adagrad_step
// ---------------------------------------------------------------------------

struct AdagradParams {  // 36 bytes: pushed directly
    float* param;
    float* grad;
    float* accum;
    float lr;
    uint numel;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void fused_adagrad_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform AdagradParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.numel) return;
    float v = p.grad[idx];
    if (!_finitef(v)) v = 0.0f;
    float a0 = p.accum[idx];
    if (!_finitef(a0)) a0 = 0.0f;   // self-heal, see fused_adam_fwd
    float a = a0 + v * v;
    if (!_finitef(a)) a = a0;
    // See fused_adam_fwd: a finite parameter must not become non-finite.
    float x_new = p.param[idx] - p.lr * v / (sqrt(a) + kAdamEps);
    if (_finitef(x_new)) p.param[idx] = x_new;
    p.accum[idx] = a;
}

// ---------------------------------------------------------------------------
// fused_adam_step_quantized (QuantizedAdamState<kOptimBits, 256>)
// ---------------------------------------------------------------------------

struct QAdamParams {  // 88 bytes: pushed directly
    float* param;
    float* grad;
    uint* packed;          // AoS (u, log_s) cells
    float4* quant_bounds;  // per-block (u_min, u_max, log_s_min, log_s_max)
    int* steps;            // has_steps gates; fallback-backed
    float lr;
    float decay;
    float decay_offset;
    float grad_scale;
    int scalar_step;
    uint has_steps;
    uint numel;
    uint stride;
    uint zero_grad;
    uint num_blocks;       // ceil(numel / 256); grid may be folded larger
    uint wgs_per_row;
    uint _pad0;
};

[ForceInline]
void _adam_q_impl<let BITS : int>(uint3 wg, uint tid, QAdamParams p) {
    uint blk = wg.y * p.wgs_per_row + wg.x;
    bool blk_ok = blk < p.num_blocks;
    uint idx = blk * 256 + tid;
    bool inside = blk_ok && idx < p.numel;

    float step = float(p.has_steps == 0
                           ? p.scalar_step
                           : p.steps[min(idx, p.numel - 1) / p.stride]);
    float ibc1 = 1.0f / (1.0f - pow(kBeta1, step));
    float ibc2 = 1.0f / (1.0f - pow(kBeta2, step));

    float x = inside ? p.param[idx] : 0.0f;
    float v = inside ? p.grad[idx] : 0.0f;
    if (!_finitef(v)) v = 0.0f;
    v *= p.grad_scale;
    v += p.decay *
         (max(x - p.decay_offset, 0.0f) + min(x + p.decay_offset, 0.0f));

    float4 mm = blk_ok ? p.quant_bounds[blk] : float4(0.0f);
    float g1 = 0.0f, g2 = 0.0f;
    if (inside) {
        float2 g = qadam_decode_g1g2<BITS>(p.packed, idx, mm);
        g1 = g.x;
        g2 = g.y;
    }

    g1 = kBeta1 * g1 + (1.0f - kBeta1) * v;
    g2 = kBeta2 * g2 + (1.0f - kBeta2) * v * v;

    x -= p.lr * ibc1 * g1 / (sqrt(g2 * ibc2) + kAdamEps);
    if (inside) p.param[idx] = x;

    float2 us = qadam_g1g2_to_us(g1, g2);
    float4 red = inside ? float4(us.x, us.x, us.y, us.y)
                        : float4(1e30f, -1e30f, 1e30f, -1e30f);
    mm = oq_block_reduce_mm4(red, tid);

    uint2 q = qadam_quantize_us<BITS>(us.x, us.y, mm);
    qadam_store<BITS>(p.packed, idx, tid, inside, q);
    if (tid == 0 && blk_ok) p.quant_bounds[blk] = mm;

    // Last, per fused_adam_fwd.
    if (p.zero_grad != 0 && inside) p.grad[idx] = 0.0f;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void fused_adam_q(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                  uniform QAdamParams p) {
    if (kOptimBits == 4) _adam_q_impl<4>(wg, tid, p);
    else _adam_q_impl<8>(wg, tid, p);
}

// ---------------------------------------------------------------------------
// fused_adam_step_quantized_value — quantized optimizer state AND parameter
// value; optionally reads the block-quantized SH gradient (gradq codec).
// ---------------------------------------------------------------------------

struct QQAdamParams {  // 112 bytes: pushed directly
    float* grad;            // fp32 dense; null under grad-quant
    uint* grad_q_packed;    // int8 codes; null keeps the fp32 read
    float2* grad_q_bounds;  // per-splat-block bounds, index (idx/stride)/256
    uint* optim_packed;
    float4* optim_bounds;
    uint* value_packed;
    float2* value_bounds;
    int* steps;             // has_steps gates; fallback-backed
    float lr;
    float decay;
    float decay_offset;
    float grad_scale;
    int scalar_step;
    uint has_steps;
    uint grad_quant;        // 1: decode grad_q_* ; 0: read fp32 grad
    uint numel;
    uint stride;
    uint zero_grad;
    uint num_blocks;
    uint wgs_per_row;
};

[ForceInline]
void _adam_qq_impl<let OPTIM_BITS : int, let VALUE_BITS : int>(
    uint3 wg, uint tid, QQAdamParams p) {
    uint blk = wg.y * p.wgs_per_row + wg.x;
    bool blk_ok = blk < p.num_blocks;
    uint idx = blk * 256 + tid;
    bool inside = blk_ok && idx < p.numel;

    float step = float(p.has_steps == 0
                           ? p.scalar_step
                           : p.steps[min(idx, p.numel - 1) / p.stride]);
    float ibc1 = 1.0f / (1.0f - pow(kBeta1, step));
    float ibc2 = 1.0f / (1.0f - pow(kBeta2, step));

    float2 value_mm_old = inside ? p.value_bounds[blk] : float2(0.0f);
    float x = inside ? qtensor_decode_v<VALUE_BITS>(p.value_packed, idx,
                                                    value_mm_old)
                     : 0.0f;

    float v;
    if (p.grad_quant != 0) {
        float2 gmm =
            inside ? p.grad_q_bounds[(idx / p.stride) / 256] : float2(0.0f);
        v = inside ? gradq8_decode1(p.grad_q_packed, idx, gmm) : 0.0f;
    } else {
        v = inside ? p.grad[idx] : 0.0f;
    }
    if (!_finitef(v)) v = 0.0f;
    v *= p.grad_scale;
    v += p.decay *
         (max(x - p.decay_offset, 0.0f) + min(x + p.decay_offset, 0.0f));

    float4 optim_mm = blk_ok ? p.optim_bounds[blk] : float4(0.0f);
    float g1 = 0.0f, g2 = 0.0f;
    if (inside) {
        float2 g =
            qadam_decode_g1g2<OPTIM_BITS>(p.optim_packed, idx, optim_mm);
        g1 = g.x;
        g2 = g.y;
    }

    g1 = kBeta1 * g1 + (1.0f - kBeta1) * v;
    g2 = kBeta2 * g2 + (1.0f - kBeta2) * v * v;

    x -= p.lr * ibc1 * g1 / (sqrt(g2 * ibc2) + kAdamEps);

    float2 value_mm =
        oq_block_reduce_mm2(inside ? float2(x, x) : float2(1e30f, -1e30f),
                            tid);

    float2 us = qadam_g1g2_to_us(g1, g2);
    float4 optim_mm_new = oq_block_reduce_mm4(
        inside ? float4(us.x, us.x, us.y, us.y)
               : float4(1e30f, -1e30f, 1e30f, -1e30f),
        tid);

    qtensor_store<VALUE_BITS>(p.value_packed, idx, tid, inside,
                              qtensor_quantize_v<VALUE_BITS>(x, value_mm));
    qadam_store<OPTIM_BITS>(p.optim_packed, idx, tid, inside,
                            qadam_quantize_us<OPTIM_BITS>(us.x, us.y,
                                                          optim_mm_new));
    if (tid == 0 && blk_ok) {
        p.value_bounds[blk] = value_mm;
        p.optim_bounds[blk] = optim_mm_new;
    }

    // Last, per fused_adam_fwd.
    if (p.zero_grad != 0 && inside && p.grad_quant == 0) p.grad[idx] = 0.0f;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void fused_adam_qq(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform QQAdamParams p) {
    if (kOptimBits == 4) {
        if (kValueBits == 8) _adam_qq_impl<4, 8>(wg, tid, p);
        else _adam_qq_impl<4, 16>(wg, tid, p);
    } else {
        if (kValueBits == 8) _adam_qq_impl<8, 8>(wg, tid, p);
        else _adam_qq_impl<8, 16>(wg, tid, p);
    }
}

// ---------------------------------------------------------------------------
// Utility kernels
// ---------------------------------------------------------------------------

struct FloatAddParams {  // 24 bytes: pushed directly
    float* dst;
    float* src;
    uint n;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void float_add(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
               uniform FloatAddParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx < p.n) p.dst[idx] += p.src[idx];
}

[shader("compute")]
[numthreads(256, 1, 1)]
void float_max(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
               uniform FloatAddParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx < p.n) p.dst[idx] = max(p.dst[idx], p.src[idx]);
}

struct IncI32Params {  // 16 bytes: pushed directly
    int* data;
    uint n;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void increment_i32(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform IncI32Params p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx < p.n) p.data[idx] += 1;
}
