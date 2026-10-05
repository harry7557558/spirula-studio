// StencilEdit.cpp -- see StencilEdit.h.

#include "app/gui/StencilEdit.h"

#include "core/CubicBezier.h"
#include "core/PolygonFill.h"

#include <algorithm>
#include <cmath>

namespace gui {

using Kind = app::MaskShape::Kind;

namespace {

constexpr size_t kMaxUndo = 64;

// Distance from (u, v) to segment a-b, in units of the stroke's half-width.
float stroke_distance(const app::MaskShape& s, size_t i, size_t j, float u, float v) {
    const float ax = s.pts[2 * i] / s.rx, ay = s.pts[2 * i + 1] / s.ry;
    const float dx = s.pts[2 * j] / s.rx - ax, dy = s.pts[2 * j + 1] / s.ry - ay;
    const float qx = u / s.rx - ax, qy = v / s.ry - ay;
    const float len2 = dx * dx + dy * dy;
    const float t = len2 > 0.0f ? std::clamp((qx * dx + qy * dy) / len2, 0.0f, 1.0f) : 0.0f;
    return std::hypot(qx - t * dx, qy - t * dy);
}

size_t anchor_count(const app::MaskShape& s) { return s.pts.size() / bezier::kAnchorFloats; }

float* anchor_at(app::MaskShape& s, int k) {
    return &s.pts[bezier::kAnchorFloats * (size_t)k];
}

}  // namespace

bool stencil_shape_from_stroke(const ShapeStroke& s, float cw, float ch, bool remove,
                               app::MaskShape& out) {
    out = app::MaskShape{};
    out.remove = remove;
    const std::vector<float>& p = s.pts;
    if (cw <= 0.0f || ch <= 0.0f || p.size() < 2) return false;
    switch (s.kind) {
        case ShapeKind::Box:
        case ShapeKind::Ellipse: {
            if (p.size() < 4) return false;
            const float ax = std::min(p[0], p[2]) / cw, bx = std::max(p[0], p[2]) / cw;
            const float ay = std::min(p[1], p[3]) / ch, by = std::max(p[1], p[3]) / ch;
            // Two pixels either way: a click is not a shape.
            if ((bx - ax) * cw < 2.0f || (by - ay) * ch < 2.0f) return false;
            if (s.kind == ShapeKind::Box) {
                out.kind = Kind::Rect;
                out.cx = ax; out.cy = ay; out.rx = bx; out.ry = by;
            } else {
                out.kind = Kind::Ellipse;
                out.cx = 0.5f * (ax + bx);
                out.cy = 0.5f * (ay + by);
                out.rx = 0.5f * (bx - ax);
                out.ry = 0.5f * (by - ay);
            }
            return true;
        }
        case ShapeKind::Lasso:
        case ShapeKind::Polygon:
            if (p.size() < 6) return false;
            out.kind = Kind::Path;
            out.pts.resize(p.size() - p.size() % 2);
            for (size_t i = 0; i + 1 < out.pts.size(); i += 2) {
                out.pts[i] = p[i] / cw;
                out.pts[i + 1] = p[i + 1] / ch;
            }
            return true;
        case ShapeKind::Brush: {
            const float r = std::max(s.brush_radius, 0.5f);
            out.kind = Kind::Stroke;
            out.rx = r / cw;
            out.ry = r / ch;
            // A drag samples every frame; a point closer than a quarter of the
            // radius to the last kept one changes nothing the fill can see.
            float lx = p[0], ly = p[1];
            out.pts = {lx / cw, ly / ch};
            for (size_t i = 2; i + 1 < p.size(); i += 2) {
                const bool last = i + 2 >= p.size();
                if (!last && std::hypot(p[i] - lx, p[i + 1] - ly) < 0.25f * r) continue;
                lx = p[i];
                ly = p[i + 1];
                out.pts.insert(out.pts.end(), {lx / cw, ly / ch});
            }
            return true;
        }
    }
    return false;
}

bool stencil_shape_from_pen(const std::vector<float>& anchors, float cw, float ch,
                            bool remove, app::MaskShape& out) {
    out = app::MaskShape{};
    const size_t n = anchors.size() / bezier::kAnchorFloats;
    if (cw <= 0.0f || ch <= 0.0f || n < 2) return false;
    if (n == 2 && bezier::is_corner(&anchors[0], 1e-3f) &&
        bezier::is_corner(&anchors[bezier::kAnchorFloats], 1e-3f))
        return false;
    std::vector<float> outline;
    bezier::flatten_closed(anchors.data(), n, 1.0f, 1.0f, 0.25f, outline);
    float x0 = outline[0], x1 = x0, y0 = outline[1], y1 = y0;
    for (size_t i = 0; i + 1 < outline.size(); i += 2) {
        x0 = std::min(x0, outline[i]);
        x1 = std::max(x1, outline[i]);
        y0 = std::min(y0, outline[i + 1]);
        y1 = std::max(y1, outline[i + 1]);
    }
    if (x1 - x0 < 2.0f || y1 - y0 < 2.0f) return false;
    out.kind = Kind::Bezier;
    out.remove = remove;
    out.pts.resize(n * bezier::kAnchorFloats);
    for (size_t i = 0; i + 1 < out.pts.size(); i += 2) {
        out.pts[i] = anchors[i] / cw;
        out.pts[i + 1] = anchors[i + 1] / ch;
    }
    return true;
}

int stencil_handles(const app::MaskShape& s, float u[3], float v[3]) {
    switch (s.kind) {
        case Kind::Ellipse:
            u[0] = s.cx; v[0] = s.cy;
            u[1] = s.cx + s.rx; v[1] = s.cy;
            u[2] = s.cx; v[2] = s.cy + s.ry;
            return 3;
        case Kind::Rect:
            u[0] = 0.5f * (s.cx + s.rx); v[0] = 0.5f * (s.cy + s.ry);
            u[1] = s.cx; v[1] = s.cy;
            u[2] = s.rx; v[2] = s.ry;
            return 3;
        case Kind::Path:
        case Kind::Stroke:
        case Kind::Bezier:
            return 0;
    }
    return 0;
}

bool stencil_contains(const app::MaskShape& s, float u, float v) {
    switch (s.kind) {
        case Kind::Path:
            return polyfill::contains(s.pts.data(), s.pts.size() / 2, u, v);
        case Kind::Bezier: {
            std::vector<float> outline;
            bezier::flatten_closed(s.pts.data(), anchor_count(s), 1.0f, 1.0f, 1e-4f, outline);
            return polyfill::contains(outline.data(), outline.size() / 2, u, v);
        }
        case Kind::Stroke: {
            const size_t n = s.pts.size() / 2;
            if (n == 0 || s.rx <= 0.0f || s.ry <= 0.0f) return false;
            for (size_t i = 0; i < n; i++)
                if (stroke_distance(s, i, i + 1 < n ? i + 1 : i, u, v) <= 1.0f) return true;
            return false;
        }
        case Kind::Ellipse: {
            if (s.rx <= 0.0f || s.ry <= 0.0f) return false;
            const float du = (u - s.cx) / s.rx, dv = (v - s.cy) / s.ry;
            return du * du + dv * dv <= 1.0f;
        }
        case Kind::Rect:
            return u >= std::min(s.cx, s.rx) && u <= std::max(s.cx, s.rx) &&
                   v >= std::min(s.cy, s.ry) && v <= std::max(s.cy, s.ry);
    }
    return false;
}

void stencil_move(app::MaskShape& s, float du, float dv) {
    if (s.kind == Kind::Path || s.kind == Kind::Stroke || s.kind == Kind::Bezier) {
        for (size_t i = 0; i + 1 < s.pts.size(); i += 2) {
            s.pts[i] += du;
            s.pts[i + 1] += dv;
        }
        return;
    }
    s.cx += du;
    s.cy += dv;
    if (s.kind == Kind::Rect) {
        s.rx += du;
        s.ry += dv;
    }
}

void stencil_move_handle(app::MaskShape& s, int handle, float u, float v) {
    if (s.kind == Kind::Ellipse) {
        if (handle == 0) { s.cx = u; s.cy = v; }
        else if (handle == 1) s.rx = std::max(0.005f, std::fabs(u - s.cx));
        else s.ry = std::max(0.005f, std::fabs(v - s.cy));
        return;
    }
    if (s.kind != Kind::Rect) return;
    if (handle == 0) {
        const float w = s.rx - s.cx, h = s.ry - s.cy;
        s.cx = u - w * 0.5f;
        s.cy = v - h * 0.5f;
        s.rx = s.cx + w;
        s.ry = s.cy + h;
    } else if (handle == 1) { s.cx = u; s.cy = v; }
    else { s.rx = u; s.ry = v; }
}

app::MaskShape stencil_crop(const app::MaskShape& s, float u0, float v0, float u1, float v1) {
    app::MaskShape o = s;
    const float ku = 1.0f / (u1 - u0), kv = 1.0f / (v1 - v0);
    for (size_t i = 0; i + 1 < o.pts.size(); i += 2) {
        o.pts[i] = (o.pts[i] - u0) * ku;
        o.pts[i + 1] = (o.pts[i + 1] - v0) * kv;
    }
    if (s.kind == Kind::Rect) {
        o.cx = (s.cx - u0) * ku;
        o.cy = (s.cy - v0) * kv;
        o.rx = (s.rx - u0) * ku;
        o.ry = (s.ry - v0) * kv;
    } else if (s.kind == Kind::Ellipse) {
        o.cx = (s.cx - u0) * ku;
        o.cy = (s.cy - v0) * kv;
        o.rx = s.rx * ku;
        o.ry = s.ry * kv;
    } else if (s.kind == Kind::Stroke) {
        o.rx = s.rx * ku;
        o.ry = s.ry * kv;
    }
    return o;
}

PenHit pen_hit(const app::MaskShape& s, float u, float v, float cw, float ch, float radius,
               bool handles) {
    PenHit hit;
    if (s.kind != Kind::Bezier) return hit;
    const size_t n = anchor_count(s);
    const float x = u * cw, y = v * ch, r2 = radius * radius;
    auto d2 = [&](size_t k, int j) {
        const float* a = &s.pts[bezier::kAnchorFloats * k];
        const float dx = a[j] * cw - x, dy = a[j + 1] * ch - y;
        return dx * dx + dy * dy;
    };
    float best = r2;
    if (handles)
        for (size_t k = 0; k < n; k++)
            for (int j : {0, 4}) {
                // A handle folded onto its point is the anchor, not a handle.
                const float* a = &s.pts[bezier::kAnchorFloats * k];
                if (std::hypot((a[j] - a[2]) * cw, (a[j + 1] - a[3]) * ch) < 0.5f) continue;
                const float d = d2(k, j);
                if (d <= best) {
                    best = d;
                    hit = {j == 0 ? PenPart::In : PenPart::Out, (int)k, 0.0f};
                }
            }
    if (hit.part != PenPart::None) return hit;
    for (size_t k = 0; k < n; k++) {
        const float d = d2(k, 2);
        if (d <= best) {
            best = d;
            hit = {PenPart::Anchor, (int)k, 0.0f};
        }
    }
    if (hit.part != PenPart::None) return hit;
    for (size_t k = 0; k < n; k++) {
        float c[8], t = 0.0f;
        bezier::segment(s.pts.data(), n, k, c);
        for (int j = 0; j < 8; j += 2) {
            c[j] *= cw;
            c[j + 1] *= ch;
        }
        const float d = bezier::nearest(c, x, y, t);
        if (d <= best) {
            best = d;
            hit = {PenPart::Segment, (int)k, t};
        }
    }
    return hit;
}

void pen_move_point(app::MaskShape& s, int anchor, PenPart part, float u, float v, float cw,
                    float ch, bool split) {
    if (s.kind != Kind::Bezier || anchor < 0 || (size_t)anchor >= anchor_count(s)) return;
    float* a = anchor_at(s, anchor);
    if (part == PenPart::Anchor) {
        bezier::move_anchor(a, u, v);
    } else if (part == PenPart::In || part == PenPart::Out) {
        const bool smooth = !split && bezier::is_smooth(a, cw, ch);
        bezier::set_handle(a, part == PenPart::Out, u, v, smooth, cw, ch);
    }
}

bool pen_is_smooth(const app::MaskShape& s, int anchor, float cw, float ch) {
    if (s.kind != Kind::Bezier || anchor < 0 || (size_t)anchor >= anchor_count(s)) return false;
    return bezier::is_smooth(&s.pts[bezier::kAnchorFloats * (size_t)anchor], cw, ch);
}

int pen_insert(app::MaskShape& s, int segment, float t) {
    if (s.kind != Kind::Bezier || segment < 0 || (size_t)segment >= anchor_count(s)) return -1;
    return (int)bezier::split(s.pts, (size_t)segment, std::clamp(t, 0.0f, 1.0f));
}

bool pen_delete(app::MaskShape& s, int anchor) {
    if (s.kind != Kind::Bezier || anchor < 0 || (size_t)anchor >= anchor_count(s) ||
        anchor_count(s) <= 2)
        return false;
    bezier::erase_anchor(s.pts, (size_t)anchor);
    return true;
}

void pen_make_corner(app::MaskShape& s, int anchor) {
    if (s.kind != Kind::Bezier || anchor < 0 || (size_t)anchor >= anchor_count(s)) return;
    bezier::make_corner(anchor_at(s, anchor));
}

void StencilHistory::push(const app::FrameStencil& before) {
    _undo.push_back(before);
    if (_undo.size() > kMaxUndo) _undo.erase(_undo.begin());
    _redo.clear();
}

bool StencilHistory::undo(app::FrameStencil& stencil) {
    if (_undo.empty()) return false;
    _redo.push_back(std::move(stencil));
    stencil = std::move(_undo.back());
    _undo.pop_back();
    return true;
}

bool StencilHistory::redo(app::FrameStencil& stencil) {
    if (_redo.empty()) return false;
    _undo.push_back(std::move(stencil));
    stencil = std::move(_redo.back());
    _redo.pop_back();
    return true;
}

void StencilHistory::clear() {
    _undo.clear();
    _redo.clear();
}

}  // namespace gui
