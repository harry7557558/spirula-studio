#pragma once

// Fitting a reconstruction onto a laser scan: the similarity (rotation,
// translation, scale -- any extrinsics) that takes the model's frame to the
// scan's. Seeded from anchors -- images whose pose in the scan frame is known --
// and refined by point-to-plane ICP of the model's points against the scan's
// surface. docs/notes/lidar-alignment.md has the measurements behind it.

#include "data/Knn.h"
#include "sfm/core/Pose.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace app::lidar {

// The scan as the alignment measures against it: thinned to one point a voxel,
// float offsets from `origin`, with unit normals (zero where the neighbourhood
// is no surface).
struct AlignCloud {
    double origin[3] = {0, 0, 0};
    std::vector<float> xyz, normal;
    std::vector<uint8_t> rgb;
    double voxel = 0;
    std::unique_ptr<knn::KdTree3> tree;
    size_t size() const { return xyz.size() / 3; }
};

// Consumes `xyz` (file frame) and `rgb`.
AlignCloud make_align_cloud(std::vector<double>& xyz, std::vector<uint8_t>& rgb,
                            int64_t target);

// What the alignment reads of a reconstruction: where its cameras are and its
// points, each with the number of images that see it.
struct ModelGeometry {
    struct View {
        std::string name;
        sfm::Mat3 R = sfm::mat3Identity();   // camera-to-world, OpenCV axes
        sfm::Vec3 centre;
    };
    std::vector<View> views;
    std::vector<sfm::Vec3> points;
    std::vector<int> track;
};

// A camera whose pose in the scan frame is known: an E57 image, or a view
// rendered from the scan. OpenCV axes, camera-to-world.
struct Anchor {
    std::string name;                 // relative to the image folder
    sfm::Mat3 R = sfm::mat3Identity();
    sfm::Vec3 centre;
    bool rendered = false;            // a view of the scan, not a photograph: never trained on
    // Its COLMAP camera, when known: what places it where the reconstruction
    // could not. -1 = none.
    int camera_model = -1;
    int64_t width = 0, height = 0;
    std::vector<double> params;
};

// The view that `name` is, with or without its extension; -1 if none.
int find_view(const ModelGeometry& m, const std::string& name);

struct AnchorFit {
    bool ok = false;
    sfm::Sim3 T;                      // model frame -> scan frame
    int registered = 0, inliers = 0;
    bool scale_known = false;         // the inlying anchors stand in more than one place
    double rot_err_deg = 0;           // medians over the inliers
    double centre_err_m = 0;
};
AnchorFit fit_anchors(const ModelGeometry& m, const std::vector<Anchor>& anchors);

struct IcpStats {
    double median_m = 0;              // |point-to-plane| over the points it kept
    double inlier_frac = 0;           // of the points it tried
    int64_t points = 0;
    int iterations = 0;
};
sfm::Sim3 refine_icp(const ModelGeometry& m, const AlignCloud& cloud,
                     const sfm::Sim3& init, IcpStats* stats = nullptr);

// Distance from each model point, through `T`, to the nearest scan point; NaN
// past `cap` metres.
std::vector<float> scan_distances(const ModelGeometry& m, const AlignCloud& cloud,
                                  const sfm::Sim3& T, double cap);

std::string describe(const sfm::Sim3& T);

}  // namespace app::lidar
