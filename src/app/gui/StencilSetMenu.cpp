// StencilSetMenu.cpp -- see StencilSetMenu.h.

#include "app/gui/StencilSetMenu.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

namespace dmsg = spirula::i18n::msg::dataset;
namespace gmsg = spirula::i18n::msg::gui;

namespace gui {

bool StencilSetMenu::item(const StencilPreset& p, bool can_edit) {
    bool edit = false;
    ImGui::PushID(p.path.c_str());
    if (ImGui::BeginPopupContextItem("##setmenu")) {
        edit = ui::MenuItem(dmsg::stencil_edit, nullptr, false, can_edit);
        if (ui::MenuItem(gmsg::view_rename)) {
            _target = p;
            _name = p.name;
            _name_error.clear();
            _ask_rename = true;
        }
        if (ui::MenuItem(gmsg::preset_delete_button)) {
            _target = p;
            _ask_delete = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return edit;
}

std::optional<StencilSetMenu::Change> StencilSetMenu::draw_dialogs(std::string& message,
                                                                   bool& failed) {
    // Opened from outside the list, these also close the list.
    if (_ask_delete) ui::OpenPopup(dmsg::stencil_delete_title);
    if (_ask_rename) ui::OpenPopup(dmsg::stencil_rename_title);
    _ask_delete = _ask_rename = false;
    std::optional<Change> change = draw_delete(message, failed);
    if (!change) change = draw_rename(message, failed);
    return change;
}

std::optional<StencilSetMenu::Change> StencilSetMenu::draw_delete(std::string& message,
                                                                  bool& failed) {
    if (!ui::BeginPopupModal(dmsg::stencil_delete_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
        return std::nullopt;
    const std::string label = stencil_preset_label(_target);
    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(dmsg::stencil_delete_confirm, {label});
    ui::TextDisabledRaw(_target.path);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    std::optional<Change> change;
    if (ui::Button(gmsg::preset_delete_button, ImVec2(px(150.0f), 0))) {
        std::string err;
        failed = !delete_stencil_preset(_target, err);
        message = spirula::i18n::format(failed ? dmsg::stencil_delete_failed : dmsg::stencil_deleted,
                                        {failed ? err : label});
        if (!failed) change = Change{_target.path, std::nullopt};
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(150.0f), 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return change;
}

std::optional<StencilSetMenu::Change> StencilSetMenu::draw_rename(std::string& message,
                                                                  bool& failed) {
    if (!ui::BeginPopupModal(dmsg::stencil_rename_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
        return std::nullopt;
    ImGui::SetNextItemWidth(px(320.0f));
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool enter = ui::InputTextWithHint(gmsg::preset_name, gmsg::preset_name_hint, &_name,
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemEdited()) _name_error.clear();
    if (!_name_error.empty()) {
        ImGui::PushTextWrapPos(px(460.0f));
        ui::TextColoredRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _name_error);
        ImGui::PopTextWrapPos();
    }
    ImGui::Spacing();

    const bool unchanged = _name == _target.name;
    ImGui::BeginDisabled(_name.empty() || unchanged);
    const bool go = ui::Button(gmsg::view_rename, ImVec2(px(150.0f), 0)) || (enter && !unchanged);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(150.0f), 0)) || (enter && unchanged))
        ImGui::CloseCurrentPopup();

    std::optional<Change> change;
    if (go && !_name.empty()) {
        const std::string was = _target.path;
        std::string err;
        const StencilRename result = rename_stencil_preset(_target, _name, err);
        if (result == StencilRename::Done) {
            message = spirula::i18n::format(dmsg::stencil_renamed, {stencil_preset_label(_target)});
            failed = false;
            change = Change{was, _target};
            ImGui::CloseCurrentPopup();
        } else {
            _name_error = result == StencilRename::NameTaken
                              ? spirula::i18n::format(dmsg::stencil_name_taken, {_name})
                              : spirula::i18n::format(dmsg::stencil_rename_failed, {err});
        }
    }
    ImGui::EndPopup();
    return change;
}

}  // namespace gui
