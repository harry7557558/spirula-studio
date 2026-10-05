#pragma once

// Closed cubic Bezier paths, the pen tool's shape: flattening, evaluation,
// splitting and the nearest point. Host only and header-only, for the reason
// PolygonFill.h is: app/FrameMask.cpp (CLI and GUI) and the GUI's pen tool
// link nothing in common.
//
// A path is n anchors of kAnchorFloats each -- in-handle, point, out-handle as
// x,y pairs -- and segment i runs from anchor i to anchor (i + 1) % n. A corner
// has its handles on its point, which makes both segments at it straight.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace bezier {

inline constexpr size_t kAnchorFloats = 6;
// A flattened segment never has more than this many pieces.
inline constexpr int kMaxPieces = 512;

// P0, P1, P2, P3 of segment `i`, as x,y pairs.
inline void segment(const float* a, size_t n, size_t i, float c[8]) {
    const float* p = a + kAnchorFloats * i;
    const float* q = a + kAnchorFloats * ((i + 1) % n);
    c[0] = p[2]; c[1] = p[3];
    c[2] = p[4]; c[3] = p[5];
    c[4] = q[0]; c[5] = q[1];
    c[6] = q[2]; c[7] = q[3];
}

inline void eval(const float c[8], float t, float& x, float& y) {
    const float u = 1.0f - t;
    const float b0 = u * u * u, b1 = 3.0f * u * u * t, b2 = 3.0f * u * t * t, b3 = t * t * t;
    x = b0 * c[0] + b1 * c[2] + b2 * c[4] + b3 * c[6];
    y = b0 * c[1] + b1 * c[3] + b2 * c[5] + b3 * c[7];
}

// Wang's bound: this many equal steps in t keep every chord within `tol` of the curve.
inline int pieces(const float c[8], float tol) {
    const float ax = c[0] - 2.0f * c[2] + c[4], ay = c[1] - 2.0f * c[3] + c[5];
    const float bx = c[2] - 2.0f * c[4] + c[6], by = c[3] - 2.0f * c[5] + c[7];
    const float m = std::max(std::hypot(ax, ay), std::hypot(bx, by));
    if (!(m > 0.0f) || !(tol > 0.0f)) return 1;
    const float n = std::ceil(std::sqrt(0.75f * m / tol));
    return n < 1.0f ? 1 : n > (float)kMaxPieces ? kMaxPieces : (int)n;
}

// The closed outline as x,y pairs, anchor 0 first and not repeated at the
// end, with every coordinate scaled by (sx, sy) before `tol` is measured.
inline void flatten_closed(const float* a, size_t n, float sx, float sy, float tol,
                           std::vector<float>& out) {
    out.clear();
    if (n == 0) return;
    for (size_t i = 0; i < n; i++) {
        float c[8];
        segment(a, n, i, c);
        for (int k = 0; k < 8; k += 2) {
            c[k] *= sx;
            c[k + 1] *= sy;
        }
        if (i == 0) out.insert(out.end(), {c[0], c[1]});
        const int m = pieces(c, tol);
        // The last piece ends on the next anchor, which the next segment
        // starts with -- or, for the closing segment, anchor 0.
        for (int k = 1; k < m || (k == m && i + 1 < n); k++) {
            float x, y;
            eval(c, (float)k / (float)m, x, y);
            out.insert(out.end(), {x, y});
        }
    }
}

// Squared distance from (x, y) to segment `c`, and the t it is nearest at:
// a coarse scan, then a bisection-style refinement around the best sample.
inline float nearest(const float c[8], float x, float y, float& t) {
    constexpr int kScan = 48;
    auto d2 = [&](float s) {
        float px, py;
        eval(c, s, px, py);
        return (px - x) * (px - x) + (py - y) * (py - y);
    };
    t = 0.0f;
    float best = d2(0.0f);
    for (int k = 1; k <= kScan; k++) {
        const float s = (float)k / kScan, d = d2(s);
        if (d < best) { best = d; t = s; }
    }
    float step = 1.0f / kScan;
    for (int it = 0; it < 20; it++) {
        step *= 0.5f;
        const float lo = std::max(0.0f, t - step), hi = std::min(1.0f, t + step);
        const float dl = d2(lo), dh = d2(hi);
        if (dl < best) { best = dl; t = lo; }
        if (dh < best) { best = dh; t = hi; }
    }
    return best;
}

// de Casteljau at `t`: segment `i` becomes two with the same shape, joined by
// a new smooth anchor inserted after anchor i. Returns the new anchor's index.
inline size_t split(std::vector<float>& a, size_t i, float t) {
    const size_t n = a.size() / kAnchorFloats;
    float c[8];
    segment(a.data(), n, i, c);
    auto lerp = [t](float p, float q) { return p + (q - p) * t; };
    const float p01x = lerp(c[0], c[2]), p01y = lerp(c[1], c[3]);
    const float p12x = lerp(c[2], c[4]), p12y = lerp(c[3], c[5]);
    const float p23x = lerp(c[4], c[6]), p23y = lerp(c[5], c[7]);
    const float ax = lerp(p01x, p12x), ay = lerp(p01y, p12y);
    const float bx = lerp(p12x, p23x), by = lerp(p12y, p23y);
    const float mx = lerp(ax, bx), my = lerp(ay, by);
    const size_t j = (i + 1) % n;
    a[kAnchorFloats * i + 4] = p01x;
    a[kAnchorFloats * i + 5] = p01y;
    a[kAnchorFloats * j + 0] = p23x;
    a[kAnchorFloats * j + 1] = p23y;
    const float mid[kAnchorFloats] = {ax, ay, mx, my, bx, by};
    const size_t at = i + 1;
    a.insert(a.begin() + (long)(kAnchorFloats * at), mid, mid + kAnchorFloats);
    return at;
}

// Whether an anchor's two handles both sit on its point.
inline bool is_corner(const float* anchor, float eps = 1e-7f) {
    return std::fabs(anchor[0] - anchor[2]) <= eps && std::fabs(anchor[1] - anchor[3]) <= eps &&
           std::fabs(anchor[4] - anchor[2]) <= eps && std::fabs(anchor[5] - anchor[3]) <= eps;
}

inline void make_corner(float* anchor) {
    anchor[0] = anchor[4] = anchor[2];
    anchor[1] = anchor[5] = anchor[3];
}

// Both handles out and pointing opposite ways, within about a degree, once
// x and y are scaled by (sx, sy) into square pixels.
inline bool is_smooth(const float* anchor, float sx, float sy) {
    const float ix = (anchor[0] - anchor[2]) * sx, iy = (anchor[1] - anchor[3]) * sy;
    const float ox = (anchor[4] - anchor[2]) * sx, oy = (anchor[5] - anchor[3]) * sy;
    const float li = std::hypot(ix, iy), lo = std::hypot(ox, oy);
    if (!(li > 1e-6f) || !(lo > 1e-6f)) return false;
    return (ix * ox + iy * oy) / (li * lo) < -0.9998f;
}

// The anchor's point to (x, y), its handles carried along.
inline void move_anchor(float* anchor, float x, float y) {
    const float dx = x - anchor[2], dy = y - anchor[3];
    for (int j = 0; j < 6; j += 2) {
        anchor[j] += dx;
        anchor[j + 1] += dy;
    }
}

// Handle `out` (else in) to (x, y). With `keep_opposite` the other handle
// swings to point the opposite way at its own length, measured after scaling
// by (sx, sy); with no length of its own it is left alone.
inline void set_handle(float* anchor, bool out, float x, float y, bool keep_opposite,
                       float sx, float sy) {
    float* h = anchor + (out ? 4 : 0);
    float* o = anchor + (out ? 0 : 4);
    h[0] = x;
    h[1] = y;
    if (!keep_opposite) return;
    const float px = anchor[2], py = anchor[3];
    const float ol = std::hypot((o[0] - px) * sx, (o[1] - py) * sy);
    const float hx = (x - px) * sx, hy = (y - py) * sy, hl = std::hypot(hx, hy);
    if (!(ol > 1e-6f) || !(hl > 1e-6f)) return;
    o[0] = px - hx / hl * ol / sx;
    o[1] = py - hy / hl * ol / sy;
}

// Drops anchor `k`; the two segments beside it become one between its
// neighbours, which keep their handles.
inline void erase_anchor(std::vector<float>& a, size_t k) {
    if ((k + 1) * kAnchorFloats > a.size()) return;
    a.erase(a.begin() + (long)(kAnchorFloats * k), a.begin() + (long)(kAnchorFloats * (k + 1)));
}

}  // namespace bezier
