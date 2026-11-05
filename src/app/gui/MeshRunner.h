#pragma once

// MeshRunner -- surface extraction from a trained model, driven from the GUI.
//
// Same public shape as SfmRunner / ColmapRunner (start / cancel / state /
// stage / error / drain_log), and for the same reason it runs as a CHILD
// PROCESS rather than in this one: the GUI already has a Vulkan device live
// (the viewer's), meshing wants its own multi-gigabyte VRAM budget for the
// per-camera renders, and every byte it held is gone when it exits. The child
// is this same executable (`spirula mesh`), so there is nothing to install.
//
// The caption comes from the child's own `[meshing] <stage>: ...` lines, which
// both backends print; how far it is and the memory it expects come from the
// report it writes beside them (app/RunReport.h, mesh/MeshProgress.h).

#include "app/gui/MeshJob.h"
#include "app/gui/ProgressClock.h"
#include "app/gui/RunMemoryView.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class MeshRunner {
public:
    enum class State { Idle, Running, Done, Failed, Cancelled };

    ~MeshRunner();

    void start(const MeshJob& job);
    void cancel();

    State state() const { return _state.load(); }
    bool busy() const { return _state.load() == State::Running; }
    // Which run the current state belongs to; 0 before the first start().
    // A finished runner STAYS Done, so "has a result the screen has not shown
    // yet" cannot be read off state() alone -- close the preview and it would
    // reopen on the next frame. The caller remembers the id it has shown.
    uint64_t run_id() const { return _run_id.load(); }

    std::string stage();
    std::string error();
    // The mesh file written, valid once state() == Done.
    std::string output_path();
    // Every file the run left on disk. One extraction can be asked for
    // several formats and colors, and they are all the same surface.
    std::vector<std::string> output_paths();
    // Vertices / faces reported by the child's final line, 0 until then.
    int64_t num_verts() const { return _verts.load(); }
    int64_t num_faces() const { return _faces.load(); }
    // Reads the child's report into the bar and the memory strip. UI thread,
    // every frame while the screen or the batch shows the run; at most twice a second.
    void poll();
    const ProgressClock& clock() const { return _clock; }
    const RunMemoryView& memory() const { return _memory; }

    std::vector<std::string> drain_log();

private:
    void run(MeshJob job);
    void log(const std::string& line);
    void note_line(const std::string& line);

    std::thread _worker;
    std::atomic<State> _state{State::Idle};
    std::atomic<bool> _cancel{false};
    std::atomic<uint64_t> _run_id{0};
    std::atomic<int64_t> _verts{0}, _faces{0};
    // The UI thread's: poll() writes them, the screen reads them.
    ProgressClock _clock;
    RunMemoryView _memory;
    uint64_t _polled_run = 0;
    std::mutex _mu;
    std::string _stage, _error, _output, _progress_dir;
    std::vector<std::string> _outputs;
    std::vector<std::string> _log;
};

}  // namespace gui
