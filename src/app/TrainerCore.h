#pragma once

// TrainerCore -- the engine-side training session shared by the standalone
// CLI (app/cli/main.cpp) and the native GUI (app/gui/). This is the single
// training driver. It began as a port of a Python trainer that no longer
// exists; the per-step logic lives in build_step_config() and nowhere else,
// so there is nothing left to keep in sync.
//
// TrainerSession splits the run into phases so front-ends can interleave
// their own UI between them:
//   check_config()  -> throws for unported features, logs warnings
//   load_dataset()  -> parse + post-split bake (no GPU work)
//   setup_engine()  -> output dir, seeding, engine + DataManager init
//   train()         -> the step loop; pause / stop / render-fairness via
//                      the public atomics, per-step callback for progress
//
// The engine is a process-global singleton: one live session at a time.
// setup_engine() calls engine_reset(), so a fresh session can follow a
// finished one in the same process (the GUI's "train again" path).

#include "engine/Engine.h"
#include "data/DatasetParser.h"
#include "app/webviewer/RenderWorker.h"
#include "config/TrainConfig.h"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace spirula {

// ===========================================================================
// Color-space handling
// ===========================================================================

using Mat3f = std::array<float, 9>;

Mat3f gamut_to_rec709(const std::string& name);
Mat3f invert3x3(const Mat3f& m);

struct ColorResolution {
    std::string splat_gamut;   // "" = Rec.709 / none
    std::string image_gamut;
    bool splat_linear  = false;
    bool image_linear  = false;
    bool convert_seed  = false;  // convert_initial_point_cloud_color resolved
};

ColorResolution resolve_color(const TrainConfig& c);


// ===========================================================================
// LR schedule
// ===========================================================================

float scheduled_lr(int step, int max_steps, float lr,
                   std::optional<float> lr_final = std::nullopt,
                   std::optional<int> warmup = std::nullopt);


// ===========================================================================
// Splat seeding (3dgs branch)
// ===========================================================================

struct SeedSplats {
    int64_t num = 0;                 // live splats (<= cap rows)
    std::vector<float> means;        // [cap, 3]
    std::vector<float> quats;        // [cap, 4]
    std::vector<float> scales;       // [cap, 3]  (log)
    std::vector<float> opacities;    // [cap, 1]  (logit)
    std::vector<float> features_dc;  // [cap, 3]
    std::vector<float> features_sh;  // [cap, dim_sh-1, 3]  (zeros)
};

SeedSplats seed_splats(const ColmapPoints3D& pts, const TrainConfig& cfg,
                       const ColorResolution& color);

// A warm start from a trained 3DGS PLY (--init-ply): the file supplies every
// parameter, and only the layout is re-fitted to this run (cap_max, sh_degree,
// relative_scale, splat gamut). The PLY must be in this run's training frame.
SeedSplats seed_splats_from_ply(const std::string& path, const TrainConfig& cfg,
                                const ColorResolution& color);

// The dataset's own seeds appended after the PLY's (--init-ply-add-points), so
// the warm start covers what the source model has and the point cloud covers
// what it does not. Adds nothing once `s` already fills cap_max.
void append_point_seeds(SeedSplats& s, const ColmapPoints3D& pts,
                        const TrainConfig& cfg, const ColorResolution& color);


// ===========================================================================
// Per-step EngineStepConfig
// ===========================================================================

struct RunState {
    float train_frame_scale = 1.0f;
    bool  splat_linear = false;
    // Resolved against the dataset's lenses when the flag is unset; see
    // TrainerCore.cpp.
    bool  input_depth_is_ray_depth = false;
    bool  bilagrid_rgb_init    = false;
    bool  bilagrid_depth_init  = false;
    bool  bilagrid_normal_init = false;
    bool  ppisp_init           = false;
};

std::array<float, (int)LossWeightIndex::length>
build_loss_weights(const TrainConfig& c, int step);

EngineStepConfig build_step_config(const TrainConfig& c, const RunState& st,
                                   int step);

// Flat config.json dump, one key per flag (config/TrainConfigJson.h).
void save_config_json(const TrainConfig& c, const std::filesystem::path& out_dir,
                      const std::string& preset);

// "" when this config is runnable, else the sentence naming the flag that is
// not implemented -- exactly what TrainerSession::check_config() throws. A
// front-end that wants to report the problem instead of catching it (the
// GUI's batch pre-flight) asks this first.
std::string train_config_unsupported(const TrainConfig& c);


// ===========================================================================
// TrainerSession
// ===========================================================================

// "m:ss", or "h:mm:ss" past an hour; negative (not known yet) is "--:--".
std::string format_duration(double seconds);

struct TrainerProgress {
    int step = 0;              // 0-based step that just finished
    int total_steps = 0;
    double step_latency = 0.0; // seconds, this step
    int64_t num_splats = 0;
    std::map<std::string, float> losses;
};

struct TrainerCallbacks {
    // Called after every completed step, engine mutex released.
    std::function<void(const TrainerProgress&)> on_step;

    // A dataset file went unreadable mid-run; blocks as long as the front end
    // needs. true retries the decode, false stops the run (still saving a
    // checkpoint). Unset fails the run outright, which is what a script wants.
    std::function<bool(const std::string&)> on_data_error;
};

class TrainerSession {
public:
    TrainerSession() = default;
    TrainerSession(const TrainerSession&) = delete;
    TrainerSession& operator=(const TrainerSession&) = delete;

    // Inputs. Set before check_config().
    TrainConfig cfg;
    std::string preset = "3dgs";
    // Human-readable progress/warning messages. Default (unset) = stdout.
    std::function<void(const std::string&)> log_fn;

    // Output-dir / config.json overrides, for a front-end that owns them.
    std::string out_dir_override;      // "" = derive from cfg
    bool        write_config_json = true;

    // Filled by load_dataset().
    ParsedDataset ds;
    PostSplitCameras post;
    bool has_mask = false;
    bool has_depth = false;
    bool has_normal = false;

    // Filled by setup_engine().
    std::filesystem::path out_dir;
    RunState st;
    // Step the restored checkpoint stopped at; train() starts here. 0 unless
    // setup_engine() resumed.
    int start_step = 0;

    // Filled by train(), reported in metrics.json: benchmark runs are separate
    // processes, so this is the only way they see the in-process totals.
    double training_time_s = 0.0;   // wall clock of the step loop
    double engine_vram_mb  = 0.0;   // pool high-water mark + scratch
    // Set by the front-end from viewer_upload_cameras()'s return value;
    // copied into ViewerRenderConfig::base_camera_size for the live
    // frustum-size control.
    float viewer_base_camera_size = 0.0f;

    // Coordination between the train loop, viewer render workers, and
    // front-end controls.
    std::mutex engine_mutex;
    std::atomic<bool> paused{false};
    std::atomic<bool> stop_requested{false};
    // Clear before stop_requested to end the run without writing the final
    // checkpoint: whatever the last periodic save left on disk is the result.
    std::atomic<bool> save_on_stop{true};
    std::atomic<bool> render_pending{false};
    std::atomic<int>  cur_step{0};

    // Throws std::runtime_error for features the managed C++ path does not
    // support; logs warnings for approximated ones.
    void check_config();

    // Parse the dataset + bake POST-split cameras. No GPU work.
    void load_dataset();

    // Create the output dir, dump config.json, reset + seed the engine,
    // set up the DataManager and bilagrid/PPISP. Requires load_dataset().
    void setup_engine();

    // The training loop. Returns when all steps ran or stop_requested was
    // set (a final checkpoint is saved either way unless steps_per_save==0
    // or save_on_stop was cleared).
    void train(const TrainerCallbacks& cb = {});

    // One step: build the EngineStepConfig for `step` and run it. This is the
    // whole of train()'s per-step work, and the reason TrainerCore exists --
    // build_step_config() is the ported logic that would otherwise drift.
    // Front-ends that keep their own loop (the Python trainer, for resume /
    // profiling / eval / debug dumps) call this instead of train(). The
    // caller must hold engine_mutex.
    std::map<std::string, float> train_step(int step);

    void save_checkpoint(int step);

    // Held-out eval: render every frame of the eval split, score it, and write
    // metrics.json. No-op when eval_mode is "all" (nothing is held out) or the
    // eval split is empty. Replaces the engine's DataManager with one over the
    // eval split, so it must run AFTER training.
    //
    // Reports l1/psnr/ssim and the cc_ variants of each (computed on the
    // colour-corrected render). LPIPS is not computed here -- pass
    // --save-eval-images and run reference/python/eval_lpips.py over the PNGs.
    void eval();

    // The seeds setup_engine() hands the engine, from the point cloud and/or
    // cfg.init_ply.
    SeedSplats seed_world(const ColorResolution& color);

    // Restore engine state from cfg.resume; sets start_step. Called by
    // setup_engine().
    void restore_checkpoint();

    // Wall clock of the step loop, with paused spans excluded. 0 before
    // train() starts, frozen once it returns.
    double elapsed_seconds() const;

    // Remaining wall clock over the last 100 steps' average, or -1 before
    // the first step lands.
    double eta_seconds() const;

    // The /progress response body.
    std::string progress_json();

    // Viewer wiring shared by the web viewer and the GUI viewport.
    ViewerRenderConfig make_viewer_config() const;
    ViewerHooks make_viewer_hooks();

    void log(const std::string& msg);

private:
    // Bracket the train loop's pause gate so paused time stays out of
    // elapsed_seconds().
    void pause_clock_start();
    void pause_clock_stop();

    // Seconds per step over the window, or -1 while it is empty.
    double avg_step_latency() const;

    mutable std::mutex _time_mutex;                        // guards the clock
    std::chrono::steady_clock::time_point _start_time{};   // {} = not started
    std::chrono::steady_clock::time_point _end_time{};     // {} = running
    std::chrono::steady_clock::time_point _pause_start{};  // {} = not paused
    double _paused_s = 0.0;
    mutable std::mutex _progress_mutex;    // guards the latency window
    std::deque<double> _step_latencies;    // last 100, seconds
};

}  // namespace spirula
