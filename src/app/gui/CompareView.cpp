// CompareView.cpp -- see CompareView.h.

#include "app/gui/CompareView.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "core/Camera.h"
#include "engine/Engine.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;

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
    m->title = title;
    m->slot = claim_slot();
    m->src.open(path, m->slot, &_engine_mutex);
    _models.push_back(std::move(m));
}

void CompareView::remove(int index) {
    if (index < 0 || index >= count()) return;
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
    std::swap(_models[index], _models[to]);
    if (index == 0 || to == 0) _overlay_key.clear();
}

void CompareView::close() {
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
    return out;
}


// ---------------------------------------------------------------------------
// Attach + placement
// ---------------------------------------------------------------------------

void CompareView::attach(Model& m) {
    switch (m.src.kind()) {
        case SplatViewer::Kind::Points:
            m.panel.attach_preview_data(m.src.points(), m.src.post(),
                                        m.src.scene_key());
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
    if (_pending_remove >= 0) {
        remove(_pending_remove);
        _pending_remove = -1;
    }
    for (auto& m : _models)
        if (!m->attached && m->src.ready()) attach(*m);
    ensure_viewer_overlay();
    update_placements();
}


// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void CompareView::draw_toolbar() {
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

void CompareView::draw_pane(int index, const ImVec2& size) {
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
            // Nothing is training, so nothing changes between frames unless
            // the camera does: the viewport renders on demand.
            m.panel.draw(/*training=*/false);
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
