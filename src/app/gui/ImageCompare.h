#pragma once

// ImageCompare -- the training preview's second mode: one training frame's
// ground truth beside the render of the same camera, one block per modality
// the run loaded (photo, depth / normals where supervised, the densification
// error map), on a grid draw_panes fits to the picture's shape.
//
// The panes come out of the engine (engine_preview_forward,
// engine_preview_loss_map), so what is on screen is what the loss compares
// and what densification scores. Zoom and pan are shared: the question this
// mode answers is whether the render is soft because the model is, or
// because the photograph was.
//
// GUI thread only, plus its one worker. detach() BEFORE the session goes away.

#include "app/gui/GlLoader.h"

#include "imgui.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spirula {
class TrainerSession;
namespace i18n { struct Msg; }
}

namespace gui {

// Where one post-split view goes on the canvas when the pair is drawn, in
// canvas pixels, and how far it is turned to sit flush against its neighbours
// (ImageCompare.cpp's face_layout unfolds the cube).
struct FaceCell { float x, y, w, h; int turns; };   // turns = quarter turns clockwise

// Where one view sits in the packed texture, in texels. Views differ in size
// when the faces were cropped per lens, so each carries its own rect.
struct AtlasRect { int x, y, w, h; };

class ImageCompare {
public:
    ~ImageCompare();

    void attach(spirula::TrainerSession& session);
    void detach();
    bool attached() const { return _session != nullptr; }

    // Draw into the current window. `step` is the training iteration that has
    // just finished (< 0 when unknown); `training` enables the follow-along
    // refresh.
    void draw(bool training, int step);

    // Free the GL textures. Call while the GL context is still current.
    void destroy_gl();

private:
    // What the region the mask excludes looks like.
    enum class MaskShow { Dim = 0, Unmarked = 1, Hide = 2 };

    // What the worker was asked for. Equality decides whether the pair on
    // screen still answers the current selection.
    struct Job {
        int  index = 0;
        bool color_correct = true;
        // Show the file itself rather than the downscaled copy the loss sees.
        bool source_gt = false;
        // Also compute the densification error map, which costs a loss pass.
        bool error_map = false;
        bool operator==(const Job& o) const {
            return index == o.index && color_correct == o.color_correct &&
                   source_gt == o.source_gt && error_map == o.error_map;
        }
    };

    // What came back. The views are packed into `gt` / `render` / `mask` as a
    // tight row-major grid, `pack_cols` wide; where each of them BELONGS on
    // screen is `cells`. Keeping the two apart is what lets the cross leave
    // cells empty without the texture paying for them.
    struct Shot {
        uint64_t id = 0;
        Job  job;
        int  views = 1;
        int  view_w = 0, view_h = 0;       // the largest view
        int  atlas_w = 0, atlas_h = 0;     // the packed texture, in texels
        int  canvas_w = 0, canvas_h = 0;   // the unfolded layout, in pixels
        std::vector<FaceCell>  cells;      // where each view is drawn
        std::vector<AtlasRect> uv;         // where each view is stored
        std::vector<uint8_t> gt, render;   // [atlas_h, atlas_w, 3]
        // The supervision modalities on the same grid, already coloured
        // (app/DepthColor.h) so the UI thread only uploads. Empty when the
        // run does not load them.
        std::vector<uint8_t> gt_depth, render_depth;
        std::vector<uint8_t> gt_normal, render_normal;
        // The densification error map on the same grid, on the depth ramp.
        std::vector<uint8_t> err_map;
        float err_mean = 0.0f, err_max = 0.0f;
        std::vector<uint8_t> mask;         // the same grid; empty when there is none
        int  src_w = 0, src_h = 0;
        std::vector<uint8_t> src;          // the file itself, when asked for
        float psnr = -1.0f;                // < 0 = no unmasked pixel to score
        int  step = -1;
        // What the job cost, measured on the worker. Timed there and not
        // across submit/poll: the panel is only polled while it is drawn, so
        // a spell on the other preview (or any stall in the frame loop) would
        // otherwise be charged to the render and stall the pacing below.
        double secs = 0.0;
        std::string error;
    };

    // One pane's picture, ready to draw: the packed texture plus the canvas
    // the shared zoom and pan move over.
    struct Pane {
        GLuint tex = 0;
        int tex_w = 0, tex_h = 0;
        int canvas_w = 0, canvas_h = 0;
        std::vector<FaceCell>  cells;
        std::vector<AtlasRect> uv;
    };

    void worker_loop();
    void run_job(const Job& j, Shot& out);
    void submit(const Job& j);
    void poll();
    // Re-composite the mask into the two byte images and re-upload. Cheap
    // enough to run on every mask / GT-source change, so those toggle without
    // costing a render.
    void rebuild_textures();
    void upload(GLuint& tex, const uint8_t* rgb, int w, int h);
    void draw_toolbar(bool training);
    void draw_image_picker();
    // Put the next control on this row if `w` still fits, else start a row.
    void place(float w);
    void draw_panes(const ImVec2& avail);
    // One modality's panes: the reference / render pair, or a lone map.
    struct Block;
    int  collect(Block* out) const;
    void draw_pane(const Pane& p, const ImVec2& box,
                   const spirula::i18n::Msg& caption);
    void handle_view_input();
    void handle_keys();
    // The split factor of the selected image.
    int  faces() const;
    bool has_masks() const;
    void select(int index);

    spirula::TrainerSession* _session = nullptr;

    // ---- selection + options ----
    int  _index = 0;
    MaskShow _mask_show = MaskShow::Dim;
    bool _color_correct = true;
    bool _source_gt = false;
    bool _smooth = false;
    bool _live = true;
    bool _show_depth = true;
    bool _show_normal = true;
    bool _show_error = true;
    // Whether this run builds a densification error map at all -- a mode of
    // `none`, or a score taken entirely from the world gradient, builds none.
    bool _has_error_map = false;
    std::string _filter;          // the picker's file-name search
    float _row_w = 0.0f, _row_used = 0.0f;   // toolbar packing (see place)

    // ---- view transform, shared by both panes ----
    float _zoom = 1.0f;           // 1 = fit the pane
    float _cx = 0.5f, _cy = 0.5f; // view centre, normalized image coordinates
    bool  _dragging = false;
    // The pane under the cursor on this frame, and the size the image is
    // drawn at there -- what the wheel and the drag are resolved against.
    bool   _hot = false;
    ImVec2 _hot_min{}, _hot_size{};
    float  _hot_w = 1.0f, _hot_h = 1.0f;

    // ---- worker ----
    std::thread _worker;
    std::mutex _mu;
    std::condition_variable _cv;
    bool _running = false;
    bool _has_pending = false;
    Job  _pending;
    uint64_t _pending_id = 0, _next_id = 1;
    Shot _result;                 // guarded by _mu until poll() takes it
    // Readback scratch, worker thread only. Reused across jobs: see run_job.
    std::vector<float> _wgt, _wrender;
    // Working-space render, fetched only alongside the source file so both
    // panes show the colour space they are stored in.
    std::vector<float> _wrender_raw;
    std::vector<uint8_t> _walpha;
    // The supervision modalities, read back inside the engine mutex and
    // coloured outside it: colouring 2 MPx would hold the trainer up for
    // nothing.
    std::vector<float> _wgt_depth, _wr_depth, _wgt_normal, _wr_normal;
    std::vector<float> _werr;
    std::vector<uint8_t> _wmodrgb;   // one of them coloured, before packing

    // ---- what is on screen ----
    Shot _shot;
    Job  _requested{-1, false, false, false};  // never equals a real selection
    uint64_t _in_flight = 0;
    bool _dirty = true;           // the selection changed; re-request
    bool _tex_dirty = true;       // the composite changed; re-upload
    Pane _gt, _render;
    Pane _gt_depth, _render_depth, _gt_normal, _render_normal, _err_map;
    std::string _error;

    // ---- refresh pacing while training (see draw) ----
    int    _rendered_step = -1;
    int    _last_seen_step = -1;
    double _last_step_time = -1.0;
    double _job_secs = 0.0;
    double _step_secs = 0.0;
};

}  // namespace gui
