#pragma once

// The soft on-screen size limit, shared by the two optimizer kernels.
// Mirrored in backend/vulkan/shaders/screen_size_hinge.slang.

#include <core/Common.cuh>

// `denom` is Adam's per-axis denominator, so `penalty` reads as the share of
// one full learning-rate step spent per octave over the limit. Axis weights
// fall off from the largest, which keeps a flat splat flat as it shrinks.
__forceinline__ __device__ float3 screen_size_hinge_grad(
    float radius, float limit, float penalty, float3 log_scale, float3 denom
) {
    float3 g = {0.0f, 0.0f, 0.0f};
    if (!(penalty > 0.0f) || !(radius > limit))
        return g;
    constexpr float kInvTau = 2.0f;
    float s_max = fmaxf(fmaxf(log_scale.x, log_scale.y), log_scale.z);
    float push = penalty * log2f(radius / limit);
    g.x = push * __expf((log_scale.x - s_max) * kInvTau) * denom.x;
    g.y = push * __expf((log_scale.y - s_max) * kInvTau) * denom.y;
    g.z = push * __expf((log_scale.z - s_max) * kInvTau) * denom.z;
    return g;
}
