#pragma once

// CUDA side of the packed screen AABB. Layout and rounding rules:
// shaders/aabb16.h; the Slang mirror is backend/vulkan/shaders/aabb16.slang.

#include "backend/api/BackendTypes.h"
#include "shaders/aabb16.h"

#include <cstdint>


// Clamped to [0, extent] first, so the box a culled splat writes (all zeros)
// stays empty and every edge lands inside the quantized range.
__device__ __forceinline__ uint2 aabb16_encode(
    float4 aabb, uint32_t image_width, uint32_t image_height
) {
    const float sx = SS_AABB16_SCALE(image_width);
    const float sy = SS_AABB16_SCALE(image_height);
    const uint32_t x0 = (uint32_t)fminf(fmaxf(floorf(aabb.x * sx), 0.f), SS_AABB16_Q);
    const uint32_t y0 = (uint32_t)fminf(fmaxf(floorf(aabb.y * sy), 0.f), SS_AABB16_Q);
    const uint32_t x1 = (uint32_t)fminf(fmaxf(ceilf (aabb.z * sx), 0.f), SS_AABB16_Q);
    const uint32_t y1 = (uint32_t)fminf(fmaxf(ceilf (aabb.w * sy), 0.f), SS_AABB16_Q);
    return make_uint2(SS_AABB16_PACK(x0, x1), SS_AABB16_PACK(y0, y1));
}

__device__ __forceinline__ float4 aabb16_decode(
    uint2 a, uint32_t image_width, uint32_t image_height
) {
    const float ix = SS_AABB16_INV(image_width);
    const float iy = SS_AABB16_INV(image_height);
    return make_float4((float)SS_AABB16_LO(a.x) * ix,
                       (float)SS_AABB16_LO(a.y) * iy,
                       (float)SS_AABB16_HI(a.x) * ix,
                       (float)SS_AABB16_HI(a.y) * iy);
}

// The cull marker, on the packed halves: monotone encoding, so no scale.
__device__ __forceinline__ bool aabb16_is_empty(uint2 a) {
    return SS_AABB16_HI(a.x) <= SS_AABB16_LO(a.x) ||
           SS_AABB16_HI(a.y) <= SS_AABB16_LO(a.y);
}

// Ellipse center for 3DGUT, which stores no screen center of its own.
__device__ __forceinline__ float2 aabb16_center(
    uint2 a, uint32_t image_width, uint32_t image_height
) {
    return make_float2(
        0.5f * (float)(SS_AABB16_LO(a.x) + SS_AABB16_HI(a.x)) * SS_AABB16_INV(image_width),
        0.5f * (float)(SS_AABB16_LO(a.y) + SS_AABB16_HI(a.y)) * SS_AABB16_INV(image_height));
}
