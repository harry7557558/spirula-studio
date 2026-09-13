#pragma once

// FrameLook -- what an input's frames look like once they are in the dataset:
// the turn the capture asks for, an extra turn and a downscale, or -- for a
// 360 file -- the unwrap into views.
//
// One description, applied twice. Extraction bakes it in (app/FrameExtract.h)
// and every panel that shows a frame outside a run reads the same fields back
// (app/gui/PreviewFrames.h). A preview that skips part of it shows a picture
// the run does not write, and a click drawn on that picture names a pixel
// nothing will ever read.

#include "app/Pano360.h"
#include "core/ImageOrient.h"
#include "nn/io/Image.h"
#include "sfm/core/Exif.h"   // ExifTransform, the turn-then-mirror convention

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace app {

struct FrameLook {
    int   rotate = 0;           // extra degrees clockwise, a multiple of 90
    // Plus the display transform the file carries -- a video container's
    // matrix, a photo's EXIF. False where the dataset keeps the stored pixels
    // and leaves the tag on the file for its readers.
    bool  auto_rotate = true;
    float scale = 1.0f;
    // A 360 capture is unwrapped rather than turned: the layout is measured in
    // source pixels and the plan turns the sphere itself.
    Eac360Layout eac;
    std::vector<Pano360View> views;
    bool pano() const { return eac.valid() && !views.empty(); }
};

// The folders a video's frames land in under its image directory: a 360 plan's
// views, a multi-lens file's cam0/cam1..., or one empty name for one camera.
std::vector<std::string> frame_folders(const FrameLook& look, int tracks);

// The turn a photo's EXIF Orientation asks for; identity where there is none.
sfm::ExifTransform photo_turn(const std::string& file);

// One turn for `tracks` (empty = all of them), which is what extraction
// applies to all of them: the first track's, with `mixed` set when the rest
// disagreed. A mirror on any of them carries.
sfm::ExifTransform merge_turns(const std::vector<sfm::ExifTransform>& per_track,
                               const std::vector<int>& tracks, bool& mixed);

// Turn and mirror an interleaved host image in place.
template <typename T>
void turn_pixels(const sfm::ExifTransform& t, int channels, std::vector<T>& px,
                 int& w, int& h) {
    if (t.identity() || px.empty() || w <= 0 || h <= 0) return;
    std::vector<T> turned(px.size());
    spirula::orient_pixels(px.data(), w, h, channels, t.turns_cw, t.mirror,
                           turned.data());
    px.swap(turned);
    spirula::oriented_size(t.turns_cw, w, h);
}

// `t` undone: what puts a mask, a depth map or a normal map back in the frame
// the file stores, which is the one the trainer pairs with the image. A
// mirroring transform is its own inverse.
inline sfm::ExifTransform inverse_turn(const sfm::ExifTransform& t) {
    return t.mirror ? t : sfm::ExifTransform{(4 - t.turns_cw) & 3, false};
}

// An xyz map turned: the pixels move and so do the directions they hold, x and
// y being image axes. A mirror flips x, which is what makes it not a rotation.
void turn_normals(const sfm::ExifTransform& t, std::vector<float>& xyz,
                  int& w, int& h);

// A dataset image turned upright by its own EXIF, as models were trained; `turn`
// puts per-pixel output back beside the image. Inline: every app target compiles
// this header, and only SS_BUILD_SAM ones link ss_nn.
inline nn::Image load_upright(const std::string& file, const std::string& gamut,
                              std::optional<bool> is_linear,
                              sfm::ExifTransform& turn) {
    turn = photo_turn(file);
    nn::Image img = nn::load_image(file, gamut, is_linear);
    if (!img.empty())
        turn_pixels(turn, img.channels, img.data, img.width, img.height);
    return img;
}

}  // namespace app
