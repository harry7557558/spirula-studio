#pragma once

// Fitting a reconstruction onto a laser scan: the similarity (rotation,
// translation, scale -- any extrinsics) that takes the model's frame to the
// scan's. Seeded from anchors -- images whose pose in the scan frame is known --
// and refined by point-to-plane ICP of the model's points against the scan's
// surface. docs/notes/lidar-alignment.md has the measurements behind it.

#include "data/Knn.h"
#include "data/PointCloudFile.h"
#include "sfm/core/Pose.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace app {
struct ScanCloud;
}

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
        std::vector<int> seen;               // indices into `points`
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
    sfm::Vec3 centre;                 // in `scan`'s own frame
    std::string scan;                 // the file it came from; "" = not recorded
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
    std::vector<size_t> used;         // the inliers, as indices into `anchors`
};
// `scale` > 0 holds the scale rather than fitting it: a metric model placing
// a scan of its own, which then moves rigidly.
AnchorFit fit_anchors(const ModelGeometry& m, const std::vector<Anchor>& anchors,
                      double scale = 0);

struct IcpStats {
    double median_m = 0;              // |point-to-plane| over the points it kept
    double inlier_frac = 0;           // of the points it tried
    int64_t points = 0;
    int iterations = 0;
};
// The scale anchors in one place leave open: each model point an anchor sees
// lies along the same ray in the scan, so the median ratio of the scan's range
// there to the model's. 0 when too few were measured; `points` says how many.
double scale_from_depth(const ModelGeometry& m, const std::vector<Anchor>& anchors,
                        const AnchorFit& fit, const app::ScanCloud& scan,
                        int64_t* points = nullptr);

sfm::Sim3 refine_icp(const ModelGeometry& m, const AlignCloud& cloud,
                     const sfm::Sim3& init, IcpStats* stats = nullptr);

// Distance from each model point, through `T`, to the nearest scan point; NaN
// past `cap` metres.
std::vector<float> scan_distances(const ModelGeometry& m, const AlignCloud& cloud,
                                  const sfm::Sim3& T, double cap);

std::string describe(const sfm::Sim3& T);

// Whether scan files share one frame by their own word: one registration
// writes its stations apart. Two files with no station of their own (mobile
// scans, unregistered setups) or with stations in one place are separate.
bool scans_share_frame(const std::vector<std::vector<spirula::cloud::Station>>& stations);

// Which scans share a frame by the reconstruction: in each model, those whose
// anchors (`scan_of` says whose) one fit holds. Frames by support, 0 the most;
// a scan no model saw is -1, or joins the only frame there is.
struct ScanFrames {
    std::vector<int> frame;           // per scan
    int count = 0;
};
ScanFrames group_scan_frames(const std::vector<ModelGeometry>& models,
                             const std::vector<Anchor>& anchors,
                             const std::vector<int>& scan_of, int scans);

// Each frame's rigid move into frame 0, through the models already fitted to
// frame 0 (`fits[m]`, model -> frame 0): from the fitted model holding most of
// the frame's anchors, at that model's scale. Frame 0 is placed as it is.
struct FramePlacement {
    bool placed = false;
    sfm::Sim3 T;                      // this frame -> frame 0, scale 1
    int model = -1, anchors = 0;      // what placed it
};
std::vector<FramePlacement> place_frames(const std::vector<ModelGeometry>& models,
                                         const std::vector<std::optional<sfm::Sim3>>& fits,
                                         const std::vector<Anchor>& anchors,
                                         const std::vector<int>& scan_of,
                                         const ScanFrames& frames);

}  // namespace app::lidar
