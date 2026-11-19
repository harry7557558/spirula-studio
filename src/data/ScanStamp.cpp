#include "data/ScanStamp.h"

#include "data/DatasetParser.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "data/LidarLayout.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace scan_stamp {
namespace {

// Each image's pose in its file's own numbers: COLMAP's qvec and tvec, or the
// 3x4 of a transforms.json frame. A record is only compared with its own format.
using Poses = std::map<std::string, std::vector<double>>;

Poses read_poses(const fs::path& file) {
    Poses out;
    if (file.filename() == "transforms.json") {
        const JsonValue meta = json_parse_file(file.string());
        const JsonValue* frames = meta.find("frames");
        if (!frames) return out;
        for (const JsonValue& f : frames->arr) {
            const JsonValue* name = f.find("file_path");
            const JsonValue* m = f.find("transform_matrix");
            if (!name || !m || m->arr.size() < 3) continue;
            std::vector<double>& pose = out[name->as_string()];
            for (int r = 0; r < 3; r++)
                for (const JsonValue& v : m->arr[(size_t)r].arr) pose.push_back(v.as_double());
        }
        return out;
    }
    const std::string dir = file.parent_path().string();
    const auto images =
        file.filename() == "images.txt" ? read_images_text(dir) : read_images_binary(dir);
    for (const auto& [id, im] : images) {
        std::vector<double>& pose = out[im.name];
        pose.assign(im.qvec.begin(), im.qvec.end());
        pose.insert(pose.end(), im.tvec.begin(), im.tvec.end());
    }
    return out;
}

}  // namespace

std::string pose_file(const std::string& dataset, const std::string& format,
                      const std::string& recon_dir, const std::string& seed) {
    std::error_code ec;
    const fs::path transforms = fs::path(dataset) / "transforms.json";
    if (format == "nerfstudio" || (format.empty() && fs::exists(transforms, ec)))
        return transforms.string();
    if (!format.empty() && format != "colmap") return {};
    DatasetParserConfig cfg;
    cfg.recon_dir = recon_dir;
    cfg.seed_pointcloud = seed;
    return colmap_images_file(dataset, cfg);
}

void write(const std::string& dataset, const std::string& poses) {
    JsonWriter w;
    w.object().key("images").object();
    for (const auto& [name, pose] : read_poses(poses)) {
        std::string list = "[";
        for (size_t k = 0; k < pose.size(); k++) list += (k ? ", " : "") + json_number_exact(pose[k]);
        w.field_raw(name.c_str(), list + "]");
    }
    w.end().end();
    const fs::path path = fs::path(dataset) / lidar_layout::kCameras;
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out << w.str();
    out.close();
    if (out.fail()) throw std::runtime_error("cannot write " + path.string());
}

bool matches(const std::string& dataset, const std::string& poses) {
    std::error_code ec;
    const fs::path path = fs::path(dataset) / lidar_layout::kCameras;
    if (poses.empty() || !fs::exists(path, ec)) return false;
    try {
        const JsonValue record = json_parse_file(path.string());
        const JsonValue* images = record.find("images");
        if (!images) return false;
        const Poses now = read_poses(poses);
        int64_t shared = 0;
        for (const auto& [name, was] : images->obj) {
            const auto it = now.find(name);
            if (it == now.end()) continue;
            if (it->second.size() != was.arr.size()) return false;
            // An unmoved model reads back exactly; a moved or scaled one is far off.
            for (size_t k = 0; k < was.arr.size(); k++) {
                const double a = was.arr[k].as_double(), b = it->second[k];
                if (!(std::fabs(a - b) <= 1e-9 * (1.0 + std::fabs(a)))) return false;
            }
            shared++;
        }
        return shared > 0;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace scan_stamp
