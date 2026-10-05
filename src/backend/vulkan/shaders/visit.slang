#pragma once

// Per-splat render counters; bit layout must match core/SplatVisitState.h.
[ForceInline]
uint visit_step(uint c, bool rendered) {
    uint renders = c & 0xffffu, streak = c >> 16;
    if (rendered) {
        renders = renders < 0xffffu ? renders + 1u : renders;
        streak = 0u;
    } else {
        streak = streak < 0xffffu ? streak + 1u : streak;
    }
    return renders | (streak << 16);
}
[ForceInline]
uint visit_renders(uint c) { return c & 0xffffu; }
[ForceInline]
uint visit_unrendered_steps(uint c) { return c >> 16; }
