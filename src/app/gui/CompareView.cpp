// CompareView.cpp -- see CompareView.h.

#include "app/gui/CompareView.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "app/gui/edit/MeshDoc.h"
#include "app/gui/edit/PointsDoc.h"
#include "app/gui/edit/SplatDoc.h"
#include "checkpoint/SplatPly.h"
#include "core/Camera.h"
#include "data/Json.h"
#include "engine/Engine.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Render.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace rmsg = spirula::i18n::msg::render;

namespace gui {

namespace {

// The placement, as the row-major 3x4 similarity ViewportPanel takes.
// `base_*` is the alignment onto the first model's frame; the hand
// adjustment applies after it.
void placement_matrix(const float euler_deg[3], float scale,
                      const float offset[3], float base_scale,
                      const float base_offset[3], float out[12]) {
    const float k = 3.14159265358979f / 180.0f;
    const float cx = std::cos(euler_deg[0]*k), sx = std::sin(euler_deg[0]*k);
    const float cy = std::cos(euler_deg[1]*k), sy = std::sin(euler_deg[1]*k);
    const float cz = std::cos(euler_deg[2]*k), sz = std::sin(euler_deg[2]*k);
    const float R[9] = {
        cz*cy,  cz*sy*sx - sz*cx,  cz*sy*cx + sz*sx,
        sz*cy,  sz*sy*sx + cz*cx,  sz*sy*cx - cz*sx,
          -sy,             cy*sx,             cy*cx};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++)
            out[r*4 + c] = scale * base_scale * R[r*3 + c];
        out[r*4 + 3] = offset[r] + scale * (R[r*3+0]*base_offset[0] +
                                            R[r*3+1]*base_offset[1] +
                                            R[r*3+2]*base_offset[2]);
    }
}

std::string display_name(const std::string& path) {
    if (path.empty()) return path;
    fs::path p(path);
    // A checkpoint is <run>/step-*.ckpt/splat.ply -- the leaf alone would
    // name every model in the view "splat".
    std::string leaf = p.filename().string();
    std::string parent = p.parent_path().filename().string();
    return parent.empty() ? leaf : parent + "/" + leaf;
}

}  // namespace


CompareView::~CompareView() { close(); }

void CompareView::take_engine() {
    if (_engine_taken) return;
    std::lock_guard<std::mutex> lk(_engine_mutex);
    engine_reset();
    _engine_taken = true;
    _overlay_key.clear();
}

int CompareView::claim_slot() {
    for (int i = 0; i < kMaxEngineScenes; i++)
        if (!(_slots_used & (1u << i))) {
            _slots_used |= 1u << i;
            return i;
        }
    return -1;
}

void CompareView::open(const std::string& path) {
    close();
    add(path);
}

void CompareView::add(const std::string& path,
                      const spirula::i18n::Msg* title) {
    if (path.empty() || full()) return;
    take_engine();
    auto m = std::make_unique<Model>();
    m->path = path;
    m->title = title;
    m->slot = claim_slot();
    m->load_id = ++_loads;
    m->uid = ++_uids;
    m->src.open(path, m->slot, &_engine_mutex);
    _models.push_back(std::move(m));
}

int CompareView::index_of(const std::string& path) const {
    for (int i = 0; i < count(); i++)
        if (_models[(size_t)i]->path == path) return i;
    return -1;
}

void CompareView::set_shown(const std::string& path, bool on,
                            const spirula::i18n::Msg* title) {
    const int at = index_of(path);
    if (on == (at >= 0)) return;
    if (on) add(path, title);
    else _pending_remove = at;
}

void CompareView::remove(int index) {
    if (index < 0 || index >= count()) return;
    if (_render_index == index) end_render();
    else if (_render_index > index) _render_index--;
    if (_edit_index == index) end_edit(false);
    else if (_edit_index > index) _edit_index--;
    Model& m = *_models[index];
    m.panel.detach();
    m.panel.destroy_gl();
    m.src.close();
    if (m.slot >= 0) _slots_used &= ~(1u << m.slot);
    _models.erase(_models.begin() + index);
    // The first model defines the shared frame; losing it re-frames the rest.
    if (index == 0) _overlay_key.clear();
}

void CompareView::move(int index, int dir) {
    const int to = index + dir;
    if (index < 0 || index >= count() || to < 0 || to >= count()) return;
    if (_edit_index == index) _edit_index = to;
    else if (_edit_index == to) _edit_index = index;
    if (_render_index == index) _render_index = to;
    else if (_render_index == to) _render_index = index;
    std::swap(_models[index], _models[to]);
    if (index == 0 || to == 0) _overlay_key.clear();
}

void CompareView::close() {
    _render_when_ready = false;
    _render_project.clear();
    end_render();
    _run_datasets.clear();
    end_edit(/*reload_panes=*/false);
    // No destroy_gl here: close() also runs from the destructor, by which
    // point the GL context may be gone. GuiApp::shutdown calls destroy_gl()
    // while it is still current.
    for (auto& m : _models) {
        m->panel.detach();
        m->src.close();
    }
    _models.clear();
    _slots_used = 0;
    _overlay_key.clear();
    _pending_remove = -1;
    _pending_move = 0;
    if (_engine_taken) {
        std::lock_guard<std::mutex> lk(_engine_mutex);
        engine_reset();
        _engine_taken = false;
    }
}

void CompareView::destroy_gl() {
    for (auto& m : _models) {
        m->panel.detach();
        m->panel.destroy_gl();
    }
}

std::vector<std::string> CompareView::drain_log() {
    std::vector<std::string> out;
    for (auto& m : _models)
        for (auto& s : m->src.drain_log()) out.push_back(std::move(s));
    for (auto& s : _edit.drain_log()) out.push_back(std::move(s));
    for (auto& s : _render.drain_log()) out.push_back(std::move(s));
    for (auto& s : _log) out.push_back(std::move(s));
    _log.clear();
    // Written by the loader thread, which clears `_edit_loading` after it.
    if (!_edit_loading.load() && !_edit_error.empty()) {
        out.push_back(spirula::i18n::format(emsg::edit_failed, {_edit_error}));
        _edit_error.clear();
    }
    return out;
}


// ---------------------------------------------------------------------------
// Attach + placement
// ---------------------------------------------------------------------------

void CompareView::attach(Model& m) {
    switch (m.src.kind()) {
        case SplatViewer::Kind::Points:
            // A reconstruction has cameras worth seeing; a loose point file
            // has none, so the controls for them are offered on the first.
            m.panel.attach_preview_data(m.src.points(), m.src.post(),
                                        m.src.scene_key(), 1.0f,
                                        m.src.post().n_post > 0);
            break;
        case SplatViewer::Kind::Mesh:
            m.panel.attach_preview_mesh(m.src.mesh(), m.src.mesh_to_normalized(),
                                        m.src.scene_key());
            break;
        default:
            m.panel.attach_scene(m.src.render_config(), m.src.make_hooks(),
                                 m.src.scene_key());
            // A file being looked at can be drawn a different way than it was
            // trained; a training session cannot, so only this path offers the
            // controls.
            m.panel.enable_scene_options(
                m.src.render_config().primitive, m.src.sh_degree(),
                m.src.gamut(), m.src.transfer(), m.src.linear_color(),
                [&m](const char* g, int t, bool lin) {
                    m.src.set_color_space(g, t, lin);
                },
                [&m] { m.src.release_screen_buffers(); });
            break;
    }
    m.attached = true;
    // Land on whatever the panes already showing are looking at, so a model
    // added to a view in progress does not arrive facing somewhere else.
    for (auto& o : _models)
        if (o.get() != &m && o->attached) {
            m.panel.sync_view_from(o->panel);
            break;
        }
}

void CompareView::ensure_viewer_overlay() {
    if (_models.empty()) return;
    SplatViewer& ref = _models[0]->src;
    if (!ref.ready()) return;
    const std::string key = ref.scene_key();
    if (key == _overlay_key) return;
    _overlay_key = key;

    // The grid is the FIRST model's, in that model's own coordinates: the
    // models a comparison view holds are meant to be one scene, so one grid
    // serves them all and the panes stay comparable.
    const float unit = ref.frame_unit();
    const float radius = unit / 1.25f;
    const float* c = ref.frame_center();
    std::lock_guard<std::mutex> lk(_engine_mutex);
    // engine_blit_view refuses to run before engine_viewer_init, and that
    // wants at least one camera -- so a file with none gets a placeholder,
    // sized to nothing and drawn by nothing. The overlay state is the point.
    std::vector<float> intrins{32, 32, 32, 32};
    std::vector<float> dist((size_t)kCameraDistortionParams, 0.0f);
    std::vector<float> c2w{1, 0, 0, c[0],
                           0, 1, 0, c[1],
                           0, 0, 1, c[2]};
    std::vector<int32_t> models_i{0}, w_i{64}, h_i{64},   // 0 = PINHOLE
                         dist_tier{0};                    // 0 = None
    auto tvf = [](std::vector<float>& v, std::vector<int64_t> shape) {
        return TorchTensorView{(uint64_t)(uintptr_t)v.data(),
                               (uint32_t)sizeof(float), std::move(shape)};
    };
    auto tvi = [](std::vector<int32_t>& v, std::vector<int64_t> shape) {
        return TorchTensorView{(uint64_t)(uintptr_t)v.data(),
                               (uint32_t)sizeof(int32_t), std::move(shape)};
    };
    engine_viewer_init(tvi(models_i, {1}), tvf(intrins, {1, 4}),
                       tvf(dist, {1, kCameraDistortionParams}),
                       tvi(dist_tier, {1}), tvf(c2w, {1, 3, 4}),
                       tvi(w_i, {1}), tvi(h_i, {1}),
                       /*camera_size=*/radius * 1e-3f);
    engine_viewer_set_grid(radius, unit);
}

void CompareView::update_placements() {
    if (_models.empty()) return;
    const SplatViewer& ref = _models[0]->src;
    const bool ref_ready = ref.ready();
    const float u0 = ref.frame_unit();
    const float* c0 = ref.frame_center();
    for (auto& m : _models) {
        float base_scale = 1.0f, base_offset[3] = {0, 0, 0};
        // Alignment is the two frames' difference expressed in the first
        // model's units; both have to be fitted before it means anything.
        if (m->align && ref_ready && m->src.ready() && u0 > 1e-20f) {
            base_scale = m->src.frame_unit() / u0;
            for (int i = 0; i < 3; i++)
                base_offset[i] = (m->src.frame_center()[i] - c0[i]) / u0;
        }
        float a[12];
        placement_matrix(m->euler, m->scale, m->offset, base_scale,
                         base_offset, a);
        m->panel.set_model_transform(a);
    }
}

void CompareView::poll() {
    // Deferred from the pane menus: removing a model mid-draw would pull the
    // child window out from under the popup that asked for it.
    if (_pending_move) {
        move(_pending_move_index, _pending_move);
        _pending_move = 0;
    }
    if (_pending_remove < 0 && !_pending_remove_uids.empty()) {
        const uint64_t uid = _pending_remove_uids.front();
        _pending_remove_uids.erase(_pending_remove_uids.begin());
        for (int i = 0; i < count(); i++)
            if (_models[(size_t)i]->uid == uid) _pending_remove = i;
    }
    if (_pending_view >= 0) {
        const int to = _pending_view;
        const bool edit = _pending_view_edit;
        _pending_view = -1;
        if (to < count() && _models[(size_t)to]->attached) {
            if (to != _render_index) begin_render(to);
            if (edit && _render_index == to) begin_edit(to);
        }
    }
    if (_pending_remove >= 0) {
        const int gone = _pending_remove;
        _pending_remove = -1;
        // The render's own model: it carries on with the next one open.
        int next = -1;
        for (int i = 0; gone == _render_index && i < count() && next < 0; i++)
            if (i != gone && _models[(size_t)i]->attached) next = i;
        auto drop = [this, gone, next] {
            // Handed on, not left: no autosave notice for a move still open.
            if (next >= 0) end_render(true);
            remove(gone);
            if (next >= 0) begin_render(next > gone ? next - 1 : next);
        };
        if (gone == _edit_index && edit_dirty()) confirm_discard_edits(drop);
        else drop();
    }
    for (auto& m : _models)
        if (!m->attached && m->src.ready()) attach(*m);
    ensure_viewer_overlay();
    update_placements();
    if (_edit_replaced >= 0) {
        _edit_reopen = _edit_replaced;
        _edit_replaced = -1;
        end_edit(true);
    }
    if (_edit_reopen >= 0 && _edit_index < 0 && _edit_reopen < count() &&
        _models[(size_t)_edit_reopen]->attached) {
        const int i = _edit_reopen;
        _edit_reopen = -1;
        begin_edit(i);
    }
    if (_edit_when_ready && _edit_index < 0 && !_edit_loading.load() &&
        !_models.empty() && _models[0]->attached) {
        _edit_when_ready = false;
        begin_edit(0);
    }
    if (_render_when_ready && _render_index < 0 && !_models.empty() &&
        _models[0]->attached) {
        _render_when_ready = false;
        begin_render(0);
    }
    finish_edit_load();
    _edit.set_keys(_render_index < 0);
    _edit.set_others_open(count() > 1);
    if (_edit.active()) _edit.poll();
    sync_placements();
    if (_render_index >= 0) {
        feed_render();
        _render.poll();
        // A project asked for with the model: now the render has its models.
        if (!_render_project.empty()) {
            _render.open_project(_render_project);
            _render_project.clear();
        }
    }
}


// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

bool CompareView::edit_dirty() const {
    return _edit_index >= 0 && _edit.active() &&
           const_cast<EditSession&>(_edit).doc()->dirty();
}

void CompareView::confirm_discard_edits(std::function<void()> then) {
    if (!edit_dirty()) {
        if (then) then();
        return;
    }
    _discard_then = std::move(then);
    _ask_discard = true;
}

void CompareView::reload(int index) {
    if (index < 0 || index >= count()) return;
    Model& m = *_models[(size_t)index];
    m.panel.detach();
    m.src.close();
    m.src.open(m.path, m.slot, &_engine_mutex);
    m.load_id = ++_loads;
    m.attached = false;
    if (index == 0) _overlay_key.clear();
}

void CompareView::end_edit(bool reload_panes) {
    _edit_when_ready = false;
    // Whatever the edit did to the other panes goes back with it.
    if (_edit_index >= 0) show_sibling_meshes(_edit_index, FaceCut{});
    if (_edit_worker.joinable()) _edit_worker.join();
    _edit_loading = false;
    _edit_pending.reset();
    // The pane goes back to what it LOADED, and after a save over that file
    // what it loaded is no longer what is on disk.
    const int index = _edit_index;
    const bool stale = _edit.active() && _edit.saved_over_source();
    const bool linked = stale && _edit.doc()->linked_count() > 0;
    // The others followed a placement that is now only in memory, unless it
    // went into the file they were aligned with.
    if (_synced && !stale) {
        static const float kIdentity[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        for (int i = 0; i < count(); i++)
            if (i != index) _models[(size_t)i]->panel.set_edit_transform(kIdentity);
    }
    _synced = false;
    _edit.close();
    _edit_index = -1;
    // Closing the editor let go of the pane; a render on it takes it back.
    if (_render_index >= 0 && _render_index < count())
        _models[(size_t)_render_index]->panel.set_interactor(&_render);
    if (!stale || !reload_panes) return;
    for (int i = 0; i < count(); i++)
        if (i == index || (linked && _models[(size_t)i]->attached &&
                           _models[(size_t)i]->src.kind() == SplatViewer::Kind::Mesh))
            reload(i);
}

void CompareView::begin_edit(int index) {
    if (index < 0 || index >= count()) return;
    // One panel beside the panes at a time; the render's project is kept.
    end_render(true);
    if (_edit_index == index && _edit.active()) {
        _models[(size_t)index]->panel.set_interactor(&_edit);
        return;
    }
    end_edit();
    Model& m = *_models[index];
    if (!m.src.ready()) return;
    _edit_index = index;
    _edit_error.clear();
    if (_render_allowed)
        _edit.set_to_render([this] {
            if (_edit_index >= 0) begin_render(_edit_index);
        });
    else
        _edit.set_to_render(nullptr);
    // A model saved moved takes its camera projects with it.
    _edit.set_on_model_replaced([this] { _edit_replaced = _edit_index; });
    _edit.set_on_saved([this](const std::string& source, const std::string& saved,
                              const spirula::Sim3& placement) {
        std::string dir;
        _render.note_saved(saved, placement);
        const int n = render::copy_moved_projects(source, saved, placement, dir);
        if (n > 0) _log.push_back(spirula::i18n::format(rmsg::projects_moved, {(long long)n, dir}));
    });

    switch (m.src.kind()) {
        case SplatViewer::Kind::Points: {
            ParsedDataset ds = m.src.points();
            PostSplitCameras post = m.src.post();
            const std::string src = m.src.file();
            const std::string dir = m.src.dataset_dir();
            ViewportPanel* panel = &m.panel;
            const std::string key = m.src.scene_key();
            _edit.open(std::make_unique<PointsDoc>(
                           std::move(ds), std::move(post), src, dir,
                           [panel, key](const ParsedDataset& d,
                                        const PostSplitCameras& p,
                                        const uint8_t* cam_sel, const float* cam_rgb) {
                               panel->attach_preview_data(d, p, key, 1.0f,
                                                          p.n_post > 0, cam_sel, cam_rgb);
                           }),
                       panel);
            break;
        }
        case SplatViewer::Kind::Mesh: {
            meshing::MeshData mesh = m.src.mesh();
            float t2n[12];
            for (int i = 0; i < 12; i++) t2n[i] = m.src.mesh_to_normalized()[i];
            ViewportPanel* panel = &m.panel;
            const std::string key = m.src.scene_key();
            const std::string file = m.src.file();
            auto doc = std::make_unique<MeshDoc>(
                std::move(mesh), file, t2n,
                [panel, key](const meshing::MeshData& d, const float* a) {
                    panel->attach_preview_mesh(d, a, key);
                });
            if (_siblings_of) doc->set_siblings(_siblings_of(file));
            // The panes showing the other outputs of the same run follow the
            // deletions, so one edit is one preview across all of them.
            doc->set_sibling_preview([this, index](const FaceCut& cut) {
                show_sibling_meshes(index, cut);
            });
            _edit.open(std::move(doc), panel);
            break;
        }
        default: {
            // The PLY is read again rather than kept: a viewer that held every
            // open model host-side would double the memory of a comparison
            // nobody is editing.
            const std::string file = m.src.file();
            const int slot = m.src.scene_slot();
            std::mutex* mu = m.src.engine_mutex();
            float t2v[12];
            m.src.to_view_frame(t2v);
            const bool linear = m.src.linear_color();
            _edit_loading = true;
            _edit_worker = std::thread([this, file, slot, mu, t2v, linear] {
                try {
                    spirula::SplatCloud c = spirula::read_splat_ply(file);
                    auto doc = std::make_unique<SplatDoc>(std::move(c), file,
                                                          t2v, slot, mu);
                    doc->set_linear_colour(linear);
                    _edit_pending = std::move(doc);
                } catch (const std::exception& e) {
                    _edit_error = e.what();
                }
                _edit_loading = false;
            });
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

CompareView::Sidebar CompareView::sidebar() const {
    if (_render_index >= 0) return Sidebar::Render;
    if (_edit_index >= 0 && _edit.active()) return Sidebar::Edit;
    return Sidebar::None;
}

void CompareView::begin_render(int index) {
    _render.set_switch_to_edit([this] {
        if (_render_index >= 0) begin_edit(_render_index);
    });
    _render.set_leave([this] { end_render(); });
    _render.set_remove_model([this](int pane) {
        if (pane >= 0 && pane < count()) _pending_remove_uids.push_back(_models[(size_t)pane]->uid);
    });
    _render.set_add_model([this](const std::string& path) {
        if (!full()) add(path);
    });
    _render.set_view_model([this](int pane, bool edit) {
        _pending_view = pane;
        _pending_view_edit = edit;
    });
    if (index < 0 || index >= count() || !_models[(size_t)index]->attached) return;
    // An edit on another pane would be left drawing nowhere.
    if (_edit_index >= 0 && _edit_index != index) {
        confirm_discard_edits([this, index] {
            end_edit();
            begin_render(index);
        });
        return;
    }
    if (_render_index >= 0 && _render_index != index) end_render(true);
    _render_index = index;
    feed_render();
    _render.open(&_models[(size_t)index]->panel);
}

void CompareView::end_render(bool switching) {
    if (_render_index < 0) return;
    const int index = _render_index;
    _render.close(switching);
    _render_index = -1;
    if (index < count()) _models[(size_t)index]->panel.set_image_width(0.0f);
    if (index < count() && _edit_index == index && _edit.active())
        _models[(size_t)index]->panel.set_interactor(&_edit);
}

const ParsedDataset* CompareView::run_dataset(const std::string& model_file) {
    if (model_file.empty()) return nullptr;
    auto it = _run_datasets.find(model_file);
    if (it == _run_datasets.end()) {
        auto rd = std::make_unique<RunDataset>();
        RunDataset* r = rd.get();
        r->worker = std::thread([r, model_file] {
            try {
                // config.json sits in the run folder: beside a mesh, one up
                // from a checkpoint's splat.ply.
                const fs::path f = fs::u8path(model_file);
                fs::path run;
                std::error_code ec;
                for (fs::path d : {f.parent_path(), f.parent_path().parent_path()})
                    if (fs::is_regular_file(d / "config.json", ec)) { run = d; break; }
                if (run.empty()) { r->done = true; return; }
                const JsonValue cfg = json_parse_file((run / "config.json").string());
                const JsonValue* data = cfg.find("data");
                if (!data || data->as_string().empty()) { r->done = true; return; }
                fs::path dir = fs::u8path(data->as_string());
                if (dir.is_relative()) dir = run / dir;
                DatasetParserConfig pc;
                pc.require_image_files = false;
                auto ds = std::make_unique<ParsedDataset>(parse_dataset(dir.string(), pc, ""));
                // The run may have trained in a frame of its own; its
                // scene_transform.json says how to get there.
                spirula::Sim3 t;
                if (fs::is_regular_file(run / "scene_transform.json", ec)) {
                    const JsonValue st = json_parse_file((run / "scene_transform.json").string());
                    const JsonValue* tw = st.find("train_from_world");
                    const JsonValue* m = tw ? tw->find("matrix_3x4_flat_row_major") : nullptr;
                    if (m && m->is_array() && m->arr.size() == 12) {
                        double a[12];
                        for (int k = 0; k < 12; k++) a[k] = m->arr[(size_t)k].as_double();
                        t = spirula::Sim3::from_3x4(a);
                    }
                }
                for (int64_t i = 0; i < ds->num_cameras; i++) {
                    float* M = &ds->c2w[(size_t)i * 12];
                    double R[9], p[3] = {M[3], M[7], M[11]}, q[3];
                    for (int r2 = 0; r2 < 3; r2++)
                        for (int c = 0; c < 3; c++) {
                            double v = 0.0;
                            for (int k = 0; k < 3; k++) v += t.R[r2*3+k] * M[k*4+c];
                            R[r2*3+c] = v;
                        }
                    t.apply(p, q);
                    for (int r2 = 0; r2 < 3; r2++) {
                        for (int c = 0; c < 3; c++) M[r2*4+c] = (float)R[r2*3+c];
                        M[r2*4+3] = (float)q[r2];
                    }
                }
                // The parsers' guess at up (DatasetParser.h normalized_rotation),
                // in the model's frame.
                const double up_w[3] = {ds->normalized_rotation[6], ds->normalized_rotation[7],
                                        ds->normalized_rotation[8]};
                t.rotate(up_w, r->up);
                r->ds = std::move(ds);
            } catch (const std::exception&) {
            }
            r->done = true;
        });
        _run_datasets[model_file] = std::move(rd);
        return nullptr;
    }
    RunDataset& r = *it->second;
    if (!r.done.load()) return nullptr;
    if (r.worker.joinable()) r.worker.join();
    return r.ds.get();
}

void CompareView::feed_render() {
    if (_render_index < 0 || _render_index >= count()) return;
    std::vector<int> order{_render_index};
    for (int i = 0; i < count(); i++)
        if (i != _render_index) order.push_back(i);
    std::vector<render::SourceInfo> out;
    for (int i : order) {
        Model& m = *_models[(size_t)i];
        if (!m.attached || !m.src.ready()) continue;
        render::SourceInfo si;
        si.path = m.path;
        si.load_id = m.load_id;
        si.uid = m.uid;
        si.pane = i;
        si.name = display_name(m.src.file().empty() ? m.path : m.src.file());
        render::SourceView& v = si.view;
        v.key = m.src.scene_key();
        spirula::Sim3 file_to_norm;
        switch (m.src.kind()) {
            case SplatViewer::Kind::Points: {
                v.kind = render::SourceView::Points;
                v.ds = &m.src.points();
                v.post = &m.src.post();
                double T[16], A[16];
                for (int k = 0; k < 16; k++) T[k] = v.ds->train_to_normalized[k];
                dsparse::invert_affine4x4(T, A);
                double a[12];
                for (int k = 0; k < 12; k++) a[k] = A[k];
                file_to_norm = spirula::Sim3::from_3x4(a);
                // Up as the pane shows it: the parsers' guess while it levels
                // the cameras, else the file's own +Z, which a measured or
                // hand-placed frame is known to have.
                const bool level = m.panel.level_cameras();
                si.has_up = level || v.ds->gauge_oriented || v.ds->edited_in_place;
                for (int k = 0; k < 3; k++)
                    si.up[k] = level ? v.ds->normalized_rotation[6 + k] : (k == 2 ? 1.0 : 0.0);
                break;
            }
            case SplatViewer::Kind::Mesh:
                v.kind = render::SourceView::Mesh;
                v.mesh = &m.src.mesh();
                si.dataset = run_dataset(m.src.file());
                for (int k = 0; k < 12; k++) v.mesh_t2n[k] = m.src.mesh_to_normalized()[k];
                file_to_norm = spirula::Sim3::from_3x4(m.src.mesh_to_normalized());
                break;
            default: {
                v.kind = render::SourceView::Splats;
                float t[12];
                m.src.to_view_frame(t);
                file_to_norm = spirula::Sim3::from_3x4(t);
                v.cfg = m.src.render_config();
                v.hooks = m.src.make_hooks();
                v.file = m.src.file();
                v.sh_max = m.src.sh_degree();
                si.dataset = run_dataset(v.file);
                // Splats deleted in the editor stay deleted in the render,
                // even while an effect rewrites the others.
                if (_edit_index == i && _edit.active() &&
                    _edit.doc()->kind() == EditDoc::Kind::Splats) {
                    if (!_alive || _alive_rev != _edit.doc()->revision()) {
                        _alive = std::make_shared<const std::vector<uint8_t>>(
                            _edit.doc()->alive_of(0));
                        _alive_rev = _edit.doc()->revision();
                    }
                    v.alive = _alive;
                }
                break;
            }
        }
        v.file_to_norm = file_to_norm;
        float e[12];
        m.panel.edit_transform(e);
        v.norm_to_world = file_to_norm.inverse() * spirula::Sim3::from_3x4(e);
        out.push_back(std::move(si));
    }
    for (render::SourceInfo& si : out) {
        if (si.has_up || !si.dataset) continue;
        const auto it = _run_datasets.find(si.view.file.empty() ? si.path : si.view.file);
        if (it == _run_datasets.end()) continue;
        si.has_up = true;
        for (int k = 0; k < 3; k++) si.up[k] = it->second->up[k];
    }
    if (out.empty()) return;
    float base[12];
    _models[(size_t)_render_index]->panel.base_transform(base);
    _render.set_world_to_shared(spirula::Sim3::from_3x4(base) * out[0].view.file_to_norm);
    _render.set_sources(std::move(out));
}

spirula::Sim3 CompareView::file_to_norm_of(Model& m) {
    switch (m.src.kind()) {
        case SplatViewer::Kind::Points: {
            double T[16], A[16], a[12];
            for (int k = 0; k < 16; k++) T[k] = m.src.points().train_to_normalized[k];
            dsparse::invert_affine4x4(T, A);
            for (int k = 0; k < 12; k++) a[k] = A[k];
            return spirula::Sim3::from_3x4(a);
        }
        case SplatViewer::Kind::Mesh:
            return spirula::Sim3::from_3x4(m.src.mesh_to_normalized());
        default: {
            float t[12];
            m.src.to_view_frame(t);
            return spirula::Sim3::from_3x4(t);
        }
    }
}

// The edited model's placement, in the file coordinates every open model is
// taken to share, shown on the others too while the editor asks for it.
void CompareView::sync_placements() {
    static const float kIdentity[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    const bool want = _edit.active() && _edit.sync_others() && _edit_index >= 0 &&
                      _edit_index < count();
    if (!want) {
        if (_synced)
            for (int i = 0; i < count(); i++)
                if (i != _edit_index) _models[(size_t)i]->panel.set_edit_transform(kIdentity);
        _synced = false;
        return;
    }
    Model& a = *_models[(size_t)_edit_index];
    if (!a.attached) return;
    float e[12];
    a.panel.edit_transform(e);
    const spirula::Sim3 fa = file_to_norm_of(a);
    const spirula::Sim3 placement = fa.inverse() * spirula::Sim3::from_3x4(e) * fa;
    for (int i = 0; i < count(); i++) {
        Model& m = *_models[(size_t)i];
        if (i == _edit_index || !m.attached) continue;
        const spirula::Sim3 f = file_to_norm_of(m);
        float out[12];
        (f * placement * f.inverse()).to_3x4(out);
        m.panel.set_edit_transform(out);
    }
    _synced = true;
}

// Every other mesh pane, filtered by the same face set. They keep their own
// colours: what they share with the edited one is the surface, not the tint.
void CompareView::show_sibling_meshes(int except, const FaceCut& cut) {
    meshing::MeshData tmp;
    for (int i = 0; i < count(); i++) {
        if (i == except) continue;
        Model& m = *_models[(size_t)i];
        if (!m.attached || m.src.kind() != SplatViewer::Kind::Mesh) continue;
        if (cut.empty())
            m.panel.attach_preview_mesh(m.src.mesh(), m.src.mesh_to_normalized(),
                                        m.src.scene_key());
        else {
            mesh_drop_faces(m.src.mesh(), cut, tmp);
            m.panel.attach_preview_mesh(tmp, m.src.mesh_to_normalized(),
                                        m.src.scene_key());
        }
    }
}

void CompareView::finish_edit_load() {
    if (_edit_loading.load() || !_edit_worker.joinable()) return;
    _edit_worker.join();
    if (_edit_pending && _edit_index >= 0 && _edit_index < count())
        _edit.open(std::move(_edit_pending), &_models[_edit_index]->panel);
    else
        _edit_index = -1;
    _edit_pending.reset();
}


// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void CompareView::draw_toolbar() {
    if (_ask_discard) {
        ui::OpenPopup(emsg::discard_title);
        _ask_discard = false;
    }
    ImGui::SetNextWindowSize(ImVec2(px(540.0f), 0.0f), ImGuiCond_Appearing);
    if (ui::BeginPopupModal(emsg::discard_title)) {
        const std::string what =
            _edit.active() ? display_name(_edit.doc()->source_path()) : "";
        ui::TextWrapped(emsg::discard_body, {what});
        auto finish = [this] {
            std::function<void()> then;
            then.swap(_discard_then);
            end_edit();
            ImGui::CloseCurrentPopup();
            if (then) then();
        };
        // One meshing run wrote the same surface several times over; leaving
        // the editor is the last chance to say how far the edit reaches.
        EditDoc* d = _edit.doc();
        const int linked = d ? d->linked_count() : 0;
        if (linked > 0) ui::TextDisabledWrapped(emsg::mesh_link_edits_help);
        ImGui::BeginDisabled(!_edit.can_save_in_place());
        if (linked > 0) {
            if (ui::Button(emsg::save_this_only)) {
                d->set_linked(false);
                _edit.save_in_place();
                finish();
            }
            ImGui::SameLine();
            if (ui::Button(emsg::save_all_files, {(long long)(linked + 1)})) {
                d->set_linked(true);
                _edit.save_in_place();
                finish();
            }
        } else if (ui::Button(emsg::save_over)) {
            _edit.save_in_place();
            finish();
        }
        ImGui::EndDisabled();
        if (linked == 0) ImGui::SameLine();
        ImGui::BeginDisabled(!_edit.can_save_copy());
        if (ui::Button(emsg::save_copy)) {
            // The picker outlives this popup, and the answer comes back to
            // the still-open document; leaving is the user's next move.
            _edit.ask_save_copy();
            _discard_then = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ui::Button(emsg::discard_yes)) finish();
        ImGui::SameLine();
        if (ui::Button(emsg::discard_no)) {
            _discard_then = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::BeginDisabled(full());
    if (ui::Button(msg::compare_add_model)) ImGui::OpenPopup("##addmodel");
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(full() ? msg::compare_full
                                      : msg::compare_add_model_help);
    if (ImGui::BeginPopup("##addmodel")) {
        if (ui::MenuItem(msg::compare_from_file) && _pick_file) _pick_file();
        int shown = 0;
        for (const std::string& r : _recents) {
            if (shown++ >= 10) break;
            if (shown == 1) ImGui::Separator();
            ImGui::PushID(shown);
            if (ui::MenuItemRaw(display_name(r).c_str())) add(r);
            ui::help_on_hover_raw(r.c_str());
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    if (count() > 1) {
        ImGui::SameLine();
        ui::Checkbox(msg::compare_link_views, &_link);
        ui::help_on_hover(msg::compare_link_views_help);
    }
    // One camera, so one set of navigation controls -- and drawing them here
    // rather than in the first pane is what keeps every pane's image the same
    // size, which is the whole point of the layout.
    if (_link && !_models.empty() && _models[0]->attached)
        _models[0]->panel.draw_nav_controls();
}

void CompareView::draw_placement_popup(int index) {
    if (!ImGui::BeginPopup("##place")) return;
    Model& m = *_models[index];
    const float w = px(240.0f);
    if (index > 0) {
        ui::Checkbox(msg::compare_align_first, &m.align);
        ui::help_on_hover(msg::compare_align_first_help);
    }
    ImGui::SetNextItemWidth(w);
    ui::SliderFloat3(msg::compare_position, m.offset, -3.0f, 3.0f, "%.3f");
    ImGui::SetNextItemWidth(w);
    ui::SliderFloat3(msg::compare_rotation, m.euler, -180.0f, 180.0f, "%.1f");
    ImGui::SetNextItemWidth(w);
    ui::SliderFloat(msg::compare_size, &m.scale, 0.1f, 10.0f, "%.3f");
    if (ui::Button(msg::compare_reset_placement)) {
        for (int i = 0; i < 3; i++) m.offset[i] = m.euler[i] = 0.0f;
        m.scale = 1.0f;
        m.align = true;
    }
    ImGui::Separator();
    // Deferred to the next poll(): this popup lives inside the pane it is
    // about to move or delete.
    ImGui::BeginDisabled(index == 0);
    if (ui::Button(msg::compare_move_left)) {
        _pending_move_index = index;
        _pending_move = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= count());
    if (ui::Button(msg::compare_move_right)) {
        _pending_move_index = index;
        _pending_move = 1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(msg::compare_remove)) {
        _pending_remove = index;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void CompareView::draw_pane(int index, const ImVec2& size, const std::function<void()>& beside) {
    Model& m = *_models[index];
    ImGui::PushID(index);
    ImGui::BeginChild("##pane", size, ImGuiChildFlags_Borders);

    if (ui::ButtonRaw("...##place")) ImGui::OpenPopup("##place");
    ui::help_on_hover(msg::compare_placement_help);
    draw_placement_popup(index);
    ImGui::SameLine();
    if (m.title) {
        ui::Text(*m.title);
    } else {
        const std::string f = m.src.file().empty() ? m.src.path() : m.src.file();
        ui::TextRaw(elide_middle(display_name(f),
                                 ImGui::GetContentRegionAvail().x * 0.6f));
        ui::help_on_hover_raw(f.c_str());
    }
    if (m.src.ready()) {
        // The way in and out of editing lives on the pane it edits: with two
        // models open, which one a toolbar button meant would be a guess.
        ImGui::SameLine();
        const bool on = _edit_index == index;
        if (on) {
            const ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            ImGui::PushStyleColor(ImGuiCol_Button, c);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
        }
        if (ui::Button(on ? emsg::leave_edit : emsg::enter_edit)) {
            if (on) confirm_discard_edits([this] { end_edit(); });
            else    begin_edit(index);
        }
        if (on) ImGui::PopStyleColor(2);
        ui::help_on_hover(emsg::enter_edit_help);
        if (_render_allowed) {
            ImGui::SameLine();
            const bool rendering = _render_index == index;
            if (rendering) {
                const ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
                ImGui::PushStyleColor(ImGuiCol_Button, c);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
            }
            ImGui::BeginDisabled(rendering && _render.exporting());
            if (ui::Button(rendering ? rmsg::leave_render : rmsg::enter_render)) {
                if (rendering) end_render();
                else begin_render(index);
            }
            ImGui::EndDisabled();
            if (rendering) ImGui::PopStyleColor(2);
            ui::help_on_hover(rmsg::enter_render_help);
        }
    }
    if (m.src.ready()) {
        ImGui::SameLine();
        if (m.src.kind() == SplatViewer::Kind::Points)
            ui::TextDisabled(msg::viewer_point_count,
                             {(long long)m.src.num_splats()});
        else if (m.src.kind() == SplatViewer::Kind::Mesh)
            ui::TextDisabled(msg::viewer_mesh_count,
                             {(long long)m.src.num_splats(),
                              (long long)m.src.num_faces()});
        else
            ui::TextDisabled(msg::viewer_splat_count,
                             {(long long)m.src.num_splats(),
                              (long long)m.src.sh_degree()});
    }

    // Navigation is the view's, not a pane's: linked, it lives on the toolbar.
    m.panel.show_nav_controls(!_link);
    m.panel.set_controls_pad(std::max(0.0f, _controls_h - m.panel.controls_height()));
    switch (m.src.state()) {
        case SplatViewer::State::Loading:
            ui::TextDisabled(msg::viewer_loading);
            break;
        case SplatViewer::State::Failed:
            ui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), msg::viewer_failed);
            ui::TextDisabledRaw(m.src.error());
            break;
        case SplatViewer::State::Ready:
            if (_render_index == index) _render.draw_status();
            else if (_edit_index == index && _edit.active()) _edit.draw_status();
            else if (_edit_index == index && _edit_loading.load())
                ui::TextDisabled(emsg::edit_preparing);
            // Nothing is training, so nothing changes between frames unless
            // the camera does: the viewport renders on demand.
            m.panel.draw(/*training=*/false);
            if (beside) beside();
            break;
        default:
            ui::TextDisabled(msg::viewer_nothing_open);
            break;
    }

    ImGui::EndChild();
    ImGui::PopID();
}

ViewportPanel* CompareView::link_master() {
    for (auto& m : _models)
        if (m->attached && m->panel.dragging()) return &m->panel;
    for (auto& m : _models)
        if (m->attached && m->panel.moved()) return &m->panel;
    return nullptr;
}

void CompareView::draw(float height) {
    const int n = count();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (height > 0.0f) avail.y = height;
    if (n == 0) {
        ImGui::Dummy(ImVec2(avail.x, avail.y * 0.4f));
        const char* line = msg::viewer_nothing_open.get();
        float tw = ImGui::CalcTextSize(line).x;
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowWidth() - tw) * 0.5f));
        ui::TextDisabledRaw(line);
        return;
    }

    const ImGuiStyle& st = ImGui::GetStyle();
    // Rendering: the primary pane, and the camera's own view where the
    // render panel asks for it. The other models are sources, not panes.
    if (_render_index >= 0 && _render_index < n) {
        using PM = render::RenderSession::PreviewMode;
        const PM mode = _render.preview_mode();
        if (mode == PM::Through) {
            ImGui::BeginChild("##camera", avail, ImGuiChildFlags_Borders);
            _render.draw_preview_pane();
            ImGui::EndChild();
        } else if (mode == PM::Beside) {
            // One pane across the width, its controls over both; under them
            // the view and the camera's picture, a splitter between.
            ViewportPanel& panel = _models[(size_t)_render_index]->panel;
            const float room = std::max(1.0f, avail.x - 2.0f * st.WindowPadding.x - splitter_extent());
            float& share = _render.beside_share();
            float w = std::clamp(room * (1.0f - share), px(160.0f), std::max(px(160.0f), room - px(160.0f)));
            panel.set_image_width(w);
            draw_pane(_render_index, avail, [&] {
                float ix, iy, iw, ih;
                panel.image_rect(ix, iy, iw, ih);
                // Where the splitter's SameLine picks up: the image's right edge.
                ImGui::SetCursorScreenPos(ImVec2(ix + iw, iy));
                ImGui::Dummy(ImVec2(0.0f, ih));
                if (splitter_v("##besidesplit", &w, px(160.0f), room - px(160.0f), ih))
                    share = std::clamp(1.0f - w / room, 0.1f, 0.9f);
                ImGui::BeginChild("##camera", ImVec2(0, ih), ImGuiChildFlags_Borders);
                _render.draw_preview_pane();
                ImGui::EndChild();
            });
        } else {
            _models[(size_t)_render_index]->panel.set_image_width(0.0f);
            draw_pane(_render_index, avail);
        }
        _controls_h = 0.0f;
        return;
    }
    if (n <= 3) {
        const float w = (avail.x - st.ItemSpacing.x * (n - 1)) / n;
        for (int i = 0; i < n; i++) {
            if (i) ImGui::SameLine();
            draw_pane(i, ImVec2(w, avail.y));
        }
    } else {
        const float w = (avail.x - st.ItemSpacing.x) * 0.5f;
        const float h = (avail.y - st.ItemSpacing.y) * 0.5f;
        for (int i = 0; i < n; i++) {
            if (i & 1) ImGui::SameLine();
            draw_pane(i, ImVec2(w, h));
        }
    }

    // What the tallest pane's controls took, for the next frame's padding.
    // A frame late, which nobody sees: it only changes when the window is
    // resized or a model is added.
    _controls_h = 0.0f;
    for (auto& m : _models)
        _controls_h = std::max(_controls_h, m->panel.controls_height());

    // The link, applied after every pane has handled its input.
    if (_link && n > 1) {
        ViewportPanel* master = link_master();
        if (master)
            for (auto& m : _models)
                if (&m->panel != master) m->panel.sync_view_from(*master);
    }
}

}  // namespace gui
