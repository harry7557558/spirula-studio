#pragma once

// Fixed-point screen AABB: 16 bits per edge, packed two edges to a uint32 as
// (min | max << 16), so one box is a uint2 rather than a float4. Shared by
// core/AabbQuant.cuh and backend/vulkan/shaders/aabb16.slang; #defines only,
// so both preprocessors read the same file.
//
// An edge is a uniform partition of [0, image extent], NOT an integer pixel:
// integer pixels would round a sub-pixel splat up to a whole one, which is
// exactly the mid/low-resolution case where splats ARE pixel-sized. The step
// is extent/65535 -- 0.009 px at 608 wide, 0.09 px at 6000. Min edges round
// DOWN and max edges UP, so a splat can gain a tile but never lose one.

#define SS_AABB16_Q 65535.0f

// Encode / decode scale for one axis of an `extent`-pixel image.
#define SS_AABB16_SCALE(extent) (SS_AABB16_Q / (float)(extent))
#define SS_AABB16_INV(extent)   ((float)(extent) * (1.0f / SS_AABB16_Q))

#define SS_AABB16_PACK(lo, hi) ((lo) | ((hi) << 16))
#define SS_AABB16_LO(v)        ((v) & 0xffffu)
#define SS_AABB16_HI(v)        ((v) >> 16)
