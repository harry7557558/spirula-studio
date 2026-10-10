#pragma once

// ViewportPanel -- the native 3D viewport, with two backends behind the
// browser-identical NavCamera navigation (see NavCamera.h: four modes,
// mouse / keyboard / touch-via-OS-pointer / gamepad input):
//
//   Preview: pure-GL sparse point cloud + camera frusta (PreviewRenderer),
//            shown as soon as a dataset is parsed, before training.
//   Engine:  the shared RenderWorker (same latest-wins engine render path
//            as the web viewer) once training has set up the engine; all
//            web-viewer camera models (pinhole / fisheye-equidistant /
//            fisheye-equisolid / equirectangular) with per-model FOV.
//
// GUI thread only. attach*() after the corresponding phase; detach() BEFORE
// the session it renders from is destroyed.

#include "app/webviewer/RenderWorker.h"
#include "core/ColorSpace.h"
#include "core/Similarity.h"
#include "data/DatasetParser.h"
#include "app/gui/NavCamera.h"
#include "app/gui/PreviewRenderer.h"
#include "app/gui/ViewportInput.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct ImVec2;
namespace spirula { class TrainerSession; struct RegionOverlay; }

namespace gui {

struct ViewBookmark;
struct ViewBookmarkSet;

// What the viewer's primitive combo offers, in order. These are the
// `forward_3dgs` primitive names, so they are identifiers, not words.
inline const char* kViewerPrimitives[] = {"3dgs", "mip", "3dgut"};
inline constexpr int kNumViewerPrimitives = 3;

// Color gamuts the render can be converted FROM, in combo order. "" is
// Rec.709 (no conversion); the rest are gamut_to_rec709's names, which are
// standard identifiers rather than translatable words.
inline const char* kViewerGamuts[] = {"", "DCI-P3", "Rec.2020", "AdobeRGB",
                                      "ACEScg", "ACES2065-1"};
inline constexpr int kNumViewerGamuts = 6;

// Output transfers, in combo order and in colorspace::Transfer order. Like
// the gamuts these are the identifiers --*-color-transfer takes.
inline constexpr int kNumViewerTransfers = colorspace::kNumTransfers;

class ViewportPanel {
public:
    // Dataset preview (needs load_dataset() done; no GPU engine).
    void attach_preview(spirula::TrainerSession& session);
    // The same GL preview over parsed data with no session behind it: a .ply
    // that turned out to hold points rather than Gaussians, or a reconstruction
    // still being built. `with_cameras` offers the frustum controls -- a point
    // file has none to show, a live model is half about them.
    void attach_preview_data(const ParsedDataset& ds, const PostSplitCameras& post,
                             const std::string& key, float radius = 1.0f,
                             bool with_cameras = false,
                             const uint8_t* cam_selected = nullptr,
                             const float* cam_rgb = nullptr);
    // The same GL preview over an extracted triangle mesh, shaded.
    // `to_normalized` is the row-major 3x4 similarity into the navigated
    // frame (PreviewRenderer's convention); nullptr for identity.
    void attach_preview_mesh(const meshing::MeshData& mesh,
                             const float to_normalized[12],
                             const std::string& key, float radius = 1.0f);
    // Engine renderer (needs engine_ready).
    void attach(spirula::TrainerSession& session);
    // A region drawn over the view: `engine` in the training frame for engine
    // renders, `preview` in the frame of the preview's points. Kept across
    // attaches; null for none.
    void set_region_overlay(std::shared_ptr<const spirula::RegionOverlay> engine,
                            std::shared_ptr<const spirula::RegionOverlay> preview,
                            std::shared_ptr<const std::vector<uint8_t>> points_inside = nullptr);
    // Engine renderer over a file (SplatViewer): `key` keeps the pose across
    // a reopen and `radius` is the scene radius in the client frame; the
    // centering menu comes from cfg.centers.
    void attach_scene(const ViewerRenderConfig& cfg, const ViewerHooks& hooks,
                      const std::string& key, float radius = 1.0f);
    // The render-option controls a VIEWER gets, on top of attach_scene. The
    // two callbacks are the owner's because both are engine calls; only it
    // holds the engine lock. `sh_degree_max` caps the SH slider.
    void enable_scene_options(
        const std::string& primitive, int sh_degree_max,
        const std::string& gamut, int transfer, bool linear,
        std::function<void(const char* gamut, int transfer, bool linear)>
            apply_color_space,
        std::function<void()> on_primitive_changed);
    void detach();
    bool attached() const { return _mode == Mode::Engine; }

    // Hide the controls the view link makes shared -- navigation, camera
    // model, field of view, reset -- so a row of linked panels shows them
    // once, drawn by draw_nav_controls() wherever the owner wants them.
    void show_nav_controls(bool on) { _nav_controls = on; }
    void draw_nav_controls();

    // How tall the last draw()'s control rows were, and extra height to leave
    // under them. A row of panels whose controls wrap differently would
    // otherwise render at different sizes, which is not a comparison.
    float controls_height() const { return _controls_h; }
    void set_controls_pad(float px) { _controls_pad = px; }
    // The image this wide, left-aligned under controls that keep the whole
    // width; the owner draws beside it. 0 = all of it.
    void set_image_width(float w) { _image_w = w; }

    // An editing tool over this viewport. While one is installed it owns the
    // left button; the other two stay with navigation, so a tool is never a
    // dead end. Null detaches.
    void set_interactor(ViewportInteractor* t) { _interactor = t; }
    // The navigated frame to camera, row-major 3x4 in the CV convention
    // viewer_pixel_ray works in, plus the intrinsics for a `W` x `H` image
    // and which display camera model they belong to.
    void view_camera(int W, int H, float w2c[12], float& fx, float& fy,
                     int& camera_model, float eye[3]) const;
    // Where the last draw put the image on screen, in ImGui coordinates.
    void image_rect(float& x, float& y, float& w, float& h) const;
    // The same pose in the SHARED frame the camera navigates, which is where
    // a placement is dragged: the model moves through it, the grid does not.
    void nav_camera(int W, int H, float w2c[12], float& fx, float& fy,
                    int& camera_model, float eye[3]) const;
    // How far the orthographic emulation pulled the render camera back along
    // its axis, in the frame view_camera / nav_camera report; 0 in perspective.
    float ortho_pullback(bool shared) const;
    // The orbit pivot, shared frame: what the view is looking at.
    void nav_target(float out[3]) const {
        for (int i = 0; i < 3; i++) out[i] = _cam.target[i];
    }
    // A render is due: what a tool calls after changing what is drawn.
    void invalidate() { _dirty = true; }

    // ---- what render mode drives (app/gui/render/) ----

    // The navigation pose, shared frame: camera-to-world 3x4 in OpenGL axes
    // and the orbit pivot. Setting it jumps; nothing animates.
    void nav_pose(float c2w[12], float target[3]) const;
    void set_nav_pose(const float c2w[12], const float target[3]);
    // The display lens: camera model index (kViewerCameraModels) and its
    // field of view, degrees across the width.
    int view_model() const { return _cam_model; }
    // The primitive the scene options render with; empty without them.
    std::string primitive() const;
    float view_fov() const { return _fov_deg[_cam_model]; }
    void set_view_lens(int model, float fov_deg);
    // The point under a fraction of the image (0..1 each way), found on the
    // next render the way a double-click finds one. take_pick() hands it
    // over once: true with `hit` false means the ray found nothing.
    void request_pick(float u, float v);
    bool take_pick(float out[3], bool& hit);
    // The placement under edit alone (see set_edit_transform), and the whole
    // model -> shared similarity.
    void edit_transform(float out[12]) const;
    void model_to_shared(float out[12]) const;

    // A placement under edit, model frame -> model frame, composed INSIDE the
    // owner's: what the editor moves while the owner's alignment stays put.
    void set_edit_transform(const float a[12]);
    // Model -> shared with no edit applied: the frame a placement is made in.
    void base_transform(float out[12]) const;
    float world_grid_cell() const;
    // The parsers' up->+Z guess (adopt_gauge). Placing a model means seeing
    // the axes that get SAVED, which is with the guess switched off.
    bool has_levelling() const { return !_align_identity; }
    bool level_cameras() const { return _level_cameras; }
    void set_level_cameras(bool on);
    // The scene's up, shared frame, for navigating only: the model, the grid
    // and the axes stay in the frame the data is in. +Z until set.
    void set_nav_up(const float up[3]);
    // The scene moved by S (row-major 3x4 [sR | t], shared frame); the camera
    // goes with it, so the picture does not change.
    void move_view(const float S[12]);

    // Take the view along with a step of the shared frame (row-major 3x4
    // similarity), then stand it upright again: the model stays where it was
    // on screen and it is the grid that arrives under it.
    void carry_view(const float step[12]);

    // Glide to look at a sphere, shared frame, from the direction the view
    // already has, near enough to fill it: what Numpad . does.
    void frame_view(const float centre[3], float radius);
    // Look along a world axis (0..2, `negative` for the far side), switching
    // to the orthographic view; and the switch on its own.
    void snap_view(int axis, bool negative);
    bool ortho() const { return _ortho; }
    void set_ortho(bool on);
    // Where the centring menu's points come from when the user PICKS one, so
    // an edited model centres on what is left of it. Asked only on the pick:
    // a median per frame is a hiccup, and a centre is where you asked for it.
    void set_center_provider(std::function<bool(dsparse::CenterTable&)> f) {
        _center_provider = std::move(f);
    }

    // Where this panel's model sits in the SHARED frame the camera navigates
    // (row-major 3x4 similarity, scale*R | t; identity by default). Applied to
    // the CAMERA, not the geometry, so moving a model costs nothing.
    void set_model_transform(const float a[12]);

    // What the dataset says about its own frame, so the panel can offer to
    // skip the parsers' up->+Z guess (DatasetParser.h). `first` picks the
    // checkbox's default; a refresh of the same scene must not.
    void adopt_gauge(const ParsedDataset& ds, bool first);

    // Side-by-side: adopt `src`'s navigation pose, camera model and FOV, so
    // two panels showing the same scene stay locked to one view. `moved()`
    // says whether this panel's own pose changed on the last draw, which is
    // how the caller decides which of the two is currently the master.
    void sync_view_from(const ViewportPanel& src);
    bool moved() const { return _moved_last_draw; }
    // A drag is in progress in THIS panel. The link's master has to be sticky
    // for the length of a drag: picking it from moved() alone would let the
    // panel that was synced last frame claim it back and fight the drag.
    bool dragging() const { return _dragging; }
    bool preview_active() const { return _mode == Mode::Preview; }

    // Draw the viewport (controls rows + image) into the current ImGui
    // window/child. `training` enables continuous refresh; `step` is the
    // training iteration that has just finished (< 0 when unknown), which is
    // what paces the refresh while nobody is steering the camera.
    void draw(bool training, int step = -1);

    // Free GL resources. Call while the GL context is still current
    // (before ImGui/GLFW shutdown).
    void destroy_gl();

    // ---- saved views (ViewBookmarks.cpp) ----
    // The slots this panel offers and its model frame -> the frame they are
    // saved in; null hides them.
    void set_views(std::shared_ptr<ViewBookmarkSet> set, const spirula::Sim3& model_to_store);
    void save_view(int slot);
    void go_to_view(int slot);
    static const char* camera_model_label(int model);
    // The pose in the saved-views frame (set = false without one), and the
    // saved view it sits on, -1 when none.
    ViewBookmark current_pose() const;
    int current_view() const;
    const ViewBookmarkSet* views() const { return _views.get(); }

    // ---- screenshots (defined in Screenshot.cpp) ----
    // W x H from the current view without grid, frusta or outline, after the
    // frame in flight; `done` runs on the GUI thread. False with no engine.
    bool request_capture(int W, int H, std::function<void(ViewResult&&)> done);
    bool capture_pending() const { return (bool)_shot_done; }

private:
    void submit_capture();
    void poll_capture();
    std::function<void(ViewResult&&)> _shot_done;
    int _shot_w = 0, _shot_h = 0;
    bool _shot_inflight = false;
    float views_width() const;
    void draw_view_buttons();
    void handle_view_keys();
    void animate_glide(double now);
    std::shared_ptr<ViewBookmarkSet> _views;
    spirula::Sim3 _views_m2s;        // model -> saved frame
    int _view_rename = -1;
    std::string _view_name_edit;
    // A glide to a saved view: pivot and rotation interpolated, the eye kept
    // where it sits relative to both; stops when anything else moves the camera.
    bool _glide = false;
    double _glide_t0 = 0.0;
    NavCamera _glide_from, _glide_to, _glide_last;

    enum class Mode { None, Preview, Engine };

    void compute_framing(const spirula::TrainerSession& session);
    // The client-frame default pose (web viewer cam.reset() + orbit(0,-250)),
    // about the chosen centre.
    void reset_pose(float radius);
    // The centering choices, in the model frame. `has_cameras` says whether
    // the camera statistics are real or fell back to the point ones.
    void set_centers(const dsparse::CenterTable* centers, bool has_cameras);
    // The chosen centre in the shared frame; the origin when none is known.
    void center_shared(float out[3]) const;
    // The mode the menu shows: the point statistic a camera one fell back to
    // when there are no cameras.
    int effective_center_mode() const;
    // Frame the scene only when a different dataset arrives; a preview ->
    // engine transition on the same dataset keeps the navigated pose and
    // intrinsics (no jump when training starts).
    bool maybe_frame(const spirula::TrainerSession& session);
    void reset_view();
    // Update _moving / _last_move from the camera pose. Runs every frame in
    // every scale mode: what it feeds is no longer only the adaptive scale.
    void note_motion(double now);
    float render_scale();
    // Training iterations between refreshes while the camera is still, sized
    // from the measured render and step costs so the viewport keeps costing
    // about the same slice of the run whatever the dataset.
    int idle_step_interval() const;
    // Render resolution for `avail` at the current scale, aspect PRESERVED.
    void render_size(const ImVec2& avail, int& W, int& H) const;
    // Double-click centering (webgl viewer's recenterAt): pan laterally so
    // p (normalized frame) sits on the optical axis and make it the orbit
    // pivot; if p is behind the camera plane (>180-degree models), rotate
    // toward it instead.
    void recenter_at(const float p[3]);
    const char* camera_model_name() const;
    float fov_min() const;
    float fov_max() const;
    void compute_intrinsics(int W, int H, float& fx, float& fy) const;
    // Camera-to-orbit-target distance (normalized frame); drives the
    // zoom-adaptive grid cell size.
    float nav_dist() const;
    // The navigation pose expressed in the MODEL's frame -- what both
    // renderers are actually given (see set_model_transform).
    void model_c2w(float out[12]) const;
    void model_point(const float shared[3], float out[3]) const;
    void shared_point(const float model[3], float out[3]) const;
    void build_request(ViewRequest& q, int W, int H) const;
    void view_matrix(float out[16]) const;   // row-major world-to-view
    void upload(const ViewResult& res);
    void draw_controls(bool engine);
    void handle_input(float item_h);         // mouse/keys/gamepad on last item
    void draw_engine(bool training, const ImVec2& avail, int step);
    void draw_preview(const ImVec2& avail);

    Mode _mode = Mode::None;
    RenderWorker _worker;
    PreviewRenderer _preview;

    // Browser-identical camera + navigation.
    NavCamera _cam;
    NavCamera _home;                 // pose restored by Reset view
    float _home_dist = 3.0f;         // scene radius (near/far for preview)
    std::string _framed_key;         // dataset identity the pose belongs to
    bool _dragging = false;
    int _drag_button = 0;            // ImGuiMouseButton at drag start
    // Pending double-click pick, as fractional viewport-image coordinates
    // (set in handle_input; consumed by draw_preview / draw_engine).
    bool _dbl_pending = false;
    float _dbl_u = 0.0f, _dbl_v = 0.0f;
    // The same for a tool that asked (request_pick): delivered, not recentred.
    bool _tool_pick = false, _tool_pick_done = false, _tool_pick_hit = false;
    bool _tool_pick_inflight = false;
    float _tool_pick_uv[2] = {0, 0};
    float _tool_pick_at[3] = {0, 0, 0};

    // Camera model + per-model FOV memory (browser _fovMemory equivalent).
    int _cam_model = 0;              // index into kViewerCameraModels
    float _fov_deg[4] = {90, 180, 180, 0};

    // Whether there are training cameras behind what is being rendered. A
    // splat file has none, so the frustum controls are hidden rather than
    // shown doing nothing.
    bool _has_cameras = true;

    // Model frame -> shared navigation frame (see set_model_transform), and
    // its scale, cached because every render divides by it. `_m2s` is the
    // owner's placement composed with the levelling correction below.
    float _m2s_owner[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float _m2s[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float _m2s_scale = 1.0f;
    bool _m2s_identity = true;
    void rebuild_m2s();

    // The parsers rotate every dataset so the mean camera up axis becomes +Z,
    // a guess, and a bad one on a tilted 360 capture. `_align` is that
    // rotation; unchecking `_level_cameras` undoes it and nothing else.
    float _align[9] = {1,0,0, 0,1,0, 0,0,1};
    bool _align_identity = true;
    bool _level_cameras = true;
    bool _gauge_metric = false;
    // What the view orbits about and Reset view frames (dsparse::CenterMode),
    // a point per mode in the model frame. Moves only the camera.
    int _center_mode = (int)dsparse::CenterMode::CameraMedian;
    dsparse::CenterTable _centers{};
    bool _centers_known = false;
    bool _center_has_cameras = false;
    // Model units per unit of the navigated frame: what turns the grid's cell
    // size into a length (ParsedDataset::train_frame_scale).
    float _scene_scale = 1.0f;
    // The grid's cell in model units, from the same rule both backends use.
    float grid_cell() const;
    void draw_grid_overlay(float x, float y, int line) const;
    // The navigation gizmo in the image's corner: drag to orbit, click an
    // axis to look along it. True while it has the pointer.
    bool gizmo_input(bool hovered_image);
    void draw_gizmo() const;
    void draw_overlays();
    void animate_view(double now);
    bool external_grid() const;
    // Camera-to-world in the shared frame, pulled back when orthographic.
    void render_c2w(float out[12]) const;
    float ortho_back() const;

    // Orthographic is a pinhole a long way off with a long lens: every
    // renderer, primitive and selection test then works unchanged.
    bool _ortho = false;
    bool _ortho_auto = false;        // entered by an axis click: orbit leaves it
    // A view change in flight (axis snap): rotation slerped, pivot distance kept.
    bool _anim = false;
    double _anim_t0 = 0.0;
    float _anim_from[4] = {0, 0, 0, 1}, _anim_to[4] = {0, 0, 0, 1};
    // And a framing: the pivot and its distance glide, the rotation stays.
    bool _frame_anim = false;
    double _frame_t0 = 0.0;
    float _frame_from[4] = {0, 0, 0, 1}, _frame_to[4] = {0, 0, 0, 1};   // pivot, distance
    // Gizmo pointer state.
    bool _giz_down = false, _giz_dragged = false, _giz_hover = false;
    int _giz_hot = -1;               // 0..5: +X +Y +Z -X -Y -Z under the cursor
    int _giz_button = 0;             // 0 none, 1 pan, 2 zoom (the side buttons)
    float _giz_press[2] = {0, 0};
    mutable int _giz_tip_on = -2;    // what the tooltip timer is running for
    mutable double _giz_tip_since = 0.0;
    float _m2s_edit[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float _m2s_base[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    ViewportInteractor* _interactor = nullptr;
    std::function<bool(dsparse::CenterTable&)> _center_provider;
    // The image rectangle of the last draw, which is the frame a tool's
    // pointer coordinates and its overlay are both in.
    float _img_x = 0, _img_y = 0, _img_w = 0, _img_h = 0;
    bool _nav_controls = true;
    float _controls_h = 0.0f;
    float _controls_pad = 0.0f;
    float _image_w = 0.0f;

    // ---- render options a VIEWER may change (a training session may not:
    // what it renders has to be what it is training) ----
    // Offered only when enable_scene_options() said so.
    bool _scene_options = false;
    int _primitive_idx = 0;       // index into kViewerPrimitives
    int _sh_degree = -1;          // < 0 = every band the file carries
    int _sh_degree_max = 0;       // what the file carries (the slider's top)
    int _gamut_idx = 0;           // index into kViewerGamuts
    int _transfer_idx = 0;        // colorspace::Transfer
    bool _linear_color = false;
    // Applying a gamut / transfer change is an ENGINE call, not a render flag,
    // so it is done by the owner (SplatViewer, which holds the engine lock)
    // rather than here.
    std::function<void(const char* gamut, int transfer, bool linear)>
        _apply_color_space;
    std::function<void()> _on_primitive_changed;

    // Mesh display switches (preview mode over a mesh).
    bool _mesh_shade = true, _mesh_flat = false, _mesh_color_on = true;

    // Render options.
    int _buffer_idx = 0;
    std::vector<std::string> _buffer_keys;
    bool _show_cams = false;
    bool _show_roi = true;
    std::shared_ptr<const spirula::RegionOverlay> _roi_engine, _roi_preview;
    std::shared_ptr<const std::vector<uint8_t>> _roi_points_inside;
    bool _show_grid = false;         // axes + ground-plane grid overlay
    float _frustum_scale = 1.0f;     // camera-frustum size multiplier
    // 0 = auto (see render_scale), 1 = 50%, 2 = 75%, 3 = 100%
    int _scale_idx = 0;
    float _last_pose[13] = {};       // pos + rot + target + ortho + lens, to spot motion
    // The pose (or camera model / FOV) changed during the last draw. Drives
    // the side-by-side link; note_motion sets it, draw clears it.
    bool _moved_last_draw = false;
    double _last_move = -1e9;
    bool _moving = false;
    bool _auto_refresh = true;

    // Refresh pacing while training (see idle_step_interval): the step the
    // last render was submitted at, and exponential averages of what a render
    // and a training step each cost.
    int _last_render_step = -1;
    int _last_seen_step = -1;
    double _last_step_time = -1.0;
    double _render_secs = 0.0;
    double _step_secs = 0.0;

    // In-flight request / result texture (engine mode).
    uint64_t _pending = 0;
    double _last_submit = 0.0;
    bool _dirty = true;
    unsigned int _tex = 0;
    int _tex_w = 0, _tex_h = 0;
    std::string _last_error;
};

}  // namespace gui
