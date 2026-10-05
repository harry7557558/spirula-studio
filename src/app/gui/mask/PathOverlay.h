#pragma once

// Draws the two pen tools over a canvas: PathTool's committed polyline and
// live segment, PenTool's curves, anchors and handles, and the first anchor
// lit when a click would close on it. The ImGui half of both; PathTool.cpp
// and PenTool.cpp have none.

#include <cstddef>

struct ImDrawList;
struct ImVec2;

namespace gui {
namespace mask {

class PathTool;
class PenTool;

void draw_path_overlay(ImDrawList* dl, const ImVec2& origin, const PathTool& tool);
void draw_pen_overlay(ImDrawList* dl, const ImVec2& origin, const PenTool& tool);

// Anchors (six floats each, canvas pixels from `origin`) as squares, the
// `active` one filled, with their handles when `handles`. `closed` draws the
// segment back to the first anchor too.
void draw_bezier(ImDrawList* dl, const ImVec2& origin, const float* anchors, size_t n,
                 bool closed, unsigned line_color, float thickness);
void draw_bezier_points(ImDrawList* dl, const ImVec2& origin, const float* anchors, size_t n,
                        int active, bool handles);

}  // namespace mask
}  // namespace gui
