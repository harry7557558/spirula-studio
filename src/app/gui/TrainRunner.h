#pragma once

// TrainRunner -- runs a spirula::TrainerSession on a worker thread for the
// GUI: async dataset preview, the full train pipeline (check -> parse ->
// engine setup -> step loop), pause/stop controls, metric history for the
// plots, and an optional web-viewer server (so a GUI run can still be
// monitored from a browser / another machine).
//
// Lifetime rules the GUI must follow:
//  - session() stays valid until the next load_dataset()/start_training()
//    call; anything holding hooks into it (the viewport's RenderWorker)
//    must detach first (GuiApp does this).
//  - engine_ready() flips true after engine setup; only then may a
//    RenderWorker attach.

#include "app/TrainerCore.h"
#include "app/webviewer/Viewer.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
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
    void note_engine_taken() { _engine_ready = false; }
    std::string error();

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

    mutable std::mutex _mu;       // guards everything below
    std::string _error;
    spirula::TrainerProgress _latest;
    std::deque<double> _latencies;
    std::vector<MetricPoint> _metrics;
    std::vector<std::string> _log;
};

}  // namespace gui
