#pragma once

// Where a capture's scan goes once its photographs are refined. The device's
// trajectory placed both, and it drifts: on a 975 s PortalCam walk the refined
// photographs of a place walked twice agree to 3 px, the scan there sits 14 cm
// off them (9 cm where the walk passed once). Moving the world at each instant
// by what the refinement moved its photographs takes the scan along.

#include "data/XbinReader.h"
#include "sfm/geometry/LinAlg.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace app::lidar {

// One photograph: camera-to-world pose as the reader gives it before the
// refinement, and after it.
struct PosePair {
    int64_t time_us = 0;
    std::string camera;              // its folder; each camera has its own mount
    sfm::Mat3 R_device, R_refined;
    sfm::Vec3 c_device, c_refined;
};

struct ScanCorrection {
    spirula::xbin::Correction keys;              // smoothed, at the instants kept
    std::vector<int64_t> instants;               // every instant, ascending
    std::vector<std::array<double, 6>> raw;      // fitted turn (rotation vector), then own shift
    std::vector<char> kept;
    int64_t rejected = 0;
    double moved_m = 0, turned_deg = 0;          // medians over the keys, at their cameras
    double mount_deg = 0;                        // the largest fixed turn taken out of a camera
    double mount_shift_m = 0;                    // the largest mean shift in a camera's frame, left in
};

// `window_s` spaces the knots of the world's turn and is the span each shift is
// smoothed over: drift changes over tens of seconds, a refined pose's own error
// from one instant to the next.
ScanCorrection scan_correction(const std::vector<PosePair>& pairs, double window_s);

}  // namespace app::lidar
