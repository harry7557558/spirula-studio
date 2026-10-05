#pragma once

// FrameExtract -- video in, a folder of sharp (optionally masked) frames out.
//
// Decoding (app/FrameDecode.h: Vulkan Video, or an ffmpeg child) composed with
// the sharpness pick and src/sam/ masking, driven identically by `spirula sam
// extract` and by the GUI's dataset preparation, so the two cannot drift.
//
// The frame-selection arithmetic reproduces
// reference/scripts/extract_frames.py's -- a window of `keep` frames, one
// written every `skip` -- and files are named by source frame index whichever
// decoder ran.

#include "app/FrameLook.h"
#include "app/FrameMotion.h"
#include "nn/io/Image.h"
#include "sam/Masking.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace app {

// Auto takes the in-process decoder when the build and the device have one,
// and ffmpeg otherwise.
enum class FrameDecoder { Auto, Builtin, Ffmpeg };

// What every frame goes through is the base class, so a preview can be handed
// it whole (app/FrameLook.h); this adds which frames are kept and where.
struct FrameExtractJob : FrameLook {
    std::string input;             // video file
    std::string image_dir;         // written here (cam0/, cam1/ ... if multi-track)
    std::string mask_dir;          // only when masking is on

    // The device request for this job, in the spelling
    // core/VulkanDeviceSelection.h parses. Frozen before the decode probe and
    // any Masker::init; a bad value fails the job.
    std::string device;

    FrameDecoder decoder = FrameDecoder::Auto;
    std::string ffmpeg_exe = "ffmpeg";

    // Selection
    int   skip = 1;                // write one frame every n source frames
    int   keep = -1;               // sharpest of the last n; -1 = round(skip/2)
    int   max_frames = 0;          // 0 = no cap
    int   quality = 95;            // JPEG quality; outside 0..100 writes PNG
    int   track = -1;              // -1 = every track
    int   threads = 0;             // encoder threads; 0 = cores - 1
    // A multi-track file decoded in lockstep, every track keeping the same
    // instants (one sharpness window over all of them), so the frames of one
    // stem are a rig. Off picks each track's sharpest frame on its own.
    bool  sync_tracks = false;
    // Space the kept frames by view change instead of by time
    // (app/FrameMotion.h): `skip` then sets the average and the rate stays
    // within `adaptive_range` of it. Costs one extra pass over the first track.
    bool  adaptive = false;
    float adaptive_range = 4.0f;
    // The spacing to keep, when the caller has already worked it out: several
    // videos on one rate share a budget, and that plan cannot be made from one
    // of them. Empty lets the run measure and plan its own.
    std::vector<int64_t> plan;

    // Masking. Empty model = no masks.
    sam::MaskOptions mask;
    bool  write_overlay = false;   // debug overlays next to the masks
};

struct FrameExtractStats {
    double decode = 0, sharpness = 0, convert = 0, mask = 0, submit = 0;
    double drain = 0, total = 0, encode_cpu = 0;
    double plan = 0;                 // the adaptive pass, decode included
    int64_t decoded = 0, measured = 0, written = 0, analyzed = 0;
    int    tracks = 1;
    int    encoder_threads = 0;
    int    write_failures = 0;
};

struct FrameExtractSinks {
    // One line of human-readable progress. May be called from a worker thread.
    std::function<void(const std::string&)> log;
    // (frames written so far, frames decoded so far). Called per written frame.
    std::function<void(int64_t, int64_t)> progress;
    // The adaptive pass, which writes nothing and runs before anything else
    // does: (frames looked at, frames in the track). Called as it goes.
    std::function<void(int64_t, int64_t)> scanning;
    // And the spacing it settled on: the source frame each kept frame ends at,
    // over a capture that many frames long.
    std::function<void(const std::vector<int64_t>&, int64_t)> planned;
    // Each step as it is measured, for a panel drawing the curve live: the
    // frame it ends at, how many the capture holds, and the view change across
    // it.
    std::function<void(int64_t, int64_t, float)> measured;
    // A frame worth showing, RGB8 tightly packed, on the extraction thread with
    // the picture still on the host, and the file it is about to be written to
    // (which the writer pool has not reached yet). Copy what you need and
    // return -- the decoder does not wait, and a caller that cannot keep up
    // should decline frames rather than slow it down.
    std::function<void(const uint8_t* rgb, int w, int h,
                       const std::string& path)> preview;
    // Polled between frames; set it to stop early. Frames already written stay.
    const std::atomic<bool>* cancel = nullptr;
};

// Runs the whole thing. False with `error` set on failure; a cancellation
// returns false with error == "cancelled".
bool extract_frames(const FrameExtractJob& job, const FrameExtractSinks& sinks,
                    FrameExtractStats& stats, std::string& error);

// The view change across this video's first track, which is what an adaptive
// plan is made of (app/FrameMotion.h). One decode of that track and nothing
// else: for a caller planning several videos against one budget.
bool scan_motion(const FrameExtractJob& job, const FrameExtractSinks& sinks,
                 MotionPlanInput& out, FrameExtractStats& stats,
                 std::string& error);

// What a plan came out as: the log line, and `sinks.planned`.
void report_plan(const FrameExtractSinks& sinks, const std::vector<int64_t>& plan,
                 int64_t frames, double fps);

// The timing table `spirula-sam extract` prints, so the CLI and the GUI log
// agree on what the numbers mean.
std::string format_extract_stats(const FrameExtractStats& s,
                                 const std::string& out_dir, bool masked);

// ---- only in SS_HAVE_VIDEO builds (app/FrameDecodeVulkan.cpp) ----

// "" when in-process decoding is available on this device, otherwise the
// reason it is not. CREATES the inference context, so freeze the device first;
// backends() keeps the build-level answer for a UI that must not probe yet.
std::string video_decode_availability();

// Number of video tracks in the file, or 0 with `error` set. Cheap: the
// demuxer alone, no decode session.
int video_track_count(const std::string& path, std::string& error);

// Each of those tracks' pixel size, in the same order.
std::vector<std::pair<int, int>> video_track_sizes(const std::string& path,
                                                   std::string& error);

// Each track's container display transform, in the same order (video::TrackInfo).
std::vector<sfm::ExifTransform> video_track_turns(const std::string& path,
                                                  std::string& error);

// The display transform `auto_rotate` folds into `look.rotate` for these
// tracks (empty = all of them). The mirror is reported and NOT applied: the
// pose that fits mirrored pixels is the mirror image of the real one.
sfm::ExifTransform fold_auto_rotate(const std::string& path,
                                    const std::vector<int>& tracks,
                                    FrameLook& look, bool& mixed);

// The frames at `indices`, as extract_frames() would write them, served by
// ONE forward pass -- the decoder cannot seek, so asking one at a time
// re-reads the file each time. `folder` indexes frame_folders().
using FrameAtSink = std::function<void(nn::Image& img, int64_t index)>;
bool extract_frames_at(const std::string& input, const FrameLook& look,
                       const std::vector<int64_t>& indices, int folder,
                       const FrameAtSink& on_frame,
                       const std::atomic<bool>* cancel, std::string& error,
                       const std::string& device = {});

bool extract_one_frame(const std::string& input, const FrameLook& look,
                       int64_t index, int folder, nn::Image& out,
                       const std::atomic<bool>* cancel, std::string& error,
                       const std::string& device = {});

}  // namespace app
