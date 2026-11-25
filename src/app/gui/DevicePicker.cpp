// The one native GPU choice, extra inference GPUs and the device-issue banner.
// The choice is per session: one canonical UUID is frozen before native work and
// passed to every native job and child. No persisted preference.

#include "app/gui/GuiApp.h"

#include "app/DeviceIssue.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "core/Env.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Mesh.h"
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
#include "core/VulkanDeviceSelection.h"
#endif

// Side-effect-free native listing; it must not publish the engine selection.
#if defined(SS_BUILD_SAM)
#include "nn/Device.h"
#include "app/InferenceWorkerDevices.h"
#include "nn/vk/Context.h"
#elif defined(SS_TOOL_SFM)
#include "sfm/vk/VkContext.h"
#endif

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
using spirula::i18n::Msg;

namespace gui {

namespace {

#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
const spirula::i18n::Msg& device_detail(spirula::vkselect::ResolveStatus s) {
    switch (s) {
        case spirula::vkselect::ResolveStatus::Malformed:
            return msg::device_detail_malformed;
        case spirula::vkselect::ResolveStatus::OutOfRange:
            return msg::device_detail_out_of_range;
        case spirula::vkselect::ResolveStatus::Unusable:
            return msg::device_unusable_native;
        case spirula::vkselect::ResolveStatus::NoDevice:
            return msg::device_detail_no_device;
        case spirula::vkselect::ResolveStatus::Missing:
        case spirula::vkselect::ResolveStatus::Ok:
            break;
    }
    return msg::device_detail_missing;
}
#endif

}  // namespace

void GuiApp::load_native_devices() {
    if (_native_devices_loaded) return;
    _native_devices.clear();
#if defined(SS_BUILD_SAM)
    // The inference layer's own side-effect-free enumeration.
    _native_devices = nn::list_devices();
#elif defined(SS_TOOL_SFM)
    // No inference layer, but SfM carries its own Vulkan context: a listing
    // that creates nothing. A build with neither has no native GPU work to
    // choose a device for, and an empty list is the honest answer.
    for (const VkDeviceRecord& d : VkContext::listDevices()) {
        NativeDeviceRow r;
        r.name = d.name;
        r.type = d.type;
        r.uuid = spirula::vkselect::selectorFor(d);
        r.vram_bytes = d.vram_bytes;
        r.usable = d.usable;
        _native_devices.push_back(std::move(r));
    }
#elif defined(SS_BACKEND_VULKAN)
    // Engine-only Vulkan build: the backend's own enumeration, which is also
    // side-effect free. Its identity helpers exist only on this backend, so
    // the whole branch is Vulkan-guarded.
    for (int i = 0; i < backend::device_count(); i++) {
        const backend::DeviceInfo d = backend::device_info(i);
        NativeDeviceRow r;
        r.name = d.name;
        r.type = d.type;
        r.vram_bytes = d.vram_bytes;
        r.usable = d.usable;
        r.uuid = d.uuid;
        _native_devices.push_back(std::move(r));
    }
#endif
    _native_devices_loaded = true;
}

void GuiApp::draw_engine_device_picker(bool busy) {
#ifdef SS_BACKEND_VULKAN
    draw_device_picker();
#else
    ui::SeparatorText(msg::section_device);
    {
        int n_dev = backend::device_count();
        int cur = backend::device_current();
        backend::DeviceInfo curd = backend::device_info(cur);
        ImGui::BeginDisabled(_cuda_device_locked || busy || n_dev == 0);
        ImGui::SetNextItemWidth(px(-8.0f));
        // Device names come from the driver; only the "none found" and
        // "unsupported" notes are ours.
        if (ui::BeginComboRaw("##device",
                              cur >= 0 ? curd.name : msg::no_device_found.get())) {
            for (int i = 0; i < n_dev; i++) {
                backend::DeviceInfo d = backend::device_info(i);
                char label[324];
                std::snprintf(label, sizeof(label), "%s (%s, %llu MB)%s##d%d",
                              d.name, d.type,
                              (unsigned long long)(d.vram_bytes >> 20),
                              d.usable ? "" : msg::device_unsupported.get(), i);
                ImGui::BeginDisabled(!d.usable);
                bool sel = i == cur;
                if (ui::SelectableRaw(label, sel)) backend::device_select(i);
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(spirula::i18n::msg::mesh::help_device_cuda);
        if (_cuda_device_locked)
            ui::TextColoredWrapped(kDim, msg::device_cuda_locked);
    }
#endif
}

void GuiApp::draw_device_picker(bool as_menu) {
    load_native_devices();

    const bool frozen = _native_device_frozen;
    const bool no_devices = std::none_of(
        _native_devices.begin(), _native_devices.end(),
        [](const NativeDeviceRow& d) { return d.usable && !d.uuid.empty(); });
    const bool disabled = frozen || no_devices;

    auto row_label = [](const NativeDeviceRow& d, size_t index, bool with_id,
                         char* buf, size_t n) {
        char suffix[64] = {};
        if (with_id)
            std::snprintf(suffix, sizeof suffix, "##native_device_%zu", index);
        std::snprintf(buf, n, "%s [%zu]%s", d.name.c_str(), index, suffix);
    };

    const bool auto_sel =
        !frozen && _native_device_choice_set && _native_device_request.empty();
    const char* env_sel = spirula::env("VK_DEVICE");
    const bool inherited = !frozen && !_native_device_choice_set &&
                           env_sel && env_sel[0];

    size_t selected = _native_devices.size();
    const std::string* selected_selector =
        frozen ? &_native_device_uuid
               : !_native_device_request.empty() ? &_native_device_request
                                                 : nullptr;
    if (selected_selector && !selected_selector->empty()) {
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            if (_native_devices[i].uuid == *selected_selector) {
                selected = i;
                break;
            }
        }
    } else if (inherited) {
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            if (_native_devices[i].uuid == env_sel) {
                selected = i;
                break;
            }
        }
    }

    char selected_label[400];
    const char* shown = nullptr;
    if (selected < _native_devices.size()) {
        row_label(_native_devices[selected], selected, false, selected_label,
                  sizeof selected_label);
        shown = selected_label;
    } else if (frozen) {
        shown = !_native_device_name.empty() ? _native_device_name.c_str()
                                              : msg::no_device_found.get();
    } else if (selected_selector) {
        shown = msg::no_device_found.get();
    } else {
        shown = msg::device_auto.get();
    }

    if (as_menu) {
        ImGui::BeginDisabled(disabled);
        if (ui::MenuItem(msg::device_auto, nullptr, auto_sel)) {
            _native_device_choice_set = true;
            _native_device_request.clear();
        }
        ui::help_on_hover(msg::device_auto_help);
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            const NativeDeviceRow& d = _native_devices[i];
            char label[400];
            row_label(d, i, true, label, sizeof label);
            const bool sel = !_native_device_request.empty() &&
                             _native_device_request == d.uuid;
            ImGui::BeginDisabled(!d.usable || d.uuid.empty());
            if (ui::MenuItemRaw(label, sel) && !d.uuid.empty()) {
                _native_device_choice_set = true;
                _native_device_request = d.uuid;
            }
            ImGui::EndDisabled();
            ui::help_on_hover_raw(d.uuid.empty() ? "" : d.uuid.c_str());
        }
        ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(disabled);
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::BeginCombo(msg::primary_gpu,
                           no_devices ? msg::no_device_found.get() : shown)) {
            if (ui::Selectable(msg::device_auto, auto_sel)) {
                _native_device_choice_set = true;
                _native_device_request.clear();
            }
            ui::help_on_hover(msg::device_auto_help);
            for (size_t i = 0; i < _native_devices.size(); ++i) {
                const NativeDeviceRow& d = _native_devices[i];
                char label[400];
                row_label(d, i, true, label, sizeof label);
                const bool sel = !_native_device_request.empty() &&
                                 _native_device_request == d.uuid;
                ImGui::BeginDisabled(!d.usable || d.uuid.empty());
                if (ui::SelectableRaw(label, sel) && !d.uuid.empty()) {
                    _native_device_choice_set = true;
                    _native_device_request = d.uuid;
                }
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(msg::device_help);
    }

    if (!_native_device_error.empty())
        ui::TextColoredWrappedRaw(kErr, _native_device_error);
}

bool GuiApp::freeze_native_device() {
#if !defined(SS_BUILD_SAM) && !defined(SS_TOOL_SFM) && !defined(SS_BACKEND_VULKAN)
    _native_device_frozen = true;
    return true;
#else
    if (_native_device_frozen) {
        // Already frozen: the request must name the same device or the app has
        // to be restarted. This is the invariant, not a new winner.
        if (_native_device_request.empty() || _native_device_uuid.empty()) return true;
        if (_native_device_request == _native_device_uuid) return true;
        _native_device_error = spirula::i18n::format(
            msg::device_conflict, {_native_device_request, _native_device_uuid});
        return false;
    }
    load_native_devices();

    const bool explicit_set = _native_device_choice_set;
    const spirula::vkselect::Request req =
        spirula::vkselect::requestFrom(_native_device_request, explicit_set);

    std::string uuid, name;
    const std::string requested = req.text.empty() ? "auto" : req.text;
    std::string diagnostic;
    const Msg* detail = nullptr;
#if defined(SS_BUILD_SAM)
    spirula::vkselect::Resolution res;
    if (req.kind == spirula::vkselect::Request::Kind::Malformed) {
        detail = &msg::device_detail_malformed;
    } else {
        try {
            res = nn::vk::Context::resolveSelector(req);
        } catch (const std::exception&) {
            detail = &msg::device_detail_no_device;
        }
    }
    if (!detail && !res.ok()) {
        detail = &device_detail(res.status);
    } else if (!detail) {
        std::string live = nn::configured_device_selector();
        if (live.empty()) live = nn::current_device_selector();
        if (!live.empty() && live != res.selector) {
            _native_device_error = spirula::i18n::format(
                msg::device_conflict, {res.selector, live});
            log(_native_device_error);
            return false;
        }
#ifdef SS_BACKEND_VULKAN
        if (!backend::device_select_identity(res.selector.c_str())) {
            diagnostic = backend::device_selection_error();
            if (diagnostic.empty()) detail = &msg::device_unusable_native;
        }
#endif
        if (!detail && diagnostic.empty()) {
            try {
                nn::configure_device(res.selector);
            } catch (const std::exception& e) {
                _native_device_error = spirula::i18n::format(
                    msg::device_error, {requested, e.what()});
                log(_native_device_error);
                return false;
            }
            uuid = nn::configured_device_selector();
        }
    }
#elif defined(SS_TOOL_SFM)
    const spirula::vkselect::Resolution res = VkContext::resolveSelector(req);
    if (!res.ok()) detail = &device_detail(res.status);
    else           uuid = res.selector;
#elif defined(SS_BACKEND_VULKAN)
    if (req.kind == spirula::vkselect::Request::Kind::Malformed) {
        detail = &msg::device_detail_malformed;
    } else if (!backend::device_select_identity(requested.c_str())) {
        diagnostic = backend::device_selection_error();
        if (diagnostic.empty()) detail = &msg::device_unusable_native;
    } else {
        uuid = backend::device_current_selector();
    }
#endif
#if defined(SS_BACKEND_VULKAN)
    if (!detail && !uuid.empty() &&
        !backend::device_select_identity(uuid.c_str())) {
        diagnostic = backend::device_selection_error();
        if (diagnostic.empty()) detail = &msg::device_unusable_native;
    }
#endif
    if (!detail && !diagnostic.empty()) {
        _native_device_error = spirula::i18n::format(
            msg::device_error, {requested, diagnostic});
        log(_native_device_error);
        return false;
    }
    if (!detail && uuid.empty())
        detail = &msg::device_detail_no_device;
    if (detail) {
        _native_device_error = spirula::i18n::format(
            msg::device_error, {requested, detail->get()});
        log(_native_device_error);
        return false;
    }

    _native_device_uuid = uuid;
    // The driver's name for the resolved device, for the display line and the
    // startup log. Falls back to the UUID when a listing is unavailable.
    for (const NativeDeviceRow& d : _native_devices)
        if (d.uuid == uuid) { name = d.name; break; }
    _native_device_name = name.empty() ? uuid : name;
    _native_device_frozen = true;
    _native_device_error.clear();
    propagate_frozen_device();
    // The actual resolved identity at workload start, through the log the
    // screen already shows -- not only the argv a child was handed.
    log(spirula::i18n::format(
        msg::device_frozen_at, {_native_device_name + " [" + uuid + "]"}));
    return true;
#endif
}
bool GuiApp::freeze_cuda_device() {
#ifdef SS_BACKEND_VULKAN
    return true;
#else
    if (_cuda_device_locked) return true;
    const int n = backend::device_count();
    const int current = backend::device_current();
    if (n <= 0 || current < 0 || current >= n ||
        !backend::device_select(current)) {
        log(msg::no_device_found.get());
        return false;
    }
    _cuda_device_index = current;
    _cuda_device_locked = true;
    return true;
#endif
}

void GuiApp::propagate_frozen_device() {
    const std::string& uuid = _native_device_uuid;
    // Copy the frozen UUID into every native job and child launch field.
    _sfm_job.prep.device = uuid;
    _colmap_job.device = uuid;
    _sfm_job.device_selector = uuid;
    _sfm_job.geometry.device_uuid = uuid;
    _colmap_job.geometry.device_uuid = uuid;
    _geometry.device_uuid = uuid;
    _mesh_job.device_uuid = uuid;
    const std::string workers = resolved_worker_devices();
    _sfm_job.prep.inference_devices = _colmap_job.inference_devices = workers;
    _sfm_job.geometry.worker_devices = _colmap_job.geometry.worker_devices =
        _geometry.worker_devices = workers;
}

std::string GuiApp::resolved_worker_devices() const {
    std::string csv;
#ifdef SS_BUILD_SAM
    for (const std::string& uuid :
         app::worker_devices_for(_additional_gpus, _native_device_uuid, _native_devices,
                                 app::workers_per_gpu())) {
        if (!csv.empty()) csv += ',';
        csv += uuid;
    }
#endif
    return csv;
}

#ifdef SS_BUILD_SAM
void GuiApp::draw_additional_gpus() {
    load_native_devices();
    std::string primary = _native_device_uuid;
#ifdef SS_BACKEND_VULKAN
    if (primary.empty()) {
        const int index = backend::device_resolve(_native_device_request, _native_device_choice_set);
        if (index >= 0) primary = backend::device_info(index).uuid;
    }
#endif
    auto selected = app::worker_devices_for(_additional_gpus, primary, _native_devices);
    if (!selected.empty()) selected.erase(selected.begin());
    const auto row = [&](size_t i) {
        return _native_devices[i].name + " [" + std::to_string(i) + "]";
    };
    std::string preview;
    for (size_t i = 0; i < _native_devices.size(); ++i)
        if (std::find(selected.begin(), selected.end(), _native_devices[i].uuid) != selected.end())
            preview += (preview.empty() ? "" : ", ") + row(i);
    const bool auto_mode = _additional_gpus == "auto";
    ImGui::SetNextItemWidth(px(kFieldW));
    if (ui::BeginCombo(msg::additional_gpus, auto_mode ? msg::device_auto.get() :
                      preview.empty() ? msg::additional_gpus_none.get() : preview.c_str())) {
        std::string next = _additional_gpus;
        if (ui::Selectable(msg::device_auto, auto_mode)) next = "auto";
        if (ui::Selectable(msg::additional_gpus_none, _additional_gpus == "none")) next = "none";
        ImGui::Separator();
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            const auto& d = _native_devices[i];
            if (!app::worker_device_supported(d) || d.uuid == primary) continue;
            const auto found = std::find(selected.begin(), selected.end(), d.uuid);
            bool on = found != selected.end();
            if (!ui::CheckboxRaw((row(i) + "##" + d.uuid).c_str(), &on)) continue;
            if (on) selected.push_back(d.uuid);
            else selected.erase(found);
            next.clear();
            for (const auto& uuid : selected) next += (next.empty() ? "" : ",") + uuid;
            if (next.empty()) next = "none";
        }
        if (next != _additional_gpus) {
            _additional_gpus = next;
            save_settings();
        }
        ImGui::EndCombo();
    }
    ui::help_on_hover(msg::additional_gpus_help);
}
#endif

// For the GPU the picker names, or the one Auto would take: the same request
// freeze_native_device() resolves when training starts.
void GuiApp::draw_device_issue_banner() {
#ifdef SS_BACKEND_VULKAN
    const bool frozen = _native_device_frozen;
    const int index = backend::device_resolve(
        frozen ? _native_device_uuid : _native_device_request,
        frozen || _native_device_choice_set);
    if (index < 0) return;
    const backend::DeviceInfo d = backend::device_info(index);
    const app::DeviceIssueText t = app::device_issue_text(d.issue);
    if (!t.title) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.58f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.11f, 0.06f, 0.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(12.0f), px(8.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, px(4.0f));
    ImGui::BeginChild("##device_issue", ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(1.15f);
    ui::TextWrapped(*t.title);
    ImGui::SetWindowFontScale(1.0f);
    ui::TextWrapped(*t.body, {d.name});

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.13f, 0.02f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.19f, 0.04f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.08f, 0.01f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.91f, 0.78f, 1.0f));
    if (ui::Button(msg::device_issue_details) && !open_url(t.url)) {
        ImGui::SetClipboardText(t.url);
        log(i18n::format(msg::link_no_browser, {t.url}));
    }
    ui::help_on_hover_raw(t.url);
    ImGui::PopStyleColor(4);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
#endif
}

}  // namespace gui
