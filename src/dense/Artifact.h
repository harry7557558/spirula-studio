#pragma once

#include "data/Json.h"
#include "core/Sha256.h"
#include "dense/Generation.h"

#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <functional>
#include <set>

namespace spirula::dense {

inline constexpr int reconstruction_revision = 4;

// Where `spirula dense --progress-dir` writes the live model.bin the GUI polls.
inline std::filesystem::path progress_dir(const std::string& dataset) {
    return std::filesystem::path(dataset) / "dense" / "progress";
}

inline std::string input_stamp(const std::vector<std::string>& paths) {
    namespace fs = std::filesystem;
    std::vector<std::string> entries;
    auto append = [&](const fs::path& p) {
        const auto name = fs::absolute(p).lexically_normal().generic_string();
        if (fs::is_regular_file(p))
            entries.push_back(name + ":" + std::to_string(fs::file_size(p)) + ":" +
                              std::to_string(static_cast<long long>(
                                  fs::last_write_time(p).time_since_epoch().count())));
        else entries.push_back(name + ":missing");
    };
    for (const auto& path : paths) {
        append(path);
        if (fs::is_directory(path))
            for (const auto& entry : fs::recursive_directory_iterator(path))
                if (entry.is_regular_file()) append(entry.path());
    }
    std::sort(entries.begin(), entries.end());
    uint64_t hash = 1469598103934665603ull;
    for (const auto& entry : entries) {
        for (unsigned char c : entry) { hash ^= c; hash *= 1099511628211ull; }
        hash ^= 0; hash *= 1099511628211ull;
    }
    char result[24]; std::snprintf(result, sizeof result, "%016llx", (unsigned long long)hash);
    return result;
}

inline std::string input_content_stamp(const std::vector<std::string>& paths,
                                       const std::function<void()>& check_cancel = {},
                                       const std::function<std::string(const std::string&)>& digest_file = {}) {
    namespace fs = std::filesystem;
    std::set<std::string> files;
    for (const auto& path : paths) {
        const auto absolute = fs::absolute(path).lexically_normal();
        files.insert(absolute.generic_string());
        if (fs::is_directory(absolute))
            for (const auto& entry : fs::recursive_directory_iterator(absolute))
                if (entry.is_regular_file()) files.insert(entry.path().lexically_normal().generic_string());
    }
    Sha256 hash;
    for (const auto& path : files) {
        if (check_cancel) check_cancel();
        std::string value = path + "\n";
        if (fs::is_regular_file(path)) {
            const auto digest = digest_file ? digest_file(path) : sha256_file(path);
            if (digest.empty()) throw std::runtime_error("cannot verify dense input contents");
            value += digest;
        } else value += fs::is_directory(path) ? "directory" : "missing";
        hash.update(reinterpret_cast<const uint8_t*>(value.data()),value.size());
        const uint8_t separator = 0; hash.update(&separator,1);
    }
    return hash.hex();
}

inline bool artifact_inputs_verified(const std::string& dataset,
                                     const std::function<void()>& check_cancel = {}) {
    try {
    const auto files = artifact_files(dataset);
    if (!std::filesystem::is_regular_file(files.manifest)) return false;
    const auto manifest = json_parse_file(files.manifest.string());
    if (manifest.get_double("reconstruction_revision",0) != reconstruction_revision) return false;
    const auto* paths = manifest.find("input_paths"), *content = manifest.find("input_content_stamp"), *cloud = manifest.find("cloud_sha256");
    if (!paths || !content || !cloud) return false;
    std::vector<std::string> inputs;
    for (const auto& path : paths->arr) inputs.push_back(path.as_string());
    return input_content_stamp(inputs,check_cancel) == content->as_string() &&
           artifact_checksum_valid(files);
    } catch (const std::exception&) {
        if (check_cancel) check_cancel();
        return false;
    }
}

inline bool artifact_complete(const std::string& dataset, bool check_inputs = false) {
    try {
        const auto files = artifact_files(dataset);
        const auto manifest = json_parse_file(files.manifest.string());
        const auto* complete = manifest.find("complete"), *hash = manifest.find("cloud_sha256");
        const auto* stats = manifest.find("statistics"), *bytes = manifest.find("cloud_bytes");
        if (check_inputs) {
            if (manifest.get_double("reconstruction_revision",0) != reconstruction_revision) return false;
            const auto* paths = manifest.find("input_paths"), *stamp = manifest.find("input_stamp");
            if (paths && stamp) {
                std::vector<std::string> inputs;
                for (const auto& path : paths->arr) inputs.push_back(path.as_string());
                if (input_stamp(inputs) != stamp->as_string()) return false;
            }
        }
        return complete && complete->as_bool() && hash && hash->as_string().size() == 64 &&
            stats && stats->get_double("exported", 0) > 0 && std::filesystem::is_regular_file(files.cloud) &&
            (!bytes || bytes->as_int() == (int64_t)std::filesystem::file_size(files.cloud));
    } catch (const std::exception&) { return false; }
}

}  // namespace spirula::dense
