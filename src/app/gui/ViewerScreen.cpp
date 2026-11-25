// The Viewer screen: a finished model with nothing training -- the same
// viewport, navigation and camera models as during a run, over splats read
// from a file. For somebody else's result, or your own from last week.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Render.h"

#include "imgui.h"

#include <algorithm>

namespace msg = spirula::i18n::msg::gui;
namespace rmsg = spirula::i18n::msg::render;

namespace gui {

void GuiApp::draw_viewer() {
    // Every route off this screen throws the open document away, so each one
    // goes past the same question first.
    if (ui::Button(msg::back_home))
        _compare.confirm_discard_edits([this] { request_go_home(); });
    ImGui::SameLine();
    if (ui::Button(msg::viewer_open_another)) {
        _compare.confirm_discard_edits([this] {
            open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                      FileDialog::Mode::FileOrFolder, kOpenableExtensions);
        });
    }
    ui::help_on_hover(msg::home_open_splat_help);
    // The way into render mode for someone who has not found it on the pane.
    if (_compare.rendering() < 0 && _compare.count() > 0) {
        ImGui::SameLine();
        if (ui::Button(rmsg::train_render)) _compare.begin_render(std::max(0, _compare.editing()));
        ui::help_on_hover(rmsg::enter_render_help);
    }
    ImGui::SameLine();
    draw_viewer_screenshot_controls();
    ImGui::SameLine();
    _compare.set_recents(recent_models(_recent));
    _compare.draw_toolbar();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##viewer", ImVec2(0, body_height(log_h)));
    draw_compare_panes();
    ImGui::EndChild();
    draw_log_panel(log_h);
}

void GuiApp::draw_compare_panes() {
    // Rendering is the viewer screen's; the meshing preview shows a result.
    _compare.set_render_allowed(_screen == Screen::Viewer);
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
