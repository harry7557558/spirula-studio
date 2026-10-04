#pragma once
// Per-pixel focus weights: how much each pixel of a photo should count in the
// photometric loss, from how far it sits from the plane the lens focused on.
// docs/notes/focus-weights.md has the method and the numbers behind it.

#include <cstdint>
#include <string>
#include <vector>

namespace app {

struct FocusEdges {
    std::vector<int32_t> y, x;   // on the measurement raster
    std::vector<float> sigma;    // optical blur, measurement px
    std::vector<float> weight;
};

// Edge blur on a grayscale raster (values 0..1, row-major).
FocusEdges measure_edge_blur(const std::vector<float>& gray, int w, int h);

struct FocusCurve {
    double u_f = 0, b0 = 0, k_near = 0, k_far = 0;
    double rms = 0;
    double u_lo = 0, u_hi = 1;
    int edges_used = 0;
    bool ok = false;
};

// blur(u) = sqrt(b0^2 + (k_near (u - u_f)+)^2 + (k_far (u_f - u)+)^2), u the
// inverse depth. Zero-depth pixels carry no answer and are skipped.
FocusCurve fit_focus_curve(const FocusEdges& edges, int meas_w, int meas_h,
                           const std::vector<float>& inv_depth, int depth_w,
                           int depth_h);

struct FocusSettings {
    float allowed_px = 1.4f;   // defocus blur allowed, px at train_side
    int train_side = 5760;
    float softness = 0.5f;
};

// Weights 0..255 at the photo's size; 0 where the depth had no answer.
std::vector<uint8_t> render_focus_weight(const FocusCurve& c,
                                         const std::vector<float>& inv_depth,
                                         int depth_w, int depth_h, int meas_w,
                                         int meas_h, int full_w, int full_h,
                                         const FocusSettings& s);

// The integer downsample the blur is measured at: noise and runtime both drop,
// and 2x on 6000-8200 px originals is what the defaults were validated on.
int focus_measure_factor(int width, int height);

std::vector<float> gray_downsampled(const uint8_t* rgb, int w, int h, int factor,
                                    int& out_w, int& out_h);

}  // namespace app
