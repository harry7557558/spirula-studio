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
// The stages come from the child's own `[meshing] <stage> (<secs>s)` lines,
// which both backends print, so the panel reports real progress rather than a
// spinner.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

// What the color combo offers, in order. The values are the `--color` tokens.
inline const char* kMeshColorModes[] = {"none", "vertex", "texture"};
inline constexpr int kNumMeshColorModes = 3;

// The output formats, in the order the checkboxes are drawn. These are the
// `--format` tokens, and the file extensions.
inline const char* kMeshFormats[] = {"ply", "obj", "gltf", "glb"};
inline constexpr int kNumMeshFormats = 4;

struct MeshJob {
    // A run directory, a step-*.ckpt directory, or a splat .ply.
    std::string checkpoint;
    // The dataset the model was trained on. "" lets the child read it from
    // the run's config.json; `use_data == false` passes --no-data, which
    // meshes from Gaussian densities alone.
    std::string data_dir;
    bool use_data = true;

    // Output base path, without an extension. "" = beside the checkpoint.
    std::string output;

    int color = 1;                     // index into kMeshColorModes
    bool formats[kNumMeshFormats] = {true, false, false, false};

    int max_cameras = 0;               // 0 = every camera
    int texture_size = 0;              // 0 = auto (texture mode only)

    // Canonical UUID for native children; empty uses shared precedence and prevents
    // inherited environment or ordinal re-ranking once populated.
    std::string device_uuid;
    // CUDA children receive the separately frozen ordinal; never a Vulkan UUID.
    int cuda_device = -1;

    // ---- advanced ----
    float iso = 0.0f;                  // 0 = the child's default for the path
    int bisection_iters = 3;
    float merge_factor = 1.0f;
    int quality_iters = 3;
    int floater_min_faces = 10;
    bool cull_unseen = true;
    int carve_k = 1;
    std::string extra_args;            // appended verbatim

    // The file the preview should open when the run finishes: the richest
    // format that was requested (a textured GLB carries its atlas, a PLY does
    // not), plus its extension.
    std::string preview_path() const;
};

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
    // Vertices / faces reported by the child's final line, 0 until then.
    int64_t num_verts() const { return _verts.load(); }
    int64_t num_faces() const { return _faces.load(); }
    // 0..1 over the pipeline's stages, or -1 before the first one. Between
    // two stage marks it creeps with elapsed time toward the next mark, so a
    // long silent phase (Delaunay is minutes on a big model) still LOOKS like
    // it is running; a phase that reports "i/n" overrides the creep with the
    // real fraction.
    float progress() const;

    std::vector<std::string> drain_log();

private:
    void run(MeshJob job);
    void log(const std::string& line);
    void note_line(const std::string& line);

    std::thread _worker;
    std::atomic<State> _state{State::Idle};
    std::atomic<bool> _cancel{false};
    std::atomic<uint64_t> _run_id{0};
    // The bracket the bar is currently inside, and when it was entered.
    std::atomic<float> _stage_lo{-1.0f}, _stage_hi{-1.0f}, _stage_frac{-1.0f};
    std::atomic<double> _stage_at{0.0};
    std::atomic<int64_t> _verts{0}, _faces{0};
    std::mutex _mu;
    std::string _stage, _error, _output;
    std::vector<std::string> _log;
};

}  // namespace gui
