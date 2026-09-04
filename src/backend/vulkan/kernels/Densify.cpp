// Vulkan implementation of the densification launch APIs (kernels/densify/Densify.cuh):
// densify_update_weight, densify_clip_scale, the long-axis-split
// relocate/add family, the MCMC relocate/add family, and the MCMC/revised
// noise kernels. Device work: shaders/densify.slang.
//
// Structure mirrors the CUDA launchers: weighted sampling without
// replacement sorts efraimidis-spirakis keys (emitted as order-preserving
// uint32 bits so the raw-unsigned radix sort reproduces CUB's float-key
// order), the MCMC paths run a float cumsum + hash-driven binary-search
// draws, and the relocation mask compacts through an atomic counter (the
// src->dst pairing is scheduling-dependent, exactly like CUDA — parity
// tests compare relocated rows as a multiset).

#include <kernels/densify/Densify.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

using backend::MemcpyKind;

// Mirrors DensifyUpdateWeightParams in shaders/densify.slang.
struct DensifyUpdateWeightParams {
    uint64_t radii, opacs, accum_weight, accum_weight2, accum_buffer;
    uint64_t oversize_accum;
    float blend_w;
    float score_power;
    int32_t score_mode;
    uint32_t use_opacs, use_w2, num_splats, wgs_per_row;
    float max_scale2d;
    uint32_t use_oversize;
    uint32_t _pad0;
};
static_assert(sizeof(DensifyUpdateWeightParams) == 6 * 8 + 10 * 4,
              "params layout must match the slang struct");

// Mirrors DensifyOversizeWeightParams.
struct DensifyOversizeWeightParams {
    uint64_t oversize_accum, score, out;
    float blend;
    uint32_t num_splats, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(DensifyOversizeWeightParams) == 3 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors DensifyClipScaleParams.
struct DensifyClipScaleParams {
    uint64_t radii, log_scales, logit_opacs;
    float max_scale2d, clip_hardness, max_scale3d;
    uint32_t has_opacs, num_splats, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(DensifyClipScaleParams) == 3 * 8 + 8 * 4,
              "params layout must match the slang struct");

// Mirrors DensifyNoiseParams.
struct DensifyNoiseParams {
    uint64_t means, log_scales, quats, logit_opacs, radii;
    float scaler;
    uint32_t num_splats, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(DensifyNoiseParams) == 5 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors EfraimidisParams.
struct EfraimidisParams {
    uint64_t weights, mask, out_keys;
    uint32_t stride, seed, use_mask, numel, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(EfraimidisParams) == 3 * 8 + 6 * 4,
              "params layout must match the slang struct");

// Mirrors RelocMaskParams.
struct RelocMaskParams {
    uint64_t means, quats, scales, opacs, features_dc, mask, count,
        dst_indices;
    float min_opacity;
    uint32_t num_splats, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(RelocMaskParams) == 8 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors RelocLasParams.
struct RelocLasParams {
    uint64_t src_indices, dst_indices;
    uint64_t means, quats, scales, opacs, features_dc, features_sh;
    uint64_t g1_means, g2_means, g1_quats, g2_quats, g1_scales, g2_scales,
        g1_opacs, g2_opacs, g1_dc, g2_dc;
    uint64_t g1_sh, g2_sh, sh_packed, sh_quant_bounds;
    uint64_t nq_means_packed, nq_quats_packed, nq_scales_packed,
        nq_opacities_packed, nq_dc_packed;
    uint64_t nq_means_bounds, nq_quats_bounds, nq_scales_bounds,
        nq_opacities_bounds, nq_dc_bounds;
    uint64_t accum, bias_steps;
    float split_opacity_k;
    uint32_t cur_num_splats, num_new_splats, num_sh;
    uint32_t has_dst, has_g1, has_fp32_sh, has_accum, has_bias_steps, non_sh,
        sh_bounds_per_splat, wgs_per_row;
};
static_assert(sizeof(RelocLasParams) == 34 * 8 + 12 * 4,
              "params layout must match the slang struct");

// Mirror AccumFinalizeParams / ScoreGatherParams / ScoreClipParams.
struct AccumFinalizeParams {
    uint64_t accum;
    uint32_t num_splats, wgs_per_row;
};
static_assert(sizeof(AccumFinalizeParams) == 8 + 2 * 4, "params layout");

struct ScorePairParams {  // densify_gather_score
    uint64_t accum_buffer, other;
    uint32_t num_splats, wgs_per_row;
};
static_assert(sizeof(ScorePairParams) == 2 * 8 + 2 * 4, "params layout");

// Mirrors ScoreClipParams.
struct ScoreClipParams {
    uint64_t accum_buffer, cutoff, score_out;
    float power;
    uint32_t use_cutoff, use_out, num_splats, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(ScoreClipParams) == 3 * 8 + 6 * 4,
              "params layout must match the slang struct");

// Mirrors NormalizeClipMapParams.
struct NormalizeClipMapParams {
    uint64_t data, inv_median, clip;
    uint32_t B, N, use_median, use_clip, wgs_per_row;
    float power;
};
static_assert(sizeof(NormalizeClipMapParams) == 3 * 8 + 6 * 4,
              "params layout must match the slang struct");

// Mirrors CopyQshPairsParams.
struct CopyQshPairsParams {
    uint64_t src_indices, dst_indices, packed, bounds;
    uint32_t cur_num_splats, num_pairs, num_sh, num_sh_buffer, has_dst,
        bounds_per_splat, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(CopyQshPairsParams) == 4 * 8 + 8 * 4,
              "params layout must match the slang struct");

// Mirrors CopyQshMapParams.
struct CopyQshMapParams {
    uint64_t index_map, packed, bounds;
    uint32_t num_splats, num_sh, num_sh_buffer, bounds_per_splat, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(CopyQshMapParams) == 3 * 8 + 6 * 4,
              "params layout must match the slang struct");

// Mirrors McmcProbsParams.
struct McmcProbsParams {
    uint64_t opacs, scales, probs;
    float min_opacity;
    uint32_t num_splats, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(McmcProbsParams) == 3 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors McmcRelocIndexMapParams.
struct McmcRelocIndexMapParams {
    uint64_t sample_probs, sample_probs_cumsum, index_map, n_idx_buffer;
    uint32_t numel, seed, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(McmcRelocIndexMapParams) == 4 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors McmcAddIndexMapParams.
struct McmcAddIndexMapParams {
    uint64_t sample_probs_cumsum, index_map, n_idx_buffer;
    uint32_t num_splats, num_add, seed, wgs_per_row;
};
static_assert(sizeof(McmcAddIndexMapParams) == 3 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors McmcRelocParams.
struct McmcRelocParams {
    uint64_t n_idx_buffer, scales, opacs;
    uint64_t g1_means, g2_means, g1_quats, g2_quats, g1_scales, g2_scales,
        g1_opacs, g2_opacs, g1_dc, g2_dc;
    uint64_t g1_sh, g2_sh, sh_packed, sh_quant_bounds;
    uint64_t nq_means_packed, nq_quats_packed, nq_scales_packed,
        nq_opacities_packed, nq_dc_packed;
    uint64_t nq_means_bounds, nq_quats_bounds, nq_scales_bounds,
        nq_opacities_bounds, nq_dc_bounds;
    uint64_t bias_steps;
    uint32_t num_splats, num_sh, has_g1, has_bias_steps, non_sh,
        sh_bounds_per_splat, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(McmcRelocParams) == 28 * 8 + 8 * 4,
              "params layout must match the slang struct");

// Mirrors McmcUpdateRelocParams.
struct McmcUpdateRelocParams {
    uint64_t index_map, means, quats, scales, opacs, features_dc, features_sh;
    uint32_t num_splats, num_sh, has_fp32_sh, wgs_per_row;
};
static_assert(sizeof(McmcUpdateRelocParams) == 7 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors McmcComputeAddParams.
struct McmcComputeAddParams {
    uint64_t n_idx_buffer, scales, opacs;
    uint32_t num_splats, wgs_per_row;
};
static_assert(sizeof(McmcComputeAddParams) == 3 * 8 + 2 * 4,
              "params layout must match the slang struct");

// Mirrors McmcUpdateAddParams.
struct McmcUpdateAddParams {
    uint64_t index_map;
    uint64_t means, quats, scales, opacs, features_dc, features_sh;
    uint64_t g1_means, g2_means, g1_quats, g2_quats, g1_scales, g2_scales,
        g1_opacs, g2_opacs, g1_dc, g2_dc;
    uint64_t g1_sh, g2_sh, sh_packed, sh_quant_bounds;
    uint64_t nq_means_packed, nq_quats_packed, nq_scales_packed,
        nq_opacities_packed, nq_dc_packed;
    uint64_t nq_means_bounds, nq_quats_bounds, nq_scales_bounds,
        nq_opacities_bounds, nq_dc_bounds;
    uint64_t bias_steps;
    uint32_t num_splats, num_add, num_sh;
    uint32_t has_g1, has_fp32_sh, has_bias_steps, non_sh, sh_bounds_per_splat,
        wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(McmcUpdateAddParams) == 32 * 8 + 10 * 4,
              "params layout must match the slang struct");

// kShOptimBits spec value: 0 = fp32 SH state, 8 / 4 = QuantizedAdamState.
uint32_t sh_optim_spec(int sh_optim_bits) {
    if (sh_optim_bits == 32) return 0;
    if (sh_optim_bits == 8 || sh_optim_bits == 4)
        return (uint32_t)sh_optim_bits;
    throw std::runtime_error("densify: sh_optim_bits must be 32, 8 or 4");
}

// Fills the shared SH-state fields of the relocate/MCMC param structs and
// returns the kShOptimBits spec value. In quant mode g1_features_sh aliases
// the packed byte buffer (as in the CUDA launchers).
template <typename P>
uint32_t fill_sh_state(P& p, const DeviceVector<float3>& g1_features_sh,
                       const DeviceVector<float3>& g2_features_sh,
                       const DeviceVector<float4>& sh_quant_bounds,
                       bool sh_bounds_per_splat, int sh_optim_bits) {
    uint32_t spec = sh_optim_spec(sh_optim_bits);
    if (spec == 0) {
        p.g1_sh = vkk::or_fallback(g1_features_sh.data_ptr());
        p.g2_sh = vkk::or_fallback(g2_features_sh.data_ptr());
        p.sh_packed = vkk::null_fallback();
        p.sh_quant_bounds = vkk::null_fallback();
    } else {
        p.g1_sh = vkk::null_fallback();
        p.g2_sh = vkk::null_fallback();
        p.sh_packed = (uint64_t)g1_features_sh.data_ptr();
        p.sh_quant_bounds = (uint64_t)sh_quant_bounds.data_ptr();
    }
    p.sh_bounds_per_splat = sh_bounds_per_splat ? 1u : 0u;
    return spec;
}

// Fills the shared non-SH quant-state fields; returns the non_sh flag.
template <typename P>
uint32_t fill_non_sh(P& p, const NonShQuantState& non_sh) {
    p.nq_means_packed = vkk::or_fallback(non_sh.means_packed);
    p.nq_quats_packed = vkk::or_fallback(non_sh.quats_packed);
    p.nq_scales_packed = vkk::or_fallback(non_sh.scales_packed);
    p.nq_opacities_packed = vkk::or_fallback(non_sh.opacities_packed);
    p.nq_dc_packed = vkk::or_fallback(non_sh.features_dc_packed);
    p.nq_means_bounds = vkk::or_fallback(non_sh.means_bounds);
    p.nq_quats_bounds = vkk::or_fallback(non_sh.quats_bounds);
    p.nq_scales_bounds = vkk::or_fallback(non_sh.scales_bounds);
    p.nq_opacities_bounds = vkk::or_fallback(non_sh.opacities_bounds);
    p.nq_dc_bounds = vkk::or_fallback(non_sh.features_dc_bounds);
    return non_sh.enabled ? 1u : 0u;
}

// Fills the shared fp32 non-SH Adam-state fields; returns the has_g1 flag
// (the g1_* buffers are freed together when FPBO non-SH quant owns them).
template <typename P>
uint32_t fill_g1g2(P& p, const DeviceVector<float3>& g1_means,
                   const DeviceVector<float3>& g2_means,
                   const DeviceVector<float4>& g1_quats,
                   const DeviceVector<float4>& g2_quats,
                   const DeviceVector<float3>& g1_scales,
                   const DeviceVector<float3>& g2_scales,
                   const DeviceVector<float>& g1_opacs,
                   const DeviceVector<float>& g2_opacs,
                   const DeviceVector<float3>& g1_dc,
                   const DeviceVector<float3>& g2_dc) {
    p.g1_means = vkk::or_fallback(g1_means.data_ptr());
    p.g2_means = vkk::or_fallback(g2_means.data_ptr());
    p.g1_quats = vkk::or_fallback(g1_quats.data_ptr());
    p.g2_quats = vkk::or_fallback(g2_quats.data_ptr());
    p.g1_scales = vkk::or_fallback(g1_scales.data_ptr());
    p.g2_scales = vkk::or_fallback(g2_scales.data_ptr());
    p.g1_opacs = vkk::or_fallback(g1_opacs.data_ptr());
    p.g2_opacs = vkk::or_fallback(g2_opacs.data_ptr());
    p.g1_dc = vkk::or_fallback(g1_dc.data_ptr());
    p.g2_dc = vkk::or_fallback(g2_dc.data_ptr());
    return g1_means.data_ptr() ? 1u : 0u;
}

// Weighted sampling without replacement (DensifySampling.cu
// weighted_sample_without_replacement_internal): efraimidis-spirakis keys
// (as order-preserving uint32 bits), stable radix sort of the identity
// permutation, first num_sample sorted indices copied to the OutIdx slot.
const int32_t* wswr_sample(int64_t numel, const float* weights_ptr,
                           const int32_t* mask_ptr, uint32_t num_sample,
                           uint32_t seed) {
    // stride fixed to 2 (accum buffer float2; see the CUDA comment about
    // warmup making the derived stride incorrect)
    DeviceVector<int32_t> keys_in, keys_out, idx_in, idx_out, out_idx;
    keys_in.resize(PoolSlot::DensifyWswrSortingValues, numel);
    keys_out.resize(PoolSlot::DensifyWswrKeysOut, numel);
    idx_in.resize(PoolSlot::DensifyWswrIndicesIn, numel);
    idx_out.resize(PoolSlot::DensifyWswrIndicesOut, numel);
    out_idx.resize(PoolSlot::DensifyWswrOutIdx, num_sample);

    EfraimidisParams ep{};
    ep.weights = (uint64_t)weights_ptr;
    ep.mask = vkk::or_fallback(mask_ptr);
    ep.out_keys = (uint64_t)keys_in.data_ptr();
    ep.stride = 2;
    ep.seed = seed;
    ep.use_mask = mask_ptr ? 1u : 0u;
    ep.numel = (uint32_t)numel;
    vkk::dispatch_flat("densify.densify_efraimidis_keys", {}, numel, 256, &ep,
                       sizeof(ep), &ep.wgs_per_row);

    struct IotaParams {
        uint64_t buf;
        uint32_t n, wgs_per_row;
    } ip{};
    ip.buf = (uint64_t)idx_in.data_ptr();
    ip.n = (uint32_t)numel;
    vkk::dispatch_flat("projection_qgrad.qgrad_iota", {}, numel, 256, &ip,
                       sizeof(ip), &ip.wgs_per_row);

    backend::DoubleBuffer<int32_t> d_keys(keys_in.data_ptr(),
                                          keys_out.data_ptr());
    backend::DoubleBuffer<int32_t> d_vals(idx_in.data_ptr(),
                                          idx_out.data_ptr());
    backend::sort_pairs(d_keys, d_vals, numel, 0, 32);

    backend::memcpy_sync(out_idx.data_ptr(), d_vals.current(),
                         sizeof(int32_t) * num_sample,
                         MemcpyKind::DeviceToDevice);
    return out_idx.data_ptr();
}

// SH value-codec copy dispatch (pairs form). No-op when bits == 32.
void launch_copy_qsh_pairs(int64_t cur_num_splats, int64_t num_pairs,
                           const int32_t* src_indices,
                           const int32_t* dst_indices, uint8_t* packed,
                           float2* bounds, int num_sh, int num_sh_buffer,
                           int bits, bool bounds_per_splat) {
    if (bits == 32 || num_pairs == 0 || packed == nullptr || bounds == nullptr)
        return;
    if (bits != 8 && bits != 16)
        throw std::runtime_error("densify: sh_value_bits must be 32, 8 or 16");
    CopyQshPairsParams p{};
    p.src_indices = (uint64_t)src_indices;
    p.dst_indices = vkk::or_fallback(dst_indices);
    p.packed = (uint64_t)packed;
    p.bounds = (uint64_t)bounds;
    p.cur_num_splats = (uint32_t)cur_num_splats;
    p.num_pairs = (uint32_t)num_pairs;
    p.num_sh = (uint32_t)num_sh;
    p.num_sh_buffer = (uint32_t)num_sh_buffer;
    p.has_dst = dst_indices ? 1u : 0u;
    p.bounds_per_splat = bounds_per_splat ? 1u : 0u;
    // Spec IDs follow declaration order in densify.slang: 0 = kShOptimBits
    // (unused by the copy kernels), 1 = kShValueBits.
    vkk::dispatch_flat("densify.densify_copy_qsh_pairs",
                       backend::vk::SpecList{0u, (uint32_t)bits}, num_pairs,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

// SH value-codec copy dispatch (index-map form).
void launch_copy_qsh_map(int64_t num_splats, const int32_t* index_map,
                         uint8_t* packed, float2* bounds, int num_sh,
                         int num_sh_buffer, int bits, bool bounds_per_splat) {
    if (bits == 32 || num_splats == 0 || packed == nullptr ||
        bounds == nullptr)
        return;
    if (bits != 8 && bits != 16)
        throw std::runtime_error("densify: sh_value_bits must be 32, 8 or 16");
    CopyQshMapParams p{};
    p.index_map = (uint64_t)index_map;
    p.packed = (uint64_t)packed;
    p.bounds = (uint64_t)bounds;
    p.num_splats = (uint32_t)num_splats;
    p.num_sh = (uint32_t)num_sh;
    p.num_sh_buffer = (uint32_t)num_sh_buffer;
    p.bounds_per_splat = bounds_per_splat ? 1u : 0u;
    vkk::dispatch_flat("densify.densify_copy_qsh_map",
                       backend::vk::SpecList{0u, (uint32_t)bits}, num_splats,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

// MCMC sampling-probability + cumsum stage shared by relocate/add.
void mcmc_probs_cumsum(int64_t cur_num_splats, float min_opacity,
                       const DeviceVector<float>& opacs,
                       const DeviceVector<float3>& scales, PoolSlot probs_slot,
                       PoolSlot cumsum_slot, DeviceVector<float>& probs,
                       DeviceVector<float>& cumsum) {
    probs.resize(probs_slot, cur_num_splats);
    cumsum.resize(cumsum_slot, cur_num_splats);
    McmcProbsParams p{};
    p.opacs = (uint64_t)opacs.data_ptr();
    p.scales = (uint64_t)scales.data_ptr();
    p.probs = (uint64_t)probs.data_ptr();
    p.min_opacity = min_opacity;
    p.num_splats = (uint32_t)cur_num_splats;
    vkk::dispatch_flat("densify.densify_mcmc_probs", {}, cur_num_splats, 256,
                       &p, sizeof(p), &p.wgs_per_row);
    backend::inclusive_sum<float>(probs.data_ptr(), cumsum.data_ptr(),
                                  cur_num_splats);
}

}  // namespace

/* API definitions matching kernels/densify/Densify.cuh (engine-referenced subset) */

void densify_update_weight(
    int64_t num_splats,
    DeviceVector<float> radii,
    float3* scales_ptr,
    float* opacs_ptr,
    DeviceVector<float> accum_weight,
    DeviceVector<float> accum_weight2,
    float blend_w,
    float score_power,
    DeviceVector<float2> accum_buffer,
    int score_mode,
    float max_scale2d,
    DeviceVector<float> oversize_accum
) {
    (void)scales_ptr;  // unused by the kernel, as in CUDA
    if (num_splats <= 0) return;
    DensifyUpdateWeightParams p{};
    p.radii = (uint64_t)radii.data_ptr();
    p.opacs = vkk::or_fallback(opacs_ptr);
    p.accum_weight = (uint64_t)accum_weight.data_ptr();
    p.accum_weight2 = vkk::or_fallback(accum_weight2.data_ptr());
    p.accum_buffer = (uint64_t)accum_buffer.data_ptr();
    p.blend_w = blend_w;
    p.score_power = score_power;
    p.score_mode = score_mode;
    p.use_opacs = opacs_ptr ? 1u : 0u;
    p.use_w2 = accum_weight2.data_ptr() ? 1u : 0u;
    p.num_splats = (uint32_t)num_splats;
    p.oversize_accum = vkk::or_fallback(oversize_accum.data_ptr());
    p.max_scale2d = max_scale2d;
    p.use_oversize = oversize_accum.data_ptr() ? 1u : 0u;
    vkk::dispatch_flat("densify.densify_update_weight",
                       backend::vk::SpecList{}, num_splats, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void densify_oversize_weight_tensor(
    int64_t num_splats,
    float blend,
    DeviceVector<float> oversize_accum,
    DeviceVector<float2> score,
    DeviceVector<float2> out
) {
    if (num_splats <= 0) return;
    DensifyOversizeWeightParams p{};
    p.oversize_accum = (uint64_t)oversize_accum.data_ptr();
    p.score = (uint64_t)score.data_ptr();
    p.out = (uint64_t)out.data_ptr();
    p.blend = blend;
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("densify.densify_oversize_weight", {}, num_splats, 256,
                       &p, sizeof(p), &p.wgs_per_row);
}

void densify_clip_scale_tensor(
    int64_t num_splats,
    DeviceVector<float> radii,
    DeviceVector<float3> log_scales,
    float* logit_opacs_ptr,
    float max_scale2d,
    float clip_hardness,
    float max_scale3d
) {
    if (num_splats <= 0) return;
    DensifyClipScaleParams p{};
    p.radii = (uint64_t)radii.data_ptr();
    p.log_scales = (uint64_t)log_scales.data_ptr();
    p.logit_opacs = vkk::or_fallback(logit_opacs_ptr);
    p.max_scale2d = max_scale2d;
    p.clip_hardness = clip_hardness;
    p.max_scale3d = max_scale3d;
    p.has_opacs = logit_opacs_ptr ? 1u : 0u;
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("densify.densify_clip_scale", {}, num_splats, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void mcmc_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs
) {
    if (num_splats <= 0) return;
    DensifyNoiseParams p{};
    p.means = (uint64_t)means.data_ptr();
    p.log_scales = (uint64_t)log_scales.data_ptr();
    p.quats = (uint64_t)quats.data_ptr();
    p.logit_opacs = (uint64_t)opacs.data_ptr();
    p.radii = vkk::null_fallback();
    p.scaler = scaler;
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("densify.densify_mcmc_add_noise", {}, num_splats, 256,
                       &p, sizeof(p), &p.wgs_per_row);
}

void revised_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float> radii,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs
) {
    if (num_splats <= 0) return;
    DensifyNoiseParams p{};
    p.means = (uint64_t)means.data_ptr();
    p.log_scales = (uint64_t)log_scales.data_ptr();
    p.quats = (uint64_t)quats.data_ptr();
    p.logit_opacs = (uint64_t)opacs.data_ptr();
    p.radii = (uint64_t)radii.data_ptr();
    p.scaler = scaler;
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("densify.densify_revised_add_noise", {}, num_splats,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

// Shared body of relocate/add with long axis split: the kernel is the same,
// relocate passes compacted dst indices while add appends at cur_num + i.
static void launch_relocate_las(
    int64_t cur_num_splats, int64_t num_new_splats, float split_opacity_k,
    const int32_t* src_indices, const int32_t* dst_indices,
    DeviceVector<float3>& means, DeviceVector<float4>& quats,
    DeviceVector<float3>& scales, DeviceVector<float>& opacs,
    DeviceVector<float3>& features_dc, DeviceVector<float3>& features_sh,
    DeviceVector<float3>& g1_means, DeviceVector<float4>& g1_quats,
    DeviceVector<float3>& g1_scales, DeviceVector<float>& g1_opacs,
    DeviceVector<float3>& g1_features_dc, DeviceVector<float3>& g1_features_sh,
    DeviceVector<float3>& g2_means, DeviceVector<float4>& g2_quats,
    DeviceVector<float3>& g2_scales, DeviceVector<float>& g2_opacs,
    DeviceVector<float3>& g2_features_dc, DeviceVector<float3>& g2_features_sh,
    DeviceVector<float2>& densify_accum_buffer,
    DeviceVector<int32_t>& bias_correction_steps, int sh_optim_bits,
    int num_sh, DeviceVector<float4>& sh_quant_bounds,
    bool sh_bounds_per_splat, const NonShQuantState& non_sh) {
    RelocLasParams p{};
    p.src_indices = (uint64_t)src_indices;
    p.dst_indices = vkk::or_fallback(dst_indices);
    p.means = (uint64_t)means.data_ptr();
    p.quats = (uint64_t)quats.data_ptr();
    p.scales = (uint64_t)scales.data_ptr();
    p.opacs = (uint64_t)opacs.data_ptr();
    p.features_dc = (uint64_t)features_dc.data_ptr();
    p.features_sh = vkk::or_fallback(features_sh.data_ptr());
    p.has_g1 = fill_g1g2(p, g1_means, g2_means, g1_quats, g2_quats,
                         g1_scales, g2_scales, g1_opacs, g2_opacs,
                         g1_features_dc, g2_features_dc);
    uint32_t spec = fill_sh_state(p, g1_features_sh, g2_features_sh,
                                  sh_quant_bounds, sh_bounds_per_splat,
                                  sh_optim_bits);
    p.non_sh = fill_non_sh(p, non_sh);
    p.accum = vkk::or_fallback(densify_accum_buffer.data_ptr());
    p.bias_steps = vkk::or_fallback(bias_correction_steps.data_ptr());
    p.split_opacity_k = split_opacity_k;
    p.cur_num_splats = (uint32_t)cur_num_splats;
    p.num_new_splats = (uint32_t)num_new_splats;
    p.num_sh = (uint32_t)num_sh;
    p.has_dst = dst_indices ? 1u : 0u;
    p.has_fp32_sh = features_sh.data_ptr() ? 1u : 0u;
    p.has_accum = densify_accum_buffer.data_ptr() ? 1u : 0u;
    p.has_bias_steps = bias_correction_steps.data_ptr() ? 1u : 0u;

    vkk::Fold f = vkk::fold_1d(num_new_splats, 256);
    p.wgs_per_row = f.per_row;
    vkk::dispatch_ring("densify.densify_relocate_las",
                       backend::vk::SpecList{spec}, f.per_row, f.rows, 1, &p,
                       sizeof(p));
}

void relocate_splats_with_long_axis_split_tensor(
    int64_t cur_num_splats,
    float min_opacity,
    float split_opacity_k,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<float2> densify_accum_buffer,
    DeviceVector<float2> sample_weights,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,
    NonShQuantState non_sh,
    uint32_t seed
) {
    if (cur_num_splats <= 0) return;

    // Relocation mask + atomic compaction of dst indices (Vulkan mask is an
    // int32 array; the CUDA bool layout is a launcher-internal detail).
    DeviceVector<int32_t> mask, count, dst_indices;
    mask.resize(PoolSlot::DensifyRelocMask, cur_num_splats);
    count.resize(PoolSlot::DensifyRelocCount, 1);
    dst_indices.resize(PoolSlot::DensifyRelocDstIndices, cur_num_splats);
    backend::memset_sync(count.data_ptr(), 0, sizeof(int32_t));

    RelocMaskParams mp{};
    mp.means = (uint64_t)means.data_ptr();
    mp.quats = (uint64_t)quats.data_ptr();
    mp.scales = (uint64_t)scales.data_ptr();
    mp.opacs = (uint64_t)opacs.data_ptr();
    mp.features_dc = (uint64_t)features_dc.data_ptr();
    mp.mask = (uint64_t)mask.data_ptr();
    mp.count = (uint64_t)count.data_ptr();
    mp.dst_indices = (uint64_t)dst_indices.data_ptr();
    mp.min_opacity = min_opacity;
    mp.num_splats = (uint32_t)cur_num_splats;
    vkk::dispatch_flat("densify.densify_reloc_mask", {}, cur_num_splats, 256,
                       &mp, sizeof(mp), &mp.wgs_per_row);

    int32_t num_relocate = 0;
    backend::memcpy_sync(&num_relocate, count.data_ptr(), sizeof(int32_t),
                         MemcpyKind::DeviceToHost);
    if (num_relocate == 0) return;

    const int32_t* src_indices = wswr_sample(
        cur_num_splats,
        (const float*)(sample_weights.data_ptr()
                           ? sample_weights.data_ptr()
                           : densify_accum_buffer.data_ptr()),
        mask.data_ptr(), (uint32_t)num_relocate, seed);

    launch_relocate_las(cur_num_splats, num_relocate, split_opacity_k,
                        src_indices, dst_indices.data_ptr(), means, quats,
                        scales, opacs, features_dc, features_sh, g1_means,
                        g1_quats, g1_scales, g1_opacs, g1_features_dc,
                        g1_features_sh, g2_means, g2_quats, g2_scales,
                        g2_opacs, g2_features_dc, g2_features_sh,
                        densify_accum_buffer, bias_correction_steps,
                        sh_optim_bits, num_sh, sh_quant_bounds,
                        sh_bounds_per_splat, non_sh);

    launch_copy_qsh_pairs(cur_num_splats, num_relocate, src_indices,
                          dst_indices.data_ptr(), sh_value_packed.data_ptr(),
                          sh_value_bounds.data_ptr(), num_sh, num_sh_buffer,
                          sh_value_bits, sh_value_bounds_per_splat);
}

void add_splats_with_long_axis_split_tensor(
    int64_t cur_num_splats,
    int64_t num_new_splats,
    float split_opacity_k,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<float2> densify_accum_buffer,
    DeviceVector<float2> sample_weights,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,
    NonShQuantState non_sh,
    uint32_t seed
) {
    if (num_new_splats == 0 || cur_num_splats <= 0) return;

    const int32_t* split_indices = wswr_sample(
        cur_num_splats,
        (const float*)(sample_weights.data_ptr()
                           ? sample_weights.data_ptr()
                           : densify_accum_buffer.data_ptr()),
        nullptr, (uint32_t)num_new_splats, seed);

    launch_relocate_las(cur_num_splats, num_new_splats, split_opacity_k,
                        split_indices, nullptr, means, quats, scales, opacs,
                        features_dc, features_sh, g1_means, g1_quats,
                        g1_scales, g1_opacs, g1_features_dc, g1_features_sh,
                        g2_means, g2_quats, g2_scales, g2_opacs,
                        g2_features_dc, g2_features_sh, densify_accum_buffer,
                        bias_correction_steps, sh_optim_bits, num_sh,
                        sh_quant_bounds, sh_bounds_per_splat, non_sh);

    launch_copy_qsh_pairs(cur_num_splats, num_new_splats, split_indices,
                          nullptr, sh_value_packed.data_ptr(),
                          sh_value_bounds.data_ptr(), num_sh, num_sh_buffer,
                          sh_value_bits, sh_value_bounds_per_splat);
}

void relocate_splats_mcmc_tensor(
    int64_t cur_num_splats,
    float min_opacity,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,
    NonShQuantState non_sh,
    uint32_t seed
) {
    if (cur_num_splats <= 0) return;

    DeviceVector<float> probs, cumsum;
    mcmc_probs_cumsum(cur_num_splats, min_opacity, opacs, scales,
                      PoolSlot::DensifyMcmcSampleProbs,
                      PoolSlot::DensifyMcmcSampleProbsCumsum, probs, cumsum);

    DeviceVector<int32_t> index_map, n_idx_buffer;
    index_map.resize(PoolSlot::DensifyMcmcIndexMap, cur_num_splats);
    n_idx_buffer.resize(PoolSlot::DensifyMcmcNIdxBuffer, cur_num_splats);
    backend::memset_sync(n_idx_buffer.data_ptr(), 0,
                         cur_num_splats * sizeof(int32_t));

    McmcRelocIndexMapParams imp{};
    imp.sample_probs = (uint64_t)probs.data_ptr();
    imp.sample_probs_cumsum = (uint64_t)cumsum.data_ptr();
    imp.index_map = (uint64_t)index_map.data_ptr();
    imp.n_idx_buffer = (uint64_t)n_idx_buffer.data_ptr();
    imp.numel = (uint32_t)cur_num_splats;
    imp.seed = seed;
    vkk::dispatch_flat("densify.densify_mcmc_reloc_index_map", {},
                       cur_num_splats, 256, &imp, sizeof(imp),
                       &imp.wgs_per_row);

    McmcRelocParams rp{};
    rp.n_idx_buffer = (uint64_t)n_idx_buffer.data_ptr();
    rp.scales = (uint64_t)scales.data_ptr();
    rp.opacs = (uint64_t)opacs.data_ptr();
    rp.has_g1 = fill_g1g2(rp, g1_means, g2_means, g1_quats, g2_quats,
                          g1_scales, g2_scales, g1_opacs, g2_opacs,
                          g1_features_dc, g2_features_dc);
    uint32_t spec = fill_sh_state(rp, g1_features_sh, g2_features_sh,
                                  sh_quant_bounds, sh_bounds_per_splat,
                                  sh_optim_bits);
    rp.non_sh = fill_non_sh(rp, non_sh);
    rp.bias_steps = vkk::or_fallback(bias_correction_steps.data_ptr());
    rp.num_splats = (uint32_t)cur_num_splats;
    rp.num_sh = (uint32_t)num_sh;
    rp.has_bias_steps = bias_correction_steps.data_ptr() ? 1u : 0u;
    {
        vkk::Fold f = vkk::fold_1d(cur_num_splats, 256);
        rp.wgs_per_row = f.per_row;
        vkk::dispatch_ring("densify.densify_mcmc_relocation",
                           backend::vk::SpecList{spec}, f.per_row, f.rows, 1,
                           &rp, sizeof(rp));
    }

    McmcUpdateRelocParams up{};
    up.index_map = (uint64_t)index_map.data_ptr();
    up.means = (uint64_t)means.data_ptr();
    up.quats = (uint64_t)quats.data_ptr();
    up.scales = (uint64_t)scales.data_ptr();
    up.opacs = (uint64_t)opacs.data_ptr();
    up.features_dc = (uint64_t)features_dc.data_ptr();
    up.features_sh = vkk::or_fallback(features_sh.data_ptr());
    up.num_splats = (uint32_t)cur_num_splats;
    up.num_sh = (uint32_t)num_sh;
    up.has_fp32_sh = features_sh.data_ptr() ? 1u : 0u;
    vkk::dispatch_flat("densify.densify_mcmc_update_reloc", {},
                       cur_num_splats, 256, &up, sizeof(up), &up.wgs_per_row);

    launch_copy_qsh_map(cur_num_splats, index_map.data_ptr(),
                        sh_value_packed.data_ptr(), sh_value_bounds.data_ptr(),
                        num_sh, num_sh_buffer, sh_value_bits,
                        sh_value_bounds_per_splat);
}

void add_splats_mcmc_tensor(
    int64_t cur_num_splats,
    int64_t num_add,
    float min_opacity,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,
    NonShQuantState non_sh,
    uint32_t seed
) {
    if (num_add == 0 || cur_num_splats <= 0) return;

    DeviceVector<float> probs, cumsum;
    mcmc_probs_cumsum(cur_num_splats, min_opacity, opacs, scales,
                      PoolSlot::DensifyMcmcAddSampleProbs,
                      PoolSlot::DensifyMcmcAddSampleProbsCumsum, probs,
                      cumsum);

    DeviceVector<int32_t> index_map, n_idx_buffer;
    index_map.resize(PoolSlot::DensifyMcmcAddIndexMap, num_add);
    n_idx_buffer.resize(PoolSlot::DensifyMcmcAddNIdxBuffer, cur_num_splats);
    backend::memset_sync(n_idx_buffer.data_ptr(), 0,
                         cur_num_splats * sizeof(int32_t));

    McmcAddIndexMapParams imp{};
    imp.sample_probs_cumsum = (uint64_t)cumsum.data_ptr();
    imp.index_map = (uint64_t)index_map.data_ptr();
    imp.n_idx_buffer = (uint64_t)n_idx_buffer.data_ptr();
    imp.num_splats = (uint32_t)cur_num_splats;
    imp.num_add = (uint32_t)num_add;
    imp.seed = seed;
    vkk::dispatch_flat("densify.densify_mcmc_add_index_map", {}, num_add, 256,
                       &imp, sizeof(imp), &imp.wgs_per_row);

    McmcComputeAddParams cp{};
    cp.n_idx_buffer = (uint64_t)n_idx_buffer.data_ptr();
    cp.scales = (uint64_t)scales.data_ptr();
    cp.opacs = (uint64_t)opacs.data_ptr();
    cp.num_splats = (uint32_t)cur_num_splats;
    vkk::dispatch_flat("densify.densify_mcmc_compute_add", {}, cur_num_splats,
                       256, &cp, sizeof(cp), &cp.wgs_per_row);

    McmcUpdateAddParams ap{};
    ap.index_map = (uint64_t)index_map.data_ptr();
    ap.means = (uint64_t)means.data_ptr();
    ap.quats = (uint64_t)quats.data_ptr();
    ap.scales = (uint64_t)scales.data_ptr();
    ap.opacs = (uint64_t)opacs.data_ptr();
    ap.features_dc = (uint64_t)features_dc.data_ptr();
    ap.features_sh = vkk::or_fallback(features_sh.data_ptr());
    ap.has_g1 = fill_g1g2(ap, g1_means, g2_means, g1_quats, g2_quats,
                          g1_scales, g2_scales, g1_opacs, g2_opacs,
                          g1_features_dc, g2_features_dc);
    uint32_t spec = fill_sh_state(ap, g1_features_sh, g2_features_sh,
                                  sh_quant_bounds, sh_bounds_per_splat,
                                  sh_optim_bits);
    ap.non_sh = fill_non_sh(ap, non_sh);
    ap.bias_steps = vkk::or_fallback(bias_correction_steps.data_ptr());
    ap.num_splats = (uint32_t)cur_num_splats;
    ap.num_add = (uint32_t)num_add;
    ap.num_sh = (uint32_t)num_sh;
    ap.has_fp32_sh = features_sh.data_ptr() ? 1u : 0u;
    ap.has_bias_steps = bias_correction_steps.data_ptr() ? 1u : 0u;
    {
        vkk::Fold f = vkk::fold_1d(num_add, 256);
        ap.wgs_per_row = f.per_row;
        vkk::dispatch_ring("densify.densify_mcmc_update_add",
                           backend::vk::SpecList{spec}, f.per_row, f.rows, 1,
                           &ap, sizeof(ap));
    }

    launch_copy_qsh_pairs(cur_num_splats, num_add, index_map.data_ptr(),
                          /*dst_indices=*/nullptr, sh_value_packed.data_ptr(),
                          sh_value_bounds.data_ptr(), num_sh, num_sh_buffer,
                          sh_value_bits, sh_value_bounds_per_splat);
}


// The quantile radix-select this shares with the CUDA path (Quantile.cpp).
// Positive-only: the map's background is tiny NEGATIVE noise, which an |x|
// population would put the median in.
template <bool invert_quantile>
int batch_quantile_positive_radix_select(const float* d_x, int B, int N,
                                         float q, float* d_out,
                                         uint32_t* temp,
                                         backend::Stream stream);

void normalize_clip_map_inplace_tensor(
    TorchTensorView data,
    bool normalize_median,
    float clip_quantile,
    float power
) {
    const bool do_clip = (clip_quantile > 0.0f && clip_quantile < 1.0f);
    if (!normalize_median && !do_clip && power == 1.0f) return;
    float* ptr = (float*)std::get<0>(data);
    if (ptr == nullptr) return;
    const auto& shape = std::get<2>(data);
    if (shape.empty()) return;
    int64_t total = 1;
    for (size_t i = 0; i < shape.size(); i++) total *= shape[i];
    if (shape[0] <= 0 || total <= 0) return;
    const int B = (int)shape[0];
    const int N = (int)(total / shape[0]);

    float* norm = DevicePool::global().acquire<float>(PoolSlot::DensifyMapNorm,
                                                      2 * (size_t)B);
    float* inv_median = norm;
    float* clip = norm + B;
    float* temp = DevicePool::global().acquire<float>(
        PoolSlot::DensifyQuantileTemp, 1024 * (size_t)B);

    if (normalize_median)
        batch_quantile_positive_radix_select<true>(ptr, B, N, 0.5f, inv_median,
                                                   (uint32_t*)temp,
                                                   backend::kDefaultStream);
    if (do_clip)
        batch_quantile_positive_radix_select<false>(ptr, B, N, clip_quantile,
                                                    clip, (uint32_t*)temp,
                                                    backend::kDefaultStream);

    NormalizeClipMapParams p{};
    p.data = (uint64_t)ptr;
    p.inv_median = (uint64_t)inv_median;
    p.clip = (uint64_t)clip;
    p.B = (uint32_t)B;
    p.N = (uint32_t)N;
    p.use_median = normalize_median ? 1u : 0u;
    p.use_clip = do_clip ? 1u : 0u;
    p.power = power;
    vkk::dispatch_flat("densify.normalize_clip_map", {}, total, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}


void densify_accum_finalize_tensor(int64_t num_splats,
                                   DeviceVector<float> accum) {
    if (num_splats <= 0 || accum.data_ptr() == nullptr) return;
    AccumFinalizeParams p{};
    p.accum = (uint64_t)accum.data_ptr();
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("densify.densify_accum_finalize", {}, num_splats, 256,
                       &p, sizeof(p), &p.wgs_per_row);
}

void densify_clip_score_tensor(int64_t num_splats,
                               DeviceVector<float2> accum_buffer,
                               float quantile,
                               float power,
                               DeviceVector<float2> score_out) {
    if (num_splats <= 0 || accum_buffer.data_ptr() == nullptr) return;
    const bool do_clip = (quantile > 0.0f && quantile < 1.0f);
    float2* out = score_out.data_ptr();
    if (!do_clip && out == nullptr) return;

    float* cutoff = nullptr;
    if (do_clip) {
        // The selector wants a contiguous row, and .x is strided; gather first.
        float* gathered = DevicePool::global().acquire<float>(
            PoolSlot::DensifyScoreGather, (size_t)num_splats);
        cutoff =
            DevicePool::global().acquire<float>(PoolSlot::DensifyScoreClip, 1);
        float* temp = DevicePool::global().acquire<float>(
            PoolSlot::DensifyQuantileTemp, 1024);

        ScorePairParams gp{};
        gp.accum_buffer = (uint64_t)accum_buffer.data_ptr();
        gp.other = (uint64_t)gathered;
        gp.num_splats = (uint32_t)num_splats;
        vkk::dispatch_flat("densify.densify_gather_score", {}, num_splats, 256,
                           &gp, sizeof(gp), &gp.wgs_per_row);

        batch_quantile_positive_radix_select<false>(gathered, 1,
                                                    (int)num_splats, quantile,
                                                    cutoff, (uint32_t*)temp,
                                                    backend::kDefaultStream);
    }

    ScoreClipParams p{};
    p.accum_buffer = (uint64_t)accum_buffer.data_ptr();
    p.cutoff = vkk::or_fallback(cutoff);
    p.score_out = vkk::or_fallback(out);
    p.power = power;
    p.use_cutoff = cutoff ? 1u : 0u;
    p.use_out = out ? 1u : 0u;
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("densify.densify_clip_score", {}, num_splats, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}
