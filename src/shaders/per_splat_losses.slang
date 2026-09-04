// All per-Gaussian losses in one kernel
//
// Every ratio of scales below is taken AFTER subtracting the largest log
// scale, and every log is floored. Raw exp(scales) flushes to zero for a
// splat under exp(-22.4) -- and these ratios' backward then divides by that
// zero, so the optimizer writes a NaN into a splat that is merely small.
// The shift cancels out of the ratios; it does not change what is computed.

// mcmc_scale_reg penalizes the LOG scale, so its gradient never vanishes: an
// unused splat drops ~lr a step forever. Floor it at densify.slang's
// kDeadLogScale, where relocation takes over. Must match that value.
static const float kMinLogScale = -40.0;
// A -inf scale would reach the shifts below as -inf minus -inf. Clamp far
// under any real splat, and under kMinLogScale, so they stay finite.
static const float kFiniteLogScale = -1e4;
// Smallest eigenvalue share fed to a log. Below this the share is zero or
// (through rounding) negative, and x*log(x) is meant to vanish there anyway.
static const float kMinEigenRatio = 1e-30;
// Largest anisotropy fed to exp, so an absurd splat saturates instead of
// returning inf (which the zero default weight turns into a NaN).
static const float kMaxLogGaussRatio = 80.0;

[ForceInline]
[Differentiable]
float erank_reg_3dgs(float3 scales, float erank_reg, float erank_reg_s3) {
    scales = exp(2.0*(scales - max(max(scales.x, scales.y), scales.z)));
    float x = scales.x, y = scales.y, z = scales.z;
    float s = x + y + z;
    float s1 = max(max(x, y), z) / s;
    float s3 = max(min(min(x, y), z) / s, kMinEigenRatio);
    float s2 = max(1.0 - s1 - s3, kMinEigenRatio);
    float r = exp(-s1*log(s1) - s2*log(s2) - s3*log(s3));
    float reg = max(-log(r-0.99999), 0.0);
    return erank_reg * reg + erank_reg_s3 * s3;
}

[ForceInline]
[Differentiable]
float erank_reg_2dgs(float2 scales, float erank_reg) {
    scales = exp(2.0*(scales - max(scales.x, scales.y)));
    float x = scales.x, y = scales.y;
    float s = x + y;
    float s1 = max(x, y) / s;
    float s2 = max(min(x, y) / s, kMinEigenRatio);
    float r = exp(-s1*log(s1) - s2*log(s2));
    float reg = max(-log(r-0.99999), 0.0);
    return erank_reg * reg;
}


[ForceInline]
[Differentiable]
float scale_reg_3dgs(float3 scales, float max_gauss_ratio, float scale_regularization_weight) {
    float hi = max(max(scales.x, scales.y), scales.z);
    float lo = min(min(scales.x, scales.y), scales.z);
    float ratio = exp(min(hi - lo, kMaxLogGaussRatio));
    float scale_reg = max(ratio, max_gauss_ratio) - max_gauss_ratio;
    return scale_regularization_weight * scale_reg;
}

[ForceInline]
[Differentiable]
float scale_reg_2dgs(float2 scales, float max_gauss_ratio, float scale_regularization_weight) {
    float ratio = exp(min(max(scales.x, scales.y) - min(scales.x, scales.y),
                         kMaxLogGaussRatio));
    float scale_reg = max(ratio, max_gauss_ratio) - max_gauss_ratio;
    return scale_regularization_weight * scale_reg;
}


[ForceInline]
[Differentiable]
float mcmc_opacity_reg(float opacity, float mcmc_opacity_reg_weight) {
    float reg = 1.0 / (1.0 + exp(-opacity));
    reg = max(reg, 0.5f + 0.25f * opacity);  // prevent vanishing gradient at high opacity
    return mcmc_opacity_reg_weight * reg;
}

[ForceInline]
[Differentiable]
float mcmc_scale_reg_3dgs(float3 scales, float mcmc_scale_reg_weight) {
    // scales = exp(scales);  // TODO: make it configurable? standard MCMC applies on post-exp but is not scale invariant
    scales = max(scales, kMinLogScale);
    mcmc_scale_reg_weight *= 1e-2;  // approximately exp(scales) for Mip-NeRF 360 outdoor scenes
    return mcmc_scale_reg_weight * (scales.x + scales.y + scales.z) / 3.0;
}

[ForceInline]
[Differentiable]
float mcmc_scale_reg_2dgs(float2 scales, float mcmc_scale_reg_weight) {
    scales = exp(scales);
    return mcmc_scale_reg_weight * (scales.x + scales.y) / 2.0;
}

[ForceInline]
[Differentiable]
float quat_norm_reg(float4 quat, float quat_norm_reg_weight) {
    float quat_norm = length(quat);
    float quat_norm_reg = quat_norm - 1.0 - log(quat_norm);
    return quat_norm_reg_weight * quat_norm_reg;
}


[ForceInline]
[Differentiable]
[CudaDeviceExport]
float[5] per_splat_losses(
    float3 scales, float opacity, float4 quat,
    no_diff float mcmc_opacity_reg_weight,
    no_diff float mcmc_scale_reg_weight,
    no_diff float max_gauss_ratio,
    no_diff float scale_regularization_weight,
    no_diff float erank_reg_weight,
    no_diff float erank_reg_weight_s3,
    no_diff float quat_norm_reg_weight
) {
    constexpr bool is_3dgs = true;
    scales = max(scales, kFiniteLogScale);
    float losses[5];
    losses[0] = mcmc_opacity_reg(opacity, mcmc_opacity_reg_weight);
    losses[4] = quat_norm_reg(quat, quat_norm_reg_weight);
    if (is_3dgs) {
        losses[1] = mcmc_scale_reg_3dgs(scales, mcmc_scale_reg_weight);
        losses[2] = scale_reg_3dgs(scales, max_gauss_ratio, scale_regularization_weight);
        losses[3] = erank_reg_3dgs(scales, erank_reg_weight, erank_reg_weight_s3);
    } else {
        losses[1] = mcmc_scale_reg_2dgs(scales.xy, mcmc_scale_reg_weight);
        losses[2] = scale_reg_2dgs(scales.xy, max_gauss_ratio, scale_regularization_weight);
        losses[3] = erank_reg_2dgs(scales.xy, erank_reg_weight);
    }
    return losses;
}


[CudaDeviceExport]
[Differentiable]
void per_splat_losses_bwd(
    float3 scales, float opacity, float4 quat,
    no_diff float[5] v_loss,
    out float3 v_scales, out float v_opacity, out float4 v_quat,
    no_diff float mcmc_opacity_reg_weight,
    no_diff float mcmc_scale_reg_weight,
    no_diff float max_gauss_ratio,
    no_diff float scale_regularization_weight,
    no_diff float erank_reg_weight,
    no_diff float erank_reg_weight_s3,
    no_diff float quat_norm_reg_weight
) {
    var p_scales = diffPair(scales);
    var p_opacity = diffPair(opacity);
    var p_quat = diffPair(quat);
    bwd_diff(per_splat_losses)(
        p_scales, p_opacity, p_quat,
        mcmc_opacity_reg_weight,
        mcmc_scale_reg_weight,
        max_gauss_ratio,
        scale_regularization_weight,
        erank_reg_weight,
        erank_reg_weight_s3,
        quat_norm_reg_weight,
        v_loss
    );
    v_scales = p_scales.d;
    v_opacity = p_opacity.d;
    v_quat = p_quat.d;
}
