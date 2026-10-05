#pragma once

// Editing a frame stencil's shapes by hand, with no ImGui: what a finished
// tool stroke becomes, how a selected shape is hit, moved and resized, a pen
// shape's points, and undo. SegmentPanel is the UI over it. A new shape kind
// goes here and in app/FrameMaskSvg.h: docs/notes/frame-stencil.md.

#include "app/FrameMask.h"
#include "app/gui/edit/SelectShape.h"

#include <vector>

namespace gui {

// `s` was drawn on a canvas of cw x ch pixels that shows the whole frame, in
// the mask editor's shape grammar. False for a stroke too small to be a shape.
bool stencil_shape_from_stroke(const ShapeStroke& s, float cw, float ch, bool remove,
                               app::MaskShape& out);

// A pen path finished on that canvas: anchors of six floats in canvas pixels
// (core/CubicBezier.h). False for fewer than two, or one that encloses nothing.
bool stencil_shape_from_pen(const std::vector<float>& anchors, float cw, float ch,
                            bool remove, app::MaskShape& out);

// Draggable points in normalized coordinates: the first moves the shape, the
// rest resize it. Paths, strokes and pen shapes have only the first.
int stencil_handles(const app::MaskShape& s, float u[3], float v[3]);
bool stencil_contains(const app::MaskShape& s, float u, float v);
void stencil_move(app::MaskShape& s, float du, float dv);
void stencil_move_handle(app::MaskShape& s, int handle, float u, float v);
// `s` with the frame's (u0, v0)-(u1, v1) stretched over the unit square: what
// rasterize_frame_mask needs to draw that part of the frame alone.
app::MaskShape stencil_crop(const app::MaskShape& s, float u0, float v0, float u1, float v1);

// ---- a pen shape's own points; (u, v) normalized, radii in canvas pixels ----

enum class PenPart { None, In, Anchor, Out, Segment };
struct PenHit {
    PenPart part = PenPart::None;
    int index = -1;         // the anchor, or the segment that starts at it
    float t = 0.0f;         // along that segment
};
// Handles first (only with `handles`), then anchors, then segments: the
// nearest within `radius` of each, in that order.
PenHit pen_hit(const app::MaskShape& s, float u, float v, float cw, float ch, float radius,
               bool handles);
// An anchor drags its handles along. A handle of a smooth anchor swings the
// other to stay opposite, unless `split`, which leaves the anchor a cusp.
void pen_move_point(app::MaskShape& s, int anchor, PenPart part, float u, float v, float cw,
                    float ch, bool split);
bool pen_is_smooth(const app::MaskShape& s, int anchor, float cw, float ch);
// Returns the new anchor's index; the outline does not change.
int pen_insert(app::MaskShape& s, int segment, float t);
// Refused when it would leave fewer than two anchors.
bool pen_delete(app::MaskShape& s, int anchor);
void pen_make_corner(app::MaskShape& s, int anchor);

// Snapshots of the whole stencil: a handful of shapes, per camera at most.
class StencilHistory {
public:
    // Call with the stencil as it was BEFORE the change.
    void push(const app::FrameStencil& before);
    bool undo(app::FrameStencil& stencil);
    bool redo(app::FrameStencil& stencil);
    bool can_undo() const { return !_undo.empty(); }
    bool can_redo() const { return !_redo.empty(); }
    void clear();

private:
    std::vector<app::FrameStencil> _undo, _redo;
};

}  // namespace gui
