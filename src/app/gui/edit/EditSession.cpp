// EditSession.cpp -- see EditSession.h. The panel is EditPanel.cpp.

#include "app/gui/edit/EditSession.h"

#include "app/gui/GlLoader.h"
#include "app/gui/Layout.h"
#include "app/gui/ViewportPanel.h"
#include "i18n/Message.h"
#include "app/gui/edit/PointsDoc.h"
#include "app/gui/edit/WorldGrid.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/EditTransform.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <numeric>

namespace msg = spirula::i18n::msg::edit;
namespace xmsg = spirula::i18n::msg::xform;
using spirula::Sim3;

namespace gui {

EditSession::~EditSession() { close(); }

void EditSession::open(std::unique_ptr<EditDoc> doc, ViewportPanel* panel) {
    close();
    _doc = std::move(doc);
    _panel = panel;
    if (_panel) {
        _panel->set_interactor(this);
        _panel->set_center_provider([this](dsparse::CenterTable& t) {
            return _doc && !busy() && _doc->live_centers(t);
        });
    }
    _opt = SelectOptions{};
    _opt.by_extent = _doc && _doc->kind() == EditDoc::Kind::Splats;
    // Opening in Navigate: the first thing anyone does with a model they have
    // just opened is look at it from somewhere else.
    _tool.set_id(ToolId::Navigate);
    _tab = 0;
    _tab_force = true;
    _xform.cancel();
    _pick = Pick::None;
    _moved_ever = false;
    _levelling_touched = false;
    _saved_over_source = false;
    _centres = Centres{};
    _attrs.clear();
    _attrs_layer = -1;
    _axis[0] = AttrAxis{};
    _axis[1] = AttrAxis{};
    _axis[1].index = -1;
    _density_rev = 0;
    _plot_tool.cancel();
    _range_set = false;
    _samples.clear();
    _colours.clear();
    _adjust_head = -1;
    _adjust_live = false;
    if (_doc) _doc->mark_geometry_dirty();
    _seen_placement = Sim3();
    _seen_head = 0;
    push_placement();
    // Placements a kept repair could not register outlive the reopen.
    if (_doc && _doc->kind() == EditDoc::Kind::Points && !_placed_carry.empty() &&
        static_cast<PointsDoc&>(*_doc).dataset_dir() == _placed_carry_dir) {
        static_cast<PointsDoc&>(*_doc).set_placed(std::move(_placed_carry));
        _tab = 2;
    }
    _placed_carry.clear();
}

void EditSession::close() {
    _cancel = true;
    _repair_cancel = true;
    if (_repair_worker.joinable()) _repair_worker.join();
    _repair_cancel = false;
    _repair_busy = false;
    discard_repair();
    _repair_avail = -1;
    _cam_xform = false;
    if (_photo.worker.joinable()) _photo.worker.join();
    if (_photo.tex) {
        GLuint t = _photo.tex;
        glDeleteTextures(1, &t);
        _photo.tex = 0;
    }
    _photo.want.clear();
    _photo.loaded.clear();
    _photo.shown.clear();
    _photo.pic = Picture{};
    _missing.clear();
    _missing_read = false;
    _missing_at = -1;
    if (_comp_worker.joinable()) _comp_worker.join();
    if (_save_worker.joinable()) _save_worker.join();
    if (_attr_worker.joinable()) _attr_worker.join();
    _cancel = false;
    _comp_busy = false;
    _save_busy = false;
    _attr_busy = false;
    _attr_job_axis = -1;
    _comp = Components{};
    _pending.reset();
    _fly_block_key = 0;
    _xform.cancel();
    _pick = Pick::None;
    _train_after_save = false;
    _ask_train = false;
    if (_panel) {
        _panel->set_interactor(nullptr);
        _panel->set_center_provider(nullptr);
        // The placement was the editor's to show; what the pane goes back to
        // is the file, as it was read.
        static const float kIdentity[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
        _panel->set_edit_transform(kIdentity);
        if (_levelling_touched) _panel->set_level_cameras(_levelling_was);
    }
    _levelling_touched = false;
    if (_doc) _doc->revert_display();
    if (_panel) _panel->invalidate();
    _panel = nullptr;
    _doc.reset();
    _grid.clear();
    _spacing = 0.0f;
    _occ.clear();
    _tool.set_id(ToolId::Navigate);
    _status.clear();
    _ask_overwrite = false;
}

std::vector<std::string> EditSession::drain_log() {
    std::vector<std::string> out;
    {
        std::lock_guard<std::mutex> lk(_repair_log_mtx);
        out.swap(_repair_log);
    }
    out.insert(out.end(), _log.begin(), _log.end());
    _log.clear();
    return out;
}

void EditSession::note(const std::string& s) {
    _log.push_back(s);
    if (_log.size() > 200) _log.erase(_log.begin(), _log.begin() + 100);
}

bool EditSession::view(ViewProjection& out) const {
    if (!_panel) return false;
    float x, y, w, h;
    _panel->image_rect(x, y, w, h);
    if (w < 8.0f || h < 8.0f) return false;
    out.W = (int)w;
    out.H = (int)h;
    _panel->view_camera(out.W, out.H, out.w2c, out.fx, out.fy,
                        out.camera_model, out.eye);
    out.cx = 0.5f * (float)out.W;
    out.cy = 0.5f * (float)out.H;
    out.ortho_back = _panel->ortho_pullback(false);
    return true;
}

Combine EditSession::combine_now(bool shift, bool ctrl) const {
    if (shift && ctrl) return Combine::Intersect;
    if (shift) return Combine::Add;
    if (ctrl) return Combine::Subtract;
    return (Combine)_combine;
}

void EditSession::set_layer(int i) {
    if (!_doc || i == _doc->layer()) return;
    _doc->set_layer(i);
    // Everything measured is measured per layer: a camera table and a point
    // cloud have nothing in common but their frame.
    _grid.clear();
    _spacing = 0.0f;
    _occ_dirty = true;
    _comp = Components{};
    _pending.reset();
    _adjust_head = -1;
    _adjust_live = false;
    _doc->mark_display_dirty();
}


// ---------------------------------------------------------------------------
// Viewport input
// ---------------------------------------------------------------------------

bool EditSession::on_viewport_input(const ViewportInput& in) {
    if (!_doc || busy()) return false;

    // A running operator has the pointer whatever tool started it.
    if (_xform.active()) {
        XformFrame f;
        if (!xform_frame(f)) {
            _xform.cancel();
            push_placement();
            return true;
        }
        const TransformTool::Result r = _xform.update(in, f);
        if (_cam_xform) {
            auto& pd = static_cast<PointsDoc&>(*_doc);
            const bool moved = !_xform.delta().is_identity();
            if (r != TransformTool::Result::Confirmed && r != TransformTool::Result::Cancelled) {
                pd.set_moved_cameras(moved_by(_xform.delta()));
            } else {
                PointsDoc::Poses next = moved_by(_xform.delta());
                pd.set_moved_cameras(_cam_from);  // the op records the move from here
                if (r == TransformTool::Result::Confirmed && moved)
                    _doc->run(make_camera_move_op(pd, std::move(next), msg::op_move_cameras.get()));
                _cam_xform = false;
            }
            return true;
        }
        const Sim3 base = base_frame();
        const Sim3 preview = base.inverse() * _xform.delta() * base * _xform_from;
        if (r == TransformTool::Result::Confirmed) {
            if (!_xform.delta().is_identity()) {
                const spirula::i18n::Msg& name =
                    _xform.kind() == XformKind::Move ? xmsg::op_move
                    : _xform.kind() == XformKind::Rotate ? xmsg::op_rotate
                                                         : xmsg::op_scale;
                set_placement(preview, name);
            } else {
                push_placement();
            }
        } else if (r == TransformTool::Result::Cancelled) {
            push_placement();
        } else if (_panel) {
            float a[12];
            preview.to_3x4(a);
            _panel->set_edit_transform(a);
        }
        return true;
    }

    if (_tool.id() == ToolId::Transform) {
        if (_pick != Pick::None) {
            if (in.hovered && in.clicked) pick_align(in.x, in.y);
            return in.down || in.clicked;
        }
        XformFrame f;
        _xform_hot = -1;
        if (in.hovered && xform_frame(f)) {
            _xform_hot = _xform.hit_handle(_xform_mode, f, in.x, in.y);
            if (_xform_hot >= 0 && in.clicked) {
                _xform_from = _doc->placement();
                begin_camera_move();
                const int axis = _xform_hot < 3 ? _xform_hot
                               : _xform_hot < 6 ? _xform_hot - 3 : -1;
                _xform.begin(_xform_mode, f, in.x, in.y, /*drag=*/true, axis,
                             _xform_hot >= 3 && _xform_hot < 6);
                return true;
            }
        }
        // Off the handles the left button is still the camera's.
        return false;
    }

    ShapeStroke s;
    bool consumed = false;
    if (_tool.update(in, s, consumed)) {
        if (_tool.id() == ToolId::Piece)
            select_component_under(s.pts[0], s.pts[1],
                                   combine_now(in.shift, in.ctrl));
        else if (_tool.id() == ToolId::Eyedropper)
            pick_colour(s.pts[0], s.pts[1], in.shift);
        else
            apply_stroke(s, in);
    }
    return consumed;
}

void EditSession::draw_viewport_overlay(const ViewportOverlay& v) {
    if (!_doc) return;
    draw_photo_overlay(v);
    const ImVec2 origin(v.x, v.y);
    XformFrame f;
    const bool have = xform_frame(f);
    if (have && v.grid && draws_world_grid()) {
        float target[3] = {0, 0, 0};
        if (_panel) _panel->nav_target(target);
        const double focus[3] = {target[0], target[1], target[2]};
        draw_world_grid(v.dl, origin, f.cam, saved_to_shared(), v.grid_cell, focus);
    }
    if (have && _xform.active()) {
        _xform.draw_overlay(v.dl, origin, f);
        // Beside the pointer, where the eyes already are.
        const std::string text = _xform.readout(f);
        const ImVec2 m = ImGui::GetIO().MousePos;
        const ImVec2 at(m.x + px(18.0f), m.y + px(14.0f));
        v.dl->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 220), text.c_str());
        v.dl->AddText(at, IM_COL32(255, 255, 255, 255), text.c_str());
    } else if (have && _tool.id() == ToolId::Transform && _pick == Pick::None) {
        _xform.draw_handles(v.dl, origin, _xform_mode, f, _xform_hot);
    }
    _tool.draw_overlay(v.dl, origin);
}

void EditSession::apply_stroke(const ShapeStroke& s, const ViewportInput& in) {
    ViewProjection vp;
    if (!view(vp)) return;
    auto r = std::make_shared<SelectRecipe>();
    r->kind = SelectRecipe::Stencil;
    r->layer = _doc->layer();
    r->before = _doc->sel().weights();
    r->combine = combine_now(in.shift, in.ctrl);
    r->shape = s;
    r->view = vp;
    start_recipe(std::move(r));
}

namespace {

const spirula::i18n::Msg& shape_op_name(ShapeKind k) {
    switch (k) {
        case ShapeKind::Ellipse: return msg::op_select_ellipse;
        case ShapeKind::Lasso:   return msg::op_select_lasso;
        case ShapeKind::Polygon: return msg::op_select_polygon;
        case ShapeKind::Brush:   return msg::op_select_brush;
        default:                 return msg::op_select_box;
    }
}

}  // namespace

// Off it goes, or onto the worker's queue when it needs components it has
// not got yet.
void EditSession::start_recipe(std::shared_ptr<SelectRecipe> r) {
    const bool needs_components =
        r->kind == SelectRecipe::Piece || r->kind == SelectRecipe::Floaters;
    if (needs_components && !components_ready()) {
        _pending = std::move(r);
        _pending_push = true;
        return;
    }
    run_recipe(r, /*push=*/true);
}

void EditSession::run_recipe(const std::shared_ptr<const SelectRecipe>& rp,
                             bool push) {
    if (!_doc || !rp) return;
    const SelectRecipe& r = *rp;
    if (r.layer != _doc->layer()) return;
    std::vector<uint8_t> w((size_t)_doc->count(), 0);
    const spirula::i18n::Msg* name = &msg::op_select;

    switch (r.kind) {
        case SelectRecipe::Stencil: {
            Stencil st;
            rasterize_shape(r.shape, r.view.W, r.view.H, st);
            if (_opt.front_only) {
                if (_occ_dirty ||
                    std::memcmp(_occ_pose, r.view.w2c, sizeof _occ_pose) != 0) {
                    _occ.build(*_doc, r.view);
                    std::memcpy(_occ_pose, r.view.w2c, sizeof _occ_pose);
                    _occ_alive = _doc->alive_count();
                    _occ_dirty = false;
                }
            }
            _last_result = select_by_stencil(*_doc, r.view, st, _opt,
                                             _opt.front_only ? &_occ : nullptr, w);
            name = &shape_op_name(r.shape.kind);
            break;
        }
        case SelectRecipe::Grow:
        case SelectRecipe::Shrink: {
            ensure_grid();
            w = r.before;
            if (r.kind == SelectRecipe::Grow)
                _grid.grow(w, _doc->alive(), _doc->count());
            else
                _grid.shrink(w, _doc->alive(), _doc->count());
            name = r.kind == SelectRecipe::Grow ? &msg::op_grow : &msg::op_shrink;
            // Grow and shrink are their own combine: they start from what is
            // already selected rather than meeting it.
            if (push) _doc->run(make_select_op(*_doc, std::move(w), name->get(), rp));
            else {
                _doc->set_selection(w);
                _doc->mark_display_dirty();
            }
            return;
        }
        case SelectRecipe::Piece: {
            if (r.seed < 0) return;
            if (!components_ready()) {
                _pending = std::make_shared<SelectRecipe>(r);
                _pending_push = push;
                return;
            }
            const int32_t want = _comp.label[(size_t)r.seed];
            if (want >= 0)
                for (int64_t i = 0; i < _doc->count(); i++)
                    if (_comp.label[(size_t)i] == want) w[(size_t)i] = 255;
            name = &msg::op_select_piece;
            break;
        }
        case SelectRecipe::Floaters: {
            if (!components_ready()) {
                _pending = std::make_shared<SelectRecipe>(r);
                _pending_push = push;
                return;
            }
            const std::vector<int64_t>& sizes = _comp.sizes;
            std::vector<int32_t> order((size_t)sizes.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](int32_t a, int32_t b) {
                return sizes[(size_t)a] > sizes[(size_t)b];
            });
            std::vector<uint8_t> big((size_t)sizes.size(), 0);
            for (int i = 0; i < std::max(1, _keep_components) &&
                            i < (int)order.size(); i++)
                big[(size_t)order[(size_t)i]] = 1;
            for (int64_t i = 0; i < _doc->count(); i++) {
                const int32_t l = _comp.label[(size_t)i];
                if (l >= 0 && !big[(size_t)l]) w[(size_t)i] = 255;
            }
            name = &msg::op_select_floaters;
            break;
        }
    }

    Selection tmp;
    tmp.assign(r.before);
    tmp.combine(w.data(), r.combine, _doc->alive());
    if (push) _doc->run(make_select_op(*_doc, tmp.weights(), name->get(), rp));
    else {
        _doc->set_selection(tmp.weights());
        _doc->mark_display_dirty();
    }
}

void EditSession::run_select(std::vector<uint8_t> w, std::string label) {
    if (!_doc) return;
    _pending.reset();
    _doc->run(make_select_op(*_doc, std::move(w), std::move(label), {}));
}


// A setting is a history step of its own, and the selection it produced
// follows it back. The recipe comes from where the history STANDS, so walking
// back to a selection and ticking a box re-runs that selection.
void EditSession::run_setting(std::function<void(bool)> write,
                              std::string label) {
    std::shared_ptr<const SelectRecipe> recipe = _doc->current_recipe();
    _doc->run(make_setting_op(
        [this, write = std::move(write), recipe](bool redo) {
            write(redo);
            run_recipe(recipe, /*push=*/false);
        },
        std::move(label), recipe));
}

void EditSession::set_option(bool* slot, bool value,
                             const spirula::i18n::Msg& name) {
    if (!_doc || *slot == value) return;
    const bool old = *slot;
    run_setting([slot, value, old](bool redo) { *slot = redo ? value : old; },
                spirula::i18n::format(
                    value ? msg::op_option_on : msg::op_option_off, {name.get()}));
}

void EditSession::set_number(float* slot, float value,
                             const spirula::i18n::Msg& name) {
    if (!_doc || *slot == value) return;
    const float old = *slot;
    run_setting([slot, value, old](bool redo) { *slot = redo ? value : old; },
                spirula::i18n::format(
                    value > old ? msg::op_value_up : msg::op_value_down,
                    {name.get()}));
}

void EditSession::set_number(int* slot, int value,
                             const spirula::i18n::Msg& name) {
    if (!_doc || *slot == value) return;
    const int old = *slot;
    run_setting([slot, value, old](bool redo) { *slot = redo ? value : old; },
                spirula::i18n::format(
                    value > old ? msg::op_value_up : msg::op_value_down,
                    {name.get()}));
}


// ---------------------------------------------------------------------------
// Neighbourhood operations
// ---------------------------------------------------------------------------

float EditSession::reach() {
    if (!_doc) return 0.0f;
    if (!(_spacing > 0.0f)) _spacing = _grid.measure_spacing(*_doc);
    // A reach of zero still needs cells to sort the elements into; there it
    // means "only what shares a cell", which is the smallest honest answer.
    return _spacing * std::max(_radius_mul, 0.05f);
}

void EditSession::ensure_grid() {
    if (!_doc) return;
    const float r = reach();
    if (_grid.built() && std::fabs(_grid.cell() - r) < 1e-9f * std::max(r, 1.0f))
        return;
    _grid.build(*_doc, r);
}

// A mesh says what is joined to what; a cloud has to be asked by proximity,
// and a Gaussian's own extent is part of that answer.
void EditSession::compute_components() {
    int64_t pairs = 0;
    if (const int32_t* topo = _doc->topology(pairs)) {
        components_from_pairs(topo, pairs, _doc->alive(), _doc->count(),
                              _comp.label, _comp.sizes);
        return;
    }
    // Gaussians overlap by construction, so "these two touch" has to mean
    // they interpenetrate: at 1.0 a trained scene is one piece.
    constexpr float kOverlap = 0.5f;
    ensure_grid();
    if (!_grid.components(_doc->alive(), _doc->count(), _comp.label,
                          _comp.sizes, _doc->radii(), kOverlap, &_cancel))
        _comp = Components{};
}

bool EditSession::components_ready() {
    const float r = reach();
    if (_comp.layer == _doc->layer() && _comp.alive == _doc->alive_count() &&
        _comp.radius == r && _comp.label.size() == (size_t)_doc->count())
        return true;
    if (_comp_busy.load()) return false;
    if (_comp_worker.joinable()) _comp_worker.join();
    _comp = Components{};
    _comp.layer = _doc->layer();
    _comp.alive = _doc->alive_count();
    _comp.radius = r;
    _cancel = false;
    _comp_busy = true;
    // Off the GUI thread, and cancellable: every edit is refused until it
    // lands, but the window keeps drawing and the user keeps the camera.
    _comp_worker = std::thread([this] {
        compute_components();
        _comp_busy = false;
    });
    return false;
}

void EditSession::cancel_work() {
    _cancel = true;
    _repair_cancel = true;
    _pending.reset();
}

void EditSession::grow_shrink(bool grow) {
    if (!_doc) return;
    auto r = std::make_shared<SelectRecipe>();
    r->kind = grow ? SelectRecipe::Grow : SelectRecipe::Shrink;
    r->layer = _doc->layer();
    r->before = _doc->sel().weights();
    start_recipe(std::move(r));
}

void EditSession::select_component_under(float px, float py, Combine how) {
    if (!_doc) return;
    ViewProjection vp;
    if (!view(vp)) return;
    const int64_t hit = pick_element(*_doc, vp, px, py, 16.0f);
    if (hit < 0) {
        // Clicking nothing in Replace means nothing is selected, which is
        // what every other selection tool on earth does.
        if (how == Combine::Replace && !_doc->sel().empty()) select_all(false);
        return;
    }
    auto r = std::make_shared<SelectRecipe>();
    r->kind = SelectRecipe::Piece;
    r->layer = _doc->layer();
    r->before = _doc->sel().weights();
    r->combine = how;
    r->seed = hit;
    start_recipe(std::move(r));
}

void EditSession::keep_largest_components() {
    if (!_doc) return;
    auto r = std::make_shared<SelectRecipe>();
    r->kind = SelectRecipe::Floaters;
    r->layer = _doc->layer();
    r->before = _doc->sel().weights();
    r->combine = Combine::Replace;
    start_recipe(std::move(r));
}

void EditSession::select_all(bool on) {
    if (!_doc) return;
    std::vector<uint8_t> w((size_t)_doc->count(), 0);
    if (on)
        for (int64_t i = 0; i < _doc->count(); i++)
            if (_doc->alive()[i]) w[(size_t)i] = 255;
    run_select(std::move(w),
               (on ? msg::op_select_all : msg::op_select_none).get());
}

void EditSession::invert_selection() {
    if (!_doc) return;
    std::vector<uint8_t> w = _doc->sel().weights();
    for (int64_t i = 0; i < _doc->count(); i++)
        w[(size_t)i] = _doc->alive()[i] ? (uint8_t)(255 - w[(size_t)i]) : 0;
    run_select(std::move(w), msg::op_select_invert.get());
}


// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void EditSession::poll() {
    if (!_doc) return;
    if (_fly_block_key && !ImGui::IsKeyDown((ImGuiKey)_fly_block_key))
        _fly_block_key = 0;
    if (!_comp_busy.load() && _comp_worker.joinable()) {
        _comp_worker.join();
        // A finished components run is the pending recipe's cue.
        if (_pending) {
            std::shared_ptr<SelectRecipe> r;
            r.swap(_pending);
            if (!_comp.label.empty()) run_recipe(r, _pending_push);
        }
    }
    if (!_attr_busy.load() && _attr_worker.joinable()) {
        _attr_worker.join();
        if (_attr_job_axis >= 0) {
            // Cancelled part way is not an answer; it is kept as a failure so
            // the same search is not started again the next frame.
            if (_cancel.load()) _attr_job.failed = true;
            const int index = _axis[_attr_job_axis].index;
            _axis[_attr_job_axis] = std::move(_attr_job);
            _axis[_attr_job_axis].index = index;
            _attr_job = AttrAxis{};
        }
        _attr_job_axis = -1;
        _cancel = false;
    }
    if (!_save_busy.load() && _save_worker.joinable()) {
        _save_worker.join();
        const bool to_trainer = _train_after_save;
        _train_after_save = false;
        if (_save_error.empty()) {
            if (_on_saved) _on_saved(_doc->source_path(), _save_path, _save_placement);
            _doc->mark_saved();
            if (_save_path == _doc->source_path() ||
                _save_path == _doc->default_save_path(_save_target))
                _saved_over_source = true;
            _status = spirula::i18n::format(msg::saved_to,
                                            {_doc->default_save_path(_save_target)});
            _status_err = false;
            // Last: the owner is free to end this session in answer.
            if (to_trainer && _to_trainer)
                _to_trainer(_save_path, trainer_dataset());
        } else {
            _status = spirula::i18n::format(msg::save_failed, {_save_error});
            _status_err = true;
        }
        note(_status);
    }
    poll_repair();
    if (!_doc) return;
    if (busy()) {
        if (_panel) _panel->invalidate();
        return;
    }
    if (_keys_on) handle_keys();
    if (!_xform.active()) {
        follow_history();
        push_placement();
    }
    // The occlusion buffer belongs to one camera and one live set; either
    // moving invalidates it, and rebuilding is the next selection's business
    // rather than this frame's.
    ViewProjection vp;
    if (view(vp) && std::memcmp(_occ_pose, vp.w2c, sizeof _occ_pose) != 0)
        _occ_dirty = true;
    if (_doc->alive_count() != _occ_alive) _occ_dirty = true;
    if (_doc->publish() && _panel) _panel->invalidate();
}


// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

std::string EditSession::trainer_dataset() const {
    if (!_to_trainer || !_doc || _doc->kind() != EditDoc::Kind::Points) return {};
    if (folder_target() < 0) return {};
    return static_cast<const PointsDoc*>(_doc.get())->dataset_dir();
}

int EditSession::folder_target() const {
    if (!_doc) return -1;
    const std::vector<SaveTarget> t = _doc->save_targets();
    for (size_t i = 0; i < t.size(); i++)
        if (t[i].folder) return (int)i;
    return -1;
}

bool EditSession::can_save_in_place() const {
    return _doc && !_doc->default_save_path(_save_target).empty();
}

bool EditSession::can_save_copy() const {
    if (!_doc || !_pick_save) return false;
    const std::vector<SaveTarget> t = _doc->save_targets();
    return _save_target >= 0 && _save_target < (int)t.size() &&
           t[(size_t)_save_target].copy;
}

void EditSession::save_in_place() {
    if (!_doc) return;
    save_to(_save_target, _doc->default_save_path(_save_target));
}

void EditSession::ask_save_copy() {
    if (!_doc || !_pick_save) return;
    const std::vector<SaveTarget> t = _doc->save_targets();
    if (_save_target < 0 || _save_target >= (int)t.size()) return;
    const SaveTarget& target = t[(size_t)_save_target];
    if (!target.copy) return;
    const std::string home = _doc->default_save_path(_save_target);
    std::string stem =
        std::filesystem::path(home.empty() ? _doc->source_path() : home)
            .stem()
            .string();
    if (stem.empty()) stem = "model";
    _pick_save(_save_target, target.ext, target.folder,
               stem + "_edited" + target.ext);
}

// On a worker, because one linked mesh is several hundred-megabyte files and
// a window that stops answering is one the desktop offers to kill.
void EditSession::save_to(int target, const std::string& path) {
    if (!_doc || path.empty() || busy()) return;
    if (_save_worker.joinable()) _save_worker.join();
    _save_error.clear();
    _save_path = path;
    _save_placement = _doc->file_placement();
    _save_done = 0;
    _save_total = std::max(1, _doc->save_steps(target));
    _save_busy = true;
    EditDoc* doc = _doc.get();
    _save_worker = std::thread([this, doc, target, path] {
        try {
            doc->save(target, path, &_save_done);
        } catch (const std::exception& e) {
            _save_error = e.what();
        }
        _save_busy = false;
    });
}

}  // namespace gui
