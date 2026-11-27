// The row every screen but Home starts with -- the way back, the way home and
// the links to where a dataset's work goes next -- and the two questions
// asked before running work is stopped: leaving its screen, and quitting.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/Ui.h"
#include "data/Json.h"
#include "data/RoiDocument.h"
#include "data/SparseEdit.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/MaskEdit.h"
#include "i18n/catalog/Partition.h"
#include "i18n/catalog/Roi.h"

#include "imgui.h"

#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;
namespace emsg = spirula::i18n::msg::edit;
namespace mmsg = spirula::i18n::msg::maskedit;

namespace gui {

namespace {

bool same_folder(const std::string& a, const std::string& b) {
    std::error_code ec;
    return !a.empty() && !b.empty() && fs::equivalent(fs::u8path(a), fs::u8path(b), ec);
}

}  // namespace

void GuiApp::draw_screen_header(const i18n::Msg* title, const std::function<void()>& links,
                                bool enabled) {
    ImGui::BeginDisabled(!enabled);
    // Home first, where it always is; one button does both when Back leads there.
    const bool deeper = _screen == Screen::Viewer && viewer_mode() > _viewer_base_mode;
    if (back_target() != Screen::Home || deeper) {
        if (ui::Button(msg::screen_home)) request_go_home();
        ui::help_on_hover(msg::nav_home_help);
        ImGui::SameLine();
    }
    if (ui::Button(msg::nav_back, {back_name().get()})) request_back();
    ui::help_on_hover(msg::nav_back_help);
    if (title) {
        ImGui::SameLine(0.0f, px(14.0f));
        ImGui::AlignTextToFramePadding();
        ui::Text(*title);
    }
    if (links) links();
    ImGui::EndDisabled();
}

void GuiApp::draw_dataset_links(const DatasetFolders& f, unsigned links, float reserve,
                                const std::string& model) {
    namespace dense = spirula::i18n::msg::dense;
    namespace part = spirula::i18n::msg::partition;
    namespace roi = spirula::i18n::msg::roi;
    if (f.dir.empty()) links &= kLinkMesh;
    if (model.empty()) links &= ~(unsigned)kLinkMesh;
    if (links == 0) return;
    bool first = true;
    // A link that would run into what the row keeps on its right starts a new line.
    auto next = [&](const i18n::Msg& m) {
        const float w = ImGui::CalcTextSize(m.get()).x + 2.0f * ImGui::GetStyle().FramePadding.x;
        ImGui::SameLine(0.0f, first ? px(18.0f) : -1.0f);
        if (ImGui::GetContentRegionAvail().x - reserve < w) ImGui::NewLine();
        first = false;
    };
    // Nothing here can change, or be left for, while a run uses the dataset
    // or the device: a run is stopped from its own screen first.
    const bool busy = dataset_busy() || native_work_busy();
    ImGui::BeginDisabled(busy);
    if (links & kLinkMesh) {
        next(msg::link_mesh);
        if (ui::Button(msg::link_mesh)) {
            const std::string path = model;
            request_leave(Screen::Mesh, [this, path] {
                set_mesh_source(path);
                go_to(Screen::Mesh);
            });
        }
        ui::help_on_hover_disabled(msg::link_mesh_help);
    }
    if (links & kLinkDataset) {
        next(msg::link_dataset);
        if (ui::Button(msg::link_dataset)) {
            const std::string dir = f.dir;
            request_leave(Screen::NewDataset, [this, dir] { open_dataset_screen(dir); });
        }
        ui::help_on_hover_disabled(msg::link_dataset_help);
    }
    if (links & kLinkTrain) {
        next(dmsg::open_in_trainer);
        // Green where it is the screen's next step. A reconstruction's build
        // log is the first thing to read if training on it goes wrong, so the
        // handoff keeps it.
        const bool from_dataset = _screen == Screen::NewDataset;
        if (from_dataset ? ui::GoButton(dmsg::open_in_trainer) : ui::Button(dmsg::open_in_trainer))
            request_open_dataset(f, /*keep_log=*/from_dataset);
        ui::help_on_hover_disabled(msg::link_train_help);
    }
    if (links & kLinkEdit) {
        next(emsg::sparse_edit);
        if (ui::Button(emsg::sparse_edit)) {
            _edit_after_open = true;
            _sparse_edit_src = f;
            request_open_splat(f.dir);
        }
        ui::help_on_hover_disabled(emsg::sparse_edit_help);
    }
    if (links & kLinkDense) {
        next(dense::edit_cloud);
        if (ui::Button(dense::edit_cloud)) edit_dense_cloud(f);
        ui::help_on_hover_disabled(dense::edit_cloud_help);
    }
    if (links & kLinkMasks) {
        next(mmsg::correct_masks);
        if (ui::Button(mmsg::correct_masks)) {
            // Folders as a run's config spells them: relative to the dataset,
            // empty for the parser's default.
            const TrainConfig stock;
            auto under = [&](const std::string& d, const std::string& fallback) {
                return (fs::path(f.dir) / (d.empty() ? fallback : d)).lexically_normal().string();
            };
            open_mask_editor(f.dir, under(f.image_dir, stock.image_dir),
                             under(f.mask_dir, stock.mask_dir), f.mask_flipped);
        }
        ui::help_on_hover_disabled(mmsg::correct_masks_help);
    }
    if (links & kLinkRegion) {
        next(roi::editor_button);
        if (ui::Button(roi::editor_button)) {
            const spirula::RoiChoice now = spirula::resolve_roi_setting(_cfg.roi_region, f.dir);
            open_roi_editor(f.dir, same_folder(f.dir, _cfg.data) && !now.off ? now.path : std::string());
        }
        ui::help_on_hover_disabled(roi::editor_button_help);
    }
    if (links & kLinkPartition) {
        next(part::open_button);
        if (ui::Button(part::open_button)) open_partition_panel(f);
        ui::help_on_hover_disabled(part::open_button_help);
    }
    ImGui::EndDisabled();
}

const GuiApp::DatasetProbe& GuiApp::train_dataset_probe() {
    const std::string key = _cfg.data + '\n' + _cfg.mask_dir;
    const double now = ImGui::GetTime();
    if (key == _train_probe.key && now - _train_probe.at < 1.0) return _train_probe;
    _train_probe.key = key;
    _train_probe.at = now;
    std::error_code ec;
    fs::path md = fs::u8path(_cfg.mask_dir);
    if (md.is_relative()) md = fs::u8path(_cfg.data) / md;
    _train_probe.masks = !_cfg.data.empty() && fs::is_directory(md, ec) && !fs::is_empty(md, ec);
    _train_probe.sparse = !_cfg.data.empty() && !spirula::resolve_sparse_dir(_cfg.data).empty();
    return _train_probe;
}

// A reconstruction opened as points names its folder; a trained model or a
// mesh names the dataset in its run's config.json, beside it or one up.
const GuiApp::ViewerDataset& GuiApp::viewer_dataset() {
    const CompareView::PaneSource p = _compare.pane_source(_compare.primary_pane());
    const std::string key = p.ready ? p.asked + '\n' + p.file + '\n' + p.dataset : std::string();
    if (key == _viewer_ds.asked) return _viewer_ds;
    _viewer_ds = {};
    _viewer_ds.asked = key;
    if (!p.ready) return _viewer_ds;
    if (!p.dataset.empty()) {
        _viewer_ds.sparse = true;
        _viewer_ds.f = same_folder(p.dataset, _sparse_edit_src.dir) ? _sparse_edit_src
                                                                    : DatasetFolders{p.dataset};
        return _viewer_ds;
    }
    std::error_code ec;
    fs::path run;
    const fs::path asked = fs::u8path(p.asked), file = fs::u8path(p.file);
    for (const fs::path& d : {asked, file.parent_path(), file.parent_path().parent_path()})
        if (!d.empty() && fs::is_regular_file(d / "config.json", ec)) { run = d; break; }
    if (run.empty()) return _viewer_ds;
    try {
        const JsonValue cfg = json_parse_file((run / "config.json").string());
        auto text = [&](const char* k) {
            const JsonValue* v = cfg.find(k);
            return v ? v->as_string() : std::string();
        };
        fs::path data = fs::u8path(text("data"));
        if (data.empty()) return _viewer_ds;
        if (data.is_relative()) data = run / data;
        if (!fs::is_directory(data, ec)) return _viewer_ds;
        DatasetFolders& f = _viewer_ds.f;
        f.dir = data.lexically_normal().u8string();
        f.image_dir = text("image_dir");
        f.mask_dir = text("mask_dir");
        const JsonValue* flip = cfg.find("flip_mask");
        f.mask_flipped = flip && flip->as_bool();
    } catch (const std::exception&) {
        _viewer_ds.f = {};
    }
    return _viewer_ds;
}

void GuiApp::draw_viewer_links(float reserve) {
    const ViewerDataset& d = viewer_dataset();
    unsigned links = kLinkDataset | kLinkTrain;
    if (d.sparse) links |= kLinkRegion | kLinkPartition;
    // Splats can be meshed; a cloud or a mesh cannot.
    const CompareView::PaneSource p = _compare.pane_source(_compare.primary_pane());
    if (p.ready && p.splats) links |= kLinkMesh;
    draw_dataset_links(d.f, links, reserve, p.asked);
}

void GuiApp::draw_leave_modal() {
    struct Text { const i18n::Msg* title; const i18n::Msg* body; };
    auto text = [](Work w) -> Text {
        switch (w) {
            case Work::Dataset: return {&msg::leave_dataset_title, &msg::leave_cancels_run};
            case Work::Meshing: return {&msg::leave_mesh_title, &msg::leave_cancels_run};
            case Work::Render:  return {&msg::leave_render_title, &msg::leave_render_body};
            case Work::Batch:   return {&msg::leave_batch_title, &msg::leave_batch_body};
            case Work::None:    break;
        }
        return {nullptr, nullptr};
    };
    const Text t = text(_leave_ask.work);
    if (!t.title) return;
    if (_leave_ask.open) {
        ui::OpenPopup(*t.title);
        _leave_ask.open = false;
        _leave_ask.shown = true;
    }
    if (!_leave_ask.shown) return;
    ImGui::SetNextWindowSize(ImVec2(px(kModalWrapW), 0.0f), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(*t.title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        _leave_ask = {};   // dismissed: stay
        forget_open_intents();
        return;
    }
    ui::TextWrapped(*t.body);
    ImGui::Spacing();
    if (ui::Button(msg::leave_stop, ImVec2(px(kModalButtonW), 0.0f))) {
        stop_work(_leave_ask.work);
        _pending = Pending::Leave;
        _pending_then = std::move(_leave_ask.go);
        _stop_confirmed = true;
        _leave_ask = {};
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(msg::leave_stay, ImVec2(px(kModalButtonW), 0.0f))) {
        _leave_ask = {};
        forget_open_intents();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::draw_quit_modal() {
    if (_quit_ask) {
        ui::OpenPopup(msg::quit_work_title);
        _quit_ask = false;
        _quit_shown = true;
    }
    if (!_quit_shown) return;
    ImGui::SetNextWindowSize(ImVec2(px(kModalWrapW), 0.0f), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(msg::quit_work_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        _quit_shown = false;
        return;
    }
    ui::TextWrapped(msg::quit_work_body);
    for (const i18n::Msg* w : running_work()) {
        ImGui::Bullet();
        ui::Text(*w);
    }
    ImGui::Spacing();
    if (ui::Button(msg::quit_anyway, ImVec2(px(kModalButtonW), 0.0f))) {
        _quit = true;
        _quit_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(msg::keep_working, ImVec2(px(kModalButtonW), 0.0f))) {
        _quit_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// Alt+Left and the mouse's back button, as a browser has them. Not while a
// field, a popup or a tool window has the user's attention.
void GuiApp::handle_back_input() {
    if (_screen == Screen::Home) return;
    if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() || _dialog.is_open()) return;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    if (_mask_editor.is_open() || _roi_editor.is_open() || _partition_panel.is_open() ||
        _segment.is_open() || _geometry_panel.is_open() || _matrix_editor.is_open())
        return;
    // A routed shortcut claims Alt, so releasing it does not then also move
    // the keyboard focus to the menu bar, as a lone Alt does.
    const bool key = ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_LeftArrow, ImGuiInputFlags_RouteGlobal);
    if (key || ImGui::IsMouseClicked(ImGuiMouseButton(3))) request_back();
}

}  // namespace gui
