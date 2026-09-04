// The soft on-screen size limit, shared by optim_geometry.slang and
// fpbo.slang. Mirrors kernels/optim/ScreenSizeHinge.cuh.
#pragma once

// `denom` is Adam's per-axis denominator, so `penalty` reads as the share of
// one full learning-rate step spent per octave over the limit. Axis weights
// fall off from the largest, which keeps a flat splat flat as it shrinks.
[ForceInline]
float3 screen_size_hinge_grad(float radius, float limit, float penalty,
                              float3 log_scale, float3 denom) {
    if (!(penalty > 0.0f) || !(radius > limit))
        return float3(0.0f);
    const float kInvTau = 2.0f;
    float s_max = max(max(log_scale.x, log_scale.y), log_scale.z);
    float push = penalty * log2(radius / limit);
    return push * exp((log_scale - s_max) * kInvTau) * denom;
}
