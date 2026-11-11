#pragma once

// The photographs of an XGRIDS capture (data/XbinReader.h) as anchors: each
// chosen camera's frames decoded by ffmpeg into `images_dir`/`subdir`/<camera>/,
// named by their frame counter, with their poses from the device's trajectory.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app::lidar {

struct XbinCameraFolder {
    std::string rel;            // relative to `images_dir`
    bool fisheye = false;
    double focal_factor = 0;    // fx over the width
    std::vector<double> params; // the factory lens, COLMAP order, source pixels
    int width = 0;
    int64_t frames = 0;
};

// One camera's part in a run. `fps` 0 keeps every frame it recorded.
struct XbinCameraPick {
    bool taken = true;
    float fps = 0.0f;
};

// A camera's frames thinned to `fps`, fed in recorded order: the first of each
// 1/`fps` s after the first, so cameras at one rate keep the same instants.
class FrameRate {
public:
    explicit FrameRate(float fps) : _period_us(fps > 0.0f ? 1e6 / fps : 0.0) {}
    bool keep(int64_t time_us) {
        if (_period_us <= 0.0) return true;
        if (_slot < 0) _first = time_us;
        const int64_t slot = (int64_t)std::floor((double)(time_us - _first) / _period_us);
        if (slot <= _slot) return false;
        _slot = slot;
        return true;
    }

private:
    double _period_us;
    int64_t _first = 0, _slot = -1;
};

// At most as many as FrameRate keeps of `frames` recorded over `seconds`.
inline int64_t frames_at_rate(int64_t frames, double seconds, float fps) {
    if (fps <= 0.0f || seconds <= 0.0) return frames;
    return std::min(frames, (int64_t)std::floor(seconds * fps) + 1);
}

// `picks`: one per camera, absent = taken at its own rate; one not taken loses
// its folder. `place_only`: never trained on. Replaces the anchors of `subdir`;
// `progress` gets the camera at work and frames written, of all.
using XbinProgress = std::function<void(const std::string& camera, int64_t done, int64_t total)>;
std::vector<XbinCameraFolder> extract_xbin_anchors(const std::string& xbin_path,
                                                   const std::string& images_dir,
                                                   const std::string& subdir,
                                                   const std::string& anchors_path,
                                                   const std::vector<XbinCameraPick>& picks,
                                                   bool place_only,
                                                   const std::string& ffmpeg_exe,
                                                   const std::atomic<bool>* cancel = nullptr,
                                                   const XbinProgress& progress = {});

// About `per_camera` of the frames a run with `picks` keeps of `camera`, spread
// through the capture, into `dir`/`camera`/ under the names a run gives them.
void extract_xbin_samples(const std::string& xbin_path, const std::string& dir,
                          const std::vector<XbinCameraPick>& picks, const std::string& camera,
                          int per_camera, const std::string& ffmpeg_exe,
                          const std::atomic<bool>* cancel = nullptr);

}  // namespace app::lidar
