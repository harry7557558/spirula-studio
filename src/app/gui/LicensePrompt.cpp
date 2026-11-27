// Licence consent for gated model downloads: the queue and the modal. Shown
// once per model family, before the first download: a plain-language summary,
// then the licence in full, which the tick below is acceptance of.

#include "app/gui/GuiApp.h"

#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "core/LicenseConsent.h"
#include "i18n/catalog/Dataset.h"
#include "roma/model/Fetch.h"

#include "imgui.h"

#include <algorithm>

namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
using spirula::i18n::Msg;

namespace gui {

// gui.conf is read once per frame, not per question: a screen may ask every frame.
bool GuiApp::license_accepted(const std::string& family) const {
    const int frame = ImGui::GetCurrentContext() ? ImGui::GetFrameCount() : -1;
    if (frame < 0 || frame != _accepted_frame) {
        _accepted_cache = spirula::license::accepted_all();
        _accepted_frame = frame;
    }
    return std::find(_accepted_cache.begin(), _accepted_cache.end(), family) != _accepted_cache.end();
}

void GuiApp::request_licenses(std::vector<std::string> families,
                              std::function<void()> then,
                              std::function<void()> declined) {
    if (!_license_prompt.empty()) return;
    _license_queue = std::move(families);
    _license_then = std::move(then);
    _license_declined = std::move(declined);
    advance_license_queue();
}

void GuiApp::advance_license_queue() {
    for (const std::string& f : _license_queue)
        if (!license_accepted(f)) {
            _license_prompt = f;
            _license_model_id.clear();
            _license_detector_id.clear();
            _license_tick = false;
            return;
        }
    _license_queue.clear();
    // Taken out first: `then` may call request_licenses and set a new one.
    std::function<void()> then = std::move(_license_then);
    _license_then = nullptr;
    _license_declined = nullptr;
    if (then) then();
}

void GuiApp::note_license_declined(const Msg& what) {
    _license_notice = what.get();
    log(_license_notice);
}

void GuiApp::start_downloads_with_consent(DownloadQueue& queue,
                                          std::vector<PendingDownload> files) {
    std::vector<std::string> lists;
    for (const PendingDownload& f : files) lists.push_back(f.license_family);
    _license_notice.clear();
    request_licenses(
        spirula::license::unique_families(lists),
        [&queue, files = std::move(files)]() mutable { queue.start(std::move(files)); },
        [this] { note_license_declined(dmsg::license_declined_download); });
}

void GuiApp::draw_license_modal() {
    if (_license_prompt.empty()) return;
    const LicenseInfo* family = license_for(_license_prompt);
    if (!family) {
        // A family nothing gave wording to cannot be accepted here; ask no more.
        _license_prompt.clear();
        _license_queue.clear();
        return;
    }
    const LicenseInfo& li = *family;

    ui::OpenPopup(dmsg::license_modal_title);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(px(720.0f), 0), ImGuiCond_Always);
    if (!ui::BeginPopupModal(dmsg::license_modal_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::SetWindowFontScale(1.15f);
    ui::Text(*li.title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ui::Text(*li.summary);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // The terms in full, not a summary of them: the tick below is acceptance of
    // THIS text, for every family alike. Embedded, so it is on screen with no network.
    ui::TextDisabled(dmsg::license_full_text);
    // ImGui keeps a child's scroll by id, and every family shares this one.
    if (ImGui::IsWindowAppearing()) ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##license_text", ImVec2(0, px(260.0f)), ImGuiChildFlags_Borders);
    ui::TextWrappedRaw(std::string(li.full_text));
    ImGui::EndChild();
    ImGui::Spacing();

    // The link is a button, not decoration: the tick below says the user has
    // read the terms, so getting to them has to be one obvious click. Copying
    // the address is the fallback for a session with no browser to launch.
    if (ui::Button(dmsg::license_read, ImVec2(px(180.0f), 0))) {
        if (!open_url(li.url)) {
            ImGui::SetClipboardText(li.url);
            log(i18n::format(dmsg::license_no_browser, {li.url}));
        }
    }
    ui::help_on_hover_raw(li.url);
    ImGui::SameLine();
    if (ui::Button(dmsg::license_copy_link, ImVec2(px(110.0f), 0)))
        ImGui::SetClipboardText(li.url);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ui::TextDisabledRaw(li.url);
    ImGui::PopTextWrapPos();
    if (_license_prompt == "roma") {
        if (ui::ButtonRaw("RoMa v2 (MIT)")) {
            if (!open_url(spirula::roma::kRomaTerms)) ImGui::SetClipboardText(spirula::roma::kRomaTerms);
        }
        ui::help_on_hover_raw(spirula::roma::kRomaTerms);
    }
    if (const ModelEntry* e = find_model(_license_model_id))
        ui::TextDisabled(dmsg::license_download_size,
                         {human_bytes(missing_download_bytes(
                             *e, detector_for(*e, _license_detector_id)))});
    ImGui::Spacing();

    // Every family, none exempt: Accept stays off until the user says they have read it.
    ui::Checkbox(dmsg::license_accept_tick, {li.title->get()}, &_license_tick);
    ImGui::Spacing();

    ImGui::BeginDisabled(!spirula::license::accept_enabled(_license_prompt, _license_tick));
    const bool downloads = find_model(_license_model_id) != nullptr;
    if (ui::Button(downloads ? dmsg::license_download : dmsg::license_accept,
                   ImVec2(px(kModalButtonW), 0))) {
        if (!accept_license(_license_prompt))
            log(i18n::format(dmsg::license_not_saved, {spirula::license::settings_path()}));
        _accepted_frame = -1;
        _license_prompt.clear();
        ImGui::CloseCurrentPopup();
        if (downloads)
            // Raises the detector's modal next if that one is still to agree to.
            request_model_download(_license_model_id, _license_detector_id);
        else
            advance_license_queue();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(kModalButtonW), 0))) {
        const bool model_flow = find_model(_license_model_id) != nullptr;
        _license_prompt.clear();
        _license_queue.clear();
        _license_then = nullptr;
        if (std::function<void()> declined = std::exchange(_license_declined, nullptr))
            declined();
        else if (model_flow)
            note_license_declined(dmsg::license_declined_download);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace gui
