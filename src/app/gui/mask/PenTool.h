#pragma once

// The pen, as vector editors have it: a click drops a corner, a drag pulls a
// smooth anchor's handles, and the first anchor, Enter or a right click
// closes the path into a cubic Bezier outline (core/CubicBezier.h). Fed one
// ViewportInput a frame; keeps its points in frame pixels through PathSpace,
// as PathTool does, so the view may zoom mid-path. No ImGui: PathOverlay.cpp
// draws it. Keys and modifiers: docs/notes/mask-editor.md.

#include "app/gui/ViewportInput.h"
#include "app/gui/mask/PathTool.h"

#include <vector>

namespace gui {
namespace mask {

inline constexpr float kPenPickRadius = 7.0f;   // fed pixels
inline constexpr float kPenDragStart = 3.0f;    // fed pixels before a press is a drag

// (x, y) turned about (ox, oy) to the nearest multiple of 45 degrees, its
// distance kept: what Shift does to a point or a handle.
void snap_45(float ox, float oy, float& x, float& y);

class PenTool {
public:
    void set_space(const PathSpace& s) { _space = s; }
    bool in_progress() const { return !_a.empty(); }
    int anchor_count() const;
    // Three anchors, or two with a curve between them.
    bool can_close() const;

    // One frame. True when the path closed: `out` is its anchors, six floats
    // each, in fed pixels. `consumed` says the left button was the tool's.
    bool update(const ViewportInput& in, std::vector<float>& out, bool& consumed);
    bool commit_pending(std::vector<float>& out);
    void cancel();
    bool pop_anchor();
    // A drag is pulling handles or moving a point; frame keys wait for it.
    bool dragging() const { return _drag.kind != DragKind::None; }

    // PathTool's rule: mirrored while idle, frozen on the first anchor.
    void note_modifiers(bool shift, bool ctrl);
    bool mode_shift() const { return _mode_shift; }
    bool mode_ctrl() const { return _mode_ctrl; }
    // Space held mid-drag moves the new anchor rather than its handles.
    void note_space(bool held) { _space_held = held; }

    // What a click would do where the pointer is, for the cursor's badge.
    enum class Cue { None, Close, Retract, Edit };
    Cue cue() const;

    // For the overlay, all in fed pixels: every anchor, and the segment the
    // next click would add, from the last anchor to the pointer.
    void overlay(std::vector<float>& anchors) const;
    bool preview(float c[8]) const;
    // The anchor being dragged, or the last one; -1 with none.
    int active_anchor() const;
    float pointer_x() const { return _cur[0]; }
    float pointer_y() const { return _cur[1]; }

private:
    enum class DragKind { None, Place, Redrag, Close, Point };
    enum class Part { In, Anchor, Out };
    struct Drag {
        DragKind kind = DragKind::None;
        int anchor = -1;
        Part part = Part::Anchor;
        float press[2] = {0.0f, 0.0f};   // fed px
        float last[2] = {0.0f, 0.0f};    // fed px, for Space
        float origin[2] = {0.0f, 0.0f};  // fed px: where a grabbed point was
        bool pulled = false;
        bool split = false;
    };

    void to_frame(float x, float y, float& fx, float& fy) const;
    void from_frame(float fx, float fy, float& x, float& y) const;
    float* anchor(int k) { return &_a[6 * (size_t)k]; }
    const float* anchor(int k) const { return &_a[6 * (size_t)k]; }
    float fed_distance(int k, int j) const;
    bool near_first() const;
    bool near_last() const;
    bool hit_point(int& k, Part& part) const;
    void drag_to(const ViewportInput& in);

    PathSpace _space;
    std::vector<float> _a;            // frame px, six per anchor
    Drag _drag;
    float _cur[2] = {0.0f, 0.0f};     // fed px
    bool _hovered = false, _ctrl = false;
    bool _space_held = false;
    bool _mode_shift = false, _mode_ctrl = false;
};

}  // namespace mask
}  // namespace gui
