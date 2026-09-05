// ViewportPanel.cpp -- see ViewportPanel.h.

#include "app/gui/ViewportPanel.h"

#include "app/gui/Layout.h"

#include "app/TrainerCore.h"
#include "app/gui/Ui.h"

#include "i18n/catalog/Gui.h"
#include "app/gui/GlLoader.h"   // GL types + 1.1 entry points

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "core/Env.h"

namespace msg = spirula::i18n::msg::gui;

namespace gui {

namespace {

// Camera models offered by the web client (viewer.html #camera-model), with
// per-model FOV ranges (viewer.html FOV_RANGE; EQUIRECTANGULAR is a fixed
// full-sphere view with no FOV).
struct ViewerCamModel {
    const char* name;      // engine camera_model string -- never translated
    const spirula::i18n::Msg* label;
    float fov_min, fov_max;
};
const ViewerCamModel kViewerCameraModels[] = {
    {"PINHOLE",         &msg::cam_perspective,          10, 150},
    {"FISHEYE",         &msg::cam_fisheye_equidistant,  10, 360},
    {"EQUISOLID",       &msg::cam_fisheye_equisolid,    10, 360},
    {"EQUIRECTANGULAR", &msg::cam_equirectangular,       0, 0},
};

// fovToIntrinsics port: inverting the actual projection (not the pinhole tan
// map) makes the slider a true field of view for the fisheye models.
void fov_to_intrinsics(float fov_deg, int w, int h, const char* model,
                       float& fx, float& fy) {
    float fov_rad = fov_deg * 3.14159265358979f / 180.0f;
    if (!std::strcmp(model, "FISHEYE"))
        fx = (w / 2.0f) / (fov_rad / 2.0f);                     // r = f*theta
    else if (!std::strcmp(model, "EQUISOLID"))
        fx = (w / 2.0f) / (2.0f * std::sin(fov_rad / 4.0f));    // r = 2f sin(theta/2)
    else
        fx = (w / 2.0f) / std::tan(fov_rad / 2.0f);             // pinhole
    fy = fx;
}

}  // namespace

// ---------------------------------------------------------------------------
// Framing + camera math
// ---------------------------------------------------------------------------

void ViewportPanel::reset_pose(float radius) {
    // Match the web viewer's cam.reset() exactly: target = client-frame
    // origin (the normalized frame is centered on the CAMERA POSES via
    // center_method="poses", i.e. the captured object for object-centric
    // datasets -- NOT the point-cloud centroid, which distant background
    // points drag far away), pos = [0,0,1], then orbit(0, -250).
    _cam.pos[0] = 0; _cam.pos[1] = 0; _cam.pos[2] = 1;
    _cam.rot[0] = _cam.rot[1] = _cam.rot[2] = 0; _cam.rot[3] = 1;
    _cam.target[0] = _cam.target[1] = _cam.target[2] = 0;
    _cam.orbit(0, -250);
    _home = _cam;
    _home_dist = radius;
    _dirty = true;
}

void ViewportPanel::compute_framing(const spirula::TrainerSession& session) {
    reset_pose(1.0f);

    // Scene radius (drives only the preview depth range): spread of the
    // camera positions in the client frame.
    double A[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const auto& ds = session.ds;
    if (ds.train_frame_scale != 1.0f) {
        double T[16];
        for (int i = 0; i < 16; i++) T[i] = ds.train_to_normalized[i];
        dsparse::invert_affine4x4(T, A);
    }
    double radius = 1.0;
    for (int64_t i = 0; i < ds.num_cameras; i++) {
        float p[3] = {ds.c2w[i*12 + 3], ds.c2w[i*12 + 7], ds.c2w[i*12 + 11]};
        double q[3];
        for (int r = 0; r < 3; r++)
            q[r] = A[r*4+0]*p[0] + A[r*4+1]*p[1] + A[r*4+2]*p[2] + A[r*4+3];
        radius = std::max(radius, std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2]));
    }
    _home_dist = (float)radius;
    _dirty = true;
}

void ViewportPanel::maybe_frame(const spirula::TrainerSession& session) {
    std::string key = session.cfg.data + ":" +
        std::to_string(session.ds.num_cameras) + ":" +
        std::to_string(session.ds.points.num());
    if (key == _framed_key) return;   // same dataset: keep the current pose
    _framed_key = key;
    compute_framing(session);
    _show_cams = true;   // default on for a fresh dataset preview
}

void ViewportPanel::reset_view() {
    _cam = _home;
    _dirty = true;
}

void ViewportPanel::recenter_at(const float p[3]) {
    // viewer.html recenterAt: lateral pan keeps the view orientation; the
    // point becomes the orbit pivot.
    float fwd[3];
    _cam.axis_forward(fwd);
    float rel[3] = {p[0] - _cam.pos[0], p[1] - _cam.pos[1],
                    p[2] - _cam.pos[2]};
    float d = rel[0]*fwd[0] + rel[1]*fwd[1] + rel[2]*fwd[2];
    if (d > 0.0f) {
        for (int i = 0; i < 3; i++)
            _cam.pos[i] += rel[i] - d * fwd[i];
    } else {
        float up[3], eye[3] = {_cam.pos[0], _cam.pos[1], _cam.pos[2]};
        _cam.axis_up(up);
        _cam.look_at(eye, p, up);
    }
    for (int i = 0; i < 3; i++) _cam.target[i] = p[i];
    _dirty = true;
}

const char* ViewportPanel::camera_model_name() const {
    return kViewerCameraModels[_cam_model].name;
}
float ViewportPanel::fov_min() const { return kViewerCameraModels[_cam_model].fov_min; }
float ViewportPanel::fov_max() const { return kViewerCameraModels[_cam_model].fov_max; }

void ViewportPanel::compute_intrinsics(int W, int H, float& fx, float& fy) const {
    const char* model = camera_model_name();
    if (!std::strcmp(model, "EQUIRECTANGULAR")) {
        // Full 360x180 panorama across the image (viewer.html
        // sendRenderRequest): fx = w/2pi, fy = h/pi.
        fx = (float)W / (2.0f * 3.14159265358979f);
        fy = (float)H / 3.14159265358979f;
    } else {
        fov_to_intrinsics(_fov_deg[_cam_model], W, H, model, fx, fy);
    }
}

float ViewportPanel::nav_dist() const {
    float dx = _cam.pos[0] - _cam.target[0];
    float dy = _cam.pos[1] - _cam.target[1];
    float dz = _cam.pos[2] - _cam.target[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

void ViewportPanel::set_model_transform(const float a[12]) {
    // Called every frame by the owner; a render costs too much to submit one
    // for a placement that has not moved.
    if (std::memcmp(_m2s, a, sizeof _m2s) == 0) return;
    for (int i = 0; i < 12; i++) _m2s[i] = a[i];
    _m2s_scale = std::sqrt(a[0]*a[0] + a[4]*a[4] + a[8]*a[8]);
    if (!(_m2s_scale > 1e-20f)) _m2s_scale = 1.0f;
    static const float kI[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    _m2s_identity = std::memcmp(_m2s, kI, sizeof kI) == 0;
    _dirty = true;
}

// Shared -> model: R^T (x - t) / s, with the 3x3 written as s*R.
void ViewportPanel::model_point(const float shared[3], float out[3]) const {
    if (_m2s_identity) {
        for (int i = 0; i < 3; i++) out[i] = shared[i];
        return;
    }
    const float inv = 1.0f / (_m2s_scale * _m2s_scale);
    float e[3] = {shared[0] - _m2s[3], shared[1] - _m2s[7], shared[2] - _m2s[11]};
    for (int r = 0; r < 3; r++)
        out[r] = inv * (_m2s[0*4+r]*e[0] + _m2s[1*4+r]*e[1] + _m2s[2*4+r]*e[2]);
}

void ViewportPanel::shared_point(const float model[3], float out[3]) const {
    if (_m2s_identity) {
        for (int i = 0; i < 3; i++) out[i] = model[i];
        return;
    }
    for (int r = 0; r < 3; r++)
        out[r] = _m2s[r*4+0]*model[0] + _m2s[r*4+1]*model[1] +
                 _m2s[r*4+2]*model[2] + _m2s[r*4+3];
}

void ViewportPanel::model_c2w(float out[12]) const {
    _cam.c2w(out);
    if (_m2s_identity) return;
    const float s = _m2s_scale;
    float m[12];
    std::memcpy(m, out, sizeof m);
    // Rotation by R^T (orthonormal, so the basis stays a rotation); position
    // through the full inverse similarity.
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            float v = 0.0f;
            for (int k = 0; k < 3; k++) v += _m2s[k*4+r] * m[k*4+c];
            out[r*4 + c] = v / s;
        }
    float p[3] = {m[3], m[7], m[11]}, q[3];
    model_point(p, q);
    out[3] = q[0]; out[7] = q[1]; out[11] = q[2];
}

void ViewportPanel::build_request(ViewRequest& q, int W, int H) const {
    if (_scene_options) {
        q.primitive = kViewerPrimitives[_primitive_idx];
        q.sh_degree = _sh_degree;
    }
    model_c2w(q.c2w);
    compute_intrinsics(W, H, q.fx, q.fy);
    q.cx = 0.5f * (float)W;
    q.cy = 0.5f * (float)H;
    q.W = W;
    q.H = H;
    q.model = camera_model_name();
    q.key = _buffer_keys.empty() ? "rgb" : _buffer_keys[_buffer_idx];
    q.show_cams = _show_cams;
    q.show_grid = _show_grid;
    q.grid_dist = nav_dist() / _m2s_scale;
    model_point(_cam.target, q.grid_target);
    q.cam_size_scale = _frustum_scale;
}

// Row-major world-to-view for the GL preview (inverse of the camera c2w).
void ViewportPanel::view_matrix(float out[16]) const {
    float m[12];
    model_c2w(m);
    // c2w columns are the view axes; view = [R^T | -R^T pos].
    for (int r = 0; r < 3; r++) {
        float ax = m[0*4 + r], ay = m[1*4 + r], az = m[2*4 + r];
        out[r*4 + 0] = ax;
        out[r*4 + 1] = ay;
        out[r*4 + 2] = az;
        out[r*4 + 3] = -(ax*m[3] + ay*m[7] + az*m[11]);
    }
    out[12] = out[13] = out[14] = 0;
    out[15] = 1;
}

// ---------------------------------------------------------------------------
// Attach / detach
// ---------------------------------------------------------------------------

void ViewportPanel::attach_preview(spirula::TrainerSession& session) {
    detach();
    _has_cameras = true;
    if (!_preview.build(session)) {
        _last_error = "preview renderer unavailable (OpenGL 3.2 required)";
        return;
    }
    maybe_frame(session);
    _last_error.clear();
    _mode = Mode::Preview;
}

void ViewportPanel::attach_preview_data(const ParsedDataset& ds,
                                       const PostSplitCameras& post,
                                       const std::string& key, float radius,
                                       bool with_cameras) {
    const bool first = key != _framed_key;
    detach();
    _has_cameras = with_cameras;
    // Frusta on by default when there are any: watching a reconstruction is
    // watching the cameras find their places. Only on the first attach, so a
    // refresh does not undo the switch.
    if (first) _show_cams = with_cameras;
    if (!_preview.build(ds, post)) {
        _last_error = "preview renderer unavailable (OpenGL 3.2 required)";
        return;
    }
    if (first) {
        _framed_key = key;
        reset_pose(radius);
    }
    _last_error.clear();
    _mode = Mode::Preview;
}

void ViewportPanel::enable_scene_options(
    const std::string& primitive, int sh_degree_max,
    const std::string& gamut, int transfer, bool linear,
    std::function<void(const char*, int, bool)> apply_color_space,
    std::function<void()> on_primitive_changed) {
    _scene_options = true;
    _primitive_idx = 0;
    for (int i = 0; i < kNumViewerPrimitives; i++)
        if (primitive == kViewerPrimitives[i]) _primitive_idx = i;
    _sh_degree_max = std::max(0, sh_degree_max);
    // Every band the file carries, which for a slider capped at that same
    // number is simply its top end -- no separate "automatic" entry needed.
    _sh_degree = _sh_degree_max;
    _gamut_idx = 0;
    for (int i = 0; i < kNumViewerGamuts; i++)
        if (gamut == kViewerGamuts[i]) _gamut_idx = i;
    _transfer_idx = transfer;
    _linear_color = linear;
    _apply_color_space = std::move(apply_color_space);
    _on_primitive_changed = std::move(on_primitive_changed);
}

void ViewportPanel::attach_preview_mesh(const meshing::MeshData& mesh,
                                        const float to_normalized[12],
                                        const std::string& key, float radius) {
    detach();
    _has_cameras = false;
    _show_cams = false;
    if (!_preview.build(mesh, to_normalized)) {
        _last_error = "preview renderer unavailable (OpenGL 3.2 required)";
        return;
    }
    if (key != _framed_key) {
        _framed_key = key;
        reset_pose(radius);
    }
    _last_error.clear();
    _mode = Mode::Preview;
}

void ViewportPanel::attach(spirula::TrainerSession& session) {
    detach();
    _worker.start(session.make_viewer_config(), session.make_viewer_hooks());
    _buffer_keys = _worker.buffer_keys();
    _buffer_idx = std::min<int>(_buffer_idx, (int)_buffer_keys.size() - 1);
    _has_cameras = session.ds.num_cameras > 0;
    maybe_frame(session);
    _pending = 0;
    _last_error.clear();
    _mode = Mode::Engine;
}

void ViewportPanel::attach_scene(const ViewerRenderConfig& cfg,
                                 const ViewerHooks& hooks,
                                 const std::string& key, float radius) {
    detach();
    _worker.start(cfg, hooks);
    _buffer_keys = _worker.buffer_keys();
    _buffer_idx = std::min<int>(_buffer_idx, (int)_buffer_keys.size() - 1);
    _has_cameras = false;
    _show_cams = false;
    if (key != _framed_key) {
        _framed_key = key;
        reset_pose(radius);
    }
    _pending = 0;
    _last_error.clear();
    _mode = Mode::Engine;
}

void ViewportPanel::detach() {
    _scene_options = false;
    _apply_color_space = nullptr;
    _on_primitive_changed = nullptr;
    if (_mode == Mode::Engine) _worker.stop();
    _pending = 0;
    _mode = Mode::None;
}

void ViewportPanel::destroy_gl() {
    _preview.destroy_gl();
    if (_tex) {
        GLuint t = (GLuint)_tex;
        glDeleteTextures(1, &t);
        _tex = 0;
    }
}

void ViewportPanel::upload(const ViewResult& res) {
    if (!_tex) {
        GLuint t = 0;
        glGenTextures(1, &t);
        _tex = t;
        glBindTexture(GL_TEXTURE_2D, (GLuint)_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, (GLuint)_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, res.W, res.H, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, res.rgb8.data());
    _tex_w = res.W;
    _tex_h = res.H;
}

// ---------------------------------------------------------------------------
// Input (browser-identical: viewer.html event wiring)
// ---------------------------------------------------------------------------

void ViewportPanel::handle_input(float /*item_h*/) {
    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsItemHovered();

    // Pointer (mouse; single-touch and OS touch/trackpad gestures arrive as
    // emulated mouse + wheel events -- one finger orbits, two-finger
    // pan/pinch maps to the wheel). Drag mapping per viewer.html:
    //   MMB / RMB / Shift+drag -> pan
    //   LMB: orbit (turntable/trackball) or look (fps/fly)
    if (hovered && !_dragging) {
        for (int b : {ImGuiMouseButton_Left, ImGuiMouseButton_Right,
                      ImGuiMouseButton_Middle}) {
            if (ImGui::IsMouseClicked(b)) {
                _dragging = true;
                _drag_button = b;
                break;
            }
        }
    }
    if (_dragging) {
        if (!ImGui::IsMouseDown(_drag_button)) {
            _dragging = false;
        } else {
            float dx = io.MouseDelta.x, dy = io.MouseDelta.y;
            if (dx != 0 || dy != 0) {
                bool is_pan = _drag_button == ImGuiMouseButton_Right ||
                              _drag_button == ImGuiMouseButton_Middle ||
                              io.KeyShift;
                if (spirula::env("NAV_DEBUG"))
                    std::fprintf(stderr,
                        "[nav] btn=%d pan=%d shift=%d d=(%.0f,%.0f) tgt=(%.3f,%.3f,%.3f) pos=(%.3f,%.3f,%.3f)\n",
                        _drag_button, (int)is_pan, (int)io.KeyShift, dx, dy,
                        _cam.target[0], _cam.target[1], _cam.target[2],
                        _cam.pos[0], _cam.pos[1], _cam.pos[2]);
                if (is_pan)
                    _cam.pan(dx, dy);
                else if (_cam.mode == NavCamera::Turntable ||
                         _cam.mode == NavCamera::Trackball)
                    _cam.orbit(dx, dy);
                else
                    _cam.look(dx, dy);
                _dirty = true;
            }
        }
    }

    // Double-click = center the view on the 3D content under the cursor
    // (viewer.html pickRecenter). Recorded here as fractional image
    // coordinates; resolved by the mode-specific draw (preview: CPU point
    // pick, engine: depth readback on the next render).
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 sz = ImGui::GetItemRectSize();
        if (sz.x > 0 && sz.y > 0) {
            _dbl_u = (io.MousePos.x - mn.x) / sz.x;
            _dbl_v = (io.MousePos.y - mn.y) / sz.y;
            _dbl_pending = true;
            _dirty = true;
        }
    }

    // Scroll = dolly (browser wheel deltaY is ~+-100 per notch, ImGui is
    // +-1 with the opposite sign convention).
    if (hovered && io.MouseWheel != 0.0f) {
        _cam.dolly(-io.MouseWheel * 100.0f);
        _dirty = true;
    }

    // Keyboard (viewer.html listens on the window; here: while the pointer
    // is over the viewport or dragging, and no text field wants input).
    if ((hovered || _dragging) && !io.WantTextInput) {
        NavCamera::Keys k;
        k.w = ImGui::IsKeyDown(ImGuiKey_W);
        k.a = ImGui::IsKeyDown(ImGuiKey_A);
        k.s = ImGui::IsKeyDown(ImGuiKey_S);
        k.d = ImGui::IsKeyDown(ImGuiKey_D);
        k.e = ImGui::IsKeyDown(ImGuiKey_E);
        k.q = ImGui::IsKeyDown(ImGuiKey_Q);
        // The claim is what the Shortcut() calls are for: an unclaimed arrow is
        // ALSO read by imgui's nav, which walks the focus along the toolbar.
        // IsKeyDown still reads it -- ownership only filters the owner-aware.
        const ImGuiInputFlags route = ImGuiInputFlags_RouteFocused |
                                      ImGuiInputFlags_RouteFromRootWindow;
        const ImGuiKey arrows[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow,
                                   ImGuiKey_LeftArrow, ImGuiKey_RightArrow};
        for (ImGuiKey key : arrows) ImGui::Shortcut(key, route);
        k.up = ImGui::IsKeyDown(ImGuiKey_UpArrow);
        k.down = ImGui::IsKeyDown(ImGuiKey_DownArrow);
        k.left = ImGui::IsKeyDown(ImGuiKey_LeftArrow);
        k.right = ImGui::IsKeyDown(ImGuiKey_RightArrow);
        float dt = std::min(io.DeltaTime, 0.1f);
        if (_cam.keyboard_tick(dt, k)) _dirty = true;
    }

    // Gamepad: always active, like the browser's gamepadTick loop.
    {
        float dt = std::min(io.DeltaTime, 0.1f);
        if (_cam.gamepad_tick(dt)) _dirty = true;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void ViewportPanel::draw_controls(bool engine) {
    // The controls pack greedily onto as many rows as they need: an item goes
    // on the current row when what is left of it can hold the item, and starts
    // a new row otherwise. A fixed width threshold cannot do this -- WHICH
    // controls are present depends on what is being shown (a viewer has render
    // options a training session has not; a mesh has display switches a splat
    // has not) and the panel is half-width when two are side by side, so a
    // threshold calibrated for one combination overflows another.
    const ImGuiStyle& st = ImGui::GetStyle();
    const float row_w = ImGui::GetContentRegionAvail().x;
    auto text_w = [](const char* s) { return ImGui::CalcTextSize(s).x; };
    auto check_w = [&](const spirula::i18n::Msg& m) {
        return ImGui::GetFrameHeight() + st.ItemInnerSpacing.x +
               text_w(m.get());
    };
    auto button_w = [&](const spirula::i18n::Msg& m) {
        return text_w(m.get()) + 2.0f * st.FramePadding.x;
    };
    // How much of the current row is used. TRACKED rather than read back from
    // ImGui's last-item rectangle, which would pick up the tooltip's contents
    // whenever one of these controls is hovered -- the row would then reflow
    // under the cursor.
    float used = 0.0f;
    auto place = [&](float w) {
        if (used > 0.0f && used + st.ItemSpacing.x + w <= row_w) {
            ImGui::SameLine();
            used += st.ItemSpacing.x + w;
        } else {
            used = w;    // first item, or a new row: no SameLine
        }
    };
    // Keep a group of related controls together: if the whole of it will not
    // fit on what is left of this row, start it on a fresh one.
    auto place_group = [&](float w) {
        if (used > 0.0f && used + st.ItemSpacing.x + w > row_w) used = 0.0f;
    };

    // The SH slider's width. Short enough to sit in a row of controls, wide
    // enough for "SH 3" plus the grab.
    constexpr float kShW = 86.0f;

    // ---- display group ----
    if (engine && !_buffer_keys.empty()) {
        place(px(130.0f));
        ImGui::SetNextItemWidth(px(130.0f));
        // Buffer names ("color", "depth") come from the engine.
        if (ui::BeginComboRaw("##buffer", _buffer_keys[_buffer_idx].c_str())) {
            for (int i = 0; i < (int)_buffer_keys.size(); i++)
                if (ui::SelectableRaw(_buffer_keys[i], i == _buffer_idx)) {
                    _buffer_idx = i;
                    _dirty = true;
                }
            ImGui::EndCombo();
        }
        ui::help_on_hover(msg::viewport_buffer_help);
    }
    // The "dataset preview" note explains why the image is points and not a
    // render; over a mesh it would be simply wrong.
    if (!engine && !_preview.has_mesh()) {
        place(text_w(msg::viewport_dataset_preview.get()));
        ui::TextDisabled(msg::viewport_dataset_preview);
        ui::help_on_hover(msg::viewport_dataset_preview_help);
    }
    // No dataset behind a splat file, so nothing to draw a frustum for.
    if (_has_cameras) {
        place(check_w(msg::viewport_cameras));
        if (ui::Checkbox(msg::viewport_cameras, &_show_cams)) _dirty = true;
        if (_show_cams) {
            place(px(110.0f));
            ImGui::SetNextItemWidth(px(110.0f));
            if (ui::SliderFloatRaw("##fsize", &_frustum_scale, 0.1f, 10.0f,
                                   "size x%.2f", ImGuiSliderFlags_Logarithmic))
                _dirty = true;
            ui::help_on_hover(msg::viewport_frustum_size_help);
        }
    }
    place(check_w(msg::viewport_grid));
    if (ui::Checkbox(msg::viewport_grid, &_show_grid)) _dirty = true;
    ui::help_on_hover(msg::viewport_cameras_help);
    if (engine) {
        place(px(66.0f) + st.ItemInnerSpacing.x +
              text_w(msg::viewport_scale.get()));
        ImGui::SetNextItemWidth(px(66.0f));
        // Only the first entry is a word; the rest are numbers, and a
        // percentage is a percentage in every language.
        const char* scales[] = {msg::viewport_scale_auto.get(),
                                "50%", "75%", "100%"};
        if (ui::ComboRaw(ui::detail::label(msg::viewport_scale), &_scale_idx,
                         scales, 4))
            _dirty = true;
        ui::help_on_hover(msg::viewport_scale_help);
        // "Live" is about keeping up with training; a file does not move.
        if (_has_cameras) {
            place(check_w(msg::viewport_live));
            ui::Checkbox(msg::viewport_live, &_auto_refresh);
            ui::help_on_hover(msg::viewport_live_help);
        }
    }
    // ---- mesh display switches (preview over a mesh) ----
    if (!engine && _preview.has_mesh()) {
        place(check_w(msg::viewport_shading));
        if (ui::Checkbox(msg::viewport_shading, &_mesh_shade)) _dirty = true;
        if (_mesh_shade) {
            place(check_w(msg::viewport_flat_shading));
            if (ui::Checkbox(msg::viewport_flat_shading, &_mesh_flat))
                _dirty = true;
            ui::help_on_hover(msg::viewport_flat_shading_help);
        }
        // Nothing to switch off when the mesh has no color of its own.
        if (_preview.mesh_color_kind() != 0) {
            place(check_w(msg::viewport_mesh_color));
            if (ui::Checkbox(msg::viewport_mesh_color, &_mesh_color_on))
                _dirty = true;
            ui::help_on_hover(msg::viewport_mesh_color_help);
        }
        _preview.set_mesh_display(_mesh_shade, _mesh_flat, _mesh_color_on);
    }

    // ---- how the model is rendered (viewer only) ----
    // A training session renders what it is training; only a file being
    // LOOKED at can be drawn a different way than it was made.
    if (engine && _scene_options) {
        float w_group = px(80.0f) + px(120.0f) + px(120.0f) +
                        check_w(msg::viewport_linear_color) +
                        3.0f * st.ItemSpacing.x;
        if (_sh_degree_max > 0) w_group += px(kShW) + st.ItemSpacing.x;
        place_group(w_group);
        place(px(80.0f));
        ImGui::SetNextItemWidth(px(80.0f));
        // Primitive names are identifiers (they are what --primitive takes).
        if (ui::ComboRaw("##primitive", &_primitive_idx, kViewerPrimitives,
                         kNumViewerPrimitives)) {
            if (_on_primitive_changed) _on_primitive_changed();
            _dirty = true;
        }
        ui::help_on_hover(msg::viewport_primitive_help);

        if (_sh_degree_max > 0) {
            place(px(kShW));
            ImGui::SetNextItemWidth(px(kShW));
            // Capped at what the file actually carries -- bands it has not got
            // cannot be drawn, so there is nothing above the top end to offer.
            // Degrees are numbers in every language.
            if (ui::SliderIntRaw("##shdeg", &_sh_degree, 0, _sh_degree_max,
                                 "SH %d"))
                _dirty = true;
            ui::help_on_hover(msg::viewport_sh_degree_help);
        }

        place(px(120.0f));
        ImGui::SetNextItemWidth(px(120.0f));
        // Gamut names are standards ("DCI-P3"); only the "none" row is a word.
        const char* gamuts[kNumViewerGamuts];
        gamuts[0] = msg::viewport_gamut_none.get();
        for (int i = 1; i < kNumViewerGamuts; i++) gamuts[i] = kViewerGamuts[i];
        auto apply = [&] {
            if (_apply_color_space)
                _apply_color_space(kViewerGamuts[_gamut_idx], _transfer_idx,
                                   _linear_color);
            _dirty = true;
        };
        if (ui::ComboRaw("##gamut", &_gamut_idx, gamuts, kNumViewerGamuts))
            apply();
        ui::help_on_hover(msg::viewport_gamut_help);
        place(check_w(msg::viewport_linear_color));
        if (ui::Checkbox(msg::viewport_linear_color, &_linear_color)) apply();
        ui::help_on_hover(msg::viewport_linear_help);
        place(px(120.0f));
        ImGui::SetNextItemWidth(px(120.0f));
        // Transfer names are the identifiers --splat-color-transfer takes.
        if (ui::ComboRaw("##transfer", &_transfer_idx, colorspace::kTransfers,
                         kNumViewerTransfers))
            apply();
        ui::help_on_hover(msg::viewport_transfer_help);
    }

    if (_nav_controls) draw_nav_controls();
}

// The four modes behave exactly as the web viewer's do; only the words are
// localized. The tooltip names them by substitution so it always uses the same
// words the combo just showed.
void ViewportPanel::draw_nav_controls() {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float row_w = ImGui::GetContentRegionAvail().x;
    auto text_w = [](const char* s) { return ImGui::CalcTextSize(s).x; };
    auto button_w = [&](const spirula::i18n::Msg& m) {
        return text_w(m.get()) + 2.0f * st.FramePadding.x;
    };
    float used = 0.0f;
    auto place = [&](float w) {
        if (used > 0.0f && used + st.ItemSpacing.x + w <= row_w) {
            ImGui::SameLine();
            used += st.ItemSpacing.x + w;
        } else {
            used = w;
        }
    };
    auto place_group = [&](float w) {
        if (used > 0.0f && used + st.ItemSpacing.x + w > row_w) used = 0.0f;
    };

    place(button_w(msg::viewport_reset_view));
    if (ui::Button(msg::viewport_reset_view)) reset_view();

    static const spirula::i18n::Msg* kNavModes[] = {
        &msg::nav_turntable, &msg::nav_trackball,
        &msg::nav_first_person, &msg::nav_free_fly};
    const float w_fov = fov_max() > 0 ? px(120.0f)
                                      : text_w("360\xc2\xb0 x 180\xc2\xb0");
    place_group(px(150.0f) + px(100.0f) + px(160.0f) + w_fov +
                3.0f * st.ItemSpacing.x);
    int nav = (int)_cam.mode;
    place(px(150.0f));
    ImGui::SetNextItemWidth(px(150.0f));
    if (ui::ComboRaw("##navmode", &nav,
                     {kNavModes[0], kNavModes[1], kNavModes[2], kNavModes[3]}))
        _cam.mode = (NavCamera::Mode)nav;
    ui::help_on_hover(msg::viewport_nav_help,
                      {kNavModes[0]->get(), kNavModes[1]->get(),
                       kNavModes[2]->get(), kNavModes[3]->get()});
    place(px(100.0f));
    ImGui::SetNextItemWidth(px(100.0f));
    ui::SliderFloatRaw("##speed", &_cam.speed_exp, -2.0f, 2.0f, "spd 10^%.1f");
    ui::help_on_hover(msg::viewport_speed_help);
    place(px(160.0f));
    ImGui::SetNextItemWidth(px(160.0f));
    if (ui::BeginComboRaw("##cammodel", kViewerCameraModels[_cam_model].label->get())) {
        for (int i = 0; i < 4; i++)
            if (ui::Selectable(*kViewerCameraModels[i].label, i == _cam_model)) {
                _cam_model = i;
                // Clamp the remembered FOV into the new model's range
                // (viewer.html _syncCameraModelUI).
                if (fov_max() > 0)
                    _fov_deg[i] = std::clamp(_fov_deg[i], fov_min(), fov_max());
                _dirty = true;
            }
        ImGui::EndCombo();
    }
    ui::help_on_hover(msg::viewport_projection_help);
    place(w_fov);
    if (fov_max() > 0) {
        ImGui::SetNextItemWidth(px(120.0f));
        if (ui::SliderFloatRaw("##fov", &_fov_deg[_cam_model],
                               fov_min(), fov_max(), "fov %.0f\xc2\xb0"))
            _dirty = true;
        ui::help_on_hover(msg::viewport_fov_help);
    } else {
        // Equirectangular has no field of view to set: the image IS the whole
        // sphere.
        ui::TextDisabledRaw("360\xc2\xb0 x 180\xc2\xb0");
        ui::help_on_hover(msg::viewport_fov_help);
    }
}

void ViewportPanel::draw(bool training, int step) {
    const float y0 = ImGui::GetCursorPosY();
    draw_controls(_mode == Mode::Engine);
    _controls_h = ImGui::GetCursorPosY() - y0;
    if (_controls_pad > 0.0f) ImGui::Dummy(ImVec2(1.0f, _controls_pad));

    const double now = ImGui::GetTime();
    note_motion(now);
    // Seconds per training step, from the steps the caller reports. Measured
    // here rather than taken from the trainer's own timing because what the
    // refresh has to be paced against is how fast the number it displays is
    // moving, which is the same thing the user sees.
    if (step >= 0 && step != _last_seen_step) {
        if (_last_seen_step >= 0 && step > _last_seen_step && _last_step_time > 0) {
            double per = (now - _last_step_time) / (step - _last_seen_step);
            _step_secs = _step_secs > 0 ? 0.8 * _step_secs + 0.2 * per : per;
        }
        _last_seen_step = step;
        _last_step_time = now;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 64.0f);
    avail.y = std::max(avail.y, 64.0f);

    if (_mode == Mode::None) {
        ImGui::Dummy(ImVec2(avail.x, avail.y * 0.4f));
        const char* line = _last_error.empty()
            ? msg::viewport_open_a_dataset.get() : _last_error.c_str();
        float tw = ImGui::CalcTextSize(line).x;
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowWidth() - tw) * 0.5f));
        ui::TextDisabledRaw(line);
        return;
    }

    if (_mode == Mode::Preview) draw_preview(avail);
    else                        draw_engine(training, avail, step);
}

void ViewportPanel::draw_preview(const ImVec2& avail) {
    int W = (int)std::clamp(avail.x, 64.0f, 4096.0f);
    int H = (int)std::clamp(avail.y, 64.0f, 4096.0f);
    float view[16];
    view_matrix(view);
    // Same intrinsics as the engine render request -> seamless transition.
    float fx, fy;
    compute_intrinsics(W, H, fx, fy);
    float target[3];
    model_point(_cam.target, target);
    unsigned tex = _preview.render(W, H, view,
                                   (PreviewProjection)_cam_model,
                                   fx / (0.5f * W), fy / (0.5f * H),
                                   _home_dist, nav_dist() / _m2s_scale, target,
                                   _show_cams, _frustum_scale, _show_grid);
    if (!tex) {
        ui::TextDisabled(msg::viewport_render_failed);
        return;
    }
    // FBO textures are bottom-up; flip V.
    ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2((float)W, (float)H),
                 ImVec2(0, 1), ImVec2(1, 0));
    handle_input((float)H);

    // Double-click centering: CPU pick against the displayed point cloud
    // (nearest point along the cursor ray, 3% angular cone).
    if (_dbl_pending) {
        _dbl_pending = false;
        float u = (_dbl_u - 0.5f) * (float)W / fx;
        float v = (_dbl_v - 0.5f) * (float)H / fy;
        float dcv[3];
        if (viewer_pixel_ray(_cam_model, u, v, dcv)) {
            float m[12];
            model_c2w(m);
            float ro[3] = {m[3], m[7], m[11]}, rd[3], p[3];
            // CV ray -> world through the OpenGL c2w (y/z column flip).
            for (int r = 0; r < 3; r++)
                rd[r] = m[r*4+0]*dcv[0] - m[r*4+1]*dcv[1] - m[r*4+2]*dcv[2];
            float hit[3];
            if (_preview.pick_point(ro, rd, p)) {
                shared_point(p, hit);
                recenter_at(hit);
            }
        }
    }

    // A count and what is being counted, which depends on what is being
    // previewed. Labelled rather than inflected ("Triangles: 12", not
    // "12 triangles") so no language needs a plural rule for it, and kept
    // short because it sits on top of the image.
    const std::string info =
        _preview.has_mesh()
            ? spirula::i18n::format(spirula::i18n::msg::gui::overlay_triangles,
                                    {(long long)_preview.num_triangles()})
            : spirula::i18n::format(spirula::i18n::msg::gui::overlay_points,
                                    {(long long)_preview.num_points()});
    ImVec2 p = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + 8, p.y + 6),
                                        IM_COL32(200, 200, 200, 180),
                                        info.c_str());
}

// Adaptive render scale (`Auto`, the default).
//
// A trained scene at full viewport resolution costs enough per frame that
// dragging the camera feels like dragging a slideshow -- and during training
// it is also time taken from the trainer. Half resolution while the camera
// moves and full resolution once it settles gets both: the frame you are
// steering by is cheap, and the frame you stop to look at is sharp.
//
// Motion is detected by comparing the pose to last frame's rather than by
// hooking the six places that move it (mouse, wheel, keyboard, gamepad, view
// reset, double-click recentre) -- one probe cannot miss one of them. It runs
// in every scale mode, not only `Auto`: the same signal now also decides how
// often a render is submitted, and a camera being dragged at a fixed 100%
// must not look idle.
void ViewportPanel::note_motion(double now) {
    constexpr double kSettle = 0.25;   // seconds of stillness before full res
    float pose[10];
    for (int i = 0; i < 3; i++) pose[i] = _cam.pos[i];
    for (int i = 0; i < 4; i++) pose[3 + i] = _cam.rot[i];
    for (int i = 0; i < 3; i++) pose[7 + i] = _cam.target[i];
    _moved_last_draw = std::memcmp(pose, _last_pose, sizeof pose) != 0;
    if (_moved_last_draw) {
        std::memcpy(_last_pose, pose, sizeof pose);
        _last_move = now;
    }

    const bool moving = (now - _last_move) < kSettle;
    // Crossing back into stillness has to force one more render, or the
    // preview would sit at half resolution until something else dirtied it.
    if (moving != _moving) {
        _moving = moving;
        _dirty = true;
    }
}

// Side-by-side link: the whole navigation state, so the two panels are one
// view shown two ways. The camera model and its FOV travel with the pose --
// a fisheye splat render next to a pinhole mesh render is not a comparison.
void ViewportPanel::sync_view_from(const ViewportPanel& src) {
    if (&src == this) return;
    if (std::memcmp(&_cam, &src._cam, sizeof(NavCamera)) == 0 &&
        _cam_model == src._cam_model &&
        _fov_deg[_cam_model] == src._fov_deg[src._cam_model])
        return;
    _cam = src._cam;
    _cam_model = src._cam_model;
    for (int i = 0; i < 4; i++) _fov_deg[i] = src._fov_deg[i];
    _home = src._home;
    _home_dist = src._home_dist;
    _dirty = true;
}

float ViewportPanel::render_scale() {
    if (_scale_idx != 0)
        return _scale_idx == 1 ? 0.5f : _scale_idx == 2 ? 0.75f : 1.0f;
    return _moving ? 0.5f : 1.0f;
}

// Render resolution, ASPECT PRESERVED.
//
// Clamping width and height independently is what put black bars down the
// sides of a wide window: a 2560x1350 viewport at 100% became a 1920x1350
// render, which the aspect-fit blit below then drew only 1920 wide -- while
// `Auto`'s half-resolution path, being under the cap, filled the width. The
// image changed size every time the camera stopped moving, which is the
// hiccup. One factor for both axes cannot do that.
void ViewportPanel::render_size(const ImVec2& avail, int& W, int& H) const {
    // A budget rather than a per-axis cap: what has to be bounded is the
    // render's cost -- pixels, and the trainer time they take. 4K's worth is
    // enough that "100%" means 100% on any ordinary display.
    constexpr double kMaxPixels = 3840.0 * 2160.0;
    constexpr double kMaxDim = 4096.0;   // texture-size safety
    const float scale = _scale_idx != 0
        ? (_scale_idx == 1 ? 0.5f : _scale_idx == 2 ? 0.75f : 1.0f)
        : (_moving ? 0.5f : 1.0f);
    double w = std::max(1.0, (double)avail.x * scale);
    double h = std::max(1.0, (double)avail.y * scale);
    double k = 1.0;
    if (w * h > kMaxPixels) k = std::sqrt(kMaxPixels / (w * h));
    if (w * k > kMaxDim) k = kMaxDim / w;
    if (h * k > kMaxDim) k = kMaxDim / h;
    // The floor is per-axis on purpose: a viewport dragged down to a sliver is
    // not worth preserving the aspect of, and the engine wants real pixels.
    W = (int)std::max(64L, std::lround(w * k));
    H = (int)std::max(64L, std::lround(h * k));
}

// How many training iterations pass between refreshes while the camera is
// still.
//
// Iterations rather than seconds, because a fixed interval in seconds spends a
// large share of a cheap dataset's training and a rounding error of a large
// one's. The count is sized from the ratio of the two measurements: a render
// that costs 8% of the steps it displaces is a viewport nobody notices, and
// the same 8% is a different number of steps on a 20 ms step than on a 200 ms
// one. Until both averages exist, the floor applies.
int ViewportPanel::idle_step_interval() const {
    constexpr double kBudget = 0.08;   // share of training time the viewport may take
    if (_render_secs <= 0.0 || _step_secs <= 0.0) return 20;
    return (int)std::clamp(std::ceil(_render_secs / (kBudget * _step_secs)),
                           15.0, 600.0);
}

void ViewportPanel::draw_engine(bool training, const ImVec2& avail, int step) {
    const double now = ImGui::GetTime();

    // Poll the in-flight render.
    if (_pending) {
        ViewResult res;
        if (_worker.try_get_result(_pending, res)) {
            _pending = 0;
            // What that render cost, for idle_step_interval(); timed on the
            // worker (ViewResult::secs), so the frames this panel spent not
            // being drawn -- the other preview mode, another screen -- are not
            // charged to it.
            if (res.secs > 0) _render_secs = _render_secs > 0
                ? 0.8 * _render_secs + 0.2 * res.secs : res.secs;
            if (res.error.empty()) {
                upload(res);
                _last_error.clear();
                // Double-click centering result (background clicks miss).
                if (res.pick_hit) {
                    float hit[3];
                    shared_point(res.pick_point, hit);
                    recenter_at(hit);
                }
            } else {
                _last_error = res.error;
            }
        }
    }

    // Submit a fresh render when needed.
    //
    // Three cases: something changed (_dirty), the user is steering (refresh
    // as fast as the worker will take it, since a stale frame is what makes
    // navigation feel broken), or the picture is simply keeping up with
    // training -- and that last one is paced in ITERATIONS, so a run whose
    // steps take 10 ms and one whose steps take a second both give the
    // viewport about the same slice of themselves.
    int W = 0, H = 0;
    render_size(avail, W, H);
    // The viewport changed size -- the window was resized, or a pane was added
    // or removed beside this one. Without this the last render is aspect-fit
    // into the new box, black bars and all, until something else dirties it.
    if (W != _tex_w || H != _tex_h) _dirty = true;
    bool live = false;
    if (training && _auto_refresh) {
        if (_moving)
            live = true;
        else if (step >= 0)
            live = _last_render_step < 0 ||
                   step - _last_render_step >= idle_step_interval();
        else
            live = now - _last_submit > 1.0;   // no step to count: 1 Hz
    }
    bool want = _dirty || live;
    if (!_pending && want) {
        ViewRequest q;
        build_request(q, W, H);
        // Attach a pending double-click pick to this render; the picked
        // point comes back with the result (depth readback, no extra pass).
        if (_dbl_pending) {
            q.pick_px = (int)(_dbl_u * (float)W);
            q.pick_py = (int)(_dbl_v * (float)H);
            _dbl_pending = false;
        }
        _pending = _worker.submit(q);
        _last_submit = now;
        _last_render_step = step;
        _dirty = false;
    }

    // Draw the last result, aspect-fit.
    if (_tex && _tex_w > 0) {
        float ar_img = (float)_tex_w / (float)_tex_h;
        float ar_avail = avail.x / avail.y;
        ImVec2 size = ar_img > ar_avail
            ? ImVec2(avail.x, avail.x / ar_img)
            : ImVec2(avail.y * ar_img, avail.y);
        ImVec2 pad((avail.x - size.x) * 0.5f, (avail.y - size.y) * 0.5f);
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + pad.x,
                                   ImGui::GetCursorPosY() + pad.y));
        ImGui::Image((ImTextureID)(intptr_t)_tex, size);
        handle_input(size.y);
    } else {
        ImGui::Dummy(ImVec2(avail.x, avail.y * 0.4f));
        const char* line = _last_error.empty() ? msg::viewport_rendering.get()
                                               : _last_error.c_str();
        float tw = ImGui::CalcTextSize(line).x;
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowWidth() - tw) * 0.5f));
        ui::TextDisabledRaw(line);
    }
    if (!_last_error.empty() && _tex)
        ui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), msg::viewport_render_error,
                        {_last_error});
}

}  // namespace gui
