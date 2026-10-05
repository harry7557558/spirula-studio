#pragma once

// The sharpness score a frame is ranked by when ffmpeg decoded it -- the
// ffmpeg fallback of `spirula sam extract` and gui/FrameSelect's candidates.
// src/video/shaders/video.slang is the GPU copy the built-in decoder runs.

#include <cstdint>

namespace app {

// Box-average interleaved RGB into an `ow` x `oh` grey rectangle, BT.601 luma.
// `rows` is how much of the source to read, so an EAC canvas can be measured
// on its top row.
void box_grey(const uint8_t* rgb, int width, int rows, int ow, int oh, float* out);

// Matching extract_frames.py add_frame(): 512x512 grey, mean subtracted,
// variance of the 3x3 Laplacian.
double sharpness_score(const uint8_t* rgb, int width, int height);

}  // namespace app
