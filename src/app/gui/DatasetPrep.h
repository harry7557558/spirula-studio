#pragma once

// DatasetPrep -- everything between "the user picked a video or a folder of
// photos" and "there is an image directory (and maybe a mask directory) ready
// for structure-from-motion".
//
// It was the half of ColmapRunner that had nothing to do with COLMAP: frame
// extraction, sharpest-frame selection, splitting a multi-track 360 file into
// one folder per lens, AI masking, and the resume rules that let an
// interrupted run reuse what it left behind. Both dataset paths -- the
// built-in SfM and the external COLMAP -- run it first, so a change to how
// frames are chosen cannot apply to only one of them.
//
// Each stage has a built-in implementation and an external fallback:
//
//   frames   in-process VK_KHR_video_decode_* (SS_ENABLE_PATENTED)
//            -> ffmpeg subprocess
//   masks    in-process SAM 2 / SAM 3          (SS_BUILD_SAM)
//            -> python + reference/scripts/mask.py
//
// The built-in path is the default when it is compiled in and the device
// supports it; the fallback is picked automatically otherwise, and can be
// forced (a codec the driver cannot decode, an HDR transfer ffmpeg handles
// better). `Backends` reports what this build and this machine can actually
// do, so the GUI can say so instead of failing at run time.

#include "app/FrameLook.h"
#include "app/FrameMask.h"
#include "app/Pano360.h"
#include "app/gui/FilmReel.h"
#include "app/gui/PrepProgress.h"
#include "app/gui/ReconStamp.h"
#include "data/DatasetColor.h"
#include "sfm/core/Telemetry.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gui {

// Reads the picture profile a clip was shot in; null means sfm::video_color.
using VideoColorRead = sfm::VideoColor (*)(const std::string& path);

// A click made in the mask preview, kept because it is a prompt for the whole
// run and not just for the frame it was drawn on.
//
// Objects are separate on purpose: SAM segments ONE thing per prompt, and a
// single instance given a click on the dog and a click on the bicycle returns
// a mask that fits neither. Each object is tracked on its own and the masks
// are unioned at the end.
//
// The frame is recorded twice because the two masking paths number frames
// differently. `frame` is exact where it can be -- the decoded index for the
// path that reads the video itself, the position in the sorted list for a
// folder of photos -- while `position`, the same point as a fraction of the
// capture, is what the ffmpeg path has to fall back on, since it resamples the
// video to a frame rate the preview never saw.
struct MaskClick {
    float x = 0.0f, y = 0.0f;   // pixels of the frame as the run writes it
    bool  positive = true;      // "this is it" vs "not this"
    int   object = 0;
    long long frame = 0;
    float position = 0.0f;      // 0..1 through the capture
    // PrepInput::path of the input it was drawn on; empty is the only one. The
    // same coordinates on another capture point at something else, so a click
    // never crosses inputs.
    std::string source;
    // And which camera folder under it (app::frame_folders), for the same
    // reason: a click on cam0 says nothing about where cam1 was pointing.
    std::string camera;
};

// One camera folder found INSIDE an input: a capture handed over already split
// into cam/, cam0/, cam1/ is several cameras in one folder, and one of them
// being a fisheye does not make the others one.
struct SubCamera {
    std::string rel;                 // "cam0", relative to the input's images
    std::string camera_model;        // empty = the input's own
    float focal_factor = 0.0f;       // 0 = no focal prior
    int rig = 0;                     // see PrepInput::rig
    bool rig_dual_fisheye = false;   // see PrepInput::rig_dual_fisheye
};

// A row's rig choice: nothing, the lenses of its own input, or one of the
// shared letters that join rows across inputs (SfmRunner::build_rigs).
inline constexpr int kRigNone = 0, kRigOwn = 1, kRigFirstShared = 2;
// How many letters the rig picker may offer; it shows one per two rows.
inline constexpr int kRigShared = 32;

// A, B, ... Z, AA, AB, ...
inline std::string rig_letter(int letter) {
    std::string s;
    for (int n = letter + 1; n > 0; n = (n - 1) / 26)
        s.insert(s.begin(), (char)('A' + (n - 1) % 26));
    return s;
}

// PrepInput::fps for "every frame" -- a 0 there already means "^". Everywhere
// else, a rate of 0 is every frame.
inline constexpr float kFpsEveryFrame = -1.0f;

// One thing the user picked: a video file, or a folder of photos. A job holds
// a list of them, because a capture is often shot as several clips, or on a rig
// whose lenses each write their own file -- and those only reconstruct together
// if they end up in one image tree.
struct PrepInput {
    std::string path;                // video file, or a folder of photos
    bool is_video = false;
    // Where this input's images land, relative to the dataset's images/. Empty
    // means images/ itself, which is what a lone input gets so a one-video
    // dataset keeps the layout it has always had. A multi-track video (an
    // Insta360 .insv carries two fisheye streams) adds cam0/, cam1/ ... below.
    std::string subdir;
    // Masks that came WITH this input -- the `masks/` beside its photos, whose
    // tree mirrors the image tree (see resolve_photo_folder). Empty when it
    // brought none. An input that has its own masks is never AI-masked: the
    // files on disk are the answer, and generating over them would mean
    // writing into a folder the user only asked us to read.
    std::string mask_dir;
    // The lens these images were shot with, and where its focal length starts.
    // Preparation reads neither -- they are here because this list is the one
    // place the inputs are enumerated, and the reconstruction has to be told
    // which folder each setting describes (SfmRunner turns them into
    // `--camera-model DIR=MODEL` / `--focal DIR=PX`). An empty model means the
    // job's dataset-wide one; a factor of 0 means no focal prior.
    std::string camera_model;
    float focal_factor = 0.0f;       // fx = fy = factor * image width
    // Which rig this input's lenses belong to (kRig*). A multi-lens video
    // starts on its own; folders sharing a letter form one rig by file name.
    int rig = kRigNone;
    // That rig's two folders are the back-to-back lenses of one dual-fisheye
    // camera, so the reconstruction starts from their known relative rotation.
    // Any row of the rig saying so is enough. A video's file says it itself.
    bool rig_dual_fisheye = false;
    // Kept frames per second for THIS video; 0 takes the job's. A capture shot
    // as several clips is rarely shot at one pace, and a clip walked through
    // slowly wants fewer frames than the one that ran past the same wall.
    float fps = 0.0f;
    // The photos were taken one after another and named in that order, so the
    // reconstruction may trust neighbouring files first (a video's frames
    // always are; SfmRunner::build_sequences).
    bool sequential = false;
    int video_tracks = 0;            // 0 = not probed yet
    // The camera folders found under this input, when it arrived with more
    // than one. Empty means the lens above describes all of it.
    std::vector<SubCamera> subcameras;
    // The 360 packing this file was found to carry, when it carries one: two
    // EAC tracks that the job's `pano` plan turns into ordinary views. Detected
    // rather than asked for, so a capture that is not one cannot be warped.
    app::Pano360Layout pano360;
    // Set instead when the file says it IS a 360 packing this build cannot
    // place, for the line that says its tracks are being left as they are.
    bool pano360_unsupported = false;
    // Fisheye circles side by side in each frame of a .lrv, or each .insp of
    // a folder (app::packed_lens_count); two are cut apart into cam0/, cam1/.
    // 0 = not such an input, or not measured yet.
    int packed_lenses = 0;
    // Areas of the frame that are never scene -- the fisheye border, a
    // watermark, the rig in shot. Per input because it describes a lens, and
    // resolved per camera folder when it asks for the border to be fitted
    // (app::FrameStencil), which is what gives a dual-fisheye file two circles.
    app::FrameStencil stencil;
};

// One row of the "Camera / lens per input" list. `rel` is the prefix
// `--camera-model PREFIX=MODEL` matches on, so the panel's rows and the
// reconstruction's overrides are one list (SfmRunner::append_camera_overrides).
struct CameraGroup {
    size_t input = 0;   // index into the job's inputs
    int sub = -1;       // index into that input's subcameras; -1 = the input
    std::string rel;    // under images/; "" is the whole capture
};

// The rows, in the order they are drawn and applied.
std::vector<CameraGroup> camera_groups(const std::vector<PrepInput>& inputs);

inline int& group_rig(std::vector<PrepInput>& in, const CameraGroup& g) {
    return g.sub < 0 ? in[g.input].rig : in[g.input].subcameras[(size_t)g.sub].rig;
}
inline int group_rig(const std::vector<PrepInput>& in, const CameraGroup& g) {
    return g.sub < 0 ? in[g.input].rig : in[g.input].subcameras[(size_t)g.sub].rig;
}
inline bool& group_rig_dual_fisheye(std::vector<PrepInput>& in, const CameraGroup& g) {
    return g.sub < 0 ? in[g.input].rig_dual_fisheye
                     : in[g.input].subcameras[(size_t)g.sub].rig_dual_fisheye;
}

// Where a row's settings are stored.
inline std::string& group_model(std::vector<PrepInput>& in, const CameraGroup& g) {
    return g.sub < 0 ? in[g.input].camera_model
                     : in[g.input].subcameras[(size_t)g.sub].camera_model;
}
inline const std::string& group_model(const std::vector<PrepInput>& in,
                                      const CameraGroup& g) {
    return g.sub < 0 ? in[g.input].camera_model
                     : in[g.input].subcameras[(size_t)g.sub].camera_model;
}
inline float& group_focal(std::vector<PrepInput>& in, const CameraGroup& g) {
    return g.sub < 0 ? in[g.input].focal_factor
                     : in[g.input].subcameras[(size_t)g.sub].focal_factor;
}
inline float group_focal(const std::vector<PrepInput>& in, const CameraGroup& g) {
    const float f = g.sub < 0 ? in[g.input].focal_factor
                              : in[g.input].subcameras[(size_t)g.sub].focal_factor;
    return f > 0 ? f : in[g.input].focal_factor;
}

// The model each row is actually fitted with. An EMPTY model means "the same
// as the row above" -- a dozen clips off one camera are one decision -- and the
// first row, having none above it, falls back to `fallback`.
std::vector<std::string> camera_group_models(const std::vector<PrepInput>& inputs,
                                             const std::vector<CameraGroup>& groups,
                                             const std::string& fallback);

// What a folder of photos does on its way into the dataset. Only `InPlace`
// leaves it pointing at a folder outside itself, and such a dataset opens
// again only if `image_dir` is set by hand -- which is why it is not default.
enum class PhotoImport {
    ConvertJpeg,   // into images/, re-encoded as JPEG where that loses nothing
    Copy,          // into images/, unchanged
    Move,          // into images/, leaving nothing behind
    InPlace,       // read where they are; only a lone folder can
};
inline constexpr int kNumPhotoImports = 4;

// Quality of the re-encode. High enough that the artefacts are below what the
// photometric loss can tell from sensor noise.
inline constexpr int kPhotoJpegQuality = 95;

struct PrepJob {
    std::vector<PrepInput> inputs;   // in the order the user added them
    std::string workspace;           // output dataset dir (created)
    bool resume = true;              // reuse what a previous run completed
    // Steps a re-run is redoing rather than reusing: their output is thrown
    // away even though `resume` would have kept it. Everything before them is
    // still reused, which is the whole point -- a bad mask prompt should not
    // cost the extraction as well.
    bool redo_frames = false;
    bool redo_masks = false;
    // The masks that came with the photos mark what to REMOVE, not what to
    // keep. Applied where those files are read, so everything this run writes
    // is in the one convention every reader uses (sfm/core/Mask.h).
    bool flip_found_masks = false;
    // How a photo input reaches images/. Videos ignore it -- their frames are
    // written into the dataset whatever this says.
    PhotoImport photo_import = PhotoImport::ConvertJpeg;

    // The device request for built-in decoding and masking: "auto", an ordinal,
    // a name substring or "uuid:<32 hex>". Frozen at the top of run(); a bad
    // value fails the run. External Python masking does not read this.
    std::string device;

    // ---- video extraction ----
    // What a 360 capture (PrepInput::pano360) becomes. Dataset-wide: mixing
    // panoramas and pinhole faces in one image tree describes no camera rig.
    app::Pano360Options pano;

    // Kept frames per second, 0 = every frame; PrepInput::fps overrides it
    // per video.
    float video_fps = 2.0f;
    // Space them by view change rather than by time (app/FrameMotion.h): the
    // rate above becomes the average and stays within `adaptive_range` of it.
    bool  adaptive_fps = false;
    float adaptive_range = 4.0f;
    int   sharp_window = 3;          // keep the sharpest of N (1 = off)
    // Every track of a multi-lens file keeps the same instants (one sharpness
    // window over all of them), so every frame is a rig frame. Built-in decoder only.
    bool  sync_tracks = true;
    int   max_frames = 100000;
    // Turn every extracted frame by the rotation the capture asks for, so a
    // portrait clip lands upright and the written files need no metadata read
    // to be shown the right way up.
    bool  auto_rotate = true;
    bool  force_external_decode = false;
    std::string ffmpeg_exe = "ffmpeg";
    // Bits per channel of a video's frames: 8 (JPEG), 16 (PNG, through ffmpeg
    // whatever the decoder setting) or 0, 16 for a D-Log M clip and 8 otherwise.
    int   frame_bits = 0;
    VideoColorRead read_color = nullptr;
    // Bytes free where the workspace is; null asks the filesystem.
    std::uintmax_t (*free_space)(const std::string& dir) = nullptr;

    // The photographs' colour space. Frames convert to sRGB before the
    // segmenter sees them, which is what it was trained on.
    std::string image_gamut;
    std::optional<bool> image_is_linear;

    // ---- masking ----
    bool mask_enable = false;
    std::string mask_prompt;         // "people; cars; ..."
    std::string mask_negative_prompt;
    bool mask_keep_subject = false;  // prompt names what to KEEP, not remove
    // Share of its own size every matched object's boundary moves by before
    // the mask is written, so the PNGs on disk carry it. SIGNED as
    // sam::MaskOptions::dilate_ratio is: negative trims inward.
    float mask_dilate_ratio = 0.05f;
    int  mask_max_image_size = 1600;
    float mask_threshold = 0.5f;     // detection score a match must reach
    // Box IoU above which the weaker of two detections of one phrase is
    // dropped. Low values thin out a crowd: two people who overlap by a fifth
    // of their boxes are one detection at 0.1.
    float mask_nms = 0.1f;
    // Follow the prompted objects through a video with the model's memory bank
    // rather than segmenting each frame alone: one extra model pass per live
    // instance per frame. Clicks turn it on whatever this says.
    bool mask_memory = false;
    // Both only bite with the bank on. The detector runs every Nth frame, the
    // bank carrying the instances in between; and each instance keeps at most
    // this many spatial memory frames, below the checkpoint's own (7) -- memory
    // attention is linear in it and is most of a tracked frame. 0 = the model's.
    int  mask_detect_every = 1;
    int  mask_memory_frames = 0;
    // Clicked objects, each tagged with its input (MaskClick::source). The only
    // way to prompt a SAM 2 checkpoint. Without a text prompt every input needs
    // its own, and run() refuses the job rather than half-mask the capture.
    std::vector<MaskClick> mask_clicks;

    // Built-in: a checkpoint file (ModelCache resolves it). External: the
    // model name reference/scripts/mask.py understands.
    std::string mask_model_path;
    std::string mask_model_name = "sam2.1_hiera_large";
    bool  force_external_masking = false;
    std::string python_exe = "python3";
};

// The rate a row is actually extracted at (0 = every frame). A row's 0 means
// "the same as the row above", as the lens column spells a decision made once
// for a run of clips; the first row falls back to the dataset's own rate.
inline float input_fps(const std::vector<PrepInput>& inputs, float dataset_fps,
                       size_t at) {
    for (size_t k = std::min(at, inputs.size() - (inputs.empty() ? 0 : 1)) + 1;
         k-- > 0;)
        if (k < inputs.size() && inputs[k].fps != 0.0f)
            return std::max(inputs[k].fps, 0.0f);
    return std::max(dataset_fps, 0.0f);
}

// Rows extracted at one rate, named by the one that states it -- so an adaptive
// plan can spend one budget over all of them and give the clip that moves more
// the denser frames.
inline size_t fps_group(const std::vector<PrepInput>& inputs, size_t at) {
    size_t g = 0;
    for (size_t k = 1; k <= at && k < inputs.size(); k++)
        if (inputs[k].fps != 0.0f) g = k;
    return g;
}

// The row `in` is. Called with an input of `job`, so pointer identity answers.
inline size_t input_index(const PrepJob& job, const PrepInput& in) {
    for (size_t i = 0; i < job.inputs.size(); i++)
        if (&job.inputs[i] == &in) return i;
    return 0;
}

inline float input_fps(const PrepJob& job, const PrepInput& in) {
    return input_fps(job.inputs, job.video_fps, input_index(job, in));
}

// No selection happens at all: no sharpness window, no motion plan.
inline bool every_frame(const PrepJob& job, const PrepInput& in) {
    return in.is_video && !(input_fps(job, in) > 0.0f);
}

// Nothing left for adaptive spacing to decide, which the panel warns about.
inline bool all_videos_every_frame(const std::vector<PrepInput>& inputs,
                                   float dataset_fps) {
    bool any = false;
    for (size_t i = 0; i < inputs.size(); i++) {
        if (!inputs[i].is_video) continue;
        if (input_fps(inputs, dataset_fps, i) > 0.0f) return false;
        any = true;
    }
    return any;
}

// Images read where they are instead of gathered into the dataset's own
// images/ (see DatasetPrep::run). Several inputs reconstruct from ONE image
// tree, so there is nowhere for a second one to be read in place from.
inline bool reads_photos_in_place(const std::vector<PrepInput>& inputs,
                                  PhotoImport mode) {
    return mode == PhotoImport::InPlace && inputs.size() == 1 &&
           !inputs[0].is_video && inputs[0].packed_lenses == 0;
}

// Where a job's images will be, before it has run: what PrepResult::image_dir
// comes out as, for the panels that must read a dataset a previous run wrote.
std::string planned_image_dir(const std::vector<PrepInput>& inputs,
                              const std::string& workspace, PhotoImport mode);

// A video the run extracted frames from, for the manifest's `captures`:
// the stems carry the source frame index (fps 0, the file's own rate) or,
// after the ffmpeg fallback, the kept-frame count at `fps`.
struct PrepCapture {
    std::string subdir;
    std::string path;
    double fps = 0;
    // Every track kept the same instants, so the frames of one stem are a rig.
    bool lockstep = false;
};

struct PrepResult {
    std::vector<PrepCapture> captures;
    std::string image_dir;           // absolute; what SfM should index
    std::string image_dir_cfg;       // what the trainer's image_dir should be
    std::string mask_dir;            // "" when there are no masks
    // ... and what the trainer's mask_dir should be: "masks" for masks the run
    // put in the dataset, an absolute path for masks it only read (photos used
    // where they are bring theirs with them).
    std::string mask_dir_cfg;
    // Those masks are still the other way round -- nonzero means REMOVE. True
    // only where the run handed them on untouched; what it wrote itself is in
    // the usual convention and the readers need no flag.
    bool mask_dir_flipped = false;
    int  n_images = 0;
    // images/ came out holding one sub-folder per camera -- several inputs, or
    // a multi-track video -- so intrinsics must not be shared across them.
    bool per_folder_cameras = false;
    // The frames were extracted again over ones already there, so anything a
    // reconstruction left describes pictures that are no longer in images/.
    bool frames_rebuilt = false;
};

// PrepJob::frame_bits for one input whose clip reads as `mode`. 16 only where
// extract_video_ffmpeg has a 16-bit path: not photos, 360 packings or packed lenses.
inline int frame_bits_for(const PrepJob& job, const PrepInput& in,
                          sfm::VideoColorMode mode) {
    if (!in.is_video || in.packed_lenses >= 2 ||
        (in.pano360.valid() && job.pano.mode != app::Pano360Mode::Off))
        return 8;
    if (job.frame_bits == 16) return 16;
    return job.frame_bits == 0 && mode == sfm::VideoColorMode::DlogM ? 16 : 8;
}

// A 16-bit extraction needing `need` bytes where `free` are: past 80% of it the
// run warns, past all of it the run refuses before writing a frame.
enum class DiskVerdict { Fits, Tight, TooBig };
inline DiskVerdict disk_verdict(double need, double free) {
    if (need > free) return DiskVerdict::TooBig;
    return need > 0.8 * free ? DiskVerdict::Tight : DiskVerdict::Fits;
}

// The same, reading the clip: `read`, else job.read_color, else sfm::video_color.
int resolved_frame_bits(const PrepJob& job, const PrepInput& in,
                        VideoColorRead read = nullptr);

// Everything that decides which pictures land in images/ (`bits`: resolved per
// input, empty reads no clip), and nothing that decides what becomes of them --
// folding masking or the reconstruction in would re-extract over a prompt.
inline ReconStamp frames_stamp(const PrepJob& job, const std::vector<int>& bits = {}) {
    auto num = [](double v) {
        char b[32];
        std::snprintf(b, sizeof b, "%g", v);
        return std::string(b);
    };
    ReconStamp st;
    st.present = true;
    st.engine = job.force_external_decode ? "ffmpeg" : "builtin";
    st.args = {"--fps",         num(job.video_fps),
               "--adaptive",    job.adaptive_fps ? "1" : "0",
               "--range",       num(job.adaptive_range),
               "--sharp",       num(job.sharp_window),
               "--sync",        job.sync_tracks ? "1" : "0",
               "--max-frames",  num(job.max_frames),
               "--rotate",      job.auto_rotate ? "1" : "0",
               "--photos",      num((int)job.photo_import),
               "--360",         num((int)job.pano.mode),
               "--360-size",    num(job.pano.size),
               "--360-orient",  num(job.pano.yaw) + "," + num(job.pano.pitch) +
                                    "," + num(job.pano.roll)};
    for (size_t i = 0; i < job.inputs.size(); i++) {
        const PrepInput& in = job.inputs[i];
        st.args.push_back("--input");
        st.args.push_back(in.path);
        st.args.push_back(in.subdir);
        st.args.push_back(num(in.fps));
        st.args.push_back("--bits");
        st.args.push_back(num(i < bits.size()
                                  ? bits[i]
                                  : frame_bits_for(job, in, sfm::VideoColorMode::NotRecorded)));
    }
    return st;
}

// What this build, on this machine, can do without an external tool.
//
// Two strings per stage on purpose: `*_reason` names the option or the missing
// device feature and belongs in a log or a tooltip; `*_note` is the sentence
// shown on the screen, which should tell a user what will happen rather than
// which CMake flag was off when the binary was made.
struct Backends {
    // Build-level answers only. The runtime answers (can THIS device decode?)
    // are not here: probing them creates the inference context, which must wait
    // until the device is frozen.
    bool builtin_video = false;
    std::string video_reason;
    std::string video_note;
    bool builtin_masking = false;
    std::string masking_reason;
    std::string masking_note;
};
// What this binary was built with. Creates no device, so it is safe on the UI
// thread and before a GPU choice exists.
const Backends& backends();

// Video container extensions the GUI offers, in the file dialog and for
// drag-and-drop. Sized here so a range-for over it works from another TU.
inline constexpr int kNumVideoExtensions = 14;
extern const char* const kVideoExtensions[kNumVideoExtensions];

// Does this path name one of them? (Extension only; the file need not exist.)
bool is_video_path(const std::string& path);
// A dual-fisheye Insta360 file: two video tracks, one per lens, and a lens the
// default camera model does not fit.
bool is_dual_fisheye_path(const std::string& path);
// A GoPro MAX .360 by its name. The packing itself is what probe_pano360
// confirms; this only decides whether it is worth asking.
bool is_pano360_path(const std::string& path);
// An Insta360 .insp photo or .lrv proxy: one or two fisheye circles packed
// into each frame, which PrepInput::packed_lenses counts.
bool is_packed_lens_path(const std::string& path);

// Two fisheye lenses back to back, as tracks or side by side.
inline bool is_dual_lens(const PrepInput& in) {
    return is_dual_fisheye_path(in.path) || in.packed_lenses >= 2;
}
// A lens the default camera model does not fit.
inline bool has_fisheye_lens(const PrepInput& in) {
    return is_dual_fisheye_path(in.path) || is_packed_lens_path(in.path) ||
           in.packed_lenses > 0;
}
// The lenses of the first .insp under `dir`, or 0 when it holds none.
int probe_packed_lenses(const std::string& dir);

// ---- the ffmpeg fallback, for callers that are not a preparation run -------
//
// Everything the GUI does to a video without the built-in decoder goes through
// an external ffmpeg, and the mask preview needs the same two answers a
// preparation run does -- how long the capture is, and what one frame of it
// looks like -- without running one. Only `ffmpeg_exe` is needed (ffprobe is
// not assumed to be installed beside it): `ffmpeg -i` prints the stream table
// on its way to complaining that no output file was named.

// What an external ffmpeg says about a video. A zero means it did not say.
struct VideoFacts {
    double duration = 0.0;    // seconds
    double fps = 0.0;
    long long frames = 0;     // duration * fps; the container's own count is
                              // not printed by `ffmpeg -i`
    int width = 0, height = 0;   // one frame, before any scaling
    // One entry per video stream, in the order ffmpeg lists them, which is the
    // order `[0:v:N]` and the built-in demuxer both number them by.
    std::vector<std::pair<int, int>> tracks;
};
bool ffmpeg_probe_video(const std::string& ffmpeg_exe, const std::string& path,
                        VideoFacts& out, const std::atomic<bool>& cancel);

// How an ffmpeg whose `-version` first line this is writes every frame exactly
// once: -fps_mode from 5.1, -vsync before (9.0 rejects it). Unparsed = new.
std::vector<std::string> passthrough_args_for(const std::string& version_line);
// ... for the ffmpeg at `ffmpeg_exe`, asked once per path.
std::vector<std::string> passthrough_args(const std::string& ffmpeg_exe);

// What one still has to reproduce of the run's own ffmpeg invocation.
struct FfmpegStillOpts {
    int  track = 0;
    // ffmpeg turns the picture by the container's matrix unless told not to,
    // which is what the built-in decoder's auto_rotate matches.
    bool auto_rotate = true;
    // A 360 capture: both tracks are decoded and the overlap strips cut out,
    // so what lands in `out_path` is the EAC canvas (app::pano360_graph).
    app::Pano360Layout eac;
};

// One frame, `seconds` into the file, written to `out_path` as a JPEG.
// False when ffmpeg is missing, was cancelled, or wrote nothing.
bool ffmpeg_extract_frame(const std::string& ffmpeg_exe, const std::string& video,
                          double seconds, const std::string& out_path,
                          const std::atomic<bool>& cancel,
                          const FfmpegStillOpts& opts = {});

// The 360 packing a video carries, asked of the built-in demuxer where there is
// one and of ffmpeg otherwise, so the answer does not depend on the decode path
// the run will take. `unsupported`: app::pano360_unsupported.
struct Pano360Probe {
    app::Pano360Layout layout;
    bool unsupported = false;
};
Pano360Probe probe_pano360(const std::string& ffmpeg_exe,
                           const std::string& path,
                           const std::atomic<bool>& cancel);

// How many video tracks a file carries (0 when it cannot be read).
int probe_video_tracks(const std::string& ffmpeg_exe, const std::string& path,
                       const std::atomic<bool>& cancel);

// The folders under images/<input> a video's frames go to: one per lens of a
// dual-fisheye file or per view of a 360 plan, none for a single lens.
std::vector<std::string> lens_dirs(const PrepJob& job, const PrepInput& in);

// What a picked folder of photos actually means, by the layout conventions the
// rest of the project already uses -- `spirula sfm auto`'s own probing and the
// dataparsers' `mask_dir = "masks"`:
//
//   <picked>/images + <picked>/masks   a dataset folder: index images/, and the
//                                      masks beside it are already made
//   <picked> + <picked>/masks          photos at the top with their masks under
//                                      them (the masks folder is NOT indexed)
//   <picked> + <picked>/../masks       ... or the images folder was picked
//                                      directly and its sibling holds the masks
//
// `images` comes back as the folder to index and `masks` as the mask tree, or
// empty when there is none. Either half may be a symlink into the raw capture,
// which is why every walk in here follows directory symlinks.
void resolve_photo_folder(const std::string& picked, std::string& images,
                          std::string& masks);

// Every folder under `dir` holding images DIRECTLY, '/'-separated, parents
// before children, "" being `dir` itself -- exactly the groups `--camera-mode
// folder` will make, grouping an image on its parent path (core/CameraSetup.h).
std::vector<std::string> camera_subfolders(const std::string& dir);

// Bounds on that walk: it runs on the UI thread and each entry becomes a panel
// row. Past them a folder still reconstructs, sharing the nearest listed
// folder's lens by the overrides' longest-prefix rule.
inline constexpr int kMaxCameraFolderDepth = 4;
inline constexpr size_t kMaxCameraFolders = 64;

// Does this folder hold any image at all, at any depth? Follows directory
// symlinks (a prepared capture's images/ is often a link into the raw one) and
// stops at the first hit, so it is cheap enough for the UI thread.
bool folder_has_images(const std::string& dir);

// The photo extensions an input folder is indexed for.
bool is_image_file(const std::filesystem::path& p);

// Is this folder an already-reconstructed dataset -- something the trainer's
// dataparsers can read -- rather than raw input? True for a Nerfstudio
// transforms.json, a COLMAP sparse/ or colmap/, or a Metashape camera .xml
// next to a point-cloud .ply, which is what the parsers themselves probe for.
//
// Cheap and deliberately shallow: it decides where a dropped folder is routed
// and whether a batch row is worth starting, not whether the parse will
// succeed. It says nothing about raw photos -- a folder of JPEGs is not a
// dataset until something has reconstructed it.
bool folder_looks_like_dataset(const std::string& dir);

// What the output folder already holds, so the screen can say what a run would
// reuse and what it would replace.
//
// `inputs` is part of the question: the natural output folder for a capture
// whose images/ is already on disk is that folder itself, and its own images
// and masks are the input -- not leftovers from a previous run.
struct WorkspaceState {
    bool frames = false;    // images/ this run would extract into
    bool features = false;  // features/, matches.bin, database.db -- reusable
    bool masks = false;     // masks/ this run would generate into
    bool input_masks = false;  // masks an input came with (PrepInput::mask_dir)
    // A reconstruction any dataset reader can open: this run's own sparse/, or
    // the transforms.json, root-level COLMAP files or Metashape export of a
    // dataset that arrived finished. A run pointed at one ADDS to it.
    bool model = false;
    bool geometry = false;  // normals/ or depths/, which a run adds to
    // Were the flags that built that model written down beside it
    // (ReconStamp.h)? Without them a run cannot tell whether reusing it still
    // answers what the panel is asking for, and reuses it regardless.
    bool recon_stamp = false;
    // Something a resumed run can pick up instead of redoing.
    bool resumable() const { return frames || features || masks; }
};
WorkspaceState probe_workspace(const std::string& workspace,
                               const std::vector<PrepInput>& inputs);

// Everything a run WROTE into the output folder, absolute, existing ones only:
// what "clear this project" deletes. Never an input -- the images and masks the
// user picked are not leftovers, which is probe_workspace's rule reused.
std::vector<std::string> workspace_artifacts(const std::string& workspace,
                                             const std::vector<PrepInput>& inputs);

// The picture profile each input was shot in, read off its DJI metadata by
// `read` (null: sfm::video_color), for the dataset's colour record. `notes` gets
// a line per input whose metadata could not be read.
spirula::DatasetColor clip_colors(const std::vector<PrepInput>& inputs,
                                  std::vector<std::string>* notes,
                                  VideoColorRead read = nullptr);

// Is this the mask half of one of those layouts, rather than an input of its
// own? By name, which is what makes it a convention: `--mask-dir masks` is the
// SfM default and `mask_dir = "masks"` the dataparsers'.
bool is_mask_folder(const std::string& path);

// The correction editor's layer folder (app/gui/mask/MaskLayer.h), which a
// finished dataset carries beside images/ and masks/ and which holds PNGs.
bool is_mask_edits_folder(const std::string& path);

// One counter for a whole step, rather than one per input: a job with three
// videos in it should fill the bar once and never wind it back, which is the
// only thing a user watching a long extraction is reading it for.
//
// `total` is a sum of estimates -- what a container says it holds, which is
// what the step can know before it runs -- and each input replaces its own
// share of it with what it actually produced as it finishes.
struct StageTally {
    int64_t done = 0;      // items produced so far, over every input
    int64_t total = 0;     // over every input, an estimate until they are done
    int64_t started = 0;   // `done` when the input now running began

    void plan(int64_t estimate) { total += estimate; }
    void settle(int64_t produced, int64_t estimated) {
        done = std::max(done, started + produced);
        total += produced - estimated;
        clamp();
        started = done;
    }
    // A planned pass that turned out not to be needed.
    void drop(int64_t estimated) {
        total -= estimated;
        clamp();
    }
    void clamp() { total = std::max(total, done); }
};

class DatasetPrep {
public:
    DatasetPrep(RunProgress* progress, RunFilms films,
                const std::atomic<bool>& cancel)
        : _prog(progress), _films(films), _cancel(cancel) {}

    // Called once, immediately before masking starts, and free to replace the
    // job's mask_* fields with whatever the screen says by then -- which is
    // what lets the masking options stay editable while frames are extracted.
    // Anything else it touches has already been acted on.
    using RefreshFn = std::function<void(PrepJob&)>;

    // False with `error` set on failure ("cancelled" when the token was set).
    bool run(const PrepJob& job, PrepResult& out, std::string& error,
             const RefreshFn& refresh_masks = {});

    // Recursive, matching what COLMAP's feature_extractor indexes. `skip` is a
    // sub-folder not to descend into: a masks/ nested under the images is full
    // of PNGs that are not views, and counting them doubles the dataset with
    // garbage (the guard `spirula sfm auto` has for the same layout).
    static int count_images(const std::string& dir, const std::string& skip = "");
    // Dimensions of the first image found, for the focal-length prior.
    static bool first_image_dims(const std::string& dir, int& w, int& h);
    // Every image under `dir`, named relative to it, with its pixel size --
    // zero when nothing here reads that format's header (stb has no TIFF or
    // WebP decoder; COLMAP's FreeImage does).
    struct ImageSize { std::string name; int w = 0, h = 0; };
    static std::vector<ImageSize> image_sizes(const std::string& dir,
                                              const std::string& skip = "");

private:
    void log(const std::string& s, bool detail = true);
    void enter(Stage s, const std::string& text);

    // One input's frames. `images` / `masks` are that input's own folders
    // (images/<subdir>, masks/<subdir>); `masked` comes back true only when a
    // resumed run found masks already sitting beside them.
    bool extract_video(const PrepJob& job, const PrepInput& in,
                       const std::string& images, const std::string& masks,
                       PrepResult& out, bool& masked, std::string& error);
    bool extract_video_builtin(const PrepJob& job, const PrepInput& in,
                               const std::string& images,
                               PrepResult& out, std::string& error);
    bool extract_video_ffmpeg(const PrepJob& job, const PrepInput& in,
                              const std::string& images, PrepResult& out,
                              std::string& error);
    // Its 16-bit half: the sharpest of each group chosen on track 0's JPEG
    // candidates, then every track decoded again for those alone as PNG16.
    bool extract_video_ffmpeg16(const PrepJob& job, const PrepInput& in,
                                const std::string& images, size_t streams,
                                bool fisheye, int width, int height,
                                long long frames, std::string& error);
    // resolved_frame_bits for an input of the running job, read once in run().
    int bits_of(const PrepJob& job, const PrepInput& in) const;
    // The built-in decoder reads this input: allowed, working, and 8-bit.
    bool builtin_decode(const PrepJob& job, const PrepInput& in) const;
    // A 360 capture through ffmpeg: one decode writing the EAC canvas, frame
    // selection over those, then our own resampler into the views. ffmpeg is
    // never asked to warp -- see app/Pano360.h.
    bool extract_360_ffmpeg(const PrepJob& job, const PrepInput& in,
                            const std::string& images, PrepResult& out,
                            std::string& error);
    // A packed video's frames, written whole into `images`, cut into one
    // folder per lens. Each frame goes once its lenses are written, so a
    // resumed run finishes an interrupted pass.
    bool split_packed_frames(const PrepInput& in, const std::string& images,
                             PrepResult& out, std::string& error);
    // Photos into the dataset's own images/<subdir>, by whichever of
    // PhotoImport the job asked for -- and the masks they came with into the
    // matching masks/<subdir>, so the two trees still mirror each other.
    bool gather_photos(const PrepJob& job, const PrepInput& in,
                       const std::string& images, const std::string& masks,
                       bool& have_masks, std::string& error);
    // Masks for ONE input's images. Run per input rather than over the whole
    // tree so the tracker's memory bank never crosses from one capture into the
    // next, and so clicks reach only the input they were drawn on.
    //
    // `folded` comes back true when the input's stencil was intersected into
    // the masks as they were produced, which is what lets apply_stencil be
    // skipped -- it would otherwise decode and re-encode every mask again.
    bool generate_masks(const PrepJob& job, const PrepInput& in,
                        const std::string& images, const std::string& images_rel,
                        const std::string& masks, const std::string& masks_rel,
                        bool& folded, std::string& error);
    bool generate_masks_builtin(const PrepJob& job, const PrepInput& in,
                                const std::string& images, const std::string& masks,
                                bool& folded, std::string& error);
    bool generate_masks_python(const PrepJob& job, const std::string& images_rel,
                               const std::string& masks_rel, std::string& error);
    // The static stencil on its own, for the masks segmentation did not make.
    // `merge_from` names the masks it folds in when they are not the ones it
    // writes -- the tree the photos arrived with; "" is `masks` itself.
    bool apply_stencil(const PrepJob& job, const PrepInput& in,
                       const std::string& images, const std::string& masks,
                       const std::string& merge_from, std::string& error);
    int exec(const std::vector<std::string>& argv,
             const std::function<void(const std::string&)>& on_line = {});
    // What this input is expected to put in images/, for the step's bar: the
    // container's frame count at the run's sampling rate for a video, the file
    // count for photos, and what is already there for an input a resumed run
    // is going to keep.
    int64_t estimate_frames(const PrepJob& job, const PrepInput& in,
                            const std::string& images);
    // Measures every video on `at`'s rate and spaces them together, once per
    // group. A no-op unless the rate is adaptive and the built-in decoder is
    // the one reading the file.
    bool plan_group(const PrepJob& job, size_t at, std::string& error);

    // Whether the settings the frames on disk were extracted with still read
    // the same (ReconStamp.h). A run that changes how a video is unwrapped has
    // to go back to the video, and `resume` cannot see that by itself.
    bool frames_stale(const PrepJob& job) const {
        return job.redo_frames || _frames_stale;
    }

    RunProgress* _prog;
    RunFilms _films;
    const std::atomic<bool>& _cancel;
    bool _frames_stale = false;
    // The spacing chosen per input, and which of them have one: an adaptive
    // plan covers a whole rate group, so it is made before any of the group is
    // extracted rather than per video.
    std::vector<std::vector<int64_t>> _plans;
    std::vector<bool> _planned;
    std::vector<int> _bits;
    StageTally _frames_tally, _masks_tally;
};

}  // namespace gui
