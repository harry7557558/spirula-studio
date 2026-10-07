#pragma once

// Picture -- one dataset image in memory, with its mask composited in, at the
// size the screen is about to draw it.
//
// That size is the whole point. A copy smaller than the pane is upscaled into
// it, which is exactly what "blurry" looks like; a 4K copy in a 900-pixel pane
// is minified by a 2x2 tap, which is what "noisy" looks like -- and a mipmap
// would not help, because the ImGui backend binds its own LINEAR sampler over
// whatever the texture asks for. So the resampling happens here, on a worker
// thread, with a box filter, once per picture. stb's buffers are read in place
// and the mask is decoded only once the photo is boxed, so the two never coexist.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gui {

struct Picture {
    std::vector<uint8_t> rgb;      // w*h*3
    int w = 0, h = 0;              // as stored
    int src_w = 0, src_h = 0;      // as the file has it
    // The long edge this copy was made for. Not the same as `w`: the box
    // filter steps by whole source pixels, so a 3840-wide frame made for a
    // 1600-pixel pane is 1280 wide and asking again would only produce 1280
    // again. Comparing the request instead of the result is what keeps a pane
    // that falls between two steps from reloading for ever.
    int made_for = 0;
    bool empty() const { return rgb.empty(); }
    size_t bytes() const { return rgb.size(); }
    bool fits(int side) const { return made_for >= side; }
};

// Compose `rgb` (w*h*3) with `mask` and `feature_mask` (w*h, 255 = keep, null
// for none; MaskTint.h), box filtered down to `max_side` on the long edge. 0,
// or a size the source does not reach, keeps the source: it never upscales.
void make_picture(const uint8_t* rgb, int w, int h, const uint8_t* mask,
                  int max_side, Picture& out, const uint8_t* feature_mask = nullptr);

// The same from files. `mask_path` may be empty or absent; a mask stored at
// another size than its image is sampled to it. `mask_flipped`: the file's
// 255 is drop (TrainConfig::flip_mask). Reuses `out`'s buffer.
bool load_picture(const std::string& image_path, const std::string& mask_path,
                  int max_side, Picture& out, bool mask_flipped = false,
                  const std::string& feature_mask_path = "");

// One panel of a row picture.
struct PicturePanel {
    std::string path;
    // A 16-bit depth PNG, coloured with the training viewport's ramp
    // (app/DepthColor.h) rather than shown as the grey its high byte gives.
    bool depth = false;
};

// Several files as ONE picture, side by side at a common height: a frame's
// photograph, normal map and depth map are only worth anything read together.
// A panel that will not load is skipped; false when none of them did.
bool load_picture_row(const std::vector<PicturePanel>& panels, int max_side,
                      Picture& out);

}  // namespace gui
