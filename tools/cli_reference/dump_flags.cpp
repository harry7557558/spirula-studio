// Prints the flag tables of `spirula train` and `spirula sfm` as Markdown, read
// from the same tables the parsers and `--help` expand. Built and run by
// tools/gen_cli_reference.py; nothing in the build uses it.

#include "config/TrainConfig.h"
#include "i18n/catalog/Train.h"
#include "i18n/catalog/TrainFields.h"
#include "sfm/SfmConfig.h"
#include "i18n/catalog/SfmFields.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

// Locale.cpp's one global; English is what the reference is written in.
namespace spirula::i18n::detail {
std::atomic<Lang> g_current{Lang::en};
}

namespace {

// One table cell: no line breaks, no column bars, and no `<word>` that a
// renderer would take for an HTML tag (inside a code span it is literal).
std::string cell(std::string s) {
    std::string out;
    bool in_code = false;
    for (char c : s) {
        if (c == '`') in_code = !in_code;
        if (c == '\n') out += ' ';
        else if (c == '|') out += "\\|";
        else if (c == '<' && !in_code) out += "&lt;";
        else out += c;
    }
    return out;
}

std::string code(const std::string& s) { return "`" + s + "`"; }

std::string choices_cell(const char* choices) {
    std::string ch = choices;
    std::string out;
    for (size_t pos = 0; pos <= ch.size();) {
        const size_t bar = ch.find('|', pos);
        const std::string tok = ch.substr(pos, bar == std::string::npos ? bar : bar - pos);
        out += (out.empty() ? "" : ", ") + code(tok.empty() ? "none" : tok);
        if (bar == std::string::npos) break;
        pos = bar + 1;
    }
    return out;
}

// ---- train: value spelling as `spirula train --help` prints it ------------

std::string train_value(bool v) { return v ? "1" : "0"; }
std::string train_value(int v) { return std::to_string(v); }
std::string train_value(float v) {
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}
std::string train_value(const std::string& v) { return v.empty() ? "none" : v; }
template <class T> std::string train_value(const std::optional<T>& v) {
    return v ? train_value(*v) : "none";
}
template <class T, size_t N> std::string train_value(const std::array<T, N>& v) {
    std::string s;
    for (size_t i = 0; i < N; i++) s += (i ? " " : "") + train_value(v[i]);
    return s;
}

std::string flag_name(const char* member) {
    std::string s = member;
    for (char& c : s) if (c == '_') c = '-';
    return "--" + s;
}

void train_presets() {
    namespace T = spirula::i18n::msg::train;
    std::printf("## Presets\n\n"
                "`spirula train [<preset>] ...` starts from one of these. Each row lists "
                "the flags the preset changes from the `3dgs` base; anything set on the "
                "command line wins.\n\n");
    std::printf("| Preset | What it is for | Flags it changes |\n|---|---|---|\n");
    const char* names[] = {"3dgs", "360-camera", "in-the-wild", "centered-object", "hdr",
                           "synthetic", "meshing", "academic-baseline"};
    const TrainConfig base;
    for (const char* name : names) {
        TrainConfig c;
        if (!train_apply_preset(c, name)) continue;
        std::string changed;
#define SS_DIFF(type, member, default_, section, tier, choices)                       \
    if (train_value(c.member) != train_value(base.member))                            \
        changed += (changed.empty() ? "" : ", ") +                                    \
                   code(flag_name(#member) + " " + train_value(c.member));
        SS_CONFIG_FIELDS(SS_DIFF)
#undef SS_DIFF
        const T::PresetText* t = T::preset_text(name);
        std::string what = t ? t->help->get() : "Hidden from `--help`; still accepted.";
        std::printf("| %s | %s | %s |\n", code(name).c_str(), cell(what).c_str(),
                    changed.empty() ? "nothing" : cell(changed).c_str());
    }
    std::printf("\n");
}

void train_flags() {
    namespace F = spirula::i18n::msg::field;
    namespace T = spirula::i18n::msg::train;
    const TrainConfig d;
    std::printf("## Flags\n\n"
                "Defaults are the `3dgs` base, before a preset or a macro flag moves them. "
                "Tier is what `--help` shows: `basic` always, the rest with `--help-all`.\n");
    const char* cur = "";
#define SS_ROW(type, member, default_, section, tier, choices)                           \
    if (std::strcmp(cur, section) != 0) {                                                 \
        cur = section;                                                                    \
        const spirula::i18n::Msg* label = T::section_label(section);                     \
        std::printf("\n### %s\n\n| Flag | Default | Tier | Description |\n|---|---|---|---|\n", \
                    label ? label->get() : section);                                     \
    }                                                                                     \
    {                                                                                     \
        std::string help = F::member##_help.get();                                       \
        if (!train_choices_free_form(choices))                                            \
            help += " One of: " + choices_cell(choices) + ".";                            \
        else if (train_path_spec(choices).is_path)                                        \
            help += std::string(" Takes a ") +                                            \
                    (train_path_spec(choices).folder ? "folder" : "file") +               \
                    (train_path_spec(choices).in_data ? ", relative to `--data`" : "") + "."; \
        std::printf("| %s | %s | %s | %s |\n", code(flag_name(#member)).c_str(),          \
                    code(train_value(d.member)).c_str(), tier, cell(help).c_str());       \
    }
    SS_CONFIG_FIELDS(SS_ROW)
#undef SS_ROW
    std::printf("\n");
}

// ---- sfm: value spelling as `spirula sfm <command> --help` prints it ------

std::string sfm_value(bool v) { return v ? "on" : "off"; }
std::string sfm_value(const std::string& v) { return v.empty() ? "none" : v; }
template <class T>
std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, std::string>
sfm_value(T v) {
    char buf[32];
    if (std::is_floating_point<T>::value) std::snprintf(buf, sizeof buf, "%g", (double)v);
    else std::snprintf(buf, sizeof buf, "%lld", (long long)v);
    return buf;
}

template <class T> std::string sfm_flag(const T&, const char* name) {
    return code(std::string("--") + name);
}
std::string sfm_flag(bool, const char* name) {
    return code(std::string("--") + name) + " / " + code(std::string("--no-") + name);
}

std::string sfm_commands(uint32_t cmds) {
    static const struct { uint32_t bit; const char* name; } kCmds[] = {
        {sfm::CMD_AUTO, "auto"}, {sfm::CMD_EXTRACT, "extract"}, {sfm::CMD_MATCH, "match"},
        {sfm::CMD_MAP, "map"},   {sfm::CMD_MERGE, "merge"},
    };
    std::string s;
    for (const auto& c : kCmds)
        if (cmds & c.bit) s += (s.empty() ? "" : ", ") + std::string(c.name);
    return s;
}

std::string sfm_range(double lo, double hi) {
    if (lo >= hi) return "";
    char buf[64];
    std::snprintf(buf, sizeof buf, " Range %.10g to %.10g.", lo, hi);
    return buf;
}

std::string sfm_group(const char* group) {
    namespace F = spirula::i18n::msg::sfmfield;
    static const struct { const char* key; const spirula::i18n::Msg* msg; } kRows[] = {
        {"pipeline", &F::group_pipeline}, {"colour", &F::group_colour},
        {"camera", &F::group_camera},     {"features", &F::group_features},
        {"matching", &F::group_matching}, {"mapper", &F::group_mapper},
        {"rig", &F::group_rig},           {"manage", &F::group_manage},
        {"merge", &F::group_merge},       {"input", &F::group_input},
        {"runtime", &F::group_runtime},
    };
    std::string label = group;
    for (const auto& r : kRows)
        if (!std::strcmp(r.key, group)) label = r.msg->get();
    if (!label.empty()) label[0] = (char)std::toupper((unsigned char)label[0]);
    return label;
}

std::string sentence(std::string s) {
    if (!s.empty() && !std::strchr(".!?)", s.back())) s += '.';
    return s;
}

void sfm_flags() {
    using namespace sfm;
    const sfm::SfmConfig d;
    std::printf("## Flags\n\n"
                "Defaults are a fresh config, which is what `--help` prints; `auto` then "
                "applies `--quality` and `--data-type` (see [sfm.md](sfm.md#presets)). "
                "A switch is written `--name` / `--no-name`. Basic flags are what the "
                "GUI shows unfolded.\n");
    // A group's rows need not be contiguous in the table; each is listed once,
    // in the order the groups first appear.
    std::vector<std::string> groups;
#define SFM_GROUP(member, name, cmds, tier, group, lo, hi, choices, help)                      \
    if (std::find(groups.begin(), groups.end(), group) == groups.end()) groups.push_back(group);
    SFM_CONFIG_FIELDS(SFM_GROUP)
#undef SFM_GROUP
    // An alias row shares its member with the row it renames.
    auto alias_of = [&](const void* member, const char* self) -> std::string {
        std::string found;
#define SFM_FIND(member_, name, cmds, tier, group, lo, hi, choices, help)                  \
    if (found.empty() && (tier) != Tier::Alias && (const void*)&d.member_ == member &&     \
        std::strcmp(name, self) != 0)                                                      \
        found = name;
        SFM_CONFIG_FIELDS(SFM_FIND)
#undef SFM_FIND
        return found;
    };
#define SFM_ROW(member, name, cmds, tier, group, lo, hi, choices, help)                         \
    if (cur == group) {                                                                          \
        std::string text;                                                                        \
        if ((tier) == Tier::Alias)                                                               \
            text = "Another spelling of " + code("--" + alias_of(&d.member, name)) + ".";        \
        else                                                                                     \
            text = sentence(spirula::i18n::msg::sfmfield::help##_help.get());                      \
        if (*(choices)) text += " One of: " + choices_cell(choices) + ".";                       \
        text += sfm_range(lo, hi);                                                               \
        std::printf("| %s | %s | %s | %s | %s |\n", sfm_flag(d.member, name).c_str(),            \
                    sfm_commands(cmds).c_str(), code(sfm_value(d.member)).c_str(),               \
                    (tier) == Tier::Basic ? "basic" : (tier) == Tier::Alias ? "alias" : "advanced", \
                    cell(text).c_str());                                                         \
    }
    for (const std::string& cur : groups) {
        std::printf("\n### %s\n\n| Flag | Commands | Default | Tier | Description |\n"
                    "|---|---|---|---|---|\n", sfm_group(cur.c_str()).c_str());
        SFM_CONFIG_FIELDS(SFM_ROW)
    }
#undef SFM_ROW
    std::printf("\n");
}

const char* kGenerated =
    "<!-- GENERATED by tools/gen_cli_reference.py from %s. Do not edit; "
    "rerun the script after changing that table. -->\n\n";

}  // namespace

int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "train") {
        std::printf(kGenerated, "src/config/TrainConfig.h and i18n/catalog/TrainFields.h");
        std::printf("# `spirula train` flags\n\n"
                    "Every training flag, from the table in `src/config/TrainConfig.h`. "
                    "How to call the trainer, the macro flags and the flags that are not "
                    "part of this table are in [train.md](train.md).\n\n");
        train_presets();
        train_flags();
        return 0;
    }
    if (which == "sfm") {
        std::printf(kGenerated, "src/sfm/SfmConfig.h and i18n/catalog/SfmFields.h");
        std::printf("# `spirula sfm` flags\n\n"
                    "Every flag in the SfM descriptor table (`src/sfm/SfmConfig.h`), with "
                    "the subcommands that accept it. The flags each subcommand parses by "
                    "hand, and `sfm ba`, are in [sfm.md](sfm.md).\n\n");
        sfm_flags();
        return 0;
    }
    std::fprintf(stderr, "usage: %s train|sfm\n", argc > 0 ? argv[0] : "dump_flags");
    return 2;
}
