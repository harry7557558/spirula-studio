#pragma once

// SplatViewer -- a FILE open for looking at, with no dataset and no training
// behind it: a 3DGS model, an SfM point cloud, or an extracted triangle mesh.
// What it turns out to be decides which renderer draws it; opening it is the
// same action either way, which is what makes "drop a file on the window" work.
//
// Splats go into an ENGINE SCENE SLOT (Engine.h "Viewer scenes"), not into the
// engine's one world, so several of these can be open at once. Taking the
// engine over is the owner's job (CompareView); this only fills and frees a
// slot. Detach anything rendering from it BEFORE close().
//
// Loading runs on a worker thread: a large model is a few hundred MB of PLY.

#include "app/webviewer/RenderWorker.h"
#include "data/DatasetParser.h"
#include "mesh/MeshExport.h"   // meshing::MeshData

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class SplatViewer {
public:
    enum class State { Idle, Loading, Ready, Failed };
    // What the file turned out to hold: a `.ply` is a container, not a format,
    // and only its property list says which of the three it is. Points and
    // meshes draw on the GL preview; only Splats take a scene slot.
    enum class Kind { Splats, Points, Mesh };

    ~SplatViewer();

    // A splat .ply, a step-*.ckpt directory, or a run directory (whose newest
    // checkpoint is used). Asynchronous; watch state(). `engine_mutex` is
    // shared by every open model -- one scene is bound at a time.
    void open(const std::string& path, int scene_slot, std::mutex* engine_mutex);
    // Give the slot back. Safe to call when nothing is open.
    void close();

    State state() const { return _state.load(); }
    bool ready() const { return _state.load() == State::Ready; }
    Kind kind() const { return _kind.load(); }

    std::string error();
    std::string path();          // what was asked for
    std::string file();          // the .ply actually opened
    int64_t num_splats() const { return _num_splats.load(); }
    int sh_degree() const { return _sh_degree.load(); }

    // Valid once ready(). The client navigates in a normalized frame centred
    // on the model and sized so it is about one unit across, exactly as it
    // does for a dataset.
    ViewerRenderConfig render_config();
    ViewerHooks make_hooks();
    // Identity of what is loaded, for the viewport's framing memory.
    std::string scene_key();

    // The frame this model was fitted to, in the file's own coordinates:
    // normalized = (model - center) / unit. Mapping between two of these is
    // what puts two models on one camera.
    const float* frame_center() const { return _center; }
    float frame_unit() const { return _unit; }

    // Valid once ready() with kind() == Points: a dataset with no cameras and
    // the cloud in it, which is all PreviewRenderer needs.
    const ParsedDataset& points() const { return _points; }
    const PostSplitCameras& post() const { return _post; }

    // Valid once ready() with kind() == Mesh: the triangles, and the
    // similarity that maps them into the normalized frame the viewport
    // navigates (row-major 3x4, the layout PreviewRenderer takes).
    const meshing::MeshData& mesh() const { return _mesh; }
    const float* mesh_to_normalized() const { return _mesh_t2n; }
    int64_t num_faces() const { return _num_faces.load(); }

    // Re-run the engine's transfer / gamut setup for the OPEN model. A PLY
    // records neither, so trying is the only way to find out. Takes the engine
    // lock; `gamut` is a gamut_to_rec709 name, "" for Rec.709.
    void set_color_space(const char* gamut, int transfer, bool linear);
    // What the run's config.json said, so the controls open on the right
    // setting rather than on "none".
    std::string gamut() const;
    int transfer() const;
    bool linear_color() const;

    // Hand the previous primitive's screen buffers back after a primitive
    // switch (see engine_release_screen_buffers). Takes the engine lock;
    // call from the GUI thread, between renders.
    void release_screen_buffers();

    std::vector<std::string> drain_log();

private:
    void run(std::string path);
    void log(const std::string& s);

    std::thread _worker;
    std::atomic<State> _state{State::Idle};
    std::atomic<Kind> _kind{Kind::Splats};
    std::atomic<int64_t> _num_splats{0};
    std::atomic<int> _sh_degree{0};
    std::atomic<int64_t> _num_faces{0};
    // The engine scene slot this object filled, or < 0 for none: what close()
    // has to free, and what says a half-finished load still left one behind.
    std::atomic<int> _scene_slot{-1};
    int _pending_slot = -1;            // what the worker is filling
    std::mutex* _engine_mutex = nullptr;

    std::mutex _mu;                    // guards everything below
    std::string _error, _path, _file;
    std::string _gamut;                // "" = Rec.709
    int _transfer = 0;                 // colorspace::Transfer
    bool _linear = false;
    ViewerRenderConfig _cfg;
    std::vector<std::string> _log;
    // The fitted normalized frame, written by the worker before Ready.
    float _center[3] = {0, 0, 0};
    float _unit = 1.0f;
    // Points only. Written by the worker before it publishes Ready, read by
    // the GUI thread after; the state flag is the handoff.
    ParsedDataset _points;
    PostSplitCameras _post;
    // Mesh only, same handoff.
    meshing::MeshData _mesh;
    float _mesh_t2n[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
};

}  // namespace gui
