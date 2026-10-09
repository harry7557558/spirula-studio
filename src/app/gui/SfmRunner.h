#pragma once
#include "app/gui/SfmPartition.h"

// SfmRunner -- turns raw images or a video into a trainable dataset using this
// repository's own structure-from-motion, with nothing else installed.
//
// Same public shape as ColmapRunner (start / cancel / state / stage / error /
// dataset_dir / image_dir / drain_log) plus a progress fraction, so GuiApp
// drives either one through the same code.
//
// Pipeline:
//   DatasetPrep  (frames, sharpest-frame selection, .insv track split, masks)
//   spirula sfm auto  ->  <workspace>/sparse/0/{cameras,images,points3D}.bin
//
// The reconstruction runs in this process (app/gui/SfmInProcess.h). `subprocess`
// runs it as a child of this same executable instead -- the escape hatch for a
// driver that resets under a long solve. Either way the screen reads the same
// typed status, from the event stream or from the snapshot the child writes.

#include "app/gui/DatasetPlan.h"
#include "app/gui/DatasetPrep.h"
#include "core/Env.h"
#include "app/gui/SfmProgress.h"
#ifdef SS_TOOL_SFM
// Reaches the Vulkan headers, which a build without the module may not have.
#include "sfm/core/Manifest.h"
#endif
#include "app/gui/FilmReel.h"
#include "app/gui/GeometryRunner.h"
#include "app/gui/LidarStep.h"
#include "app/gui/PrepProgress.h"
#include "i18n/catalog/Dataset.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace gui {

// The camera models the dataset parser and renderer can also consume,
// EQUIRECTANGULAR included (ColmapParser reads model 17, 2:1 aspect checked).
// A capture can mix them: the manifest gives one input its own -- build_manifest().
inline const char* kSfmCameraModels[] = {
    "opencv", "pinhole", "simple-pinhole", "radial",
    "full-opencv", "opencv-fisheye", "thin-prism-fisheye", "equirectangular",
};
inline constexpr int kNumSfmCameraModels = 8;

// What the combo boxes show for each of the above, in the same order.
inline std::vector<const spirula::i18n::Msg*> sfm_camera_model_labels() {
    namespace m = spirula::i18n::msg::dataset;
    return {&m::lens_opencv, &m::lens_pinhole, &m::lens_simple_pinhole,
            &m::lens_radial, &m::lens_full_opencv, &m::lens_fisheye_opencv,
            &m::lens_fisheye_thin_prism, &m::lens_equirectangular};
}

// The sentence beside each of them: which physical camera it is for. The
// choice is the one thing on the screen a user cannot check afterwards -- a
// dual-fisheye 360 clip fitted with the panorama model reconstructs into
// nothing -- so the picker carries a description per row, not one per combo.
inline std::vector<const spirula::i18n::Msg*> sfm_camera_model_helps() {
    namespace m = spirula::i18n::msg::dataset;
    return {&m::lens_opencv_help, &m::lens_pinhole_help,
            &m::lens_simple_pinhole_help, &m::lens_radial_help,
            &m::lens_full_opencv_help, &m::lens_fisheye_opencv_help,
            &m::lens_fisheye_thin_prism_help, &m::lens_equirectangular_help};
}

// Does this model describe a fisheye circle rather than a rectilinear frame?
inline bool sfm_model_is_fisheye(const std::string& m) {
    return m == "opencv-fisheye" || m == "thin-prism-fisheye";
}

// What `spirula sfm` spells each of SfmJob's choices as, in index order.
inline const char* const kSfmQuality[] = {"low", "medium", "high", "extreme"};
inline const char* const kSfmDataType[] = {"individual", "video", "internet"};
inline const char* const kSfmCameraMode[] = {"single", "folder", "image"};
inline const char* const kSfmPairs[] = {"auto", "exhaustive", "sequential",
                                        "prefilter", "spatial-blocks"};
inline const char* const kSfmMapper[] = {"flat", "bottom-up"};
inline const char* const kSfmFeatures[] = {"sift", "aliked-n16rot", "aliked-n32",
                                           "loma-b128", "loma-b"};
inline const char* const kSfmMetricGps[] = {"none", "horizontal", "full", "auto"};
inline const char* const kSfmSensorGauge[] = {"none", "up", "auto"};
inline const char* const kSfmExifAttitude[] = {"none", "up", "auto"};

template <int N>
const char* sfm_pick(const char* const (&table)[N], int i, int fallback = 0) {
    return table[(i >= 0 && i < N) ? i : fallback];
}

// The matcher combo is two entries -- brute force, or "the learned matcher for
// this frontend" -- because a learned matcher only reads the descriptors it
// was trained on. Which one that is follows --features.
inline const char* sfm_matcher_for(int features, int matcher) {
    if (features == 0 || matcher != 1) return "bruteforce";
    const std::string f = sfm_pick(kSfmFeatures, features);
    return f.rfind("loma", 0) == 0 ? sfm_pick(kSfmFeatures, features) : "lightglue";
}

// SS_SFM_SUBPROCESS=1 starts a session with the escape hatch below on.
inline bool sfm_subprocess_default() {
    const char* v = spirula::env("SFM_SUBPROCESS");
    return v && *v && *v != '0';
}

struct SfmJob {
    // ---- shared with ColmapRunner's path ----
    PrepJob prep;
    // ... and so is this: the depth and normal maps are written after the
    // reconstruction, from the dataset it produced, whichever engine made it.
    GeometryJob geometry;
    // Laser scans the model is aligned with; they replace the geometry step.
    LidarJob lidar;
    DenseJob dense;

    // ---- reconstruction ----
    // What the user asked to redo or keep; the plan (DatasetPlan.h) decides
    // the rest, step by step, against what the workspace records.
    PlanRequest request;
    // Keep feature detection out of the masked areas too, not only training.
    // Off still writes the masks -- worth it where what they cover holds still
    // and carries the finer detail the cameras converge on.
    bool mask_features = true;
    int quality = 2;                  // 0 low, 1 medium, 2 high, 3 extreme
    int data_type = 0;                // 0 individual photos, 1 video, 2 internet
    std::string camera_model = "opencv";
    int camera_mode = 1;              // 0 single, 1 per folder, 2 per image
    int pairs = 0;                    // index into kSfmPairs
    int partition_mode = 0;           // 0 automatic, 1 off, 2 on when GPS is complete
    int non_partition_pairs = 0;      // pairing choice restored when partitioning is off
    bool partition_resolved = false; // live GUI memory snapshot, not a preset setting
    int block_size = 2000;
    bool block_size_auto = true;
    int block_neighbours = 40;
    float block_radius = 0;
    int block_cache_mb = 512;
    int map_memory_mb = 0;
    bool block_cache_auto = true;
    int overlap = 10;                 // sequential neighbours
    // Sequential pairing only: also match the pairs GPU pair selection finds,
    // so a capture that comes back on itself links across the seam instead of
    // breaking into one model per unbroken run of frames.
    bool loop_closure = true;
    // Pair selection only: also match each image with its neighbours in file
    // order, the converse of the above.
    bool prefilter_sequential = false;
    // Hand the reconstruction the frame order of every video and of every
    // folder marked as shot in order (the manifest's `sequences:`), so the
    // mapper places an image among its neighbours before it consults the rest.
    bool use_sequence = true;
    float init_focal_px = 0.0f;       // 0 = guess from EXIF / image size
    // Starting distortion, "k1,k2,..." in the lens model's own order; empty
    // starts at zero.
    std::string init_distortion;
    // When bundle adjustment fits the distortion: 0 throughout, 1 only in the
    // finishing pass, 2 never -- hold init_distortion as a calibration.
    int distortion_refine = 0;
    // One more bundle adjustment at the very end with every image on its own
    // intrinsics, whatever the camera sharing above says.
    bool final_per_image_intrinsics = false;
    // ... and one with the rigs released, every image on its own pose.
    bool final_free_rig = false;
    int max_features = 0;             // 0 = the quality preset's
    int max_image_size = 0;           // 0 = the quality preset's
    // 0 flat, 1 bottom-up. GPS partitioning chooses bottom-up.
    int mapper = 0;
    // An index into kSfmFeatures. A frontend choice, not a quality level: the
    // learned ones run on their own resolution ladder, so the quality preset
    // means something different for each.
    int features = 0;
    // 0 brute force, 1 LightGlue. Only meaningful with a learned frontend, and
    // an order of magnitude slower per pair -- the panel greys it out for SIFT
    // and the CLI refuses the combination outright.
    int matcher = 0;
    // Scale and heading from the capture's GPS: 0 off, 1 latitude and
    // longitude, 2 with altitude, 3 (the default) the CLI's per-capture `auto`.
    // Indices are saved in presets, so `auto` is appended rather than first.
    int metric_gps = 3;
    // The video's own IMU and GPS track: 0 off, 1 orientation only, 2 (the
    // default) orientation and whatever metric scale passes its own checks.
    int sensor_gauge = 2;
    // The camera attitude a drone writes into each photo: 0 off, 1 orientation
    // only, 2 (the default) orientation and north.
    int exif_attitude = 2;
    // Keep features/, matches.bin and .resume/ after a finished run. On by
    // default and remembered between sessions: they are what makes a cancelled
    // or failed reconstruction resumable (sfm/core/Resume.h).
    bool keep_intermediate = true;
    // Bundle adjustment on the host from the start. The escape hatch for a
    // driver that resets under a long solve: a run falls back by itself when
    // the device fails, but only after paying for the failure.
    bool ba_cpu = false;
    // Reconstruct in a child process instead of this one: the escape hatch for
    // a driver that resets under a long solve. SS_SFM_SUBPROCESS=1 starts a
    // session with it on.
    bool subprocess = sfm_subprocess_default();

    // Canonical UUID passed to in-process and child runs; empty uses shared
    // precedence.
    std::string device_selector;

    // What colour space the photographs are in. Everything that reads pixels --
    // SfM, AI masking, depth and normals -- converts to sRGB first, which is
    // what those detectors and models were trained on. Empty = Rec.709/sRGB.
    std::string image_gamut;
    std::optional<bool> image_is_linear;
    // What the detectors and models see, brightened in linear light: "", "auto"
    // or stops (core/ColorSpace.h). Training reads the files as they are.
    std::string image_exposure;
    // false: the sparse point cloud stays sRGB (train with point-color-gamut
    // Rec.709). true: written in the images' space, the trainer's default.
    bool point_color_in_image_space = false;

    // Extra flags typed by the user, appended verbatim. The escape hatch for
    // everything the panel does not surface -- `spirula-sfm auto --help` lists
    // the lot, and this is how an expert reaches it without us mirroring 130
    // flags into the GUI.
    std::string extra_args;
};

// Whether a sequential window can run, and so whether `overlap` means
// anything: named, or what "auto" resolves to for a short video, or taken
// alongside pair selection.
inline bool sequential_window_applies(const SfmJob& j) {
    return j.pairs == 2 || (j.pairs == 0 && j.data_type == 1) ||
           ((j.pairs == 0 || j.pairs == 3) && j.prefilter_sequential);
}

inline SfmPartitionRecommendation sfm_partition_for_job(
    const SfmJob& job, size_t total_ram, size_t available_ram) {
    const int quality = std::clamp(job.quality, 0, 3);
    const size_t features = job.max_features > 0 ? (size_t)job.max_features
        : (size_t)(job.features == 0 ? 2048 : 1024) << quality;
    const size_t descriptor = job.features == 0 ? 128 : job.features == 4 ? 1024 : 512;
    return sfm_partition_recommendation(total_ram, available_ram, features, descriptor);
}

// What a learned frontend still has to fetch, in order; empty for SIFT with
// brute force, and empty once both artifacts are cached.
std::vector<PendingDownload> sfm_feature_downloads(int features, int matcher);
bool sfm_features_cached(int features, int matcher);

class SfmRunner {
public:
    enum class State { Idle, Running, Done, Failed, Cancelled };

    ~SfmRunner();

    // "" when spirula-sfm is available, otherwise why it is not.
    static std::string availability();

    // `films` are the screen's picture reels, null for a caller with no
    // screen; they outlive the run.
    void start(const SfmJob& job, RunFilms films = {});
    // Replace the settings no stage has read yet, so the screen's masking and
    // reconstruction options stay live while an earlier stage works. The
    // inputs, the output folder and the frame settings define the run and are
    // never taken from here.
    void update(const SfmJob& job);
    void cancel();

    State state() const { return _state.load(); }
    // Which step the run is on, how far through, and its lines -- everything
    // the dataset screen draws.
    RunProgress& steps() { return _prog; }
    std::string stage();
    std::string error();
    std::string dataset_dir();
    std::string image_dir();
    // What the trainer's mask_dir should be, or "" when the dataset has no
    // masks. Usually "masks" (the parser default); an absolute path when the
    // masks were only read, which is what photos used where they are do.
    std::string mask_dir();
    // Are those masks white where the image is REMOVED? Only ones the run
    // handed on untouched can be; what it wrote is the usual way round.
    bool mask_flipped() const;
    // 0..1 within the current stage, or -1 when it cannot be estimated.
    float progress() const;
    // Where the child is writing its snapshots, and the two folders the screen
    // reads alongside them. Set once the run has decided its workspace; empty
    // before that and for a run that produced neither.
    std::string progress_dir();
    // The extractor's downscaled copies, for a screen drawing the frames.
    std::string thumbs_dir();
    // The matches the verification stage has produced so far.
    std::string live_matches_path();
    std::string features_dir();
    std::string matches_path();
    // The two folders the reconstruction is reading, absolute: what the
    // previews draw their pictures from. `sfm_mask_dir` is "" when the dataset
    // has none.
    std::string sfm_image_dir();
    std::string sfm_mask_dir();
    // Throw away what the last run left for the screen to read: the snapshot
    // directory, the feature files and matches.bin. Deliberately NOT done when
    // the run ends -- going back to the features or the match map after a
    // reconstruction finishes is the ordinary thing to do, and they are the
    // only copy of what it found. Call it when the screen is done with them:
    // leaving the dataset screen, or closing the app. A no-op while a run is
    // going, and for a job that asked to keep them.
    void sweep_intermediates();
    // Done, but under half the images registered (or a high reprojection
    // error). The dataset is usable; the user should know it has gaps.
    bool partial() const { return _partial.load(); }
    // Done, but the metric frame asked for could not be fitted: the model is
    // in its own units, not metres.
    bool not_metric() const { return _not_metric.load(); }

private:
    void run(SfmJob job);
    // The three parts of update(), applied where the run reaches them.
    void take_reconstruction(SfmJob& job);
    void take_masking(PrepJob& prep);
    void take_geometry(SfmJob& job);
    void take_dense(SfmJob& job);
    void log(const std::string& line, bool detail = true);
    void set_stage(Stage st, const std::string& s);
    // Stage changes driven by the child's output, which repeats a
    // stage's lines many times over.
    void set_stage_if_new(Stage st, const char* s);
    // Where the run is: polled from the child's status.bin, or handed over
    // by the in-process run. apply_status is what both feed.
    void poll_status();
    void apply_status(const RunStatus& st);
#ifdef SS_TOOL_SFM
    // The panel's per-input lens and focal rows, as the file the run reads.
    sfm::Manifest build_manifest(const SfmJob& job, const PrepResult& prep);
    static std::vector<sfm::SequenceDef> build_sequences(const SfmJob& job);
    static std::vector<sfm::RigDef> build_rigs(const PrepJob& prep,
                                               const PrepResult* res = nullptr);
#endif
    std::vector<std::string> recon_args(const SfmJob& job,
                                        const PrepResult& prep);

    std::thread _worker;
    std::atomic<State> _state{State::Idle};
    std::atomic<bool> _cancel{false};
    std::atomic<bool> _partial{false};
    std::atomic<bool> _not_metric{false};
    // The child said how it ended, so the exit code need not be interpreted.
    bool _have_status = false;
    int64_t _status_mtime = 0;
    RunProgress _prog;
    RunFilms _films;
    std::mutex _mu;
    std::string _error, _dataset_dir, _image_dir, _mask_dir;
    std::atomic<bool> _mask_flipped{false};
    // Absolute; what the screen polls while the run is going.
    std::string _progress_dir, _features_dir, _matches_path;
    std::string _sfm_image_dir, _sfm_mask_dir;
    // The workspace whose intermediates are still on disk, "" when there are
    // none to sweep (see sweep_intermediates).
    std::string _sweep_dir;
    SfmJob _live;                        // guarded by _mu; see update()
};

}  // namespace gui
