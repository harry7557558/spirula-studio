#pragma once

#include "dense/Generation.h"

namespace spirula::dense {

struct CloudVersion {
    ArtifactFiles files;
    std::string relative;
    bool current = false;
};

inline std::vector<CloudVersion> cloud_versions(const std::string& dataset) {
    namespace fs = std::filesystem;
    const auto root = resolved_dataset(dataset) / "dense";
    ArtifactFiles current;
    try { current = artifact_files(dataset); } catch (const std::exception&) {}
    std::vector<CloudVersion> versions;
    auto add = [&](const fs::path& path) {
        std::error_code ec;
        if (path.extension() != ".ply" || !fs::is_regular_file(path, ec)) return;
        const auto normal = fs::weakly_canonical(path.parent_path(), ec) / path.filename();
        const auto relative = normal.lexically_relative(root);
        if (ec || relative.empty() || *relative.begin() == "..") return;
        for (const auto& v : versions)
            if (same_artifact_path(path, v.files.cloud)) return;
        const bool active = same_artifact_path(path, current.cloud);
        const auto parent = path.parent_path();
        auto manifest = parent / "manifest.json";
        if (!fs::is_regular_file(manifest, ec)) manifest = current.manifest;
        const auto generation = parent.parent_path() == root / "generations"
            ? parent.filename().string() : std::string{};
        versions.push_back({{normal, manifest, generation}, relative.generic_string(), active});
    };
    add(current.cloud);
    auto scan = [&](const fs::path& dir, bool compatibility) {
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (compatibility && it->path().filename() == "roma.ply" &&
                fs::exists(root / "current.json", ec) && !same_artifact_path(it->path(), current.cloud)) continue;
            add(it->path());
        }
    };
    scan(root, true);
    std::error_code ec;
    for (fs::directory_iterator it(root / "generations", ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_symlink(ec) || !it->is_directory(ec) ||
            !generation_name(it->path().filename().string()) || fs::exists(it->path() / ".pending", ec)) continue;
        scan(it->path(), false);
    }
    std::sort(versions.begin(), versions.end(), [](const auto& a, const auto& b) {
        return a.current != b.current ? a.current : a.relative < b.relative;
    });
    return versions;
}

inline void delete_cloud_version(const std::string& dataset, const std::string& relative) {
    namespace fs = std::filesystem;
    const auto versions = cloud_versions(dataset);
    const auto found = std::find_if(versions.begin(), versions.end(),
        [&](const auto& v) { return v.relative == relative; });
    if (found == versions.end()) throw std::runtime_error("dense cloud version no longer exists");
    const auto root = resolved_dataset(dataset) / "dense";
    const auto parent = fs::weakly_canonical(found->files.cloud.parent_path());
    const auto under = parent.lexically_relative(root);
    if (under.empty() || *under.begin() == "..") throw std::runtime_error("invalid dense cloud version path");
    if (found->current) {
        ArtifactFiles next;
        for (const auto& v : versions)
            if (v.relative != relative) { next = v.files; break; }
        write_current_generation(root, next);
    }
    fs::remove(found->files.cloud);
}

}  // namespace spirula::dense
