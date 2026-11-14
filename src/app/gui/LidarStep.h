#pragma once

// The laser-scan half of a dataset run (docs/notes/lidar-alignment.md): before
// the reconstruction, the scans' photographs -- or, for a scan that has none,
// views rendered from where it was taken -- go in beside the frames with their
// poses as anchors; after it, `spirula lidar` moves the model into the scans'
// frame and renders depth and normals from them, in place of the geometry step.

#include "app/XbinAnchors.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/FilmReel.h"
#include "app/gui/PrepProgress.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gui {

struct LidarJob {
    std::vector<std::string> clouds;   // .e57 / .las / .ply / .xbin
    std::vector<std::string> with_photos;   // those of them with photographs
    // An XGRIDS capture's cameras and the rate each gives photographs at; absent = all of all.
    std::map<std::string, std::vector<app::lidar::XbinCameraPick>> xbin_cameras;
    // Captures whose photographs only place the model: in the reconstruction, not the dataset.
    std::set<std::string> xbin_place_only;
    app::lidar::XbinDecoder decoder;   // an XGRIDS capture's frames
    bool scan_photos = true;           // an E57's photographs join the reconstruction
    bool in_frame = false;             // the model is already in the scans' frame
    bool scanner_only = false;         // its photographs are all the images: no reconstruction
    std::string mask_dir;              // the dataset's masks, "" for none
    bool flip_masks = false;           // the dataset's masks paint what to remove
    bool scanner_poses_only = false;   // no reconstruction, or one that failed: use none of it
    bool refine_capture = false;       // the scanner's poses refined on the run's matches instead
    bool overwrite = false;            // align again although nothing it was made from changed
    bool enabled() const { return !clouds.empty(); }
    // A capture's photographs are taken whatever `scan_photos` says: its points
    // have no colour, so views rendered from them would place nothing.
    bool takes_photos(const std::string& cloud) const {
        return !in_frame && (scan_photos || xbin_cameras.count(cloud) > 0);
    }
};

struct LidarPrep {
    std::vector<std::string> sfm_args;   // the rendered views' lens, for `spirula sfm`
    int64_t photos = 0;                  // scanner photographs with known poses
    int64_t views = 0;                   // views rendered from the scans
    std::vector<std::string> photographed;   // the clouds those photographs came from
};

// Each scan's folder under <workspace>/lidar/photos/, in `clouds` order.
std::vector<std::string> scan_photo_subdirs(const std::vector<std::string>& clouds);

// Before the frames: each scan's photographs, extracted under
// <workspace>/lidar/photos/ with their poses recorded as anchors, become
// inputs of their own, imported and masked like any other with `stencils`.
bool add_scan_photo_inputs(const LidarJob& job, const std::string& workspace,
                           const std::map<std::string, app::FrameStencil>& stencils,
                           std::vector<PrepInput>& inputs, LidarPrep& out, RunProgress& prog,
                           const std::atomic<bool>& cancel, std::string& error);

// One scan's photographs to draw on before a run: `dir` is the folder. An E57's
// go where a run extracts them; a capture's are a few of the frames the run
// keeps of `camera`, in `scratch`, which the caller removes.
bool extract_scan_photos(const LidarJob& job, size_t scan, const std::string& workspace,
                         const std::string& scratch, const std::string& camera,
                         const std::atomic<bool>& cancel, std::string& dir, std::string& error);

// After the frames: views of the scans without photographs rendered into
// `image_dir`/scan_views as anchors, unless one sharing their frame has them,
// and only when that folder is inside `workspace` (else it is the user's).
bool render_scan_views(const LidarJob& job, const std::string& workspace,
                       const std::string& image_dir, RunProgress& prog,
                       const std::atomic<bool>& cancel, LidarPrep& out, std::string& error);

bool run_lidar_step(const LidarJob& job, const std::string& dataset,
                    const std::string& image_dir, RunProgress& prog, FilmReel* reel,
                    const std::atomic<bool>& cancel, std::string& error);

}  // namespace gui
