#pragma once
// Every plausible PLY under <dataset>/dense/ is a cloud version; the newest is the default.
// Dot directories hold run scratch (dense/.work) and are never searched. docs/dense.md.

#include "core/AtomicFile.h"
#include "core/OwnedDirectory.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace spirula::dense {

inline std::filesystem::path dense_root(const std::string& dataset) {
    return std::filesystem::absolute(dataset).lexically_normal() / "dense";
}

// Lock, settings, live preview and the resumable pair cache of a run.
inline std::filesystem::path work_root(const std::string& dataset) { return dense_root(dataset) / ".work"; }

// Links above the dataset are resolved here; cleanup still refuses any inside it.
inline std::filesystem::path resolved_dataset(const std::string& dataset) {
    return std::filesystem::weakly_canonical(std::filesystem::absolute(dataset));
}

// A cancelled run is terminated, so its work directory outlives it. Call only under
// the run lock, which proves no `run-*` directory belongs to a live process.
inline void cleanup_abandoned_runs(const std::filesystem::path& cache) {
    namespace fs = std::filesystem;
    if (!fs::is_directory(cache)) return;
    auto digits_after = [](const std::string& name, size_t from, const char* allowed) {
        return name.size() > from && name.find_first_not_of(allowed, from) == std::string::npos;
    };
    for (const auto& entry : fs::directory_iterator(cache)) {
        const auto identity = entry.path();
        const auto identity_name = identity.filename().string();
        if (entry.is_symlink() || !entry.is_directory() || identity_name.size() != 64 ||
            !digits_after(identity_name, 0, "0123456789abcdef")) continue;
        for (const auto& run : fs::directory_iterator(identity)) {
            const auto name = run.path().filename().string();
            if (!run.is_symlink() && run.is_directory() && name.rfind("run-", 0) == 0 && digits_after(name, 4, "0123456789"))
                remove_owned_directory(run.path(), identity, name);
        }
        if (fs::is_empty(identity)) fs::remove(identity);
    }
}

// A point cloud's header with a size the file can hold; meshes and truncated binaries fail.
inline bool plausible_cloud(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (path.extension() != ".ply" || !fs::is_regular_file(path, ec)) return false;
    const uint64_t size = fs::file_size(path, ec);
    std::ifstream in(path, std::ios::binary);
    std::string line, format, element;
    if (ec || !std::getline(in, line) || (line != "ply" && line != "ply\r")) return false;
    int64_t vertices = -1, count = 0;
    uint64_t body = 0, stride = 0;
    bool fixed = true, x = false, faces = false;
    auto close_element = [&] { body += (uint64_t)count * stride; };
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream f(line);
        std::string word, type, name;
        f >> word;
        if (word == "format") f >> format;
        else if (word == "element") {
            close_element();
            stride = 0;
            if (!(f >> element >> count) || count < 0) return false;
            if (element == "vertex") vertices = count;
            if (element == "face" && count > 0) faces = true;
        } else if (word == "property") {
            f >> type;
            if (type == "list") { fixed = false; continue; }
            f >> name;
            if (element == "vertex" && name == "x") x = true;
            const int bytes = type == "char" || type == "uchar" || type == "int8" || type == "uint8" ? 1
                : type == "short" || type == "ushort" || type == "int16" || type == "uint16" ? 2
                : type == "double" || type == "float64" ? 8 : 4;
            stride += (uint64_t)bytes;
        } else if (word == "end_header") {
            close_element();
            if (vertices <= 0 || !x || faces) return false;
            const uint64_t header = (uint64_t)in.tellg();
            return format == "ascii" ? size > header : !fixed || size >= header + body;
        }
    }
    return false;
}

// `<stem>.json` beside the cloud; a `roma.ply` from older runs also reads its `manifest.json`.
inline std::filesystem::path manifest_of(const std::filesystem::path& cloud) {
    namespace fs = std::filesystem;
    std::error_code ec;
    auto own = cloud; own.replace_extension(".json");
    if (cloud.filename() == "roma.ply" && !fs::exists(own, ec)) return cloud.parent_path() / "manifest.json";
    return own;
}

struct Cloud {
    std::filesystem::path ply, manifest;
    std::string relative;   // under dense/, generic separators
    bool current = false;
};

// Newest first; the first one is the default seed.
inline std::vector<Cloud> clouds(const std::string& dataset) {
    namespace fs = std::filesystem;
    const auto root = dense_root(dataset);
    struct Found { Cloud cloud; fs::file_time_type time; };
    std::vector<Found> found;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        std::error_code entry;
        if (it->is_directory(entry)) {
            if (!name.empty() && name[0] == '.') it.disable_recursion_pending();
            continue;
        }
        if (!plausible_cloud(it->path())) continue;
        const auto time = fs::last_write_time(it->path(), entry);
        if (entry) continue;
        found.push_back({{it->path(), manifest_of(it->path()),
                          it->path().lexically_relative(root).generic_string()}, time});
    }
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) {
        return a.time != b.time ? a.time > b.time : a.cloud.relative < b.cloud.relative;
    });
    std::vector<Cloud> result;
    for (auto& f : found) result.push_back(std::move(f.cloud));
    if (!result.empty()) result.front().current = true;
    return result;
}

inline Cloud default_cloud(const std::string& dataset) {
    auto all = clouds(dataset);
    return all.empty() ? Cloud{} : all.front();
}

// Moves a finished run's files to `dense/<local date>_<label>.ply` and `.json`, manifest first.
inline Cloud publish_cloud(const std::string& dataset, const std::string& label,
                           const std::filesystem::path& ply, const std::filesystem::path& manifest) {
    namespace fs = std::filesystem;
    const auto root = dense_root(dataset);
    fs::create_directories(root);
    char date[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(date, sizeof date, "%Y-%m-%d_%H%M%S", std::localtime(&now));
    std::string stem = date;
    if (!label.empty()) stem += "_" + label;
    for (char& c : stem)
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') c = '-';
    std::error_code ec;
    std::string name = stem;
    for (int n = 2; fs::exists(root / (name + ".ply"), ec) || fs::exists(root / (name + ".json"), ec); ++n)
        name = stem + "-" + std::to_string(n);
    Cloud cloud{root / (name + ".ply"), root / (name + ".json"), name + ".ply", true};
    replace_file(manifest, cloud.manifest);
    replace_file(ply, cloud.ply);
    return cloud;
}

// The dataset whose dense/ folder holds `path`, from the path alone; "" otherwise.
inline std::string dense_dataset_of(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    const fs::path file = fs::absolute(path).lexically_normal();
    if (file.extension() != ".ply") return {};
    for (fs::path dir = file.parent_path(); dir.has_relative_path(); dir = dir.parent_path()) {
        const auto name = dir.filename().string();
        if (name == "dense") return dir.parent_path().string();
        if (!name.empty() && name[0] == '.') return {};
    }
    return {};
}

inline bool is_dense_seed(const std::string& dataset, const std::string& seed) {
    namespace fs = std::filesystem;
    if (dataset.empty() || seed.empty()) return false;
    const auto file = fs::path(seed).is_absolute() ? fs::path(seed) : fs::path(dataset) / seed;
    const auto owner = dense_dataset_of(file);
    std::error_code ec;
    return !owner.empty() && fs::equivalent(owner, dataset, ec);
}

inline std::string resolve_seed_path(const std::string& dataset, const std::string& seed) {
    namespace fs = std::filesystem;
    if (!is_dense_seed(dataset, seed)) return seed;
    return fs::absolute(fs::path(dataset) / seed).lexically_normal().string();
}

inline std::string resolve_dense_file(const std::string& path) {
    return std::filesystem::absolute(path).lexically_normal().string();
}

// The first in-place edit keeps the preceding cloud as `<stem>.original.ply`.
inline void commit_edit(const std::filesystem::path& cloud, const std::filesystem::path& edited) {
    namespace fs = std::filesystem;
    auto original = cloud;
    original.replace_extension(".original.ply");
    std::error_code ec;
    if (!fs::exists(original, ec) && fs::is_regular_file(cloud, ec)) fs::copy_file(cloud, original);
    replace_file(edited, cloud);
}

inline void delete_cloud(const std::string& dataset, const std::string& relative) {
    namespace fs = std::filesystem;
    for (const auto& c : clouds(dataset))
        if (c.relative == relative) {
            fs::remove(c.ply);
            auto own = c.ply; own.replace_extension(".json");
            std::error_code ec; fs::remove(own, ec);
            return;
        }
    throw std::runtime_error("dense cloud version no longer exists");
}

}  // namespace spirula::dense
