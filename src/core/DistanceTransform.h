#pragma once
// Exact Euclidean distance transform on a binary mask, and the dilate/shrink
// built on it.
//
// Header-only because its two callers link nothing in common: the dataset
// decoder (src/data/DataManager.cpp) applies mask_boundary_offset as an image
// arrives, and the masking policy (src/sam/MaskDilate.cpp) grows one
// detection before it joins the union.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace edt {

// Felzenszwalb-Huttenlocher 1D lower-envelope squared-Euclidean DT:
// d[q] = min over i of f[i] + (q-i)^2, over the i whose f[i] is finite.
// v / z are scratch of length n / n+1.
inline void dt_1d_squared(const float* f, float* d, int n,
                          int* v, double* z)
{
    const float  INF_F = std::numeric_limits<float>::infinity();
    const double INF_D = std::numeric_limits<double>::infinity();
    int first = 0;
    while (first < n && !std::isfinite(f[first])) ++first;
    if (first >= n) {
        for (int q = 0; q < n; ++q) d[q] = INF_F;
        return;
    }
    int k = 0;
    v[0] = first;
    z[0] = -INF_D;
    z[1] = +INF_D;
    for (int q = first + 1; q < n; ++q) {
        if (!std::isfinite(f[q])) continue;   // NaN too: it makes s NaN below
        double s;
        for (;;) {
            int vk = v[k];
            // Intersections in double though f is float: at 4K the operands
            // reach ~3e7 and the subtraction cancels, which is where a float
            // envelope would start picking the wrong parabola.
            double aq = (double)f[q]  + (double)q  * (double)q;
            double av = (double)f[vk] + (double)vk * (double)vk;
            s = (aq - av) / (2.0 * (double)(q - vk));
            if (k == 0 || s > z[k]) break;    // k == 0: never index v below 0
            --k;
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k+1] = +INF_D;
    }
    const int kmax = k;
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (k < kmax && z[k+1] < (double)q) ++k;
        double dq = (double)(q - v[k]);
        d[q] = (float)(dq * dq + (double)f[v[k]]);
    }
}

// One row of the 2D DT, straight off the mask bytes. Along a single row the
// Euclidean distance is just |dx| to the nearest source, so two linear sweeps
// give the same answer as the parabola envelope for half the time.
inline void dt_row_squared(const uint8_t* m, int w, uint8_t src_value, float* d)
{
    const float INF_F = std::numeric_limits<float>::infinity();
    int last = -1;
    for (int x = 0; x < w; ++x) {
        if (m[x] == src_value) last = x;
        d[x] = (last < 0) ? INF_F : (float)(x - last);
    }
    int next = -1;
    for (int x = w - 1; x >= 0; --x) {
        if (m[x] == src_value) next = x;
        const float back = (next < 0) ? INF_F : (float)(next - x);
        const float best = d[x] < back ? d[x] : back;
        d[x] = best * best;                   // INF squared stays INF
    }
}

// Columns are transformed a block at a time rather than one at a time: 16
// floats is one cache line, and the naive per-column walk strides the whole
// image twice per column, which measured 64% of the transform at 3840^2.
inline constexpr int kColBlock = 16;

// 2D squared-Euclidean DT via separable 1D passes (row, then col).
// `src_value` selects which mask value (0 or 1) acts as the source set.
// `cols` / `cols_out` are scratch of kColBlock * h.
inline void dt2d_squared(const uint8_t* mask, int h, int w, uint8_t src_value,
                         float* d2_out, float* cols, float* cols_out,
                         int* v_buf, double* z_buf)
{
    for (int y = 0; y < h; ++y)
        dt_row_squared(mask + (size_t)y * w, w, src_value,
                       d2_out + (size_t)y * w);

    for (int x0 = 0; x0 < w; x0 += kColBlock) {
        const int nc = std::min(kColBlock, w - x0);
        for (int y = 0; y < h; ++y) {
            const float* src = d2_out + (size_t)y * w + x0;
            for (int c = 0; c < nc; ++c) cols[(size_t)c * h + y] = src[c];
        }
        for (int c = 0; c < nc; ++c)
            dt_1d_squared(cols + (size_t)c * h, cols_out + (size_t)c * h,
                          h, v_buf, z_buf);
        for (int y = 0; y < h; ++y) {
            float* dst = d2_out + (size_t)y * w + x0;
            for (int c = 0; c < nc; ++c) dst[c] = cols_out[(size_t)c * h + y];
        }
    }
}

// Shrink (offset_px < 0) or dilate (offset_px > 0) a binary mask in place
// by abs(offset_px) Euclidean pixels. Output: 1 where signed-distance <=
// offset_px, signed-distance = (dist to foreground) - (dist to background).
inline void apply_mask_boundary_offset_in_place(uint8_t* mask, int h, int w,
                                                float offset_px)
{
    if (offset_px == 0.0f || h <= 0 || w <= 0 || (h == 1 && w == 1)) return;

    const int mx = std::max(h, w);
    // new[] over vector: the row pass writes every element, so vector's
    // value-initialising zero fill is 118 MB of pure memset at 3840^2.
    std::unique_ptr<float[]>  d2_fg(new float[(size_t)h * w]);
    std::unique_ptr<float[]>  d2_bg(new float[(size_t)h * w]);
    std::unique_ptr<float[]>  cols(new float[(size_t)kColBlock * h]);
    std::unique_ptr<float[]>  cols_out(new float[(size_t)kColBlock * h]);
    std::unique_ptr<int[]>    v_buf(new int[mx]);
    std::unique_ptr<double[]> z_buf(new double[(size_t)mx + 1]);

    dt2d_squared(mask, h, w, /*src_value=*/1, d2_fg.get(),
                 cols.get(), cols_out.get(), v_buf.get(), z_buf.get());
    dt2d_squared(mask, h, w, /*src_value=*/0, d2_bg.get(),
                 cols.get(), cols_out.get(), v_buf.get(), z_buf.get());

    for (size_t i = 0; i < (size_t)h * w; ++i) {
        float sd = std::sqrt(d2_fg[i]) - std::sqrt(d2_bg[i]);
        mask[i] = (sd <= offset_px) ? 1 : 0;
    }
}

}  // namespace edt
