#pragma once

// Runs TrainerSession on a GUI worker with progress and optional web viewing.
// session() remains valid until the next load/start; detach its consumers first.
// Attach renderers only after engine_ready(); after TrainError, detach them
// before cleanup_failed_engine().

#include "app/TrainerCore.h"
#include "app/webviewer/Viewer.h"
#include "backend/api/BackendRuntime.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class TrainRunner {
public:
    enum class Phase {
        Idle,       // nothing loaded
        Loading,    // dataset preview parse in flight
        Ready,      // dataset parsed; can start training
        LoadError,  // preview parse failed
        Preparing,  // start_training: re-parse + engine setup
        Training,   // step loop running
        Done,       // finished or stopped (checkpoint saved); engine viewable
        TrainError, // training pipeline failed
    };

    struct MetricPoint {
        int step = 0;
        float psnr = 0, ssim = 0, rgb_loss = 0;
        float num_splats = 0;
    };

    ~TrainRunner() { shutdown(); }

    // Async dataset preview (parse only; no GPU). Replaces the session.
    void load_dataset(const TrainConfig& cfg, const std::string& preset);

    // Async full run with the final config (re-parses the dataset so any
    // dataparser option changes apply). Replaces the session -- the caller
    // must detach viewers from the previous one first.
    void start_training(const TrainConfig& cfg, const std::string& preset);

    void set_paused(bool p);
    bool paused() const;
    // Graceful: finish the step in flight, then save a final checkpoint
    // unless `save` is false. Refusing to save is sticky for the session --
    // shutdown() asks to stop too, and must not undo the user's answer.
    void request_stop(bool save = true);
    void shutdown();              // stop + join (app exit / new session)

    Phase phase() const { return _phase.load(); }
    // Did the finished run write a final checkpoint? False only after a stop
    // the user asked not to save.
    bool saved_on_stop() const {
        return !_session || _session->save_on_stop.load();
    }
    bool engine_ready() const { return _engine_ready.load(); }
    // The engine is a process-global singleton, and something else (a splat
    // file in the viewer, the mesh preview) has just reset it -- which takes
    // this session's engine state with it. Nothing may attach a renderer to
    // the session after that, so engine_ready() goes back to false.
    void note_engine_taken();
    std::string error();

    // Structured GPU-memory failure behind a failed run, if that is what
    // failed it (copy). Empty for a generic training exception. Localize the
    // message from these fields -- error() is the English fallback.
    std::optional<backend::BudgetFailure> memory_failure();
    struct MemoryStatus {
        spirula::TrainingMemoryEstimate estimate;
        uint64_t allowance = 0;
    };
    std::optional<MemoryStatus> memory_status();

    // After TrainError, call once after native render consumers detach.
    // Stops the viewer, joins the worker, then resets the engine.
    // Repeated calls are no-ops; the last checkpoint remains untouched.
    void cleanup_failed_engine();

    // Valid between load_dataset()/start_training() calls; see lifetime
    // rules above.
    spirula::TrainerSession* session() { return _session.get(); }

    // Latest per-step progress (copy).
    spirula::TrainerProgress latest_progress();
    // Mean over the last 100 steps, which is also what the ETA is built from.
    // A single step's latency swings several-fold with whether a viewer
    // render landed on it, so it is not a number to put on screen.
    double avg_step_latency();
    double eta_seconds();         // < 0 when unknown
    // Time spent in the step loop, pauses excluded: < 0 before a session
    // exists, 0 until the loop starts, frozen once it ends.
    double elapsed_seconds();
    void get_metrics(std::vector<MetricPoint>& out);
    std::vector<std::string> drain_log();

    // ---- Unreadable dataset file ------------------------------------------

    // Non-empty while the training thread is blocked on a file it could not
    // read. The GUI shows it and answers with resolve_data_error().
    std::string data_error();
    // true retries the decode and the run picks up where it stopped; false
    // stops the run, which still saves a checkpoint.
    void resolve_data_error(bool retry);

private:
    void push_log(const std::string& s);
    double avg_latency_locked() const;   // caller holds _mu
    void join_worker();
    bool await_data_decision(const std::string& what);

    std::unique_ptr<spirula::TrainerSession> _session;
    std::unique_ptr<ViewerServer> _web_viewer;
    std::thread _worker;
    std::atomic<Phase> _phase{Phase::Idle};
    std::atomic<bool> _engine_ready{false};

    std::mutex              _data_mu;
    std::condition_variable _data_cv;
    std::string             _data_err;     // non-empty while awaiting an answer
    int                     _data_answer = 0;   // 0 pending, 1 retry, 2 stop

    // Set when the worker failed inside engine setup/training: the engine may
    // hold this session's state, and cleanup_failed_engine() is owed. Reset on
    // every new load_dataset()/start_training().
    std::atomic<bool> _engine_dirty{false};

    mutable std::mutex _mu;       // guards everything below
    std::string _error;
    std::optional<backend::BudgetFailure> _memory_failure;
    std::optional<MemoryStatus> _memory_status;
    spirula::TrainerProgress _latest;
    std::deque<double> _latencies;
    std::vector<MetricPoint> _metrics;
    std::vector<std::string> _log;
};

}  // namespace gui
