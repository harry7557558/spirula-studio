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

#include "app/FrameMask.h"
#include "app/gui/FilmReel.h"
#include "app/gui/PrepProgress.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gui {

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
    float x = 0.0f, y = 0.0f;   // pixels of the source frame
    bool  positive = true;      // "this is it" vs "not this"
    int   object = 0;
    long long frame = 0;
    float position = 0.0f;      // 0..1 through the capture
    // PrepInput::path of the input it was drawn on; empty is the only one. The
    // same coordinates on another capture point at something else, so a click
    // never crosses inputs.
    std::string source;
};

// One camera folder found INSIDE an input: a capture handed over already split
// into cam/, cam0/, cam1/ is several cameras in one folder, and one of them
// being a fisheye does not make the others one.
struct SubCamera {
    std::string rel;                 // "cam0", relative to the input's images
    std::string camera_model;        // empty = the input's own
    float focal_factor = 0.0f;       // 0 = no focal prior
};

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
    // The camera folders found under this input, when it arrived with more
    // than one. Empty means the lens above describes all of it.
    std::vector<SubCamera> subcameras;
    // Areas of the frame that are never scene -- the fisheye border, a
    // watermark, the rig in shot. Per input because it describes a lens, and
    // resolved per camera folder when it asks for the border to be fitted
    // (app::FrameStencil), which is what gives a dual-fisheye file two circles.
    app::FrameStencil stencil;
};

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

    // ---- video extraction ----
    float video_fps = 2.0f;          // kept frames per second
    int   sharp_window = 3;          // keep the sharpest of N (1 = off)
    int   max_frames = 100000;
    bool  force_external_decode = false;
    std::string ffmpeg_exe = "ffmpeg";

    // The photographs' colour space. Frames convert to sRGB before the
    // segmenter sees them, which is what it was trained on.
    std::string image_gamut;
    std::optional<bool> image_is_linear;

    // ---- masking ----
    bool mask_enable = false;
    std::string mask_prompt;         // "people; cars; ..."
    std::string mask_negative_prompt;
    bool mask_keep_subject = false;  // prompt names what to KEEP, not remove
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

// The one case where images are read where they are instead of being gathered
// into the dataset's own images/ (see DatasetPrep::run): one folder of photos.
inline bool reads_photos_in_place(const std::vector<PrepInput>& inputs) {
    return inputs.size() == 1 && !inputs[0].is_video;
}

// Where a job's images will be, before it has run: what PrepResult::image_dir
// comes out as, for the panels that must read a dataset a previous run wrote.
std::string planned_image_dir(const std::vector<PrepInput>& inputs,
                              const std::string& workspace);

struct PrepResult {
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
};

// What this build, on this machine, can do without an external tool.
//
// Two strings per stage on purpose: `*_reason` names the option or the missing
// device feature and belongs in a log or a tooltip; `*_note` is the sentence
// shown on the screen, which should tell a user what will happen rather than
// which CMake flag was off when the binary was made.
struct Backends {
    bool builtin_video = false;
    std::string video_reason;
    std::string video_note;
    bool builtin_masking = false;
    std::string masking_reason;
    std::string masking_note;
};
// Probes the Vulkan device on first call for the video answer, so call it off
// the UI thread the first time if that matters.
const Backends& backends();

// Video container extensions the GUI offers, in the file dialog and for
// drag-and-drop. Sized here so a range-for over it works from another TU.
inline constexpr int kNumVideoExtensions = 13;
extern const char* const kVideoExtensions[kNumVideoExtensions];

// Does this path name one of them? (Extension only; the file need not exist.)
bool is_video_path(const std::string& path);
// A dual-fisheye Insta360 file: two video tracks, one per lens, and a lens the
// default camera model does not fit.
bool is_dual_fisheye_path(const std::string& path);

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
};
bool ffmpeg_probe_video(const std::string& ffmpeg_exe, const std::string& path,
                        VideoFacts& out, const std::atomic<bool>& cancel);

// One frame, `seconds` into the file, written to `out_path` as a JPEG.
// False when ffmpeg is missing, was cancelled, or wrote nothing.
bool ffmpeg_extract_frame(const std::string& ffmpeg_exe, const std::string& video,
                          double seconds, const std::string& out_path,
                          const std::atomic<bool>& cancel);

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

// The immediate sub-folders of `dir` that hold images, sorted -- the camera
// folders of a capture that was handed over already split. Empty when the
// images sit in `dir` itself, which is the ordinary case.
std::vector<std::string> camera_subfolders(const std::string& dir);

// Does this folder hold any image at all, at any depth? Follows directory
// symlinks (a prepared capture's images/ is often a link into the raw one) and
// stops at the first hit, so it is cheap enough for the UI thread.
bool folder_has_images(const std::string& dir);

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
    // A reconstruction any dataset reader can open: this run's own sparse/, or
    // the transforms.json / Metashape export of a dataset that arrived
    // finished. A run pointed at one ADDS to it rather than rebuilding it.
    bool model = false;
    bool geometry = false;  // normals/ or depths/, which a run adds to
    // Something a resumed run can pick up instead of redoing.
    bool resumable() const { return frames || features || masks; }
};
WorkspaceState probe_workspace(const std::string& workspace,
                               const std::vector<PrepInput>& inputs);

// Is this the mask half of one of those layouts, rather than an input of its
// own? By name, which is what makes it a convention: `--mask-dir masks` is the
// SfM default and `mask_dir = "masks"` the dataparsers'.
bool is_mask_folder(const std::string& path);

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
    // Photos into the dataset's own images/<subdir>, when they cannot simply be
    // read where they are (see run()) -- and the masks they came with into the
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
    int exec(const std::vector<std::string>& argv);
    // What this input is expected to put in images/, for the step's bar: the
    // container's frame count at the run's sampling rate for a video, the file
    // count for photos, and what is already there for an input a resumed run
    // is going to keep.
    int64_t estimate_frames(const PrepJob& job, const PrepInput& in,
                            const std::string& images);

    RunProgress* _prog;
    RunFilms _films;
    const std::atomic<bool>& _cancel;
    StageTally _frames_tally, _masks_tally;
};

}  // namespace gui
