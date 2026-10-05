// PenTool.cpp -- see PenTool.h.

#include "app/gui/mask/PenTool.h"

#include "core/CubicBezier.h"

#include <cmath>

namespace gui {
namespace mask {

void snap_45(float ox, float oy, float& x, float& y) {
    const float dx = x - ox, dy = y - oy, len = std::hypot(dx, dy);
    if (!(len > 0.0f)) return;
    constexpr float kStep = 0.78539816f;
    const float a = std::round(std::atan2(dy, dx) / kStep) * kStep;
    x = ox + len * std::cos(a);
    y = oy + len * std::sin(a);
}

void PenTool::to_frame(float x, float y, float& fx, float& fy) const {
    if (_space.to_frame) {
        _space.to_frame(x, y, fx, fy);
    } else {
        fx = x;
        fy = y;
    }
}

void PenTool::from_frame(float fx, float fy, float& x, float& y) const {
    if (_space.from_frame) {
        _space.from_frame(fx, fy, x, y);
    } else {
        x = fx;
        y = fy;
    }
}

int PenTool::anchor_count() const { return (int)(_a.size() / bezier::kAnchorFloats); }

bool PenTool::can_close() const {
    const int n = anchor_count();
    if (n >= 3) return true;
    return n == 2 && !(bezier::is_corner(anchor(0), 1e-3f) && bezier::is_corner(anchor(1), 1e-3f));
}

// From the pointer to anchor k's float pair j (0 in, 2 point, 4 out), fed px.
float PenTool::fed_distance(int k, int j) const {
    float x, y;
    from_frame(anchor(k)[j], anchor(k)[j + 1], x, y);
    return std::hypot(_cur[0] - x, _cur[1] - y);
}

bool PenTool::near_first() const {
    return in_progress() && fed_distance(0, 2) <= kPathCloseRadius;
}

bool PenTool::near_last() const {
    return in_progress() && fed_distance(anchor_count() - 1, 2) <= kPenPickRadius;
}

// Handles before points, so a handle pulled short can still be caught.
bool PenTool::hit_point(int& k, Part& part) const {
    float best = kPenPickRadius;
    k = -1;
    for (int i = 0; i < anchor_count(); i++)
        for (int j : {0, 4}) {
            // A handle folded onto its point is the anchor, not a handle.
            float hx, hy, px, py;
            from_frame(anchor(i)[j], anchor(i)[j + 1], hx, hy);
            from_frame(anchor(i)[2], anchor(i)[3], px, py);
            if (std::hypot(hx - px, hy - py) < 1.0f) continue;
            const float d = fed_distance(i, j);
            if (d <= best) {
                best = d;
                k = i;
                part = j == 0 ? Part::In : Part::Out;
            }
        }
    if (k >= 0) return true;
    best = kPenPickRadius;
    for (int i = 0; i < anchor_count(); i++) {
        const float d = fed_distance(i, 2);
        if (d <= best) {
            best = d;
            k = i;
            part = Part::Anchor;
        }
    }
    return k >= 0;
}

bool PenTool::update(const ViewportInput& in, std::vector<float>& out, bool& consumed) {
    consumed = false;
    _cur[0] = in.x;
    _cur[1] = in.y;
    _hovered = in.hovered;
    _ctrl = in.ctrl;
    if (in.hovered && in.clicked) {
        consumed = true;
        Drag d;
        d.press[0] = d.last[0] = in.x;
        d.press[1] = d.last[1] = in.y;
        int k = -1;
        Part part = Part::Anchor;
        if (in_progress() && in.ctrl) {
            // Ctrl is the vector editors' "direct selection for now".
            if (hit_point(k, part)) {
                d.kind = DragKind::Point;
                d.anchor = k;
                d.part = part;
                const int j = part == Part::In ? 0 : part == Part::Out ? 4 : 2;
                from_frame(anchor(k)[j], anchor(k)[j + 1], d.origin[0], d.origin[1]);
                _drag = d;
            }
            return false;
        }
        if (can_close() && near_first()) {
            d.kind = DragKind::Close;
            d.anchor = 0;
            _drag = d;
            return false;
        }
        if (near_last()) {
            d.kind = DragKind::Redrag;
            d.anchor = anchor_count() - 1;
            _drag = d;
            return false;
        }
        float x = in.x, y = in.y;
        if (in_progress() && in.shift) {
            float lx, ly;
            const float* l = anchor(anchor_count() - 1);
            from_frame(l[2], l[3], lx, ly);
            snap_45(lx, ly, x, y);
        }
        float fx, fy;
        to_frame(x, y, fx, fy);
        _a.insert(_a.end(), {fx, fy, fx, fy, fx, fy});
        d.kind = DragKind::Place;
        d.anchor = anchor_count() - 1;
        _drag = d;
        return false;
    }
    if (_drag.kind != DragKind::None) {
        if (in.down) {
            consumed = true;
            drag_to(in);
            return false;
        }
        const Drag d = _drag;
        _drag = Drag{};
        // A click on the anchor just placed takes its out-handle back, so the
        // next segment leaves it straight: the cusp every pen tool makes so.
        if (d.kind == DragKind::Redrag && !d.pulled) {
            float* a = anchor(d.anchor);
            a[4] = a[2];
            a[5] = a[3];
        }
        if (d.kind == DragKind::Close) return commit_pending(out);
        return false;
    }
    if (in_progress() && in.right_clicked) {
        consumed = true;
        return commit_pending(out);
    }
    return false;
}

void PenTool::drag_to(const ViewportInput& in) {
    Drag& d = _drag;
    if (d.anchor < 0 || d.anchor >= anchor_count()) return;
    float* a = anchor(d.anchor);
    float ax, ay;
    from_frame(a[2], a[3], ax, ay);
    float x = in.x, y = in.y, fx, fy;

    if (d.kind == DragKind::Point) {
        // By the pointer's travel, so the grab point does not jump onto it.
        x = d.origin[0] + in.x - d.press[0];
        y = d.origin[1] + in.y - d.press[1];
        if (d.part == Part::Anchor) {
            if (in.shift) snap_45(d.origin[0], d.origin[1], x, y);
            to_frame(x, y, fx, fy);
            bezier::move_anchor(a, fx, fy);
        } else {
            if (in.shift) snap_45(ax, ay, x, y);
            to_frame(x, y, fx, fy);
            const bool smooth = !in.alt && bezier::is_smooth(a, 1.0f, 1.0f);
            bezier::set_handle(a, d.part == Part::Out, fx, fy, smooth, 1.0f, 1.0f);
        }
        return;
    }
    if (d.kind == DragKind::Place && _space_held) {
        to_frame(ax + in.x - d.last[0], ay + in.y - d.last[1], fx, fy);
        bezier::move_anchor(a, fx, fy);
        d.last[0] = in.x;
        d.last[1] = in.y;
        // The handles follow from here, not from where the button went down.
        d.press[0] = in.x;
        d.press[1] = in.y;
        return;
    }
    d.last[0] = in.x;
    d.last[1] = in.y;
    if (!d.pulled && std::hypot(in.x - d.press[0], in.y - d.press[1]) < kPenDragStart) return;
    d.pulled = true;
    d.split = d.split || in.alt;
    if (in.shift) snap_45(ax, ay, x, y);
    to_frame(x, y, fx, fy);
    const float mx = 2.0f * a[2] - fx, my = 2.0f * a[3] - fy;
    switch (d.kind) {
        case DragKind::Place:
            // Alt freezes the in-handle where it is and steers the out alone.
            a[4] = fx;
            a[5] = fy;
            if (!d.split) {
                a[0] = mx;
                a[1] = my;
            }
            break;
        case DragKind::Redrag:
            a[4] = fx;
            a[5] = fy;
            break;
        case DragKind::Close:
            // The in-handle shapes the closing segment; Alt keeps the first
            // segment as it was drawn.
            a[0] = mx;
            a[1] = my;
            if (!d.split) {
                a[4] = fx;
                a[5] = fy;
            }
            break;
        default:
            break;
    }
}

bool PenTool::commit_pending(std::vector<float>& out) {
    out.clear();
    if (!can_close()) return false;
    out.resize(_a.size());
    for (size_t i = 0; i + 1 < _a.size(); i += 2) from_frame(_a[i], _a[i + 1], out[i], out[i + 1]);
    cancel();
    return true;
}

void PenTool::cancel() {
    _a.clear();
    _drag = Drag{};
}

bool PenTool::pop_anchor() {
    if (_a.empty()) return false;
    _a.resize(_a.size() - bezier::kAnchorFloats);
    _drag = Drag{};
    return true;
}

void PenTool::note_modifiers(bool shift, bool ctrl) {
    if (in_progress()) return;
    _mode_shift = shift;
    _mode_ctrl = ctrl;
}

PenTool::Cue PenTool::cue() const {
    if (!_hovered || !in_progress() || _drag.kind != DragKind::None) return Cue::None;
    int k;
    Part part;
    if (_ctrl) return hit_point(k, part) ? Cue::Edit : Cue::None;
    if (can_close() && near_first()) return Cue::Close;
    if (near_last()) return Cue::Retract;
    return Cue::None;
}

void PenTool::overlay(std::vector<float>& anchors) const {
    anchors.resize(_a.size());
    for (size_t i = 0; i + 1 < _a.size(); i += 2)
        from_frame(_a[i], _a[i + 1], anchors[i], anchors[i + 1]);
}

bool PenTool::preview(float c[8]) const {
    if (!in_progress() || _drag.kind != DragKind::None || !_hovered || _ctrl) return false;
    const float* l = anchor(anchor_count() - 1);
    from_frame(l[2], l[3], c[0], c[1]);
    from_frame(l[4], l[5], c[2], c[3]);
    c[4] = c[6] = _cur[0];
    c[5] = c[7] = _cur[1];
    return true;
}

int PenTool::active_anchor() const {
    if (_drag.kind != DragKind::None) return _drag.anchor;
    return anchor_count() - 1;
}

}  // namespace mask
}  // namespace gui
