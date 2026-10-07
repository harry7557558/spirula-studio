#pragma once

// Turning what was drawn on the viewport into a set.
//
// Box, ellipse, lasso, polygon and brush are one thing: a screen-space
// stencil. The shape rasterizes into a bitmask -- cheap, and different per
// shape -- and one loop does the rest, projecting every element with the
// current view and testing the mask. Two modifiers make it usable rather than
// a demo: front-most only, and a depth range; without them a lasso around a
// chair also takes the wall behind it.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gui {

class EditDoc;

// The viewport's camera as a selection needs it: the document's own frame to
// pixels, at the resolution the shape was drawn at.
struct ViewProjection {
    // Model frame -> camera, row-major 3x4, CV convention (+x right, +y down,
    // +z forward) -- the frame viewer_pixel_ray works in.
    float w2c[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float fx = 1, fy = 1, cx = 0, cy = 0;
    int camera_model = 0;        // ViewportPanel's kViewerCameraModels index
    int W = 1, H = 1;
    // Camera position in the model frame, for the radius-to-pixels estimate.
    float eye[3] = {0, 0, 0};
    // An orthographic view is a pinhole pulled this far back (ViewportPanel::
    // ortho_pullback). Depths include it; a RELATIVE depth test must not.
    float ortho_back = 0.0f;

    // Pixel and the distance in front of the camera. False where the model
    // has no image for that direction.
    bool project(const float p[3], float& px, float& py, float& depth) const;
    // And back: the ray through a pixel, in the same frame.
    bool unproject(float px, float py, float origin[3], float dir[3]) const;
};

enum class ShapeKind { Box = 0, Ellipse, Lasso, Polygon, Brush };
inline constexpr int kNumShapeKinds = 5;

// The drawn shape, in the same pixels as the ViewProjection.
struct ShapeStroke {
    ShapeKind kind = ShapeKind::Box;
    std::vector<float> pts;      // x,y pairs: two corners, or a path
    float brush_radius = 24.0f;
};

// One bit per pixel of the view, at `W` x `H`.
struct Stencil {
    int W = 0, H = 0;
    std::vector<uint8_t> in;
    bool at(int x, int y) const {
        return x >= 0 && y >= 0 && x < W && y < H && in[(size_t)y * W + x];
    }
};

void rasterize_shape(const ShapeStroke& s, int W, int H, Stencil& out);

struct SelectOptions {
    // Only what is not behind something else, against a depth buffer built
    // from the live elements themselves -- the first thing anyone tries.
    bool front_only = true;
    float front_tol = 0.05f;      // relative depth slack
    // Trim the hit set by distance, as a fraction of its own depth range.
    bool depth_limit = false;
    float near_frac = 0.0f, far_frac = 1.0f;
    // A Gaussian is not a point: test any part of its projected extent
    // rather than only its centre.
    bool by_extent = false;
    float extent_scale = 1.0f;
};

// A depth buffer over the live elements, at a reduced resolution. Rebuilt on
// a camera or live-set change rather than per selection: a set being
// re-trimmed by its depth sliders runs several times against one camera.
class OcclusionBuffer {
public:
    void build(const EditDoc& doc, const ViewProjection& view);
    void clear() { _z.clear(); _W = _H = 0; }
    bool valid() const { return _W > 0; }
    // True when nothing live is drawn in front of `depth` at that pixel.
    bool visible(float px, float py, float depth, float tol) const;

private:
    int _W = 0, _H = 0;
    float _back = 0.0f;          // ViewProjection::ortho_back it was built for
    float _scale = 1.0f;         // view pixels -> buffer pixels
    std::vector<float> _z;
};

// One weight per element for `st` under `view`. `out` is resized. Returns the
// depth range of what was hit before the depth trim, which is what the depth
// sliders are a fraction of.
struct SelectResult {
    int64_t hits = 0;
    float near_depth = 0.0f, far_depth = 0.0f;
};
SelectResult select_by_stencil(const EditDoc& doc, const ViewProjection& view,
                               const Stencil& st, const SelectOptions& opt,
                               const OcclusionBuffer* occ,
                               std::vector<uint8_t>& out);

// The live element under a pixel, or -1: the document's own answer where it
// has one (a mesh intersects its faces), else the nearest projected element,
// searched OUTWARD -- a flat surface's vertices are a long way apart.
int64_t pick_element(const EditDoc& doc, const ViewProjection& view,
                     float px, float py, float radius_px);

}  // namespace gui
