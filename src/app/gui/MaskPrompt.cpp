// MaskPrompt.cpp -- see MaskPrompt.h.

#include "app/gui/MaskPrompt.h"

#include "app/gui/Layout.h"
#include "app/gui/ModelCache.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"

#include <algorithm>
#include <cctype>

namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

// GuiApp.cpp's kOk and kErr, so the picker reads the same on both screens.
const ImVec4 kPickerOk(0.35f, 0.85f, 0.45f, 1.0f);
const ImVec4 kPickerErr(1.0f, 0.42f, 0.42f, 1.0f);

// The English terms. Chosen for what SAM 3 actually responds to -- a concrete
// countable noun, singular, optionally qualified -- rather than for what reads
// best in a menu.
//
// Nothing here names the black area outside a fisheye circle. It is not an
// object, the model does not find it under any wording, and it is in the same
// place in every frame anyway, which makes it app::FrameMask's job.
const std::vector<MaskSubject>& subjects_impl() {
    static const std::vector<MaskSubject> v = {
        {&dmsg::subj_person,          "person"},
        {&dmsg::subj_hand,            "hand"},
        {&dmsg::subj_shoe,            "shoe"},
        {&dmsg::subj_dog,             "dog"},
        {&dmsg::subj_animal,          "animal"},
        {&dmsg::subj_car,             "car"},
        {&dmsg::subj_bicycle,         "bicycle"},
        {&dmsg::subj_vehicle,         "vehicle"},
        {&dmsg::subj_license_plate,   "license plate"},
        {&dmsg::subj_sky,             "sky"},
        {&dmsg::subj_shadow,          "shadow of person"},
        // {&dmsg::subj_water,           "water"},
        // {&dmsg::subj_reflection,      "reflection"},
        {&dmsg::subj_camera,          "camera"},
        {&dmsg::subj_tripod,          "tripod"},
        {&dmsg::subj_backpack,        "backpack"},
        {&dmsg::subj_helmet,          "helmet"},
        {&dmsg::subj_watermark,       "watermark"},
    };
    return v;
}

const std::vector<MaskSubject>& exceptions_impl() {
    static const std::vector<MaskSubject> v = {
        {&dmsg::subj_person_painting, "person in a painting"},
        {&dmsg::subj_statue,          "statue"},
        {&dmsg::subj_mannequin,       "mannequin"},
    };
    return v;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<std::string> split_terms(const std::string& prompt) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= prompt.size()) {
        const size_t sep = prompt.find(';', start);
        const std::string piece =
            trim(prompt.substr(start, sep == std::string::npos
                                          ? std::string::npos : sep - start));
        if (!piece.empty()) out.push_back(piece);
        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return out;
}

std::string join_terms(const std::vector<std::string>& terms) {
    std::string out;
    for (const std::string& t : terms) {
        if (!out.empty()) out += "; ";
        out += t;
    }
    return out;
}

// One chip. Highlighted when its term is already in the target field, which
// makes the palette double as a readout of what the field says.
bool chip(const MaskSubject& s, std::string& target) {
    const bool on = prompt_has_term(target, s.term);
    if (on) {
        const ImVec4& c = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_Button, c);
    }
    const bool hit = ui::SmallButton(*s.label);
    if (on) ImGui::PopStyleColor();
    if (hit) prompt_toggle_term(target, s.term);
    return hit;
}

// Chips laid out left to right, wrapping at the content edge. ImGui has no
// flow layout, so the wrap is done by hand against the next chip's measured
// width -- SameLine() unconditionally would run off the panel in the
// languages whose words are longest (German, and Portuguese here).
bool chip_row(const std::vector<MaskSubject>& group, std::string& target) {
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    bool edited = false;
    for (size_t i = 0; i < group.size(); i++) {
        if (i) {
            const float last = ImGui::GetItemRectMax().x;
            const float w = ImGui::CalcTextSize(group[i].label->get()).x +
                            ImGui::GetStyle().FramePadding.x * 2.0f;
            if (last + spacing + w < right) ImGui::SameLine();
        }
        edited |= chip(group[i], target);
    }
    return edited;
}

}  // namespace

const std::vector<MaskSubject>& mask_subjects() { return subjects_impl(); }
const std::vector<MaskSubject>& mask_exception_subjects() {
    return exceptions_impl();
}

bool prompt_has_term(const std::string& prompt, const char* term) {
    const std::string want = lower(trim(term));
    for (const std::string& t : split_terms(prompt))
        if (lower(t) == want) return true;
    return false;
}

void prompt_toggle_term(std::string& prompt, const char* term) {
    const std::string want = lower(trim(term));
    std::vector<std::string> terms = split_terms(prompt);
    const size_t before = terms.size();
    terms.erase(std::remove_if(terms.begin(), terms.end(),
                               [&](const std::string& t) {
                                   return lower(t) == want;
                               }),
                terms.end());
    if (terms.size() == before) terms.push_back(trim(term));
    prompt = join_terms(terms);
}

bool draw_subject_palette(std::string& prompt, std::string& negative,
                          bool keep_subject) {
    if (!ui::TreeNode(dmsg::mask_subjects)) return false;
    ui::help_on_hover(dmsg::mask_subjects_help);

    // Each group is headed by the label of the field it writes into. That is
    // the whole explanation of where a chip's word goes, and it stays correct
    // when the keep/remove radio flips both labels.
    bool edited = false;
    ui::TextDisabled(keep_subject ? dmsg::mask_what_to_keep
                                  : dmsg::mask_what_to_remove);
    edited |= chip_row(mask_subjects(), prompt);

    ImGui::Spacing();
    ui::TextDisabled(keep_subject ? dmsg::mask_but_remove : dmsg::mask_but_keep);
    edited |= chip_row(mask_exception_subjects(), negative);

    ImGui::TreePop();
    return edited;
}

bool draw_margin_slider(float& dilate_ratio, float& shrink_ratio, bool keep, float width,
                        bool inline_label) {
    float& ratio = keep ? shrink_ratio : dilate_ratio;
    float margin_pct = ratio * 100.0f;
    const spirula::i18n::Msg& label = keep ? dmsg::mask_dilate_keep : dmsg::mask_dilate_remove;
    bool changed = false;
    if (inline_label) {
        ImGui::SetNextItemWidth(width);
        changed = ui::SliderFloat(label, &margin_pct, 0.0f, 50.0f, "%.0f%%");
    } else {
        ui::Text(label);
        ImGui::SetNextItemWidth(width);
        changed = ui::SliderFloatRaw("##dilate", &margin_pct, 0.0f, 50.0f, "%.0f%%");
    }
    if (changed) ratio = margin_pct / 100.0f;
    ui::help_on_hover(keep ? dmsg::mask_shrink_help : dmsg::mask_dilate_help);
    return changed;
}

// One colour per object, so a dot on the image and a row in the list are
// obviously the same thing. Red is reserved for negative clicks.
unsigned int mask_object_color(int object) {
    static const ImU32 kColors[] = {
        IM_COL32(80, 220, 110, 255),  IM_COL32(90, 170, 245, 255),
        IM_COL32(245, 200, 70, 255),  IM_COL32(200, 130, 245, 255),
        IM_COL32(80, 225, 220, 255),  IM_COL32(245, 150, 90, 255),
    };
    const int n = (int)(sizeof(kColors) / sizeof(kColors[0]));
    return kColors[((object % n) + n) % n];
}

void draw_mask_objects(MaskSettings& settings, long long frame, const std::string& camera,
                       const std::string& source, bool& edited) {
    ui::Text(dmsg::objects_to_click);
    ui::help_on_hover(dmsg::objects_to_click_help);

    for (int o = 0; o < settings.object_count; ++o) {
        ImGui::PushID(o);
        int here = 0, elsewhere = 0;
        for (const MaskClick& c : settings.clicks)
            if (c.source == source && c.object == o)
                (c.frame == frame && c.camera == camera ? here : elsewhere)++;

        const ImU32 col = mask_object_color(o);
        ImGui::ColorButton("##col", ImGui::ColorConvertU32ToFloat4(col),
                           ImGuiColorEditFlags_NoTooltip |
                               ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(12, 12));
        ImGui::SameLine();
        const std::string label =
            (here || elsewhere)
                ? spirula::i18n::format(dmsg::object_with_clicks,
                                        {o + 1, here, elsewhere})
                : spirula::i18n::format(dmsg::object_no_clicks, {o + 1});
        // PushID(o) above already separates the rows, so the label carries no
        // ID of its own.
        if (ui::RadioButtonRaw(label.c_str(), settings.current_object == o))
            settings.current_object = o;
        if (here || elsewhere) {
            ImGui::SameLine();
            if (ui::SmallButton(dmsg::object_clear)) {
                auto& v = settings.clicks;
                v.erase(std::remove_if(v.begin(), v.end(),
                                       [&](const MaskClick& c) {
                                           return c.source == source && c.object == o;
                                       }),
                        v.end());
                edited = true;
            }
        }
        ImGui::PopID();
    }

    if (ui::SmallButton(dmsg::object_another)) {
        settings.current_object = settings.object_count++;
    }
    ui::help_on_hover(dmsg::object_another_help);
    if (settings.object_count > 1) {
        ImGui::SameLine();
        if (ui::SmallButton(dmsg::object_clear_all)) {
            auto& v = settings.clicks;
            v.erase(std::remove_if(v.begin(), v.end(),
                                   [&](const MaskClick& c) { return c.source == source; }),
                    v.end());
            settings.object_count = 1;
            settings.current_object = 0;
            edited = true;
        }
    }
}

void draw_mask_model_picker(std::string& model_id, std::string* detector_id,
                            FileDownload& download,
                            const std::function<void()>& request_download) {
    // "(download)" after whatever is not on disk yet, the blurb on hover.
    auto item = [](const ::spirula::i18n::Msg& label, const ::spirula::i18n::Msg& blurb,
                   bool cached, bool selected) {
        const std::string text =
            cached ? std::string(label.get())
                   : spirula::i18n::format(dmsg::mask_model_needs_download, {label.get()});
        const bool picked = ui::SelectableRaw(text, selected);
        if (ImGui::IsItemHovered()) ui::SetTooltipWrapped(blurb);
        return picked;
    };
    int model_idx = 0;
    const auto& catalog = model_catalog();
    for (size_t i = 0; i < catalog.size(); i++)
        if (model_id == catalog[i].id) model_idx = (int)i;
    ImGui::SetNextItemWidth(px(260.0f));
    if (ui::BeginCombo(dmsg::mask_model, catalog[model_idx].label->get())) {
        for (size_t i = 0; i < catalog.size(); i++) {
            if (!detector_id && catalog[i].kind != MaskModelKind::Sam) continue;
            if (item(*catalog[i].label, *catalog[i].blurb, model_is_cached(catalog[i]),
                     (int)i == model_idx))
                model_id = catalog[i].id;
        }
        ImGui::EndCombo();
    }
    const ModelEntry* entry = find_model(model_id);
    if (entry) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + px(560.0f));
        ui::TextDisabled(*entry->blurb);
        ImGui::PopTextWrapPos();
    }
    const TextDetector* detector = nullptr;
    if (entry && detector_id && takes_detector(*entry)) {
        detector = find_detector(*detector_id);
        if (!detector) {
            detector = &text_detectors().front();
            *detector_id = detector->id;
        }
        ImGui::SetNextItemWidth(px(260.0f));
        if (ui::BeginCombo(dmsg::mask_text_detector, detector->label->get())) {
            for (const TextDetector& d : text_detectors())
                if (item(*d.label, *d.blurb, detector_is_cached(d), &d == detector))
                    *detector_id = d.id;
            ImGui::EndCombo();
        }
        ui::help_on_hover(dmsg::mask_text_detector_help);
        detector = find_detector(*detector_id);
    }
    const bool cached = entry && model_is_cached(*entry) &&
                        (!detector || detector_is_cached(*detector));
    const bool downloading = download.state() == FileDownload::State::Running;
    switch (mask_picker_row(entry != nullptr, cached, downloading)) {
        case PickerRow::GetModel:
            if (ui::Button(dmsg::mask_get_model)) request_download();
            ImGui::SameLine();
            ui::TextDisabled(dmsg::mask_one_time_download);
            break;
        case PickerRow::Downloading:
            // The overlay is a byte count from curl, not a sentence.
            ui::ProgressBarRaw(std::max(download.progress(), 0.0f), ImVec2(260, 0),
                               download.status().c_str());
            ImGui::SameLine();
            if (ui::Button(dmsg::stop)) download.cancel();
            break;
        case PickerRow::Ready:
            ui::TextColored(kPickerOk, dmsg::mask_model_ready);
            break;
        case PickerRow::None:
            break;
    }
    if (download.state() == FileDownload::State::Failed)
        ui::TextColoredWrappedRaw(kPickerErr, download.status());
}

}  // namespace gui
