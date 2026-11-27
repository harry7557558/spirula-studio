// Running the batch queue: checking rows, fetching models, and launching each
// task. The queue is BatchProcess.h; this drives the three live runners rather
// than reimplementing them, so a batch shows up on their screens as a run does.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/Subprocess.h"
#include "checkpoint/SplatMerge.h"
#include "core/LicenseConsent.h"
#include "core/LicenseFamilies.h"
#include "data/RoiDocument.h"
#include "data/ScenePartition.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/Partition.h"

#include "imgui.h"

#include <algorithm>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace dmsg = spirula::i18n::msg::dataset;
namespace lmsg = spirula::i18n::msg::log;

namespace gui {

// Append a row for `dataset`, seeded with whatever preset the trainer screen
// is on -- a queue is usually built right after tuning the settings it should
// run with. Shared by the picker, the recents menu and drag-and-drop.
BatchRun GuiApp::batch_run_on_screen(const std::string& dataset) const {
    BatchRun run;
    run.preset.path = _train_presets.file;
    run.preset.name =
        _train_presets.file.empty() ? _preset : _train_presets.display;
    if (!dataset.empty() && dataset == _cfg.data) run.roi_region = _cfg.roi_region;
    return run;
}

void GuiApp::add_batch_row(const std::string& dataset) {
    if (dataset.empty()) return;
    BatchRow r;
    r.dataset = dataset;
    r.does(BatchStage::Train) = true;
    r.runs.push_back(batch_run_on_screen(dataset));
    _batch.push_back(std::move(r));
    _batch_open_row = (int)_batch.size() - 1;
    batch_edited();
}

void GuiApp::add_batch_source_row(const std::vector<std::string>& sources) {
    if (sources.empty()) return;
    BatchRow r;
    r.sources = sources;
    // Building a dataset and then training it is the whole reason a row can
    // do more than one thing; the checkboxes still say so, and still undo it.
    r.does(BatchStage::Dataset) = true;
    r.does(BatchStage::Train) = true;
    r.dataset_preset.path = _ds_presets.file;
    r.dataset_preset.name =
        _ds_presets.file.empty() ? _ds_presets.builtin : _ds_presets.display;
    r.runs.push_back(batch_run_on_screen());
    _batch.push_back(std::move(r));
    _batch_open_row = (int)_batch.size() - 1;
    batch_edited();
}

void GuiApp::add_batch_mesh_row(const std::string& model) {
    if (model.empty()) return;
    BatchRow r;
    r.model = model;
    r.does(BatchStage::Train) = false;
    r.does(BatchStage::Mesh) = true;
    r.mesh.preset.path = _mesh_presets.file;
    r.mesh.preset.name = _mesh_presets.display;
    _batch.push_back(std::move(r));
    _batch_open_row = (int)_batch.size() - 1;
    batch_edited();
}

void GuiApp::batch_edited() {
    _batch_save.edited();
    _batch_checked = false;
}

void GuiApp::save_batch() {
    if (!_batch_save.save(_batch)) log(i18n::format(emsg::save_failed, {_batch_save.error()}));
}

BatchCapabilities GuiApp::batch_capabilities() const {
    BatchCapabilities caps;
    caps.device = backend::device_count() > 0 && backend::device_current() >= 0;
    caps.builtin_sfm = builtin_sfm_available();
    caps.colmap = colmap_available();
    caps.masking = backends().builtin_masking;
    caps.geometry = geometry_availability().empty();
    caps.mask_model_ready = [](const std::string& id, const std::string& detector_id) {
        return !cached_mask_model(id, detector_id).empty();
    };
    caps.mask_model_prompted = [](const std::string& id) {
        const ModelEntry* e = find_model(id);
        return !e || e->kind != MaskModelKind::Subject;
    };
    caps.geometry_model_ready = [](const std::string& id) {
        return geometry_model_cached(id);
    };
    // A local checkpoint path is the dense step's own to report.
    caps.dense_model_ready = [](const std::string& checkpoint) {
        return checkpoint != dense_model_entry().id || model_is_cached(dense_model_entry());
    };
    return caps;
}

void GuiApp::check_batch() {
    const BatchCapabilities caps = batch_capabilities();
    for (int i = 0; i < (int)_batch.size(); i++)
        _batch[i].issues = batch_check_row(_batch[i], _batch, i, caps);
    _batch_checked = true;
    _batch_checked_at = ImGui::GetTime();
}

// Nothing waits for a button: an edit re-checks at once, and a timer catches
// a preset file or a dataset folder that went missing while the screen was
// up. A few stats and a small JSON per preset, so the cost is noise.
void GuiApp::check_batch_if_stale() {
    if (_batch_active) return;
    const double now = ImGui::GetTime();
    if (_batch_checked && _batch_checked_at >= 0.0 && now - _batch_checked_at < 2.0)
        return;
    check_batch();
}

void GuiApp::request_start_batch(bool skip_invalid) {
    // A reconstruction or a meshing child owns the device and has no
    // stop-and-do-this-instead confirmation; only training does.
    if (dataset_busy() || _mesh.busy()) {
        _batch_msg = msg::batch_busy_elsewhere.get();
        _batch_msg_err = true;
        return;
    }
    if (training_busy()) {
        _pending = Pending::StartBatch;
        _pending_then = [this, skip_invalid] { begin_batch(skip_invalid); };
        _open_confirm = true;
        return;
    }
    if (native_work_busy()) return;
    begin_batch(skip_invalid);
}

// Every checkpoint the enabled rows read, over what is on disk now.
GuiApp::BatchFetchPlan GuiApp::batch_fetch_plan() const {
    BatchFetchPlan plan;
    std::vector<std::string> lists;
    for (const BatchRow& row : _batch) {
        BatchModelNeeds n;
        if (!batch_model_needs(row, n)) continue;
        if (n.mask) {
            const ModelEntry* e = find_model(n.mask_model_id);
            const auto pick = std::make_pair(n.mask_model_id, n.mask_detector_id);
            if (e && cached_mask_model(pick.first, pick.second).empty() &&
                std::find(plan.masks.begin(), plan.masks.end(), pick) == plan.masks.end()) {
                plan.masks.push_back(pick);
                const TextDetector* d = detector_for(*e, pick.second);
                if (!model_is_cached(*e)) lists.push_back(e->family);
                if (d && !detector_is_cached(*d)) lists.push_back(spirula::license::family::kGdino);
            }
        }
        if (n.geometry && !geometry_model_cached(n.geometry_model) &&
            std::find(plan.geometries.begin(), plan.geometries.end(), n.geometry_model) ==
                plan.geometries.end()) {
            plan.geometries.push_back(n.geometry_model);
            for (const PendingDownload& d : geometry_model_downloads(n.geometry_model))
                lists.push_back(d.license_family);
        }
        for (size_t i = 0; i < _batch_fetchers.size(); ++i)
            if (std::find(plan.fetchers.begin(), plan.fetchers.end(), i) == plan.fetchers.end() &&
                _batch_fetchers[i].due(n, lists))
                plan.fetchers.push_back(i);
    }
    for (const std::string& f : spirula::license::unique_families(lists))
        if (!license_accepted(f)) plan.families.push_back(f);
    return plan;
}

// The one place a batch can ask a question: here, before anything has run.
void GuiApp::begin_batch(bool skip_invalid) {
    const BatchFetchPlan plan = batch_fetch_plan();
    if (plan.empty()) {
        start_batch(skip_invalid);
        return;
    }
    _license_notice.clear();
    request_licenses(
        plan.families,
        [this, plan, skip_invalid] {
            _batch_fetch = plan;
            _batch_fetch_skip = skip_invalid;
            _batch_fetching = true;
        },
        [this] {
            _batch_msg = dmsg::batch_licence_declined.get();
            _batch_msg_err = true;
            _license_notice = _batch_msg;
            log(_batch_msg);
        });
}

// Steps the fetches one at a time, then starts the batch. A model that failed to
// arrive is not handled here: start_batch's own pre-flight names it per row.
void GuiApp::pump_batch_fetch() {
    if (!_batch_fetching) return;
    FileDownload* running = nullptr;
    if (_download.state() == FileDownload::State::Running || !_download_model_id.empty())
        running = &_download;
    else if (_geom_download.running() || _geom_download.pending())
        running = &_geom_download.current();
    for (const BatchFetcher& f : _batch_fetchers)
        if (!running) running = f.active();
    if (running) {
        _batch_msg = running->status();
        _batch_msg_err = false;
        return;
    }
    BatchFetchPlan& p = _batch_fetch;
    while (!p.masks.empty()) {
        const auto pick = p.masks.front();
        p.masks.erase(p.masks.begin());
        const ModelEntry* e = find_model(pick.first);
        if (e && _download.start(*e, detector_for(*e, pick.second))) {
            _download_model_id = pick.first;
            _download_detector_id = pick.second;
            return;
        }
    }
    if (!p.geometries.empty()) {
        const std::string id = p.geometries.front();
        p.geometries.erase(p.geometries.begin());
        _geom_download.start(geometry_model_downloads(id));
        return;
    }
    if (!p.fetchers.empty()) {
        const size_t i = p.fetchers.front();
        p.fetchers.erase(p.fetchers.begin());
        _batch_fetchers[i].start();
        return;
    }
    _batch_fetching = false;
    _batch_msg.clear();
    start_batch(_batch_fetch_skip);
}

void GuiApp::start_batch(bool skip_invalid) {
    check_batch();

    int bad = 0, runnable = 0;
    for (const BatchRow& r : _batch) {
        if (!r.enabled) continue;
        (batch_has_error(r.issues) ? bad : runnable)++;
    }
    if (bad > 0 && !skip_invalid) {
        _batch_msg = i18n::format(msg::batch_blocked, {(long long)bad});
        _batch_msg_err = true;
        return;
    }
    if (runnable == 0) {
        _batch_msg = msg::batch_no_runnable.get();
        _batch_msg_err = true;
        return;
    }
    // Frozen once for the whole queue, so no task halfway down it can be
    // refused the device the ones before it ran on.
    if (!freeze_native_device() || !freeze_cuda_device()) {
        _batch_msg = _native_device_error.empty() ? msg::no_device_found.get()
                                                  : _native_device_error;
        _batch_msg_err = true;
        return;
    }

    _batch_tasks = batch_plan(_batch);
    for (BatchTask& t : _batch_tasks)
        if (batch_has_error(_batch[(size_t)t.row].issues))
            t.status = BatchStatus::Skipped;
    save_batch();

    _batch_active = true;
    _batch_launched = false;
    _batch_current = -1;
    _batch_queue_clock.start(progress_now());
    _batch_clock_task = -1;
    _batch_stop_after = false;
    _batch_stop_now = false;
    int todo = 0;
    for (const BatchTask& t : _batch_tasks)
        todo += t.status == BatchStatus::Pending ? 1 : 0;
    _batch_msg = i18n::format(msg::batch_log_started, {(long long)todo});
    _batch_msg_err = false;
    log(_batch_msg);
}

bool GuiApp::batch_stage_busy(BatchStage stage) const {
    switch (stage) {
        case BatchStage::Dataset: return dataset_busy();
        case BatchStage::Mesh:    return _mesh.busy();
        case BatchStage::Merge:   return _merge_busy.load();
        case BatchStage::Train: {
            const TrainRunner::Phase ph = _runner.phase();
            return ph == TrainRunner::Phase::Preparing ||
                   ph == TrainRunner::Phase::Training ||
                   ph == TrainRunner::Phase::Evaluating;
        }
    }
    return false;
}

const BatchTask* GuiApp::batch_task_of(int row, BatchStage stage,
                                       int variant) const {
    for (const BatchTask& t : _batch_tasks)
        if (t.row == row && t.stage == stage && t.variant == variant) return &t;
    return nullptr;
}

void GuiApp::fail_batch_task(BatchTask& task, const std::string& error) {
    task.status = BatchStatus::Failed;
    task.message = error;
    log(i18n::format(msg::batch_log_job_failed,
                     {(long long)(_batch_current + 1), error}));
}

// What became of the task that was running, read off the runner that owned it.
void GuiApp::record_batch_task() {
    if (_batch_current < 0 || _batch_current >= (int)_batch_tasks.size()) return;
    BatchTask& t = _batch_tasks[(size_t)_batch_current];
    const long long n = _batch_current + 1;
    t.seconds = std::max(0.0, ImGui::GetTime() - t.started_at);

    bool ok = false, cancelled = false;
    std::string error;
    switch (t.stage) {
        case BatchStage::Dataset: {
            const bool builtin = effective_engine() == Engine::BuiltIn;
            ok = builtin ? _sfm.state() == SfmRunner::State::Done
                         : _colmap.state() == ColmapRunner::State::Done;
            cancelled = builtin
                            ? _sfm.state() == SfmRunner::State::Cancelled
                            : _colmap.state() == ColmapRunner::State::Cancelled;
            error = builtin ? _sfm.error() : _colmap.error();
            if (ok) {
                t.result = builtin ? _sfm.dataset_dir() : _colmap.dataset_dir();
                t.image_dir = builtin ? _sfm.image_dir() : _colmap.image_dir();
                t.mask_dir = builtin ? _sfm.mask_dir() : _colmap.mask_dir();
                t.mask_flipped = builtin ? _sfm.mask_flipped()
                                         : _colmap.mask_flipped();
                // The rest of the row trains and meshes this folder, so the
                // row remembers it even for a re-run started later.
                if (t.row < (int)_batch.size() && _batch[(size_t)t.row].dataset.empty()) {
                    _batch[(size_t)t.row].dataset = t.result;
                    _batch_save.edited();
                }
            }
            break;
        }
        case BatchStage::Train: {
            ok = _runner.phase() == TrainRunner::Phase::Done && !_batch_stop_now;
            error = _runner.error();
            if (ok) {
                t.steps = _runner.latest_progress().step + 1;
                if (auto* s = _runner.session()) t.result = s->out_dir.string();
                // Here as well as in frame(): the next task starts in the same
                // frame, so Done may never be seen there.
                std::error_code ec;
                if (!t.result.empty())
                    remember(RecentKind::Run, fs::absolute(fs::u8path(t.result), ec).u8string());
            }
            break;
        }
        case BatchStage::Mesh: {
            ok = _mesh.state() == MeshRunner::State::Done;
            cancelled = _mesh.state() == MeshRunner::State::Cancelled;
            error = _mesh.error();
            if (ok) t.result = _mesh.output_path();
            break;
        }
        case BatchStage::Merge: {
            if (_merge_thread.joinable()) _merge_thread.join();
            ok = _merge_error.empty();
            error = _merge_error;
            if (ok) t.result = _merge_result;
            if (ok && !_batch_stop_now) _open_after_batch = _merge_result;
            break;
        }
    }

    if (ok) {
        t.status = BatchStatus::Done;
        log(i18n::format(msg::batch_log_job_done, {n, t.result}));
    } else if (_batch_stop_now || cancelled) {
        t.status = BatchStatus::Stopped;
        log(i18n::format(msg::batch_log_job_stopped, {n}));
    } else {
        // Anything the pipeline threw: an unreadable dataset, an OOM, a driver
        // fault. Recorded and left behind -- the point of a queue is that the
        // next row still gets its turn.
        t.status = BatchStatus::Failed;
        t.message = error;
        log(i18n::format(msg::batch_log_job_failed, {n, error}));
    }
}

bool GuiApp::launch_batch_dataset(BatchTask& task, const BatchRow& row) {
    std::vector<PrepInput> sources;
    DatasetSettings settings;
    std::string workspace, error;
    if (!batch_build_dataset_job(row, _ffmpeg_exe, sources, settings, workspace,
                                 error)) {
        fail_batch_task(task, error);
        return false;
    }
    // The screen is driven rather than bypassed: one implementation of "start
    // a dataset run", and whoever is watching sees what is running.
    detach_session_views();
    close_splat();
    // A reconstruction brings up a Vulkan device of its own and wants the
    // VRAM the last training run is still holding.
    _runner.release_engine();
    clear_lidar_sources();
    _sources = sources;
    _workspace = _workspace_auto = workspace;
    apply_dataset_settings(settings);
    _sources = sources;          // rescan_found_masks() re-derived the list
    _source_path_edits.clear();  // re-seeded from _sources by the next draw
    forget_redo_requests();
    _keep_built = false;
    // The row's settings are the run's; the folder's record must not replace them.
    _restored_ws = workspace;
    _ws_state_at = -1.0;
    if (const StepPlan& frames = dataset_plan()[Step::Frames]; frames.ask && !frames.can_keep) {
        fail_batch_task(task, dmsg::rerun_section_help.get());
        return false;
    }
    log(i18n::format(msg::batch_log_build,
                     {(long long)(_batch_current + 1), workspace}));
    follow_batch_screen(BatchStage::Dataset);
    if (!launch_dataset_job()) {
        fail_batch_task(task, _native_device_error);
        return false;
    }
    return true;
}

bool GuiApp::launch_batch_train(BatchTask& task, const BatchRow& row) {
    // What this row produced upstream, or what it was pointed at.
    std::string dataset = row.dataset, image_dir, mask_dir;
    bool flipped = false;
    if (const BatchTask* made = batch_task_of(task.row, BatchStage::Dataset, 0)) {
        if (made->status != BatchStatus::Done) {
            task.status = BatchStatus::Skipped;
            log(i18n::format(msg::batch_log_task_skipped,
                             {(long long)(_batch_current + 1)}));
            return false;
        }
        dataset = made->result;
        image_dir = made->image_dir;
        mask_dir = made->mask_dir;
        flipped = made->mask_flipped;
    }

    if (image_dir.empty() && !row.image_dir.empty()) {
        image_dir = row.image_dir;
        mask_dir = row.mask_dir;
        flipped = row.mask_flipped;
    }
    TrainConfig cfg;
    std::string base, error;
    if (!batch_build_train_config(row, task.variant, dataset, image_dir, mask_dir,
                                  flipped, cfg, base, error)) {
        fail_batch_task(task, error);
        return false;
    }
    if (const BatchTask* made = batch_task_of(task.row, BatchStage::Dataset, 0);
        made && made->status == BatchStatus::Done && _dense.enable && !_dense.use_for_training &&
        cfg.seed_pointcloud.empty())
        cfg.seed_pointcloud = spirula::dense::kSparseSeed;
    log(i18n::format(msg::batch_log_train,
                     {(long long)(_batch_current + 1), dataset}));
    follow_batch_screen(BatchStage::Train);
    // A run's own dataset options must not be re-parsed under it by a stale
    // edit made on the trainer screen before the queue started.
    _parse_dirty = false;
    launch_training(cfg, base);
    return true;
}

bool GuiApp::launch_batch_mesh(BatchTask& task, const BatchRow& row) {
    std::string model = row.model, dataset = row.dataset;
    if (const BatchTask* made = batch_task_of(task.row, BatchStage::Dataset, 0))
        if (made->status == BatchStatus::Done) dataset = made->result;
    if (model.empty()) {
        const BatchTask* run = batch_task_of(task.row, BatchStage::Train,
                                             task.variant);
        if (!run || run->status != BatchStatus::Done) {
            task.status = BatchStatus::Skipped;
            log(i18n::format(msg::batch_log_task_skipped,
                             {(long long)(_batch_current + 1)}));
            return false;
        }
        model = run->result;
    }

    MeshJob job;
    std::string error;
    if (!batch_build_mesh_job(row, model, dataset, job, error)) {
        fail_batch_task(task, error);
        return false;
    }
    log(i18n::format(msg::batch_log_mesh,
                     {(long long)(_batch_current + 1), model}));
    follow_batch_screen(BatchStage::Mesh);
    // The engine has to be free: the mesh child wants the VRAM, and both the
    // last preview and the run that trained this model are holding it.
    stop_inference_users();
    close_mesh_preview();
    close_splat();
    _runner.release_engine();
    job.device_uuid = _native_device_uuid;
    job.cuda_device = _cuda_device_index;
    _mesh_job = job;
    _mesh_shown_run = _mesh.run_id();   // a batch opens no preview
    _mesh.start(job);
    return true;
}

bool GuiApp::launch_batch_task(BatchTask& task) {
    const BatchRow& row = _batch[(size_t)task.row];
    task.started_at = ImGui::GetTime();
    try {
        switch (task.stage) {
            case BatchStage::Dataset: return launch_batch_dataset(task, row);
            case BatchStage::Train:   return launch_batch_train(task, row);
            case BatchStage::Mesh:    return launch_batch_mesh(task, row);
            case BatchStage::Merge:   return launch_batch_merge(task, row);
        }
    } catch (const std::exception& e) {
        // Nothing below here is supposed to throw, and a queue that dies on
        // one row that does is the failure this whole screen exists to avoid.
        fail_batch_task(task, e.what());
    }
    return false;
}

void GuiApp::advance_batch() {
    if (!_batch_active) return;

    if (_batch_launched) {
        if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size() &&
            batch_stage_busy(_batch_tasks[(size_t)_batch_current].stage))
            return;   // still going
        record_batch_task();
        _batch_launched = false;
        _batch_current = -1;
        if (_batch_stop_after) { finish_batch(); return; }
    }

    // The next task, skipping the ones whose input never arrived. Nothing is
    // started on a pass that skips, so the loop is bounded by the task count.
    for (;;) {
        int next = -1;
        for (int i = 0; i < (int)_batch_tasks.size(); i++)
            if (_batch_tasks[(size_t)i].status == BatchStatus::Pending) {
                next = i;
                break;
            }
        if (next < 0) { finish_batch(); return; }
        _batch_current = next;
        BatchTask& t = _batch_tasks[(size_t)next];
        if (t.row < 0 || t.row >= (int)_batch.size()) {
            t.status = BatchStatus::Skipped;
            continue;
        }
        if (launch_batch_task(t)) {
            t.status = BatchStatus::Running;
            _batch_launched = true;
            return;
        }
        // launch_batch_task always recorded an outcome, so the next pass
        // picks a different task.
    }
}

void GuiApp::finish_batch() {
    int done = 0, failed = 0, other = 0;
    for (const BatchTask& t : _batch_tasks) {
        if (t.status == BatchStatus::Done) done++;
        else if (t.status == BatchStatus::Failed) failed++;
        else other++;
    }
    // A row that ran through is left unticked: re-running the list should
    // not redo it, and "clear done rows" is what removes it.
    for (int i = 0; i < (int)_batch.size(); i++)
        if (_batch[(size_t)i].enabled && batch_row_done(i)) {
            _batch[(size_t)i].enabled = false;
            _batch[(size_t)i].done = true;
            _batch_save.edited();
        }
    _batch_active = false;
    _batch_launched = false;
    _batch_current = -1;
    _batch_stop_after = false;
    _batch_stop_now = false;
    _batch_msg = i18n::format(msg::batch_log_summary,
                              {(long long)done, (long long)failed,
                               (long long)other});
    _batch_msg_err = failed > 0;
    log(_batch_msg);
    if (_batch_save.dirty()) save_batch();
    run_batch_command(i18n::format(msg::batch_cmd_message,
                                   {(long long)done, (long long)failed,
                                    (long long)other}));
    // What the queue made last is worth looking at, unless whoever is here
    // is already looking at a model of their own.
    std::string merged;
    merged.swap(_open_after_batch);
    if (!merged.empty() && _screen != Screen::Viewer) open_splat(merged);
}

// The whole point of an unattended queue is not watching it, so the one thing
// it can do on its own behalf is tell somebody it is over. Runs however the
// queue ended -- finished, failed or stopped; the message says which.
void GuiApp::run_batch_command(const std::string& message) {
    run_finish_command(_batch_cmd, message);
}

void GuiApp::run_finish_command(const std::string& command,
                                const std::string& message) {
    if (command.empty()) return;
    const std::vector<std::string> argv =
        command_argv(command, kMessageToken, message);
    if (argv.empty()) return;
    log(i18n::format(lmsg::finish_cmd_running, {argv.front()}));
    if (!_finish_cmd_run.start(argv)) log(lmsg::finish_cmd_busy.get());
}

// What it printed and how it ended, in the log like any other runner's.
void GuiApp::poll_finish_command() {
    for (const std::string& s : _finish_cmd_run.drain_log()) log(s);
    const int code = _finish_cmd_run.take_exit_code();
    if (code == CommandRunner::kNoResult || code == kCancelled) return;
    if (code == kSpawnFailed)
        log(i18n::format(lmsg::finish_cmd_missing, {_finish_cmd_run.program()}));
    else if (code != 0)
        log(i18n::format(lmsg::finish_cmd_exit, {(long long)code}));
    else
        log(lmsg::finish_cmd_ok.get());
}

void GuiApp::cancel_batch() {
    if (!_batch_active) return;
    if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size())
        _batch_tasks[(size_t)_batch_current].status = BatchStatus::Stopped;
    _batch_launched = false;
    _batch_current = -1;
    finish_batch();
}

// The screen a running stage belongs to, unless whoever is here has walked off
// to the list or the viewer to look at something else.
void GuiApp::follow_batch_screen(BatchStage stage) {
    if (_screen == Screen::Batch || _screen == Screen::Viewer) return;
    switch (stage) {
        case BatchStage::Dataset: follow_to(Screen::NewDataset); break;
        case BatchStage::Train:   follow_to(Screen::Train); break;
        case BatchStage::Mesh:    follow_to(Screen::Mesh); break;
        case BatchStage::Merge:   follow_to(Screen::Batch); break;
    }
}

bool GuiApp::launch_batch_merge(BatchTask& task, const BatchRow& row) {
    namespace pmsg = spirula::i18n::msg::partition;
    if (_merge_thread.joinable()) _merge_thread.join();
    const std::string partition = row.partition;
    const std::string outputs = row.output_dir.empty()
                                    ? (fs::path(row.dataset).parent_path() / fs::path(row.dataset).filename() / "outputs").string()
                                    : row.output_dir;
    _merge_result.clear();
    _merge_error.clear();
    _merge_busy = true;
    follow_batch_screen(BatchStage::Merge);
    log(i18n::format(pmsg::batch_log_merge, {(long long)(_batch_current + 1), partition}));
    _merge_thread = std::thread([this, partition, outputs] {
        try {
            std::string dataset;
            const spirula::ScenePartition p = spirula::read_partition(partition, &dataset);
            const std::vector<std::string> runs =
                spirula::find_partition_runs(outputs, partition, p.num_parts);
            int found = 0;
            for (const std::string& r : runs) found += !r.empty();
            if (found == 0) throw std::runtime_error(pmsg::err_nothing_to_merge.get());
            spirula::MergeStats st;
            char stamp[32];
            const std::time_t now = std::time(nullptr);
            std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
            const std::string stem = fs::path(dataset.empty() ? fs::path(partition).parent_path().string() : dataset).filename().string();
            std::error_code ec;
            fs::create_directories(outputs, ec);
            const std::string out = (fs::path(outputs) / (stem + "_merged_" + stamp + ".ply")).string();
            spirula::merge_partition_splats_file(p, runs, out, st);
            _merge_result = out;
        } catch (const std::exception& e) {
            _merge_error = e.what();
        }
        _merge_busy = false;
    });
    (void)task;
    return true;
}

}  // namespace gui
