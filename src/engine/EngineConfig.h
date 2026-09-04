#pragma once

#include "primitives/Primitive.cuh"  // DensifyAccumMode

// EngineConfig -- per-step config structs accepted by the engine_*_step
// entrypoints. Every numeric field below is the already-resolved value for
// this step: scheduling lives in the caller (TrainerCore), never here.

#include "kernels/loss/PerPixelLoss.cuh"   // LossWeightIndex
#include "kernels/pixelwise/PixelWise.cuh"      // PPISPRegLossIndex

#include <array>


// Bundles the scalars engine_compute_loss_backward takes, for the call path
// that reaches it through engine_train_step.
struct LossConfig {
    std::array<float, (int)LossWeightIndex::length> weights{};
    float w_ssim          = 0.0f;
    int   num_loss_scales = 1;
    // When positive, overrides num_loss_scales per train step based on the
    // render resolution: the number of scales is chosen so that the smallest
    // image dimension is downscaled toward (but not below) this pixel count.
    // Concretely num_loss_scales = floor(log2(min(H,W) / loss_scale_min_pixels))
    // + 1 for min(H,W) >= loss_scale_min_pixels, else the single-scale default.
    // This adapts automatically to datasets with mixed image resolutions, since
    // each step's own resolution drives its scale count. Zero (default) leaves
    // num_loss_scales untouched.
    int   loss_scale_min_pixels = 0;
    bool  compute_loss_map = false;
    // DensifyLossMapMode (PerPixelLoss.cuh) as an int. Affects loss_map
    // output only; gradients, scalar loss values and the SSIM display
    // scalar are unchanged.
    int   loss_map_mode = 0;
    // Per-image quantile of the BT.601-luma residual used as the Tukey
    // cutoff in RobustEdgeAware mode. Lower values are more aggressive
    // outlier rejection (more pixels treated as distractors). Ignored
    // unless loss_map_mode == 6. Typical values: 0.8-0.95; default 0.9.
    float robust_edge_aware_quantile = 0.9f;
    // What a *_nms mode keeps per side that outranks the pixel along the ridge
    // normal: 0 is canny's hard suppression, 1 disables the pass entirely.
    float nms_falloff = 0.5f;
    // In-place conditioning of the finished loss map, in this order: divide
    // each image by the median of its nonzero finite pixels, then clip to
    // this quantile of the same population. Quantile >= 1 skips the clip.
    bool  loss_map_normalize     = false;
    float loss_map_clip_quantile = 1.0f;
    // Exponent applied to every pixel of the conditioned map, in the same
    // pass. Powers commute with both the median divide and the quantile clip
    // (both are monotone in the pixel value), so the pass order is free.
    float loss_map_power         = 1.0f;
    // DensifyAccumMode (primitives/Primitive.cuh) as an int: how the raster
    // backward reduces the map to one score per splat. Ignored when the map
    // is off.
    int   loss_map_accum_mode    = (int)DensifyAccumMode::Max;
    // Image-space overexposure regularization weight. When non-zero, a
    // dedicated kernel adds dL/dx of L = w * mean(max(-x, x-1, 0)^2) directly
    // into v_render_rgb (in the pre-bilagrid / pre-PPISP / pre-color-space
    // working space, which is what raster bwd consumes). The scalar loss is
    // never computed.
    float overexposure_reg_weight = 0.0f;
    // Color-shift regularizer (design 1, ColorShiftReg.cu). Penalizes the
    // dataset-wide mean sign(post - pre) across the COMBINED bilagrid + PPISP
    // color transform: `pre` is the input to whichever of the two runs first
    // and `post` is the final post-both image fed to the photometric loss.
    // Zero disables the kernel launch entirely. beta is the EMA decay (e.g.
    // 1 - 1/T where T = #batches per epoch); ignored when weight is 0.
    // Applied during loss backward, BEFORE either bilagrid or PPISP bwd hooks
    // consume v_render_rgb. Active when at least one of bilagrid_rgb / PPISP
    // is enabled.
    float color_shift_reg_weight = 0.0f;
    float color_shift_reg_beta   = 0.0f;
    // Supervision depth maps may store either ray depth (Euclidean distance
    // along the camera ray) or linear depth (z component). The rasterizer
    // renders ray depth, so when this is false the freshly uploaded GT depth
    // is converted from linear to ray depth in place (in set_training_data,
    // before the depth bilateral grid). True = already ray depth, no-op.
    bool  input_depth_is_ray_depth = true;
};


// Per-param learning rates + per-splat regularization weights + flags.
// LR values arrive already multiplied by train_frame_scale / alpha where
// that applies; the engine consumes them verbatim.
struct OptimConfig {
    float lr_means        = 0.0f;
    float lr_quats        = 0.0f;
    float lr_scales       = 0.0f;
    float lr_opacities    = 0.0f;
    float lr_features_dc  = 0.0f;
    float lr_features_sh  = 0.0f;

    float max_gauss_ratio              = 0.0f;
    float scale_regularization_weight  = 0.0f;
    float mcmc_opacity_reg_weight      = 0.0f;
    float mcmc_scale_reg_weight        = 0.0f;
    float erank_reg_weight             = 0.0f;
    float erank_reg_weight_s3          = 0.0f;
    float quat_norm_reg_weight         = 0.0f;
    float dc_reg_weight                = 0.0f;
    float sh_reg_weight                = 0.0f;

    // Soft on-screen size limit. The penalty enters the scale gradient
    // scaled by Adam's denominator, so it reads as the share of one full
    // learning-rate step spent per octave over the limit.
    float max_screen_size              = 0.0f;
    float max_screen_size_penalty      = 0.0f;

    bool  use_scale_agnostic_mean         = false;
    // SH-Adam optimizer-state quantization bit depth. 32 = no quantization (full
    // fp32 g1/g2). 4 or 8 = QuantizedAdamState<BITS, 256> with one float4 per
    // 256-cell block of (u, sqrt_g2) bounds. Other values are rejected.
    int   sh_optim_bits                   = 32;
    // SH PARAMETER (value) quantization bit depth. 32 = no quantization (full
    // fp32 features_sh). 8 or 16 = QuantizedTensor<BITS, 256> with one float2
    // per 256-cell block of (min, max) bounds; canonical storage is the packed
    // buffer, and every consumer kernel dequantizes on load via the codec.
    // Other values are rejected. INCOMPLETE: storage is allocated and the
    // flag exposed, but the read/write paths through Slang harmonics + FPBO
    // + densify are not plumbed, so != 32 throws at optimizer state init.
    int   sh_value_bits                   = 8;
    // Non-SH Adam-state quantization bit depth (means, quats, scales, opacities,
    // features_dc). 32 = no quantization (full fp32 g1/g2). 16 =
    // QuantizedAdamState<16, 256> -- joint (u, log_s) at 16-bit per primitive
    // (4 B / cell). FPBO-only; the non-FPBO Adam kernel throws when this is
    // non-32. quantization_level 1 sets it to 16.
    int   non_sh_optim_bits               = 32;
    // Single SH quantization level. Collapses the FPBO dispatch's
    // (sh_optim_bits, sh_value_bits) axes down to 2 instantiations:
    //   0 = off    : fp32 everywhere
    //   1 = light  : 16-bit SH value, 8x2-bit SH optim, 16x2-bit non-SH optim
    // The caller must ALSO set sh_optim_bits / sh_value_bits /
    // non_sh_optim_bits to the values this level implies: the FPBO dispatcher
    // reads only the level, but non-FPBO paths and EngineState read the
    // individual bits.
    int   quantization_level           = 0;
    bool  use_per_splat_bias_correction   = false;

    // When true, fold projection-backward and Adam-based per-splat optim into
    // a single fused kernel (FusedProjectionBwdOptim). The engine then skips
    // the standalone projection_*_backward + engine_optim_step calls. The
    // fused path does not support SH quantization: it allocates full fp32
    // g1/g2 momentum (no `sh_quant_state`).
    bool  use_fused_proj_bwd_optim        = false;

    // When true, the splat optim step writes engine().fwd.world_grad_score
    // (per-splat ||dL/dmean_world|| * max post-exp world scale) for the
    // densification score blend (DensifyConfig::score_blend_world_grad).
    // Both v_mean and the world scales already live in registers at the
    // optimizer's mean-update, so the write costs one [N] float store; when
    // false no buffer is allocated and the kernels skip the store entirely.
    bool  write_densify_world_grad_score  = false;

    // When true, split the camera batch into one-camera sub-batches inside
    // engine_train_step. Forward + bilagrid/PPISP fwd + loss + raster/proj
    // bwd run once per sub-batch and atomicAdd into the per-splat grad
    // accumulators; a single optimizer + bilagrid/PPISP optim + densify pass
    // runs at the end. Inside the splat Adam kernels, the accumulated data
    // gradient is scaled by `1/B` (B = full batch size) before adding the
    // per-splat regularization terms, so per-image grad magnitude vs reg
    // weight is batch-size invariant. Frees the per-sub-batch screen-space
    // buffers (splats_s, aabb, depths, isect/flatten ids, render_Ts, raster
    // bwd v_splats_s) between sub-batches -- peak VRAM scales by ~1/B.
    //
    // Not compatible with use_fused_proj_bwd_optim or use_color_trust_region;
    // the engine throws when either is also set. The warped training_step
    // path also throws (would need per-input-image splitting).
    bool  split_batch      = false;

    // Trust-region color-space Adam, for a linear or wide-gamut splat color
    // space: the DC and SH color updates are clipped to
    // +/-kSh0*sqrt(4*eps_tr*c/opac) per step, so the working-color-space
    // update stays inside the model's confidence radius.
    bool  use_color_trust_region          = false;
    bool  color_is_linear                 = false;   // gradient gets divided by linear->sRGB Jacobian
    float eps_tr                          = 1e-6f;
};


// MCMC / revised-relocate densification controls. max_world_size / noise_lr*
// arrive pre-scaled by alpha.
struct DensifyConfig {
    int   refine_start_iter             = 0;
    int   refine_stop_num_iter          = 0;
    // Densification runs until max(refine_stop_iter, max_steps -
    // refine_stop_num_iter). The absolute floor keeps densification active
    // in runs shorter than refine_stop_num_iter, where the relative rule
    // alone would disable it entirely.
    int   refine_stop_iter              = 0;
    int   refine_every                  = 0;
    float growth_factor                 = 1.0f;
    float min_opacity                   = 0.0f;
    float max_screen_size               = 0.0f;
    float max_screen_size_clip_hardness = 0.0f;
    // With the soft penalty on, the hard clip drops to a once-per-refine
    // backstop: the penalty needs room to find a balance above the limit,
    // and a same-step split is what pays for the shrink.
    bool  clip_screen_size_at_refine    = false;
    float max_world_size                = 0.0f;
    float noise_lr                      = 0.0f;
    float noise_lr_final                = 0.0f;
    bool use_revised_densification      = true;
    int  score_mode                     = 0;    // 0=mean, 1=max, 2=median, 3=geom
    // Blend weight `w` between the image-space accum_weight score and the
    // world-space gradient score: per-step weight =
    //   accum_weight^(1-w) * (||dL/dmean_world|| * max post-exp scale)^w.
    // 0 (default) = accum_weight only (identical to before, no extra buffer);
    // 1 = world-grad score only (accum_weight production can be skipped);
    // in between = geometric blend, ranking-invariant to each score's global
    // scale. Requires OptimConfig::write_densify_world_grad_score when > 0.
    float score_blend_world_grad        = 0.0f;
    // Exponent applied to a step's per-splat score after the image-to-splat
    // reduction (Avg's divide included) and before the across-step one.
    float score_power                   = 1.0f;
    // Clips the per-splat score that densification actually samples -- after
    // both the image-to-splat and the across-step reduction -- at this
    // quantile of the positive scores. Outside (0, 1) nothing is clipped.
    float score_clip_quantile           = 1.0f;
    // Exponent applied after the across-step reduction, on the way to the
    // sampling draw. Fused into the clip pass but written to a side buffer:
    // powering the running accumulator in place would compound every step.
    float final_score_power             = 1.0f;
    // Share of each refine step's new splats drawn from the oversize channel
    // (weight = accumulated log2 oversize * score^oversize_score_blend)
    // instead of the plain error score. 0 keeps the single draw.
    float oversize_split_fraction       = 0.0f;
    float oversize_score_blend          = 0.5f;
    // Long-axis-split opacity split factor `k`, linearly scheduled from
    // `las_split_opacity_k_init` to `..._final` over `..._warmup` steps.
    float las_split_opacity_k_init      = 0.5f;
    float las_split_opacity_k_final     = 0.6f;
    int   las_split_opacity_k_warmup    = 4500;
};


// Per-type Adam LR + TV regularization weight. lr <= 0 disables the channel
// for the current step (so a single config covers "enabled but skipped" too).
//
// When the engine was init'd with use_adagrad=true for a given type, those
// (lr_*) values are the AdaGrad LRs (typically larger than Adam's, e.g. 1e-1
// for RGB, since AdaGrad's effective per-parameter LR shrinks as the
// accumulator grows).
struct BilagridStepConfig {
    float lr_rgb         = 0.0f;
    float lr_depth       = 0.0f;
    float lr_normal      = 0.0f;
    float tv_weight_rgb  = 0.0f;
    float tv_weight_depth = 0.0f;
    float tv_weight_normal = 0.0f;
};


// Background blending step config. When mode=Noise, only `randomize_weight`
// Adam over the per-band SH coefficients (index 0 = DC color, 1+ = bands).
struct BackgroundStepConfig {
    float    lr_dc           = 0.0f;
    float    lr_sh           = 0.0f;
    float    randomize_weight = 0.0f;
    uint32_t seed             = 0;
};


// PPISP Adam LR, the 6 regularization weights (PPISPRegLossIndex order), and
// where PPISP sits in the chain render -> bg -> [PPISP] -> display encode ->
// bilagrid -> [PPISP] -> loss. The backward hooks invert whichever it picks.
struct PpispStepConfig {
    float lr = 0.0f;
    std::array<float, (int)PPISPRegLossIndex::length> reg_weights{};
    bool  run_before_bilagrid = false;
    bool  run_before_color_space = false;
};


// Bundle passed to engine_train_step covering all per-step config groups.
struct EngineStepConfig {
    LossConfig           loss;
    OptimConfig          optim;
    DensifyConfig        densify;
    BilagridStepConfig   bilagrid;
    PpispStepConfig      ppisp;
    BackgroundStepConfig background;
};
