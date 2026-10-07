// EditTransform.cpp -- the editing session's placement half: the modal
// operator's glue, the alignment helpers and the Transform tab. The session
// is in EditSession.h; the frames are written out in
// docs/notes/scene-transform.md.

#include "app/gui/edit/EditSession.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "app/gui/ViewportPanel.h"
#include "core/SceneAlign.h"
#include "app/gui/edit/WorldGrid.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/EditTransform.h"

#include "imgui.h"

#include <cstddef>
#include <algorithm>
#include <cmath>

namespace msg = spirula::i18n::msg::xform;
namespace emsg = spirula::i18n::msg::edit;
using spirula::Sim3;
using spirula::i18n::Msg;
namespace align = spirula::align;

namespace gui {

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace


// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

Sim3 EditSession::base_frame() const {
    float b[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    if (_panel) _panel->base_transform(b);
    return Sim3::from_3x4(b);
}

Sim3 EditSession::saved_to_shared() const {
    return base_frame() * _doc->view_frame();
}

// The current layer's selection, or all of it, by its median and a high
// percentile of the distance from it: a stray element should not push the
// view back, and a selection is what was meant, so it keeps nearly all.
bool EditSession::frame_bounds(double centre[3], double& radius) {
    if (!_doc || !_panel || _xform.active()) return false;
    const Selection& sel = _doc->sel();
    const std::vector<uint8_t>& alive = _doc->alive_of(_doc->layer());
    const float* p = _doc->positions();
    const bool only = !sel.empty();
    const int64_t n = (int64_t)alive.size();
    const int64_t want = only ? sel.count() : _doc->alive_count();
    const int64_t step = std::max<int64_t>(1, want / 50000);
    std::vector<double> pts;
    int64_t seen = 0;
    for (int64_t i = 0; i < n; i++) {
        if (!alive[(size_t)i] || (only && !sel.selected(i))) continue;
        if (seen++ % step) continue;
        pts.insert(pts.end(), {(double)p[i*3], (double)p[i*3+1], (double)p[i*3+2]});
    }
    const size_t m = pts.size() / 3;
    if (!m) return false;
    double c[3];
    std::vector<double> v(m);
    for (int d = 0; d < 3; d++) {
        for (size_t i = 0; i < m; i++) v[i] = pts[i * 3 + d];
        std::nth_element(v.begin(), v.begin() + (ptrdiff_t)(m / 2), v.end());
        c[d] = v[m / 2];
    }
    for (size_t i = 0; i < m; i++) {
        const double dx = pts[i*3] - c[0], dy = pts[i*3+1] - c[1], dz = pts[i*3+2] - c[2];
        v[i] = std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    const size_t k = std::min(m - 1, (size_t)((only ? 0.98 : 0.9) * (double)(m - 1)));
    std::nth_element(v.begin(), v.begin() + (ptrdiff_t)k, v.end());
    const double r = std::max(v[k], 0.01 * (double)_doc->extent());
    const Sim3 to_shared = base_frame() * _doc->placement();
    to_shared.apply(c, centre);
    radius = r * to_shared.s;
    return radius > 0.0;
}

bool EditSession::draws_world_grid() const {
    return _doc && (_moved_ever || _xform.active() || !_doc->placement().is_identity());
}

bool EditSession::pointer_in_view(float& x, float& y) const {
    if (!_panel) return false;
    float ix, iy, iw, ih;
    _panel->image_rect(ix, iy, iw, ih);
    const ImVec2 m = ImGui::GetIO().MousePos;
    x = m.x - ix;
    y = m.y - iy;
    return iw > 8 && ih > 8 && x >= 0 && y >= 0 && x < iw && y < ih;
}

// The pivot in the positions() frame. Medians are not free over a million
// elements, so they are taken when the live set or the selection changes.
void EditSession::pivot_model(double out[3]) {
    out[0] = out[1] = out[2] = 0.0;
    const Pivot want = (Pivot)_pivot;
    if (want == Pivot::Origin) {
        // The point whose SAVED coordinate is the origin: saved = N^-1 E p,
        // so p = E^-1 N 0.
        const double zero[3] = {0, 0, 0};
        double n0[3];
        _doc->view_frame().apply(zero, n0);
        _doc->placement().inverse().apply(n0, out);
        return;
    }
    const int64_t n = _doc->count();
    const uint8_t* alive = _doc->alive();
    const Selection& sel = _doc->sel();
    if (_centres.layer != _doc->layer() || _centres.alive != _doc->alive_count() ||
        _centres.selected != sel.count() || _centres.sel_rev != _doc->revision()) {
        _centres.layer = _doc->layer();
        _centres.alive = _doc->alive_count();
        _centres.selected = sel.count();
        _centres.sel_rev = _doc->revision();
        const float* p = _doc->positions();
        const int64_t step = std::max<int64_t>(1, n / 400000);
        std::vector<float> ax[3], sx[3];
        double sum[3] = {0, 0, 0};
        int64_t m = 0;
        for (int64_t i = 0; i < n; i += step) {
            if (!alive[i]) continue;
            for (int k = 0; k < 3; k++) {
                ax[k].push_back(p[i*3+k]);
                sum[k] += p[i*3+k];
                if (sel.selected(i)) sx[k].push_back(p[i*3+k]);
            }
            m++;
        }
        for (int k = 0; k < 3; k++) {
            _centres.mean[k] = m ? sum[k] / (double)m : 0.0;
            auto mid = [](std::vector<float>& v) {
                if (v.empty()) return 0.0;
                std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
                return (double)v[v.size() / 2];
            };
            _centres.median[k] = mid(ax[k]);
            _centres.sel[k] = sx[k].empty() ? _centres.median[k] : mid(sx[k]);
        }
    }
    const double* c = want == Pivot::Mean ? _centres.mean
                    : want == Pivot::Selection ? _centres.sel : _centres.median;
    for (int k = 0; k < 3; k++) out[k] = c[k];
}

bool EditSession::xform_frame(XformFrame& f) {
    if (!_panel || !_doc) return false;
    float x, y, w, h;
    _panel->image_rect(x, y, w, h);
    if (w < 8.0f || h < 8.0f) return false;
    f.cam.W = (int)w;
    f.cam.H = (int)h;
    _panel->nav_camera(f.cam.W, f.cam.H, f.cam.w2c, f.cam.fx, f.cam.fy,
                       f.cam.camera_model, f.cam.eye);
    f.cam.cx = 0.5f * (float)f.cam.W;
    f.cam.cy = 0.5f * (float)f.cam.H;

    const Sim3 base = base_frame();
    // While the operator runs the pivot stays where it was when it began.
    const Sim3& placed = _xform.active() ? _xform_from : _doc->placement();
    double pm[3], q[3];
    pivot_model(pm);
    placed.apply(pm, q);
    base.apply(q, f.pivot);

    const Sim3 g = base * _doc->view_frame();
    const Sim3 l = base * placed * _doc->view_frame();
    for (int a = 0; a < 3; a++)
        for (int k = 0; k < 3; k++) {
            f.global_axes[a*3+k] = g.R[k*3+a];
            f.local_axes[a*3+k] = l.R[k*3+a];
        }
    f.unit = g.s;
    f.grid_cell = _panel->world_grid_cell();
    return true;
}


// ---------------------------------------------------------------------------
// Steps
// ---------------------------------------------------------------------------

void EditSession::push_placement() {
    if (!_panel || !_doc) return;
    float a[12];
    _doc->placement().to_3x4(a);
    _panel->set_edit_transform(a);
}

void EditSession::set_placement(const Sim3& next, const Msg& name, bool carry) {
    if (!_doc) return;
    Sim3 clean = next;
    clean.orthonormalize();
    const Sim3 was = _doc->placement();
    _doc->run(make_placement_op(*_doc, clean, name.get(), carry));
    _moved_ever = true;
    push_placement();
    if (carry && _panel) {
        // shared' = base * next * was^-1 * base^-1 * shared.
        const Sim3 base = base_frame();
        float a[12];
        (base * clean * was.inverse() * base.inverse()).to_3x4(a);
        _panel->carry_view(a);
    }
    _seen_placement = _doc->placement();
    _seen_head = _doc->history_head();
}

void EditSession::follow_history() {
    if (!_doc || !_panel) return;
    const int head = _doc->history_head();
    if (head == _seen_head) return;
    bool carried = false;
    const auto& ops = _doc->history();
    for (int i = std::min(head, _seen_head); i < std::max(head, _seen_head); i++)
        if (i >= 0 && i < (int)ops.size() && ops[(size_t)i]->carries_view())
            carried = true;
    if (carried) {
        const Sim3 base = base_frame();
        float a[12];
        (base * _doc->placement() * _seen_placement.inverse() * base.inverse()).to_3x4(a);
        _panel->carry_view(a);
    }
    _seen_head = head;
    _seen_placement = _doc->placement();
}

// shared' = step * shared, and shared = base * placement * p.
void EditSession::place_shared(const Sim3& step, const Msg& name) {
    const Sim3 base = base_frame();
    set_placement(base.inverse() * step * base * _doc->placement(), name);
}

// saved' = step * saved, and saved = N^-1 * placement * p.
void EditSession::place_saved(const Sim3& step, const Msg& name, bool carry) {
    const Sim3 n = _doc->view_frame();
    set_placement(n * step * n.inverse() * _doc->placement(), name, carry);
}

void EditSession::begin_xform(XformKind kind) {
    if (!_doc || busy() || _xform.active()) return;
    _xform_mode = kind;
    XformFrame f;
    float mx, my;
    // Started from the keyboard, so it begins where the pointer is; with the
    // pointer somewhere else the key still picks which handles are shown.
    if (!xform_frame(f) || !pointer_in_view(mx, my)) return;
    _xform_from = _doc->placement();
    _xform.begin(kind, f, mx, my, /*drag=*/false);
}

void EditSession::quarter_turn(int axis, bool negative) {
    XformFrame f;
    if (!xform_frame(f)) return;
    const Sim3 step = Sim3::rotation_about(f.global_axes + axis * 3,
                                           negative ? -kPi / 2 : kPi / 2, f.pivot);
    place_shared(step, msg::op_quarter_turn);
}

void EditSession::enter_transform() {
    if (!_doc) return;
    if (_tool.id() != ToolId::Transform) _xform_return = _tool.id();
    _tool.set_id(ToolId::Transform);
    _tab = 1;
    _tab_force = true;
    // The axes that get SAVED are the file's own. With the parsers' levelling
    // guess on top, the model would look upright and save tilted.
    if (_panel && _panel->has_levelling() && _panel->level_cameras()) {
        _levelling_was = true;
        _levelling_touched = true;
        _panel->set_level_cameras(false);
    }
}


// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------

std::vector<double> EditSession::saved_points(int64_t cap, bool selected_only,
                                              std::vector<int64_t>* index) const {
    std::vector<double> out;
    // Layer 0 whatever is current: a camera table is not a surface.
    const std::vector<uint8_t>& alive = _doc->alive_of(0);
    const Selection& sel = _doc->sel_of(0);
    const float* p = _doc->positions_of(0);
    const int64_t n = (int64_t)alive.size();
    const int64_t want = selected_only ? sel.count() : _doc->alive_count_of(0);
    const int64_t step = std::max<int64_t>(1, want / std::max<int64_t>(cap, 1));
    const Sim3 to_saved = _doc->view_frame().inverse() * _doc->placement();
    // What the user selected is what they meant, haze and all.
    const float* solid = selected_only ? nullptr : _doc->solidity();
    int64_t seen = 0;
    for (int64_t i = 0; i < n; i++) {
        if (!alive[(size_t)i] || (selected_only && !sel.selected(i))) continue;
        if (solid && solid[i] < 0.3f) continue;
        if (seen++ % step) continue;
        const double q[3] = {p[i*3], p[i*3+1], p[i*3+2]};
        double w[3];
        to_saved.apply(q, w);
        out.insert(out.end(), w, w + 3);
        if (index) index->push_back(i);
    }
    return out;
}

void EditSession::auto_align() {
    if (!_doc || busy()) return;
    std::vector<int64_t> index;
    const std::vector<double> pts = saved_points(250000, false, &index);
    const int64_t n = (int64_t)pts.size() / 3;
    if (n < 16) return;
    const Sim3 to_saved = _doc->view_frame().inverse() * _doc->placement();

    double up[3];
    float hint[3];
    const bool have_up = _doc->up_hint(hint);
    if (have_up) {
        const double h[3] = {hint[0], hint[1], hint[2]};
        to_saved.rotate(h, up);
    }
    std::vector<float> normals, weights, all_n, all_w;
    if (_doc->normals(all_n, all_w)) {
        normals.resize((size_t)n * 3);
        weights.resize((size_t)n);
        for (int64_t k = 0; k < n; k++) {
            const int64_t i = index[(size_t)k];
            const double v[3] = {all_n[(size_t)i*3], all_n[(size_t)i*3+1],
                                 all_n[(size_t)i*3+2]};
            double w[3];
            to_saved.rotate(v, w);
            for (int r = 0; r < 3; r++) normals[(size_t)k*3+r] = (float)w[r];
            weights[(size_t)k] = all_w[(size_t)i];
        }
    }
    align::AutoAlignOptions opt;
    opt.tol = 0.01 * _align_tol * (double)_doc->extent() * to_saved.s;
    opt.yaw = _align_yaw;
    opt.centre = _align_centre;
    const align::AutoAlignResult r = align::auto_align(
        pts.data(), n, have_up ? up : nullptr,
        normals.empty() ? nullptr : normals.data(),
        weights.empty() ? nullptr : weights.data(), opt);
    place_saved(r.T, msg::op_auto_align, /*carry=*/true);
    _status = spirula::i18n::format(
        r.ground ? (r.walls ? msg::align_found_both : msg::align_found_ground)
                 : msg::align_found_nothing,
        {(long long)std::lround(r.ground_share * 100.0)});
    _status_err = !r.ground;
    note(_status);
}

void EditSession::ground_from_selection() {
    if (!_doc || busy() || _doc->sel_of(0).empty()) return;
    const std::vector<double> pts = saved_points(250000, true);
    const int64_t n = (int64_t)pts.size() / 3;
    const Sim3 to_saved = _doc->view_frame().inverse() * _doc->placement();
    align::Plane pl;
    if (n < 8 || !align::fit_plane(pts.data(), n,
                                   0.01 * _align_tol * (double)_doc->extent() * to_saved.s,
                                   7u, pl)) {
        _status = msg::align_no_surface.get();
        _status_err = true;
        return;
    }
    // Which way is up is whichever way is nearer to what up is now: a floor
    // selected from above and one selected from below are the same floor.
    if (pl.n[2] < 0) {
        for (double& v : pl.n) v = -v;
        pl.d = -pl.d;
    }
    double R[9];
    const double zaxis[3] = {0, 0, 1};
    align::rotation_between(pl.n, zaxis, R);
    Sim3 step;
    for (int i = 0; i < 9; i++) step.R[i] = R[i];
    step.t[2] = pl.d;
    place_saved(step, msg::op_ground, /*carry=*/true);
}

void EditSession::pick_align(float px_, float py_) {
    const Pick what = _pick;
    ViewProjection vp;
    if (!_doc || !view(vp)) return;
    // Surfaces are made of layer 0; a click while the camera layer is
    // current still means the points under it.
    const int was = _doc->layer();
    _doc->set_layer(0);
    const int64_t hit = pick_element(*_doc, vp, px_, py_, 16.0f);
    _doc->set_layer(was);
    if (hit < 0) {
        _status = msg::align_no_surface.get();
        _status_err = true;
        return;
    }
    const Sim3 to_saved = _doc->view_frame().inverse() * _doc->placement();
    const float* p = _doc->positions_of(0) + hit * 3;
    const double pm[3] = {p[0], p[1], p[2]};
    double at[3], eye[3];
    to_saved.apply(pm, at);
    // view() reports the camera in the positions() frame, like the elements.
    const double em[3] = {vp.eye[0], vp.eye[1], vp.eye[2]};
    to_saved.apply(em, eye);

    _pick = Pick::None;
    if (what == Pick::Origin) {
        const double back[3] = {-at[0], -at[1], -at[2]};
        place_saved(Sim3::translation(back), msg::op_set_origin, /*carry=*/true);
        return;
    }

    const std::vector<double> pts = saved_points(1500000, false);
    const int64_t n = (int64_t)pts.size() / 3;
    const double r0 = 0.04 * (double)_doc->extent() * to_saved.s;
    if (what == Pick::Ground) {
        align::Plane pl;
        if (!align::fit_plane_at(pts.data(), n, at, r0, pl)) {
            _status = msg::align_no_surface.get();
            _status_err = true;
            return;
        }
        // A floor is looked at from above: its up is the side the eye is on.
        const double to_eye[3] = {eye[0]-at[0], eye[1]-at[1], eye[2]-at[2]};
        if (pl.n[0]*to_eye[0] + pl.n[1]*to_eye[1] + pl.n[2]*to_eye[2] < 0) {
            for (double& v : pl.n) v = -v;
            pl.d = -pl.d;
        }
        double R[9];
        const double zaxis[3] = {0, 0, 1};
        align::rotation_between(pl.n, zaxis, R);
        Sim3 step;
        for (int i = 0; i < 9; i++) step.R[i] = R[i];
        step.t[2] = pl.d;
        // Turn about the clicked point's own column, so the floor drops onto
        // the grid under where it was rather than swinging away from it.
        double moved[3];
        step.apply(at, moved);
        step.t[0] += at[0] - moved[0];
        step.t[1] += at[1] - moved[1];
        place_saved(step, msg::op_ground, /*carry=*/true);
        return;
    }

    double axes[9], corner[3];
    const int m = align::fit_corner(pts.data(), n, at, r0 * 2.5, axes, corner);
    if (m == 0) {
        _status = msg::align_no_surface.get();
        _status_err = true;
        return;
    }
    // Each surface goes to the axis it is already nearest, the best-measured
    // first, so the model turns as little as it can.
    int target[3] = {-1, -1, -1};
    double sign[3] = {1, 1, 1};
    bool used[3] = {false, false, false};
    for (int r = 0; r < 3; r++) {
        int best = -1;
        double best_d = -1.0;
        for (int k = 0; k < 3; k++) {
            if (used[k]) continue;
            const double d = std::fabs(axes[r*3+k]);
            if (d > best_d) { best_d = d; best = k; }
        }
        target[r] = best;
        used[best] = true;
        sign[r] = axes[r*3+best] < 0 ? -1.0 : 1.0;
    }
    // R = T^t A sends row r of A to the signed axis T holds for it. The last
    // row is the one the fit constructed, so it is the one that gives way
    // when the three signs do not make a rotation.
    auto build = [&](double R[9]) {
        for (int i = 0; i < 9; i++) R[i] = 0.0;
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 3; k++)
                R[target[r]*3+k] += sign[r] * axes[r*3+k];
    };
    double R[9];
    build(R);
    const double det = R[0]*(R[4]*R[8]-R[5]*R[7]) - R[1]*(R[3]*R[8]-R[5]*R[6]) +
                       R[2]*(R[3]*R[7]-R[4]*R[6]);
    if (det < 0) {
        sign[2] = -sign[2];
        build(R);
    }
    Sim3 step;
    for (int i = 0; i < 9; i++) step.R[i] = R[i];
    double moved[3];
    step.apply(corner, moved);
    for (int k = 0; k < 3; k++)
        step.t[k] = _corner_to_origin ? -moved[k] : corner[k] - moved[k];
    place_saved(step, msg::op_corner, /*carry=*/true);
    _status = spirula::i18n::format(msg::align_corner_found, {(long long)m});
    _status_err = false;
}

}  // namespace gui
