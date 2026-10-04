#pragma once

// The laser-scan half of a dataset run (docs/notes/lidar-alignment.md): before
// the reconstruction, the scans' photographs -- or, for a scan that has none,
// views rendered from where it was taken -- go in beside the frames with their
// poses as anchors; after it, `spirula lidar` moves the model into the scans'
// frame and renders depth and normals from them, in place of the geometry step.

#include "app/gui/DatasetPrep.h"
#include "app/gui/FilmReel.h"
#include "app/gui/PrepProgress.h"

#include <atomic>
#include <string>
#include <vector>

namespace gui {

struct LidarJob {
    std::vector<std::string> clouds;   // .e57 / .las / .ply, one frame
    bool scan_photos = true;           // an E57's photographs join the reconstruction
    bool in_frame = false;             // the model is already in the scans' frame
    bool flip_masks = false;           // the dataset's masks paint what to remove
    bool scanner_poses_only = false;   // the reconstruction failed: use none of it
    bool enabled() const { return !clouds.empty(); }
};

struct LidarPrep {
    std::vector<std::string> sfm_args;   // the rendered views' lens, for `spirula sfm`
    int64_t photos = 0;                  // scanner photographs with known poses
    int64_t views = 0;                   // views rendered from the scans
};

// Before the frames: each scan's photographs, extracted under
// <workspace>/lidar/photos/ with their poses recorded as anchors, become
// inputs of their own, so they are imported and masked like any other.
bool add_scan_photo_inputs(const LidarJob& job, const std::string& workspace,
                           std::vector<PrepInput>& inputs, LidarPrep& out, std::string& error);

// After the frames, for scans without photographs: views of them rendered
// into `image_dir`/scan_views as anchors -- only when that folder is inside
// `workspace`, since a folder of photos read in place is the user's.
bool render_scan_views(const LidarJob& job, const std::string& workspace,
                       const std::string& image_dir, RunProgress& prog,
                       const std::atomic<bool>& cancel, LidarPrep& out, std::string& error);

bool run_lidar_step(const LidarJob& job, const std::string& dataset,
                    const std::string& image_dir, RunProgress& prog, FilmReel* reel,
                    const std::atomic<bool>& cancel, std::string& error);

}  // namespace gui
