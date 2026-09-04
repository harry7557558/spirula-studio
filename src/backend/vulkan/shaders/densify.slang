// Densification kernels (Vulkan backend), mirroring the csrc densify family (DensifySampling/Scoring/Relocation/Mcmc/SplitFilter):
// densify_update_weight (per-step score accumulator), densify_clip_scale,
// the long-axis-split relocate/add family, the MCMC relocate/add family,
// and the MCMC/revised noise kernels. The split/noise math is the canonical
// shaders/densify.slang device code; the mcmc_relocation opacity/scale math
// is CPU-side C++ in the csrc densify family and is mirrored here.
//
// Score modes match DensifyScoreMode in kernels/densify/Densify.cuh:
//   0 = Mean, 1 = Max, 2 = Median, 3 = Geom.
// The Median mode's multiplicative random walk uses hash14 (shadertoy
// 4djSRW); its float arithmetic is contraction-sensitive, so CUDA and
// Vulkan can draw different hashes for the same input — parity tests
// compare only the deterministic count channel for Median. The same caveat
// applies to the randn3-based noise kernels and to the float sampling keys
// (efraimidis / MCMC cumsum draws): integer hashes match bit-exactly, but
// the float post-processing can flip rare boundary cases cross-backend, so
// the parity tests use small violation-fraction caps there.
//
// Sub-word quantized-state writes (SH optim-state zeroing, SH value-codec
// copies) go through the And+Or masked stores in optim_quant.slang: a
// relocated splat's cell run need not be word-aligned, and two dst splats
// can share a boundary word.

#define CudaDeviceExport ForceInline
#include "shaders/densify.slang"
#undef CudaDeviceExport
#include "optim_quant.slang"
#include "sh_addr.slang"

[ForceInline]
float _dn_hash14(float4 p4) {
    p4 = p4 * float4(0.1031f, 0.1030f, 0.0973f, 0.1099f);
    p4 = float4(fmod(p4.x, 1.0f), fmod(p4.y, 1.0f), fmod(p4.z, 1.0f),
                fmod(p4.w, 1.0f));
    p4 = p4 + dot(p4, p4.wzxy + 33.33f);
    return fmod((p4.x + p4.y) * (p4.z + p4.w), 1.0f);
}

[ForceInline]
float _dn_sigmoid(float x) { return 1.0f / (1.0f + exp(-x)); }

[ForceInline]
bool _dn_finitef(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

// Optional inputs are flag-gated (use_*) with fallback-backed pointers —
// never null-pointer-gated (see KernelCommon.h null_fallback). The CUDA
// kernel's accum_weight_scalar input is dropped: the launch API always
// passes null for it.
struct DensifyUpdateWeightParams {  // 88 bytes: pushed directly
    float* radii;         // [N]
    float* opacs;         // [N] logit-space (use_opacs gates)
    float* accum_weight;  // [N]
    float* accum_weight2; // [N] blend partner (use_w2 gates)
    float2* accum_buffer; // [N, 2] (score, count)
    float* oversize_accum;  // [N] summed log2 oversize (use_oversize gates)
    float blend_w;
    float score_power;
    int score_mode;
    uint use_opacs;
    uint use_w2;
    uint num_splats;
    uint wgs_per_row;
    float max_scale2d;
    uint use_oversize;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_update_weight(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                           uniform DensifyUpdateWeightParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    if (!(p.radii[idx] > 0.0f)) return;

    if (p.use_oversize != 0 && p.radii[idx] > p.max_scale2d)
        p.oversize_accum[idx] += log2(p.radii[idx] / p.max_scale2d);

    float weight = abs(p.accum_weight[idx]);
    // Geometric blend: weight^(1-w) * |accum_weight2|^w (see DensifyScoring.cu).
    if (p.use_w2 != 0) {
        weight = pow(weight, 1.0f - p.blend_w) *
                 pow(abs(p.accum_weight2[idx]), p.blend_w);
        if (!_dn_finitef(weight)) weight = 0.0f;
    }
    if (p.use_opacs != 0) weight *= _dn_sigmoid(p.opacs[idx]);
    if (weight == 0.0f) return;
    if (p.score_power != 1.0f) weight = pow(weight, p.score_power);

    float2 accum = p.accum_buffer[idx];
    if (p.score_mode == 1) {  // Max
        accum.x = max(accum.x, weight);
        accum.y = max(accum.y, 1.0f);
    } else if (p.score_mode == 0) {  // Mean
        accum.x = (accum.x * accum.y + weight) / (accum.y + 1.0f);
        accum.y += 1.0f;
    } else if (p.score_mode == 2) {  // Median
        float rand = _dn_hash14(
            1e2f * float4(weight, accum.x, accum.y, accum.y + 1.5f));
        if (accum.y == 0.0f) {
            accum.x = weight * exp2(rand - 0.5f);
            accum.y = 1.0f;
        } else if (weight != 0.0f) {
            float s = weight > accum.x ? 1.0f : -1.0f;
            accum.x *= exp2(s * rand);
            accum.y += 1.0f;
        }
    } else if (p.score_mode == 3) {  // Geom
        accum.x = exp((log(max(accum.x, 1e-30f)) * accum.y +
                       log(max(weight, 1e-30f))) /
                      (accum.y + 1.0f));
        accum.y += 1.0f;
    }
    p.accum_buffer[idx] = accum;
}

// ---------------------------------------------------------------------------
// densify_oversize_weight
// ---------------------------------------------------------------------------

struct DensifyOversizeWeightParams {  // 32 bytes: pushed directly
    float* oversize_accum;  // [N]
    float2* score;          // [N, 2], lane 0 = error score
    float2* out;            // [N, 2], lane 0 = weight
    float blend;
    uint num_splats;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_oversize_weight(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform DensifyOversizeWeightParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    const float kOversizeFloor = 1e-3f;
    float s = max(p.score[idx].x, 0.0f);
    float w = p.oversize_accum[idx] + kOversizeFloor;
    if (p.blend > 0.0f)
        w *= (p.blend == 1.0f) ? s : pow(s, p.blend);
    p.out[idx] = float2(_dn_finitef(w) ? w : 0.0f, 0.0f);
}


// ---------------------------------------------------------------------------
// densify_clip_scale
// ---------------------------------------------------------------------------

struct DensifyClipScaleParams {  // 48 bytes: pushed directly
    float* radii;        // [N]
    float* log_scales;   // [N, 3]
    float* logit_opacs;  // [N] (has_opacs gates; fallback-backed)
    float max_scale2d;
    float clip_hardness;
    float max_scale3d;
    uint has_opacs;
    uint num_splats;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_clip_scale(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform DensifyClipScaleParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;

    if (_dn_finitef(p.max_scale2d)) {
        float oversize_factor = p.radii[idx] / p.max_scale2d;
        if (_dn_finitef(p.clip_hardness))
            oversize_factor = min(oversize_factor, p.clip_hardness);
        if (oversize_factor > 1.0f) {
            oversize_factor = log(oversize_factor);
            p.log_scales[3 * idx] -= oversize_factor;
            p.log_scales[3 * idx + 1] -= oversize_factor;
            p.log_scales[3 * idx + 2] -= oversize_factor;
            // encourages being relocated to (see DensifyScoring.cu)
            if (p.has_opacs != 0) {
                float lo = p.logit_opacs[idx];
                p.logit_opacs[idx] =
                    min(lo + 3.0f * oversize_factor, max(lo, 5.0f));
            }
        }
    }

    if (_dn_finitef(p.max_scale3d)) {
        float m = log(p.max_scale3d);
        p.log_scales[3 * idx] = min(p.log_scales[3 * idx], m);
        p.log_scales[3 * idx + 1] = min(p.log_scales[3 * idx + 1], m);
        p.log_scales[3 * idx + 2] = min(p.log_scales[3 * idx + 2], m);
        // don't touch opacity
    }
}

// ---------------------------------------------------------------------------
// MCMC / revised noise (canonical device code from ../densify.slang)
// ---------------------------------------------------------------------------

struct DensifyNoiseParams {  // 48 bytes: pushed directly
    float* means;       // [N, 3] updated in place
    float* log_scales;  // [N, 3]
    float* quats;       // [N, 4]
    float* logit_opacs; // [N]
    float* radii;       // [N] (revised only; fallback-backed for mcmc)
    float scaler;
    uint num_splats;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_add_noise(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform DensifyNoiseParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    float3 mean = float3(p.means[3 * idx], p.means[3 * idx + 1],
                         p.means[3 * idx + 2]);
    float3 log_scale = float3(p.log_scales[3 * idx], p.log_scales[3 * idx + 1],
                              p.log_scales[3 * idx + 2]);
    float4 quat = float4(p.quats[4 * idx], p.quats[4 * idx + 1],
                         p.quats[4 * idx + 2], p.quats[4 * idx + 3]);
    mcmc_add_noise_3dgs(p.scaler, mean, log_scale, quat,
                        p.logit_opacs[idx]);
    p.means[3 * idx] = mean.x;
    p.means[3 * idx + 1] = mean.y;
    p.means[3 * idx + 2] = mean.z;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_revised_add_noise(uint3 wg: SV_GroupID,
                               uint tid: SV_GroupThreadID,
                               uniform DensifyNoiseParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    float3 mean = float3(p.means[3 * idx], p.means[3 * idx + 1],
                         p.means[3 * idx + 2]);
    float3 log_scale = float3(p.log_scales[3 * idx], p.log_scales[3 * idx + 1],
                              p.log_scales[3 * idx + 2]);
    float4 quat = float4(p.quats[4 * idx], p.quats[4 * idx + 1],
                         p.quats[4 * idx + 2], p.quats[4 * idx + 3]);
    revised_add_noise_3dgs(p.scaler, p.radii[idx], mean, log_scale, quat,
                           p.logit_opacs[idx]);
    p.means[3 * idx] = mean.x;
    p.means[3 * idx + 1] = mean.y;
    p.means[3 * idx + 2] = mean.z;
}

// ---------------------------------------------------------------------------
// Shared helpers for the relocation / MCMC kernels
// ---------------------------------------------------------------------------

// Integer hash matching hash_uint3 in core/Common.cuh bit-exactly.
[ForceInline]
uint _dn_hash_u3(uint a, uint b, uint c) {
    uint hash = a;
    hash *= 0x01000193u;
    hash = (hash << 16) | (hash >> 16);
    hash ^= b;
    hash *= 0x01000193u;
    hash = (hash << 16) | (hash >> 16);
    hash ^= c;
    hash *= 0x01000193u;
    hash ^= hash >> 15;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16;
    return hash;
}

// Spec axes shared by the relocate/MCMC kernels.
//   kShOptimBits:  0 = fp32 SH Adam state, 8 / 4 = QuantizedAdamState bits
//   kShValueBits:  8 / 16 (densify_copy_qsh_* only)
[SpecializationConstant]
const int kShOptimBits = 0;
[SpecializationConstant]
const int kShValueBits = 16;

// Zero-encode codes against a [lo, hi] block bound (see DensifyQuantCopy.cuh
// _quant_encode_zero_byte / _quant_encode_zero_word16: saturating is
// acceptable, the next optim pass recomputes bounds).
[ForceInline]
uint _dn_qzero<let QMAX : int>(float lo, float hi) {
    float range = max(hi - lo, 1e-30f);
    float qf = _oq_round(float(QMAX) * (0.0f - lo) / range);
    return uint(clamp(qf, 0.0f, float(QMAX)));
}

// Encode (g1 = 0, g2 = 0) into one splat's SH optim-state cells
// (kShOptimBits = 8: halfword/cell; 4: byte/cell). Masked atomic stores:
// a splat's cell run need not be word-aligned and neighbors may write the
// boundary words concurrently.
[ForceInline]
void _dn_zero_qsh_splat(uint* packed, uint splat, uint num_sh,
                        float4* bounds, bool bounds_per_splat) {
    uint cells = 3u * num_sh;
    ShAddrW a = sh_quant_addr_w(num_sh, bounds_per_splat ? 0 : 256);
    gindex_t base = sh_addr_base(a, splat);
    if (bounds_per_splat) {
        float4 mm = bounds[splat / 256u];
        uint u_q, s_q;
        if (kShOptimBits == 8) {
            u_q = _dn_qzero<255>(mm.x, mm.y);
            s_q = _dn_qzero<255>(mm.z, mm.w);
            for (uint c = 0; c < cells; c++)
                vkq_store_u16(packed,
                              sh_cell_at_w(base, int(c), a.pair_pitch),
                              u_q | (s_q << 8));
        } else {
            u_q = _dn_qzero<15>(mm.x, mm.y);
            s_q = _dn_qzero<15>(mm.z, mm.w);
            for (uint c = 0; c < cells; c++)
                vkq_store_u8(packed,
                             sh_cell_at_w(base, int(c), a.pair_pitch),
                             (u_q & 0xfu) | ((s_q & 0xfu) << 4));
        }
    } else {
        for (uint c = 0; c < cells; c++) {
            gindex_t cell = base + (gindex_t)c;
            float4 mm = bounds[uint(cell >> 8)];
            if (kShOptimBits == 8) {
                uint u_q = _dn_qzero<255>(mm.x, mm.y);
                uint s_q = _dn_qzero<255>(mm.z, mm.w);
                vkq_store_u16(packed, cell, u_q | (s_q << 8));
            } else {
                uint u_q = _dn_qzero<15>(mm.x, mm.y);
                uint s_q = _dn_qzero<15>(mm.z, mm.w);
                vkq_store_u8(packed, cell,
                             (u_q & 0xfu) | ((s_q & 0xfu) << 4));
            }
        }
    }
}

// Encode (g1 = 0, g2 = 0) into one splat's non-SH 16-bit qadam cells
// (word/cell — whole-word stores, no atomics needed).
[ForceInline]
void _dn_zero_nq_splat<let PRIMS : int>(uint* packed, float4* bounds,
                                        uint splat) {
    float4 mm = bounds[splat / 256u];
    uint u_q = _dn_qzero<65535>(mm.x, mm.y);
    uint s_q = _dn_qzero<65535>(mm.z, mm.w);
    uint w = u_q | (s_q << 16);
    [ForceUnroll]
    for (int i = 0; i < PRIMS; i++)
        packed[uint(PRIMS) * splat + uint(i)] = w;
}

// mcmc_relocation opacity/scale math, mirroring McmcRelocation.cu (which mirrors
// the MCMC paper's relocation formula; log_factorial is the least-squares
// fit valid to n = 50).
[ForceInline]
float _dn_log_factorial(float x) {
    const float kA = 0.996763591291f;
    const float kH = 2.73507778369f;
    const float kB = 0.978531458767f;
    const float kC2 = -0.0400131099548f;
    const float kC1 = -0.596969060666f;
    const float kD3 = 0.00251628691783f;
    const float kD2 = 0.0959712063178f;
    const float kD1 = 0.803207449544f;
    return (kA * log(x + kH) - kB +
            (kC2 * x + kC1) / (((kD3 * x + kD2) * x + kD1) * x + 1.0f)) * x;
}

[ForceInline]
float _dn_binom(float n, float k) {
    return exp(_dn_log_factorial(n) - _dn_log_factorial(k) -
               _dn_log_factorial(n - k));
}

[ForceInline]
void _dn_mcmc_relocation(inout float opacity, inout float3 scale, int n_idx) {
    n_idx = min(n_idx, 50);  // log_factorial only fits to 50

    opacity = _dn_sigmoid(opacity);

    float new_opacity = 1.0f - pow(1.0f - opacity, 1.0f / float(n_idx));

    float denom_sum = 0.0f;
    for (int i = 1; i <= n_idx; ++i) {
        for (int k = 0; k <= (i - 1); ++k) {
            denom_sum += _dn_binom(float(i - 1), float(k)) *
                         (cos(3.14159265358979323846f * float(k)) /
                          sqrt(float(k + 1))) *  // (-1)^k / sqrt(k+1)
                         pow(new_opacity, float(k + 1));
        }
    }
    // log(coeff * exp(s)) == log(coeff) + s, kept in log space: the round trip
    // out and back flushes a splat near FLT_MIN to -inf, which per_splat_losses
    // then turns into a NaN (shaders/per_splat_losses.slang).
    float log_coeff = log(opacity / denom_sum);

    opacity = log(new_opacity / (1.0f - new_opacity));  // logit
    scale = scale + log_coeff;
}

// ---------------------------------------------------------------------------
// Weighted sampling without replacement: efraimidis-spirakis keys. The float
// key w / log(1 - u) is emitted through the order-preserving uint32 flip so
// the raw-unsigned radix sort (sort_scan.slang) reproduces CUB's float-key
// ascending order bit-exactly for identical key bits.
// ---------------------------------------------------------------------------

struct EfraimidisParams {  // 52 bytes: pushed directly
    float* weights;   // [numel * stride]
    int* mask;        // [numel] int32 0/1 (use_mask gates; fallback-backed)
    uint* out_keys;   // [numel] sortable key bits
    uint stride;
    uint seed;
    uint use_mask;
    uint numel;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_efraimidis_keys(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform EfraimidisParams p) {
    uint block_lin = wg.y * p.wgs_per_row + wg.x;
    uint idx = block_lin * 256 + tid;
    if (idx >= p.numel) return;

    float u = float(_dn_hash_u3(p.seed, block_lin, tid)) * exp2(-32.0f);
    float w = p.weights[idx * p.stride];
    // larger w -> smaller (more negative) value
    w = w / log(max(1.0f - u, 1e-30f));
    if (p.use_mask != 0 && p.mask[idx] == 0) w = 0.0f;

    // float bits -> order-preserving unsigned key
    uint b = asuint(w);
    b ^= (b >> 31) != 0 ? 0xffffffffu : 0x80000000u;
    p.out_keys[idx] = b;
}

// ---------------------------------------------------------------------------
// Long-axis-split relocation mask (which splats get replaced) + compaction.
// The dst-index order comes from the atomic counter and is scheduling-
// dependent, exactly like the CUDA kernel — the src->dst PAIRING is
// arbitrary; parity tests compare relocated rows as a multiset.
// ---------------------------------------------------------------------------

struct RelocMaskParams {  // 80 bytes: pushed directly
    float* means;        // [N, 3]
    float* quats;        // [N, 4]
    float* scales;       // [N, 3]
    float* opacs;        // [N]
    float* features_dc;  // [N, 3]
    int* mask;           // [N] out: 1 = keep (samplable), 0 = relocated
    int* count;          // [1] out: number of relocated splats
    int* dst_indices;    // [N] out: compacted relocated indices
    float min_opacity;
    uint num_splats;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_reloc_mask(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform RelocMaskParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;

    float3 mean = float3(p.means[3 * idx], p.means[3 * idx + 1],
                         p.means[3 * idx + 2]);
    float4 quat = float4(p.quats[4 * idx], p.quats[4 * idx + 1],
                         p.quats[4 * idx + 2], p.quats[4 * idx + 3]);
    float3 scale = float3(p.scales[3 * idx], p.scales[3 * idx + 1],
                          p.scales[3 * idx + 2]);
    float opac = p.opacs[idx];
    float3 dc = float3(p.features_dc[3 * idx], p.features_dc[3 * idx + 1],
                       p.features_dc[3 * idx + 2]);

    bool is_low_opac = _dn_sigmoid(opac) <= p.min_opacity;
    bool is_finite = _dn_finitef(dot(mean, mean) / dot(quat, quat) +
                                 dot(scale, dc) * opac);
    bool relocate = is_low_opac || !is_finite || splat_scale_is_dead(scale);

    p.mask[idx] = relocate ? 0 : 1;

    if (relocate) {
        int slot;
        InterlockedAdd(p.count[0], 1, slot);
        p.dst_indices[slot] = int(idx);
    }
}

// ---------------------------------------------------------------------------
// Long-axis-split relocate/add kernel (relocate_with_long_axis_split_kernel)
// ---------------------------------------------------------------------------

struct RelocLasParams {
    int* src_indices;    // [num_new]
    int* dst_indices;    // [num_new] (has_dst; else dst = cur_num + idx)
    float* means;        // [cap, 3]
    float* quats;        // [cap, 4]
    float* scales;       // [cap, 3]
    float* opacs;        // [cap]
    float* features_dc;  // [cap, 3]
    float* features_sh;  // [cap, K, 3] fp32 values (has_fp32_sh)
    // fp32 non-SH Adam state (has_g1)
    float* g1_means;
    float* g2_means;
    float* g1_quats;
    float* g2_quats;
    float* g1_scales;
    float* g2_scales;
    float* g1_opacs;
    float* g2_opacs;
    float* g1_dc;
    float* g2_dc;
    // SH Adam state: fp32 pair (kShOptimBits == 0) or packed codec bytes
    float* g1_sh;
    float* g2_sh;
    uint* sh_packed;
    float4* sh_quant_bounds;
    // non-SH quantized Adam state (non_sh)
    uint* nq_means_packed;
    uint* nq_quats_packed;
    uint* nq_scales_packed;
    uint* nq_opacities_packed;
    uint* nq_dc_packed;
    float4* nq_means_bounds;
    float4* nq_quats_bounds;
    float4* nq_scales_bounds;
    float4* nq_opacities_bounds;
    float4* nq_dc_bounds;
    float2* accum;       // [cap] densify accum buffer (has_accum)
    int* bias_steps;     // [cap] (has_bias_steps)
    float split_opacity_k;
    uint cur_num_splats;
    uint num_new_splats;
    uint num_sh;
    uint has_dst;
    uint has_g1;
    uint has_fp32_sh;
    uint has_accum;
    uint has_bias_steps;
    uint non_sh;
    uint sh_bounds_per_splat;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_relocate_las(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform RelocLasParams* pp) {
    RelocLasParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_new_splats) return;

    uint idx_src = uint(p.src_indices[idx]);
    uint idx_dst = p.has_dst != 0 ? uint(p.dst_indices[idx])
                                  : p.cur_num_splats + idx;

    // geometry - long axis split (canonical device code)
    float3 mean = float3(p.means[3 * idx_src], p.means[3 * idx_src + 1],
                         p.means[3 * idx_src + 2]);
    float3 scale = float3(p.scales[3 * idx_src], p.scales[3 * idx_src + 1],
                          p.scales[3 * idx_src + 2]);
    float4 quat = float4(p.quats[4 * idx_src], p.quats[4 * idx_src + 1],
                         p.quats[4 * idx_src + 2], p.quats[4 * idx_src + 3]);
    float opac = p.opacs[idx_src];
    float3 new_scale;
    float new_opac;
    float3 mean_delta;
    long_axis_split_3dgs(p.split_opacity_k, scale, opac, quat, new_scale,
                         new_opac, mean_delta);
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.means[3 * idx_src + i] = mean[i] - mean_delta[i];
        p.means[3 * idx_dst + i] = mean[i] + mean_delta[i];
        p.scales[3 * idx_src + i] = new_scale[i];
        p.scales[3 * idx_dst + i] = new_scale[i];
    }
    p.opacs[idx_src] = new_opac;
    p.opacs[idx_dst] = new_opac;
    [ForceUnroll]
    for (int i = 0; i < 4; i++)
        p.quats[4 * idx_dst + i] = quat[i];

    // appearance - copy
    [ForceUnroll]
    for (int i = 0; i < 3; i++)
        p.features_dc[3 * idx_dst + i] = p.features_dc[3 * idx_src + i];
    // fp32 SH values may live in the value-codec packed buffer instead
    // (copied by densify_copy_qsh_pairs); skip the fp32 copy then.
    if (p.has_fp32_sh != 0) {
        gindex_t R = (gindex_t)(3u * p.num_sh);
        float* sh_dst = elem_at(p.features_sh, R * (gindex_t)idx_dst);
        float* sh_src = elem_at(p.features_sh, R * (gindex_t)idx_src);
        for (uint i = 0; i < 3 * p.num_sh; i++)
            sh_dst[i] = sh_src[i];
    }

    // optimizer state - zero (dst)
    if (p.has_g1 != 0) {
        [ForceUnroll]
        for (int i = 0; i < 3; i++) {
            p.g1_means[3 * idx_dst + i] = 0.0f;
            p.g2_means[3 * idx_dst + i] = 0.0f;
            p.g1_scales[3 * idx_dst + i] = 0.0f;
            p.g2_scales[3 * idx_dst + i] = 0.0f;
            p.g1_dc[3 * idx_dst + i] = 0.0f;
            p.g2_dc[3 * idx_dst + i] = 0.0f;
        }
        [ForceUnroll]
        for (int i = 0; i < 4; i++) {
            p.g1_quats[4 * idx_dst + i] = 0.0f;
            p.g2_quats[4 * idx_dst + i] = 0.0f;
        }
        p.g1_opacs[idx_dst] = 0.0f;
        p.g2_opacs[idx_dst] = 0.0f;
    }
    if (p.non_sh != 0) {
        _dn_zero_nq_splat<3>(p.nq_means_packed, p.nq_means_bounds, idx_dst);
        _dn_zero_nq_splat<4>(p.nq_quats_packed, p.nq_quats_bounds, idx_dst);
        _dn_zero_nq_splat<3>(p.nq_scales_packed, p.nq_scales_bounds, idx_dst);
        _dn_zero_nq_splat<1>(p.nq_opacities_packed, p.nq_opacities_bounds,
                             idx_dst);
        _dn_zero_nq_splat<3>(p.nq_dc_packed, p.nq_dc_bounds, idx_dst);
    }
    if (kShOptimBits == 0) {
        gindex_t off = (gindex_t)(3u * p.num_sh) * (gindex_t)idx_dst;
        float* g1 = elem_at(p.g1_sh, off);
        float* g2 = elem_at(p.g2_sh, off);
        for (uint i = 0; i < 3 * p.num_sh; i++) {
            g1[i] = 0.0f;
            g2[i] = 0.0f;
        }
    } else {
        // g1 and g2 alias the same packed buffer; one pass covers both.
        _dn_zero_qsh_splat(p.sh_packed, idx_dst, p.num_sh, p.sh_quant_bounds,
                           p.sh_bounds_per_splat != 0);
    }
    if (p.has_bias_steps != 0) p.bias_steps[idx_dst] = 0;

    // will be cleared after densification anyway but doesn't hurt to do so
    if (p.has_accum != 0) p.accum[idx_dst] = p.accum[idx_src];
}

// ---------------------------------------------------------------------------
// SH VALUE-codec copies (decode src cells, encode into dst against dst's
// CURRENT bounds; clipping accepted — see DensifyQuantCopy.cuh).
// ---------------------------------------------------------------------------

[ForceInline]
void _dn_copy_qsh_value(uint* packed, float2* bounds, uint src_splat,
                        uint dst_splat, uint num_sh, uint num_sh_buffer,
                        bool bounds_per_splat) {
    // bounds_per_splat is the FPBO layout, whose cells are word-paired and
    // block-transposed (docs/notes/sh-quant-layout.md).
    ShAddrW a = sh_quant_addr_w(num_sh_buffer, bounds_per_splat ? 0 : 256);
    gindex_t src_base = sh_addr_base(a, src_splat);
    gindex_t dst_base = sh_addr_base(a, dst_splat);
    float2 src_mm = float2(0.0f, 0.0f), dst_mm = float2(0.0f, 0.0f);
    if (bounds_per_splat) {
        src_mm = bounds[src_splat / 256u];
        dst_mm = bounds[dst_splat / 256u];
    }
    uint cells = 3u * num_sh;
    for (uint c = 0; c < cells; c++) {
        gindex_t src_cell = sh_cell_at_w(src_base, int(c), a.pair_pitch);
        gindex_t dst_cell = sh_cell_at_w(dst_base, int(c), a.pair_pitch);
        if (!bounds_per_splat) {
            src_mm = bounds[uint(src_cell >> 8)];
            dst_mm = bounds[uint(dst_cell >> 8)];
        }
        if (kShValueBits == 16) {
            float v = qtensor_decode_v<16>(packed, src_cell, src_mm);
            vkq_store_u16(packed, dst_cell,
                          qtensor_quantize_v<16>(v, dst_mm));
        } else {
            float v = qtensor_decode_v<8>(packed, src_cell, src_mm);
            vkq_store_u8(packed, dst_cell, qtensor_quantize_v<8>(v, dst_mm));
        }
    }
}

struct CopyQshPairsParams {  // 64 bytes: pushed directly
    int* src_indices;  // [num_pairs]
    int* dst_indices;  // [num_pairs] (has_dst; else dst = cur_num + pair)
    uint* packed;
    float2* bounds;
    uint cur_num_splats;
    uint num_pairs;
    uint num_sh;
    uint num_sh_buffer;
    uint has_dst;
    uint bounds_per_splat;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_copy_qsh_pairs(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform CopyQshPairsParams p) {
    uint pair = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (pair >= p.num_pairs) return;
    uint src = uint(p.src_indices[pair]);
    uint dst = p.has_dst != 0 ? uint(p.dst_indices[pair])
                              : p.cur_num_splats + pair;
    _dn_copy_qsh_value(p.packed, p.bounds, src, dst, p.num_sh,
                       p.num_sh_buffer, p.bounds_per_splat != 0);
}

struct CopyQshMapParams {  // 48 bytes: pushed directly
    int* index_map;  // [num_splats]; copy only when index_map[dst] != dst
    uint* packed;
    float2* bounds;
    uint num_splats;
    uint num_sh;
    uint num_sh_buffer;
    uint bounds_per_splat;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_copy_qsh_map(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform CopyQshMapParams p) {
    uint dst = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (dst >= p.num_splats) return;
    uint src = uint(p.index_map[dst]);
    if (src == dst) return;
    _dn_copy_qsh_value(p.packed, p.bounds, src, dst, p.num_sh,
                       p.num_sh_buffer, p.bounds_per_splat != 0);
}

// ---------------------------------------------------------------------------
// MCMC relocation / add
// ---------------------------------------------------------------------------

struct McmcProbsParams {  // 40 bytes: pushed directly
    float* opacs;   // [N] logit-space
    float* scales;  // [N, 3] log-space
    float* probs;   // [N] out
    float min_opacity;
    uint num_splats;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_probs(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform McmcProbsParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    float opac = _dn_sigmoid(p.opacs[idx]);
    float3 scale = float3(p.scales[3 * idx], p.scales[3 * idx + 1],
                          p.scales[3 * idx + 2]);
    if (opac <= p.min_opacity || !_dn_finitef(opac) ||
        splat_scale_is_dead(scale))
        opac = 0.0f;
    p.probs[idx] = opac;
}

struct McmcRelocIndexMapParams {  // 48 bytes: pushed directly
    float* sample_probs;         // [N]
    float* sample_probs_cumsum;  // [N]
    int* index_map;              // [N] out
    int* n_idx_buffer;           // [N] atomically incremented
    uint numel;
    uint seed;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_reloc_index_map(uint3 wg: SV_GroupID,
                                  uint tid: SV_GroupThreadID,
                                  uniform McmcRelocIndexMapParams p) {
    uint block_lin = wg.y * p.wgs_per_row + wg.x;
    uint idx = block_lin * 256 + tid;
    if (idx >= p.numel) return;

    float prob = p.sample_probs[idx];

    // not relocated, put same ID
    if (prob != 0.0f) {
        p.index_map[idx] = int(idx);
        return;
    }

    // binary search: min i such that cumsum[i] >= rand
    float rand_max = p.sample_probs_cumsum[p.numel - 1];
    float rand = rand_max * float(_dn_hash_u3(p.seed, block_lin, tid)) *
                 exp2(-32.0f);
    uint lo = 0;
    uint hi = p.numel - 1;
    while (lo < hi) {
        uint mid = lo + ((hi - lo) >> 1);
        if (p.sample_probs_cumsum[mid] < rand)
            lo = mid + 1;
        else
            hi = mid;
    }
    p.index_map[idx] = int(lo);

    InterlockedAdd(p.n_idx_buffer[lo], 1);
}

struct McmcAddIndexMapParams {  // 48 bytes: pushed directly
    float* sample_probs_cumsum;  // [N]
    int* index_map;              // [num_add] out
    int* n_idx_buffer;           // [N] atomically incremented
    uint num_splats;
    uint num_add;
    uint seed;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_add_index_map(uint3 wg: SV_GroupID,
                                uint tid: SV_GroupThreadID,
                                uniform McmcAddIndexMapParams p) {
    uint block_lin = wg.y * p.wgs_per_row + wg.x;
    uint idx = block_lin * 256 + tid;
    if (idx >= p.num_add) return;

    // binary search (note <=, matching the CUDA add variant)
    float rand_max = p.sample_probs_cumsum[p.num_splats - 1];
    float rand = rand_max * float(_dn_hash_u3(p.seed, block_lin, tid)) *
                 exp2(-32.0f);
    uint lo = 0;
    uint hi = p.num_splats - 1;
    while (lo < hi) {
        uint mid = lo + ((hi - lo) >> 1);
        if (p.sample_probs_cumsum[mid] <= rand)
            lo = mid + 1;
        else
            hi = mid;
    }
    p.index_map[idx] = int(lo);

    InterlockedAdd(p.n_idx_buffer[lo], 1);
}

// Relocation math + optim-state zeroing for splats being relocated TO
// (n_idx > 1). Mirrors mcmc_compute_relocation_kernel.
struct McmcRelocParams {
    int* n_idx_buffer;  // [N]
    float* scales;      // [N, 3]
    float* opacs;       // [N]
    // fp32 non-SH Adam state (has_g1)
    float* g1_means;
    float* g2_means;
    float* g1_quats;
    float* g2_quats;
    float* g1_scales;
    float* g2_scales;
    float* g1_opacs;
    float* g2_opacs;
    float* g1_dc;
    float* g2_dc;
    // SH Adam state (kShOptimBits)
    float* g1_sh;
    float* g2_sh;
    uint* sh_packed;
    float4* sh_quant_bounds;
    // non-SH quantized Adam state (non_sh)
    uint* nq_means_packed;
    uint* nq_quats_packed;
    uint* nq_scales_packed;
    uint* nq_opacities_packed;
    uint* nq_dc_packed;
    float4* nq_means_bounds;
    float4* nq_quats_bounds;
    float4* nq_scales_bounds;
    float4* nq_opacities_bounds;
    float4* nq_dc_bounds;
    int* bias_steps;  // [N] (has_bias_steps)
    uint num_splats;
    uint num_sh;
    uint has_g1;
    uint has_bias_steps;
    uint non_sh;
    uint sh_bounds_per_splat;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_relocation(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform McmcRelocParams* pp) {
    McmcRelocParams p = *pp;
    uint cur_idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (cur_idx >= p.num_splats) return;

    // only compute on indices being relocated to
    uint n_idx = uint(p.n_idx_buffer[cur_idx]) + 1u;
    if (n_idx == 1u) return;

    // compute relocation
    float3 scale = float3(p.scales[3 * cur_idx], p.scales[3 * cur_idx + 1],
                          p.scales[3 * cur_idx + 2]);
    float opac = p.opacs[cur_idx];
    _dn_mcmc_relocation(opac, scale, int(n_idx));
    p.scales[3 * cur_idx] = scale.x;
    p.scales[3 * cur_idx + 1] = scale.y;
    p.scales[3 * cur_idx + 2] = scale.z;
    p.opacs[cur_idx] = opac;

    // set grad to zero
    if (p.has_g1 != 0) {
        [ForceUnroll]
        for (int i = 0; i < 3; i++) {
            p.g1_means[3 * cur_idx + i] = 0.0f;
            p.g2_means[3 * cur_idx + i] = 0.0f;
            p.g1_scales[3 * cur_idx + i] = 0.0f;
            p.g2_scales[3 * cur_idx + i] = 0.0f;
            p.g1_dc[3 * cur_idx + i] = 0.0f;
            p.g2_dc[3 * cur_idx + i] = 0.0f;
        }
        [ForceUnroll]
        for (int i = 0; i < 4; i++) {
            p.g1_quats[4 * cur_idx + i] = 0.0f;
            p.g2_quats[4 * cur_idx + i] = 0.0f;
        }
        p.g1_opacs[cur_idx] = 0.0f;
        p.g2_opacs[cur_idx] = 0.0f;
    }
    if (kShOptimBits == 0) {
        gindex_t off = (gindex_t)(3u * p.num_sh) * (gindex_t)cur_idx;
        float* g1 = elem_at(p.g1_sh, off);
        float* g2 = elem_at(p.g2_sh, off);
        for (uint i = 0; i < 3 * p.num_sh; i++) {
            g1[i] = 0.0f;
            g2[i] = 0.0f;
        }
    } else {
        _dn_zero_qsh_splat(p.sh_packed, cur_idx, p.num_sh, p.sh_quant_bounds,
                           p.sh_bounds_per_splat != 0);
    }
    if (p.non_sh != 0) {
        _dn_zero_nq_splat<3>(p.nq_means_packed, p.nq_means_bounds, cur_idx);
        _dn_zero_nq_splat<4>(p.nq_quats_packed, p.nq_quats_bounds, cur_idx);
        _dn_zero_nq_splat<3>(p.nq_scales_packed, p.nq_scales_bounds, cur_idx);
        _dn_zero_nq_splat<1>(p.nq_opacities_packed, p.nq_opacities_bounds,
                             cur_idx);
        _dn_zero_nq_splat<3>(p.nq_dc_packed, p.nq_dc_bounds, cur_idx);
    }
    if (p.has_bias_steps != 0) p.bias_steps[cur_idx] = 0;
}

struct McmcUpdateRelocParams {  // 72 bytes: pushed directly
    int* index_map;      // [N]
    float* means;
    float* quats;
    float* scales;
    float* opacs;
    float* features_dc;
    float* features_sh;  // (has_fp32_sh)
    uint num_splats;
    uint num_sh;
    uint has_fp32_sh;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_update_reloc(uint3 wg: SV_GroupID,
                               uint tid: SV_GroupThreadID,
                               uniform McmcUpdateRelocParams p) {
    uint id_dst = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (id_dst >= p.num_splats) return;

    // only update on those relocated from
    uint id_src = uint(p.index_map[id_dst]);
    if (id_src == id_dst) return;

    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.means[3 * id_dst + i] = p.means[3 * id_src + i];
        p.scales[3 * id_dst + i] = p.scales[3 * id_src + i];
        p.features_dc[3 * id_dst + i] = p.features_dc[3 * id_src + i];
    }
    [ForceUnroll]
    for (int i = 0; i < 4; i++)
        p.quats[4 * id_dst + i] = p.quats[4 * id_src + i];
    p.opacs[id_dst] = p.opacs[id_src];
    if (p.has_fp32_sh != 0) {
        gindex_t R = (gindex_t)(3u * p.num_sh);
        float* sh_dst = elem_at(p.features_sh, R * (gindex_t)id_dst);
        float* sh_src = elem_at(p.features_sh, R * (gindex_t)id_src);
        for (uint i = 0; i < 3 * p.num_sh; i++)
            sh_dst[i] = sh_src[i];
    }
}

struct McmcComputeAddParams {  // 32 bytes: pushed directly
    int* n_idx_buffer;  // [N]
    float* scales;      // [N, 3]
    float* opacs;       // [N]
    uint num_splats;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_compute_add(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                              uniform McmcComputeAddParams p) {
    uint cur_idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (cur_idx >= p.num_splats) return;

    uint n_idx = uint(p.n_idx_buffer[cur_idx]) + 1u;
    if (n_idx == 1u) return;

    float3 scale = float3(p.scales[3 * cur_idx], p.scales[3 * cur_idx + 1],
                          p.scales[3 * cur_idx + 2]);
    float opac = p.opacs[cur_idx];
    _dn_mcmc_relocation(opac, scale, int(n_idx));
    p.scales[3 * cur_idx] = scale.x;
    p.scales[3 * cur_idx + 1] = scale.y;
    p.scales[3 * cur_idx + 2] = scale.z;
    p.opacs[cur_idx] = opac;

    // don't change grad
}

// Copy + optim-state zeroing for the appended splats (mcmc_update_add_kernel).
struct McmcUpdateAddParams {
    int* index_map;  // [num_add]
    float* means;
    float* quats;
    float* scales;
    float* opacs;
    float* features_dc;
    float* features_sh;  // (has_fp32_sh)
    // fp32 non-SH Adam state (has_g1)
    float* g1_means;
    float* g2_means;
    float* g1_quats;
    float* g2_quats;
    float* g1_scales;
    float* g2_scales;
    float* g1_opacs;
    float* g2_opacs;
    float* g1_dc;
    float* g2_dc;
    // SH Adam state (kShOptimBits)
    float* g1_sh;
    float* g2_sh;
    uint* sh_packed;
    float4* sh_quant_bounds;
    // non-SH quantized Adam state (non_sh)
    uint* nq_means_packed;
    uint* nq_quats_packed;
    uint* nq_scales_packed;
    uint* nq_opacities_packed;
    uint* nq_dc_packed;
    float4* nq_means_bounds;
    float4* nq_quats_bounds;
    float4* nq_scales_bounds;
    float4* nq_opacities_bounds;
    float4* nq_dc_bounds;
    int* bias_steps;  // (has_bias_steps)
    uint num_splats;
    uint num_add;
    uint num_sh;
    uint has_g1;
    uint has_fp32_sh;
    uint has_bias_steps;
    uint non_sh;
    uint sh_bounds_per_splat;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_mcmc_update_add(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform McmcUpdateAddParams* pp) {
    McmcUpdateAddParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_add) return;

    uint id_dst = idx + p.num_splats;
    uint id_src = uint(p.index_map[idx]);

    // copy components
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.means[3 * id_dst + i] = p.means[3 * id_src + i];
        p.scales[3 * id_dst + i] = p.scales[3 * id_src + i];
        p.features_dc[3 * id_dst + i] = p.features_dc[3 * id_src + i];
    }
    [ForceUnroll]
    for (int i = 0; i < 4; i++)
        p.quats[4 * id_dst + i] = p.quats[4 * id_src + i];
    p.opacs[id_dst] = p.opacs[id_src];
    if (p.has_fp32_sh != 0) {
        gindex_t R = (gindex_t)(3u * p.num_sh);
        float* sh_dst = elem_at(p.features_sh, R * (gindex_t)id_dst);
        float* sh_src = elem_at(p.features_sh, R * (gindex_t)id_src);
        for (uint i = 0; i < 3 * p.num_sh; i++)
            sh_dst[i] = sh_src[i];
    }

    // set grad to zero
    if (p.has_g1 != 0) {
        [ForceUnroll]
        for (int i = 0; i < 3; i++) {
            p.g1_means[3 * id_dst + i] = 0.0f;
            p.g2_means[3 * id_dst + i] = 0.0f;
            p.g1_scales[3 * id_dst + i] = 0.0f;
            p.g2_scales[3 * id_dst + i] = 0.0f;
            p.g1_dc[3 * id_dst + i] = 0.0f;
            p.g2_dc[3 * id_dst + i] = 0.0f;
        }
        [ForceUnroll]
        for (int i = 0; i < 4; i++) {
            p.g1_quats[4 * id_dst + i] = 0.0f;
            p.g2_quats[4 * id_dst + i] = 0.0f;
        }
        p.g1_opacs[id_dst] = 0.0f;
        p.g2_opacs[id_dst] = 0.0f;
    }
    if (kShOptimBits == 0) {
        gindex_t off = (gindex_t)(3u * p.num_sh) * (gindex_t)id_dst;
        float* g1 = elem_at(p.g1_sh, off);
        float* g2 = elem_at(p.g2_sh, off);
        for (uint i = 0; i < 3 * p.num_sh; i++) {
            g1[i] = 0.0f;
            g2[i] = 0.0f;
        }
    } else {
        _dn_zero_qsh_splat(p.sh_packed, id_dst, p.num_sh, p.sh_quant_bounds,
                           p.sh_bounds_per_splat != 0);
    }
    if (p.non_sh != 0) {
        _dn_zero_nq_splat<3>(p.nq_means_packed, p.nq_means_bounds, id_dst);
        _dn_zero_nq_splat<4>(p.nq_quats_packed, p.nq_quats_bounds, id_dst);
        _dn_zero_nq_splat<3>(p.nq_scales_packed, p.nq_scales_bounds, id_dst);
        _dn_zero_nq_splat<1>(p.nq_opacities_packed, p.nq_opacities_bounds,
                             id_dst);
        _dn_zero_nq_splat<3>(p.nq_dc_packed, p.nq_dc_bounds, id_dst);
    }
    if (p.has_bias_steps != 0) p.bias_steps[id_dst] = 0;
}

// ---------------------------------------------------------------------------
// normalize_clip_map_inplace (DensifySampling.cu _normalize_clip_map_kernel)
// ---------------------------------------------------------------------------

struct NormalizeClipMapParams {  // 48 bytes: pushed directly
    float* data;        // [B, N]
    float* inv_median;  // [B] (use_median gates)
    float* clip;        // [B] (use_clip gates)
    uint B, N;
    uint use_median, use_clip, wgs_per_row;
    float power;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void normalize_clip_map(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform NormalizeClipMapParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.B * p.N) return;
    uint b = idx / p.N;
    float v = p.data[idx];
    if (p.use_clip != 0) {
        float c = p.clip[b];
        if (!(v <= c)) v = c;
    }
    if (p.use_median != 0)
        v *= p.inv_median[b];
    // Floored before the power: the flat-region background is tiny negative
    // noise, and a fractional power of it is NaN (see DensifySampling.cu).
    if (p.power != 1.0f)
        v = pow(max(v, 0.0f), p.power);
    p.data[idx] = v;
}


// ---------------------------------------------------------------------------
// accum finalize / score clip (DensifyScoring.cu)
// ---------------------------------------------------------------------------

struct AccumFinalizeParams {  // 24 bytes: pushed directly
    float* accum;   // [2N] planar: numerator then denominator
    uint num_splats, wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_accum_finalize(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform AccumFinalizeParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    float den = p.accum[p.num_splats + idx];
    p.accum[idx] = den > 0.0f ? p.accum[idx] / den : 0.0f;
}

struct ScoreGatherParams {  // 24 bytes: pushed directly
    float2* accum_buffer;
    float* out;
    uint num_splats, wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_gather_score(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform ScoreGatherParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    p.out[idx] = p.accum_buffer[idx].x;
}

struct ScoreClipParams {  // 48 bytes: pushed directly
    float2* accum_buffer;
    float* cutoff;      // [1] (use_cutoff gates)
    float2* score_out;  // [N] (use_out gates)
    float power;
    uint use_cutoff, use_out, num_splats, wgs_per_row;
    uint _pad0;
};

// The clip is in place and idempotent; the power is not, so it lands in
// score_out (see DensifyScoring.cu).
[shader("compute")]
[numthreads(256, 1, 1)]
void densify_clip_score(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform ScoreClipParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    float2 a = p.accum_buffer[idx];
    float v = a.x;
    if (p.use_cutoff != 0) {
        float c = p.cutoff[0];
        // !(v <= c) also catches NaN, which would otherwise survive every ranking.
        if (c > 0.0f && !(v <= c)) {
            v = c;
            p.accum_buffer[idx].x = c;
        }
    }
    if (p.use_out != 0) {
        if (p.power != 1.0f) v = pow(max(v, 0.0f), p.power);
        p.score_out[idx] = float2(v, a.y);
    }
}
