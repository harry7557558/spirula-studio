#pragma once
// Component-sized binary mask margins shared by sam::Masker and the mask editor.
// Header-only and model-free, so the editor's test binary links no sam::.

#include "core/DistanceTransform.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace margin {

// Signed Euclidean pixels for a box: the odd kernel SIZE first, the radius half
// of it, so the box changes by 2r = `ratio` of its mean side. Floor of 3 on the
// size keeps a tiny detection moving; ratio 0 is exactly 0.
inline int radius_px(float x0, float y0, float x1, float y1, float ratio) {
    if (ratio == 0.0f) return 0;
    const float bw = std::max(1.0f, x1 - x0);
    const float bh = std::max(1.0f, y1 - y0);
    int k = (int)(std::fabs(ratio) * 0.5f * (bw + bh));
    if (k < 3) k = 3;
    k |= 1;
    const int r = (k - 1) / 2;
    return ratio > 0.0f ? r : -r;
}

// ORs a w x h mask (>127 = set) into `hit` (1 = covered) with its boundary moved
// `radius` px: outward stops at the frame border, inward repeats the border
// pixel, so a subject the frame cuts off keeps that edge.
inline void accumulate(const uint8_t* m, int w, int h, int radius, std::vector<uint8_t>& hit) {
    const size_t n = (size_t)w * (size_t)h;
    if (w <= 0 || h <= 0 || hit.size() != n) return;
    if (radius == 0) {
        for (size_t i = 0; i < n; ++i)
            if (m[i] > 127) hit[i] = 1;
        return;
    }
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = m + (size_t)y * w;
        int rx0 = -1, rx1 = -1;
        for (int x = 0; x < w; ++x)
            if (row[x] > 127) { if (rx0 < 0) rx0 = x; rx1 = x; }
        if (rx0 < 0) continue;
        if (rx0 < x0) x0 = rx0;
        if (rx1 > x1) x1 = rx1;
        if (y < y0) y0 = y;
        y1 = y;
    }
    if (x1 < 0) return;
    // The bounding box plus the offset holds the whole answer.
    const int pad = radius < 0 ? -radius : radius;
    const int cx0 = radius > 0 ? std::max(0, x0 - pad) : x0 - pad;
    const int cy0 = radius > 0 ? std::max(0, y0 - pad) : y0 - pad;
    const int cx1 = radius > 0 ? std::min(w - 1, x1 + pad) : x1 + pad;
    const int cy1 = radius > 0 ? std::min(h - 1, y1 + pad) : y1 + pad;
    const int cw = cx1 - cx0 + 1, ch = cy1 - cy0 + 1;
    std::vector<uint8_t> crop((size_t)cw * ch);
    for (int y = 0; y < ch; ++y) {
        const uint8_t* src = m + (size_t)std::clamp(cy0 + y, 0, h - 1) * w;
        uint8_t* dst = crop.data() + (size_t)y * cw;
        for (int x = 0; x < cw; ++x)
            dst[x] = src[std::clamp(cx0 + x, 0, w - 1)] > 127 ? 1 : 0;
    }
    edt::apply_mask_boundary_offset_in_place(crop.data(), ch, cw, (float)radius);
    for (int y = 0; y < ch; ++y) {
        const int dy = cy0 + y;
        if (dy < 0 || dy >= h) continue;
        const uint8_t* src = crop.data() + (size_t)y * cw;
        uint8_t* dst = hit.data() + (size_t)dy * w;
        for (int x = 0; x < cw; ++x) {
            const int dx = cx0 + x;
            if (dx >= 0 && dx < w && src[x]) dst[dx] = 1;
        }
    }
}

// Eight-connected pixels belong to one component, including diagonal edges.
inline void accumulate_components(const uint8_t* m, int w, int h, float ratio,
                                  std::vector<uint8_t>& hit) {
    if (w <= 0 || h <= 0 || hit.size() != (size_t)w * h) return;
    if (ratio == 0.0f) {
        accumulate(m, w, h, 0, hit);
        return;
    }
    struct Run { int x0, x1, y; };
    std::vector<uint8_t> pending(m, m + hit.size());
    std::vector<Run> runs;
    std::vector<uint8_t> component, grown;
    auto take_run = [&](int x, int y) {
        uint8_t* row = pending.data() + (size_t)y * w;
        int left = x, right = x;
        while (left > 0 && row[left - 1] > 127) --left;
        while (right + 1 < w && row[right + 1] > 127) ++right;
        std::fill(row + left, row + right + 1, 0);
        runs.push_back({left, right, y});
        return right;
    };
    for (int sy = 0; sy < h; ++sy) for (int sx = 0; sx < w; ++sx) {
        if (pending[(size_t)sy * w + sx] <= 127) continue;
        runs.clear();
        sx = take_run(sx, sy);
        int x0 = runs[0].x0, x1 = runs[0].x1, y0 = sy, y1 = sy;
        for (size_t i = 0; i < runs.size(); ++i) {
            const Run run = runs[i];
            x0 = std::min(x0, run.x0); x1 = std::max(x1, run.x1);
            y0 = std::min(y0, run.y); y1 = std::max(y1, run.y);
            for (int y : {run.y - 1, run.y + 1}) {
                if (y < 0 || y >= h) continue;
                const uint8_t* row = pending.data() + (size_t)y * w;
                const int end = std::min(w - 1, run.x1 + 1);
                for (int x = std::max(0, run.x0 - 1); x <= end; ++x)
                    if (row[x] > 127) x = take_run(x, y);
            }
        }
        const int radius = radius_px((float)x0, (float)y0, (float)x1, (float)y1, ratio);
        if (radius == 1 || radius == -1) {
            // The inclusive inward threshold keeps pixels one step from background.
            for (const Run& run : runs) {
                const int pad = radius > 0 ? 1 : 0;
                uint8_t* row = hit.data() + (size_t)run.y * w;
                std::fill(row + std::max(0, run.x0 - pad),
                          row + std::min(w, run.x1 + pad + 1), 1);
                if (radius < 0) continue;
                for (int y : {run.y - 1, run.y + 1}) {
                    if (y < 0 || y >= h) continue;
                    row = hit.data() + (size_t)y * w;
                    std::fill(row + run.x0, row + run.x1 + 1, 1);
                }
            }
            continue;
        }
        const int pad = std::abs(radius);
        const int cx0 = std::max(0, x0 - pad), cy0 = std::max(0, y0 - pad);
        const int cx1 = std::min(w - 1, x1 + pad), cy1 = std::min(h - 1, y1 + pad);
        const int cw = cx1 - cx0 + 1, ch = cy1 - cy0 + 1;
        component.assign((size_t)cw * ch, 0);
        // Other components inside this box must not inherit its radius.
        for (const Run& run : runs) {
            uint8_t* row = component.data() + (size_t)(run.y - cy0) * cw;
            std::fill(row + run.x0 - cx0, row + run.x1 - cx0 + 1, radius > 0 ? 1 : 255);
        }
        const uint8_t* result = component.data();
        if (radius > 0) {
            edt::apply_mask_boundary_offset_in_place(component.data(), ch, cw, (float)radius);
        } else {
            grown.assign(component.size(), 0);
            accumulate(component.data(), cw, ch, radius, grown);
            result = grown.data();
        }
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x)
                if (result[(size_t)y * cw + x]) hit[(size_t)(cy0 + y) * w + cx0 + x] = 1;
    }
}

}  // namespace margin
