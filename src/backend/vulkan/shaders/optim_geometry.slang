// Fused 3DGS geometry optimizer (Vulkan backend), mirroring
// fused_optim_3dgs_geometry_kernel in kernels/optim/FusedGeometryOptim.cu: one thread per
// splat (256/block), per-splat regularizer gradients, then in-place Adam on
// scales / quats (Riemannian) / opacities / means (optionally
// scale-agnostic), plus features_dc when non-SH Adam-state quantization is
// on (the fp32 path drives DC via a separate fused_adam_step launch). This
// is the non-FPBO geometry step: data gradients come from the fp32 v_*
// buffers or the block-wise 16-bit grad-quant codec, not from a camera loop.
//
// Spec constants:
//   kScaleAgnostic = use_scale_agnostic_mean
//   kZeroGrad      = zero fp32 grad buffers after consuming them
//   kNonShQuant    = non_sh.enabled (16-bit qadam for the five attributes)
//   kGqMask        = which grad-quant buffers exist (1 means, 2 quats,
//                    4 scales, 8 opac, 16 dc) — folded decode-vs-fp32 reads
//
// Barriers only exist in the kNonShQuant reduces, which every thread
// reaches (`inside` masks work, never control flow around a reduce).

#define CudaDeviceExport ForceInline
#include "shaders/per_splat_losses.slang"
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport
#include "optim_quant.slang"
#include "screen_size_hinge.slang"

[SpecializationConstant]
const int kScaleAgnostic = 0;
[SpecializationConstant]
const int kZeroGrad = 0;
[SpecializationConstant]
const int kNonShQuant = 0;
[SpecializationConstant]
const int kGqMask = 0;

static const float kGeoBeta1 = 0.9f;
static const float kGeoBeta2 = 0.999f;
static const float kGeoSh0 = 0.28209479177387814f;  // 1 / (2*sqrt(pi))

[ForceInline]
float _geo_sigmoid(float x) { return 1.0f / (1.0f + exp(-x)); }

struct OptimGeoParams {
    // params + fp32 grads + fp32 Adam state (grad/state slots may be
    // fallback-backed when grad-quant / non-SH quant replaces them)
    float* means;
    float* v_means;
    float* g1_means;
    float* g2_means;
    float* quats;
    float* v_quats;
    float* g1_quats;
    float* g2_quats;
    float* scales;
    float* v_scales;
    float* g1_scales;
    float* g2_scales;
    float* opacities;
    float* v_opacities;
    float* g1_opacities;
    float* g2_opacities;
    float* features_dc;    // consumed only under kNonShQuant
    float* v_features_dc;
    float* radii;          // (kScaleAgnostic)
    float* densify_score;  // optional output (has_densify_score)
    int* steps;            // per-splat steps (has_steps)
    // block-wise 16-bit quantized gradients (kGqMask; bound per 256 splats)
    uint* gq_means_packed;
    float2* gq_means_bounds;
    uint* gq_quats_packed;
    float2* gq_quats_bounds;
    uint* gq_scales_packed;
    float2* gq_scales_bounds;
    uint* gq_opac_packed;
    float2* gq_opac_bounds;
    uint* gq_dc_packed;
    float2* gq_dc_bounds;
    // non-SH quantized Adam state (kNonShQuant; 16-bit qadam, word/cell)
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
    // learning rates + regularizer weights (already divided by N host-side)
    float lr_means;
    float lr_quats;
    float lr_scales;
    float lr_opacs;
    float lr_features_dc;
    float max_gauss_ratio;
    float scale_regularization_weight;
    float mcmc_opacity_reg_weight;
    float mcmc_scale_reg_weight;
    float erank_reg_weight;
    float erank_reg_weight_s3;
    float quat_norm_reg_weight;
    float dc_reg_weight;
    float sh_reg_weight;
    float grad_scale;
    float max_screen_size;
    float max_screen_size_penalty;
    float _pad;
    int32_t scalar_step;
    uint32_t has_steps;
    uint32_t has_densify_score;
    uint32_t numel;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

// Data gradient for a PRIMS-wide attribute: decode from the 16-bit
// grad-quant codec when its buffers exist (per the kGqMask bit), else read
// the fp32 v_* buffer. Mirrors the per-attribute null-pattern dispatch in
// the CUDA kernel.
[ForceInline]
void _geo_grad_read<let PRIMS : int>(bool gq, uint* gq_packed,
                                     float2* gq_bounds, float* v_buf,
                                     uint block_id, uint gid,
                                     out float v[PRIMS]) {
    if (gq) {
        float2 mm = gq_bounds[block_id];
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++)
            v[i] = gradq_decode1<16>(gq_packed, uint(PRIMS) * gid + uint(i),
                                     mm);
    } else {
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++)
            v[i] = v_buf[PRIMS * gid + i];
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void fused_optim_3dgs_geometry(uint3 wg: SV_GroupID,
                               uint tid: SV_GroupThreadID,
                               uniform OptimGeoParams* pp) {
    OptimGeoParams p = *pp;
    uint block_id = wg.y * p.wgs_per_row + wg.x;
    uint gid = block_id * 256 + tid;
    bool inside = gid < p.numel;
    // Grid-fold tail: block_id can pad past the last quant block, so bounds
    // writes below are blk_ok-gated (optimizer.slang convention).
    bool blk_ok = block_id < (p.numel + 255u) / 256u;

    // Hoisted quantized-state registers + per-thread (u, log_s) bounds.
    // Zero-init: assigned under `inside` in the per-attribute update blocks
    // and consumed under the same guard by oq_state_encode below.
    float nq_g1_scale[3] = {}, nq_g2_scale[3] = {};
    float nq_g1_quat[4] = {}, nq_g2_quat[4] = {};
    float nq_g1_opac[1] = {}, nq_g2_opac[1] = {};
    float nq_g1_mean[3] = {}, nq_g2_mean[3] = {};
    float nq_g1_dc[3] = {}, nq_g2_dc[3] = {};
    float4 nq_mm_scale = float4(1e30f, -1e30f, 1e30f, -1e30f);
    float4 nq_mm_quat = float4(1e30f, -1e30f, 1e30f, -1e30f);
    float4 nq_mm_opac = float4(1e30f, -1e30f, 1e30f, -1e30f);
    float4 nq_mm_mean = float4(1e30f, -1e30f, 1e30f, -1e30f);
    float4 nq_mm_dc = float4(1e30f, -1e30f, 1e30f, -1e30f);

    if (inside) {
        float step = float(p.has_steps != 0 ? p.steps[gid] : p.scalar_step);
        float ibc1 = 1.0f / (1.0f - pow(kGeoBeta1, step));
        float ibc2 = 1.0f / (1.0f - pow(kGeoBeta2, step));
        float lr_means = p.lr_means * ibc1;
        float lr_quats = p.lr_quats * ibc1;
        float lr_scales = p.lr_scales * ibc1;
        float lr_opacs = p.lr_opacs * ibc1;
        float lr_features_dc = p.lr_features_dc * ibc1;

        // load params (quat normalized at load, as in CUDA)
        float3 scale = float3(p.scales[3 * gid], p.scales[3 * gid + 1],
                              p.scales[3 * gid + 2]);
        float4 quat = normalize(float4(p.quats[4 * gid], p.quats[4 * gid + 1],
                                       p.quats[4 * gid + 2],
                                       p.quats[4 * gid + 3]));
        float opac = p.opacities[gid];

        // regularizer gradients
        float v_losses[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        float3 v_scale;
        float v_opac;
        float4 v_quat;
        per_splat_losses_bwd(scale, opac, quat, v_losses, v_scale, v_opac,
                             v_quat, p.mcmc_opacity_reg_weight,
                             p.mcmc_scale_reg_weight, p.max_gauss_ratio,
                             p.scale_regularization_weight,
                             p.erank_reg_weight, p.erank_reg_weight_s3,
                             p.quat_norm_reg_weight);

        // data gradients (grad-quant codec or fp32 buffers)
        float gd_scale[3], gd_quat[4], gd_opac[1];
        _geo_grad_read<3>((kGqMask & 4) != 0, p.gq_scales_packed,
                          p.gq_scales_bounds, p.v_scales, block_id, gid,
                          gd_scale);
        _geo_grad_read<4>((kGqMask & 2) != 0, p.gq_quats_packed,
                          p.gq_quats_bounds, p.v_quats, block_id, gid,
                          gd_quat);
        _geo_grad_read<1>((kGqMask & 8) != 0, p.gq_opac_packed,
                          p.gq_opac_bounds, p.v_opacities, block_id, gid,
                          gd_opac);
        v_scale += p.grad_scale * float3(gd_scale[0], gd_scale[1],
                                         gd_scale[2]);
        v_quat += p.grad_scale * float4(gd_quat[0], gd_quat[1], gd_quat[2],
                                        gd_quat[3]);
        v_opac += p.grad_scale * gd_opac[0];
        if (kZeroGrad != 0) {
            if ((kGqMask & 4) == 0) {
                p.v_scales[3 * gid] = 0.0f;
                p.v_scales[3 * gid + 1] = 0.0f;
                p.v_scales[3 * gid + 2] = 0.0f;
            }
            if ((kGqMask & 2) == 0) {
                p.v_quats[4 * gid] = 0.0f;
                p.v_quats[4 * gid + 1] = 0.0f;
                p.v_quats[4 * gid + 2] = 0.0f;
                p.v_quats[4 * gid + 3] = 0.0f;
            }
            if ((kGqMask & 8) == 0) p.v_opacities[gid] = 0.0f;
        }

        // update scales
        {
            float g1[3], g2[3];
            oq_state_read<3>(kNonShQuant != 0, p.nq_scales_packed,
                             p.nq_scales_bounds, p.g1_scales, p.g2_scales,
                             block_id, gid, g1, g2);
            v_scale += screen_size_hinge_grad(
                p.radii[gid], p.max_screen_size, p.max_screen_size_penalty,
                scale,
                oq_sqrt3(float3(g2[0], g2[1], g2[2]) * ibc2) +
                    float3(kAdamEps));
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                g1[i] = kGeoBeta1 * g1[i] + (1.0f - kGeoBeta1) * v_scale[i];
                g2[i] = kGeoBeta2 * g2[i] +
                        (1.0f - kGeoBeta2) * v_scale[i] * v_scale[i];
            }
            float3 g1v = float3(g1[0], g1[1], g1[2]);
            float3 g2v = float3(g2[0], g2[1], g2[2]);
            float3 new_scale = scale - lr_scales * g1v /
                                           (oq_sqrt3(g2v * ibc2) +
                                            float3(kAdamEps));
            p.scales[3 * gid] = new_scale.x;
            p.scales[3 * gid + 1] = new_scale.y;
            p.scales[3 * gid + 2] = new_scale.z;
            if (kNonShQuant != 0) {
                [ForceUnroll]
                for (int i = 0; i < 3; i++) {
                    nq_g1_scale[i] = g1[i];
                    nq_g2_scale[i] = g2[i];
                }
                oq_accum_us<3>(g1, g2, nq_mm_scale);
            } else {
                [ForceUnroll]
                for (int i = 0; i < 3; i++) {
                    p.g1_scales[3 * gid + i] = g1[i];
                    p.g2_scales[3 * gid + i] = g2[i];
                }
            }
        }

        // update quats (Riemannian)
        {
            v_quat -= dot(quat, v_quat) * quat;
            float g1[4], g2[4];
            oq_state_read<4>(kNonShQuant != 0, p.nq_quats_packed,
                             p.nq_quats_bounds, p.g1_quats, p.g2_quats,
                             block_id, gid, g1, g2);
            [ForceUnroll]
            for (int i = 0; i < 4; i++) {
                g1[i] = kGeoBeta1 * g1[i] + (1.0f - kGeoBeta1) * v_quat[i];
                g2[i] = kGeoBeta2 * g2[i] +
                        (1.0f - kGeoBeta2) * v_quat[i] * v_quat[i];
            }
            float4 g1v = float4(g1[0], g1[1], g1[2], g1[3]);
            float4 g2v = float4(g2[0], g2[1], g2[2], g2[3]);
            float4 new_quat = normalize(
                quat - lr_quats * g1v /
                           (oq_sqrt4(g2v * ibc2) + float4(kAdamEps)));
            p.quats[4 * gid] = new_quat.x;
            p.quats[4 * gid + 1] = new_quat.y;
            p.quats[4 * gid + 2] = new_quat.z;
            p.quats[4 * gid + 3] = new_quat.w;
            if (kNonShQuant != 0) {
                [ForceUnroll]
                for (int i = 0; i < 4; i++) {
                    nq_g1_quat[i] = g1[i];
                    nq_g2_quat[i] = g2[i];
                }
                oq_accum_us<4>(g1, g2, nq_mm_quat);
            } else {
                [ForceUnroll]
                for (int i = 0; i < 4; i++) {
                    p.g1_quats[4 * gid + i] = g1[i];
                    p.g2_quats[4 * gid + i] = g2[i];
                }
            }
        }

        // update opacities
        {
            float g1[1], g2[1];
            oq_state_read<1>(kNonShQuant != 0, p.nq_opacities_packed,
                             p.nq_opacities_bounds, p.g1_opacities,
                             p.g2_opacities, block_id, gid, g1, g2);
            g1[0] = kGeoBeta1 * g1[0] + (1.0f - kGeoBeta1) * v_opac;
            g2[0] = kGeoBeta2 * g2[0] + (1.0f - kGeoBeta2) * v_opac * v_opac;
            float new_opac = opac - lr_opacs * g1[0] /
                                        (sqrt(max(g2[0] * ibc2, 0.0f)) +
                                         kAdamEps);
            p.opacities[gid] = new_opac;
            if (kNonShQuant != 0) {
                nq_g1_opac[0] = g1[0];
                nq_g2_opac[0] = g2[0];
                oq_accum_us<1>(g1, g2, nq_mm_opac);
            } else {
                p.g1_opacities[gid] = g1[0];
                p.g2_opacities[gid] = g2[0];
            }
        }

        // update means (optionally scale-agnostic)
        {
            float3 mean = float3(p.means[3 * gid], p.means[3 * gid + 1],
                                 p.means[3 * gid + 2]);
            float gd_mean[3];
            _geo_grad_read<3>((kGqMask & 1) != 0, p.gq_means_packed,
                              p.gq_means_bounds, p.v_means, block_id, gid,
                              gd_mean);
            float3 v_mean = p.grad_scale * float3(gd_mean[0], gd_mean[1],
                                                  gd_mean[2]);
            if (kZeroGrad != 0 && (kGqMask & 1) == 0) {
                p.v_means[3 * gid] = 0.0f;
                p.v_means[3 * gid + 1] = 0.0f;
                p.v_means[3 * gid + 2] = 0.0f;
            }
            // densification score: ||dL/dmean_world|| * max post-exp world
            // scale (pre-update log scales).
            if (p.has_densify_score != 0) {
                float s_max = max(max(scale.x, scale.y), scale.z);
                p.densify_score[gid] =
                    exp(s_max) * sqrt(dot(v_mean, v_mean));
            }
            float g1[3], g2[3];
            oq_state_read<3>(kNonShQuant != 0, p.nq_means_packed,
                             p.nq_means_bounds, p.g1_means, p.g2_means,
                             block_id, gid, g1, g2);
            if (kScaleAgnostic != 0) {
                float3 v_num =
                    apply_covar_to_vec(
                        quat,
                        float3(exp(0.5f * scale.x), exp(0.5f * scale.y),
                               exp(0.5f * scale.z)),
                        v_mean) *
                    sqrt(2.0f * log(max(255.0f * _geo_sigmoid(opac),
                                        1.00001f)));
                float v_den = p.radii[gid] * 0.6f;
                [ForceUnroll]
                for (int i = 0; i < 3; i++) {
                    g1[i] = kGeoBeta1 * g1[i] + (1.0f - kGeoBeta1) * v_num[i];
                    g2[i] = kGeoBeta2 * g2[i] + (1.0f - kGeoBeta2) *
                                v_mean[i] * v_mean[i] * v_den * v_den;
                }
            } else {
                [ForceUnroll]
                for (int i = 0; i < 3; i++) {
                    g1[i] = kGeoBeta1 * g1[i] +
                            (1.0f - kGeoBeta1) * v_mean[i];
                    g2[i] = kGeoBeta2 * g2[i] +
                            (1.0f - kGeoBeta2) * v_mean[i] * v_mean[i];
                }
            }
            float3 g1v = float3(g1[0], g1[1], g1[2]);
            float3 g2v = float3(g2[0], g2[1], g2[2]);
            float3 new_mean = mean - lr_means * g1v /
                                         (oq_sqrt3(g2v * ibc2) +
                                          float3(kAdamEps));
            p.means[3 * gid] = new_mean.x;
            p.means[3 * gid + 1] = new_mean.y;
            p.means[3 * gid + 2] = new_mean.z;
            if (kNonShQuant != 0) {
                [ForceUnroll]
                for (int i = 0; i < 3; i++) {
                    nq_g1_mean[i] = g1[i];
                    nq_g2_mean[i] = g2[i];
                }
                oq_accum_us<3>(g1, g2, nq_mm_mean);
            } else {
                [ForceUnroll]
                for (int i = 0; i < 3; i++) {
                    p.g1_means[3 * gid + i] = g1[i];
                    p.g2_means[3 * gid + i] = g2[i];
                }
            }
        }

        // update features_dc (only under kNonShQuant; the fp32 path keeps
        // the separate fused_adam_step launch for features_dc)
        if (kNonShQuant != 0) {
            float3 fdc = float3(p.features_dc[3 * gid],
                                p.features_dc[3 * gid + 1],
                                p.features_dc[3 * gid + 2]);
            float gd_dc[3];
            _geo_grad_read<3>((kGqMask & 16) != 0, p.gq_dc_packed,
                              p.gq_dc_bounds, p.v_features_dc, block_id, gid,
                              gd_dc);
            float3 v_dc = p.grad_scale * float3(gd_dc[0], gd_dc[1],
                                                gd_dc[2]);
            if (kZeroGrad != 0 && (kGqMask & 16) == 0) {
                p.v_features_dc[3 * gid] = 0.0f;
                p.v_features_dc[3 * gid + 1] = 0.0f;
                p.v_features_dc[3 * gid + 2] = 0.0f;
            }
            // L1-shrinkage DC color regularization (mirrors the fp32 path's
            // fused_adam_step l2_reg + offset = 0.5/kSh0).
            const float dc_off = 0.5f / kGeoSh0;
            float under_reg_weight = p.dc_reg_weight + p.sh_reg_weight;
            v_dc.x += p.dc_reg_weight * max(fdc.x - dc_off, 0.0f) +
                under_reg_weight * min(fdc.x + dc_off, 0.0f);
            v_dc.y += p.dc_reg_weight * max(fdc.y - dc_off, 0.0f) +
                under_reg_weight * min(fdc.y + dc_off, 0.0f);
            v_dc.z += p.dc_reg_weight * max(fdc.z - dc_off, 0.0f) +
                under_reg_weight * min(fdc.z + dc_off, 0.0f);

            float g1[3], g2[3];
            oq_state_read<3>(kNonShQuant != 0, p.nq_dc_packed, p.nq_dc_bounds,
                             p.g1_means, p.g2_means, block_id, gid, g1, g2);
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                g1[i] = kGeoBeta1 * g1[i] + (1.0f - kGeoBeta1) * v_dc[i];
                g2[i] = kGeoBeta2 * g2[i] +
                        (1.0f - kGeoBeta2) * v_dc[i] * v_dc[i];
            }
            float3 g1v = float3(g1[0], g1[1], g1[2]);
            float3 g2v = float3(g2[0], g2[1], g2[2]);
            float3 new_dc = fdc - lr_features_dc * g1v /
                                      (oq_sqrt3(g2v * ibc2) +
                                       float3(kAdamEps));
            p.features_dc[3 * gid] = new_dc.x;
            p.features_dc[3 * gid + 1] = new_dc.y;
            p.features_dc[3 * gid + 2] = new_dc.z;
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                nq_g1_dc[i] = g1[i];
                nq_g2_dc[i] = g2[i];
            }
            oq_accum_us<3>(g1, g2, nq_mm_dc);
        }
    }  // inside

    // Non-SH Adam-state quant: block-reduce per-attribute bounds, write,
    // encode against the fresh block-wide bound. All threads participate;
    // outside-threads pass sentinels.
    if (kNonShQuant != 0) {
        nq_mm_scale = oq_block_reduce_mm4(nq_mm_scale, tid);
        if (tid == 0 && blk_ok) p.nq_scales_bounds[block_id] = nq_mm_scale;
        nq_mm_quat = oq_block_reduce_mm4(nq_mm_quat, tid);
        if (tid == 0 && blk_ok) p.nq_quats_bounds[block_id] = nq_mm_quat;
        nq_mm_opac = oq_block_reduce_mm4(nq_mm_opac, tid);
        if (tid == 0 && blk_ok) p.nq_opacities_bounds[block_id] = nq_mm_opac;
        nq_mm_mean = oq_block_reduce_mm4(nq_mm_mean, tid);
        if (tid == 0 && blk_ok) p.nq_means_bounds[block_id] = nq_mm_mean;
        nq_mm_dc = oq_block_reduce_mm4(nq_mm_dc, tid);
        if (tid == 0 && blk_ok) p.nq_dc_bounds[block_id] = nq_mm_dc;
        if (inside) {
            oq_state_encode<3>(p.nq_scales_packed, gid, nq_g1_scale,
                               nq_g2_scale, nq_mm_scale);
            oq_state_encode<4>(p.nq_quats_packed, gid, nq_g1_quat,
                               nq_g2_quat, nq_mm_quat);
            oq_state_encode<1>(p.nq_opacities_packed, gid, nq_g1_opac,
                               nq_g2_opac, nq_mm_opac);
            oq_state_encode<3>(p.nq_means_packed, gid, nq_g1_mean,
                               nq_g2_mean, nq_mm_mean);
            oq_state_encode<3>(p.nq_dc_packed, gid, nq_g1_dc, nq_g2_dc,
                               nq_mm_dc);
        }
    }
}
