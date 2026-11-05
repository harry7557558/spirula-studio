#pragma once

#include "core/AtomicFile.h"
#include "core/Sha256.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace app {

inline std::filesystem::path geometry_completion_path(const std::filesystem::path& path) {
    return path.string() + ".geometry-complete";
}
inline bool geometry_output_requires_overwrite(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) ||
        std::filesystem::file_size(path, ec) == 0 || ec) return false;
    std::ifstream record(geometry_completion_path(path), std::ios::binary);
    std::string version;
    return !std::getline(record, version) || version != "spirula-geometry-1";
}


inline bool geometry_output_complete(const std::filesystem::path& path, const std::string& key) {
    std::error_code ec;
    if (key.empty() || !std::filesystem::is_regular_file(path, ec) ||
        std::filesystem::file_size(path, ec) == 0 || ec) return false;
    std::ifstream record(geometry_completion_path(path), std::ios::binary);
    std::string version, request, checksum, extra;
    if (!std::getline(record, version) || version != "spirula-geometry-1" ||
        !std::getline(record, request) || request != key ||
        !std::getline(record, checksum) || checksum.size() != 64 ||
        std::getline(record, extra)) return false;
    return checksum == spirula::sha256_file(path.string());
}

// Unmarked legacy files are not proof that every face and encoder finished.
// Keep the old output until the replacement has been fully written and closed.
template<class Encode>
bool publish_geometry_output(const std::filesystem::path& path, const std::string& key,
                             Encode encode, bool overwrite = false) {
    static std::atomic<unsigned long long> serial{0};
    const std::string suffix = ".part-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(serial++);
    const std::filesystem::path temporary(path.string() + suffix);
    const auto record = geometry_completion_path(path);
    const std::filesystem::path record_temporary(record.string() + suffix);
    auto cleanup = [&] {
        std::error_code ec;
        std::filesystem::remove(temporary, ec);
        std::filesystem::remove(record_temporary, ec);
    };
    try {
        if (key.empty() || !encode(temporary.string())) { cleanup(); return false; }
        std::error_code ec;
        if (std::filesystem::file_size(temporary, ec) == 0 || ec) { cleanup(); return false; }
        const std::string checksum = spirula::sha256_file(temporary.string());
        if (checksum.empty()) { cleanup(); return false; }
        std::ofstream output(record_temporary, std::ios::binary | std::ios::trunc);
        output << "spirula-geometry-1\n" << key << '\n' << checksum << '\n';
        output.close();
        if (!output) { cleanup(); return false; }
        if (!overwrite && geometry_output_requires_overwrite(path))
            throw std::runtime_error("unmarked geometry output " + path.string() +
                                     " is preserved; use --overwrite to replace it");
        spirula::replace_file(temporary, path);
        spirula::replace_file(record_temporary, record);
        return true;
    } catch (...) {
        cleanup();
        throw;
    }
}

struct GeometryStagedOutput {
    std::filesystem::path staged, destination;
    std::string key;
};

inline void publish_geometry_family(const std::vector<GeometryStagedOutput>& outputs,
                                    bool overwrite) {
    if (outputs.empty()) throw std::runtime_error("empty geometry output family");
    for (const auto& output : outputs) {
        if (!geometry_output_complete(output.staged, output.key))
            throw std::runtime_error("incomplete staged geometry: " + output.staged.string());
        if (!overwrite && geometry_output_requires_overwrite(output.destination))
            throw std::runtime_error("unmarked geometry output " + output.destination.string() +
                                     " is preserved; use --overwrite to replace it");
    }
    // Incomplete owner records permit safe resume without adopting legacy files.
    auto incomplete = [](const GeometryStagedOutput& output) {
        const std::filesystem::path pending(output.staged.string() + ".pending");
        std::ofstream record(pending, std::ios::binary | std::ios::trunc);
        record << "spirula-geometry-1\n";
        record.close();
        if (!record) throw std::runtime_error("cannot invalidate geometry completion");
        spirula::replace_file(pending, geometry_completion_path(output.destination));
    };
    for (const auto& output : outputs) incomplete(output);
    for (const auto& output : outputs)
        spirula::replace_file(output.staged, output.destination);
    try {
        for (const auto& output : outputs)
            spirula::replace_file(geometry_completion_path(output.staged),
                                  geometry_completion_path(output.destination));
    } catch (...) {
        for (const auto& output : outputs) {
            try { incomplete(output); } catch (...) {
                std::error_code ec;
                std::filesystem::remove(geometry_completion_path(output.destination), ec);
            }
        }
        throw;
    }
}

}  // namespace app
