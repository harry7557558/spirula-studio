#pragma once

// One SAM prompt's detections -> one document-resolution stencil and the
// rectangle it covers, as MaskDoc::paint takes them. No ImGui, no GL, no sam::,
// no nn:: -- this half is what mask_doc_test compiles.
// Design: docs/notes/mask-editor.md.

#include "app/gui/mask/MaskDoc.h"

#include <cstdint>
#include <vector>

namespace gui {
namespace mask {

// sam::Detection's box in plane pixels.
struct RegionBox {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    bool set = false;
};

// A detection as the seam takes it: a 0/255 plane at the size the model was
// handed, and the box the model gave it.
struct AddRegion {
    int w = 0, h = 0;
    std::vector<uint8_t> mask;
    float score = 0.0f;
    RegionBox box;
};

// Clears from each region every pixel a same-sized `veto` plane covers: a
// negative phrase is an explicit keep. Returns how many pixels it cleared.
int64_t veto_regions(std::vector<AddRegion>& regions, const std::vector<AddRegion>& veto);

// Unions `regions` (consumed), each component grown by `margin` of its box
// (core/MaskMargin.h) and THEN vetoed, sam::compose_hit's order, into one W x H
// nearest-resampled stencil. False, outputs untouched, when nothing landed.
bool build_add_stencil(std::vector<AddRegion>& regions, int W, int H,
                       Stencil& out, Rect& bounds, int64_t& set_px, float margin = 0.0f,
                       const std::vector<AddRegion>& veto = {});

// A detection cut down to its set pixels' extent `box`, so the last add can be
// rebuilt at another margin without keeping the whole plane.
struct HeldRegion {
    int w = 0, h = 0;
    Rect box;
    std::vector<uint8_t> mask;   // box.w() x box.h()
    RegionBox model_box;
};
HeldRegion hold_region(const AddRegion& g);
AddRegion expand_region(const HeldRegion& held);

// Every mode grows by the margin: a keep or clear that stopped at SAM's exact
// outline would leave the rim an earlier drop's margin took. Never signed.
inline float add_margin(float ratio) { return ratio > 0.0f ? ratio : 0.0f; }

}  // namespace mask
}  // namespace gui
