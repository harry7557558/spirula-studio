#pragma once

#include "core/AtomicFile.h"
#include "core/OwnedDirectory.h"
#include "core/Sha256.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

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
    const auto* name_value = record.find("generation"), *expected_value = record.find("manifest_sha256");
    const auto name = name_value ? name_value->as_string() : std::string{};
    const auto expected = expected_value ? expected_value->as_string() : std::string{};
    if (record.get_double("version", 0) != 1 || !generation_name(name) || expected.size() != 64)
        throw std::runtime_error("invalid dense generation record");
    const auto directory = root / "generations" / name;
    const auto manifest = directory / "manifest.json";
    if (sha256_file(manifest.string()) != expected) throw std::runtime_error("dense generation manifest checksum mismatch");
    return {directory / "roma.ply", manifest, name};
}

inline bool artifact_checksum_valid(const ArtifactFiles& files) {
    try {
    namespace fs = std::filesystem;
    if (!fs::is_regular_file(files.cloud) || !fs::is_regular_file(files.manifest)) return false;
    const auto manifest = json_parse_file(files.manifest.string());
    const auto* bytes = manifest.find("cloud_bytes"), *statistics = manifest.find("statistics");
    const auto* hash = manifest.find("cloud_sha256"), *complete = manifest.find("complete");
    const auto expected = hash ? hash->as_string() : std::string{};
    return complete && complete->as_bool() && expected.size() == 64 && statistics &&
        statistics->get_double("exported", 0) > 0 &&
        (!bytes || bytes->as_int() == (int64_t)fs::file_size(files.cloud)) &&
        sha256_file(files.cloud.string()) == expected;
    } catch (const std::exception&) { return false; }
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
    JsonWriter record;
    record.object().field("version", 1).field("generation", files.generation)
        .field("manifest_sha256", files.manifest.empty() ? std::string{} : sha256_file(files.manifest.string())).end();
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
    if (previous.generation.empty() && artifact_checksum_valid(previous)) {
        const auto old_name = "legacy-" + sha256_file(previous.manifest.string()).substr(0, 16) + "-" + generation;
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
    if (!artifact_checksum_valid(result)) throw std::runtime_error("cannot publish inconsistent dense generation");
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
    return file.filename() == "roma.ply" && generation_name(file.parent_path().filename().string()) &&
        same_artifact_path(file.parent_path().parent_path(), root / "generations");
}

inline std::string verified_seed_path(const std::string& dataset, const std::string& seed) {
    namespace fs = std::filesystem;
    if (!is_dense_seed(dataset, seed)) return seed;
    const auto root = fs::path(dataset) / "dense";
    const auto file = fs::path(seed).is_absolute() ? fs::path(seed) : fs::path(dataset) / seed;
    const bool current = same_artifact_path(file, root / "roma.ply");
    const auto files = current ? artifact_files(dataset) : ArtifactFiles{file, file.parent_path() / "manifest.json", {}};
    if (!fs::exists(files.manifest) && files.generation.empty()) return seed;
    if (!artifact_checksum_valid(files)) throw std::runtime_error("dense cloud checksum mismatch");
    return fs::absolute(files.cloud).lexically_normal().string();
}

// The dense cloud an edit saved to `path` replaces, and its dataset: the current
// generation for the dense folder's own roma.ply. False when `path` is not one.
// The dataset folder whose dense cloud `path` is, from the path alone; "" otherwise.
inline std::string dense_dataset_of(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    const fs::path file = fs::absolute(path).lexically_normal(), parent = file.parent_path();
    if (file.filename() != "roma.ply") return {};
    if (parent.filename() == "dense") return parent.parent_path().string();
    const auto generations = parent.parent_path();
    if (generations.filename() != "generations" || generations.parent_path().filename() != "dense" ||
        !generation_name(parent.filename().string()))
        return {};
    return generations.parent_path().parent_path().string();
}

inline bool edited_artifact(const std::filesystem::path& path, std::string& dataset, ArtifactFiles& files) {
    namespace fs = std::filesystem;
    dataset = dense_dataset_of(path);
    if (dataset.empty()) return false;
    const fs::path file = fs::absolute(path).lexically_normal(), parent = file.parent_path();
    files = parent.filename() == "dense" ? artifact_files(dataset)
                                         : ArtifactFiles{file, parent / "manifest.json", parent.filename().string()};
    return true;
}

// Replaces a dense cloud with an edit of it (`edited`, `points` long, on the same
// volume) and re-signs the manifest so training accepts it. The first edit keeps
// the untouched original beside it as roma.original.ply.
inline void commit_edit(const std::string& dataset, const ArtifactFiles& files,
                        const std::filesystem::path& edited, int64_t points) {
    namespace fs = std::filesystem;
    JsonValue manifest = json_parse_file(files.manifest.string());
    if (!manifest.is_object()) throw std::runtime_error("invalid dense manifest");
    auto set = [](JsonValue& object, const std::string& key, JsonValue value) {
        for (auto& [k, v] : object.obj)
            if (k == key) { v = std::move(value); return; }
        object.obj.emplace_back(key, std::move(value));
    };
    auto number = [](double v) { JsonValue j; j.type = JsonValue::Type::Number; j.num = v; return j; };
    auto text = [](const std::string& v) { JsonValue j; j.type = JsonValue::Type::String; j.str = v; return j; };

    const auto root = fs::absolute(dataset).lexically_normal() / "dense";
    bool current = false;
    if (!files.generation.empty()) {
        const auto record = json_parse_file((root / "current.json").string());
        const auto* name = record.find("generation");
        current = name && name->as_string() == files.generation;
    }

    // The original is kept from whichever copy still matches it: an edit saved
    // by an older build overwrote the generation's file but not the folder copy.
    JsonValue edit;
    if (const JsonValue* earlier = manifest.find("edited"); earlier && earlier->is_object()) edit = *earlier;
    edit.type = JsonValue::Type::Object;
    const auto* hash = manifest.find("cloud_sha256");
    const fs::path original = files.cloud.parent_path() / "roma.original.ply";
    if (!edit.has("original_sha256") && hash) {
        std::vector<fs::path> copies{files.cloud};
        if (current) copies.push_back(root / "roma.ply");
        for (const fs::path& copy : copies) {
            if (!fs::is_regular_file(copy) || sha256_file(copy.string()) != hash->as_string()) continue;
            if (!fs::exists(original)) fs::rename(copy, original);
            set(edit, "original_sha256", text(hash->as_string()));
            set(edit, "original_cloud", text(original.filename().string()));
            break;
        }
    }
    replace_file(edited, files.cloud);
    set(edit, "points", number((double)points));
    set(edit, "unix_seconds", number((double)std::time(nullptr)));
    set(manifest, "cloud_sha256", text(sha256_file(files.cloud.string())));
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

    // The current generation's record pins its manifest's checksum, and the
    // dense folder keeps a copy of its files for older readers.
    write_current_generation(root, files);
    for (const auto& source : {files.cloud, files.manifest}) {
        const auto copy = root / (source.filename().string() + "-edit.part");
        fs::copy_file(source, copy, fs::copy_options::overwrite_existing);
        replace_file(copy, root / source.filename());
    }
}

inline std::string verified_dense_file(const std::string& path) {
    namespace fs = std::filesystem;
    const fs::path file = fs::absolute(path).lexically_normal(), parent = file.parent_path();
    if (file.filename() != "roma.ply") return path;
    if (parent.filename() == "dense") return verified_seed_path(parent.parent_path().string(), file.string());
    const auto generations = parent.parent_path();
    if (generations.filename() == "generations" && generations.parent_path().filename() == "dense")
        return verified_seed_path(generations.parent_path().parent_path().string(), file.string());
    return path;
}

}  // namespace spirula::dense
