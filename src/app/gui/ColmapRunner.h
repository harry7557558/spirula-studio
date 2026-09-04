#pragma once

// ColmapRunner -- turns raw images or a video into a trainable COLMAP
// dataset by driving the external `colmap` (>= 4.x; the CLI flags follow
// reference/scripts/run_colmap.bash) on a worker thread with live log
// streaming and cancellation.
//
// It is the CUDA build's dataset path and the fallback everywhere else; the
// Vulkan build defaults to SfmRunner, which needs nothing installed. The two
// share their first half through DatasetPrep -- frames, sharpest-frame
// selection, .insv track splitting and masking -- so only what is actually
// COLMAP-specific lives here.
//
// Pipeline:
//   DatasetPrep (frames + masks; see DatasetPrep.h)
//   feature_extractor (SIFT or ALIKED; optional initial camera params) ->
//           exhaustive / sequential (+ optional vocab-tree loop closure) /
//           vocab-tree matcher (the tree is auto-found or downloaded;
//           optional LightGlue matching) -> mapper -> best-effort
//           model_merger when the scene splits into partial models ->
//           [optional] bundle_adjuster refinement on the largest model
//
// Output layout (what the dataset parsers auto-detect):
//   <workspace>/database.db
//   <workspace>/sparse/0/{cameras,images,points3D}.bin
//   <workspace>/images/            (video input: extracted frames)
//   <workspace>/masks/             (when masking is enabled)
//
// For a folder-of-images input the images are NOT copied; COLMAP indexes
// them where they are (recursively) and the GUI passes the absolute path as
// image_dir for the immediate open (the parsers join dataset_dir /
// image_dir, absolute wins). No marker file is written -- when re-opening
// such a dataset later, set data.image_dir in the dataparser options (video
// datasets need nothing: images/ is the default).

#include "app/gui/DatasetPrep.h"   // MaskClick
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

// COLMAP camera models the dataset parser understands (ColmapParser.cpp).
inline const char* kColmapCameraModels[] = {
    "OPENCV", "PINHOLE", "SIMPLE_PINHOLE", "SIMPLE_RADIAL", "RADIAL",
    "FULL_OPENCV", "OPENCV_FISHEYE", "THIN_PRISM_FISHEYE",
    "SIMPLE_RADIAL_FISHEYE", "RADIAL_FISHEYE",
};
inline constexpr int kNumColmapCameraModels = 10;

// The description shown beside each of them. COLMAP's names stay COLMAP's --
// they are what a user who read its documentation is looking for -- so the
// sentence saying which camera each one is for is what carries the meaning.
// Several of them are the same lens with fewer coefficients, and share one.
inline std::vector<const spirula::i18n::Msg*> colmap_camera_model_helps() {
    namespace m = spirula::i18n::msg::dataset;
    return {&m::lens_opencv_help, &m::lens_pinhole_help,
            &m::lens_simple_pinhole_help, &m::lens_radial_help,
            &m::lens_radial_help, &m::lens_full_opencv_help,
            &m::lens_fisheye_kb_help, &m::lens_fisheye_thin_prism_help,
            &m::lens_fisheye_kb_help, &m::lens_fisheye_kb_help};
}

inline bool colmap_model_is_fisheye(const std::string& m) {
    return m.size() > 7 && m.compare(m.size() - 7, 7, "FISHEYE") == 0;
}

struct ColmapJob {
    // What the user picked, in order (see PrepInput). Frame extraction and
    // masking are shared with the built-in path, so several inputs land in one
    // images/ tree here too -- but COLMAP's feature_extractor takes ONE camera
    // model for the run, so the per-input models a PrepInput can carry are only
    // honoured by the built-in reconstruction.
    std::vector<PrepInput> inputs;
    std::string workspace;               // output dataset dir (created)
    bool resume = true;                  // reuse artifacts an interrupted
                                         // run left in the workspace:
                                         // extracted frames, masks (mask.py
                                         // resumes), features + matches
                                         // (COLMAP skips existing DB rows),
                                         // and completed sparse models.
                                         // false = require a clean folder.
    std::string colmap_exe = "colmap";
    std::string ffmpeg_exe = "ffmpeg";
    std::string python_exe = "python3";  // for the masking script
    bool force_external_decode = false;  // ffmpeg even when we could decode
    bool force_external_masking = false; // mask.py even when we could segment

    // Steps a re-run redoes rather than reuses; see PrepJob.
    bool redo_frames = false;
    bool redo_masks = false;
    bool redo_model = false;             // reconstruct again over existing
                                         // frames, masks and features

    // Cameras
    std::string camera_model = "OPENCV"; // ImageReader.camera_model
    int camera_mode = 0;                 // 0 = one shared camera,
                                         // 1 = one per subfolder,
                                         // 2 = one per image
    float init_focal_factor = 0.0f;      // > 0: initial fx = fy = factor *
                                         // image width (composed into
                                         // ImageReader.camera_params with
                                         // centered principal point)
    std::string camera_params;           // raw ImageReader.camera_params
                                         // (overrides init_focal_factor)

    // Features & matching
    int feature_type = 0;                // 0 = SIFT, 1 = ALIKED (neural)
    bool lightglue = false;              // LightGlue neural matching
    int quality = 1;                     // 0 = fast, 1 = balanced, 2 = high
    int matcher = 1;                     // 1 exhaustive, 2 sequential,
                                         // 3 vocab tree (no auto: the GUI
                                         // presets a default per input type
                                         // and the user confirms it)
    bool seq_loop_closure = true;        // sequential: vocab-tree loop
                                         // detection (SIFT features only)

    // Video extraction
    float video_fps = 2.0f;              // kept frames per second
    int sharp_window = 3;                // pick sharpest of N candidates (1 = off)
    int max_frames = 100000;

    // Advanced
    int max_num_features = 0;            // 0 = per-quality default
    int max_image_size = 0;              // 0 = COLMAP default
    int seq_overlap = 10;                // sequential matcher overlap
    bool seq_quadratic_overlap = true;   // match frame i with i +- 2^k too
    bool estimate_affine_shape = false;  // + guided feature matching (SIFT)
    bool ba_use_gpu = true;              // Mapper.ba_use_gpu (forced off for
                                         // fisheye models: not supported)
    int mapper_extra_params = 0;         // Mapper.ba_refine_extra_params:
                                         // 0 auto (fix for perspective
                                         //   models, refine for fisheye),
                                         // 1 refine during mapping,
                                         // 2 fix until the final BA pass
    int min_num_matches = 0;             // 0 = COLMAP default (15)

    // Wrong-match suppression for large scenes with repetitive texture
    // (multiple similar rooms etc.), where visually similar but physically
    // different parts get matched and weld together. All 0 = COLMAP
    // defaults; see the GUI tooltips for suggested values.
    float match_max_ratio = 0.0f;        // SiftMatching.max_ratio (def 0.8;
                                         // lower = stricter Lowe ratio test)
    int   min_inliers_per_pair = 0;      // TwoViewGeometry.min_num_inliers
                                         // (def 15; raise to drop weakly
                                         // verified pairs entirely)
    int   abs_pose_min_num_inliers = 0;  // Mapper.abs_pose_min_num_inliers
                                         // (def 30; raise = stricter image
                                         // registration)
    float abs_pose_min_inlier_ratio = 0; // Mapper.abs_pose_min_inlier_ratio
                                         // (def 0.25)
    float abs_pose_max_error = 0.0f;     // Mapper.abs_pose_max_error px
                                         // (def 12; lower = stricter)
    bool merge_models = true;            // try model_merger when the mapper
                                         // splits into partial models
    bool final_bundle_adjust = true;     // bundle_adjuster refinement pass
    std::string vocab_tree_path;         // "" = auto find / download

    // AI masking. The built-in path wants a checkpoint file
    // (mask_model_path, from ModelCache); the mask.py fallback wants
    // a model name it understands (mask_model).
    bool mask_enable = false;
    std::string mask_prompt;             // "people; cars; ..."
    std::string mask_negative_prompt;
    bool mask_keep_subject = false;      // prompt names what to KEEP
    std::string mask_model_path;
    std::string mask_model = "sam2.1_hiera_large";
    int mask_max_image_size = 1600;
    float mask_threshold = 0.5f;         // all four: see PrepJob in DatasetPrep.h
    float mask_nms = 0.1f;
    bool mask_memory = false;            // track objects across a video's frames
    int mask_detect_every = 1;
    int mask_memory_frames = 0;
    std::vector<MaskClick> mask_clicks;  // clicked objects, see DatasetPrep.h

    // The photographs' colour space; masking and geometry convert to sRGB.
    std::string image_gamut;
    std::optional<bool> image_is_linear;

    // Depth and normals, written after the reconstruction from the dataset it
    // produced. Shared with the built-in path (SfmJob), which runs the same
    // child over the same folder.
    GeometryJob geometry;
};

class ColmapRunner {
public:
    enum class State { Idle, Running, Done, Failed, Cancelled };

    ~ColmapRunner();

    void start(const ColmapJob& job, RunFilms films = {});
    // Replace the settings no stage has read yet; see SfmRunner::update.
    void update(const ColmapJob& job);
    void cancel();

    State state() const { return _state.load(); }
    RunProgress& steps() { return _prog; }
    std::string stage();                 // current pipeline stage label
    std::string error();                 // set when Failed
    std::string dataset_dir();           // valid when Done
    std::string image_dir();             // image_dir to train with ("" = default)
    std::string mask_dir();              // mask_dir to train with ("" = none)
    bool mask_flipped() const;           // ... and are they white where REMOVED?

private:
    void run(ColmapJob job);
    void take_reconstruction(ColmapJob& job);
    void take_masking(PrepJob& prep);
    void take_geometry(ColmapJob& job);
    void log(const std::string& line, bool detail = true);
    int  exec(const std::vector<std::string>& argv);
    void set_stage(Stage st, const std::string& s);
    bool check_colmap_version(const ColmapJob& job, std::string& err);
    std::string resolve_vocab_tree(const ColmapJob& job);
    // Mean reprojection error of a model (colmap model_analyzer); a large
    // sentinel when it cannot be determined.
    double model_reproj_error(const ColmapJob& job, const std::string& model);

    std::thread _worker;
    std::atomic<State> _state{State::Idle};
    std::atomic<bool> _cancel{false};
    RunProgress _prog;
    RunFilms _films;
    std::mutex _mu;                      // guards the strings below
    std::string _error, _dataset_dir, _image_dir, _mask_dir;
    std::atomic<bool> _mask_flipped{false};
    ColmapJob _live;                     // guarded by _mu; see update()
};

}  // namespace gui
