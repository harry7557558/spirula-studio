#ifndef SS_PROJECTIVE_DEPTH_SLANG
#define SS_PROJECTIVE_DEPTH_SLANG
#ifndef CudaDeviceExport
#define CudaDeviceExport ForceInline
#define SS_PROJECTIVE_LOCAL_EXPORT
#endif
#include "projection_utils.slang"
#ifdef SS_PROJECTIVE_LOCAL_EXPORT
#undef CudaDeviceExport
#undef SS_PROJECTIVE_LOCAL_EXPORT
#endif

[CudaDeviceExport]
float projective_depth_information(
    float3 anchor, float3 camera, float3 mean, float3 log_scale, float4 quat
) {
    float3 view = mean-camera;
    float distance = length(view);
    if (!(distance > 0)) return 0;
    float3 ray = view/distance;
    float3 baseline = camera-anchor;
    float3 local_ray = mul(transpose(normalized_quat_to_rotmat(normalize(quat))),ray);
    float3 variance = exp(2*log_scale);
    float footprint = .5f*(variance.x*(local_ray.y*local_ray.y+local_ray.z*local_ray.z)
                         +variance.y*(local_ray.x*local_ray.x+local_ray.z*local_ray.z)
                         +variance.z*(local_ray.x*local_ray.x+local_ray.y*local_ray.y));
    float3 transverse = baseline-ray*dot(ray,baseline);
    float magnification = dot(ray,baseline)/distance;
    return dot(transverse,transverse)/max(footprint,1e-30f)+4*magnification*magnification;
}

// Euler's identity avoids cancellation in the log-range derivative.
[CudaDeviceExport]
float projective_depth_gradient(
    float3 anchor, float3 camera, float3 mean,
    float3 position_gradient, float3 scale_gradient, bool single_photometric
) {
    float gradient = single_photometric ? dot(camera-anchor,position_gradient)
        : dot(mean-anchor,position_gradient)+scale_gradient.x+scale_gradient.y+scale_gradient.z;
    return gradient;
}

[CudaDeviceExport]
void projective_depth_update(
    float3 anchor, float3 camera, float lr, float angular_lr,
    bool single_photometric, bool angular_relative, float gradient, float information,
    inout float4 state, inout float3 mean, inout float3 log_scale
) {
    float3 baseline = camera-anchor;
    if (single_photometric && dot(baseline,baseline) == 0) return;
    float3 view = mean-camera;
    float distance = length(view);
    if (!(distance > 0) || !isfinite(gradient)) return;
    if (!angular_relative) angular_lr /= distance;
    if (angular_lr <= 0) return;
    state.y = .9f*state.y+.1f*gradient;
    state.z = .999f*state.z+.001f*gradient*gradient;
    state.w += 1;
    float m = state.y/(1-pow(.9f,state.w));
    float v = state.z/(1-pow(.999f,state.w));
    float change = -lr*m/((sqrt(v)+1e-15f)*(1+information));
    float3 ray = view/distance;
    float3 derivative = (baseline-ray*dot(ray,baseline))/distance;
    float projected_step = length(derivative)*change/angular_lr;
    change /= sqrt(1+projected_step*projected_step);
    if (change == 0) return;
    // The series avoids exp(t)-1 cancellation and rounding the anchor subtraction twice.
    float fraction = abs(change) < .01f
        ? change*(1+change*(.5f+change*(1.f/6+change*(1.f/24+change/120))))
        : exp(change)-1;
    mean += fraction*(mean-anchor);
    log_scale += change;
}
#endif
