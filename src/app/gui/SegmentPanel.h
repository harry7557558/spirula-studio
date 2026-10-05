#pragma once

// SegmentPanel -- try a mask prompt on one real frame before committing to a
// run over the whole capture, with the same sam::Masker the run uses, so what
// is on screen is what will be written. Clicks prompt a SAM 2 checkpoint (no
// text tower); they belong to an object and a frame (MaskClick) and live in
// the settings, because they are prompts for the run. The model is dropped
// when the panel closes, so a reconstruction after it has the VRAM.
//
// The same panel edits the input's static stencil (app::FrameStencil): the
// fitted fisheye border plus shapes drawn with the mask editor's tools
// (StencilEdit.h), for the whole input or per camera. That half needs no
// model and works when there is none.

#include "app/FrameMask.h"
#include "app/gui/GlLoader.h"
#include "app/gui/MaskSettings.h"
#include "app/gui/ModelCache.h"
#include "app/gui/PreviewFrames.h"
#include "app/gui/StencilEdit.h"
#include "app/gui/StencilPreset.h"
#include "app/gui/edit/EditTool.h"
#include "app/gui/mask/Livewire.h"
#include "app/gui/mask/MaskWindow.h"
#include "app/gui/mask/PathTool.h"
#include "app/gui/mask/PenTool.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gui {

class SegmentPanel {
public:
    // Both out of line: the pimpl'd Job is incomplete here.
    SegmentPanel();
    ~SegmentPanel();

    // `src` carries the decoder and the FrameLook the run will use, so the
    // picture here is the file it writes. Video listing and probing run on the
    // panel worker; frame decoding follows there after the list is ready.
    void open(const PreviewSource& src, const MaskModelFiles& model);
    bool is_open() const { return _open; }
    void close();

    // Draws the modal window. Call once per frame from the dataset screen,
    // inside its ImGui frame. Both arguments are edited in place.
    void draw(MaskSettings& settings, app::FrameStencil& stencil);

    // Frees the GL textures; call while the GL context is current.
    void destroy_gl();

    // Whether the drawn shapes changed since the last call.
    bool take_shapes_edited() { return std::exchange(_shapes_edited, false); }
    // The output folder, whose kDatasetStencilDir the Load menu offers.
    void set_workspace(const std::string& w) { _workspace = w; }
    // Load menu's "Other file...": the caller owns the file picker.
    bool take_browse_request() { return std::exchange(_browse_requested, false); }
    void load_file(app::FrameStencil& stencil, const std::string& path);

private:
    struct Rgb;
    struct Job;

    void start_job(const MaskSettings& s, const app::FrameMask& stencil);
    void start_detect();
    void start_livewire();
    // The key the worker's Job uses for the shown frame, so the panel can
    // tell whether the edge map it holds is this frame's.
    std::string shown_frame_key() const;
    void upload_preview();
    // `win` is the part of the frame on screen, normalized (u0, v0, u1, v1).
    void upload_stencil(const app::FrameMask& stencil, const float win[4], int w, int h);
    void draw_image(MaskSettings& settings, app::FrameStencil& stencil,
                    bool& edited);
    void draw_objects(MaskSettings& settings, bool& edited);
    void draw_stencil(app::FrameStencil& stencil, bool& edited);
    void draw_tools(app::FrameStencil& stencil, bool& edited);
    void draw_saved_areas(app::FrameStencil& stencil);
    // A pen shape's anchors and handles under Points, or the pen with Ctrl:
    // hover, grab, drag, and the history step on release. True: the pointer
    // is on one of them, so the click is not the canvas's.
    bool edit_points(app::FrameStencil& stencil, app::MaskShape& shape, float mu, float mv,
                     float cw, float ch, bool hovered, bool& edited);
    // The pen over the selected pen shape: add, delete or convert an anchor.
    bool pen_on_shape(app::FrameStencil& stencil, app::MaskShape& shape, float mu, float mv,
                      float cw, float ch, bool hovered, bool& edited);
    // Every change to the stencil goes through here, for undo.
    void change_shapes(app::FrameStencil& stencil, bool& edited);
    void undo_shapes(app::FrameStencil& stencil, bool redo, bool& edited);
    // A plain stroke removes; Subtract, the eraser and Ctrl each flip it.
    bool stroke_removes(bool ctrl) const;

    // What the tools edit: the input's stencil, or with separate areas on,
    // the shown camera's (a copy of the input's until it is drawn on).
    app::CameraStencil& scope(app::FrameStencil& stencil);
    void set_per_camera(app::FrameStencil& stencil, bool on);
    // The camera folders this input writes, keyed as shown_camera() keys them;
    // worked out when the listing lands, into _cameras.
    std::vector<std::string> camera_names() const;

    // The stencil as it will be written: the shapes plus, when asked for, the
    // border this panel found, shrunk by the current amount.
    app::FrameMask resolved(const app::FrameStencil& stencil) const;

    // The settings hold every input's clicks; these are the ones on this picture.
    bool mine(const MaskClick& c) const { return c.source == _src.input; }

    // A photo input's file's subfolder under the input. "" for a video.
    std::string camera_of(const std::string& file) const;
    // Where the run puts that file's frames (DatasetPrep's StencilRaster
    // keys): its subfolder, under the shown lens's cam<k>/ for packed photos.
    std::string file_camera(const std::string& file) const;

    // The folder the shown frame lands in under the input's images: a 360
    // view, one lens of a multi-lens file, or the photo's own subfolder.
    std::string shown_camera() const;

    bool _open = false;
    MaskModelFiles _model;
    PreviewSource _src;
    // What the run splits this input into; one empty name for one camera.
    std::vector<std::string> _folders;
    std::vector<std::string> _cameras;
    int  _folder_idx = 0;
    std::vector<PreviewFrame> _frames;
    // Every image of a photo input; the border fit reads the ones sharing the
    // shown frame's camera folder. The slider offers a dozen, a fit two dozen.
    std::vector<std::string> _all_files;
    bool _frames_ready = false;         // guarded by _mu
    PreviewSource _listed_src;          // guarded by _mu
    std::vector<PreviewFrame> _frames_pending;
    std::vector<std::string> _all_files_pending;
    std::vector<std::string> _folders_pending;
    int  _frame_idx = 0;
    bool _frame_dirty = true;           // the chosen frame changed
    bool _needs_run = false;            // prompt edited; rerun on release
    std::atomic<bool> _listing{false};

    // ---- the stencil ----

    // The border of the camera the shown frame belongs to -- the run fits one
    // per camera and so does this -- carrying no shrink, so the slider
    // re-applies it without another fit.
    app::BorderDetect _border;          // UI thread
    std::string _border_camera;         // which camera _border was fitted on
    app::BorderDetect _border_pending;  // guarded by _mu
    bool _border_ready = false;         // guarded by _mu
    std::atomic<bool> _detecting{false};
    bool _detect_asked = false;         // a fit has been asked for at least once
    int  _shape_sel = -1;               // which shape carries handles
    // Which of its handles is being dragged, kDragBody for the whole shape,
    // -1 for none. The body drag is relative, so it also remembers where the
    // pointer was last frame.
    static constexpr int kDragBody = -2;
    int   _drag_handle = -1;
    float _drag_from_u = 0.0f, _drag_from_v = 0.0f;
    // A pen shape's point being dragged (Points, or the pen with Ctrl or
    // Alt), relative like the body: where it and the pointer were at the grab.
    PenPart _pt_part = PenPart::None;
    int   _pt_anchor = -1;              // also the picked anchor Delete removes
    float _pt_from[2] = {0.0f, 0.0f};
    float _pt_press[2] = {0.0f, 0.0f};
    bool  _pt_moved = false;
    bool  _pt_pull = false;             // Alt on an anchor under the pen: new handles
    GLuint _stencil_tex = 0;
    std::string _stencil_key;           // what _stencil_tex was built from
    float _stencil_win[4] = {0.0f, 0.0f, 1.0f, 1.0f};   // and the part of the frame it covers

    // The canvas: the mask editor's view over the preview's pixels, reset
    // when their size changes.
    mask::View _view;
    int _view_w = 0, _view_h = 0;
    bool _panning = false;

    // ---- the drawing tools ----
    enum class DrawTool { Select, Points, Shape, Eraser, Path, Pen };
    DrawTool _draw = DrawTool::Select;
    EditTool _tool;                     // Box to Brush, for Shape and Eraser
    bool _subtract = false;
    float _brush_pct = 2.0f;            // radius, % of the picture's shorter side
    StencilHistory _history;
    app::FrameStencil _drag_before;     // the stencil as a drag found it
    std::string _drag_before_key;       // and its edited shapes, spelled
    bool _shapes_edited = false;
    std::vector<StencilPreset> _saved;  // refreshed when the load popup opens
    std::vector<StencilPreset> _in_dataset;
    std::string _workspace;
    bool _browse_requested = false;
    std::string _save_name;
    std::string _saved_msg;
    bool _saved_msg_err = false;
    mask::PathTool _path;
    mask::PenTool _pen;
    std::unique_ptr<mask::Livewire> _livewire;          // UI thread
    std::string _livewire_key;                          // frame it was built for
    std::unique_ptr<mask::Livewire> _livewire_pending;  // guarded by _mu
    std::string _livewire_pending_key;                  // guarded by _mu
    bool _livewire_ready = false;                       // guarded by _mu
    double _livewire_ms = 0.0;                          // guarded by _mu
    std::atomic<bool> _livewiring{false};

    // The composited RGB preview handed to GL, guarded by _mu.
    std::mutex _mu;
    std::vector<uint8_t> _preview;
    int _preview_w = 0, _preview_h = 0;
    bool _preview_dirty = false;
    std::string _status, _error;
    float _kept_fraction = -1.0f; // guarded by _mu
    // What is left for feature points; -1 without a feature prompt. Ditto.
    float _feature_fraction = -1.0f;

    GLuint _tex = 0;
    int _tex_w = 0, _tex_h = 0;

    std::thread _worker;
    std::atomic<bool> _busy{false};
    std::atomic<bool> _cancel{false};
    std::unique_ptr<Job> _job;          // the session, kept warm between runs
};

}  // namespace gui
