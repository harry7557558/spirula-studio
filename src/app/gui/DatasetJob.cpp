// The dataset job: what is running, the plan it follows, and launching it.

#include "app/gui/GuiApp.h"

#include "app/CrashLog.h"
#include "app/RunLog.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/Subprocess.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Dataset.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
namespace runlog = spirula::runlog;

namespace gui {

bool GuiApp::builtin_sfm_available() const {
    return SfmRunner::availability().empty();
}

bool GuiApp::colmap_available() const {
    return command_exists(_colmap_exe);
}

GuiApp::Engine GuiApp::effective_engine() const {
    if (!builtin_sfm_available()) return Engine::Colmap;
    if (!colmap_available()) return Engine::BuiltIn;
    return _engine;
}

bool GuiApp::dataset_busy() const {
    return _sfm.state() == SfmRunner::State::Running ||
           _colmap.state() == ColmapRunner::State::Running;
}
bool GuiApp::native_work_busy() const {
    const TrainRunner::Phase phase = _runner.phase();
    return _dense_delete.valid() || _mesh.busy() || dataset_busy() || scan_photos_busy() ||
           phase == TrainRunner::Phase::Loading ||
           phase == TrainRunner::Phase::Preparing ||
           phase == TrainRunner::Phase::Training ||
           phase == TrainRunner::Phase::Evaluating;
}

RunProgress* GuiApp::dataset_steps() {
    return effective_engine() == Engine::BuiltIn ? &_sfm.steps() : &_colmap.steps();
}

bool GuiApp::dataset_locked(Stage s) {
    if (!dataset_busy()) return false;
    const bool builtin = _sfm.state() == SfmRunner::State::Running;
    return (builtin ? _sfm.steps() : _colmap.steps()).ran(s);
}

void GuiApp::cancel_dataset_job() {
    // Cancelling the run on screen gives up the queue driving it, which is
    // what the trainer's stop confirmation already does.
    if (_batch_active) _batch_stop_after = _batch_stop_now = true;
    _dataset_training.pending = false;
    _pending_dataset_finish_command.clear();
    _sfm.cancel();
    _colmap.cancel();
}

const WorkspaceState& GuiApp::workspace_state() {
    std::string key = _workspace;
    for (const PrepInput& s : _sources) key += '\n' + s.path + '\n' + s.mask_dir;
    const double now = ImGui::GetTime();
    if (key != _ws_state_key || _ws_state_at < 0.0 || now - _ws_state_at > 1.0) {
        _ws_state_key = std::move(key);
        _ws_state_at = now;
        _ws_state = probe_workspace(_workspace, _sources);
        _ws_artifacts = workspace_artifacts(_workspace, _sources);
        const StencilTable t = stencil_table();
        settle_follow(t.owners, t.rows);
        PrepJob job = _sfm_job.prep;
        job.inputs = _sources;
        job.workspace = _workspace;
        _ws_record = read_plan_record(_workspace, job);
    }
    return _ws_state;
}

// The panel edits one set of fields; each runner gets its own struct because
// their remaining options do not overlap. This is the one place they are
// copied across, so a field cannot be set on the screen and silently not run.
void GuiApp::fill_masking(PrepJob& prep) const {
    // A click-only pick hides the prompt boxes, so what is left in them is not run.
    const MaskModelFiles mask_model = selected_mask_model();
    prep.mask_prompt = mask_model.text ? _mask.prompt : "";
    prep.mask_negative_prompt = mask_model.text ? _mask.negative_prompt : "";
    prep.mask_feature_prompt = mask_model.text ? _mask.feature_prompt : "";
    prep.mask_keep_subject = _mask.keep_subject;
    prep.mask_max_image_size = _mask.max_image_size;
    prep.mask_dilate_ratio = _mask.boundary_ratio();
    prep.mask_threshold = _mask.threshold;
    prep.mask_nms = _mask.nms;
    prep.mask_memory = _mask_memory;
    prep.mask_detect_every = _mask_detect_every;
    prep.mask_memory_frames = _mask_memory_frames;
    prep.mask_clicks = _mask.clicks;
    prep.mask_model_path = mask_model.model;
    prep.mask_detector_path = mask_model.detector;
    prep.mask_detector_threshold = _mask.box_threshold;
    // The one frozen choice, re-applied here because a job is rebuilt from
    // panel state and would otherwise drop it. Empty before the freeze, which
    // is exactly what an unstarted job wants.
    prep.device = _native_device_uuid;
    prep.inference_devices = resolved_worker_devices();
}

void GuiApp::sync_dataset_jobs() {
    // Shared lens circles start where the rows are first met, table shown or not.
    {
        const StencilTable t = stencil_table();
        settle_follow(t.owners, t.rows);
    }
    PrepJob prep;
    prep.inputs = _sources;
    // The checkbox decides whether the run is given the stencils; the drawings
    // stay on _sources either way, so switching it off does not lose them.
    if (!_border_enable)
        for (PrepInput& in : prep.inputs) in.stencil = app::FrameStencil{};
    prep.workspace = _workspace;
    prep.resume = _resume;
    prep.video_fps = _sfm_job.prep.video_fps;
    prep.adaptive_fps = _sfm_job.prep.adaptive_fps;
    prep.adaptive_range = _sfm_job.prep.adaptive_range;
    prep.sharp_window = _sfm_job.prep.sharp_window;
    prep.skip_blurry = _sfm_job.prep.skip_blurry;
    prep.blur_px = _sfm_job.prep.blur_px;
    prep.blur_neighbour = _sfm_job.prep.blur_neighbour;
    prep.similar_bits = _sfm_job.prep.similar_bits;
    prep.blur_review = _sfm_job.prep.blur_review;
    prep.keep_skipped = _sfm_job.prep.keep_skipped;
    prep.pano = _sfm_job.prep.pano;
    prep.max_frames = _sfm_job.prep.max_frames;
    prep.auto_rotate = _sfm_job.prep.auto_rotate;
    prep.force_external_decode = _sfm_job.prep.force_external_decode;
    prep.sync_tracks = _sfm_job.prep.sync_tracks;
    prep.ffmpeg_exe = _ffmpeg_exe;
    prep.mask_enable = _mask_enable;
    prep.flip_found_masks = _use_found_masks && _flip_found_masks;
    prep.photo_import = _photo_import;
    if (!_lidar.empty() && !_lidar_in_frame && prep.photo_import == PhotoImport::InPlace)
        prep.photo_import = PhotoImport::ConvertJpeg;
    fill_masking(prep);
    _sfm_job.prep = prep;
    _sfm_job.lidar = lidar_job();
    // Only for scans whose photographs become inputs; read nowhere else.
    if (_border_enable && _sfm_job.lidar.scan_photos && !_sfm_job.lidar.in_frame)
        for (const LidarInput& l : _lidar)
            if (l.photos > 0 && !l.stencil.empty()) _sfm_job.prep.scan_stencils[l.path] = l.stencil;

    _colmap_job.inputs = prep.inputs;
    _colmap_job.workspace = prep.workspace;
    _colmap_job.resume = prep.resume;
    // The frozen choice, so a later edit cannot drop the UUID the run has
    // already committed to. Empty before the freeze.
    _colmap_job.device = _native_device_uuid;
    _colmap_job.inference_devices = prep.inference_devices;
    _colmap_job.video_fps = prep.video_fps;
    _colmap_job.adaptive_fps = prep.adaptive_fps;
    _colmap_job.adaptive_range = prep.adaptive_range;
    _colmap_job.sharp_window = prep.sharp_window;
    _colmap_job.skip_blurry = prep.skip_blurry;
    _colmap_job.blur_px = prep.blur_px;
    _colmap_job.blur_neighbour = prep.blur_neighbour;
    _colmap_job.similar_bits = prep.similar_bits;
    _colmap_job.keep_skipped = prep.keep_skipped;
    _colmap_job.sync_tracks = prep.sync_tracks;
    _colmap_job.auto_rotate = prep.auto_rotate;
    _colmap_job.pano = prep.pano;
    _colmap_job.max_frames = prep.max_frames;
    _colmap_job.force_external_decode = prep.force_external_decode;
    _colmap_job.photo_import = prep.photo_import;
    _colmap_job.colmap_exe = _colmap_exe;
    _colmap_job.ffmpeg_exe = _ffmpeg_exe;
    _colmap_job.mask_enable = prep.mask_enable;
    _colmap_job.mask_prompt = prep.mask_prompt;
    _colmap_job.mask_negative_prompt = prep.mask_negative_prompt;
    _colmap_job.mask_feature_prompt = prep.mask_feature_prompt;
    _colmap_job.mask_keep_subject = prep.mask_keep_subject;
    _colmap_job.mask_max_image_size = prep.mask_max_image_size;
    _colmap_job.mask_dilate_ratio = prep.mask_dilate_ratio;
    _colmap_job.mask_threshold = prep.mask_threshold;
    _colmap_job.mask_nms = prep.mask_nms;
    _colmap_job.mask_memory = prep.mask_memory;
    _colmap_job.mask_detect_every = prep.mask_detect_every;
    _colmap_job.mask_memory_frames = prep.mask_memory_frames;
    _colmap_job.mask_clicks = prep.mask_clicks;
    _colmap_job.mask_model_path = prep.mask_model_path;
    _colmap_job.mask_detector_path = prep.mask_detector_path;
    _colmap_job.mask_detector_threshold = prep.mask_detector_threshold;

    PlanRequest req;
    req.redo_frames = _redo_frames;
    req.redo_masks = _redo_masks;
    req.redo_model = _redo_model;
    req.redo_geometry = _redo_geometry;
    req.redo_dense = _redo_dense;
    req.keep_built = _keep_built && _keep_built_for == _workspace;
    if (_part_choice_for == _workspace)
        std::copy(std::begin(_part_choice), std::end(_part_choice), std::begin(req.parts));
    _sfm_job.request = _colmap_job.request = req;
    _sfm_job.mask_features = _colmap_job.mask_features = _mask_features;
    // The same step either way: `spirula geometry` over the finished dataset.
    _sfm_job.geometry = _colmap_job.geometry = _geometry;
    _sfm_job.geometry.worker_devices = _colmap_job.geometry.worker_devices = resolved_worker_devices();
    _sfm_job.dense = _colmap_job.dense = _dense;
    if (_redo_dense || _dense_rebuild) _sfm_job.dense.config.rebuild = _colmap_job.dense.config.rebuild = true;
    _sfm_job.dense.config.device = _colmap_job.dense.config.device = _native_device_uuid;
    _sfm_job.dense.config.image_gamut = _colmap_job.dense.config.image_gamut = _sfm_job.image_gamut;
    _sfm_job.dense.config.image_is_linear = _colmap_job.dense.config.image_is_linear = _sfm_job.image_is_linear;
    _sfm_job.dense.config.image_exposure = _colmap_job.dense.config.image_exposure = _sfm_job.image_exposure;
    // The frozen choice survives the copy above, so a later panel edit cannot
    // drop the UUID a run already committed to.
    _sfm_job.geometry.device_uuid = _colmap_job.geometry.device_uuid =
        _geometry.device_uuid = _native_device_uuid;
    // ... and the reconstruction's own selector, which SfmRunner::update must
    // not be able to overwrite once the run has started.
    _sfm_job.device_selector = _native_device_uuid;
    // A settings file written by a build that HAS the inference layer must not
    // make a run on one that has not fail at the last step.
    _sfm_job.geometry.enable = _colmap_job.geometry.enable =
        _geometry.enable && geometry_availability().empty();
    // Saved presets supply the defaults for rows without color overrides.
    _colmap_job.image_gamut = _sfm_job.image_gamut;
    _colmap_job.image_is_linear = _sfm_job.image_is_linear;
    _sfm_job.geometry.image_gamut = _colmap_job.geometry.image_gamut =
        _sfm_job.image_gamut;
    _sfm_job.geometry.image_is_linear = _colmap_job.geometry.image_is_linear =
        _sfm_job.image_is_linear;
    _sfm_job.prep.image_gamut = _sfm_job.image_gamut;
    _sfm_job.prep.image_is_linear = _sfm_job.image_is_linear;
    _colmap_job.image_exposure = _sfm_job.geometry.image_exposure =
        _colmap_job.geometry.image_exposure = _sfm_job.prep.image_exposure =
            _sfm_job.image_exposure;
}

void GuiApp::update_dataset_job() {
    if (!dataset_busy()) { _dense_rebuild = false; return; }
    sync_dataset_jobs();
    if (_sfm.state() == SfmRunner::State::Running) _sfm.update(_sfm_job);
    else                                           _colmap.update(_colmap_job);
}

void GuiApp::poll_dataset_finish_command() {
    if (_pending_dataset_finish_command.empty() || dataset_busy()) return;
    const std::string command = std::exchange(_pending_dataset_finish_command, {});
    auto notify = [&](auto& runner) {
        using State = decltype(runner.state());
        const State state = runner.state();
        if (state == State::Done)
            run_finish_command(command, i18n::format(dmsg::finish_cmd_ready,
                                                      {runner.dataset_dir()}));
        else if (state == State::Failed)
            run_finish_command(command, i18n::format(dmsg::finish_cmd_failed,
                                                      {runner.error()}));
    };
    if (_dataset_run_builtin) notify(_sfm);
    else                      notify(_colmap);
}

void GuiApp::advance_dataset_training() {
    if (!_dataset_training.pending || dataset_busy()) return;
    _dataset_training.pending = false;
    const bool builtin = _dataset_run_builtin;
    const bool done = builtin ? _sfm.state() == SfmRunner::State::Done
                              : _colmap.state() == ColmapRunner::State::Done;
    if (!done || _batch_active || native_work_busy()) return;

    TrainPreset& preset = _dataset_training.preset;
    TrainConfig& cfg = preset.cfg;
    cfg.data = builtin ? _sfm.dataset_dir() : _colmap.dataset_dir();
    const std::string images = builtin ? _sfm.image_dir() : _colmap.image_dir();
    const std::string masks = builtin ? _sfm.mask_dir() : _colmap.mask_dir();
    if (!images.empty()) cfg.image_dir = images;
    if (!masks.empty()) {
        cfg.mask_dir = masks;
        cfg.flip_mask = builtin ? _sfm.mask_flipped() : _colmap.mask_flipped();
    }
    cfg.output_dir_prefix = (fs::path(cfg.data) / "outputs").string();
    if (_dense.enable && !_dense.use_for_training && cfg.seed_pointcloud.empty())
        cfg.seed_pointcloud = spirula::dense::kSparseSeed;

    _defaults = cfg;
    cfg.finish_command = _cfg.finish_command;   // as apply_preset
    _cfg = cfg;
    _cfg_ui.touched = preset.touched;
    _preset = preset.base;
    _train_presets.file = preset.path;
    _train_presets.display = preset.name;
    _train_presets.desc.clear();
    _train_presets.msg.clear();
    _parse_dirty = false;
    remember_dir("dataset", cfg.data);
    remember(RecentKind::Dataset, cfg.data);
    reset_dataset_preview();
    close_source_preview();
    go_to(Screen::Train);
    launch_training(_cfg, _preset);
}

const DatasetPlan& GuiApp::dataset_plan() {
    sync_dataset_jobs();
    const WorkspaceState& ws = workspace_state();
    if (effective_engine() == Engine::BuiltIn)
        _plan = plan_dataset(plan_job(_sfm_job), ws, _ws_record, _sfm_job.request);
    else
        _plan = plan_dataset(plan_job(_colmap_job, ColmapRunner::prep_job(_colmap_job)),
                             ws, _ws_record, _colmap_job.request);
    return _plan;
}

void GuiApp::start_dataset_job() {
    if (native_work_busy()) return;
    pump_source_probes();
    if (!_source_probes_ready) {
        log(dmsg::sensors_reading.get());
        return;
    }
    _ws_state_at = -1.0;
    const DatasetPlan& plan = dataset_plan();
    if (plan.ask()) {
        _rebuild_plan = plan;
        _rebuild_open = true;
        return;
    }
    launch_dataset_job();
}

bool GuiApp::launch_dataset_job() {
    // Device choice must freeze before any preview, decode or child process starts.
    if (native_work_busy()) return false;
    poll_dataset_finish_command();
    _ws_state_at = -1.0;
    if (const StepPlan& frames = dataset_plan()[Step::Frames];
        !_redo_frames && frames.ask && !frames.can_keep) {
        log(dmsg::rerun_section_help.get());
        return false;
    }
    _dataset_training.pending = false;
    _dataset_training.error.clear();
    if (!_batch_active && _dataset_training.enabled) {
        BatchRow row;
        row.runs.push_back(_dataset_training.run);
        TrainPreset& preset = _dataset_training.preset;
        if (!batch_build_train_config(row, 0, _workspace, {}, {}, false,
                                      preset.cfg, preset.base, _dataset_training.error,
                                      &preset.touched)) {
            log(_dataset_training.error);
            return false;
        }
        _dataset_training.error = spirula::train_config_error(preset.cfg);
        if (!_dataset_training.error.empty()) {
            log(_dataset_training.error);
            return false;
        }
        preset.path = _dataset_training.run.preset.path;
        preset.name = _dataset_training.run.preset.name;
    }
    stop_inference_users();
    close_splat();
    if (!freeze_native_device()) return false;
    app::set_crash_note("building dataset " + _workspace);
    // The runner takes the dense settings when that step starts, after the request is forgotten.
    _dense_rebuild = _redo_dense;
    sync_dataset_jobs();
    save_run_stencils();
    {
        const DatasetPlan& plan = dataset_plan();
        bool made[kNumSteps];
        for (int k = 0; k < kNumSteps; k++) made[k] = makes(plan[(Step)k].act);
        // Sorting leaves the frames as they were made but is what they are
        // now: the rule's settings come back with the dataset.
        made[(int)Step::Frames] = made[(int)Step::Frames] || plan[Step::Frames].sort;
        write_record_settings(_workspace, dataset_settings_json(capture_dataset_settings()),
                              encode_record_inputs({_sources, _mask.clicks}, _workspace), made);
    }
    if (!_workspace.empty()) {
        std::error_code ec;
        remember(RecentKind::Reconstruction, fs::absolute(fs::u8path(_workspace), ec).u8string());
    }
    _restored_ws = _workspace;
    const std::string stamp = runlog::file_stamp();
    const fs::path prep_log_file =
        runlog::open(_prep_log,
                     fs::path(planned_image_dir(_sources, _workspace,
                                                _photo_import)).parent_path().string(),
                     "prep", stamp);
    // The unregistered-images list, beside this log with the same stamp,
    // written by whichever engine runs (sfm/Pipeline.cpp or ColmapRunner).
    set_ss_env("UNREG_LOG", prep_log_file.empty()
                                ? std::string()
                                : (prep_log_file.parent_path() /
                                   ("unreg_" + stamp + ".log")).string());
    write_run_settings(_prep_log);
    // The preview holds a multi-gigabyte backbone; the run about to start
    // wants that VRAM for reconstruction.
    reset_dataset_preview(effective_engine() != Engine::BuiltIn);
    const RunFilms films{&_film_frames, &_film_masks, &_film_geometry, &_picks};
    // An earlier run's picks would otherwise be the only view with anything in
    // it for the first moments of this one, and the screen would open on them.
    // A run that keeps this folder's frames keeps what was measured on them.
    if (makes(dataset_plan()[Step::Frames].act) || dataset_plan()[Step::Frames].sort ||
        _picks_workspace != _workspace) {
        _picks.clear();
        _picks_view.destroy_gl();
        _picks_workspace = _workspace;
    }
    _dataset_run_builtin = effective_engine() == Engine::BuiltIn;
    _sfm_job.prep.review_frames = !_batch_active && _dataset_run_builtin &&
                                  _sfm_job.prep.skip_blurry && _sfm_job.prep.blur_review;
    _pending_dataset_finish_command = _batch_active ? std::string() : _dataset_finish_command;
    _dataset_training.pending = !_batch_active && _dataset_training.enabled;
    if (_dataset_run_builtin) _sfm.start(_sfm_job, films);
    else                      _colmap.start(_colmap_job, films);
    // A run is what the column is for while it goes.
    _right_input = false;
    // One run each: a re-do that stayed armed would throw the same step away
    // again the next time the button is pressed.
    forget_redo_requests();
    return true;
}

}  // namespace gui
