#pragma once

// SceneCenter -- where a scene's origin goes: the `--scene-center` choices
// and the viewports' centering menu, over cameras and points in one frame.
// Implemented in parsers/DatasetCommon.cpp; kept apart from DatasetParser.h
// so the WebGL viewer's fast-math source can call it without the parsers.

#include "data/NavigationHint.h"

#include <array>
#include <cstdint>
#include <string>

namespace dsparse {

// Where a scene's origin goes. The names are the `--scene-center` choices and
// the viewport's centering menu, in that order.
enum class CenterMode {
    None = 0, PointMedian, CameraMedian, CameraFocus, PointMean, CameraMean
};
constexpr const char* kCenterModeNames[] = {
    "none", "point-median", "camera-median", "camera-focus", "point-mean",
    "camera-mean"};
constexpr int kNumCenterModes = 6;
// Throws on a name not in kCenterModeNames; "" is `none`.
CenterMode center_mode_from_name(const std::string& name);

// Weiszfeld from the per-axis median, ~8 passes over `pos`: n points of
// `stride` elements, the first three being xyz. `max_samples` > 0 strides
// over at most that many of them; the zero vector when n == 0.
std::array<double, 3> geometric_median(const double* pos, int64_t n,
                                       int stride = 3, int64_t max_samples = 0);
std::array<double, 3> geometric_median(const float* pos, int64_t n,
                                       int stride = 3, int64_t max_samples = 0);

// The point the cameras look at: the least-squares intersection of the
// optical axes of every camera that has it in front, iterated from `init`
// (camera_utils.focus_of_attention). c2w [N,3,4], OpenGL convention.
std::array<double, 3> focus_of_attention(const double* c2w, int64_t n,
                                         const double init[3]);

// The center `mode` names over c2w [N,3,4] and m points of `stride` elements.
// A point mode with no points falls back to the camera mode of the same
// statistic and vice versa; over nothing at all it is the origin.
std::array<double, 3> scene_center(CenterMode mode, const double* c2w, int64_t n,
                                   const double* points, int64_t m,
                                   int stride = 3, int64_t max_samples = 0);
std::array<double, 3> scene_center(CenterMode mode, const double* c2w, int64_t n,
                                   const float* points, int64_t m,
                                   int stride = 3, int64_t max_samples = 0);

struct ResolvedCenter {
    CenterMode mode = CenterMode::None;
    std::array<double, 3> center{0.0, 0.0, 0.0};
};
// A mode name, or `auto`: point-median when the cameras and the points each lie
// more than `auto_threshold` times their median distance to their own median
// from the origin, else none. c2w [N,3,4], m points of 3 values.
ResolvedCenter resolve_scene_center(const std::string& name, float auto_threshold,
                                    const double* c2w, int64_t n,
                                    const double* points, int64_t m);

// One centre per mode, in mode order.
using CenterTable = std::array<std::array<float, 3>, kNumCenterModes>;

// Every mode at once over c2w [N,3,4] (may be null) and m points of `stride`
// values, each mapped through `to_model` (row-major 3x4 similarity, null =
// identity). A viewport centre needs no more than 2^18 samples.
CenterTable scene_centers(const double* c2w, int64_t n,
                          const float* points, int64_t m,
                          int stride = 3, const double* to_model = nullptr);
CenterTable scene_centers(const double* c2w, int64_t n,
                          const double* points, int64_t m,
                          int stride = 3, const double* to_model = nullptr);

}  // namespace dsparse
