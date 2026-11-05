// suppress CudaDeviceExport to reduce code size (CUDA emission); when an
// includer already maps the attribute (Vulkan backend), keep its mapping.
#ifndef CudaDeviceExport
#define _DENSIFY_LOCAL_CDE
#define CudaDeviceExport Differentiable
#endif
#include "projection_utils.slang"
#ifdef _DENSIFY_LOCAL_CDE
#undef CudaDeviceExport
#undef _DENSIFY_LOCAL_CDE
#endif

// Covers no pixel under any plausible scene unit (a healthy 5th percentile
// sits near -6), and mcmc_scale_reg floors here (per_splat_losses.slang), so
// one that reaches it has stopped moving -- densification recycles it.
static const float kDeadLogScale = -40.0;

[CudaDeviceExport]
bool splat_scale_is_dead(float3 log_scale) {
    return !(max(max(log_scale.x, log_scale.y), log_scale.z) > kDeadLogScale);
}

[CudaDeviceExport]
float3 densify_normal(uint4 key) {
    float4 u = float4(key >> 8) * (1.0f / 16777216.0f);
    return float3(box_muller(u.xz), box_muller(u.yw).x);
}

[CudaDeviceExport]
void mcmc_add_noise_3dgs(
    float scaler,  // unit: L^-1, same as mean_lr
    inout float3 mean, float3 log_scale, float4 quat, float logit_opac, uint4 noise_key
) {
    constexpr float min_opacity = 0.005f;
    float k = 0.5 / min_opacity;
    float x0 = 1.0 - min_opacity;
    float opac = 1.0f / (1.0f + exp(-logit_opac));
    float op_sigmoid = 1.0 / (1.0 + exp(-k * ((1.0 - opac) - x0)));

    float3x3 covar;
    quat_scale_to_covar(normalize(quat), exp(log_scale), covar);  // unit: L^2

    float3 noise = densify_normal(noise_key);
    noise = noise * scaler * op_sigmoid;  // unit: L^-1
    mean += mul(covar, noise);  // unit: L (same as mean)
}

// gate_opac caps the opacity the noise gate sees: a splat's peak alpha * T when tracked, else 1
[CudaDeviceExport]
void revised_add_noise_3dgs(
    float scaler,  // unit: dimensionless, same as mean_lr
    float radii,
    inout float3 mean, float3 log_scale, float4 quat, float logit_opac,
    float gate_opac, uint4 noise_key
) {
    if (radii <= 0.0f)
        return;

    float3 scale = exp(log_scale);  // unit: L
    float opac = 1.0f / (1.0 + exp(-logit_opac));

    float3x3 sqrt_covar;
    quat_scale_to_covar(normalize(quat), sqrt(scale), sqrt_covar);  // unit: L

    constexpr float typical_scale = 0.05;  // typical scale for Mip-NeRF 360 divided by fraction of visible Gaussians
    float op_pow = pow(1.0f - min(opac, gate_opac), 150.0f);
    float extend = sqrt(2.0f * log(max(255.0f*opac, 1.0f)));
    float noise_lr = min(typical_scale * scaler * extend * op_pow, 1.0f);  // unit: dimensionless

    float3 noise = densify_normal(noise_key);

    mean += mul(sqrt_covar, noise_lr * noise);  // unit: L
}

[CudaDeviceExport]
void long_axis_split_3dgs(
    float k,  // opacity split factor, scheduled by the caller
    float3 log_scale, float logit_opacity, float4 quat,
    out float3 new_log_scale, out float new_logit_opacity, out float3 mean_delta
) {
    // https://arxiv.org/abs/2508.12313
    float d = 0.5f * exp(max(max(log_scale.x, log_scale.y), log_scale.z));
    new_log_scale = log_scale;
    mean_delta = float3(0, 0, 0);
    constexpr float kl = log(0.5f), ks = log(0.85f);
    if (log_scale.x > log_scale.y && log_scale.x > log_scale.z)
    { new_log_scale += float3(kl, ks, ks); mean_delta.x = d; }
    else if (log_scale.y > log_scale.z)
    { new_log_scale += float3(ks, kl, ks); mean_delta.y = d; }
    else
    { new_log_scale += float3(ks, ks, kl); mean_delta.z = d; }
    mean_delta = mul(normalized_quat_to_rotmat(normalize(quat)), mean_delta);
    new_logit_opacity = log(k / (1.0 + exp(-logit_opacity) - k));
}
