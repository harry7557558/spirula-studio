// The Viewer screen: a finished model with nothing training -- the same
// viewport, navigation and camera models as during a run, over splats read
// from a file. For somebody else's result, your own from last week, or the
// mesh a meshing run just made beside the splats it came from.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Render.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace rmsg = spirula::i18n::msg::render;

namespace gui {

void GuiApp::draw_viewer() {
    _compare.set_recents(recent_models(_recent));
    const ImGuiStyle& st = ImGui::GetStyle();
    const float shot_w = ImGui::CalcTextSize(msg::screenshot.get()).x +
                         ImGui::CalcTextSize("...").x + 4.0f * st.FramePadding.x +
                         st.ItemSpacing.x;
    draw_screen_header(nullptr, [&] {
        ImGui::SameLine(0.0f, px(14.0f));
        // Every route off this screen throws the open document away, so each
        // one goes past the same question first.
        if (ui::Button(msg::viewer_open_another)) {
            _compare.confirm_discard_edits([this] {
                open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                          FileDialog::Mode::FileOrFolder, kOpenableExtensions);
            });
        }
        ui::help_on_hover(msg::home_open_splat_help);
        ImGui::SameLine();
        _compare.draw_toolbar();
        // The way into render mode for someone who has not found it on the pane.
        if (_compare.rendering() < 0 && _compare.count() > 0) {
            ImGui::SameLine();
            if (ui::Button(rmsg::train_render)) _compare.begin_render(std::max(0, _compare.editing()));
            ui::help_on_hover(rmsg::enter_render_help);
        }
        draw_viewer_links(shot_w + st.ItemSpacing.x);
    });
    ImGui::SameLine();
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > shot_w) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - shot_w);
    draw_viewer_screenshot_controls();
    if (_mesh_preview_open) draw_mesh_result();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##viewer", ImVec2(0, body_height(log_h)));
    draw_compare_panes();
    ImGui::EndChild();
    draw_log_panel(log_h);
}

// One run writes several formats of one surface, and four panes of the same
// mesh is not a comparison: which of them is on screen is the user's call.
void GuiApp::draw_mesh_result() {
    if (_mesh.num_verts() > 0) {
        ui::Text(msg::mesh_done, {(long long)_mesh.num_verts(), (long long)_mesh.num_faces()});
        ImGui::SameLine();
    }
    ui::TextDisabledRaw(elide_middle(_mesh.output_path(), ImGui::GetContentRegionAvail().x));
    ImGui::AlignTextToFramePadding();
    ui::TextDisabled(emsg::mesh_shown);
    if (!training_busy() && !_mesh_job.checkpoint.empty()) {
        ImGui::SameLine();
        bool on = _compare.index_of(_mesh_job.checkpoint) >= 0;
        if (ui::Checkbox(emsg::mesh_side_splats_pane, &on))
            _compare.set_shown(_mesh_job.checkpoint, on, &msg::mesh_side_splats);
    }
    for (const std::string& out : _mesh.output_paths()) {
        ImGui::SameLine();
        ImGui::PushID(out.c_str());
        bool on = _compare.index_of(out) >= 0;
        if (ui::CheckboxRaw(fs::path(out).filename().string().c_str(), &on))
            _compare.set_shown(out, on, &msg::mesh_side_mesh);
        ImGui::PopID();
    }
}

void GuiApp::draw_compare_panes() {
    _compare.render().set_ffmpeg(_ffmpeg_exe);
    const CompareView::Sidebar side = _compare.sidebar();
    if (side != CompareView::Sidebar::None) {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float h = ImGui::GetContentRegionAvail().y;
        const float w = std::clamp(_edit_panel_w * ui_scale(), px(220.0f),
                                   std::max(px(220.0f), avail * 0.6f));
        ImGui::BeginChild("##editpanel", ImVec2(w, 0), ImGuiChildFlags_Borders);
        if (side == CompareView::Sidebar::Render) _compare.render().draw_panel();
        else _compare.edit().draw_panel();
        ImGui::EndChild();
        float dragged = w;
        if (splitter_v("##editpanelsplit", &dragged, px(220.0f), avail * 0.7f, h)) {
            _edit_panel_w = dragged / ui_scale();
            _layout_dirty = true;
        }
        ImGui::BeginChild("##editpanes", ImVec2(0, 0));
        if (side == CompareView::Sidebar::Render) {
            // The timeline runs under the panes, the width of all of them.
            const float tl = _compare.render().timeline_height();
            _compare.draw(std::max(px(120.0f), ImGui::GetContentRegionAvail().y - tl));
            _compare.render().draw_timeline();
        } else {
            _compare.draw(0.0f);
        }
        ImGui::EndChild();
        return;
    }
    _compare.draw(0.0f);
}

}  // namespace gui
