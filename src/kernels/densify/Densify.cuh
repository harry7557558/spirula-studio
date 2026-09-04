#pragma once


#include <core/Tensor.h>

#include "core/NonShQuantState.h"


// densify_update_weight score mode. Mirrors DensifyConfig::score_mode.
enum class DensifyScoreMode : int {
    Mean   = 0,
    Max    = 1,
    Median = 2,
    Geom   = 3,
};


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void quantile_of_abs_of_finite_elements_tensor(
    DeviceVector<float> inputs,
    float q,
    bool return_reciprocal,
    DeviceVector<float> outputs
);


void normalize_clip_map_inplace_tensor(
    TorchTensorView data,  // [B, ...], rows normalized independently
    bool normalize_median,
    float clip_quantile,
    float power
);


void inplace_index_tensor(
    DeviceVector<int32_t> indices,
    DeviceVector<float> src,
    DeviceVector<float> dst
);


void inplace_scatter_add_tensor(
    DeviceVector<int32_t> indices,
    DeviceVector<float> src,
    DeviceVector<float> dst
);


void inplace_scatter_max_tensor(
    DeviceVector<int32_t> indices,
    DeviceVector<float> src,
    DeviceVector<float> dst
);


void weighted_sample_without_replacement_tensor(
    int64_t numel,
    DeviceVector<float> weights,
    bool* masks_ptr,
    uint32_t num_sample,
    uint32_t seed,
    DeviceVector<int32_t> out_idx
);


void cov_scale_init_tensor(
    DeviceVector<float3> points,  // [N, 3]
    const std::string camera_model,
    const std::string distortion,
    DeviceVector<int2> sizes,  // [C, 2], int32
    DeviceVector<float4> intrins,  // [C, 4]
    DeviceVector<float4> viewmats,  // [C, 4, 4] as 4*C float4 elements
    TorchTensorView dist_coeffs, // [C, 8]
    DeviceVector<float> log_scales  // [N, 1] output
);


void densify_clip_scale_tensor(
    int64_t num_splats,
    DeviceVector<float> radii,
    DeviceVector<float3> log_scales,
    float* logit_opacs_ptr,
    float max_scale2d,
    float clip_hardness,
    float max_scale3d
);


void densify_accum_finalize_tensor(
    int64_t num_splats,
    DeviceVector<float> accum  // [2 * num_splats], planar num then den
);


void densify_clip_score_tensor(
    int64_t num_splats,
    DeviceVector<float2> accum_buffer,  // [N, 2]; only .x is clipped
    float quantile,
    float power,
    DeviceVector<float2> score_out  // [N, 2], optional: clipped .x ^ power
);


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
);


void densify_oversize_weight_tensor(
    int64_t num_splats,
    float blend,
    DeviceVector<float> oversize_accum,
    DeviceVector<float2> score,
    DeviceVector<float2> out
);


void relocate_splats_with_long_axis_split_tensor(
    int64_t cur_num_splats,
    float min_opacity,
    float split_opacity_k,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<float2> densify_accum_buffer,
    // Sampling weights, [N, 2] with the score in lane 0; empty falls back to
    // densify_accum_buffer. Separate so a transformed score can drive the
    // draw while the kernel still propagates the raw accumulator src -> dst.
    DeviceVector<float2> sample_weights,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    // SH-quant bounds buffer + layout flag used to encode (g1=0, g2=0) into
    // the dst splats' packed bytes. Null when sh_optim_bits == 32 (no quant).
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    // SH VALUE-quant codec copy params. When sh_value_bits != 32 we also do
    // a codec-aware src->dst copy of the SH coefficient bytes (decode src,
    // encode dst against current dst bounds; clipping is acceptable -- see
    // _copy_quant_sh_value_for_splat for the rationale).
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,             // 32 / 8 / 16
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,             // buffer stride for cell indexing
    // Non-SH Adam-state quant: when enabled, each relocated dst splat gets
    // its packed bytes set to codec-encoded zero against the current bound.
    NonShQuantState non_sh,
    uint32_t seed
);


void add_splats_with_long_axis_split_tensor(
    int64_t cur_num_splats,
    int64_t num_new_splats,
    float split_opacity_k,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<float2> densify_accum_buffer,
    // Sampling weights, [N, 2] with the score in lane 0; empty falls back to
    // densify_accum_buffer. Separate so a transformed score can drive the
    // draw while the kernel still propagates the raw accumulator src -> dst.
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
);


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
);


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
);


void mcmc_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs
);


void revised_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float> radii,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs
);


void long_axis_split_tensor(
    std::string primitive,
    float split_opacity_k,
    DeviceVector<float3> log_scales,
    DeviceVector<float> logit_opacities,
    DeviceVector<float4> quats,
    DeviceVector<float3> new_log_scales,
    DeviceVector<float> new_logit_opacities,
    DeviceVector<float3> mean_deltas
);


void laplacian_edge_filter_tensor(
    DeviceTensor3D<float3> img_in,
    DeviceTensor3D<float> img_out
);


void smoothed_laplacian_edge_filter_tensor(
    DeviceTensor3D<float3> img_in,
    DeviceTensor3D<float> img_out
);


void canny_edge_filter_tensor(
    DeviceTensor3D<float3> img_in,
    bool* mask_in_ptr,
    DeviceTensor3D<float> img_out
);


void robust_canny_residual_tensor(
    DeviceTensor3D<float3> render,   // [B, H, W, 3]
    DeviceTensor3D<float3> ref,      // [B, H, W, 3]
    bool* mask_in_ptr,               // optional [B*H*W] mask; nullptr for none
    float quantile,                  // Tukey cutoff = per-image q-quantile of |r|
    DeviceTensor3D<float> img_out    // [B, H, W, 1] -- written (not added)
);
