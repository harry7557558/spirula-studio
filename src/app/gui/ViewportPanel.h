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
#include "app/gui/NavCamera.h"
#include "app/gui/PreviewRenderer.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct ImVec2;
namespace spirula { class TrainerSession; }

namespace gui {

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
                             bool with_cameras = false);
    // The same GL preview over an extracted triangle mesh, shaded.
    // `to_normalized` is the row-major 3x4 similarity into the navigated
    // frame (PreviewRenderer's convention); nullptr for identity.
    void attach_preview_mesh(const meshing::MeshData& mesh,
                             const float to_normalized[12],
                             const std::string& key, float radius = 1.0f);
    // Engine renderer (needs engine_ready).
    void attach(spirula::TrainerSession& session);
    // Engine renderer over something that is not a training session -- a splat
    // file opened in the viewer (SplatViewer). `key` identifies the scene, so
    // reopening the same file keeps the pose; `radius` is the scene radius in
    // the client's normalized frame, which is 1 for anything normalized.
    // There are no training cameras behind this, so the frustum controls are
    // not offered.
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

    // Where this panel's model sits in the SHARED frame the camera navigates
    // (row-major 3x4 similarity, scale*R | t; identity by default). Applied to
    // the CAMERA, not the geometry, so moving a model costs nothing.
    void set_model_transform(const float a[12]);

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

private:
    enum class Mode { None, Preview, Engine };

    void compute_framing(const spirula::TrainerSession& session);
    // The client-frame default pose (web viewer cam.reset() + orbit(0,-250)).
    void reset_pose(float radius);
    // Frame the scene only when a different dataset arrives; a preview ->
    // engine transition on the same dataset keeps the navigated pose and
    // intrinsics (no jump when training starts).
    void maybe_frame(const spirula::TrainerSession& session);
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

    // Camera model + per-model FOV memory (browser _fovMemory equivalent).
    int _cam_model = 0;              // index into kViewerCameraModels
    float _fov_deg[4] = {90, 180, 180, 0};

    // Whether there are training cameras behind what is being rendered. A
    // splat file has none, so the frustum controls are hidden rather than
    // shown doing nothing.
    bool _has_cameras = true;

    // Model frame -> shared navigation frame (see set_model_transform), and
    // its scale, cached because every render divides by it.
    float _m2s[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float _m2s_scale = 1.0f;
    bool _m2s_identity = true;
    bool _nav_controls = true;
    float _controls_h = 0.0f;
    float _controls_pad = 0.0f;

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
    bool _show_grid = false;         // axes + ground-plane grid overlay
    float _frustum_scale = 1.0f;     // camera-frustum size multiplier
    // 0 = auto (see render_scale), 1 = 50%, 2 = 75%, 3 = 100%
    int _scale_idx = 0;
    float _last_pose[10] = {};       // pos + rot + target, to spot motion
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
