#pragma once

// The half of a saved preset that is the same whatever it is a preset OF:
// where the files live, what the header looks like, and how one is probed,
// listed and deleted.
//
// A preset file is a JSON object carrying `spirula_preset`, a `kind`, a
// `name`, a `description`, and whatever keys that kind defines. Train presets
// predate the `kind` key and a run's config.json has never had one, so a file
// without it reads as a train preset -- which is what it was.

#include "data/Json.h"
#include "data/JsonWrite.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gui {

enum class PresetKind { Train, Dataset, Mesh, Sfm };

// What the `kind` key spells, and where that kind's presets live. The folder
// is created on first call; train presets keep <config>/presets itself, which
// is where they have always been.
const char* preset_kind_name(PresetKind k);
std::string preset_dir(PresetKind k);

// A file name for `name` -- lowercased, spaces and separators folded to '-',
// always ending in ".json". Never empty (an unnameable name becomes
// "preset.json"), and never a path: the caller decides the folder.
std::string preset_file_name(const std::string& name);

// The header every preset file carries.
struct PresetHeader {
    std::string name;
    std::string description;
    std::string path;        // the file it was read from / written to
};

// Read `path` and check it is a preset of `kind`, filling `head`. Throws
// std::runtime_error when it is unreadable, is not JSON, or is another kind.
// The parsed document comes back so the caller can read its own keys.
JsonValue read_preset_file(const std::string& path, PresetKind kind,
                           PresetHeader& head);

// The kind `path` declares, or nullopt when it is not a preset file at all.
// Never throws -- this runs on whatever was dropped on the window.
std::optional<PresetKind> probe_preset_kind(const std::string& path);

// A document opened with the marker, the kind and the header written. The
// caller adds its own keys, calls end(), and hands str() to write_preset_file.
JsonWriter preset_writer(PresetKind kind, const PresetHeader& head);
void write_preset_file(const std::string& path, const std::string& text);

// Refuses anything that is not a preset of `kind`, whatever path a caller
// hands over: this is the one operation here that destroys something.
void delete_preset_file(const std::string& path, PresetKind kind);

// Every *.json in preset_dir(kind) that `load` accepts, sorted by name. Files
// that fail to parse are skipped rather than reported: this fills a dropdown.
template <class Load>
auto list_preset_files(PresetKind kind, Load load)
    -> std::vector<decltype(load(std::string()))> {
    namespace fs = std::filesystem;
    std::vector<decltype(load(std::string()))> out;
    std::error_code ec;
    for (fs::directory_iterator it(preset_dir(kind), ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (it->path().extension() != ".json") continue;
        try {
            out.push_back(load(it->path().string()));
        } catch (const std::exception&) {
            // Something else that happens to be JSON, or a file half-written
            // by a crash.
        }
    }
    std::sort(out.begin(), out.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

}  // namespace gui
