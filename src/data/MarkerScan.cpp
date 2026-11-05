#include "data/MarkerScan.h"
#include "data/MarkerDetector.h"
#include "data/SparseEdit.h"
#include "data/Json.h"
#include "data/SceneTransform.h"
#include "external/stb_image.h"

#include <filesystem>
#include <algorithm>
#include <map>

namespace spirula {

MarkerScanResult scan_markers(const ParsedDataset& ds, MarkerFamily family,
                              double decimate, int threads,
                              const std::atomic<bool>* cancel,
                              const std::function<void(int, int)>& progress) {
    MarkerScanResult result;
    MarkerDetector detector(family, threads, decimate);
    std::vector<MarkerObservation> observations;
    const int total = (int)ds.image_filenames.size();
    if (progress) progress(0, total);
    for (int i = 0; i < total; ++i) {
        if (cancel && cancel->load()) { result.cancelled = true; return result; }
        const auto& path = ds.image_filenames[(size_t)i];
        int w = 0, h = 0, channels = 0;
        stbi_uc* img = stbi_load(path.c_str(), &w, &h, &channels, 1);
        if (!img) ++result.unreadable;
        else {
            const auto detections = detector.detect_grayscale(img, w, h);
            stbi_image_free(img);
            for (auto d : detections) {
                if (d.margin < 20.0 || d.hamming > 1.0) continue;
                // Intrinsics describe calibrated pixels; photos may be resized copies.
                const double sx = (double)ds.widths[(size_t)i] / w;
                const double sy = (double)ds.heights[(size_t)i] / h;
                d.center[0] *= sx; d.center[1] *= sy;
                for (auto& c : d.corners) { c[0] *= sx; c[1] *= sy; }
                observations.push_back({(uint32_t)i, path, d});
            }
        }
        ++result.images;
        if (progress) progress(i + 1, total);
    }
    if (cancel && cancel->load()) { result.cancelled = true; return result; }
    auto markers = triangulate_markers(observations, ds);
    std::map<MarkerFamily, int> support;
    for (const auto& m : markers)
        if (m.reproj_error_px < 4.0) support[m.family] += m.num_views;
    result.family = family;
    if (family == MarkerFamily::Auto) {
        if (support.empty())
            for (const auto& o : observations) ++support[o.det.family];
        int best = 0;
        for (const auto& entry : support)
            if (entry.second > best) { best = entry.second; result.family = entry.first; }
    }
    // Auto chooses one dictionary; IDs from different dictionaries are unrelated.
    for (const auto& m : markers)
        if (m.family == result.family && m.reproj_error_px < 4.0) result.markers.push_back(m);
    for (const auto& o : observations)
        if (o.det.family == result.family) ++result.observations;
    return result;
}

std::string marker_dataset_dir(const std::string& path) {
    namespace fs = std::filesystem;
    if (path.empty()) return {};
    std::error_code ec;
    fs::path p(path);
    if (fs::is_regular_file(p, ec)) p = p.parent_path();
    const auto resolved = resolve_sparse_dir(p.string());
    return resolved.empty() ? p.string() : resolved;
}

std::string find_markers_file(const std::string& path) {
    namespace fs = std::filesystem;
    if (path.empty()) return {};
    std::error_code ec;
    fs::path p(path);
    if (fs::is_regular_file(p, ec)) p = p.parent_path();
    for (const auto& dir : {p, fs::path(marker_dataset_dir(path)), p.parent_path()}) {
        const auto file = dir / "markers.json";
        if (fs::is_regular_file(file, ec)) return file.string();
    }
    return {};
}

MarkerDataset resolve_marker_dataset(const std::string& path) {
    namespace fs = std::filesystem;
    MarkerDataset result;
    if (path.empty()) return result;
    std::error_code ec;
    fs::path start = fs::absolute(path, ec);
    if (ec) return result;
    if (!fs::is_directory(start, ec)) start = start.parent_path();
    auto use = [&](const fs::path& candidate) {
        if (sparse_format_of(candidate.lexically_normal().string()) == SparseFormat::None) return false;
        const auto directory = resolve_sparse_dir(candidate.lexically_normal().string());
        if (directory.empty()) return false;
        result.directory = fs::weakly_canonical(directory, ec).string();
        if (ec) result.directory = directory;
        return true;
    };
    for (auto dir = start; !dir.empty(); dir = dir.parent_path()) {
        const auto config = dir / "config.json";
        if (fs::is_regular_file(config, ec)) {
            try {
                const auto root = json_parse_file(config.string());
                const auto* data = root.find("data");
                if (data && !data->as_string().empty()) {
                    fs::path dataset(data->as_string());
                    if (!use(dataset.is_absolute() ? dataset : dir / dataset) &&
                        !use(dataset)) return {};
                    for (auto field : {std::make_pair("image_dir", &result.config.image_dir),
                                       std::make_pair("colmap_recon_dir", &result.config.recon_dir)}) {
                        if (const auto* value = root.find(field.first)) *field.second = value->as_string();
                    }
                    SceneTransform transform;
                    if (read_scene_transform_json((dir / "scene_transform.json").string(), transform)) {
                        result.file_from_dataset.s = transform.scale;
                        std::copy(transform.R, transform.R + 9, result.file_from_dataset.R);
                        std::copy(transform.t, transform.t + 3, result.file_from_dataset.t);
                    }
                    return result;
                }
            } catch (const std::exception&) {}
        }
        if (use(dir)) return result;
        if (dir == dir.parent_path()) break;
        if (std::distance(start.begin(), start.end()) - std::distance(dir.begin(), dir.end()) >= 7) break;
    }
    return result;
}

}  // namespace spirula
