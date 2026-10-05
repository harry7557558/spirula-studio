#pragma once

// A frame stencil's shapes as an SVG file, in normalized image coordinates
// (viewBox "0 0 1 1", y down). Painted in order over a base rect, black removes
// and white keeps, so any SVG viewer shows the mask it rasterizes to. The
// format and how to add a shape kind: docs/notes/frame-stencil.md.

#include "app/FrameMask.h"

#include <string>
#include <vector>

namespace app {

// `title` becomes <title>, the name a saved stencil is listed under. `camera`
// marks one file of a per-camera set (data-camera on the root).
std::string write_mask_svg(const std::vector<MaskShape>& shapes, const std::string& title = "",
                           const std::string& camera = "");

// Also reads hand-made SVG, one shape per subpath: a filled one with curves
// and no arc stays a Bezier, the rest are flattened. False, with `error`, on
// anything it cannot place -- a transform, say. docs/notes/frame-stencil.md.
bool read_mask_svg(const std::string& text, std::vector<MaskShape>& out, std::string& title,
                   std::string& error, std::string* camera = nullptr);

bool load_mask_svg(const std::string& path, std::vector<MaskShape>& out, std::string& title,
                   std::string& error, std::string* camera = nullptr);
bool save_mask_svg(const std::string& path, const std::vector<MaskShape>& shapes,
                   const std::string& title, std::string& error,
                   const std::string& camera = "");

// A file, as the set it belongs to: on its own, or, when it names a camera,
// with every file beside it that has its title and names a camera.
bool load_mask_svg_set(const std::string& path, MaskSet& out, std::string& title,
                       std::string& error);

// One subpath of an SVG path's `d`, curves flattened, in the path's own units.
struct SvgSubpath {
    std::vector<float> pts;   // x,y pairs
    bool closed = false;
    // The same outline as Bezier anchors (core/CubicBezier.h), quadratics
    // raised to cubics; empty unless it has a curve and no arc.
    std::vector<float> anchors;
};
bool parse_svg_path(const std::string& d, std::vector<SvgSubpath>& out, std::string& error);

}  // namespace app
