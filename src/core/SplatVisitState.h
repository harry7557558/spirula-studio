#pragma once

// Per-splat render bookkeeping: renders since creation or last split (low 16
// bits) | steps since last render (high 16 bits), both saturating. Rendered
// means a nonzero world opacity gradient, which excludes occluded and masked
// splats. Bit layout must match backend/vulkan/shaders/visit.slang.

#include <cstdint>

#ifdef __CUDACC__
#define SS_VISIT_FN __host__ __device__ __forceinline__
#else
#define SS_VISIT_FN inline
#endif

struct SplatVisitState {
    uint32_t* counters           = nullptr;  // [N], null = off
    bool      skip_unrendered_reg = false;   // per-splat regularizers only on rendered splats
};

SS_VISIT_FN uint32_t visit_step(uint32_t c, bool rendered) {
    uint32_t renders = c & 0xffffu, streak = c >> 16;
    if (rendered) {
        renders = renders < 0xffffu ? renders + 1u : renders;
        streak = 0u;
    } else {
        streak = streak < 0xffffu ? streak + 1u : streak;
    }
    return renders | (streak << 16);
}
SS_VISIT_FN uint32_t visit_renders(uint32_t c) { return c & 0xffffu; }
SS_VISIT_FN uint32_t visit_unrendered_steps(uint32_t c) { return c >> 16; }
