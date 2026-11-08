#pragma once

// A handful of frames of one input, and the pixels of any of them: what the
// panels that try a setting on a real picture (SegmentPanel, GeometryPanel)
// both need before a dataset run has extracted anything.
//
// The pixels come back processed -- turned, scaled, a 360 capture unwrapped --
// exactly as preparation will write them (app/FrameLook.h), read through the
// decoder preparation will use. What a panel shows is therefore the file the
// run produces, and a click drawn on it names a pixel the masker will read.

#include "app/FrameLook.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace gui {

// One offer on a frame slider. A click records BOTH `index` and `position`,
// because which of the two survives depends on which extraction path the run
// takes (see MaskClick).
struct PreviewFrame {
    std::string path;       // empty for a video: decoded on demand
    long long   index = 0;
    float       position = 0.0f;   // 0..1 through the capture, or its clip
};

// How an input is read, how long it is, and what the run makes of it. One
// struct because the answers have to agree about which decoder is in use.
struct PreviewSource {
    std::string input;      // video file, or a folder of photos
    bool is_video = false;
    std::string ffmpeg_exe = "ffmpeg";
    bool builtin_decode = true;
    // Seconds, when ffmpeg had to be asked. Zero for photos, and for a video
    // the built-in decoder can seek by frame -- the ffmpeg path is the only
    // one that needs a timestamp.
    double video_seconds = 0.0;
    long long video_frames = 0;
    // The part of the video the run extracts, seconds; `clip_end` 0 is the
    // end. Offers stay inside it and `position` runs 0..1 across it.
    double clip_start = 0.0, clip_end = 0.0;
    long long clip_first = 0, clip_frames = 0;   // ... as frames, once probed
    // Filled by the caller from the dataset job, never guessed here.
    app::FrameLook look;
    // The run's device request (PrepJob::device), so the preview decodes and
    // segments on the GPU the run will use.
    std::string device;
    // Set when that request could not be honoured. A failed selection is not a
    // decode fallback: the preview reports it instead of reading the frame some
    // other way.
    std::string device_error;
    int tracks = 1;         // video tracks in the file
    int track = -1;         // the one extracted (PrepInput::active_track), -1 all
    // The same input color metadata passed to dataset masking.
    std::string image_gamut;
    std::optional<bool> image_is_linear;
    std::string image_exposure;

    // Photos read as the file STORES them rather than as it asks to be shown.
    // For a panel whose frames come with a camera: the camera describes the
    // stored pixels, and turning them would leave the two disagreeing.
    bool photos_as_stored = false;
};

// Whether a preview may decode on the inference device now, and the lock a
// decode holds while it does, so whoever takes the device back can wait it out.
struct DecodeGate {
    std::atomic<bool> allowed{false};
    std::mutex busy;
};

// `src` with the in-process decoder off unless `gate` allows it; `hold` keeps
// the gate's lock while the copy is in use.
PreviewSource gated(const PreviewSource& src, DecodeGate& gate,
                    std::unique_lock<std::mutex>& hold);

// The folders this input's frames land in under images/, and so what the
// `folder` argument below selects. One empty name for a single camera.
std::vector<std::string> preview_folders(const PreviewSource& src);

// `offers` frames spread through the input. `all_files` comes back holding
// every image of a photo folder (what a border fit reads), empty for a video.
void collect_preview_frames(PreviewSource& src, int offers,
                            std::vector<PreviewFrame>& frames,
                            std::vector<std::string>& all_files,
                            const std::atomic<bool>& cancel);

// One frame's pixels, w*h*3 interleaved. False with `error` set (already a
// sentence for the screen) when it cannot be read.
bool load_preview_frame(const PreviewSource& src, const PreviewFrame& frame,
                        int folder, int& w, int& h, std::vector<uint8_t>& rgb,
                        std::string& error, const std::atomic<bool>& cancel);

// The colour handling load_preview_frame gives a decoded video frame.
void preview_to_srgb(const PreviewSource& src, std::vector<uint8_t>& rgb);

struct PreviewPixels {
    int w = 0, h = 0;
    std::vector<uint8_t> rgb;
};
// `folders` of a video at `seconds`, from ONE ffmpeg run, each as
// load_preview_frame's ffmpeg path leaves it. `keyframe` settles for the sync
// sample at or before it; `max_side` > 0 scales a track down in ffmpeg.
bool load_ffmpeg_stills(const PreviewSource& src, double seconds,
                        const std::vector<int>& folders, bool keyframe, int max_side,
                        std::vector<PreviewPixels>& out, const std::atomic<bool>& cancel);

// A spread for a fit that wants several frames (the fisheye border): `samples`
// of `files`, or for a video the first few hundred frames -- as far forward as
// a decoder that cannot seek can afford -- and `offers` where ffmpeg seeks.
std::vector<PreviewFrame> spread_preview_frames(
    const PreviewSource& src, const std::vector<PreviewFrame>& offers,
    const std::vector<std::string>& files, int samples);

// `frames` handed over one at a time as they are read. A video is read forward
// ONCE, which is what makes this worth having over a load per frame.
void scan_preview_frames(const PreviewSource& src,
                         const std::vector<PreviewFrame>& frames, int folder,
                         const std::function<void(const uint8_t*, int, int)>& on_frame,
                         const std::atomic<bool>& cancel);

}  // namespace gui
