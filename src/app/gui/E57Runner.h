#pragma once

// E57Runner -- "Create Dataset from E57": `spirula e57` as a child process
// (like MeshRunner; the work is file I/O, and a malformed file cannot take the
// window with it), then the dataset screen's own masking over the images it
// wrote -- DatasetPrep, in this process, on the frozen GPU. Both report through
// one RunProgress, the child's translated lines read back by i18n::scan(), and
// feed the dataset screen's reels: what the child writes is watched on disk.

#include "app/gui/DatasetPrep.h"
#include "app/gui/FilmReel.h"
#include "app/gui/PrepProgress.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

struct E57Job {
    std::string input, output;
    int seed_points = 500000;
    bool all_points = false;
    bool depth_maps = true;
    bool pinhole = true;
    bool spherical = true;
    // Masking, as the dataset screen builds it (GuiApp::fill_masking); its
    // input is made from the written dataset. `frames` is the folder "Try the
    // mask" read, whose clicks are the dataset's images under another name.
    bool masks = false;
    PrepJob prep;
    app::FrameStencil stencil;
    std::string frames;
};

class E57Runner {
public:
    enum class State { Idle, Running, Done, Failed, Cancelled };

    ~E57Runner();

    // `films` are fed while the run works, and must outlive it.
    void start(const E57Job& job, RunFilms films = {});
    void cancel();
    // Back to Idle unless running: the screen has moved on to another scan.
    void reset();

    State state() const { return _state.load(); }
    bool busy() const { return _state.load() == State::Running; }
    // The steps this run goes through, in the order it takes them.
    const std::vector<Stage>& steps_taken() const { return _taken; }

    std::string error();
    // The folder written, valid once state() == Done.
    std::string output();
    // What the trainer's mask_dir should be, "" when the run made no masks.
    std::string mask_dir();
    // The dataset was written, whatever became of masking it.
    bool converted() const { return _converted.load(); }
    // The alignment check's line when it corrected the photos or could not
    // confirm them; empty when they simply agreed.
    std::string check_warning();
    std::vector<std::string> drain_log();
    RunProgress& steps() { return _prog; }

private:
    void run(E57Job job);
    void log(const std::string& line);
    void note_line(const std::string& line);
    void step(Stage s, const std::string& line, int64_t done, int64_t total);

    std::thread _worker;
    std::atomic<State> _state{State::Idle};
    std::atomic<bool> _cancel{false};
    std::atomic<bool> _converted{false};
    RunProgress _prog;
    RunFilms _films;
    std::vector<Stage> _taken;
    std::mutex _mu;
    std::string _error, _output, _mask_dir, _check_warning;
    std::vector<std::string> _log;
};

}  // namespace gui
