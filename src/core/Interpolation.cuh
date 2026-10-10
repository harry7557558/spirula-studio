#pragma once

// GT sampling uses half-pixel centers (align-corners=false) and border clamping.
// Equal-shape inputs read one tap.

#ifdef __CUDACC__

#include <cuda_runtime.h>
#include <cstdint>


namespace _bilinear_detail {

// Resolve the four bilinear taps at destination (x_dst, y_dst) sampling
// source (W_src, H_src) from destination (W_dst, H_dst). Returns the four
// clamped integer taps + their bilinear weights.
__device__ __forceinline__ void resolve_taps(
    int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src,
    int& x0, int& y0, int& x1, int& y1,
    float& w00, float& w01, float& w10, float& w11
) {
    float u = ((float)x_dst + 0.5f) * (float)W_src / (float)W_dst - 0.5f;
    float v = ((float)y_dst + 0.5f) * (float)H_src / (float)H_dst - 0.5f;

    int x0_raw = (int)floorf(u);
    int y0_raw = (int)floorf(v);
    float fx = u - (float)x0_raw;
    float fy = v - (float)y0_raw;

    x0 = max(0, min(W_src - 1, x0_raw));
    y0 = max(0, min(H_src - 1, y0_raw));
    x1 = max(0, min(W_src - 1, x0_raw + 1));
    y1 = max(0, min(H_src - 1, y0_raw + 1));

    w00 = (1.0f - fx) * (1.0f - fy);
    w10 = fx        * (1.0f - fy);
    w01 = (1.0f - fx) * fy;
    w11 = fx        * fy;
}

// A GT sentinel -- 0 in a depth map, black in a normal map -- is not a value:
// averaged with a real neighbour it reads as supervision in a one-pixel band
// around every hole. Zeros those taps; false means the pixel has no GT at all.
__device__ __forceinline__ bool revalidate_weights(
    bool v00, bool v10, bool v01, bool v11,
    float& w00, float& w10, float& w01, float& w11
) {
    if (!v00) w00 = 0.0f;
    if (!v10) w10 = 0.0f;
    if (!v01) w01 = 0.0f;
    if (!v11) w11 = 0.0f;
    float s = w00 + w10 + w01 + w11;
    if (s <= 0.0f) return false;
    float inv = 1.0f / s;
    w00 *= inv; w10 *= inv; w01 *= inv; w11 *= inv;
    return true;
}

__device__ __forceinline__ bool gt_depth_valid(float d) { return d > 0.0f; }

// A GT normal is a unit vector: black (sum <= -2.366) and mid-grey (length
// ~0) are both "no normal here". 0.25 is what `spirula geometry` writes with,
// and a blend of unit normals only falls under it past 120 degrees apart.
__device__ __forceinline__ bool gt_normal_valid(float3 n) {
    return n.x + n.y + n.z > -2.366f &&
           n.x * n.x + n.y * n.y + n.z * n.z > 0.25f;
}

} // namespace _bilinear_detail


// Bilinear sample of a [B, H, W, 3] float source (the viewer's GT thumbnails).
__device__ __forceinline__ float3 bilinear_sample_f3(
    const float3* __restrict__ src,
    int b, int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src
) {
    int x0, y0, x1, y1; float w00, w01, w10, w11;
    _bilinear_detail::resolve_taps(
        x_dst, y_dst, W_dst, H_dst, W_src, H_src,
        x0, y0, x1, y1, w00, w01, w10, w11);
    size_t plane = (size_t)b * (size_t)H_src * (size_t)W_src;
    float3 v00 = src[plane + (size_t)y0 * W_src + x0];
    float3 v10 = src[plane + (size_t)y0 * W_src + x1];
    float3 v01 = src[plane + (size_t)y1 * W_src + x0];
    float3 v11 = src[plane + (size_t)y1 * W_src + x1];
    float3 out;
    out.x = w00*v00.x + w10*v10.x + w01*v01.x + w11*v11.x;
    out.y = w00*v00.y + w10*v10.y + w01*v01.y + w11*v11.y;
    out.z = w00*v00.z + w10*v10.z + w01*v01.z + w11*v11.z;
    return out;
}

// Mask labels must survive resampling without interpolation.
__device__ __forceinline__ uint8_t nearest_sample_mask(
    const uint8_t* __restrict__ src,
    int b, int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src
) {
    float u = ((float)x_dst + 0.5f) * (float)W_src / (float)W_dst - 0.5f;
    float v = ((float)y_dst + 0.5f) * (float)H_src / (float)H_dst - 0.5f;
    int x = max(0, min(W_src - 1, (int)floorf(u + 0.5f)));
    int y = max(0, min(H_src - 1, (int)floorf(v + 0.5f)));
    size_t plane = (size_t)b * (size_t)H_src * (size_t)W_src;
    return src[plane + (size_t)y * W_src + x];
}


// ---------------------------------------------------------------------------
// GT geometry samplers: same taps as the plain ones above, but a sentinel tap
// is dropped from the blend rather than averaged in (revalidate_weights). The
// scatter halves re-derive the weights from the same buffer, so the two agree.
// ---------------------------------------------------------------------------

__device__ __forceinline__ float bilinear_sample_gt_depth(
    const float* __restrict__ src,
    int b, int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src
) {
    int x0, y0, x1, y1; float w00, w01, w10, w11;
    _bilinear_detail::resolve_taps(
        x_dst, y_dst, W_dst, H_dst, W_src, H_src,
        x0, y0, x1, y1, w00, w01, w10, w11);
    size_t plane = (size_t)b * (size_t)H_src * (size_t)W_src;
    float v00 = src[plane + (size_t)y0 * W_src + x0];
    float v10 = src[plane + (size_t)y0 * W_src + x1];
    float v01 = src[plane + (size_t)y1 * W_src + x0];
    float v11 = src[plane + (size_t)y1 * W_src + x1];
    using _bilinear_detail::gt_depth_valid;
    if (!_bilinear_detail::revalidate_weights(
            gt_depth_valid(v00), gt_depth_valid(v10),
            gt_depth_valid(v01), gt_depth_valid(v11), w00, w10, w01, w11))
        return 0.0f;
    return w00*v00 + w10*v10 + w01*v01 + w11*v11;
}

__device__ __forceinline__ float3 bilinear_sample_gt_normal(
    const float3* __restrict__ src,
    int b, int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src
) {
    int x0, y0, x1, y1; float w00, w01, w10, w11;
    _bilinear_detail::resolve_taps(
        x_dst, y_dst, W_dst, H_dst, W_src, H_src,
        x0, y0, x1, y1, w00, w01, w10, w11);
    size_t plane = (size_t)b * (size_t)H_src * (size_t)W_src;
    float3 v00 = src[plane + (size_t)y0 * W_src + x0];
    float3 v10 = src[plane + (size_t)y0 * W_src + x1];
    float3 v01 = src[plane + (size_t)y1 * W_src + x0];
    float3 v11 = src[plane + (size_t)y1 * W_src + x1];
    using _bilinear_detail::gt_normal_valid;
    if (!_bilinear_detail::revalidate_weights(
            gt_normal_valid(v00), gt_normal_valid(v10),
            gt_normal_valid(v01), gt_normal_valid(v11), w00, w10, w01, w11))
        return make_float3(-1.0f, -1.0f, -1.0f);
    float3 out;
    out.x = w00*v00.x + w10*v10.x + w01*v01.x + w11*v11.x;
    out.y = w00*v00.y + w10*v10.y + w01*v01.y + w11*v11.y;
    out.z = w00*v00.z + w10*v10.z + w01*v01.z + w11*v11.z;
    return out;
}

__device__ __forceinline__ void bilinear_scatter_add_gt_depth(
    const float* __restrict__ src, float* __restrict__ dst,
    int b, int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src,
    float dvalue
) {
    int x0, y0, x1, y1; float w00, w01, w10, w11;
    _bilinear_detail::resolve_taps(
        x_dst, y_dst, W_dst, H_dst, W_src, H_src,
        x0, y0, x1, y1, w00, w01, w10, w11);
    size_t plane = (size_t)b * (size_t)H_src * (size_t)W_src;
    size_t i00 = plane + (size_t)y0 * W_src + x0;
    size_t i10 = plane + (size_t)y0 * W_src + x1;
    size_t i01 = plane + (size_t)y1 * W_src + x0;
    size_t i11 = plane + (size_t)y1 * W_src + x1;
    using _bilinear_detail::gt_depth_valid;
    if (!_bilinear_detail::revalidate_weights(
            gt_depth_valid(src[i00]), gt_depth_valid(src[i10]),
            gt_depth_valid(src[i01]), gt_depth_valid(src[i11]),
            w00, w10, w01, w11))
        return;
    if (w00 != 0.0f) atomicAdd(dst + i00, w00 * dvalue);
    if (w10 != 0.0f) atomicAdd(dst + i10, w10 * dvalue);
    if (w01 != 0.0f) atomicAdd(dst + i01, w01 * dvalue);
    if (w11 != 0.0f) atomicAdd(dst + i11, w11 * dvalue);
}

__device__ __forceinline__ void bilinear_scatter_add_gt_normal(
    const float3* __restrict__ src, float3* __restrict__ dst,
    int b, int x_dst, int y_dst,
    int W_dst, int H_dst,
    int W_src, int H_src,
    float3 dvalue
) {
    int x0, y0, x1, y1; float w00, w01, w10, w11;
    _bilinear_detail::resolve_taps(
        x_dst, y_dst, W_dst, H_dst, W_src, H_src,
        x0, y0, x1, y1, w00, w01, w10, w11);
    size_t plane = (size_t)b * (size_t)H_src * (size_t)W_src;
    size_t i00 = plane + (size_t)y0 * W_src + x0;
    size_t i10 = plane + (size_t)y0 * W_src + x1;
    size_t i01 = plane + (size_t)y1 * W_src + x0;
    size_t i11 = plane + (size_t)y1 * W_src + x1;
    using _bilinear_detail::gt_normal_valid;
    if (!_bilinear_detail::revalidate_weights(
            gt_normal_valid(src[i00]), gt_normal_valid(src[i10]),
            gt_normal_valid(src[i01]), gt_normal_valid(src[i11]),
            w00, w10, w01, w11))
        return;
    auto scatter = [&](size_t off, float w) {
        if (w == 0.0f) return;
        float* p = (float*)(dst + off);
        atomicAdd(p + 0, w * dvalue.x);
        atomicAdd(p + 1, w * dvalue.y);
        atomicAdd(p + 2, w * dvalue.z);
    };
    scatter(i00, w00);
    scatter(i10, w10);
    scatter(i01, w01);
    scatter(i11, w11);
}

#endif // __CUDACC__
