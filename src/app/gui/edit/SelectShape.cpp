// SelectShape.cpp -- see SelectShape.h.

#include "app/gui/edit/SelectShape.h"

#include "app/gui/edit/EditDoc.h"
#include "app/webviewer/RenderWorker.h"
#include "core/PolygonFill.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gui {

namespace {

// The occlusion buffer runs over every live element on the CPU, so its cost
// is the resolution: 384 is enough to tell a chair from the wall behind it.
constexpr int kOcclusionMaxDim = 512;
constexpr float kFar = std::numeric_limits<float>::max();

void put_disc(Stencil& st, float cx, float cy, float r) {
    const int x0 = std::max(0, (int)std::floor(cx - r));
    const int x1 = std::min(st.W - 1, (int)std::ceil(cx + r));
    const int y0 = std::max(0, (int)std::floor(cy - r));
    const int y1 = std::min(st.H - 1, (int)std::ceil(cy + r));
    const float r2 = r * r;
    for (int y = y0; y <= y1; y++) {
        const float dy = (float)y + 0.5f - cy;
        for (int x = x0; x <= x1; x++) {
            const float dx = (float)x + 0.5f - cx;
            if (dx * dx + dy * dy <= r2) st.in[(size_t)y * st.W + x] = 1;
        }
    }
}

// Even-odd scanline fill, which is what makes a lasso that crosses itself
// behave the way the drawn outline looks. Shared with app/FrameMask.cpp.
void fill_polygon(Stencil& st, const std::vector<float>& p) {
    polyfill::fill_even_odd(p.data(), p.size() / 2, st.W, st.H, st.in.data(), 1);
}

}  // namespace


bool ViewProjection::project(const float p[3], float& px, float& py,
                             float& depth) const {
    float c[3];
    for (int r = 0; r < 3; r++)
        c[r] = w2c[r * 4 + 0] * p[0] + w2c[r * 4 + 1] * p[1] +
               w2c[r * 4 + 2] * p[2] + w2c[r * 4 + 3];
    depth = camera_model == 4 ? c[2] : std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    float u, v;
    if (!viewer_ray_pixel(camera_model, c, u, v)) return false;
    px = u * fx + cx;
    py = v * fy + cy;
    return true;
}


bool ViewProjection::unproject(float px, float py, float origin[3],
                               float dir[3]) const {
    float cam[3];
    if (!viewer_pixel_ray(camera_model, (px - cx) / fx, (py - cy) / fy, cam))
        return false;
    // w2c's rows are the camera axes in the navigated frame, so its transpose
    // takes a camera direction back out.
    for (int r = 0; r < 3; r++)
        dir[r] = w2c[0 * 4 + r] * cam[0] + w2c[1 * 4 + r] * cam[1] +
                 w2c[2 * 4 + r] * cam[2];
    for (int r = 0; r < 3; r++)
        origin[r] = eye[r] + (camera_model == 4
            ? w2c[r] * (px - cx) / fx + w2c[4 + r] * (py - cy) / fy : 0.0f);
    return true;
}


void rasterize_shape(const ShapeStroke& s, int W, int H, Stencil& out) {
    out.W = W;
    out.H = H;
    out.in.assign((size_t)W * H, 0);
    const std::vector<float>& p = s.pts;
    switch (s.kind) {
        case ShapeKind::Box:
        case ShapeKind::Ellipse: {
            if (p.size() < 4) return;
            const float ax = std::min(p[0], p[2]), bx = std::max(p[0], p[2]);
            const float ay = std::min(p[1], p[3]), by = std::max(p[1], p[3]);
            const int x0 = std::max(0, (int)std::floor(ax));
            const int x1 = std::min(W - 1, (int)std::ceil(bx));
            const int y0 = std::max(0, (int)std::floor(ay));
            const int y1 = std::min(H - 1, (int)std::ceil(by));
            const float mx = 0.5f * (ax + bx), my = 0.5f * (ay + by);
            const float rx = std::max(0.5f * (bx - ax), 1e-3f);
            const float ry = std::max(0.5f * (by - ay), 1e-3f);
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    if (s.kind == ShapeKind::Ellipse) {
                        const float dx = ((float)x + 0.5f - mx) / rx;
                        const float dy = ((float)y + 0.5f - my) / ry;
                        if (dx * dx + dy * dy > 1.0f) continue;
                    }
                    out.in[(size_t)y * W + x] = 1;
                }
            break;
        }
        case ShapeKind::Lasso:
        case ShapeKind::Polygon:
            fill_polygon(out, p);
            break;
        case ShapeKind::Brush: {
            const float r = std::max(s.brush_radius, 1.0f);
            for (size_t i = 0; i * 2 + 1 < p.size(); i++) {
                put_disc(out, p[2 * i], p[2 * i + 1], r);
                if (i == 0) continue;
                // Stamp along the segment as well: a fast drag is a handful
                // of points a long way apart, and a dotted stroke is a bug.
                const float x0 = p[2 * i - 2], y0 = p[2 * i - 1];
                const float dx = p[2 * i] - x0, dy = p[2 * i + 1] - y0;
                const float len = std::sqrt(dx * dx + dy * dy);
                const int steps = (int)std::min(len / (0.4f * r), 256.0f);
                for (int k = 1; k < steps; k++) {
                    const float t = (float)k / (float)steps;
                    put_disc(out, x0 + t * dx, y0 + t * dy, r);
                }
            }
            break;
        }
    }
}


// ---------------------------------------------------------------------------
// Occlusion
// ---------------------------------------------------------------------------

void OcclusionBuffer::build(const EditDoc& doc, const ViewProjection& view) {
    const int big = std::max(view.W, view.H);
    _scale = big > kOcclusionMaxDim ? (float)kOcclusionMaxDim / (float)big : 1.0f;
    _W = std::max(1, (int)(view.W * _scale));
    _H = std::max(1, (int)(view.H * _scale));
    _z.assign((size_t)_W * _H, kFar);

    const float* pos = doc.positions();
    const float* rad = doc.radii();
    const uint8_t* alive = doc.alive();
    const int64_t n = doc.count();
    for (int64_t i = 0; i < n; i++) {
        if (!alive[i]) continue;
        float px, py, d;
        if (!view.project(pos + i * 3, px, py, d) || d <= 0.0f) continue;
        px *= _scale;
        py *= _scale;
        // The footprint matters: a big foreground Gaussian has to occlude the
        // pixels around its centre or a lasso still reaches through it.
        int r = 1;
        if (rad) {
            const float rp = rad[i] * view.fx * _scale / (view.camera_model == 4 ? 1.0f : std::max(d, 1e-6f));
            r = std::clamp((int)rp, 1, 6);
        }
        const int x0 = std::max(0, (int)px - r), x1 = std::min(_W - 1, (int)px + r);
        const int y0 = std::max(0, (int)py - r), y1 = std::min(_H - 1, (int)py + r);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                float& z = _z[(size_t)y * _W + x];
                if (d < z) z = d;
            }
    }
}

bool OcclusionBuffer::visible(float px, float py, float depth, float tol) const {
    if (!valid()) return true;
    const int x = (int)(px * _scale), y = (int)(py * _scale);
    if (x < 0 || y < 0 || x >= _W || y >= _H) return true;
    const float z = _z[(size_t)y * _W + x];
    return z >= kFar || depth <= z + tol * std::max(z, 1e-6f) + 1e-6f;
}


// ---------------------------------------------------------------------------
// The one loop every shape goes through
// ---------------------------------------------------------------------------

SelectResult select_by_stencil(const EditDoc& doc, const ViewProjection& view,
                               const Stencil& st, const SelectOptions& opt,
                               const OcclusionBuffer* occ,
                               std::vector<uint8_t>& out) {
    const int64_t n = doc.count();
    out.assign((size_t)n, 0);
    SelectResult res;
    if (st.W <= 0 || st.H <= 0 || n <= 0) return res;

    const float* pos = doc.positions();
    const float* rad = opt.by_extent ? doc.radii() : nullptr;
    const uint8_t* alive = doc.alive();
    std::vector<float> depth((size_t)n, 0.0f);

#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) {
        if (!alive[i]) continue;
        float px, py, d;
        if (!view.project(pos + i * 3, px, py, d) || d <= 0.0f) continue;
        bool hit = st.at((int)px, (int)py);
        if (!hit && rad) {
            const float rp = std::min(rad[i] * opt.extent_scale * view.fx /
                                          (view.camera_model == 4 ? 1.0f : std::max(d, 1e-6f)), 256.0f);
            if (rp >= 1.0f)
                hit = st.at((int)(px - rp), (int)py) ||
                      st.at((int)(px + rp), (int)py) ||
                      st.at((int)px, (int)(py - rp)) ||
                      st.at((int)px, (int)(py + rp));
        }
        if (!hit) continue;
        if (opt.front_only && occ && !occ->visible(px, py, d, opt.front_tol))
            continue;
        out[(size_t)i] = 255;
        depth[(size_t)i] = d;
    }

    float lo = kFar, hi = 0.0f;
    int64_t hits = 0;
    for (int64_t i = 0; i < n; i++) {
        if (!out[(size_t)i]) continue;
        hits++;
        lo = std::min(lo, depth[(size_t)i]);
        hi = std::max(hi, depth[(size_t)i]);
    }
    res.hits = hits;
    res.near_depth = hits ? lo : 0.0f;
    res.far_depth = hits ? hi : 0.0f;

    if (opt.depth_limit && hits > 0 && hi > lo) {
        const float a = lo + (hi - lo) * std::min(opt.near_frac, opt.far_frac);
        const float b = lo + (hi - lo) * std::max(opt.near_frac, opt.far_frac);
        int64_t kept = 0;
        for (int64_t i = 0; i < n; i++) {
            if (!out[(size_t)i]) continue;
            const float d = depth[(size_t)i];
            if (d < a || d > b) out[(size_t)i] = 0;
            else kept++;
        }
        res.hits = kept;
    }
    return res;
}


int64_t pick_element(const EditDoc& doc, const ViewProjection& view,
                     float px, float py, float radius_px) {
    if (const int64_t hit = doc.pick(view, px, py); hit >= 0) return hit;
    const int64_t n = doc.count();
    const float* pos = doc.positions();
    const uint8_t* alive = doc.alive();
    for (float r = radius_px; r <= radius_px * 8.0f + 1.0f; r *= 2.0f) {
        const float r2 = r * r;
        int64_t best = -1;
        float best_d = kFar;
        for (int64_t i = 0; i < n; i++) {
            if (!alive[i]) continue;
            float ux, uy, d;
            if (!view.project(pos + i * 3, ux, uy, d) || d <= 0.0f) continue;
            const float dx = ux - px, dy = uy - py;
            if (dx * dx + dy * dy > r2) continue;
            if (d < best_d) {
                best_d = d;
                best = i;
            }
        }
        if (best >= 0) return best;
    }
    return -1;
}

}  // namespace gui
