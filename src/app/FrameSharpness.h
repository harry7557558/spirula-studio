#pragma once

// The sharpness score a frame is ranked by when ffmpeg decoded it -- the
// ffmpeg fallback of `spirula sam extract` and gui/FrameSelect's candidates.
// src/video/shaders/video.slang is the GPU copy the built-in decoder runs.

#include <cstdint>
#include <memory>
#include <vector>

namespace blur { class EdgeWidthGpu; }

namespace app {

// Box-average interleaved RGB into an `ow` x `oh` grey rectangle, BT.601 luma.
// `rows` is how much of the source to read, so an EAC canvas can be measured
// on its top row.
void box_grey(const uint8_t* rgb, int width, int rows, int ow, int oh, float* out);

// 512x512 grey, mean subtracted, variance of the 3x3 Laplacian.
double sharpness_score(const uint8_t* rgb, int width, int height);
double sharpness_score(const uint8_t* rgb, int width, int height,
                       std::vector<float>& scratch);
// Big-endian RGB16 samples, scored in the same 0..255 units as RGB8.
double sharpness_score_rgb48be(const uint8_t* rgb, int width, int height,
                               std::vector<float>& scratch);

// How wide the sharpest of the frame's strong edges are, in its own pixels;
// the wider of across-x and across-y, since motion smears one. A length, so it
// compares between scenes. Negative where too few edges were found.
float edge_width_px(const uint8_t* rgb, int width, int height);

// edge_width_px on the inference layer's device (blur/EdgeWidthGpu.h) where the
// build has one and it starts, else on the host; SS_BLUR_CPU=1 forces the host.
// One a run, shared by its threads.
class EdgeMeter {
public:
    EdgeMeter();
    ~EdgeMeter();
    // `device` is the same pixels already on that device (a VkDeviceAddress),
    // read in place of an upload; 0 for none.
    float measure(const uint8_t* rgb, int width, int height, uint64_t device = 0);
    bool on_device() const { return _gpu != nullptr; }

private:
    std::unique_ptr<blur::EdgeWidthGpu> _gpu;
};

}  // namespace app
