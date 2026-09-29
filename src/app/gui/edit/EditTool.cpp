// EditTool.cpp -- see EditTool.h.

#include "app/gui/edit/EditTool.h"

#include "i18n/catalog/Edit.h"
#include "i18n/catalog/EditAttributes.h"
#include "i18n/catalog/EditTransform.h"

#include "imgui.h"

#include <cmath>

namespace msg = spirula::i18n::msg::edit;
namespace xmsg = spirula::i18n::msg::xform;
namespace amsg = spirula::i18n::msg::attr;

namespace gui {

namespace {

// How far the pointer has to travel before a lasso or brush records another
// point: below this the path is a few thousand duplicates of one pixel.
constexpr float kPathStep = 3.0f;
// Clicking this near the first corner closes a polygon, the way every other
// polygon tool works -- Enter still does it for a corner too small to hit.
constexpr float kCloseRadius = 10.0f;

}  // namespace


ShapeKind shape_of(ToolId t) {
    switch (t) {
        case ToolId::Ellipse: return ShapeKind::Ellipse;
        case ToolId::Lasso:   return ShapeKind::Lasso;
        case ToolId::Polygon: return ShapeKind::Polygon;
        case ToolId::Brush:   return ShapeKind::Brush;
        default:              return ShapeKind::Box;
    }
}

const ToolRow* tool_table() {
    static const ToolRow rows[kNumTools] = {
        {ToolId::Navigate, "Q", ImGuiKey_Q},
        {ToolId::Box,      "B", ImGuiKey_B},
        {ToolId::Ellipse,  "E", ImGuiKey_E},
        {ToolId::Lasso,    "L", ImGuiKey_L},
        {ToolId::Polygon,  "P", ImGuiKey_P},
        {ToolId::Brush,    "C", ImGuiKey_C},
        {ToolId::Piece,    "F", ImGuiKey_F},
        {ToolId::Transform,  "T", ImGuiKey_T},
        {ToolId::Eyedropper, "K", ImGuiKey_K},
    };
    return rows;
}

const spirula::i18n::Msg& tool_label(ToolId t) {
    switch (t) {
        case ToolId::Box:     return msg::tool_box;
        case ToolId::Ellipse: return msg::tool_ellipse;
        case ToolId::Lasso:   return msg::tool_lasso;
        case ToolId::Polygon: return msg::tool_polygon;
        case ToolId::Brush:   return msg::tool_brush;
        case ToolId::Piece:   return msg::tool_piece;
        case ToolId::Transform:  return xmsg::tool_transform;
        case ToolId::Eyedropper: return amsg::tool_eyedropper;
        default:              return msg::tool_navigate;
    }
}

const spirula::i18n::Msg& tool_hint(ToolId t) {
    switch (t) {
        case ToolId::Box:     return msg::hint_box;
        case ToolId::Ellipse: return msg::hint_ellipse;
        case ToolId::Lasso:   return msg::hint_lasso;
        case ToolId::Polygon: return msg::hint_polygon;
        case ToolId::Brush:   return msg::hint_brush;
        case ToolId::Piece:   return msg::hint_piece;
        case ToolId::Transform:  return xmsg::hint_transform;
        case ToolId::Eyedropper: return amsg::hint_eyedropper;
        default:              return msg::hint_navigate;
    }
}

void EditTool::set_id(ToolId t) {
    if (t == _id) return;
    cancel();
    _id = t;
}

void EditTool::cancel() {
    _active = false;
    _pts.clear();
}

bool EditTool::pop_point() {
    if (!_active || _pts.size() < 2) return false;
    _pts.resize(_pts.size() - 2);
    if (_pts.empty()) _active = false;
    return true;
}

bool EditTool::update(const ViewportInput& in, ShapeStroke& out, bool& consumed) {
    consumed = false;
    if (_id == ToolId::Navigate || _id == ToolId::Transform) return false;
    _cur[0] = in.x;
    _cur[1] = in.y;

    if (_id == ToolId::Piece || _id == ToolId::Eyedropper) {
        if (in.hovered && in.clicked) {
            consumed = true;
            out.kind = ShapeKind::Box;
            out.pts = {in.x, in.y};
            return true;
        }
        consumed = in.down;
        return false;
    }

    if (_id == ToolId::Polygon) {
        // Click-to-place rather than drag, so a corner can be put down
        // precisely and the camera left alone in between.
        if (in.hovered && in.clicked) {
            if (_active && _pts.size() >= 6) {
                const float dx = in.x - _pts[0], dy = in.y - _pts[1];
                if (dx * dx + dy * dy <= kCloseRadius * kCloseRadius) {
                    consumed = true;
                    return commit_pending(out);
                }
            }
            _active = true;
            _pts.push_back(in.x);
            _pts.push_back(in.y);
            consumed = true;
            return false;
        }
        if (_active && in.right_clicked) {
            consumed = true;
            return commit_pending(out);
        }
        consumed = _active && in.down;
        return false;
    }

    if (!_active) {
        if (!in.hovered || !in.clicked) return false;
        _active = true;
        _pts.clear();
        _pts.push_back(in.x);
        _pts.push_back(in.y);
        consumed = true;
        // A press and release in one frame is an ordinary fast click; without
        // this the stroke never ends and the tool sticks.
        if (!in.released) return false;
        _pts.push_back(in.x);
        _pts.push_back(in.y);
        return commit_pending(out);
    }

    consumed = true;
    const bool path = _id == ToolId::Lasso || _id == ToolId::Brush;
    if (path) {
        const float dx = in.x - _pts[_pts.size() - 2];
        const float dy = in.y - _pts[_pts.size() - 1];
        if (dx * dx + dy * dy >= kPathStep * kPathStep) {
            _pts.push_back(in.x);
            _pts.push_back(in.y);
        }
    } else if (_pts.size() >= 4) {
        _pts[2] = in.x;
        _pts[3] = in.y;
    } else {
        _pts.push_back(in.x);
        _pts.push_back(in.y);
    }
    if (!in.released) return false;
    return commit_pending(out);
}

bool EditTool::commit_pending(ShapeStroke& out) {
    if (!_active) return false;
    _active = false;
    out.kind = shape_of(_id);
    out.brush_radius = _brush;
    out.pts.swap(_pts);
    _pts.clear();
    const size_t need = out.kind == ShapeKind::Brush ? 2 : 4;
    return out.pts.size() >= need;
}

void EditTool::draw_overlay(ImDrawList* dl, const ImVec2& o) const {
    if (!dl) return;
    const ImU32 line = IM_COL32(255, 190, 60, 230);
    const ImU32 fill = IM_COL32(255, 190, 60, 40);
    auto at = [&](size_t i) {
        return ImVec2(o.x + _pts[2 * i], o.y + _pts[2 * i + 1]);
    };
    if (_id == ToolId::Brush) {
        dl->AddCircle(ImVec2(o.x + _cur[0], o.y + _cur[1]), _brush, line, 0, 1.5f);
        if (!_active) return;
        for (size_t i = 0; i * 2 + 1 < _pts.size(); i++)
            dl->AddCircleFilled(at(i), _brush, fill);
        return;
    }
    if (!_active || _pts.size() < 2) return;
    if (_id == ToolId::Box || _id == ToolId::Ellipse) {
        if (_pts.size() < 4) return;
        const ImVec2 a = at(0), b = at(1);
        if (_id == ToolId::Box) {
            dl->AddRectFilled(a, b, fill);
            dl->AddRect(a, b, line, 0.0f, 0, 1.5f);
        } else {
            const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            const ImVec2 r(std::fabs(b.x - a.x) * 0.5f, std::fabs(b.y - a.y) * 0.5f);
            dl->AddEllipseFilled(c, r, fill);
            dl->AddEllipse(c, r, line, 0.0f, 0, 1.5f);
        }
        return;
    }
    const size_t n = _pts.size() / 2;
    for (size_t i = 0; i < n; i++) dl->PathLineTo(at(i));
    if (_id == ToolId::Polygon) dl->PathLineTo(ImVec2(o.x + _cur[0], o.y + _cur[1]));
    dl->PathStroke(line, ImDrawFlags_Closed, 1.5f);
    if (_id != ToolId::Polygon) return;
    for (size_t i = 1; i < n; i++) dl->AddCircleFilled(at(i), 3.0f, line);
    // The first corner is the one that closes the loop, so it says so: bigger
    // than the rest, and lit when the cursor is near enough to hit it.
    const float dx = _cur[0] - _pts[0], dy = _cur[1] - _pts[1];
    const bool is_near = n >= 3 && dx * dx + dy * dy <= kCloseRadius * kCloseRadius;
    dl->AddCircleFilled(at(0), is_near ? 8.0f : 5.0f,
                        is_near ? IM_COL32(120, 255, 140, 255) : line);
    if (is_near) dl->AddCircle(at(0), 12.0f, IM_COL32(120, 255, 140, 200), 0, 2.0f);
}

const spirula::i18n::Msg& EditTool::label() const { return tool_label(_id); }
const spirula::i18n::Msg& EditTool::hint() const { return tool_hint(_id); }

}  // namespace gui
