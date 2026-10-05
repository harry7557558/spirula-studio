// PathOverlay.cpp -- see PathOverlay.h.

#include "app/gui/mask/PathOverlay.h"

#include "app/gui/mask/PathTool.h"
#include "app/gui/mask/PenTool.h"
#include "core/CubicBezier.h"

#include "imgui.h"

#include <cmath>
#include <vector>

namespace gui {
namespace mask {

void draw_path_overlay(ImDrawList* dl, const ImVec2& o, const PathTool& tool) {
    if (!dl || !tool.in_progress()) return;
    const ImU32 line = IM_COL32(255, 190, 60, 230);
    const ImU32 soft = IM_COL32(255, 190, 60, 140);
    std::vector<float> anchors, committed, live;
    tool.overlay(anchors, committed, live);
    auto path = [&](const std::vector<float>& p, ImU32 col, float thick) {
        if (p.size() < 4) return;
        for (size_t i = 0; i + 1 < p.size(); i += 2)
            dl->PathLineTo(ImVec2(o.x + p[i], o.y + p[i + 1]));
        dl->PathStroke(col, 0, thick);
    };
    path(committed, line, 2.0f);
    path(live, soft, 1.5f);
    for (size_t i = 2; i + 1 < anchors.size(); i += 2)
        dl->AddCircleFilled(ImVec2(o.x + anchors[i], o.y + anchors[i + 1]), 3.0f, line);
    // The first anchor is the one that closes the loop: bigger, and lit when
    // the cursor is near enough to hit it, as EditTool's polygon does.
    const bool can_close = tool.anchor_count() >= kPathMinAnchors && tool.near_first();
    const ImVec2 first(o.x + anchors[0], o.y + anchors[1]);
    dl->AddCircleFilled(first, can_close ? 8.0f : 5.0f,
                        can_close ? IM_COL32(120, 255, 140, 255) : line);
    if (can_close) dl->AddCircle(first, 12.0f, IM_COL32(120, 255, 140, 200), 0, 2.0f);
}

void draw_bezier(ImDrawList* dl, const ImVec2& o, const float* a, size_t n, bool closed,
                 unsigned line_color, float thickness) {
    if (!dl || n == 0) return;
    std::vector<float> pts;
    for (size_t i = 0; i + (closed ? 0 : 1) < n; i++) {
        float c[8];
        bezier::segment(a, n, i, c);
        const int m = bezier::pieces(c, 0.3f);
        if (i == 0) dl->PathLineTo(ImVec2(o.x + c[0], o.y + c[1]));
        for (int k = 1; k <= m; k++) {
            float x, y;
            bezier::eval(c, (float)k / (float)m, x, y);
            dl->PathLineTo(ImVec2(o.x + x, o.y + y));
        }
    }
    dl->PathStroke(line_color, 0, thickness);
}

void draw_bezier_points(ImDrawList* dl, const ImVec2& o, const float* a, size_t n, int active,
                        bool handles) {
    if (!dl) return;
    const ImU32 ink = IM_COL32(30, 30, 30, 230);
    const ImU32 paper = IM_COL32(255, 255, 255, 240);
    const ImU32 lit = IM_COL32(70, 150, 255, 255);
    for (size_t k = 0; handles && k < n; k++) {
        const float* p = a + bezier::kAnchorFloats * k;
        const ImVec2 at(o.x + p[2], o.y + p[3]);
        for (int j : {0, 4}) {
            const ImVec2 h(o.x + p[j], o.y + p[j + 1]);
            if (std::hypot(h.x - at.x, h.y - at.y) < 1.0f) continue;
            dl->AddLine(at, h, IM_COL32(70, 150, 255, 200), 1.0f);
            dl->AddCircleFilled(h, 3.5f, (int)k == active ? lit : paper);
            dl->AddCircle(h, 3.5f, ink, 0, 1.0f);
        }
    }
    // Squares for anchors and circles for handles, the vector editors' shorthand.
    for (size_t k = 0; k < n; k++) {
        const float* p = a + bezier::kAnchorFloats * k;
        const ImVec2 lo(o.x + p[2] - 3.5f, o.y + p[3] - 3.5f), hi(lo.x + 7.0f, lo.y + 7.0f);
        dl->AddRectFilled(lo, hi, (int)k == active ? lit : paper);
        dl->AddRect(lo, hi, ink, 0.0f, 0, 1.0f);
    }
}

void draw_pen_overlay(ImDrawList* dl, const ImVec2& o, const PenTool& tool) {
    if (!dl) return;
    const ImU32 line = IM_COL32(255, 190, 60, 230);
    std::vector<float> a;
    tool.overlay(a);
    const size_t n = a.size() / bezier::kAnchorFloats;
    if (n > 0) {
        draw_bezier(dl, o, a.data(), n, /*closed=*/false, line, 2.0f);
        float c[8];
        if (tool.preview(c)) {
            const int m = bezier::pieces(c, 0.3f);
            for (int k = 0; k <= m; k++) {
                float x, y;
                bezier::eval(c, (float)k / (float)m, x, y);
                dl->PathLineTo(ImVec2(o.x + x, o.y + y));
            }
            dl->PathStroke(IM_COL32(255, 190, 60, 140), 0, 1.5f);
        }
        draw_bezier_points(dl, o, a.data(), n, tool.active_anchor(), /*handles=*/true);
    }
    // The badge beside the pointer that says what a click does, where a
    // vector editor would change the cursor itself.
    const ImVec2 at(o.x + tool.pointer_x() + 10.0f, o.y + tool.pointer_y() + 10.0f);
    const ImU32 badge = IM_COL32(255, 255, 255, 230);
    switch (tool.cue()) {
        case PenTool::Cue::Close:
            dl->AddCircle(at, 4.0f, IM_COL32(120, 255, 140, 255), 0, 2.0f);
            if (n > 0) {
                const ImVec2 first(o.x + a[2], o.y + a[3]);
                dl->AddCircle(first, 9.0f, IM_COL32(120, 255, 140, 220), 0, 2.0f);
            }
            break;
        case PenTool::Cue::Retract:
            dl->AddLine(ImVec2(at.x - 4.0f, at.y + 3.0f), ImVec2(at.x, at.y - 3.0f), badge, 1.5f);
            dl->AddLine(ImVec2(at.x, at.y - 3.0f), ImVec2(at.x + 4.0f, at.y + 3.0f), badge, 1.5f);
            break;
        case PenTool::Cue::Edit:
            dl->AddTriangleFilled(ImVec2(at.x - 3.0f, at.y - 5.0f), ImVec2(at.x - 3.0f, at.y + 5.0f),
                                  ImVec2(at.x + 5.0f, at.y + 1.0f), badge);
            break;
        case PenTool::Cue::None:
            break;
    }
}

}  // namespace mask
}  // namespace gui
