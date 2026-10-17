// SfmOptionsUI.cpp -- see SfmOptionsUI.h.

#include "app/gui/SfmOptionsUI.h"

#include "app/gui/Layout.h"
#include "app/gui/SfmRunner.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"

#ifdef SS_TOOL_SFM
#include "i18n/catalog/SfmFields.h"
#include "sfm/SfmConfig.h"
#endif

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gui {

namespace {

namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;

const ImVec4 kModifiedColor(1.0f, 0.72f, 0.25f, 1.0f);

// The frontend's own budget flag: the counts are not comparable across them.
const char* feature_budget_flag(int features) {
    return features == 0 ? "max-features" : features >= 3 ? "loma-max-features" : "aliked-max-features";
}

}  // namespace

bool sfm_option_panel_owned(const std::string& flag) {
    static const char* const kOwned[] = {
        "quality", "data-type", "pairs", "overlap", "loop-closure", "prefilter-sequential",
        "masks", "flip-mask", "feature-masks", "max-image-size", "camera-mode", "camera-model",
        "focal", "distortion", "features", "max-features", "aliked-max-features",
        "loma-max-features", "matcher", "mapper", "refine-extra-params", "final-extra-params",
        "final-per-image-intrinsics", "final-free-rig", "metric-gps", "exif-attitude",
        "sensor-gauge", "ba-real", "ba-real-coarse", "metric-positions", "telemetry", "rigs",
        "manifest", "resume", "aliked-model", "lightglue-model", "loma-detector-model",
        "loma-descriptor-model", "loma-matcher-model", "progressive", "progressive-error-start",
        "progressive-error-end", "progressive-error-steps", "progressive-features",
        "progressive-max-features-end", "progressive-image-size-end", "progressive-feature-steps",
        "progressive-patience", "progressive-time",
    };
    for (const char* o : kOwned)
        if (flag == o) return true;
    return false;
}

#ifdef SS_TOOL_SFM

namespace {

namespace F = spirula::i18n::msg::sfmfield;

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// The groups an editor offers; colour, input and runtime are the panel's or
// the machine's, not the reconstruction's.
const spirula::i18n::Msg* group_label(const char* group) {
    struct Row { const char* key; const spirula::i18n::Msg* msg; };
    static const Row kRows[] = {
        {"pipeline", &F::group_pipeline}, {"camera", &F::group_camera},
        {"features", &F::group_features}, {"matching", &F::group_matching},
        {"mapper", &F::group_mapper},     {"rig", &F::group_rig},
        {"manage", &F::group_manage},     {"merge", &F::group_merge},
    };
    for (const Row& r : kRows)
        if (std::strcmp(r.key, group) == 0) return r.msg;
    return nullptr;
}

std::string number_text(double v, bool integer) {
    if (integer) return std::to_string((long long)std::llround(v));
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", v);
    return buf;
}

void row_tooltip(const char* name, const char* help, const std::string& def) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled) ||
        !ImGui::BeginTooltip())
        return;
    ImGui::PushTextWrapPos(px(420.0f));
    ui::TextColoredRaw(kModifiedColor, "--" + std::string(name));
    ui::TextRaw(help);
    ImGui::Separator();
    ui::Text(msg::cfg_preset_default, {def});
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void row_label(const char* name, bool modified) {
    ImGui::SameLine();
    if (modified) ui::TextColoredRaw(kModifiedColor, std::string(name) + " *");
    else ui::TextRaw(name);
}

// One table row over job.options; returns true when it changed.
bool option_row(const sfm::FieldView& f, SfmJob& job) {
    const auto it = job.options.find(f.name);
    const bool modified = it != job.options.end();
    const std::string cur = modified ? it->second : f.value;
    std::string next = cur;
    bool changed = false;

    ImGui::PushID(f.name);
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(px(140.0f));
    switch (f.kind) {
        case sfm::FieldView::Kind::Switch: {
            bool on = cur == "on";
            if (ui::CheckboxRaw("##v", &on)) { next = on ? "on" : "off"; changed = true; }
            break;
        }
        case sfm::FieldView::Kind::Integer:
        case sfm::FieldView::Kind::Real: {
            const bool integer = f.kind == sfm::FieldView::Kind::Integer;
            double v = std::atof(cur.c_str());
            if (ui::InputDoubleRaw("##v", &v, 0, 0, integer ? "%.0f" : "%g")) {
                if (f.lo < f.hi) v = std::clamp(v, f.lo, f.hi);
                next = number_text(v, integer);
                changed = true;
            }
            break;
        }
        case sfm::FieldView::Kind::Text:
            if (f.choices && *f.choices) {
                if (ui::BeginComboRaw("##v", cur.c_str())) {
                    std::string all = f.choices;
                    for (size_t pos = 0; pos <= all.size();) {
                        const size_t bar = std::min(all.find('|', pos), all.size());
                        const std::string choice = all.substr(pos, bar - pos);
                        if (ui::SelectableRaw(choice, choice == cur)) { next = choice; changed = true; }
                        pos = bar + 1;
                    }
                    ImGui::EndCombo();
                }
            } else if (ui::InputTextRaw("##v", &next)) {
                changed = true;
            }
            break;
    }
    row_label(f.name, modified);
    ImGui::EndGroup();
    row_tooltip(f.name, f.help, f.value);
    ImGui::OpenPopupOnItemClick("ctx", ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::BeginPopup("ctx")) {
        if (ui::MenuItem(msg::cfg_reset_to, {f.value})) { next = f.value; changed = true; }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (!changed) return false;
    if (next == f.value) job.options.erase(f.name);
    else job.options[f.name] = next;
    return true;
}

// max-image-size and the feature budget live in SfmJob's own fields, where 0
// means the quality level's.
bool budget_row(const char* name, const char* help, int& field, int def) {
    ImGui::PushID(name);
    ImGui::BeginGroup();
    int v = field > 0 ? field : def;
    ImGui::SetNextItemWidth(px(140.0f));
    bool changed = false;
    if (ui::InputIntRaw("##v", &v)) {
        field = v > 0 && v != def ? v : 0;
        changed = true;
    }
    row_label(name, field > 0);
    ImGui::EndGroup();
    row_tooltip(name, help, std::to_string(def));
    ImGui::OpenPopupOnItemClick("ctx", ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::BeginPopup("ctx")) {
        if (ui::MenuItem(msg::cfg_reset_to, {std::to_string(def)})) { field = 0; changed = true; }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

}  // namespace

bool draw_sfm_options_editor(SfmJob& job, SfmOptionsState& st) {
    // What the run gets with nothing edited: the CLI's own presets, applied
    // to the panel's choices, so a default shown here is never a guess.
    sfm::SfmConfig preset;
    preset.quality = sfm_pick(kSfmQuality, job.quality, 2);
    preset.data_type = sfm_pick(kSfmDataType, job.data_type);
    preset.features = sfm_pick(kSfmFeatures, job.features);
    preset.matcher = sfm_matcher_for(job.features, job.matcher);
    std::vector<sfm::PresetChange> moved;
    sfm::applyPresets(preset, {}, moved);
    const std::vector<sfm::FieldView> rows = sfm::describeConfigFields(preset, sfm::CMD_AUTO);
    auto row_of = [&](const std::string& name) -> const sfm::FieldView* {
        for (const sfm::FieldView& f : rows)
            if (name == f.name) return &f;
        return nullptr;
    };

    // The quality level's own rows: the two budgets, the pair-selection
    // breadth it scales, and whatever else it moved for this frontend.
    std::vector<std::string> quality_rows = {"prefilter-neighbors"};
    for (const sfm::PresetChange& c : moved)
        if (!sfm_option_panel_owned(c.flag) &&
            std::find(quality_rows.begin(), quality_rows.end(), c.flag) == quality_rows.end())
            quality_rows.push_back(c.flag);

    bool changed = false;
    ui::TextColoredWrapped(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), dmsg::sfm_options_intro);

    ImGui::SetNextItemWidth(px(-150.0f));
    char search[128];
    std::snprintf(search, sizeof search, "%s", st.search.c_str());
    if (ui::InputTextHintBufRaw("##sfmoptsearch", msg::cfg_search_hint, search, sizeof search))
        st.search = search;
    ImGui::SameLine();
    ui::Checkbox(msg::cfg_edited_only, &st.modified_only);
    ui::help_on_hover(msg::cfg_edited_only_help);

    const bool edited = !job.options.empty() || job.max_features > 0 || job.max_image_size > 0;
    ImGui::BeginDisabled(!edited);
    if (ui::Button(dmsg::sfm_options_reset_all)) {
        job.options.clear();
        job.max_features = job.max_image_size = 0;
        changed = true;
    }
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::sfm_options_reset_all_help);

    const std::string query = lower(st.search);
    const bool searching = !query.empty() || st.modified_only;
    auto shown = [&](const char* name, const char* help, bool modified) {
        if (st.modified_only && !modified) return false;
        return query.empty() || lower(name).find(query) != std::string::npos ||
               lower(help).find(query) != std::string::npos;
    };

    ui::SeparatorText(dmsg::sfm_options_from_quality);
    ui::help_on_hover(dmsg::sfm_options_from_quality_help);
    const char* budget = feature_budget_flag(job.features);
    if (const sfm::FieldView* f = row_of("max-image-size"); f && shown(f->name, f->help, job.max_image_size > 0))
        changed |= budget_row(f->name, f->help, job.max_image_size, (int)std::atof(f->value.c_str()));
    if (const sfm::FieldView* f = row_of(budget); f && shown(f->name, f->help, job.max_features > 0))
        changed |= budget_row(f->name, f->help, job.max_features, (int)std::atof(f->value.c_str()));
    for (const std::string& name : quality_rows)
        if (const sfm::FieldView* f = row_of(name); f && shown(f->name, f->help, job.options.count(name) > 0))
            changed |= option_row(*f, job);

    // Every other flag, under the heading `spirula sfm --help` prints it.
    std::vector<const char*> groups;
    for (const sfm::FieldView& f : rows)
        if (std::none_of(groups.begin(), groups.end(), [&](const char* g) { return !std::strcmp(g, f.group); }))
            groups.push_back(f.group);
    for (const char* group : groups) {
        const spirula::i18n::Msg* label = group_label(group);
        if (!label) continue;
        std::vector<const sfm::FieldView*> visible;
        for (const sfm::FieldView& f : rows) {
            if (std::strcmp(f.group, group) != 0 || sfm_option_panel_owned(f.name) ||
                std::find(quality_rows.begin(), quality_rows.end(), f.name) != quality_rows.end())
                continue;
            if (shown(f.name, f.help, job.options.count(f.name) > 0)) visible.push_back(&f);
        }
        if (visible.empty()) continue;
        if (searching) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (!ui::TreeNode(*label)) continue;
        for (const sfm::FieldView* f : visible) changed |= option_row(*f, job);
        ImGui::TreePop();
    }
    return changed;
}

#else

bool draw_sfm_options_editor(SfmJob&, SfmOptionsState&) { return false; }

#endif  // SS_TOOL_SFM

}  // namespace gui
