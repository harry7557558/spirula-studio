// TrainRunner.cpp -- see TrainRunner.h.

#include "app/gui/TrainRunner.h"

#include "i18n/catalog/Log.h"

#include <algorithm>

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

std::optional<backend::BudgetFailure> TrainRunner::memory_failure() {
    std::lock_guard<std::mutex> lk(_mu);
    return _memory_failure;
}

std::optional<TrainRunner::MemoryStatus> TrainRunner::memory_status() {
    std::lock_guard<std::mutex> lk(_mu);
    return _memory_status;
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
    // Stop the web viewer before joining the worker: its render thread takes
    // engine_mutex, which the training loop needs to finish the step in flight
    // and write the final checkpoint. Joining first would deadlock the stop.
    if (_web_viewer) { _web_viewer->stop(); _web_viewer.reset(); }
    join_worker();
}

void TrainRunner::note_engine_taken() {
    TrainerSession* session;
    {
        std::lock_guard<std::mutex> lk(_mu);
        session = _session.get();
        _engine_ready = false;
    }
    if (session) session->release_engine_budget();
}

void TrainRunner::cleanup_failed_engine() {
    // Joining a live worker would hang; only TrainError owes cleanup.
    // The exchange below makes repeated cleanup harmless.
    if (_phase.load() != Phase::TrainError) return;
    if (!_engine_dirty.exchange(false)) return;

    // Outside _mu, and without engine_mutex: ViewerServer::stop() joins the
    // render worker, and join_worker() waits on the training thread -- the
    // failing worker is unwinding out of its lock_guard, so it holds nothing.
    if (_web_viewer) { _web_viewer->stop(); _web_viewer.reset(); }
    join_worker();

    // The engine is a process-global singleton; the failed session left its
    // state in it. This resets the engine only -- the checkpoints on disk are
    // the run's result and are never touched.
    if (_session) _session->reset_engine();
    _engine_ready = false;
}

void TrainRunner::load_dataset(const TrainConfig& cfg, const std::string& preset) {
    shutdown();
    _engine_ready = false;
    _engine_dirty = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _memory_failure.reset();
        _memory_status.reset();
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
    _engine_dirty = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _memory_failure.reset();
        _memory_status.reset();
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
            s->check_config();
            s->load_dataset();
            // From here on the engine may hold this session's state, so a
            // failure owes cleanup_failed_engine() a call.
            _engine_dirty = true;
            s->setup_engine();
            {
                std::lock_guard<std::mutex> lk(_mu);
                _memory_status = MemoryStatus{
                    s->memory_estimate, s->memory_allowance};
            }
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
            _engine_dirty = false;   // the engine belongs to a live session
            _phase = Phase::Done;
        } catch (const backend::BudgetError& e) {
            // Preserve structured refusal fields for localization; error()
            // remains the fallback. The GUI resets after renderers detach.
            std::lock_guard<std::mutex> lk(_mu);
            _error = e.what();
            _memory_failure = e.failure;
            _memory_status = MemoryStatus{
                s->memory_estimate, s->memory_allowance};
            _engine_ready = false;
            _phase = Phase::TrainError;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(_mu);
            _error = e.what();
            _engine_ready = false;
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
