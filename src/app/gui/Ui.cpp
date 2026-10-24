// Ui.cpp -- see Ui.h.

#include "app/gui/Ui.h"

#include "imgui_internal.h"

namespace ui {
namespace detail {

bool locked_click() {
    return (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) &&
           ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

void view_locked_combo(ImGuiID id) {
    if (ImGui::GetItemID() == id && locked_click())
        ImGui::OpenPopupEx(ImHashStr("##ComboPopup", 0, id));
}

bool view_locked_node(bool open) {
    if (locked_click()) ImGui::TreeNodeSetOpen(ImGui::GetItemID(), !open);
    return open;
}

}  // namespace detail
}  // namespace ui
