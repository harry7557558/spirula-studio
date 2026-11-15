#pragma once

#include "core/AtomicFile.h"
#include "core/OwnedDirectory.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

namespace spirula::dense {

struct ArtifactFiles {
    std::filesystem::path cloud, manifest;
    std::string generation;
};

inline bool generation_name(const std::string& name) {
    return !name.empty() && name != "." && name != ".." &&
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") == std::string::npos;
}

inline ArtifactFiles artifact_files(const std::string& dataset) {
    namespace fs = std::filesystem;
    const auto root = fs::absolute(dataset).lexically_normal() / "dense";
    const auto current = root / "current.json";
    if (!fs::exists(current)) return {root / "roma.ply", root / "manifest.json", {}};
    const auto record = json_parse_file(current.string());
    const auto name = record.find("generation") ? record.find("generation")->as_string() : std::string{};
    if (record.get_double("version", 0) != 1 || (!name.empty() && !generation_name(name)))
        throw std::runtime_error("invalid dense generation record");
    auto relative = [&](const std::string& value) {
        const fs::path path(value);
        if (path.is_absolute()) throw std::runtime_error("invalid dense cloud path");
        for (const auto& part : path)
            if (part == "..") throw std::runtime_error("invalid dense cloud path");
        return value.empty() ? fs::path{} : root / path;
    };
    if (const auto* cloud = record.find("cloud")) {
        const auto* manifest = record.find("manifest");
        return {relative(cloud->as_string()), relative(manifest ? manifest->as_string() : ""), name};
    }
    if (name.empty()) return {};
    const auto directory = root / "generations" / name;
    return {directory / "roma.ply", directory / "manifest.json", name};
}

inline bool artifact_available(const ArtifactFiles& files) {
    namespace fs = std::filesystem;
    std::error_code ec;
    return fs::is_regular_file(files.cloud, ec) && fs::file_size(files.cloud, ec) > 0 && !ec;
}

inline void generation_marker(const std::filesystem::path& directory, const char* marker) {
    std::ofstream output(directory / marker,std::ios::binary | std::ios::trunc);
    output << "spirula-dense-generation-1\n" << directory.filename().string(); output.flush();
    if (!output) throw std::runtime_error("cannot write dense generation ownership marker");
}

// Links above the dataset are resolved here; cleanup still refuses any inside it.
inline std::filesystem::path resolved_dataset(const std::string& dataset) {
    return std::filesystem::weakly_canonical(std::filesystem::absolute(dataset));
}

inline void cleanup_pending_generations(const std::string& dataset) {
    namespace fs = std::filesystem;
    const auto root = resolved_dataset(dataset) / "dense", parent = root / "generations";
    if (!fs::is_directory(parent)) return;
    std::string current;
    try {
        const auto record = json_parse_file((root / "current.json").string());
        if (const auto* value = record.find("generation")) current = value->as_string();
    } catch (const std::exception&) {}
    for (const auto& entry : fs::directory_iterator(parent)) {
        const auto directory = entry.path();
        if (!entry.is_directory()) continue;
        const auto name = directory.filename().string();
        if (!generation_name(name) || name == current || fs::exists(directory / ".published")) continue;
        std::ifstream marker(directory / ".pending",std::ios::binary);
        std::string version, owner; std::getline(marker,version); std::getline(marker,owner);
        marker.close();
        if (version == "spirula-dense-generation-1" && owner == name) remove_owned_directory(directory,parent,name);
    }
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

inline void write_current_generation(const std::filesystem::path& root, const ArtifactFiles& files) {
    namespace fs = std::filesystem;
    JsonWriter record;
    auto relative = [&](const fs::path& path) {
        if (path.empty()) return std::string{};
        const auto normal = fs::weakly_canonical(path.parent_path()) / path.filename();
        const auto rel = normal.lexically_relative(fs::weakly_canonical(root));
        if (rel.empty() || *rel.begin() == "..") throw std::runtime_error("invalid dense cloud path");
        return rel.generic_string();
    };
    record.object().field("version", 1).field("generation", files.generation)
        .field("cloud", relative(files.cloud)).field("manifest", relative(files.manifest)).end();
    const auto temporary = root / ("current-" + files.generation + ".part");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << record.str(); output.flush();
        if (!output) throw std::runtime_error("cannot write dense generation record");
    }
    replace_file(temporary, root / "current.json");
}

inline ArtifactFiles publish_generation(const std::string& dataset, const std::string& generation,
                                        const std::filesystem::path& cloud, const std::filesystem::path& manifest,
                                        const std::function<void(const char*)>& checkpoint = {}) {
    namespace fs = std::filesystem;
    const auto root = resolved_dataset(dataset) / "dense", directory = root / "generations" / generation;
    if (!generation_name(generation) || fs::exists(directory)) throw std::runtime_error("invalid or repeated dense generation");
    ArtifactFiles previous;
    try { previous = artifact_files(dataset); } catch (const std::exception&) {}
    if (previous.generation.empty() && artifact_available(previous) && fs::is_regular_file(previous.manifest)) {
        const auto old_name = "legacy-" + generation;
        const auto old_directory = root / "generations" / old_name;
        if (!fs::create_directories(old_directory)) throw std::runtime_error("dense legacy generation already exists");
        fs::copy_file(previous.cloud, old_directory / "roma.ply");
        fs::copy_file(previous.manifest, old_directory / "manifest.json");
        write_current_generation(root, {old_directory / "roma.ply", old_directory / "manifest.json", old_name});
    }
    if (!fs::exists(root / "current.json")) {
        fs::create_directories(root);
        write_current_generation(root,{});
    }
    if (!fs::create_directories(directory)) throw std::runtime_error("dense generation already exists");
    struct PendingGeneration {
        fs::path directory, parent; bool committed = false;
        ~PendingGeneration() {
            if (!committed) try { remove_owned_directory(directory,parent,directory.filename().string()); } catch (const std::exception&) {}
        }
    } pending{directory,directory.parent_path()};
    generation_marker(directory,".pending");
    ArtifactFiles result{directory / "roma.ply", directory / "manifest.json", generation};
    replace_file(cloud, result.cloud);
    if (checkpoint) checkpoint("generation_cloud");
    replace_file(manifest, result.manifest);
    if (checkpoint) checkpoint("generation_manifest");
    if (!artifact_available(result)) throw std::runtime_error("cannot publish inconsistent dense generation");
    for (const auto& source : {result.cloud, result.manifest}) {
        const auto temporary = root / (source.filename().string() + "-" + generation + ".part");
        fs::copy_file(source, temporary, fs::copy_options::overwrite_existing);
        replace_file(temporary, root / source.filename());
        if (checkpoint) checkpoint(source == result.cloud ? "compatibility_cloud" : "compatibility_manifest");
    }
    if (checkpoint) checkpoint("before_current");
    generation_marker(directory,".published");
    write_current_generation(root, result);
    pending.committed = true;
    std::error_code error; fs::remove(directory / ".pending",error);
    return result;
}

inline bool same_artifact_path(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code error;
    if (std::filesystem::equivalent(a, b, error)) return true;
    std::error_code first_error, second_error;
    const auto first = std::filesystem::weakly_canonical(a, first_error);
    const auto second = std::filesystem::weakly_canonical(b, second_error);
    return !first_error && !second_error && first == second;
}

inline bool is_dense_seed(const std::string& dataset, const std::string& seed) {
    namespace fs = std::filesystem;
    if (dataset.empty() || seed.empty()) return false;
    const auto root = fs::path(dataset) / "dense";
    const auto file = fs::path(seed).is_absolute() ? fs::path(seed) : fs::path(dataset) / seed;
    if (same_artifact_path(file, root / "roma.ply")) return true;
    try { if (same_artifact_path(file, artifact_files(dataset).cloud)) return true; }
    catch (const std::exception&) {}
    return file.filename() == "roma.ply" && generation_name(file.parent_path().filename().string()) &&
        same_artifact_path(file.parent_path().parent_path(), root / "generations");
}

inline std::string resolve_seed_path(const std::string& dataset, const std::string& seed) {
    namespace fs = std::filesystem;
    if (!is_dense_seed(dataset, seed)) return seed;
    return fs::absolute(fs::path(dataset) / seed).lexically_normal().string();
}

// The dataset folder whose dense cloud `path` is, from the path alone; "" otherwise.
inline std::string dense_dataset_of(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    const fs::path file = fs::absolute(path).lexically_normal(), parent = file.parent_path();
    if (file.extension() != ".ply") return {};
    if (parent.filename() == "dense") return parent.parent_path().string();
    const auto generations = parent.parent_path();
    if (generations.filename() != "generations" || generations.parent_path().filename() != "dense" ||
        !generation_name(parent.filename().string()))
        return {};
    return generations.parent_path().parent_path().string();
}

inline bool edited_artifact(const std::filesystem::path& path, std::string& dataset, ArtifactFiles& files) {
    namespace fs = std::filesystem;
    if (path.filename() != "roma.ply") return false;
    dataset = dense_dataset_of(path);
    if (dataset.empty()) return false;
    const fs::path file = fs::absolute(path).lexically_normal(), parent = file.parent_path();
    files = {file, parent / "manifest.json", parent.filename() == "dense" ? std::string{} : parent.filename().string()};
    return true;
}

// The first in-place edit keeps the preceding cloud as roma.original.ply.
inline void commit_edit(const std::string& dataset, const ArtifactFiles& files,
                        const std::filesystem::path& edited, int64_t points) {
    namespace fs = std::filesystem;
    JsonValue manifest;
    if (fs::is_regular_file(files.manifest)) manifest = json_parse_file(files.manifest.string());
    else manifest.type = JsonValue::Type::Object;
    if (!manifest.is_object()) throw std::runtime_error("invalid dense manifest");
    auto set = [](JsonValue& object, const std::string& key, JsonValue value) {
        for (auto& [k, v] : object.obj)
            if (k == key) { v = std::move(value); return; }
        object.obj.emplace_back(key, std::move(value));
    };
    auto number = [](double v) { JsonValue j; j.type = JsonValue::Type::Number; j.num = v; return j; };
    auto text = [](const std::string& v) { JsonValue j; j.type = JsonValue::Type::String; j.str = v; return j; };
    JsonValue complete;
    complete.type = JsonValue::Type::Bool;
    complete.b = true;
    set(manifest, "complete", complete);

    const auto root = fs::absolute(dataset).lexically_normal() / "dense";
    bool current = false;
    try { current = same_artifact_path(files.cloud, artifact_files(dataset).cloud); }
    catch (const std::exception&) {}
    JsonValue edit;
    if (const JsonValue* earlier = manifest.find("edited"); earlier && earlier->is_object()) edit = *earlier;
    edit.type = JsonValue::Type::Object;
    const fs::path original = files.cloud.parent_path() / "roma.original.ply";
    if (!fs::exists(original) && fs::is_regular_file(files.cloud))
        fs::copy_file(files.cloud, original);
    set(edit, "original_cloud", text(original.filename().string()));
    edit.obj.erase(std::remove_if(edit.obj.begin(), edit.obj.end(),
        [](const auto& field) { return field.first == "original_sha256"; }), edit.obj.end());
    manifest.obj.erase(std::remove_if(manifest.obj.begin(), manifest.obj.end(),
        [](const auto& field) { return field.first == "cloud_sha256"; }), manifest.obj.end());
    replace_file(edited, files.cloud);
    set(edit, "points", number((double)points));
    set(edit, "unix_seconds", number((double)std::time(nullptr)));
    set(manifest, "cloud_bytes", number((double)fs::file_size(files.cloud)));
    JsonValue statistics;
    if (const JsonValue* s = manifest.find("statistics"); s && s->is_object()) statistics = *s;
    statistics.type = JsonValue::Type::Object;
    set(statistics, "exported", number((double)points));
    set(manifest, "statistics", std::move(statistics));
    set(manifest, "edited", std::move(edit));

    JsonWriter writer;
    json_write(writer, manifest);
    const auto temporary = files.manifest.parent_path() / "manifest-edit.part";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << writer.str(); output.flush();
        if (!output) throw std::runtime_error("cannot write dense manifest");
    }
    replace_file(temporary, files.manifest);
    if (!current) return;

    if (files.cloud.parent_path() == root) return;
    for (const auto& source : {files.cloud, files.manifest}) {
        const auto copy = root / (source.filename().string() + "-edit.part");
        fs::copy_file(source, copy, fs::copy_options::overwrite_existing);
        replace_file(copy, root / source.filename());
    }
}

inline std::string resolve_dense_file(const std::string& path) {
    return std::filesystem::absolute(path).lexically_normal().string();
}

}  // namespace spirula::dense
