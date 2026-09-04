// TrainPreset.cpp -- see TrainPreset.h.

#include "app/gui/TrainPreset.h"

#include "app/AppPaths.h"
#include "config/TrainConfigJson.h"
#include "data/Json.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gui {

namespace {

// The JSON string escape the config writer uses, for the three keys that are
// not training flags (name, description, base). Same two characters, same
// reason: these files are written and read by this program only.
std::string quote(const std::string& v) {
    std::string s = "\"";
    for (char ch : v) {
        if (ch == '"' || ch == '\\') { s += '\\'; s += ch; }
        else if (ch == '\n') s += "\\n";
        else if (ch == '\r') continue;
        else if (ch == '\t') s += "\\t";
        else s += ch;
    }
    return s + "\"";
}

// Every flag whose value differs from `ref`. Used when the file carries no
// `touched` list -- a run's config.json, or a preset from a build that
// predates the key -- so that loading one never lets a macro option quietly
// overwrite a value the file spelled out.
std::set<std::string> diff_fields(const TrainConfig& c, const TrainConfig& ref) {
    std::set<std::string> out;
#define SS_PRESET_DIFF(type, member, default_, section, tier, choices)        \
    if (!(c.member == ref.member)) out.insert(#member);
    SS_CONFIG_FIELDS(SS_PRESET_DIFF)
#undef SS_PRESET_DIFF
    // The context fields are not part of a preset, so a difference in one is
    // not tuning to protect.
#define SS_PRESET_DROP(member) out.erase(#member);
    SS_PRESET_CONTEXT_FIELDS(SS_PRESET_DROP)
#undef SS_PRESET_DROP
    return out;
}

void clear_context(TrainConfig& c) {
    const TrainConfig stock;
#define SS_PRESET_CLEAR(member) c.member = stock.member;
    SS_PRESET_CONTEXT_FIELDS(SS_PRESET_CLEAR)
#undef SS_PRESET_CLEAR
}

}  // namespace


std::string preset_dir() {
    fs::path d = fs::path(app::config_dir()) / "presets";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d.string();
}


std::string preset_file_name(const std::string& name) {
    std::string out;
    bool dash = false;
    for (unsigned char ch : name) {
        if (std::isalnum(ch)) {
            out += (char)std::tolower(ch);
            dash = false;
        } else if (ch >= 0x80) {
            // Keep non-ASCII bytes as they are: a Japanese or Russian preset
            // name makes a perfectly good file name on every platform this
            // runs on, and transliterating it would produce a name its author
            // cannot find.
            out += (char)ch;
            dash = false;
        } else if (!dash && !out.empty()) {
            out += '-';
            dash = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) out = "preset";
    return out + ".json";
}


void save_preset(const TrainPreset& p, const std::string& path) {
    TrainPreset copy = p;
    clear_context(copy.cfg);

    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("cannot write " + path);

    std::fprintf(f, "{\n");
    std::fprintf(f, "    \"spirula_preset\": 1,\n");
    std::fprintf(f, "    \"name\": %s,\n", quote(copy.name).c_str());
    std::fprintf(f, "    \"description\": %s,\n", quote(copy.description).c_str());
    std::fprintf(f, "    \"base_preset\": %s,\n", quote(copy.base).c_str());

    std::fprintf(f, "    \"touched\": [");
    bool first = true;
    for (const std::string& t : copy.touched) {
        std::fprintf(f, "%s%s", first ? "" : ", ", quote(t).c_str());
        first = false;
    }
    std::fprintf(f, "],\n");

    std::fprintf(f, "    \"config\": {");
    first = true;
    for (const auto& [key, value] : train_config_json_pairs(copy.cfg)) {
        std::fprintf(f, "%s\n        \"%s\": %s", first ? "" : ",", key,
                     value.c_str());
        first = false;
    }
    std::fprintf(f, "\n    }\n}\n");

    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) throw std::runtime_error("failed while writing " + path);
}


TrainPreset load_preset(const std::string& path) {
    JsonValue root = json_parse_file(path);   // throws on unreadable / not JSON
    if (!root.is_object())
        throw std::runtime_error(path + " is not a preset file");

    TrainPreset p;
    p.path = path;

    auto str = [&](const char* key) -> std::string {
        const JsonValue* v = root.find(key);
        return v ? v->as_string() : std::string();
    };

    const JsonValue* cfg_obj = root.find("config");
    const JsonValue& fields = (cfg_obj && cfg_obj->is_object()) ? *cfg_obj : root;
    if (!train_config_json_has_fields(fields))
        throw std::runtime_error(path + " holds no training options");

    // Our own format names the built-in it started from "base_preset"; a run's
    // config.json names it "preset". Neither is required -- an unknown or
    // absent name just leaves the stock base, and the config it carries is
    // absolute anyway.
    p.base = str("base_preset");
    if (p.base.empty()) p.base = str("preset");
    TrainConfig base_cfg;
    if (p.base.empty() || !train_apply_preset(base_cfg, p.base)) {
        p.base = "3dgs";
        base_cfg = TrainConfig();
    }

    p.cfg = base_cfg;
    train_config_from_json(fields, p.cfg);   // throws on a malformed vec3
    clear_context(p.cfg);

    p.name = str("name");
    p.description = str("description");
    if (p.name.empty()) {
        // A run's config.json: the run folder is what anyone would call it.
        fs::path fp(path);
        p.name = fp.stem() == "config" && fp.has_parent_path()
                     ? fp.parent_path().filename().string()
                     : fp.stem().string();
    }

    if (const JsonValue* t = root.find("touched"); t && t->is_array()) {
        for (const JsonValue& e : t->arr)
            if (!e.as_string().empty()) p.touched.insert(e.as_string());
    } else {
        p.touched = diff_fields(p.cfg, base_cfg);
    }
    return p;
}


bool is_preset_file(const std::string& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    // Guard the parse: this runs on whatever was dropped on the window, and
    // the parser holds the whole file in memory.
    if (fs::file_size(path, ec) > (uintmax_t)4 << 20) return false;
    try {
        JsonValue root = json_parse_file(path);
        if (!root.is_object()) return false;
        const JsonValue* cfg = root.find("config");
        return train_config_json_has_fields(
            (cfg && cfg->is_object()) ? *cfg : root);
    } catch (const std::exception&) {
        return false;
    }
}


void delete_preset(const std::string& path) {
    // Deleting is the one operation here that destroys something, so it
    // checks what it is pointed at rather than trusting the caller: only a
    // file this module would read back as a preset can be removed through it.
    if (!is_preset_file(path))
        throw std::runtime_error(path + " is not a preset file");
    std::error_code ec;
    if (!fs::remove(path, ec) || ec)
        throw std::runtime_error("cannot delete " + path + ": " + ec.message());
}


std::vector<TrainPreset> list_presets() {
    std::vector<TrainPreset> out;
    std::error_code ec;
    for (fs::directory_iterator it(preset_dir(), ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (it->path().extension() != ".json") continue;
        try {
            out.push_back(load_preset(it->path().string()));
        } catch (const std::exception&) {
            // Something else that happens to be JSON, or a file half-written
            // by a crash. A dropdown is no place to report it.
        }
    }
    std::sort(out.begin(), out.end(),
              [](const TrainPreset& a, const TrainPreset& b) {
                  return a.name < b.name;
              });
    return out;
}

}  // namespace gui
