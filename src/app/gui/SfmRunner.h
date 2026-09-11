#pragma once

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

#include "app/gui/DatasetPrep.h"
#include "core/Env.h"
#include "app/gui/SfmProgress.h"
#include "sfm/core/Manifest.h"
#include "app/gui/FilmReel.h"
#include "app/gui/GeometryRunner.h"
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

    // ---- reconstruction ----
    // Replace the model in the output folder. A run left to itself REUSES one,
    // which is how a finished dataset gets masks and geometry without a rebuild.
    bool redo_model = false;
    int quality = 2;                  // 0 low, 1 medium, 2 high, 3 extreme
    int data_type = 0;                // 0 individual photos, 1 video, 2 internet
    std::string camera_model = "opencv";
    int camera_mode = 1;              // 0 single, 1 per folder, 2 per image
    int pairs = 0;                    // 0 auto, 1 exhaustive, 2 sequential, 3 prefilter
    int overlap = 10;                 // sequential neighbours
    // Sequential pairing only: also match the pairs GPU pair selection finds,
    // so a capture that comes back on itself links across the seam instead of
    // breaking into one model per unbroken run of frames.
    bool loop_closure = true;
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
    // 0 flat, 1 bottom-up. Flat for every capture, whatever its size: there is
    // no automatic switch, here or in `spirula sfm`.
    int mapper = 0;
    // 0 SIFT, 1 ALIKED-n16rot, 2 ALIKED-n32. The learned ones fetch a
    // checkpoint on first use and run on their own resolution ladder, so the
    // quality preset means something different for each -- which is why this
    // is a frontend choice and not a quality level.
    int features = 0;
    // 0 brute force, 1 LightGlue. Only meaningful with a learned frontend, and
    // an order of magnitude slower per pair -- the panel greys it out for SIFT
    // and the CLI refuses the combination outright.
    int matcher = 0;
    // Scale and heading from the photographs' EXIF GPS: 0 off, 1 (the default)
    // latitude and longitude, 2 with altitude. 1 leaves the tilt to the
    // cameras, which a city capture's altitude is too biased to give.
    int metric_gps = 1;
    // The video's own IMU and GPS track: 0 off, 1 orientation only, 2 (the
    // default) orientation and whatever metric scale passes its own checks.
    int sensor_gauge = 2;
    bool keep_intermediate = false;   // keep features/ and matches.bin
    // Bundle adjustment on the host from the start. The escape hatch for a
    // driver that resets under a long solve: a run falls back by itself when
    // the device fails, but only after paying for the failure.
    bool ba_cpu = false;
    // Reconstruct in a child process instead of this one: the escape hatch for
    // a driver that resets under a long solve. SS_SFM_SUBPROCESS=1 starts a
    // session with it on.
    bool subprocess = sfm_subprocess_default();

    // What colour space the photographs are in. Everything that reads pixels --
    // SfM, AI masking, depth and normals -- converts to sRGB first, which is
    // what those detectors and models were trained on. Empty = Rec.709/sRGB.
    std::string image_gamut;
    std::optional<bool> image_is_linear;
    // false: the sparse point cloud stays sRGB (train with
    // convert-initial-point-cloud-color on). true: written in the images' space.
    bool point_color_in_image_space = false;

    // Extra flags typed by the user, appended verbatim. The escape hatch for
    // everything the panel does not surface -- `spirula-sfm auto --help` lists
    // the lot, and this is how an expert reaches it without us mirroring 130
    // flags into the GUI.
    std::string extra_args;
};

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
    void log(const std::string& line, bool detail = true);
    void set_stage(Stage st, const std::string& s);
    // Stage changes driven by the child's output, which repeats a
    // stage's lines many times over.
    void set_stage_if_new(Stage st, const char* s);
    // Where the run is: polled from the child's status.bin, or handed over
    // by the in-process run. apply_status is what both feed.
    void poll_status();
    void apply_status(const RunStatus& st);
    // The panel's per-input lens and focal rows, as the file the run reads.
    sfm::Manifest build_manifest(const SfmJob& job, const PrepResult& prep);
    static std::vector<sfm::RigDef> build_rigs(const PrepJob& prep);
    // Everything the child is told about the model, as against where to put
    // it. Both the command line and the workspace's stamp are made from this.
    std::vector<std::string> recon_args(const SfmJob& job, const PrepResult& prep);

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
