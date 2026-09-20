// TrainRunner.cpp -- see TrainRunner.h.

#include "app/gui/TrainRunner.h"

#include "backend/api/BackendRuntime.h"
#include "engine/Engine.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/Gui.h"
#include "data/JsonWrite.h"
#include "config/TrainConfigJson.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <stdexcept>

namespace gui {

using spirula::TrainerSession;

void TrainRunner::push_log(const std::string& s) {
    std::lock_guard<std::mutex> lk(_mu);
    _log.push_back(s);
    if (_log.size() > 5000) _log.erase(_log.begin(), _log.begin() + 1000);
}

std::vector<std::string> TrainRunner::drain_log() {
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<std::string> out;
    out.swap(_log);
    return out;
}

std::string TrainRunner::error() {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}

spirula::TrainerProgress TrainRunner::latest_progress() {
    std::lock_guard<std::mutex> lk(_mu);
    return _latest;
}

double TrainRunner::avg_latency_locked() const {
    if (_latencies.empty()) return -1.0;
    double sum = 0.0;
    for (double v : _latencies) sum += v;
    return sum / (double)_latencies.size();
}

double TrainRunner::avg_step_latency() {
    std::lock_guard<std::mutex> lk(_mu);
    return avg_latency_locked();
}

double TrainRunner::eta_seconds() {
    std::lock_guard<std::mutex> lk(_mu);
    const double avg = avg_latency_locked();
    if (avg < 0.0 || _latest.total_steps <= 0) return -1.0;
    return avg * std::max(0, _latest.total_steps - (_latest.step + 1));
}

double TrainRunner::elapsed_seconds() {
    if (!_session) return -1.0;
    return _session->elapsed_seconds();
}

void TrainRunner::get_metrics(std::vector<MetricPoint>& out) {
    std::lock_guard<std::mutex> lk(_mu);
    out = _metrics;
}

void TrainRunner::join_worker() {
    if (_worker.joinable()) _worker.join();
}

void TrainRunner::set_paused(bool p) {
    if (_session) _session->paused = p;
}

bool TrainRunner::paused() const {
    return _session && _session->paused.load();
}

void TrainRunner::request_stop(bool save) {
    if (!_session) return;
    if (!save) _session->save_on_stop = false;
    _session->stop_requested = true;
    _data_cv.notify_all();   // the step loop may be parked on a file error
}

void TrainRunner::shutdown() {
    request_stop();
    join_worker();
    if (_snapshot_worker.joinable()) _snapshot_worker.join();
    if (_web_viewer) { _web_viewer->stop(); _web_viewer.reset(); }
}

void TrainRunner::note_engine_taken() {
    if (_snapshot_worker.joinable()) _snapshot_worker.join();
    _engine_ready = false;
}

std::string TrainRunner::snapshot_message() {
    std::lock_guard<std::mutex> lk(_mu);
    return _snapshot_message;
}

void TrainRunner::export_snapshot(const SnapshotExport& snapshot) {
    if (!_session || !engine_ready() || snapshot_busy()) return;
    const std::time_t requested_at = std::time(nullptr);
    if (_snapshot_worker.joinable()) _snapshot_worker.join();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _snapshot_message.clear();
    }
    TrainerSession* session = _session.get();
    _snapshot_busy = true;
    session->snapshot_pending = true;
    try {
        _snapshot_worker = std::thread([this, session, snapshot, requested_at] {
            struct Finish {
                std::atomic<bool>& pending;
                std::atomic<bool>& busy;
                ~Finish() { pending = false; busy = false; }
            } finish{session->snapshot_pending, _snapshot_busy};
            std::string message;
            namespace fs = std::filesystem;
            namespace msg = spirula::i18n::msg::gui;
            fs::path staging;
            fs::path partial;
            fs::path metadata_partial;
            fs::path metadata_target;
            bool metadata_published = false;
            try {
                std::lock_guard<std::mutex> engine_lock(session->engine_mutex);
                const int step = session->cur_step.load();
                fs::path dataset_dir = fs::absolute(fs::u8path(session->cfg.data)).lexically_normal();
                if (!fs::is_directory(dataset_dir))
                    throw std::runtime_error("Snapshot: dataset directory is unavailable");
                if (dataset_dir.filename().empty() && dataset_dir.has_relative_path())
                    dataset_dir = dataset_dir.parent_path();
                const fs::path dataset_name = dataset_dir.filename().empty()
                    ? fs::path("dataset") : dataset_dir.filename();
                std::tm local_tm{};
#ifdef _WIN32
                if (localtime_s(&local_tm, &requested_at) != 0)
#else
                if (!localtime_r(&requested_at, &local_tm))
#endif
                    throw std::runtime_error("Snapshot: cannot format the local request time");
                char stamp[32]{};
                if (std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &local_tm) == 0)
                    throw std::runtime_error("Snapshot: cannot format the local request time");
                fs::path target;
                for (int attempt = 0;; ++attempt) {
                    const std::string suffix = attempt == 0 ? "" : ("_" + std::to_string(attempt));
                    fs::path base = dataset_dir / dataset_name;
                    base += std::string("_") + stamp + suffix;
                    target = base;
                    target += ".ply";
                    metadata_target = base;
                    metadata_target += ".transform.json";
                    fs::path candidate = base;
                    candidate += ".snapshot-tmp";
                    // Atomic reservation also separates simultaneous app instances.
                    if (!fs::create_directory(candidate)) continue;
                    staging = candidate;
                    if (!fs::exists(target) && !fs::exists(metadata_target)) break;
                    fs::remove(staging);
                    staging.clear();
                }
                partial = staging / "splat.ply";
                metadata_partial = staging / "transform.json";
                engine_export_ply(partial.u8string(), &snapshot.transform);
                JsonWriter metadata;
                metadata.object().field("format", "spirula-snapshot-transform").field("version", 1)
                    .field("step", step).field("coordinates", snapshot_coordinates_name(snapshot.coordinates))
                    .field("requested_at_local", stamp).field("training_output", session->out_dir.u8string())
                    .field("convention", "p_snapshot = R * p_training + t; original scene units")
                    .key("rotation_row_major").array();
                for (double v : snapshot.transform.R) metadata.value(v);
                metadata.end().key("translation").array();
                for (double v : snapshot.transform.t) metadata.value(v);
                metadata.end().key("config").object();
                for (const auto& field : train_config_json_pairs(session->cfg))
                    metadata.field_raw(field.first, field.second);
                metadata.end().end();
                std::ofstream info(metadata_partial, std::ios::binary);
                info.exceptions(std::ios::failbit | std::ios::badbit);
                info << metadata.str();
                info.close();
                fs::rename(metadata_partial, metadata_target);
                metadata_published = true;
                fs::rename(partial, target);
                metadata_published = false;
                std::error_code ec;
                fs::remove(staging, ec);
                message = spirula::i18n::format(msg::snapshot_saved, {target.u8string()});
            } catch (const std::exception& e) {
                if (!partial.empty()) {
                    std::error_code ec;
                    fs::remove(partial, ec);
                }
                if (!metadata_partial.empty()) {
                    std::error_code ec;
                    fs::remove(metadata_partial, ec);
                }
                if (metadata_published) {
                    std::error_code ec;
                    fs::remove(metadata_target, ec);
                }
                if (!staging.empty()) {
                    std::error_code ec;
                    fs::remove(staging, ec);
                }
                message = spirula::i18n::format(msg::snapshot_failed, {e.what()});
            }
            {
                std::lock_guard<std::mutex> lk(_mu);
                _snapshot_message = message;
            }
            push_log(message);
        });
    } catch (const std::exception& e) {
        session->snapshot_pending = false;
        _snapshot_busy = false;
        std::lock_guard<std::mutex> lk(_mu);
        _snapshot_message = spirula::i18n::format(spirula::i18n::msg::gui::snapshot_failed, {e.what()});
    }
}

void TrainRunner::release_engine() {
    shutdown();
    _engine_ready = false;
    _session.reset();
    engine_reset();
}

void TrainRunner::load_dataset(const TrainConfig& cfg, const std::string& preset) {
    shutdown();
    _engine_ready = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _snapshot_message.clear();
    }
    _session.reset(new TrainerSession());
    _session->cfg = cfg;
    _session->preset = preset;
    _session->log_fn = [this](const std::string& s) { push_log(s); };
    _phase = Phase::Loading;
    TrainerSession* s = _session.get();
    _worker = std::thread([this, s] {
        try {
            s->check_config();
            s->load_dataset();
            _phase = Phase::Ready;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(_mu);
            _error = e.what();
            _phase = Phase::LoadError;
        }
    });
}

void TrainRunner::start_training(const TrainConfig& cfg, const std::string& preset) {
    shutdown();
    _engine_ready = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _snapshot_message.clear();
        _latest = {};
        _latencies.clear();
        _metrics.clear();
    }
    _session.reset(new TrainerSession());
    _session->cfg = cfg;
    _session->preset = preset;
    _session->log_fn = [this](const std::string& s) { push_log(s); };
    _phase = Phase::Preparing;
    TrainerSession* s = _session.get();
    _worker = std::thread([this, s] {
        try {
#ifndef SS_BACKEND_VULKAN
            // The CUDA runtime's current device is per thread; this worker
            // does all the engine work, so it re-applies the process-wide
            // selection before the first driver call.
            if (!backend::device_bind())
                throw std::runtime_error(
                    "could not make the selected GPU current on the training "
                    "thread; restart the application or choose another GPU");
#endif
            s->check_config();
            s->load_dataset();
            s->setup_engine();
            s->viewer_base_camera_size = viewer_upload_cameras(s->post);
            viewer_upload_grid(s->post);
            _engine_ready = true;

            // Optional web viewer alongside the native viewport.
            if (!s->cfg.disable_viewer) {
                _web_viewer.reset(new ViewerServer());
                _web_viewer->start("0.0.0.0", s->cfg.viewer_port,
                                   s->make_viewer_config(),
                                   s->make_viewer_hooks(), s->post);
                push_log(spirula::i18n::format(
                    spirula::i18n::msg::log::web_viewer_at,
                    {(long long)s->cfg.viewer_port}));
            }

            _phase = Phase::Training;
            spirula::TrainerCallbacks cb;
            cb.on_data_error = [this](const std::string& what) {
                return await_data_decision(what);
            };
            cb.on_step = [this](const spirula::TrainerProgress& p) {
                std::lock_guard<std::mutex> lk(_mu);
                _latest = p;
                _latencies.push_back(p.step_latency);
                if (_latencies.size() > 100) _latencies.pop_front();
                MetricPoint m;
                m.step = p.step;
                auto get = [&](const char* k) {
                    auto it = p.losses.find(k);
                    return it == p.losses.end() ? 0.0f : it->second;
                };
                m.psnr = get("psnr");
                m.ssim = get("ssim");
                m.rgb_loss = get("rgb_loss");
                m.num_splats = (float)p.num_splats;
                _metrics.push_back(m);
            };
            s->train(cb);
            _phase = Phase::Done;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(_mu);
            _error = e.what();
            _phase = Phase::TrainError;
        }
    });
}

bool TrainRunner::await_data_decision(const std::string& what) {
    std::unique_lock<std::mutex> lk(_data_mu);
    _data_err    = what;
    _data_answer = 0;
    _data_cv.wait(lk, [&]{
        return _data_answer != 0 ||
               (_session && _session->stop_requested.load());
    });
    _data_err.clear();
    return _data_answer == 1;
}

std::string TrainRunner::data_error() {
    std::lock_guard<std::mutex> lk(_data_mu);
    return _data_err;
}

void TrainRunner::resolve_data_error(bool retry) {
    {
        std::lock_guard<std::mutex> lk(_data_mu);
        if (_data_err.empty()) return;
        _data_answer = retry ? 1 : 2;
    }
    _data_cv.notify_all();
}

}  // namespace gui
