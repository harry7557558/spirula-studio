// The main menu bar and its language menu.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Ui.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Brand.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>

namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;

namespace gui {

void GuiApp::draw_menu_bar() {
    if (!ImGui::BeginMenuBar()) return;
    if (ui::BeginMenu(msg::menu_file)) {
        if (ui::MenuItem(msg::menu_open_dataset)) {
            open_pick(PickAction::OpenDataset, msg::menu_open_dataset.get(),
                      FileDialog::Mode::Folder);
        }
        if (ui::MenuItem(msg::resume_training, nullptr, false, !native_work_busy() && !training_busy()))
            open_pick(PickAction::ResumeRun, msg::resume_training.get(),
                      FileDialog::Mode::FileOrFolder, {".tar", ".ply"});
        ui::help_on_hover_disabled(msg::resume_training_help);
        if (ui::MenuItem(msg::menu_new_dataset) && !native_work_busy()) {
            close_mesh_preview();
            close_splat();
            _screen = Screen::NewDataset;
        }
        if (ui::MenuItem(msg::menu_open_splat)) {
            open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                      FileDialog::Mode::File, {".ply"});
        }
        if (ui::BeginMenu(msg::menu_open_recent, !_recent.items().empty())) {
            probe_recent();
            RecentItem go;
            bool picked = false;
            int shown = 0;
            for (const RecentItem& it : _recent.items()) {
                if (++shown > 15) break;
                ImGui::PushID(shown);
                const bool blocked = recent_blocked(it);
                if (ui::MenuItemRaw(recent_name(it).c_str(), recent_badge(it).get(), false,
                                    !blocked)) {
                    go = it;
                    picked = true;
                }
                if (blocked) ui::help_on_hover_disabled(msg::home_recon_busy);
                else ui::help_on_hover_raw(it.path.c_str());
                ImGui::PopID();
            }
            ImGui::EndMenu();
            if (picked) open_recent(go);
        }
        ImGui::Separator();
        if (ui::MenuItem(msg::menu_batch) && !native_work_busy()) {
            close_mesh_preview();
            _screen = Screen::Batch;
        }
        ImGui::Separator();
        if (ui::MenuItem(msg::menu_quit)) request_close();
        ImGui::EndMenu();
    }
    if (ui::BeginMenu(msg::menu_view)) {
        if (ui::MenuItem(msg::menu_show_log, nullptr, &_show_log))
            _layout_dirty = true;
        if (ui::MenuItem(msg::menu_show_settings, nullptr, &_show_settings))
            _layout_dirty = true;
        if (ui::MenuItem(msg::menu_reset_layout)) {
            _panel_w = kDefaultPanelW;
            _edit_panel_w = kEditPanelW;
            _log_h = kDefaultLogH;
            _show_log = _show_settings = true;
            _layout_dirty = true;
        }
        ImGui::Separator();
        if (ui::BeginMenu(msg::menu_ui_size)) {
            const float cur = _scale.user();
            if (ui::MenuItem(msg::ui_size_auto, nullptr, cur <= 0.0f)) {
                _scale.set_user(0.0f);
                _layout_dirty = true;
            }
            ImGui::Separator();
            // Percentages, which are the same in every language.
            for (float f : {0.75f, 0.9f, 1.0f, 1.15f, 1.35f, 1.6f, 2.0f}) {
                char label[16];
                std::snprintf(label, sizeof label, "%d%%", (int)(f * 100.0f + 0.5f));
                if (ui::MenuItemRaw(label, std::fabs(cur - f) < 1e-3f)) {
                    _scale.set_user(f);
                    _layout_dirty = true;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        {
            bool native = _dialog.native_enabled();
            ImGui::BeginDisabled(!_dialog.native_available());
            if (ui::MenuItem(msg::menu_native_dialogs, nullptr, &native)) {
                _dialog.use_native(native);
                _layout_dirty = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover_disabled(msg::native_dialogs_help);
        }
        ImGui::EndMenu();
    }
    draw_language_menu();
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
    {
        // Native choice is reachable before previews and engine work.
        if (ui::BeginMenu(msg::primary_gpu)) {
            draw_device_picker(/*as_menu=*/true);
            ImGui::EndMenu();
        }
    }
#endif
    if (ui::BeginMenu(msg::menu_help)) {
        if (ui::BeginMenu(msg::menu_about)) {
            ui::Text(spirula::i18n::msg::brand::product);
            ui::Text(msg::about_version, {SS_VERSION});
            ui::TextDisabled(spirula::i18n::msg::brand::about_line);
            ui::TextDisabledRaw("github.com/harry7557558/spirulae-splat");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

// Made for a user who cannot read the language on screen: an icon (文A) and
// native names, all renderable from the embedded subsets (Fonts.h). Below the
// list, an offer of the full face for text this program did not write.
void GuiApp::draw_language_menu() {
    const i18n::Lang before = i18n::current();
    const std::string title =
        std::string(SS_LANG_MENU_ICON " ") + i18n::native_name(before);
    if (!ui::BeginMenuRaw(ui::detail::label(title, msg::menu_language))) return;

    for (unsigned i = 0; i < i18n::kLangCount; i++) {
        const i18n::Lang l = i18n::Lang(i);
        if (ui::SelectableRaw(i18n::native_name(l), l == before) && l != before) {
            i18n::set_current(l);
            save_settings();
        }
    }

    if (const CjkFace* f = _fonts.optional_face()) {
        ImGui::Separator();
        ImGui::PushTextWrapPos(px(360.0f));
        // The language's own name, not CjkFace::label -- that one is English
        // ("Korean"), which is the one word in this sentence a reader of a
        // Korean UI did not ask for.
        ui::TextDisabled(msg::font_needed,
                         {i18n::native_name(before), human_bytes(f->bytes)});
        ImGui::PopTextWrapPos();
        if (!FontSet::fetch_enabled()) {
            ui::TextDisabled(msg::font_no_fetch);
        } else if (_font_download.state() == FileDownload::State::Running) {
            download_bar(_font_download, msg::font_downloading.get());
        } else {
            if (ui::Button(msg::font_download, ImVec2(px(340.0f), 0))) {
                _font_fetching = f;
                _font_download.start(PendingDownload{f->url, cjk_face_download_path(*f), f->bytes});
            }
            if (_font_download.state() == FileDownload::State::Failed)
                ui::TextColoredWrapped(kErr, msg::font_failed,
                                       {_font_download.status()});
        }
    }
    ImGui::EndMenu();
}

}  // namespace gui
