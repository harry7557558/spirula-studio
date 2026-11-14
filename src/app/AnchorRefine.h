#pragma once

// A capture's photographs placed from the device's own poses instead of by a
// free reconstruction: the run's verified matches triangulated with those
// poses (sfm/map/KnownPoses.h), then one bundle adjustment in which each
// instant is one rig pose held near the device's, the factory rig turns only,
// and each camera's lens is refined. docs/notes/lidar-alignment.md.

#include "app/LidarAlign.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sfm {
struct MatchesDatabase;
struct FeatureSet;
}  // namespace sfm

namespace app::lidar {

// A probe bundle adjustment of a 300 s PortalCam capture, principal points free:
// 1 cm and 0.1 deg pushed the fisheyes' p90 to 10-13 px; 5 cm and 2 deg let the
// instants and the rig trade turns. --refine moved its photos 6.6 cm and 0.97 deg.
struct RefineOptions {
    double centre_sigma_m = 0.02;
    double rotation_sigma_deg = 0.5;
    double max_error_px = 4.0;     // extraction pixels
};

struct RefineResult {
    int64_t photos = 0, matched = 0, moments = 0, points = 0;
    double error_px = 0;                  // median reprojection, source pixels
    double moved_m = 0, turned_deg = 0;   // medians against the device's poses
};

// Refines, in place, the anchors that carry a lens: poses and, per folder, lens.
// A photograph `db` lacks keeps its instant's pose. The photographs of an
// instant that sees no point keep the device's poses and their factory lens.
RefineResult refine_anchors(std::vector<Anchor>& anchors, const sfm::MatchesDatabase& db,
                            const std::vector<sfm::FeatureSet>& features,
                            const RefineOptions& opt);

// The same over `workspace`/features/ and `workspace`/matches.bin. Throws when
// either is missing.
RefineResult refine_anchors_in(std::vector<Anchor>& anchors, const std::string& workspace,
                               const RefineOptions& opt);

}  // namespace app::lidar
