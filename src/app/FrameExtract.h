#pragma once

// FrameExtract -- video in, a folder of sharp (optionally masked) frames out.
//
// The composition of src/video/ (decode + a sharpness shader) and src/sam/
// (masking), driven identically by `spirula-sam extract` and by the GUI's
// dataset preparation, so the two cannot drift. It sits in src/app/ for the
// same reason TrainerCore does: it belongs to neither subsystem, and every
// front end wants exactly one copy of it.
//
// The frame-selection arithmetic reproduces
// reference/scripts/extract_frames.py's -- a window of `keep` frames, one
// written every `skip` -- so the same source frames get the same file names
// whichever path produced them. That is what lets the GUI fall back to
// ffmpeg + FrameSelect without the dataset changing.
//
// Only compiled when SS_ENABLE_PATENTED is ON; every caller has an ffmpeg
// fallback for when it is not.

#include "app/Pano360.h"
#include "sam/Masking.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace app {

struct FrameExtractJob {
    std::string input;             // video file
    std::string image_dir;         // written here (cam0/, cam1/ ... if multi-track)
    std::string mask_dir;          // only when masking is on

    // Selection
    int   skip = 1;                // write one frame every n source frames
    int   keep = -1;               // sharpest of the last n; -1 = round(skip/2)
    int   max_frames = 0;          // 0 = no cap
    int   quality = 95;            // JPEG quality; outside 0..100 writes PNG
    int   rotate = 0;              // 0/90/180/270 clockwise
    float scale = 1.0f;
    int   track = -1;              // -1 = every track
    int   threads = 0;             // encoder threads; 0 = cores - 1
    // A multi-track file decoded in lockstep, every track keeping the same
    // instants (one sharpness window over all of them), so the frames of one
    // stem are a rig. Off picks each track's sharpest frame on its own.
    bool  sync_tracks = false;

    // A 360 capture: both tracks are decoded together, stitched into the EAC
    // canvas and resampled into `views` (app/Pano360.h) instead of being split
    // one folder per track. `track`, `scale` and `rotate` do not apply.
    Eac360Layout eac;
    std::vector<Pano360View> views;

    // Masking. Empty model = no masks.
    sam::MaskOptions mask;
    bool  write_overlay = false;   // debug overlays next to the masks
};

struct FrameExtractStats {
    double decode = 0, sharpness = 0, convert = 0, mask = 0, submit = 0;
    double drain = 0, total = 0, encode_cpu = 0;
    int64_t decoded = 0, measured = 0, written = 0;
    int    tracks = 1;
    int    encoder_threads = 0;
    int    write_failures = 0;
};

struct FrameExtractSinks {
    // One line of human-readable progress. May be called from a worker thread.
    std::function<void(const std::string&)> log;
    // (frames written so far, frames decoded so far). Called per written frame.
    std::function<void(int64_t, int64_t)> progress;
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

// "" when in-process decoding is available on this device, otherwise the
// reason it is not (missing extension, unsupported codec, no driver support).
std::string video_decode_availability();

// Number of video tracks in the file, or 0 with `error` set. Cheap: the
// demuxer alone, no decode session.
int video_track_count(const std::string& path, std::string& error);

// Each of those tracks' pixel size, in the same order.
std::vector<std::pair<int, int>> video_track_sizes(const std::string& path,
                                                   std::string& error);

// Runs the whole thing. False with `error` set on failure; a cancellation
// returns false with error == "cancelled".
bool extract_frames(const FrameExtractJob& job, const FrameExtractSinks& sinks,
                    FrameExtractStats& stats, std::string& error);

// The timing table `spirula-sam extract` prints, so the CLI and the GUI log
// agree on what the numbers mean.
std::string format_extract_stats(const FrameExtractStats& s,
                                 const std::string& out_dir, bool masked);

}  // namespace app
