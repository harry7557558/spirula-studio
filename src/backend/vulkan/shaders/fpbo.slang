// Fused projection-backward + optimizer (Vulkan backend), mirroring
// kernels/optim/FusedProjectionBwdOptim_kernel.cuh: one thread per splat,
// 256 per group -- camera-loop register gradient accumulation, per-splat
// regularizers, then the in-place Adam update of every attribute.
//
// Spec constants 0..4 come from proj_bwd_common.slang; this file adds
// kEval3d (3DGUT screen-grad layout), kScaleAgnostic, kColorTrust, kLevel
// (1 = 8-bit qadam SH state + 16-bit value codec,
// docs/notes/sh-quant-layout.md), kNonShQuant, kVwMask (which
// v_splats_world tensors exist), and kDistortion, declared last.
#define CudaDeviceExport ForceInline
#include "aabb16.slang"
#include "int64_compat.slang"
#include "screen.slang"
#include "shaders/primitive_3dgs.slang"
#include "shaders/harmonics.slang"
#include "shaders/per_splat_losses.slang"
#include "shaders/pixel_wise.slang"
#undef CudaDeviceExport
#include "sh_quant.slang"
#include "sh_vjp.slang"
#include "atomic_float.slang"
#include "proj_bwd_common.slang"
#include "optim_quant.slang"
#include "screen_size_hinge.slang"

[SpecializationConstant]
const int kEval3d = 0;
[SpecializationConstant]
const int kScaleAgnostic = 0;
[SpecializationConstant]
const int kColorTrust = 0;
[SpecializationConstant]
const int kLevel = 0;
[SpecializationConstant]
const int kNonShQuant = 0;
[SpecializationConstant]
const int kVwMask = 0;

static const float kFpboEps = 1e-15f;
static const float kFpboBeta1 = 0.9f;
static const float kFpboBeta2 = 0.999f;
static const float kSh0 = 0.28209479177387814f;

struct FpboParams {
    // splat world (updated in place)
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    // cameras
    float* viewmats;
    float4* intrins;
    float* dist_coeffs;
    // packed intersection ranges (kPacked)
    int* camera_id_bounds;
    int* camera_ids;
    uint2* aabb;        // packed, aabb16.slang
    // packed screen-space gradient rows (SCR2_* for 2D, SCRG_* for 3DGUT)
    float* vs_screen;
    // world-space gradients from rasterization backward (kVwMask)
    float* vw_means;
    float* vw_quats;
    float* vw_scales;
    float* vw_opacities;
    float* vw_dc;
    // fp32 Adam state (g1/g2 world buffers; SH slots also used at kLevel 0)
    float* g1_means;
    float* g1_quats;
    float* g1_scales;
    float* g1_opacities;
    float* g1_dc;
    float* g1_sh;
    float* g2_means;
    float* g2_quats;
    float* g2_scales;
    float* g2_opacities;
    float* g2_dc;
    float* g2_sh;
    // SH quantized Adam state + value codec (kLevel 1)
    uint* sh_packed;         // 8-bit qadam AoS, halfword/cell
    float4* sh_quant_bounds; // per 256-splat block
    uint* sh_value_packed;   // 16-bit value codec, halfword/cell
    float2* sh_value_bounds; // per 256-splat block
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
    // optimizer inputs
    float* radii;          // (kScaleAgnostic)
    float* densify_score;  // optional output (has_densify_score)
    int* steps;            // per-splat steps (has_steps)
    // learning rates + regularizer weights (already divided by N host-side)
    float lr_means;
    float lr_quats;
    float lr_scales;
    float lr_opacs;
    float lr_features_dc;
    float lr_features_sh;
    float max_gauss_ratio;
    float scale_regularization_weight;
    float mcmc_opacity_reg_weight;
    float mcmc_scale_reg_weight;
    float erank_reg_weight;
    float erank_reg_weight_s3;
    float quat_norm_reg_weight;
    float dc_reg_weight;
    float sh_reg_weight;
    float max_screen_size;
    float max_screen_size_penalty;
    float eps_tr;
    int32_t scalar_step;
    uint32_t has_steps;
    uint32_t has_densify_score;
    uint32_t C;
    uint32_t N;
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};

[ForceInline]
float _fp_sigmoid(float x) { return 1.0f / (1.0f + exp(-x)); }

// Non-SH attribute Adam-state IO + clamped sqrt helpers live in
// optim_quant.slang (oq_state_read / oq_accum_us / oq_state_encode /
// oq_sqrt3 / oq_sqrt4), shared with optim_geometry.slang.

// The per-splat camera loop, generic over the two projection spec axes so the
// entry point folds them once (its block reduces stay outside).
[ForceInline]
void _fpbo_camera_loop<let cam : CameraModelType, D : ICameraDistortion>(
    FpboParams p, PbSplat s, uint gid, int cid_0, int cid_1, gindex_t qbase,
    gindex_t vstride, gindex_t vpitch, inout PbGrads g) {
    for (int cid_t = cid_0; cid_t < cid_1; ++cid_t) {
        // Packed: the forward emits (gaussian, camera) order, so cid_t is
        // the out_idx that indexes aabb / camera_ids / vs_screen.
        int idx = (kPacked != 0) ? cid_t : cid_t * int(p.N) + int(gid);
        int cid = (kPacked != 0) ? p.camera_ids[idx] : cid_t;
        if (aabb16_is_empty(p.aabb[idx]))
            continue;
        PbCamera<D> c = pb_load_camera<D>(p.viewmats, p.intrins,
                                          p.dist_coeffs, uint(cid));
        if (kEval3d != 0) {
            float* v_row = scrg_row(p.vs_screen, idx);
            float3 v_ss = scrg_scale(v_row);
            float v_so = scrg_opac(v_row);
            float3 v_rgb = scrg_rgb(v_row);
            pb_vjp_3dgut<cam, D>(s, c, p.width, p.height, v_ss, v_so, g);
            pb_sh_vjp<D>(s, c, p.features_sh + gid * 3 * p.num_sh_buffer,
                         p.sh_value_packed, p.sh_value_bounds, qbase,
                         vstride, vpitch, v_rgb, g);
        } else {
            float* v_row = scr2_row(p.vs_screen, idx);
            float2 v_xy = scr2_xy(v_row);
            float v_depth = scr2_depth(v_row);
            float3 v_conic = scr2_conic(v_row);
            float v_opac = scr2_opac(v_row);
            float3 v_rgb = scr2_rgb(v_row);
            pb_vjp_2d<cam, D>(s, c, p.width, p.height, v_xy, v_depth, v_conic,
                              v_opac, g);
            pb_sh_vjp<D>(s, c, p.features_sh + gid * 3 * p.num_sh_buffer,
                         p.sh_value_packed, p.sh_value_bounds, qbase,
                         vstride, vpitch, v_rgb, g);
        }
    }
}

#define _fpbo_call(M, D) \
    _fpbo_camera_loop<M, D>(p, s, gid, cid_0, cid_1, qbase, vstride, vpitch, g)

[shader("compute")]
[numthreads(256, 1, 1)]
void fpbo(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
          uniform FpboParams* pp) {
    FpboParams p = *pp;
    uint block_id = wg.y * p.wgs_per_row + wg.x;
    uint gid = block_id * 256 + tid;
    bool inside = gid < p.N;
    // Grid-fold tail: block_id can pad past the last quant block, so bounds
    // writes below are blk_ok-gated (optimizer.slang convention).
    bool blk_ok = block_id < (p.N + 255u) / 256u;

    int cid_0 = 0, cid_1 = int(p.C);
    if (kPacked != 0) {
        cid_0 = inside ? p.camera_id_bounds[gid] : 0;
        cid_1 = inside ? p.camera_id_bounds[gid + 1] : 0;
    }

    // Zero-initialized, not merely declared: every read is under the same
    // `inside` guard as the load, but slangc 2026.12.1 (the macOS pin) cannot
    // see that and warns 160 times across this file's variants.
    PbSplat s = {};
    PbGrads g = pb_grads_zero();
    if (inside) {
        s = pb_load_splat(p.means, p.quats, p.scales, p.opacities,
                          p.features_dc, gid);

        // v_splats_world contributions from rasterization backward
        // (World::atomicLoad; kVwMask folds absent tensors away).
        if ((kVwMask & 1) != 0)
            g.v_mean += float3(p.vw_means[3 * gid], p.vw_means[3 * gid + 1],
                               p.vw_means[3 * gid + 2]);
        if ((kVwMask & 2) != 0)
            g.v_quat += float4(p.vw_quats[4 * gid], p.vw_quats[4 * gid + 1],
                               p.vw_quats[4 * gid + 2],
                               p.vw_quats[4 * gid + 3]);
        if ((kVwMask & 4) != 0)
            g.v_scale += float3(p.vw_scales[3 * gid],
                                p.vw_scales[3 * gid + 1],
                                p.vw_scales[3 * gid + 2]);
        if ((kVwMask & 8) != 0)
            g.v_opacity += p.vw_opacities[gid];
        if ((kVwMask & 16) != 0)
            g.v_dc += float3(p.vw_dc[3 * gid], p.vw_dc[3 * gid + 1],
                             p.vw_dc[3 * gid + 2]);

        // camera loop (register accumulation, no atomics)
        // kLevel 1: source SH reads go through the 16-bit value codec in
        // the FPBO layout (docs/notes/sh-quant-layout.md).
        ShAddrW sha = sh_quant_addr_w(p.num_sh_buffer, 0);
        gindex_t qbase = sh_addr_base(sha, gid);
        gindex_t vstride = sha.bounds_stride;
        gindex_t vpitch = sha.pair_pitch;
        SS_DISPATCH_CAM_DIST(_fpbo_call)

        // per-splat regularizers -> gradient
        float v_losses[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        float3 v_scale_t;
        float v_opac_t;
        float4 v_quat_t;
        per_splat_losses_bwd(s.scale, s.in_opacity, s.quat, v_losses,
                             v_scale_t, v_opac_t, v_quat_t,
                             p.mcmc_opacity_reg_weight,
                             p.mcmc_scale_reg_weight, p.max_gauss_ratio,
                             p.scale_regularization_weight,
                             p.erank_reg_weight, p.erank_reg_weight_s3,
                             p.quat_norm_reg_weight);
        g.v_scale += v_scale_t;
        g.v_quat += v_quat_t;
        g.v_opacity += v_opac_t;
        g.v_dc += p.dc_reg_weight * max(s.dc - float3(0.5f / kSh0), float3(0.0f)) +
            (p.dc_reg_weight + p.sh_reg_weight) * min(s.dc + float3(0.5f / kSh0), float3(0.0f));
    }

    // ---- optimizer ----
    float step = 0.0f;
    if (inside)
        step = float(p.has_steps != 0 ? p.steps[gid] : p.scalar_step);
    float ibc1 = 1.0f / (1.0f - pow(kFpboBeta1, step));
    float ibc2 = 1.0f / (1.0f - pow(kFpboBeta2, step));

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

    // update scales
    if (inside) {
        float g1[3], g2[3];
        oq_state_read<3>(kNonShQuant != 0, p.nq_scales_packed,
                          p.nq_scales_bounds, p.g1_scales, p.g2_scales,
                          block_id, gid, g1, g2);
        g.v_scale += screen_size_hinge_grad(
            p.radii[gid], p.max_screen_size, p.max_screen_size_penalty,
            s.scale,
            oq_sqrt3(float3(g2[0], g2[1], g2[2]) * ibc2) + float3(kFpboEps));
        [ForceUnroll]
        for (int i = 0; i < 3; i++) {
            g1[i] = kFpboBeta1 * g1[i] + (1.0f - kFpboBeta1) * g.v_scale[i];
            g2[i] = kFpboBeta2 * g2[i] +
                    (1.0f - kFpboBeta2) * g.v_scale[i] * g.v_scale[i];
        }
        float3 g1v = float3(g1[0], g1[1], g1[2]);
        float3 g2v = float3(g2[0], g2[1], g2[2]);
        float3 new_scale =
            s.scale - p.lr_scales * ibc1 * g1v /
                          (oq_sqrt3(g2v * ibc2) + float3(kFpboEps));
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
    if (inside) {
        float4 new_quat = normalize(s.quat);
        g.v_quat -= dot(new_quat, g.v_quat) * new_quat;
        float g1[4], g2[4];
        oq_state_read<4>(kNonShQuant != 0, p.nq_quats_packed,
                          p.nq_quats_bounds, p.g1_quats, p.g2_quats,
                          block_id, gid, g1, g2);
        [ForceUnroll]
        for (int i = 0; i < 4; i++) {
            g1[i] = kFpboBeta1 * g1[i] + (1.0f - kFpboBeta1) * g.v_quat[i];
            g2[i] = kFpboBeta2 * g2[i] +
                    (1.0f - kFpboBeta2) * g.v_quat[i] * g.v_quat[i];
        }
        float4 g1v = float4(g1[0], g1[1], g1[2], g1[3]);
        float4 g2v = float4(g2[0], g2[1], g2[2], g2[3]);
        new_quat -= p.lr_quats * ibc1 * g1v /
                    (oq_sqrt4(g2v * ibc2) + float4(kFpboEps));
        new_quat = normalize(new_quat);
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
    if (inside) {
        float g1[1], g2[1];
        oq_state_read<1>(kNonShQuant != 0, p.nq_opacities_packed,
                          p.nq_opacities_bounds, p.g1_opacities,
                          p.g2_opacities, block_id, gid, g1, g2);
        g1[0] = kFpboBeta1 * g1[0] + (1.0f - kFpboBeta1) * g.v_opacity;
        g2[0] = kFpboBeta2 * g2[0] +
                (1.0f - kFpboBeta2) * g.v_opacity * g.v_opacity;
        float new_opac = s.in_opacity -
                         p.lr_opacs * ibc1 * g1[0] /
                             (sqrt(max(g2[0] * ibc2, 0.0f)) + kFpboEps);
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

    // densification score: ||dL/dmean_world|| * max post-exp world scale
    if (inside && p.has_densify_score != 0) {
        float s_max = max(max(s.scale.x, s.scale.y), s.scale.z);
        p.densify_score[gid] =
            exp(s_max) * sqrt(dot(g.v_mean, g.v_mean));
    }

    // update means (optionally scale-agnostic)
    if (inside) {
        float g1[3], g2[3];
        oq_state_read<3>(kNonShQuant != 0, p.nq_means_packed,
                          p.nq_means_bounds, p.g1_means, p.g2_means,
                          block_id, gid, g1, g2);
        if (kScaleAgnostic != 0) {
            float3 v_num =
                apply_covar_to_vec(
                    s.quat,
                    float3(exp(0.5f * s.scale.x), exp(0.5f * s.scale.y),
                           exp(0.5f * s.scale.z)),
                    g.v_mean) *
                sqrt(2.0f * log(max(255.0f * _fp_sigmoid(s.in_opacity),
                                    1.00001f)));
            float v_den = p.radii[gid] * 0.6f;
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                g1[i] = kFpboBeta1 * g1[i] + (1.0f - kFpboBeta1) * v_num[i];
                g2[i] = kFpboBeta2 * g2[i] + (1.0f - kFpboBeta2) *
                            g.v_mean[i] * g.v_mean[i] * v_den * v_den;
            }
        } else {
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                g1[i] = kFpboBeta1 * g1[i] +
                        (1.0f - kFpboBeta1) * g.v_mean[i];
                g2[i] = kFpboBeta2 * g2[i] +
                        (1.0f - kFpboBeta2) * g.v_mean[i] * g.v_mean[i];
            }
        }
        float3 g1v = float3(g1[0], g1[1], g1[2]);
        float3 g2v = float3(g2[0], g2[1], g2[2]);
        float3 new_mean =
            s.mean - p.lr_means * ibc1 * g1v /
                         (oq_sqrt3(g2v * ibc2) + float3(kFpboEps));
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

    // update features_dc
    if (inside) {
        float3 v_dc = g.v_dc;
        if (kColorTrust != 0) {
            v_dc.x /= linear_rgb_to_srgb_grad(kSh0 * s.dc.x + 0.5f);
            v_dc.y /= linear_rgb_to_srgb_grad(kSh0 * s.dc.y + 0.5f);
            v_dc.z /= linear_rgb_to_srgb_grad(kSh0 * s.dc.z + 0.5f);
        }
        float g1[3], g2[3];
        oq_state_read<3>(kNonShQuant != 0, p.nq_dc_packed, p.nq_dc_bounds,
                          p.g1_dc, p.g2_dc, block_id, gid, g1, g2);
        [ForceUnroll]
        for (int i = 0; i < 3; i++) {
            g1[i] = kFpboBeta1 * g1[i] + (1.0f - kFpboBeta1) * v_dc[i];
            g2[i] = kFpboBeta2 * g2[i] +
                    (1.0f - kFpboBeta2) * v_dc[i] * v_dc[i];
        }
        float3 g1v = float3(g1[0], g1[1], g1[2]);
        float3 g2v = float3(g2[0], g2[1], g2[2]);
        float3 denom = oq_sqrt3(g2v * ibc2) + float3(kFpboEps);
        float3 delta = float3(-p.lr_features_dc * ibc1) * (g1v / denom);
        float3 new_dc;
        if (kColorTrust != 0) {
            float opac = _fp_sigmoid(s.in_opacity);
            float clip = kSh0 * sqrt(2.0f * p.eps_tr / max(opac, 1e-12f));
            delta = clamp(delta, float3(-clip), float3(clip));
            delta.x = isfinite(delta.x) ? delta.x : 0.0f;
            delta.y = isfinite(delta.y) ? delta.y : 0.0f;
            delta.z = isfinite(delta.z) ? delta.z : 0.0f;
            new_dc = float3(splat_dc_decode(splat_dc_encode(s.dc.x) + delta.x),
                            splat_dc_decode(splat_dc_encode(s.dc.y) + delta.y),
                            splat_dc_decode(splat_dc_encode(s.dc.z) + delta.z));
        } else {
            new_dc = s.dc + delta;
        }
        p.features_dc[3 * gid] = new_dc.x;
        p.features_dc[3 * gid + 1] = new_dc.y;
        p.features_dc[3 * gid + 2] = new_dc.z;
        if (kNonShQuant != 0) {
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                nq_g1_dc[i] = g1[i];
                nq_g2_dc[i] = g2[i];
            }
            oq_accum_us<3>(g1, g2, nq_mm_dc);
        } else {
            [ForceUnroll]
            for (int i = 0; i < 3; i++) {
                p.g1_dc[3 * gid + i] = g1[i];
                p.g2_dc[3 * gid + i] = g2[i];
            }
        }
    }

    // Non-SH Adam-state quant: block-reduce per-attribute bounds, write,
    // encode against the fresh block-wide bound.
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

    // ---- update features_sh ----
    const int num_sh = kShDegree * (kShDegree + 2);
    float lr_sh = p.lr_features_sh * ibc1;
    const float reg_weight =
        p.sh_reg_weight * (1.0f / max(float(num_sh), 1.0f));

    // Per-channel DC color + clip radius for the trust-region SH update.
    float3 c_dc = float3(0.0f);
    float3 clip_dc = float3(0.0f);
    if (kColorTrust != 0 && inside) {
        c_dc = kSh0 * s.dc + float3(0.5f);
        float3 c_dc_clamped =
            max(c_dc, float3((1.0f / 255.0f) * (1.0f / 255.0f)));
        float inv_opac = 1.0f / max(_fp_sigmoid(s.in_opacity), 1e-12f);
        clip_dc = kSh0 *
                  oq_sqrt3(float3(4.0f * p.eps_tr * c_dc_clamped.x * inv_opac,
                                   4.0f * p.eps_tr * c_dc_clamped.y * inv_opac,
                                   4.0f * p.eps_tr * c_dc_clamped.z *
                                       inv_opac));
    }

    if (kLevel == 0) {
        // fp32 SH state + values (no barriers below: early-out is safe).
        if (!inside)
            return;
        uint sh_off = gid * 3u * p.num_sh_buffer;
        for (int i = 0; i < 3 * num_sh; ++i) {
            float sh_coeff = p.features_sh[sh_off + uint(i)];
            float v_sh_coeff =
                g.v_sh[i / 3][i % 3] + reg_weight * sh_coeff;
            const int ch = i % 3;
            if (kColorTrust != 0) {
                float c_ch = (ch == 0) ? c_dc.x : (ch == 1) ? c_dc.y
                                                            : c_dc.z;
                v_sh_coeff /= linear_rgb_to_srgb_grad(c_ch);
            }
            float g1 = kFpboBeta1 * p.g1_sh[sh_off + uint(i)] +
                       (1.0f - kFpboBeta1) * v_sh_coeff;
            float g2 = kFpboBeta2 * p.g2_sh[sh_off + uint(i)] +
                       (1.0f - kFpboBeta2) * v_sh_coeff * v_sh_coeff;
            float delta = -lr_sh * g1 /
                          (sqrt(max(g2 * ibc2, 0.0f)) + kFpboEps);
            if (kColorTrust != 0) {
                float clip = (ch == 0) ? clip_dc.x : (ch == 1) ? clip_dc.y
                                                               : clip_dc.z;
                delta = clamp(delta, -clip, clip);
                delta = isfinite(delta) ? delta : 0.0f;
            }
            p.features_sh[sh_off + uint(i)] = sh_coeff + delta;
            p.g1_sh[sh_off + uint(i)] = g1;
            p.g2_sh[sh_off + uint(i)] = g2;
        }
        return;
    }

    // kLevel == 1: 8-bit joint (u, sqrt_g2) Adam state + 16-bit value codec,
    // FPBO per-splat-block layouts (one bound per 256 splats).
    // No active SH bands -> nothing to update, and the inactive cells need
    // no zero-encode either: the buffers' zero-init state already decodes
    // to 0 (codec fixed point). Mirrors the CUDA kernel, whose whole SH
    // block is `else if constexpr (num_sh > 0)`. Spec-folded and uniform,
    // so skipping past the staging barriers is safe.
    if (num_sh == 0)
        return;
    {
        ShAddrW shq = sh_quant_addr_w(p.num_sh_buffer, 0);
        const gindex_t sh_base = sh_addr_base(shq, gid);
        float4 quant_bounds =
            inside ? p.sh_quant_bounds[block_id]
                   : float4(0.0f, 0.0f, 0.0f, 0.0f);
        float4 mm = float4(1e30f, -1e30f, 1e30f, -1e30f);
        float2 sh_value_old_mm =
            inside ? p.sh_value_bounds[block_id] : float2(0.0f, 0.0f);
        float2 sh_value_new_mm = float2(1e30f, -1e30f);

        // Held until after the bounds reduce the encode ranges come from,
        // as the codec's (u, log_s) primitives rather than (g1, g2) so
        // reduce and encode share one qadam_g1g2_to_us per cell.
        float3 sh_updated_vals[24] = {};
        float3 u_sh_vals[24] = {};
        float3 s_sh_vals[24] = {};

        if (inside) {
            for (int j = 0; j < num_sh; ++j) {
                float3 g1v, g2v;
                [ForceUnroll]
                for (int ch = 0; ch < 3; ch++) {
                    float2 ab = qadam_decode_g1g2<8>(
                        p.sh_packed,
                        uint(sh_cell_at_w(sh_base, 3 * j + ch, shq.pair_pitch)),
                        quant_bounds);
                    g1v[ch] = ab.x;
                    g2v[ch] = ab.y;
                }
                float3 sh_coeff;
                [ForceUnroll]
                for (int ch = 0; ch < 3; ch++)
                    sh_coeff[ch] = qtensor_decode_v<16>(
                        p.sh_value_packed,
                        uint(sh_cell_at_w(sh_base, 3 * j + ch, shq.pair_pitch)),
                        sh_value_old_mm);
                float3 v_sh_coeff = float3(g.v_sh[j].x, g.v_sh[j].y,
                                           g.v_sh[j].z) +
                                    reg_weight * sh_coeff;
                if (kColorTrust != 0) {
                    v_sh_coeff.x /= linear_rgb_to_srgb_grad(c_dc.x);
                    v_sh_coeff.y /= linear_rgb_to_srgb_grad(c_dc.y);
                    v_sh_coeff.z /= linear_rgb_to_srgb_grad(c_dc.z);
                }
                float3 g1_upd = kFpboBeta1 * g1v +
                                (1.0f - kFpboBeta1) * v_sh_coeff;
                float3 g2_upd = kFpboBeta2 * g2v +
                                (1.0f - kFpboBeta2) * v_sh_coeff *
                                    v_sh_coeff;
                float3 denom = oq_sqrt3(g2_upd * ibc2) + float3(kFpboEps);
                float3 sh_delta = float3(-lr_sh) * (g1_upd / denom);
                if (kColorTrust != 0) {
                    sh_delta = clamp(sh_delta, -clip_dc, clip_dc);
                    sh_delta.x = isfinite(sh_delta.x) ? sh_delta.x : 0.0f;
                    sh_delta.y = isfinite(sh_delta.y) ? sh_delta.y : 0.0f;
                    sh_delta.z = isfinite(sh_delta.z) ? sh_delta.z : 0.0f;
                }
                float3 sh_updated = sh_coeff + sh_delta;
                sh_updated_vals[j] = sh_updated;
                sh_value_new_mm.x =
                    min(min(min(sh_value_new_mm.x, sh_updated.x),
                            sh_updated.y),
                        sh_updated.z);
                sh_value_new_mm.y =
                    max(max(max(sh_value_new_mm.y, sh_updated.x),
                            sh_updated.y),
                        sh_updated.z);

                [ForceUnroll]
                for (int ch = 0; ch < 3; ch++) {
                    float2 us = qadam_g1g2_to_us(g1_upd[ch], g2_upd[ch]);
                    u_sh_vals[j][ch] = us.x;
                    s_sh_vals[j][ch] = us.y;
                    mm.x = min(mm.x, us.x);
                    mm.y = max(mm.y, us.x);
                    mm.z = min(mm.z, us.y);
                    mm.w = max(mm.w, us.y);
                }
            }
            // SH-degree warmup: inactive cells zero-encode after the reduce;
            // expand this thread's bound so 0 is inside both value and
            // (u, log_s) ranges.
            if (p.num_sh_buffer > uint(num_sh)) {
                sh_value_new_mm.x = min(sh_value_new_mm.x, 0.0f);
                sh_value_new_mm.y = max(sh_value_new_mm.y, 0.0f);
                mm.x = min(mm.x, 0.0f);
                mm.y = max(mm.y, 0.0f);
                mm.z = min(mm.z, 0.0f);
            }
        }

        mm = oq_block_reduce_mm4(mm, tid);
        if (tid == 0 && blk_ok)
            p.sh_quant_bounds[block_id] = mm;

        sh_value_new_mm = oq_block_reduce_mm2(sh_value_new_mm, tid);
        if (tid == 0 && blk_ok)
            p.sh_value_bounds[block_id] = sh_value_new_mm;

        // Direct whole-word writeback. A splat's cells sit two per u32 word
        // and the words are transposed across the block, so lane stores for
        // one word index are contiguous (docs/notes/sh-quant-layout.md).
        float2 us0 = qadam_g1g2_to_us(0.0f, 0.0f);
        uint2 q0 = qadam_quantize_us<8>(us0.x, us0.y, mm);
        const uint warm_st = q0.x | (q0.y << 8);
        const uint warm_vv = qtensor_quantize_v<16>(0.0f, sh_value_new_mm);
        if (inside) {
            uint pend_s = 0, pend_v = 0, w = uint(sh_base >> 1);
            for (int j = 0; j < num_sh; ++j) {
                [ForceUnroll]
                for (int ch = 0; ch < 3; ++ch) {
                    uint2 q = qadam_quantize_us<8>(u_sh_vals[j][ch],
                                                   s_sh_vals[j][ch], mm);
                    uint st = q.x | (q.y << 8);
                    uint vv = qtensor_quantize_v<16>(sh_updated_vals[j][ch],
                                                     sh_value_new_mm);
                    if (((3 * j + ch) & 1) == 0) {
                        pend_s = st;
                        pend_v = vv;
                    } else {
                        p.sh_packed[w] = pend_s | (st << 16);
                        p.sh_value_packed[w] = pend_v | (vv << 16);
                        w += 256u;
                    }
                }
            }
            // Inactive warmup bands, and the pad cell an odd cell count
            // leaves in the last word: both take the zero encode, which the
            // 0-inclusive fresh bounds make exact.
            const uint R = 3u * p.num_sh_buffer;
            for (uint c = uint(3 * num_sh); c < R + (R & 1u); ++c) {
                if ((c & 1u) == 0u) {
                    pend_s = warm_st;
                    pend_v = warm_vv;
                } else {
                    p.sh_packed[w] = pend_s | (warm_st << 16);
                    p.sh_value_packed[w] = pend_v | (warm_vv << 16);
                    w += 256u;
                }
            }
        }
    }
}

#undef _fpbo_call

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
