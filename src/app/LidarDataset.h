#pragma once

// A reconstruction moved into a laser scan's frame and given the scan's
// geometry: every model of the run aligned (LidarAlign.h) and merged into one
// COLMAP model at sparse/0, the scan's points as its seed points -- each with
// the images that see it as its track, so partitioning works on them -- and
// depth and normal maps rendered from the scan. docs/notes/lidar-alignment.md.

#include "app/LidarAlign.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app::lidar {

enum class AlignMode {
    Auto,      // anchors, then ICP
    Anchors,   // the anchors' fit alone
    Keep,      // the model is already in the scan's frame (an XGRIDS export)
    Refine,    // close to it already: ICP from where it is
};

struct DatasetOptions {
    std::string dataset;                 // the reconstruction, and where maps go
    std::string image_dir = "images";    // relative to `dataset`, or absolute
    std::vector<std::string> clouds;     // .e57 / .las / .ply, one frame
    std::string anchors;                 // "" = <dataset>/lidar/anchors.json, if there
    AlignMode mode = AlignMode::Auto;
    int64_t seed_points = 500000;        // 0 keeps the model's own points only
    bool all_points = false;
    bool depth_maps = true;
    int track_cap = 12;                  // images listed per scan point
    bool sfm_points_in_gaps = true;      // keep model points where the scan has none
    bool flip_masks = false;             // the dataset's masks paint what to remove
    bool overwrite = false;              // redo a result whose inputs have not changed
    bool scanner_poses_only = false;     // the reconstruction failed: use none of it
};

struct DatasetResult {
    int models = 0, aligned = 0;
    int64_t images = 0, seed_points = 0, sfm_points = 0, depth_maps = 0;
    double residual_m = 0;               // median point-to-plane, largest model
    bool cancelled = false;
};

// In place: the reconstruction it starts from is kept at sparse_unaligned/,
// which a second run reads again; nothing changes until the result is ready.
// Throws on what it cannot read.
DatasetResult write_lidar_dataset(const DatasetOptions& opt,
                                  const std::function<void(const std::string&)>& log,
                                  const std::atomic<bool>* cancel = nullptr);

// The images of an E57 file as anchors: written under `images_dir`/`subdir`
// (in pinhole/ and panorama/ below it when it has both), their poses added to
// `anchors_path`.
struct ExtractedPhotos {
    int64_t pinhole = 0, panorama = 0;
    double focal = 0;                    // the pinholes' median, pixels
    double focal_factor = 0;             // ... over their width
    bool split = false;
};
ExtractedPhotos extract_e57_anchors(const std::string& e57_path, const std::string& images_dir,
                                    const std::string& subdir, const std::string& anchors_path);

// Views rendered from where the scanner stood (E57 scan poses, a PLY `camera`,
// a `<name>.trajectory.las` beside the file) and recorded as anchors, for a
// scan without photographs. Returns how many.
int64_t render_anchor_views(const std::vector<std::string>& clouds, const std::string& images_dir,
                            const std::string& subdir, const std::string& anchors_path,
                            const std::function<void(const std::string&)>& log,
                            const std::atomic<bool>* cancel = nullptr);

std::vector<Anchor> read_anchors(const std::string& path);
void write_anchors(const std::string& path, const std::vector<Anchor>& anchors);

}  // namespace app::lidar
