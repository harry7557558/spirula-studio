#pragma once

// Slang side of the packed screen AABB. Layout and rounding rules:
// shaders/aabb16.h; the CUDA mirror is core/AabbQuant.cuh.

#include "shaders/aabb16.h"

// Clamped to [0, extent] first, so the box a culled splat writes (all zeros)
// stays empty and every edge lands inside the quantized range.
[ForceInline]
uint2 aabb16_encode(float4 aabb, uint image_width, uint image_height) {
    float sx = SS_AABB16_SCALE(image_width);
    float sy = SS_AABB16_SCALE(image_height);
    uint x0 = uint(clamp(floor(aabb.x * sx), 0.0f, SS_AABB16_Q));
    uint y0 = uint(clamp(floor(aabb.y * sy), 0.0f, SS_AABB16_Q));
    uint x1 = uint(clamp(ceil (aabb.z * sx), 0.0f, SS_AABB16_Q));
    uint y1 = uint(clamp(ceil (aabb.w * sy), 0.0f, SS_AABB16_Q));
    return uint2(SS_AABB16_PACK(x0, x1), SS_AABB16_PACK(y0, y1));
}

[ForceInline]
float4 aabb16_decode(uint2 a, uint image_width, uint image_height) {
    float ix = SS_AABB16_INV(image_width);
    float iy = SS_AABB16_INV(image_height);
    return float4(float(SS_AABB16_LO(a.x)) * ix, float(SS_AABB16_LO(a.y)) * iy,
                  float(SS_AABB16_HI(a.x)) * ix, float(SS_AABB16_HI(a.y)) * iy);
}

// The cull marker, on the packed halves: monotone encoding, so no scale.
[ForceInline]
bool aabb16_is_empty(uint2 a) {
    return SS_AABB16_HI(a.x) <= SS_AABB16_LO(a.x) ||
           SS_AABB16_HI(a.y) <= SS_AABB16_LO(a.y);
}

// Ellipse center for 3DGUT, which stores no screen center of its own.
[ForceInline]
float2 aabb16_center(uint2 a, uint image_width, uint image_height) {
    return float2(
        0.5f * float(SS_AABB16_LO(a.x) + SS_AABB16_HI(a.x)) * SS_AABB16_INV(image_width),
        0.5f * float(SS_AABB16_LO(a.y) + SS_AABB16_HI(a.y)) * SS_AABB16_INV(image_height));
}
