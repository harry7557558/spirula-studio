#pragma once

#include "shaders/screen_layout.h"

// Accessors for the packed per-splat screen rows. Layout and rationale live in
// shaders/screen_layout.h; the C++ side is ScreenBuffer in
// primitives/PrimitiveBase3DGS.cuh.

// ---- 2D (3DGS, mip) -------------------------------------------------------
[ForceInline]
float* scr2_row(float* s, int i) { return s + SCR2_STRIDE * i; }

[ForceInline]
float2 scr2_xy(float* r) { return float2(r[SCR2_XY], r[SCR2_XY + 1]); }
[ForceInline]
float3 scr2_conic(float* r) {
    return float3(r[SCR2_CONIC], r[SCR2_CONIC + 1], r[SCR2_CONIC + 2]);
}
[ForceInline]
float scr2_opac(float* r) { return r[SCR2_OPAC]; }
[ForceInline]
float scr2_depth(float* r) { return r[SCR2_DEPTH]; }
[ForceInline]
float3 scr2_rgb(float* r) {
    return float3(r[SCR2_RGB], r[SCR2_RGB + 1], r[SCR2_RGB + 2]);
}

[ForceInline]
void scr2_store(float* r, float2 xy, float depth, float3 conic, float opac,
                float3 rgb) {
    r[SCR2_XY] = xy.x;
    r[SCR2_XY + 1] = xy.y;
    r[SCR2_CONIC] = conic.x;
    r[SCR2_CONIC + 1] = conic.y;
    r[SCR2_CONIC + 2] = conic.z;
    r[SCR2_OPAC] = opac;
    r[SCR2_DEPTH] = depth;
    r[SCR2_RGB] = rgb.x;
    r[SCR2_RGB + 1] = rgb.y;
    r[SCR2_RGB + 2] = rgb.z;
}

// ---- 3DGUT ----------------------------------------------------------------
[ForceInline]
float* scrg_row(float* s, int i) { return s + SCRG_STRIDE * i; }

[ForceInline]
float3 scrg_scale(float* r) {
    return float3(r[SCRG_SCALE], r[SCRG_SCALE + 1], r[SCRG_SCALE + 2]);
}
[ForceInline]
float scrg_opac(float* r) { return r[SCRG_OPAC]; }
[ForceInline]
float3 scrg_rgb(float* r) {
    return float3(r[SCRG_RGB], r[SCRG_RGB + 1], r[SCRG_RGB + 2]);
}

[ForceInline]
void scrg_store(float* r, float3 scale, float opac, float3 rgb) {
    r[SCRG_SCALE] = scale.x;
    r[SCRG_SCALE + 1] = scale.y;
    r[SCRG_SCALE + 2] = scale.z;
    r[SCRG_OPAC] = opac;
    r[SCRG_RGB] = rgb.x;
    r[SCRG_RGB + 1] = rgb.y;
    r[SCRG_RGB + 2] = rgb.z;
}
