#pragma once

// An XGRIDS capture (.xbin): its LiDAR sweeps, the device's own trajectory,
// the factory calibration and the photographs of its cameras. The container
// has no published specification; what is read here was measured on PortalCam
// firmware V3.2.5 (docs/notes/lidar-alignment.md). The device message after
// the file header names the device, and a table before the first chunk names
// each topic; a device or a topic that was not measured is refused.

#include "data/E57Reader.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spirula::xbin {

struct Camera {
    std::string name;          // its key in the capture's camera.yaml
    bool fisheye = false;      // kb4, which is COLMAP OPENCV_FISHEYE; else OPENCV
    int width = 0, height = 0;
    double params[8] = {};     // fx fy cx cy, then k1..k4 (fisheye) or k1 k2 p1 p2
    int64_t frames = 0;
    double seconds = 0;        // first frame to last, from the index; 0 when it has none
    bool spare = false;        // XGRIDS' own LCC Studio uses none of its frames
};

struct Frame {
    int64_t counter = 0;        // the same on every camera's frame of one instant
    int64_t time_us = 0;
    std::vector<uint8_t> h264;  // one intra-coded access unit, Annex B
};

class Reader {
public:
    explicit Reader(const std::string& path);   // throws std::runtime_error
    ~Reader();

    const std::vector<Camera>& cameras() const { return _cameras; }
    int64_t sweeps() const;
    int64_t points_estimate() const;

    // Every LiDAR point in the device's world frame (metres, +Z up), each
    // placed by the trajectory at its own time, in batches; false when
    // `cancel` stopped it.
    bool read_points(const std::function<void(const e57::Point*, size_t)>& sink,
                     const std::atomic<bool>* cancel = nullptr);

    // Camera-to-world rotation (row-major, OpenCV axes) and centre of camera
    // `cam` at `time_us`; false outside the trajectory.
    bool camera_pose(size_t cam, int64_t time_us, double R[9], double centre[3]) const;

    // One camera's frames in recorded order; `sink` returns false to stop.
    void read_frames(size_t cam, const std::function<bool(const Frame&)>& sink);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
    std::vector<Camera> _cameras;
};

bool is_xbin_path(const std::string& path);

// The .xbin of an XGRIDS capture folder -- the one such file in `dir` -- or "".
std::string capture_in_folder(const std::string& dir);

}  // namespace spirula::xbin
