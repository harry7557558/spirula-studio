// ViewportPanel.cpp -- see ViewportPanel.h.

#include "app/gui/ViewportPanel.h"

#include "app/gui/Layout.h"

#include "app/TrainerCore.h"
#include "app/gui/Ui.h"

#include "i18n/catalog/Gui.h"
#include "i18n/catalog/TrainFields.h"
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

// Orthographic as a pinhole this many times further off with a lens this
// many times longer. At 256 a box as deep as the view distance changes size
// by 0.4% front to back, and float still resolves 3e-5 of that distance.
constexpr float kOrthoPull = 256.0f;
// An axis snap turns the view over this long rather than jumping: the eye
// keeps track of which way up the model is.
constexpr double kSnapSeconds = 0.18;
// A saved view glides in over this long: far enough to follow, short enough
// not to wait for.
constexpr double kGlideSeconds = 0.3;

void quat_slerp(const float a[4], const float b[4], float t, float out[4]) {
    float d = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    float sgn = d < 0 ? -1.0f : 1.0f;
    d = std::fabs(d);
    float ka = 1.0f - t, kb = t;
    if (d < 0.9995f) {
        const float th = std::acos(d), sn = std::sin(th);
        ka = std::sin((1.0f - t) * th) / sn;
        kb = std::sin(t * th) / sn;
    }
    float n = 0.0f;
    for (int i = 0; i < 4; i++) {
        out[i] = ka * a[i] + kb * sgn * b[i];
        n += out[i] * out[i];
    }
    n = std::sqrt(std::max(n, 1e-20f));
    for (int i = 0; i < 4; i++) out[i] /= n;
}

// a after b, both row-major 3x4.
void compose_3x4(const float a[12], const float b[12], float out[12]) {
    float o[12];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            float v = c == 3 ? a[r*4+3] : 0.0f;
            for (int k = 0; k < 3; k++) v += a[r*4+k] * b[k*4+c];
            o[r*4+c] = v;
        }
    }
    std::memcpy(out, o, sizeof o);
}

// The eye relative to the pivot, in the camera's own axes.
void eye_offset(const NavCamera& c, float out[3]) {
    float m[12];
    c.c2w(m);
    const float d[3] = {c.pos[0] - c.target[0], c.pos[1] - c.target[1],
                        c.pos[2] - c.target[2]};
    for (int k = 0; k < 3; k++) out[k] = m[0*4+k]*d[0] + m[1*4+k]*d[1] + m[2*4+k]*d[2];
}

bool same_pose(const NavCamera& a, const NavCamera& b) {
    return std::memcmp(a.pos, b.pos, sizeof a.pos) == 0 &&
           std::memcmp(a.rot, b.rot, sizeof a.rot) == 0 &&
           std::memcmp(a.target, b.target, sizeof a.target) == 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Framing + camera math
// ---------------------------------------------------------------------------

void ViewportPanel::reset_pose(float radius) {
    // The web viewer's cam.reset() about the chosen centre: target = centre,
    // pos = centre + up, looking down it, then orbit(0, -250).
    float c[3];
    center_shared(c);
    const float* u = _cam.world_up;
    for (int k = 0; k < 3; k++) {
        _cam.pos[k] = c[k] + u[k];
        _cam.target[k] = c[k];
    }
    // The shortest turn taking the camera's back axis, +Z, onto `u`.
    if (u[2] < -0.9999f) {
        _cam.rot[0] = 1; _cam.rot[1] = _cam.rot[2] = _cam.rot[3] = 0;
    } else {
        const float w = 1.0f + u[2];
        const float n = std::sqrt(u[1]*u[1] + u[0]*u[0] + w*w);
        _cam.rot[0] = -u[1] / n; _cam.rot[1] = u[0] / n; _cam.rot[2] = 0; _cam.rot[3] = w / n;
    }
    _cam.orbit(0, -250);
    _home = _cam;
    _home_dist = radius;
    _dirty = true;
}

void ViewportPanel::set_centers(const dsparse::CenterTable* centers, bool has_cameras) {
    _centers_known = centers != nullptr;
    _center_has_cameras = has_cameras;
    if (centers) _centers = *centers;
}

int ViewportPanel::effective_center_mode() const {
    using M = dsparse::CenterMode;
    if (_center_has_cameras) return _center_mode;
    switch ((M)_center_mode) {
        case M::CameraMedian: return (int)M::PointMedian;
        case M::CameraMean:
        case M::CameraFocus:  return (int)M::PointMean;
        default:              return _center_mode;
    }
}

void ViewportPanel::center_shared(float out[3]) const {
    if (!_centers_known) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    shared_point(_centers[_center_mode].data(), out);
}

void ViewportPanel::compute_framing(const spirula::TrainerSession& session) {
    reset_pose(1.0f);

    // Scene radius (drives only the preview depth range): spread of the
    // camera positions in the client frame.
    const auto& ds = session.ds;
    double A[16];
    dsparse::train_to_normalized_inverse(ds, A);
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

bool ViewportPanel::maybe_frame(const spirula::TrainerSession& session) {
    std::string key = session.cfg.data + ":" +
        std::to_string(session.ds.num_cameras) + ":" +
        std::to_string(session.ds.points.num());
    if (key == _framed_key) return false;   // same dataset: keep the pose
    _framed_key = key;
    const dsparse::CenterTable centers = dsparse::scene_centers(session.ds);
    set_centers(&centers, session.ds.num_cameras > 0);
    compute_framing(session);
    _show_cams = true;   // default on for a fresh dataset preview
    return true;
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
const char* ViewportPanel::camera_model_label(int model) {
    return kViewerCameraModels[std::clamp(model, 0, 3)].label->get();
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
    if (ortho_back() > 0.0f) {
        fx *= kOrthoPull;
        fy *= kOrthoPull;
    }
}

// The distance the render camera stands behind the navigated one. The depth
// that keeps its size is the pivot's, which is what an orbit turns about.
float ViewportPanel::ortho_back() const {
    if (!_ortho || _cam_model != 0) return 0.0f;
    float f[3];
    _cam.axis_forward(f);
    float d = (_cam.target[0] - _cam.pos[0]) * f[0] +
              (_cam.target[1] - _cam.pos[1]) * f[1] +
              (_cam.target[2] - _cam.pos[2]) * f[2];
    if (!(d > 1e-6f)) d = std::max(nav_dist(), 1e-6f);
    return d * (kOrthoPull - 1.0f);
}

float ViewportPanel::ortho_pullback(bool shared) const {
    const float b = ortho_back();
    return shared ? b : b / _m2s_scale;
}

void ViewportPanel::render_c2w(float out[12]) const {
    _cam.c2w(out);
    const float back = ortho_back();
    if (back <= 0.0f) return;
    float f[3];
    _cam.axis_forward(f);
    out[3] -= f[0] * back;
    out[7] -= f[1] * back;
    out[11] -= f[2] * back;
}

void ViewportPanel::set_ortho(bool on) {
    if (on && _cam_model != 0) {
        _cam_model = 0;
        _fov_deg[0] = std::clamp(_fov_deg[0], fov_min(), fov_max());
    }
    if (_ortho == on && !_ortho_auto) return;
    _ortho = on;
    _ortho_auto = false;
    _dirty = true;
}

void ViewportPanel::snap_view(int axis, bool negative) {
    axis = std::clamp(axis, 0, 2);
    const float dist = std::max(nav_dist(), 1e-6f);
    float dir[3] = {0, 0, 0};
    dir[axis] = negative ? -1.0f : 1.0f;
    const float eye[3] = {_cam.target[0] + dir[0] * dist,
                          _cam.target[1] + dir[1] * dist,
                          _cam.target[2] + dir[2] * dist};
    // Looking straight down +Z has no up left in +Z; +Y is what a plan view
    // puts at the top of the page.
    const float up_z[3] = {0, 0, 1}, up_y[3] = {0, 1, 0};
    NavCamera to = _cam;
    const float tgt[3] = {_cam.target[0], _cam.target[1], _cam.target[2]};
    to.look_at(eye, tgt, axis == 2 ? up_y : up_z);
    std::memcpy(_anim_from, _cam.rot, sizeof _anim_from);
    std::memcpy(_anim_to, to.rot, sizeof _anim_to);
    _anim = true;
    _anim_t0 = ImGui::GetTime();
    if (_cam_model != 0) _cam_model = 0;
    _ortho = true;
    _ortho_auto = true;
    _dirty = true;
}

void ViewportPanel::carry_view(const float d[12]) {
    auto carry = [&](NavCamera& cam) {
        float fwd[3];
        cam.axis_forward(fwd);
        float pos[3], tgt[3];
        for (int r = 0; r < 3; r++) {
            pos[r] = d[r*4]*cam.pos[0] + d[r*4+1]*cam.pos[1] + d[r*4+2]*cam.pos[2] + d[r*4+3];
            tgt[r] = d[r*4]*cam.target[0] + d[r*4+1]*cam.target[1] +
                     d[r*4+2]*cam.target[2] + d[r*4+3];
        }
        // The pivot can sit off the optical axis after a pan; what is looked
        // AT is the point straight ahead at the pivot's distance.
        const float scale = std::sqrt(d[0]*d[0] + d[4]*d[4] + d[8]*d[8]);
        float dist = 0.0f;
        for (int k = 0; k < 3; k++) dist += (cam.target[k] - cam.pos[k]) * fwd[k];
        dist = std::max(std::fabs(dist), 1e-6f) * scale;
        float f2[3], ahead[3];
        for (int r = 0; r < 3; r++)
            f2[r] = (d[r*4]*fwd[0] + d[r*4+1]*fwd[1] + d[r*4+2]*fwd[2]) / scale;
        for (int k = 0; k < 3; k++) ahead[k] = pos[k] + f2[k] * dist;
        const float up_z[3] = {0, 0, 1}, up_y[3] = {0, 1, 0};
        cam.look_at(pos, ahead, std::fabs(f2[2]) > 0.999f ? up_y : up_z);
        for (int k = 0; k < 3; k++) cam.target[k] = tgt[k];
    };
    carry(_cam);
    carry(_home);
    _anim = false;
    _dirty = true;
}

void ViewportPanel::frame_view(const float centre[3], float radius) {
    if (!(radius > 0.0f) || !std::isfinite(radius)) return;
    // The narrower half-angle of the view, kept sane for the wide lenses.
    constexpr float kDeg = 3.14159265f / 180.0f;
    const float aspect = _img_w > 1.0f && _img_h > 1.0f ? _img_h / _img_w : 0.5625f;
    const float hx = 0.5f * std::min(_fov_deg[_cam_model] > 0.0f ? _fov_deg[_cam_model] : 90.0f,
                                     170.0f) * kDeg;
    const float hy = std::atan(std::tan(hx) * aspect);
    const float a = std::clamp(std::min(hx, hy), 5.0f * kDeg, 60.0f * kDeg);
    for (int i = 0; i < 3; i++) {
        _frame_from[i] = _cam.target[i];
        _frame_to[i] = centre[i];
    }
    _frame_from[3] = std::max(nav_dist(), 1e-6f);
    _frame_to[3] = radius / std::sin(a) * 1.1f;
    _frame_anim = true;
    _frame_t0 = ImGui::GetTime();
    _anim = false;
    _dirty = true;
}

void ViewportPanel::animate_view(double now) {
    if (_glide) animate_glide(now);
    if (_frame_anim) {
        float t = (float)std::clamp((now - _frame_t0) / (kSnapSeconds * 1.5), 0.0, 1.0);
        t = t * t * (3.0f - 2.0f * t);
        float fwd[3];
        _cam.axis_forward(fwd);
        const float d = _frame_from[3] * std::pow(_frame_to[3] / _frame_from[3], t);
        for (int i = 0; i < 3; i++) {
            _cam.target[i] = _frame_from[i] + (_frame_to[i] - _frame_from[i]) * t;
            _cam.pos[i] = _cam.target[i] - fwd[i] * d;
        }
        _dirty = true;
        if (t >= 1.0f) _frame_anim = false;
    }
    if (!_anim) return;
    float t = (float)std::clamp((now - _anim_t0) / kSnapSeconds, 0.0, 1.0);
    t = t * t * (3.0f - 2.0f * t);
    const float dist = std::max(nav_dist(), 1e-6f);
    quat_slerp(_anim_from, _anim_to, t, _cam.rot);
    float back[3];
    _cam.axis_forward(back);
    for (int i = 0; i < 3; i++) _cam.pos[i] = _cam.target[i] - back[i] * dist;
    _dirty = true;
    if (t >= 1.0f) _anim = false;
}

void ViewportPanel::animate_glide(double now) {
    // Anything else that moved the camera meanwhile wins over the glide.
    if (!same_pose(_cam, _glide_last)) {
        _glide = false;
        return;
    }
    float t = (float)std::clamp((now - _glide_t0) / kGlideSeconds, 0.0, 1.0);
    if (t >= 1.0f) {
        std::memcpy(_cam.pos, _glide_to.pos, sizeof _cam.pos);
        std::memcpy(_cam.rot, _glide_to.rot, sizeof _cam.rot);
        std::memcpy(_cam.target, _glide_to.target, sizeof _cam.target);
        _glide = false;
        _dirty = true;
        return;
    }
    t = t * t * (3.0f - 2.0f * t);
    float of[3], ot[3];
    eye_offset(_glide_from, of);
    eye_offset(_glide_to, ot);
    // The offset's length goes geometrically, so a long way in is not a lunge.
    const float lf = std::sqrt(of[0]*of[0] + of[1]*of[1] + of[2]*of[2]);
    const float lt = std::sqrt(ot[0]*ot[0] + ot[1]*ot[1] + ot[2]*ot[2]);
    float off[3];
    for (int k = 0; k < 3; k++) off[k] = of[k] + (ot[k] - of[k]) * t;
    const float lo = std::sqrt(off[0]*off[0] + off[1]*off[1] + off[2]*off[2]);
    if (lf > 1e-9f && lt > 1e-9f && lo > 1e-9f) {
        const float want = lf * std::pow(lt / lf, t);
        for (float& v : off) v *= want / lo;
    }
    quat_slerp(_glide_from.rot, _glide_to.rot, t, _cam.rot);
    for (int k = 0; k < 3; k++)
        _cam.target[k] = _glide_from.target[k] + (_glide_to.target[k] - _glide_from.target[k]) * t;
    float m[12];
    _cam.c2w(m);
    for (int r = 0; r < 3; r++)
        _cam.pos[r] = _cam.target[r] + m[r*4+0]*off[0] + m[r*4+1]*off[1] + m[r*4+2]*off[2];
    _glide_last = _cam;
    _dirty = true;
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
    if (std::memcmp(_m2s_owner, a, sizeof _m2s_owner) == 0) return;
    for (int i = 0; i < 12; i++) _m2s_owner[i] = a[i];
    rebuild_m2s();
    _dirty = true;
}

// owner placement, then the levelling correction (R_align transposed when
// the parsers' up guess is switched off), then whatever placement is being
// edited -- innermost, so it happens in the model's own frame.
void ViewportPanel::rebuild_m2s() {
    const float* o = _m2s_owner;
    const bool corr = !_level_cameras && !_align_identity;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            float v = 0.0f;
            if (corr)
                for (int k = 0; k < 3; k++) v += o[r*4+k] * _align[c*3+k];
            else
                v = o[r*4+c];
            _m2s_base[r*4+c] = v;
        }
        _m2s_base[r*4+3] = o[r*4+3];
    }
    compose_3x4(_m2s_base, _m2s_edit, _m2s);
    _m2s_scale = std::sqrt(_m2s[0]*_m2s[0] + _m2s[4]*_m2s[4] + _m2s[8]*_m2s[8]);
    if (!(_m2s_scale > 1e-20f)) _m2s_scale = 1.0f;
    static const float kI[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    _m2s_identity = std::memcmp(_m2s, kI, sizeof kI) == 0;
}

void ViewportPanel::set_edit_transform(const float a[12]) {
    if (std::memcmp(_m2s_edit, a, sizeof _m2s_edit) == 0) return;
    std::memcpy(_m2s_edit, a, sizeof _m2s_edit);
    rebuild_m2s();
    // A model being dragged is a camera being moved as far as the render's
    // cost goes, so it gets the same half-resolution frames.
    _last_move = ImGui::GetTime();
    _dirty = true;
}

void ViewportPanel::base_transform(float out[12]) const {
    std::memcpy(out, _m2s_base, sizeof _m2s_base);
}

void ViewportPanel::set_nav_up(const float up[3]) {
    const float n = std::sqrt(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
    if (!(n > 1e-12f)) return;
    const float u[3] = {up[0] / n, up[1] / n, up[2] / n};
    const float* was = _cam.world_up;
    if (u[0]*was[0] + u[1]*was[1] + u[2]*was[2] > 0.99999f) return;
    for (NavCamera* c : {&_cam, &_home}) {
        for (int k = 0; k < 3; k++) c->world_up[k] = u[k];
        // What these modes keep level; the others roll where the user put them.
        if (c->mode == NavCamera::Turntable || c->mode == NavCamera::Fps) c->level_roll();
    }
    _dirty = true;
}

void ViewportPanel::move_view(const float S[12]) {
    _cam.transform(S);
    _home.transform(S);
    _dirty = true;
}

void ViewportPanel::set_level_cameras(bool on) {
    if (_level_cameras == on) return;
    _level_cameras = on;
    rebuild_m2s();
    _dirty = true;
}

void ViewportPanel::adopt_gauge(const ParsedDataset& ds, bool first) {
    for (int k = 0; k < 9; k++) _align[k] = ds.normalized_rotation[k];
    _align_identity = true;
    for (int r = 0; r < 3 && _align_identity; r++)
        for (int c = 0; c < 3; c++)
            if (std::fabs(_align[r*3+c] - (r == c ? 1.0f : 0.0f)) > 1e-6f) {
                _align_identity = false;
                break;
            }
    _gauge_metric = ds.gauge_metric;
    _scene_scale = ds.train_frame_scale > 0 ? ds.train_frame_scale : 1.0f;
    // A model whose orientation was measured, or placed by hand in the
    // editor, does not want the guess on top of it.
    if (first) _level_cameras = !ds.gauge_oriented && !ds.edited_in_place;
    rebuild_m2s();
    _dirty = true;
}

// Both backends pick the cell from the same rule: a power of ten a little
// under the view distance, measured in model units (PreviewRenderer.cpp
// ensure_grid, Visualizer.cu _viewer_build_grid).
float ViewportPanel::grid_cell() const {
    const float d = nav_dist() / _m2s_scale * _scene_scale;
    return std::pow(10.0f, std::floor(std::log10(std::max(d, 1e-6f) * 0.5f)));
}

// The same rule over the BASE frame: a grid the model is placed against must
// not rescale because the model did.
float ViewportPanel::world_grid_cell() const {
    const float bs = std::sqrt(_m2s_base[0]*_m2s_base[0] + _m2s_base[4]*_m2s_base[4] +
                               _m2s_base[8]*_m2s_base[8]);
    const float d = nav_dist() / std::max(bs, 1e-20f) * _scene_scale;
    return std::pow(10.0f, std::floor(std::log10(std::max(d, 1e-6f) * 0.5f)));
}

bool ViewportPanel::external_grid() const {
    return _interactor && _interactor->draws_world_grid();
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
    render_c2w(out);
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
    q.show_grid = _show_grid && !external_grid();
    q.show_roi = _show_roi && _roi_engine != nullptr;
    q.grid_dist = nav_dist() / _m2s_scale;
    model_point(_cam.target, q.grid_target);
    q.cam_size_scale = _frustum_scale;
}

// One line under the image's top-left corner. `line` is which row it is, so
// the point count and the grid legend stack without measuring the font twice.
void ViewportPanel::draw_grid_overlay(float x, float y, int line) const {
    if (!_show_grid) return;
    const float c = external_grid() ? world_grid_cell() : grid_cell();
    char buf[32];
    if (_gauge_metric) {
        // Symbols, not words: km/m/cm/mm read the same in every language.
        const char* unit = c >= 1000.0f ? "km" : c >= 1.0f ? "m"
                           : c >= 0.01f ? "cm" : "mm";
        const float mul = c >= 1000.0f ? 1e-3f : c >= 1.0f ? 1.0f
                          : c >= 0.01f ? 100.0f : 1000.0f;
        snprintf(buf, sizeof buf, "%g %s", (double)(c * mul), unit);
    } else {
        snprintf(buf, sizeof buf, "%g", (double)c);
    }
    const std::string t = spirula::i18n::format(
        _gauge_metric ? spirula::i18n::msg::gui::overlay_grid_metric
                      : spirula::i18n::msg::gui::overlay_grid_relative,
        {std::string(buf)});
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(x, y + line * ImGui::GetTextLineHeight()),
        IM_COL32(200, 200, 200, 180), t.c_str());
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

// The same pose in the CV convention a selection projects through: the c2w
// columns are the GL view axes, and CV is (x, -y, -z) of them.
static void cv_w2c(const float m[12], float w2c[12], float eye[3]) {
    const float sign[3] = {1.0f, -1.0f, -1.0f};
    for (int r = 0; r < 3; r++) {
        float t = 0.0f;
        for (int c = 0; c < 3; c++) {
            const float v = sign[r] * m[c * 4 + r];
            w2c[r * 4 + c] = v;
            t += v * m[c * 4 + 3];
        }
        w2c[r * 4 + 3] = -t;
    }
    eye[0] = m[3];
    eye[1] = m[7];
    eye[2] = m[11];
}

void ViewportPanel::view_camera(int W, int H, float w2c[12], float& fx,
                                float& fy, int& camera_model,
                                float eye[3]) const {
    float m[12];
    model_c2w(m);
    cv_w2c(m, w2c, eye);
    compute_intrinsics(W, H, fx, fy);
    camera_model = _cam_model;
}

void ViewportPanel::nav_camera(int W, int H, float w2c[12], float& fx,
                               float& fy, int& camera_model,
                               float eye[3]) const {
    float m[12];
    render_c2w(m);
    cv_w2c(m, w2c, eye);
    compute_intrinsics(W, H, fx, fy);
    camera_model = _cam_model;
}

void ViewportPanel::nav_pose(float c2w[12], float target[3]) const {
    _cam.c2w(c2w);
    for (int i = 0; i < 3; i++) target[i] = _cam.target[i];
}

void ViewportPanel::set_nav_pose(const float c2w[12], const float target[3]) {
    const float eye[3] = {c2w[3], c2w[7], c2w[11]};
    // Column 1 is up and column 2 points back, OpenGL axes.
    const float up[3] = {c2w[1], c2w[5], c2w[9]};
    const float ahead[3] = {eye[0] - c2w[2], eye[1] - c2w[6], eye[2] - c2w[10]};
    _cam.look_at(eye, ahead, up);
    for (int i = 0; i < 3; i++) _cam.target[i] = target[i];
    _anim = false;
    if (_ortho) _ortho = _ortho_auto = false;
    _dirty = true;
}

void ViewportPanel::set_view_lens(int model, float fov_deg) {
    model = std::clamp(model, 0, 3);
    if (model != 0) _ortho = _ortho_auto = false;
    _cam_model = model;
    if (fov_max() > 0) _fov_deg[model] = std::clamp(fov_deg, fov_min(), fov_max());
    _dirty = true;
}

void ViewportPanel::request_pick(float u, float v) {
    _tool_pick = true;
    _tool_pick_done = false;
    _tool_pick_uv[0] = u;
    _tool_pick_uv[1] = v;
    _dirty = true;
}

bool ViewportPanel::take_pick(float out[3], bool& hit) {
    if (!_tool_pick_done) return false;
    _tool_pick_done = false;
    hit = _tool_pick_hit;
    for (int i = 0; i < 3; i++) out[i] = _tool_pick_at[i];
    return true;
}

std::string ViewportPanel::primitive() const {
    return _scene_options ? kViewerPrimitives[_primitive_idx] : "";
}

void ViewportPanel::edit_transform(float out[12]) const {
    std::memcpy(out, _m2s_edit, sizeof _m2s_edit);
}

void ViewportPanel::model_to_shared(float out[12]) const {
    std::memcpy(out, _m2s, sizeof _m2s);
}

void ViewportPanel::image_rect(float& x, float& y, float& w, float& h) const {
    x = _img_x;
    y = _img_y;
    w = _img_w;
    h = _img_h;
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
    adopt_gauge(session.ds, maybe_frame(session));
    _last_error.clear();
    _mode = Mode::Preview;
}

void ViewportPanel::attach_preview_data(const ParsedDataset& ds,
                                       const PostSplitCameras& post,
                                       const std::string& key, float radius,
                                       bool with_cameras,
                                       const uint8_t* cam_selected,
                                       const float* cam_rgb) {
    const bool first = key != _framed_key;
    detach();
    _has_cameras = with_cameras;
    // Frusta on by default when there are any: watching a reconstruction is
    // watching the cameras find their places. Only on the first attach, so a
    // refresh does not undo the switch.
    if (first) _show_cams = with_cameras;
    if (!_preview.build(ds, post, cam_selected, cam_rgb)) {
        _last_error = "preview renderer unavailable (OpenGL 3.2 required)";
        return;
    }
    {
        const dsparse::CenterTable centers = dsparse::scene_centers(ds);
        set_centers(&centers, ds.num_cameras > 0);
    }
    if (first) {
        _framed_key = key;
        reset_pose(radius);
    }
    adopt_gauge(ds, first);
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
    {
        double A[12];
        for (int i = 0; i < 12; i++) A[i] = to_normalized ? to_normalized[i] : (i % 5 == 0);
        const dsparse::CenterTable centers = dsparse::scene_centers(
            nullptr, 0, mesh.V.empty() ? nullptr : mesh.V[0].data(),
            (int64_t)mesh.V.size(), 3, A);
        set_centers(&centers, false);
    }
    if (key != _framed_key) {
        _framed_key = key;
        reset_pose(radius);
    }
    _last_error.clear();
    _mode = Mode::Preview;
}

void ViewportPanel::set_region_overlay(std::shared_ptr<const spirula::RegionOverlay> engine,
                                       std::shared_ptr<const spirula::RegionOverlay> preview,
                                       std::shared_ptr<const std::vector<uint8_t>> points_inside) {
    _roi_engine = std::move(engine);
    _roi_preview = std::move(preview);
    _roi_points_inside = std::move(points_inside);
    if (_mode == Mode::Engine) _worker.set_region_overlay(_roi_engine);
    _dirty = true;
}

void ViewportPanel::attach(spirula::TrainerSession& session) {
    detach();
    _worker.start(session.make_viewer_config(), session.make_viewer_hooks());
    _worker.set_region_overlay(_roi_engine);
    _buffer_keys = _worker.buffer_keys();
    _buffer_idx = std::min<int>(_buffer_idx, (int)_buffer_keys.size() - 1);
    _has_cameras = session.ds.num_cameras > 0;
    adopt_gauge(session.ds, maybe_frame(session));
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
    set_centers(&cfg.centers, cfg.center_cameras);
    if (key != _framed_key) {
        _framed_key = key;
        reset_pose(radius);
    }
    // A file, not a dataset: nothing rotated it and nothing says what a unit
    // is, but its own scale still sets what the grid's cell measures.
    _align_identity = true;
    _gauge_metric = false;
    _scene_scale = cfg.train_frame_scale > 0 ? cfg.train_frame_scale : 1.0f;
    rebuild_m2s();
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
    const ImVec2 rmin = ImGui::GetItemRectMin();
    const ImVec2 rsz = ImGui::GetItemRectSize();
    _img_x = rmin.x;
    _img_y = rmin.y;
    _img_w = rsz.x;
    _img_h = rsz.y;

    // The gizmo sits on top of the image, so it answers first: a click on it
    // is neither a tool's nor the start of a drag on what is under it.
    if (gizmo_input(hovered)) hovered = false;
    animate_view(ImGui::GetTime());

    // A tool owns the left button for its whole lifetime, so that "does this
    // drag orbit or lasso?" is answered once rather than per feature.
    bool tool_owns_left = false;
    if (_interactor) {
        ViewportInput in;
        in.hovered = hovered;
        in.x = io.MousePos.x - rmin.x;
        in.y = io.MousePos.y - rmin.y;
        in.W = (int)rsz.x;
        in.H = (int)rsz.y;
        in.down = !_giz_down && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        in.clicked = !_giz_down && hovered &&
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        in.released = !_giz_down && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        in.right_clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        in.double_clicked = hovered &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        in.shift = io.KeyShift;
        in.ctrl = io.KeyCtrl;
        in.alt = io.KeyAlt;
        tool_owns_left = _interactor->on_viewport_input(in);
    }

    // Pointer (touch and trackpad gestures arrive as mouse + wheel events):
    //   LMB / MMB: orbit (turntable/trackball) or look (fps/fly)
    //   RMB, Shift+MMB, Shift+LMB: pan
    if (hovered && !_dragging) {
        for (int b : {ImGuiMouseButton_Left, ImGuiMouseButton_Right,
                      ImGuiMouseButton_Middle}) {
            if (b == ImGuiMouseButton_Left && tool_owns_left) continue;
            if (b == ImGuiMouseButton_Right && _interactor &&
                _interactor->owns_right_button())
                continue;
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
                // The middle button orbits in every viewport: a hand that
                // learned it in the editor drags with it everywhere. Shift is
                // a tool's modifier while one owns the left button.
                const bool modal = _interactor &&
                                   _interactor->owns_left_button();
                bool is_pan = _drag_button == ImGuiMouseButton_Right ||
                              (io.KeyShift &&
                               (_drag_button == ImGuiMouseButton_Middle || !modal));
                if (spirula::env("NAV_DEBUG"))
                    std::fprintf(stderr,
                        "[nav] btn=%d pan=%d shift=%d d=(%.0f,%.0f) tgt=(%.3f,%.3f,%.3f) pos=(%.3f,%.3f,%.3f)\n",
                        _drag_button, (int)is_pan, (int)io.KeyShift, dx, dy,
                        _cam.target[0], _cam.target[1], _cam.target[2],
                        _cam.pos[0], _cam.pos[1], _cam.pos[2]);
                _frame_anim = false;
                if (is_pan) {
                    _cam.pan(dx, dy);
                } else {
                    if (_cam.mode == NavCamera::Turntable ||
                        _cam.mode == NavCamera::Trackball)
                        _cam.orbit(dx, dy);
                    else
                        _cam.look(dx, dy);
                    // An axis view is orthographic because it is an axis
                    // view; turned away from the axis it is a view again.
                    if (_ortho_auto) _ortho = _ortho_auto = false;
                    _anim = false;
                }
                _dirty = true;
            }
        }
    }

    // Double-click = center the view on the 3D content under the cursor
    // (viewer.html pickRecenter). Recorded here as fractional image
    // coordinates; resolved by the mode-specific draw (preview: CPU point
    // pick, engine: depth readback on the next render).
    if (hovered && !tool_owns_left &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
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
        _frame_anim = false;
        if (ortho_back() > 0.0f) {
            // Moving forward changes nothing about an orthographic image, so
            // every mode zooms the way the orbiting ones do.
            const float k = std::exp(-io.MouseWheel * 100.0f * 0.004f *
                                     _cam.speed() * 0.2f);
            for (int i = 0; i < 3; i++)
                _cam.pos[i] = _cam.target[i] + (_cam.pos[i] - _cam.target[i]) * k;
        } else {
            _cam.dolly(-io.MouseWheel * 100.0f);
        }
        _dirty = true;
    }

    // Keyboard: while the pointer is over the viewport or dragging, no text
    // field wants input, and no modifier is down -- Ctrl+D is "deselect", and
    // a viewport that also reads the D moves unasked.
    const bool plain = !io.KeyCtrl && !io.KeyAlt && !io.KeyShift && !io.KeySuper;
    if ((hovered || _dragging) && !io.WantTextInput && plain) {
        // A tool owns the letter keys -- they are its grammar -- so with one
        // active the camera keeps only what nothing competes for.
        const bool letters = !(_interactor && _interactor->blocks_fly_keys());
        NavCamera::Keys k;
        auto fly = [&](char c) {
            return letters && ImGui::IsKeyDown((ImGuiKey)fly_key(c));
        };
        k.w = fly('w');
        k.a = fly('a');
        k.s = fly('s');
        k.d = fly('d');
        k.e = fly('e');
        k.q = fly('q');
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
        if (_cam.keyboard_tick(dt, k)) _dirty = true, _frame_anim = false;
    }
    // The numeric-pad views every 3D package shares: 1 front, 3 right, 7 top,
    // Ctrl for the far side, 5 for perspective / orthographic.
    if (hovered && !io.WantTextInput && !io.KeyAlt && !io.KeyShift) {
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false)) snap_view(1, !io.KeyCtrl);
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false)) snap_view(0, io.KeyCtrl);
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false)) snap_view(2, io.KeyCtrl);
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad5, false)) set_ortho(!_ortho);
        // And . for the selection, the way every 3D package has it.
        double c[3], r = 0.0;
        if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal, false) && _interactor &&
            _interactor->frame_bounds(c, r)) {
            const float cf[3] = {(float)c[0], (float)c[1], (float)c[2]};
            frame_view(cf, (float)r);
        }
    }
    if (hovered && !io.WantTextInput) handle_view_keys();

    // Gamepad: always active, like the browser's gamepadTick loop.
    {
        float dt = std::min(io.DeltaTime, 0.1f);
        if (_cam.gamepad_tick(dt)) _dirty = true, _frame_anim = false;
    }
}

// ---------------------------------------------------------------------------
// The navigation gizmo
// ---------------------------------------------------------------------------

// For a pointer with no middle button, and it works with a tool active --
// which is exactly when the left button is otherwise spoken for.

namespace {

struct GizmoLayout {
    ImVec2 c;            // ball centre
    float R = 0;         // ball radius
    float rb = 0;        // side-button radius
    ImVec2 btn[3];       // zoom, pan, projection
    bool shown = false;
};

GizmoLayout gizmo_layout(float x, float y, float w, float h) {
    GizmoLayout g;
    g.R = px(38.0f);
    g.rb = px(13.0f);
    const float m = px(10.0f);
    g.shown = w > 5.0f * g.R && h > 6.0f * g.R;
    g.c = ImVec2(x + w - g.R - m, y + g.R + m);
    for (int i = 0; i < 3; i++)
        g.btn[i] = ImVec2(x + w - m - g.rb,
                          g.c.y + g.R + m + g.rb + (float)i * (2.0f * g.rb + px(6.0f)));
    return g;
}

// Where axis `a` (0..5: +X +Y +Z -X -Y -Z) lands: x right, y down, z toward
// the viewer, in units of the ball radius.
void gizmo_axis(const NavCamera& cam, int a, float out[3]) {
    float r[3], u[3], f[3];
    cam.axis_right(r);
    cam.axis_up(u);
    cam.axis_forward(f);
    const int k = a % 3;
    const float sgn = a < 3 ? 1.0f : -1.0f;
    out[0] = sgn * r[k];
    out[1] = -sgn * u[k];
    out[2] = -sgn * f[k];
}

constexpr ImU32 kAxisCol[3] = {IM_COL32(250, 51, 79, 255),
                               IM_COL32(140, 219, 0, 255),
                               IM_COL32(41, 140, 250, 255)};

}  // namespace

bool ViewportPanel::gizmo_input(bool hovered_image) {
    const GizmoLayout g = gizmo_layout(_img_x, _img_y, _img_w, _img_h);
    if (!g.shown) {
        _giz_down = _giz_hover = false;
        _giz_hot = -1;
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mp = io.MousePos;
    auto within = [&](const ImVec2& c, float r) {
        const float dx = mp.x - c.x, dy = mp.y - c.y;
        return dx * dx + dy * dy <= r * r;
    };

    if (!_giz_down) {
        _giz_hot = -1;
        _giz_hover = false;
        if (hovered_image || ImGui::IsWindowHovered()) {
            for (int i = 0; i < 3; i++)
                if (within(g.btn[i], g.rb)) _giz_hot = 6 + i;
            if (_giz_hot < 0 && within(g.c, g.R + px(6.0f))) {
                _giz_hover = true;
                // The bubble nearest the viewer wins where two overlap.
                float best_z = -2.0f;
                for (int a = 0; a < 6; a++) {
                    float v[3];
                    gizmo_axis(_cam, a, v);
                    const ImVec2 at(g.c.x + v[0] * g.R * 0.78f,
                                    g.c.y + v[1] * g.R * 0.78f);
                    if (within(at, px(a < 3 ? 10.0f : 8.0f)) && v[2] > best_z) {
                        best_z = v[2];
                        _giz_hot = a;
                    }
                }
            }
        }
        if ((_giz_hover || _giz_hot >= 0) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            _giz_down = true;
            _giz_dragged = false;
            _giz_button = _giz_hot >= 6 ? _giz_hot - 5 : 0;
            _giz_press[0] = mp.x;
            _giz_press[1] = mp.y;
        }
        return _giz_hover || _giz_hot >= 0;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (!_giz_dragged) {
            if (_giz_button == 3) {
                set_ortho(!_ortho);
            } else if (_giz_button == 0 && _giz_hot >= 0 && _giz_hot < 6) {
                // Already looking along it: a second click is the far side.
                float v[3];
                gizmo_axis(_cam, _giz_hot, v);
                const bool facing = v[2] > 0.999f;
                snap_view(_giz_hot % 3, (_giz_hot >= 3) != facing);
            }
        }
        _giz_down = false;
        _giz_button = 0;
        return true;
    }
    const float ddx = mp.x - _giz_press[0], ddy = mp.y - _giz_press[1];
    if (ddx * ddx + ddy * ddy > 16.0f) _giz_dragged = true;
    const float dx = io.MouseDelta.x, dy = io.MouseDelta.y;
    if (_giz_dragged && (dx != 0.0f || dy != 0.0f)) {
        if (_giz_button == 1) {
            // Down is closer, the way a scroll toward you is.
            const float k = std::exp(-dy * 0.01f);
            for (int i = 0; i < 3; i++)
                _cam.pos[i] = _cam.target[i] + (_cam.pos[i] - _cam.target[i]) * k;
        } else if (_giz_button == 2) {
            _cam.pan(dx * 2.0f, dy * 2.0f);
        } else if (_giz_button == 0) {
            if (_cam.mode == NavCamera::Turntable || _cam.mode == NavCamera::Trackball)
                _cam.orbit(dx * 1.5f, dy * 1.5f);
            else
                _cam.look(dx * 1.5f, dy * 1.5f);
            if (_ortho_auto) _ortho = _ortho_auto = false;
            _anim = false;
        }
        _dirty = true;
    }
    return true;
}

void ViewportPanel::draw_gizmo() const {
    const GizmoLayout g = gizmo_layout(_img_x, _img_y, _img_w, _img_h);
    if (!g.shown) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (_giz_hover || (_giz_down && _giz_button == 0))
        dl->AddCircleFilled(g.c, g.R + px(4.0f), IM_COL32(255, 255, 255, 38), 48);

    int order[6] = {0, 1, 2, 3, 4, 5};
    float z[6];
    ImVec2 at[6];
    for (int a = 0; a < 6; a++) {
        float v[3];
        gizmo_axis(_cam, a, v);
        z[a] = v[2];
        at[a] = ImVec2(g.c.x + v[0] * g.R * 0.78f, g.c.y + v[1] * g.R * 0.78f);
    }
    std::sort(order, order + 6, [&](int a, int b) { return z[a] < z[b]; });
    const char* names[3] = {"X", "Y", "Z"};
    for (int a : order) {
        const int k = a % 3;
        const bool hot = _giz_hot == a;
        // Dimmed toward the back, so the ball reads as a ball.
        const float shade = 0.55f + 0.45f * (z[a] * 0.5f + 0.5f);
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(kAxisCol[k]);
        c.x *= shade; c.y *= shade; c.z *= shade;
        const ImU32 col = ImGui::ColorConvertFloat4ToU32(c);
        if (a < 3) {
            dl->AddLine(g.c, at[a], col, px(2.0f));
            dl->AddCircleFilled(at[a], px(hot ? 10.0f : 9.0f), col, 24);
            const ImVec2 ts = ImGui::CalcTextSize(names[k]);
            dl->AddText(ImVec2(at[a].x - ts.x * 0.5f, at[a].y - ts.y * 0.5f),
                        hot ? IM_COL32(255, 255, 255, 255) : IM_COL32(0, 0, 0, 230),
                        names[k]);
        } else {
            ImVec4 fill = c;
            fill.w = hot ? 0.85f : 0.35f;
            dl->AddCircleFilled(at[a], px(7.0f), ImGui::ColorConvertFloat4ToU32(fill), 20);
            dl->AddCircle(at[a], px(7.0f), col, 20, px(1.5f));
        }
    }

    // zoom, pan, projection -- drawn, since no icon face is embedded.
    for (int i = 0; i < 3; i++) {
        const bool hot = _giz_hot == 6 + i || (_giz_down && _giz_button == i + 1);
        const ImVec2 c = g.btn[i];
        dl->AddCircleFilled(c, g.rb, hot ? IM_COL32(255, 255, 255, 70)
                                         : IM_COL32(20, 22, 26, 170), 24);
        const ImU32 ink = IM_COL32(235, 235, 235, 235);
        const float u = g.rb * 0.5f, t = px(1.6f);
        if (i == 0) {
            dl->AddCircle(ImVec2(c.x - u * 0.2f, c.y - u * 0.2f), u * 0.75f, ink, 16, t);
            dl->AddLine(ImVec2(c.x + u * 0.35f, c.y + u * 0.35f),
                        ImVec2(c.x + u, c.y + u), ink, t * 1.3f);
        } else if (i == 1) {
            dl->AddLine(ImVec2(c.x - u, c.y), ImVec2(c.x + u, c.y), ink, t);
            dl->AddLine(ImVec2(c.x, c.y - u), ImVec2(c.x, c.y + u), ink, t);
            const float a = u * 0.35f;
            for (int d = 0; d < 4; d++) {
                const float ex = d == 0 ? -u : d == 1 ? u : 0.0f;
                const float ey = d == 2 ? -u : d == 3 ? u : 0.0f;
                const ImVec2 tip(c.x + ex, c.y + ey);
                const float bx = ex == 0 ? a : (ex < 0 ? a : -a);
                const float by = ey == 0 ? a : (ey < 0 ? a : -a);
                if (ex != 0) {
                    dl->AddLine(tip, ImVec2(tip.x + bx, tip.y - a), ink, t);
                    dl->AddLine(tip, ImVec2(tip.x + bx, tip.y + a), ink, t);
                } else {
                    dl->AddLine(tip, ImVec2(tip.x - a, tip.y + by), ink, t);
                    dl->AddLine(tip, ImVec2(tip.x + a, tip.y + by), ink, t);
                }
            }
        } else if (ortho_back() > 0.0f) {
            // Parallel edges: a square grid.
            dl->AddRect(ImVec2(c.x - u, c.y - u), ImVec2(c.x + u, c.y + u), ink, 0.0f, 0, t);
            dl->AddLine(ImVec2(c.x, c.y - u), ImVec2(c.x, c.y + u), ink, t);
            dl->AddLine(ImVec2(c.x - u, c.y), ImVec2(c.x + u, c.y), ink, t);
        } else {
            // Converging edges: the same grid seen in perspective.
            const ImVec2 q[4] = {ImVec2(c.x - u * 0.55f, c.y - u * 0.8f),
                                 ImVec2(c.x + u * 0.55f, c.y - u * 0.8f),
                                 ImVec2(c.x + u, c.y + u * 0.8f),
                                 ImVec2(c.x - u, c.y + u * 0.8f)};
            dl->AddPolyline(q, 4, ink, ImDrawFlags_Closed, t);
            dl->AddLine(ImVec2(c.x, q[0].y), ImVec2(c.x, q[2].y), ink, t);
            dl->AddLine(ImVec2(c.x - u * 0.78f, c.y), ImVec2(c.x + u * 0.78f, c.y), ink, t);
        }
    }

    // Not an ImGui item, so the usual hover delay is kept by hand: a tooltip
    // that opens the instant the pointer crosses the ball covers it.
    const int on = _giz_down ? -2 : _giz_hot >= 6 ? _giz_hot : _giz_hover ? -1 : -2;
    if (on != _giz_tip_on) {
        _giz_tip_on = on;
        _giz_tip_since = ImGui::GetTime();
    }
    if (on != -2 && ImGui::GetTime() - _giz_tip_since > 0.6) {
        if (_giz_hot == 6) ui::SetTooltip(msg::gizmo_zoom_help);
        else if (_giz_hot == 7) ui::SetTooltip(msg::gizmo_pan_help);
        else if (_giz_hot == 8)
            ui::SetTooltip(ortho_back() > 0.0f ? msg::gizmo_to_perspective
                                               : msg::gizmo_to_ortho);
        else if (_giz_hover) ui::SetTooltip(msg::gizmo_help);
    }
}

// What is drawn over the image in either mode: the tool's overlay, then the
// gizmo on top of it.
void ViewportPanel::draw_overlays() {
    if (_interactor) {
        ViewportOverlay ov;
        ov.dl = ImGui::GetWindowDrawList();
        ov.x = _img_x;
        ov.y = _img_y;
        ov.w = _img_w;
        ov.h = _img_h;
        ov.grid = _show_grid && external_grid();
        ov.grid_cell = world_grid_cell();
        ov.dl->PushClipRect(ImVec2(_img_x, _img_y),
                            ImVec2(_img_x + _img_w, _img_y + _img_h), true);
        _interactor->draw_viewport_overlay(ov);
        ov.dl->PopClipRect();
    }
    draw_gizmo();
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
    if (_mode == Mode::Engine ? _roi_engine != nullptr : (_roi_preview || _roi_points_inside)) {
        place(check_w(msg::viewport_region));
        if (ui::Checkbox(msg::viewport_region, &_show_roi)) _dirty = true;
        ui::help_on_hover(msg::viewport_region_help);
    }
    // Only where there is a guess to switch off. Turntable and first-person
    // orbit about the navigated frame's +Z, so this is what they turn about.
    if (!_align_identity) {
        place(check_w(msg::viewport_level_cameras));
        if (ui::Checkbox(msg::viewport_level_cameras, &_level_cameras)) {
            rebuild_m2s();
            _dirty = true;
        }
        ui::help_on_hover(msg::viewport_level_cameras_help);
    }
    if (_centers_known) {
        namespace fld = spirula::i18n::msg::field;
        auto center_label = [](int mode) -> const char* {
            const spirula::i18n::Msg* m =
                fld::choice_label("scene_center", dsparse::kCenterModeNames[mode]);
            return m ? m->get() : dsparse::kCenterModeNames[mode];
        };
        auto is_camera_mode = [](int mode) {
            using M = dsparse::CenterMode;
            return mode == (int)M::CameraMedian || mode == (int)M::CameraMean ||
                   mode == (int)M::CameraFocus;
        };
        place(px(170.0f) + st.ItemInnerSpacing.x + text_w(msg::viewport_center.get()));
        ImGui::SetNextItemWidth(px(170.0f));
        if (ui::BeginComboRaw(ui::detail::label(msg::viewport_center),
                              center_label(effective_center_mode()))) {
            for (int i = 0; i < dsparse::kNumCenterModes; i++) {
                if (!_center_has_cameras && is_camera_mode(i)) continue;
                if (ui::SelectableRaw(center_label(i), i == _center_mode) &&
                    i != _center_mode) {
                    _center_mode = i;
                    // What is live now, not what the file held.
                    if (_center_provider) {
                        dsparse::CenterTable live;
                        if (_center_provider(live)) _centers = live;
                    }
                    // The model stays where it is; the pivot moves to the new
                    // centre, and so does the pose Reset view returns to.
                    float c[3];
                    center_shared(c);
                    recenter_at(c);
                    const NavCamera live = _cam;
                    reset_pose(_home_dist);
                    _cam = live;
                }
            }
            ImGui::EndCombo();
        }
        ui::help_on_hover(msg::viewport_center_help);
    }
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
    if (_views) {
        place(views_width());
        draw_view_buttons();
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
    if (_image_w > 0.0f) avail.x = std::min(avail.x, _image_w);
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
    _preview.set_overlay(_roi_preview, _show_roi);
    _preview.dim_points_outside(_roi_points_inside, _show_roi);
    unsigned tex = _preview.render(W, H, view,
                                   (PreviewProjection)_cam_model,
                                   fx / (0.5f * W), fy / (0.5f * H),
                                   _home_dist, nav_dist() / _m2s_scale, target,
                                   _show_cams, _frustum_scale,
                                   _show_grid && !external_grid(),
                                   ortho_pullback(false));
    if (!tex) {
        ui::TextDisabled(msg::viewport_render_failed);
        return;
    }
    // FBO textures are bottom-up; flip V.
    ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2((float)W, (float)H),
                 ImVec2(0, 1), ImVec2(1, 0));
    handle_input((float)H);

    // A tool's pick is the same search, handed over instead of recentred on.
    if (_tool_pick) {
        _tool_pick = false;
        _tool_pick_done = true;
        _tool_pick_hit = false;
        const float u = (_tool_pick_uv[0] - 0.5f) * (float)W / fx;
        const float v = (_tool_pick_uv[1] - 0.5f) * (float)H / fy;
        float dcv[3], m[12];
        model_c2w(m);
        if (viewer_pixel_ray(_cam_model, u, v, dcv)) {
            float ro[3] = {m[3], m[7], m[11]}, rd[3], p[3];
            for (int r = 0; r < 3; r++)
                rd[r] = m[r*4+0]*dcv[0] - m[r*4+1]*dcv[1] - m[r*4+2]*dcv[2];
            if (_preview.pick_point(ro, rd, p)) {
                shared_point(p, _tool_pick_at);
                _tool_pick_hit = true;
            }
        }
    }

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

    draw_overlays();

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
    draw_grid_overlay(p.x + 8, p.y + 6, 1);
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
    float pose[13];
    for (int i = 0; i < 3; i++) pose[i] = _cam.pos[i];
    for (int i = 0; i < 4; i++) pose[3 + i] = _cam.rot[i];
    for (int i = 0; i < 3; i++) pose[7 + i] = _cam.target[i];
    pose[10] = _ortho ? 1.0f : 0.0f;
    pose[11] = (float)_cam_model;
    pose[12] = _fov_deg[_cam_model];
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
        _cam_model == src._cam_model && _ortho == src._ortho &&
        _fov_deg[_cam_model] == src._fov_deg[src._cam_model])
        return;
    _cam = src._cam;
    _cam_model = src._cam_model;
    _ortho = src._ortho;
    _ortho_auto = src._ortho_auto;
    for (int i = 0; i < 4; i++) _fov_deg[i] = src._fov_deg[i];
    _home = src._home;
    _home_dist = src._home_dist;
    // A glide travels too: the master is picked a frame late, so a pane left
    // behind would otherwise pull the view back.
    _glide = src._glide;
    _glide_t0 = src._glide_t0;
    _glide_from = src._glide_from;
    _glide_to = src._glide_to;
    _glide_last = src._glide_last;
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
                // Double-click centering result (background clicks miss), or
                // a tool's pick, which is delivered whatever it found.
                if (_tool_pick_inflight) {
                    _tool_pick_inflight = false;
                    _tool_pick_done = true;
                    _tool_pick_hit = res.pick_hit;
                    if (res.pick_hit) shared_point(res.pick_point, _tool_pick_at);
                } else if (res.pick_hit) {
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
        if (_tool_pick) {
            q.pick_px = std::clamp((int)(_tool_pick_uv[0] * (float)W), 0, W - 1);
            q.pick_py = std::clamp((int)(_tool_pick_uv[1] * (float)H), 0, H - 1);
            _tool_pick = false;
            _tool_pick_inflight = true;
        } else if (_dbl_pending) {
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
        const ImVec2 tl = ImGui::GetItemRectMin();
        draw_grid_overlay(tl.x + 8, tl.y + 6, 0);
        handle_input(size.y);
        draw_overlays();
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
