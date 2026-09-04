#pragma once

// The post-split face a GT warp samples for: its pixel's ray through the
// face's own pinhole intrinsics and frame (ImageWarp.cu, GtDepthNormalWarp.cu).
//
// Per post camera p = bid * K + ki: `axes[p]` holds unit rows (ax, ay, az) in
// the INPUT camera's frame and `post_intrins[p]` is (fx, fy, cx, cy) in face
// pixels, exactly the table the renderer draws that face with.

#include "kernels/pixelwise/PixelWiseCommon.cuh"

__forceinline__ __device__ float3 face_pixel_ray(
    const float* __restrict__ axes, const float* __restrict__ post_intrins,
    long p, int i, int j
) {
    const float4 k = ((const float4*)post_intrins)[p];
    const float u = ((float)i + 0.5f - k.z) / k.x;
    const float v = ((float)j + 0.5f - k.w) / k.y;
    const float* a = axes + 9 * p;
    return make_float3(a[6] + u * a[0] + v * a[3],
                       a[7] + u * a[1] + v * a[4],
                       a[8] + u * a[2] + v * a[5]);
}

// Panorama sample position of a ray; false past the rows the frame holds,
// with a pixel of slack at the poles, where a half-row's rounding would
// otherwise cut them off.
__forceinline__ __device__ bool equi_ray_to_uv(float3 r, int h, int w, float2* uv) {
    const float f = (float)w * (0.5f / (float)M_PI);
    uv->x = 0.5f * (float)w + f * atan2f(r.x, r.z);
    uv->y = 0.5f * (float)h + f * atan2f(r.y, hypotf(r.x, r.z));
    return uv->y >= -1.0f && uv->y <= (float)h + 1.0f;
}
