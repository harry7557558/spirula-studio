#pragma once

// FrameSelect -- which of the candidate frames ffmpeg extracted are kept.
//
// The sharpness metric is the variance of the 3x3 Laplacian of the
// mean-subtracted 512x512 grayscale image. Evenly, one per group of `group`
// candidates; or spaced by how much the view changes (app/FrameMotion.h),
// which is what the built-in decoder's adaptive mode does from the source
// frames themselves.

#include "app/FrameMotion.h"
#include "app/FramePick.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace gui {

struct FrameSelectOptions {
    int group = 1;          // candidates per kept frame
    int max_frames = 0;     // 0 = no cap
    // Adaptive: the rate stays within `range` either side of the average, and
    // the sharpest of `window` candidates around each chosen one is kept.
    bool  adaptive = false;
    float range = 4.0f;
    int   window = 3;
    app::MotionView view = app::MotionView::Planar;
    // Set when the candidates are 360 canvases. An EAC one is measured on its
    // top row alone, which is half the sphere and all a rotation needs.
    app::Pano360Layout eac;
    float out_fov = 1.5708f;
    // Which picks are left out after all (app/FramePick.h, PickRule).
    float blur_reject = 0.0f;
    bool  blur_neighbour = true;
    int   similar_bits = 0;
    // Where a pick left out is moved rather than deleted, so it can still be
    // looked at. Empty deletes it.
    std::string skipped_dir;
    // (candidates scored, candidates in all), and the spacing settled on: the
    // candidate each kept frame ends at, out of that many. Both optional.
    std::function<void(int64_t, int64_t)> scanning;
    std::function<void(const std::vector<int64_t>&, int64_t)> planned;
    // Each candidate's view change as it is measured, for a live curve:
    // the candidate it ends at, how many there are, and the change.
    std::function<void(int64_t, int64_t, float)> measured;
    // Each pick as the rule sees it, kept or not, with its thumbnail as a JPEG
    // (gui/PickSheet.h) and the file it ends up as, "" for none.
    std::function<void(const app::FramePick&, std::vector<uint8_t>&&,
                       const std::string&)> judged;
};

// Scores every image in cand_dir (sorted by filename) across all hardware
// threads and MOVES the keepers to out_dir as <prefix>NNNNN.<ext>, numbered by
// their place in cand_dir; the losers are deleted. -1 on error or cancellation.
int select_sharpest_frames(const std::string& cand_dir,
                           const std::string& out_dir,
                           const std::string& prefix,
                           const FrameSelectOptions& options,
                           const std::function<void(const std::string&)>& log,
                           const std::atomic<bool>& cancel);

// The same rule over a folder of images that already ARE the frames, by file
// name. Those left out are MOVED under `options.skipped_dir`, or DELETED when
// it is empty (`judged` still gets where each was). Returns kept, -1 cancelled.
int judge_image_folder(const std::string& dir, const FrameSelectOptions& options,
                       const std::function<void(const std::string&)>& log,
                       const std::atomic<bool>& cancel);

// The same over frames of several files: each of `folders` under `dir` is a
// camera ("" is `dir`), files of one name are one frame, as blurred as its
// worst lens, and it goes or stays whole. `judged` gets the first folder's.
int judge_image_sets(const std::string& dir, const std::vector<std::string>& folders,
                     const FrameSelectOptions& options,
                     const std::function<void(const std::string&)>& log,
                     const std::atomic<bool>& cancel);

}  // namespace gui
