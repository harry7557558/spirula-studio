#pragma once

// A capture's trajectory made again offline: the device's own is real time and
// drifts over minutes. Kept per dataset as moves of the device's world
// (`xbin::Correction`), so the frames, anchors and scan of one dataset agree.

#include "data/XbinReader.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace app::lidar {

// <capture stem>.trajectory.csv in a dataset's lidar folder, beside anchors.json.
std::string trajectory_path(const std::string& lidar_dir, const std::string& capture);

// Empty when the file does not open. Throws on a bad line or on times out of order.
spirula::xbin::Correction read_trajectory(const std::string& path);
void write_trajectory(const std::string& path, const spirula::xbin::Correction& c);

struct TrajectoryReport {
    int64_t sweeps = 0, nodes = 0;
    int64_t neighbour_edges = 0, loops_tried = 0, loops = 0;
    // Occupied 5 cm voxels of every tenth sweep: fewer is a sharper map.
    int64_t voxels_device = 0, voxels_built = 0;
    double moved_m = 0;    // median over the nodes, from the device's trajectory
    bool kept = false;     // false: the built trajectory was no sharper, and is empty
    double seconds_read = 0, seconds_neighbours = 0, seconds_loops = 0, seconds_graph = 0;
    int64_t solver_steps = 0;     // the most conjugate-gradient steps one graph step took
    double solver_residual = 0;   // the worst final |r| / |g| of them
};

// The device's trajectory corrected by its own LiDAR: each second of sweeps
// registered to its neighbours and to the places walked again (GICP), then one
// pose graph. Empty when cancelled or no sharper than the device's.
spirula::xbin::Correction build_trajectory(spirula::xbin::Reader& reader, TrajectoryReport& report,
                                           const std::function<void(double)>& progress = {},
                                           const std::atomic<bool>* cancel = nullptr);

// Builds and writes the trajectory file of `capture` in `lidar_dir` unless it
// is there. A file of no keys records that the device's trajectory stays.
// False when cancelled.
bool ensure_trajectory(const std::string& capture, const std::string& lidar_dir,
                       const std::function<void(const std::string&)>& log,
                       const std::function<void(double)>& progress = {},
                       const std::atomic<bool>* cancel = nullptr);

}  // namespace app::lidar
