// ConfigUI.cpp -- see ConfigUI.h. Every widget below is expanded from the
// SS_CONFIG_FIELDS X-macro; keep this file free of per-field special
// cases (the point is that a new row in the field table shows up here with
// zero GUI work).

#include "app/gui/ConfigUI.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"

#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Train.h"
#include "i18n/catalog/TrainFields.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace msg = spirula::i18n::msg::gui;
namespace fld = spirula::i18n::msg::field;
using spirula::i18n::Msg;

namespace gui {

namespace {

constexpr float kFieldWidth = 170.0f;
const ImVec4 kModifiedColor(1.0f, 0.72f, 0.25f, 1.0f);

std::vector<std::string> split_choices(const char* choices) {
    std::vector<std::string> out;
    std::string ch = choices;
    size_t pos = 0;
    while (pos <= ch.size()) {
        size_t bar = ch.find('|', pos);
        out.push_back(ch.substr(pos, bar == std::string::npos
                                         ? std::string::npos : bar - pos));
        if (bar == std::string::npos) break;
        pos = bar + 1;
    }
    return out;
}

// ---- search ------------------------------------------------------------------

// ASCII only: the catalogs are UTF-8 and std::tolower would mangle a
// continuation byte under a non-C locale.
std::string lower_ascii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}

// The UI's language and English both, since the flag names, `--help` and
// every config.json are English whatever the labels are showing. NOT all 13:
// Spanish "superficie" answering an English "sup" is noise, not reach.
void add_text(std::string& out, const Msg& m) {
    out += ' ';
    out += m.get();
    const char* en = m.in(spirula::i18n::Lang::en);
    if (en != m.get()) { out += ' '; out += en; }
}

// Everything one field can be found by.
std::string field_text(const char* key, const char* section, const Msg& name,
                       const Msg& help, const char* choices) {
    std::string s = key;
    add_text(s, name);
    add_text(s, help);
    if (const Msg* m = spirula::i18n::msg::train::section_label(section))
        add_text(s, *m);
    for (const std::string& c : split_choices(choices)) {
        s += ' ';
        s += c;
        if (const Msg* m = fld::choice_label(key, c.c_str())) add_text(s, *m);
    }
    return lower_ascii(s);
}

enum : size_t {
#define SS_FIELD_INDEX(type, member, default_, section, tier, choices)         \
    kField_##member,
    SS_CONFIG_FIELDS(SS_FIELD_INDEX)
#undef SS_FIELD_INDEX
    kNumFields
};

const std::vector<std::string>& field_texts() {
    static std::vector<std::string> texts;
    static spirula::i18n::Lang built = spirula::i18n::Lang::en;
    if (texts.size() != kNumFields || built != spirula::i18n::current()) {
        built = spirula::i18n::current();
        texts.clear();
        texts.reserve(kNumFields);
#define SS_FIELD_TEXT(type, member, default_, section, tier, choices)          \
        texts.push_back(field_text(#member, section, fld::member,              \
                                   fld::member##_help, choices));
        SS_CONFIG_FIELDS(SS_FIELD_TEXT)
#undef SS_FIELD_TEXT
    }
    return texts;
}

// Space, `_` and `-` all separate, so `depth sup`, `depth-sup` and
// `depth_sup` are one query, and each token may match anywhere: `depth weight`
// finds depth_supervision_weight without naming what is between them.
std::vector<std::string> query_tokens(const char* q) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* c = q;; c++) {
        if (*c && !std::strchr(" \t_-+/,.", *c)) { cur += *c; continue; }
        if (!cur.empty()) { out.push_back(lower_ascii(cur)); cur.clear(); }
        if (!*c) break;
    }
    return out;
}

void refresh_matches(ConfigUIState& st) {
    // The language is part of the key: switching it rebuilds the texts below.
    std::string query = st.search +
                        std::to_string((unsigned)spirula::i18n::current());
    if (st.match.size() == kNumFields && st.match_query == query) return;
    st.match_query = query;
    st.match.assign(kNumFields, 1);
    std::vector<std::string> tokens = query_tokens(st.search);
    if (tokens.empty()) return;
    const std::vector<std::string>& texts = field_texts();
    for (size_t i = 0; i < kNumFields; i++)
        for (const std::string& t : tokens)
            if (texts[i].find(t) == std::string::npos) { st.match[i] = 0; break; }
}

// ---- value -> display string (tooltips / reset labels) ---------------------

std::string value_str(bool v)               { return v ? "true" : "false"; }
std::string value_str(int v)                { return std::to_string(v); }
std::string value_str(float v) {
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[32]; std::snprintf(buf, sizeof buf, "%g", v); return buf;
}
std::string value_str(const std::string& v) { return v.empty() ? "none" : v; }
template <typename T> std::string value_str(const std::optional<T>& v) {
    return v ? value_str(*v) : "auto";
}
template <typename T, size_t N> std::string value_str(const std::array<T, N>& v) {
    std::string s;
    for (size_t i = 0; i < N; i++) s += (i ? " " : "") + value_str(v[i]);
    return s;
}

// ---- per-type widgets -------------------------------------------------------

bool draw_value(const char*, bool& v, const char*) {
    return ui::CheckboxRaw("##v", &v);
}

bool draw_value(const char*, int& v, const char*) {
    ImGui::SetNextItemWidth(kFieldWidth);
    return ui::InputIntRaw("##v", &v);
}

bool draw_value(const char*, float& v, const char*) {
    ImGui::SetNextItemWidth(kFieldWidth);
    return ui::InputFloatRaw("##v", &v, "%g");
}

// `blank_none`: the "none" token IS the empty value, true where the default is
// empty. Where the default is a real word "none" is a value like any other,
// and writing "" gives the trainer a spelling no parser knows.
bool draw_value(const char* key, std::string& v, const char* choices,
                bool blank_none = true) {
    std::string ch = choices;
    if (ch.empty() || ch == "none") {
        ImGui::SetNextItemWidth(kFieldWidth);
        return ui::InputTextRaw("##v", &v);
    }
    std::vector<std::string> opts = split_choices(choices);
    std::string cur = (blank_none && v.empty()) ? "none" : v;
    bool changed = false;
    ImGui::SetNextItemWidth(kFieldWidth);
    if (ui::BeginComboRaw("##v", choice_display(key, cur).c_str())) {
        for (const auto& o : opts) {
            bool sel = (o == cur);
            if (ui::SelectableRaw(choice_display(key, o), sel)) {
                v = (blank_none && o == "none") ? "" : o;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool draw_value(const char*, std::optional<bool>& v, const char*) {
    // Literal[True, False, None] -> tri-state dropdown.
    const char* cur = !v.has_value() ? "auto" : (*v ? "true" : "false");
    bool changed = false;
    ImGui::SetNextItemWidth(kFieldWidth);
    if (ui::BeginComboRaw("##v", cur)) {
        if (ui::SelectableRaw("auto",  !v.has_value())) { v = std::nullopt; changed = true; }
        if (ui::SelectableRaw("true",  v.has_value() && *v))  { v = true;  changed = true; }
        if (ui::SelectableRaw("false", v.has_value() && !*v)) { v = false; changed = true; }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename T>
bool draw_value(const char* key, std::optional<T>& v, const char* choices) {
    bool has = v.has_value();
    bool changed = false;
    if (ui::CheckboxRaw("##has", &has)) {
        v = has ? std::optional<T>(T{}) : std::nullopt;
        changed = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ui::SetTooltip(msg::cfg_unchecked_is_auto);
    ImGui::SameLine();
    if (v.has_value()) {
        T tmp = *v;
        if (draw_value(key, tmp, choices)) { v = tmp; changed = true; }
    } else {
        ui::TextDisabled(msg::cfg_auto);
    }
    return changed;
}

template <size_t N>
bool draw_value(const char*, std::array<float, N>& v, const char*) {
    ImGui::SetNextItemWidth(kFieldWidth + 90);
    return ImGui::InputScalarN("##v", ImGuiDataType_Float, v.data(), (int)N,
                               nullptr, nullptr, "%g");
}

template <size_t N>
bool draw_value(const char*, std::array<int, N>& v, const char*) {
    ImGui::SetNextItemWidth(kFieldWidth + 90);
    return ImGui::InputScalarN("##v", ImGuiDataType_S32, v.data(), (int)N);
}

// ---- one field row -----------------------------------------------------------

// Only a string widget cares what the default is, and it wants the BASE one:
// a preset filling the field in does not stop "none" meaning unset, and the
// word where "" belongs is what the trainer then has to reject.
template <typename T>
bool draw_value_of(const char* key, T& v, const T&, const char* choices) {
    return draw_value(key, v, choices);
}

bool draw_value_of(const char* key, std::string& v, const std::string& base,
                   const char* choices) {
    return draw_value(key, v, choices, /*blank_none=*/base.empty());
}

template <typename T>
bool field_row(const char* cli_key, const Msg& name, const Msg& help,
               T& v, const T& def, const T& base, const char* choices) {
    bool modified = !(v == def);
    ImGui::PushID(cli_key);
    bool changed = draw_value_of(cli_key, v, base, choices);
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                                        ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::OpenPopupOnItemClick("ctx", ImGuiPopupFlags_MouseButtonRight);
    ImGui::SameLine();
    // What the row is called is translated; what it IS on the command line is
    // `--<cli_key>`, one hover away and still what the search box matches.
    if (modified) ui::TextColoredRaw(kModifiedColor, std::string(name.get()) + " *");
    else          ui::TextRaw(name.get());
    hovered |= ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                                    ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::OpenPopupOnItemClick("ctx", ImGuiPopupFlags_MouseButtonRight);

    if (hovered && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(px(420.0f));
        ui::TextColoredRaw(kModifiedColor, "--" + std::string(cli_key));
        // The same sentence `spirula train --help` prints for this flag.
        ui::TextRaw(help.get());
        ImGui::Separator();
        ui::Text(msg::cfg_preset_default, {value_str(def)});
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    if (ImGui::BeginPopup("ctx")) {
        if (ui::MenuItem(msg::cfg_reset_to, {value_str(def)}))
            { v = def; changed = true; }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

// The heading text is shared with `spirula train --help`, so it lives in the
// train catalog rather than this file's.
const Msg& section_label(const char* section) {
    static_assert((size_t)kTrainNumSections ==
                      spirula::i18n::msg::train::kNumSectionText,
                  "config/TrainConfig.h and i18n/catalog/Train.h disagree "
                  "about how many section headings there are");
    const Msg* m = spirula::i18n::msg::train::section_label(section);
    return m ? *m : msg::cfg_no_description;
}

// The detail filter: how specialist a flag may be and still be listed.
const Msg& tier_label(int rank) {
    switch (rank) {
        case 0:  return msg::cfg_tier_basic;
        case 1:  return msg::cfg_tier_advanced;
        default: return msg::cfg_tier_all;
    }
}
constexpr int kNumTierChoices = 3;

// Fields owned by dedicated GUI controls; hidden from the generated list.
// The dataset path has the folder picker next to it, and the macro options
// (train_resolve_macros()) are the top of Basic Options -- listing them again
// here would offer the user two places to set the same thing.
bool gui_managed(const char* cli_key) {
    return !std::strcmp(cli_key, "data") ||
           !std::strcmp(cli_key, "quality") ||
           !std::strcmp(cli_key, "floater_suppression") ||
           !std::strcmp(cli_key, "distraction_robustness");
}

}  // namespace


std::string choice_display(const char* flag, const std::string& value) {
    const Msg* m = fld::choice_label(flag, value.c_str());
    if (!m) return value;
    std::string label = m->get();
    return label == value ? value : label + " (" + value + ")";
}

bool draw_config_editor(TrainConfig& cfg, const TrainConfig& defaults,
                        ConfigUIState& st) {
    ImGui::SetNextItemWidth(px(-250.0f));
    ui::InputTextHintBufRaw("##cfgsearch", msg::cfg_search_hint,
                            st.search, sizeof st.search);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(130.0f));
    // The list is short and its entries are the label, so the combo carries
    // no separate caption -- what it filters is explained on hover.
    const bool tier_open = ui::BeginComboRaw("##cfgtier", tier_label(st.tier).get());
    if (!tier_open) ui::help_on_hover(msg::cfg_tier_help);
    if (tier_open) {
        for (int i = 0; i < kNumTierChoices; i++)
            if (ui::Selectable(tier_label(i), i == st.tier)) st.tier = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ui::Checkbox(msg::cfg_edited_only, &st.modified_only);
    ui::help_on_hover(msg::cfg_edited_only_help);

    const bool searching = st.search[0] != 0 || st.modified_only;
    // Searching reaches past the detail filter: a flag you can name should be
    // findable without first working out how specialist it is.
    const int max_tier = searching ? kTrainNumTiers - 1
                       : st.tier >= kNumTierChoices - 1 ? kTrainNumTiers - 1
                                                        : st.tier;

    refresh_matches(st);
    auto passes = [&](size_t idx, const char* key, const char* tier,
                      bool modified) {
        if (gui_managed(key)) return false;
        if (train_tier_rank(tier) > max_tier) return false;
        if (st.modified_only && !modified) return false;
        return st.match[idx] != 0;
    };

    // A filter change is what opens sections; between changes the fold state
    // is the user's, so a section can be collapsed while the filter is on.
    const std::string filter_key =
        searching ? std::string(st.search) + (st.modified_only ? "\x01" : "")
                  : std::string();
    const bool filter_changed = filter_key != st.filter_key;
    if (filter_changed) {
        if (searching && !st.filtering) {
            std::memcpy(st.saved_open, st.open, sizeof st.open);
            std::memset(st.sticky, 0, sizeof st.sticky);
        } else if (!searching) {
            for (int i = 0; i < kTrainNumSections; i++)
                st.open[i] = st.sticky[i] ? st.sticky[i] > 0 : st.saved_open[i];
        }
        st.filter_key = filter_key;
        st.filtering = searching;
    }

    // Pass 1: per-section visible-field counts (sections are contiguous in
    // the field table, so pass 2 can stream headings).
    int vis[kTrainNumSections] = {0};
#define SS_COUNT(type, member, default_, section, tier, choices)               \
    if (passes(kField_##member, #member, tier,                                 \
               !(cfg.member == defaults.member)))                              \
        vis[train_section_index(section)]++;
    SS_CONFIG_FIELDS(SS_COUNT)
#undef SS_COUNT

    if (filter_changed && searching)
        for (int i = 0; i < kTrainNumSections; i++)
            if (vis[i]) st.open[i] = true;

    // Pass 2: draw. `base` is the field table's own defaults, before any
    // preset; only the "none means unset" question reads it.
    static const TrainConfig base{};
    bool any_changed = false;
    const char* cur_section = "";
    bool section_open = false;
    int cur_index = 0;
#define SS_DRAW(type, member, default_, section, tier, choices)                \
    if (std::strcmp(cur_section, section) != 0) {                              \
        cur_section = section;                                                 \
        cur_index = train_section_index(section);                              \
        if (vis[cur_index] == 0) {                                             \
            section_open = false;                                              \
        } else {                                                               \
            ImGui::SetNextItemOpen(st.open[cur_index]);                        \
            section_open = ui::CollapsingHeader(section_label(section));       \
            if (section_open != st.open[cur_index]) {                          \
                st.open[cur_index] = section_open;                             \
                if (searching) st.sticky[cur_index] = section_open ? 1 : -1;   \
            }                                                                  \
        }                                                                      \
    }                                                                          \
    if (section_open &&                                                        \
        passes(kField_##member, #member, tier,                                 \
               !(cfg.member == defaults.member)) &&                            \
        field_row(#member, fld::member, fld::member##_help,                    \
                  cfg.member, defaults.member, base.member, choices)) {        \
        any_changed = true;                                                    \
        st.touched.insert(#member);                                            \
        if (searching) st.sticky[cur_index] = 1;                               \
    }
    SS_CONFIG_FIELDS(SS_DRAW)
#undef SS_DRAW

    return any_changed;
}

}  // namespace gui
