// PresetFile.cpp -- see PresetFile.h.

#include "app/gui/PresetFile.h"

#include "app/AppPaths.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gui {

namespace {

// Bounds the parse: this runs on whatever was dropped on the window, and the
// parser holds the whole file in memory.
constexpr uintmax_t kMaxPresetBytes = (uintmax_t)4 << 20;

std::optional<PresetKind> kind_of(const JsonValue& root) {
    if (!root.is_object()) return std::nullopt;
    const JsonValue* k = root.find("kind");
    if (!k) return PresetKind::Train;   // predates the key
    const std::string& s = k->as_string();
    if (s == "train") return PresetKind::Train;
    if (s == "dataset") return PresetKind::Dataset;
    if (s == "mesh") return PresetKind::Mesh;
    if (s == "sfm") return PresetKind::Sfm;
    return std::nullopt;
}

}  // namespace


const char* preset_kind_name(PresetKind k) {
    switch (k) {
        case PresetKind::Dataset: return "dataset";
        case PresetKind::Mesh:    return "mesh";
        case PresetKind::Sfm:     return "sfm";
        default:                  return "train";
    }
}


std::string preset_dir(PresetKind k) {
    fs::path d = fs::path(app::config_dir()) / "presets";
    if (k != PresetKind::Train) d /= preset_kind_name(k);
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
            // Keep non-ASCII bytes: a Japanese or Russian preset name is a
            // good file name on every platform this runs on, and
            // transliterating it produces a name its author cannot find.
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


JsonValue read_preset_file(const std::string& path, PresetKind kind,
                           PresetHeader& head) {
    std::error_code ec;
    const uintmax_t size = fs::file_size(path, ec);
    if (!ec && size > kMaxPresetBytes)
        throw std::runtime_error(path + " is too large to be a preset file");
    JsonValue root = json_parse_file(path);   // throws on unreadable / not JSON
    if (!root.is_object())
        throw std::runtime_error(path + " is not a preset file");
    const std::optional<PresetKind> got = kind_of(root);
    if (!got || *got != kind)
        throw std::runtime_error(path + " is not a " +
                                 preset_kind_name(kind) + " preset");
    head.path = path;
    if (const JsonValue* v = root.find("name")) head.name = v->as_string();
    if (const JsonValue* v = root.find("description"))
        head.description = v->as_string();
    return root;
}


std::optional<PresetKind> probe_preset_kind(const std::string& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return std::nullopt;
    if (fs::file_size(path, ec) > kMaxPresetBytes) return std::nullopt;
    try {
        return kind_of(json_parse_file(path));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}


JsonWriter preset_writer(PresetKind kind, const PresetHeader& head) {
    JsonWriter w;
    w.object();
    w.field("spirula_preset", 1);
    w.field("kind", preset_kind_name(kind));
    w.field("name", head.name);
    w.field("description", head.description);
    return w;
}


void write_preset_file(const std::string& path, const std::string& text) {
    std::error_code ec;
    const fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot write " + path);
    const size_t n = std::fwrite(text.data(), 1, text.size(), f);
    const bool ok = n == text.size() && std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) throw std::runtime_error("failed while writing " + path);
}


void delete_preset_file(const std::string& path, PresetKind kind) {
    const std::optional<PresetKind> got = probe_preset_kind(path);
    if (!got || *got != kind)
        throw std::runtime_error(path + " is not a " +
                                 preset_kind_name(kind) + " preset");
    std::error_code ec;
    if (!fs::remove(path, ec) || ec)
        throw std::runtime_error("cannot delete " + path + ": " + ec.message());
}

}  // namespace gui
