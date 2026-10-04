#include "app/LidarDataset.h"

#include "app/DepthPng.h"
#include "app/E57Dataset.h"
#include "app/ScanDepth.h"
#include "core/CameraModel.h"
#include "data/CameraMath.h"
#include "data/DatasetParser.h"
#include "data/ImageProbe.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "data/PointCloudFile.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "i18n/Message.h"
#include "i18n/catalog/Lidar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::lidar;
using spirula::i18n::format;
using sfm::Mat3;
using sfm::Vec3;

namespace app::lidar {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kAlignedMarker = "lidar_alignment.json";
constexpr const char* kUnalignedDir = "sparse_unaligned";

// The one draw of the scan everything else is taken from: the depth maps'
// cloud (15 bytes a point once bucketed; fewer leave holes in a 1600 px map),
// and random subsets of it for the alignment and the seed points.
constexpr int64_t kDrawCap = 40000000;
constexpr int64_t kAlignDraw = 8000000;
constexpr int64_t kAlignTarget = 2000000;
constexpr int kMapSide = 1600;
// A scan point is seen by an image when it lies on the depth that image's map
// holds there, to this much.
constexpr double kSeenRatio = 0.03;
constexpr double kSeenSlack = 0.05;   // metres
// The surface fit is refused when it leaves the anchors further than this,
// in the scene's own size (the anchors' spread).
constexpr double kAnchorDrift = 0.05;
// How closely the anchors must agree with the fit for the scanner's poses to
// stand in for the photographs the reconstruction did not place.
constexpr double kPlaceCentre = 0.05;   // metres, median
constexpr double kPlaceRot = 0.5;       // degrees, median

uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

uint64_t draw_threshold(int64_t cap, int64_t total) {
    if (total <= cap) return std::numeric_limits<uint64_t>::max();
    return (uint64_t)std::min(std::ldexp((double)cap / (double)total, 64),
                              18446744073709549568.0);
}

std::string fixed(double v, int places) {
    char b[48];
    std::snprintf(b, sizeof b, "%.*f", places, v);
    return b;
}

// ================
// COLMAP binary, cameras passed through untouched
// ================

int colmap_num_params(int model) {
    static const int kParams[18] = {3, 4, 4, 5, 8, 8, 12, 5, 4, 5, 12, 16, 4, 5, 3, 4, 0, 2};
    if (model < 0 || model > 17 || (model == 16)) throw std::runtime_error(
        "COLMAP camera model " + std::to_string(model) + " is not one this reads");
    return kParams[model];
}

struct ColCamera {
    int32_t model = 0;
    uint64_t width = 0, height = 0;
    std::vector<double> params;
};
struct ColImage {
    double q[4] = {1, 0, 0, 0};   // world -> camera, w x y z
    double t[3] = {0, 0, 0};
    uint32_t camera = 0;
    std::string name;
    std::vector<double> xy;       // [2n]
    std::vector<uint64_t> point;  // [n]
};
struct ColPoint {
    double xyz[3];
    uint8_t rgb[3];
    double error = 0;
    std::vector<uint32_t> track;  // [2n]: image id, point2D index
};
struct ColModel {
    std::map<uint32_t, ColCamera> cameras;
    std::map<uint32_t, ColImage> images;
    std::map<uint64_t, ColPoint> points;
};

template <class T> T rd(std::ifstream& f) {
    T v{};
    f.read((char*)&v, sizeof v);
    if (!f) throw std::runtime_error("a COLMAP file ends early");
    return v;
}
template <class T> void wr(std::ofstream& f, T v) { f.write((const char*)&v, sizeof v); }

ColModel read_model(const fs::path& dir) {
    ColModel m;
    {
        std::ifstream f(dir / "cameras.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot read " + (dir / "cameras.bin").string());
        const uint64_t n = rd<uint64_t>(f);
        for (uint64_t i = 0; i < n; i++) {
            const uint32_t id = rd<uint32_t>(f);
            ColCamera& c = m.cameras[id];
            c.model = rd<int32_t>(f);
            c.width = rd<uint64_t>(f);
            c.height = rd<uint64_t>(f);
            c.params.resize((size_t)colmap_num_params(c.model));
            for (double& p : c.params) p = rd<double>(f);
        }
    }
    {
        std::ifstream f(dir / "images.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot read " + (dir / "images.bin").string());
        const uint64_t n = rd<uint64_t>(f);
        for (uint64_t i = 0; i < n; i++) {
            const uint32_t id = rd<uint32_t>(f);
            ColImage& im = m.images[id];
            for (double& v : im.q) v = rd<double>(f);
            for (double& v : im.t) v = rd<double>(f);
            im.camera = rd<uint32_t>(f);
            char ch;
            while (f.get(ch) && ch != '\0') im.name += ch;
            const uint64_t np = rd<uint64_t>(f);
            im.xy.resize(np * 2);
            im.point.resize(np);
            for (uint64_t k = 0; k < np; k++) {
                im.xy[k * 2] = rd<double>(f);
                im.xy[k * 2 + 1] = rd<double>(f);
                im.point[k] = rd<uint64_t>(f);
            }
        }
    }
    {
        std::ifstream f(dir / "points3D.bin", std::ios::binary);
        if (!f) return m;
        const uint64_t n = rd<uint64_t>(f);
        for (uint64_t i = 0; i < n; i++) {
            const uint64_t id = rd<uint64_t>(f);
            ColPoint& p = m.points[id];
            for (double& v : p.xyz) v = rd<double>(f);
            for (uint8_t& c : p.rgb) c = rd<uint8_t>(f);
            p.error = rd<double>(f);
            const uint64_t tl = rd<uint64_t>(f);
            p.track.resize(tl * 2);
            for (uint32_t& v : p.track) v = rd<uint32_t>(f);
        }
    }
    return m;
}

void write_model(const ColModel& m, const fs::path& dir) {
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "cameras.bin", std::ios::binary | std::ios::trunc);
        wr<uint64_t>(f, m.cameras.size());
        for (const auto& [id, c] : m.cameras) {
            wr<uint32_t>(f, id);
            wr<int32_t>(f, c.model);
            wr<uint64_t>(f, c.width);
            wr<uint64_t>(f, c.height);
            for (double p : c.params) wr<double>(f, p);
        }
        if (!f) throw std::runtime_error("cannot write " + (dir / "cameras.bin").string());
    }
    {
        std::ofstream f(dir / "images.bin", std::ios::binary | std::ios::trunc);
        wr<uint64_t>(f, m.images.size());
        for (const auto& [id, im] : m.images) {
            wr<uint32_t>(f, id);
            for (double v : im.q) wr<double>(f, v);
            for (double v : im.t) wr<double>(f, v);
            wr<uint32_t>(f, im.camera);
            f.write(im.name.c_str(), (std::streamsize)im.name.size() + 1);
            wr<uint64_t>(f, im.point.size());
            for (size_t k = 0; k < im.point.size(); k++) {
                wr<double>(f, im.xy[k * 2]);
                wr<double>(f, im.xy[k * 2 + 1]);
                wr<uint64_t>(f, im.point[k]);
            }
        }
        if (!f) throw std::runtime_error("cannot write " + (dir / "images.bin").string());
    }
    {
        std::ofstream f(dir / "points3D.bin", std::ios::binary | std::ios::trunc);
        wr<uint64_t>(f, m.points.size());
        for (const auto& [id, p] : m.points) {
            wr<uint64_t>(f, id);
            for (double v : p.xyz) wr<double>(f, v);
            f.write((const char*)p.rgb, 3);
            wr<double>(f, p.error);
            wr<uint64_t>(f, p.track.size() / 2);
            for (uint32_t v : p.track) wr<uint32_t>(f, v);
        }
        if (!f) throw std::runtime_error("cannot write " + (dir / "points3D.bin").string());
    }
}

Mat3 quat_to_R(const double q[4]) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    return {1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)};
}

void R_to_quat(const Mat3& R, double q[4]) {
    const double tr = R[0] + R[4] + R[8];
    if (tr > 0) {
        const double s = 0.5 / std::sqrt(tr + 1.0);
        q[0] = 0.25 / s;
        q[1] = (R[7] - R[5]) * s; q[2] = (R[2] - R[6]) * s; q[3] = (R[3] - R[1]) * s;
    } else if (R[0] > R[4] && R[0] > R[8]) {
        const double s = 2.0 * std::sqrt(1.0 + R[0] - R[4] - R[8]);
        q[0] = (R[7] - R[5]) / s; q[1] = 0.25 * s;
        q[2] = (R[1] + R[3]) / s; q[3] = (R[2] + R[6]) / s;
    } else if (R[4] > R[8]) {
        const double s = 2.0 * std::sqrt(1.0 + R[4] - R[0] - R[8]);
        q[0] = (R[2] - R[6]) / s; q[1] = (R[1] + R[3]) / s;
        q[2] = 0.25 * s; q[3] = (R[5] + R[7]) / s;
    } else {
        const double s = 2.0 * std::sqrt(1.0 + R[8] - R[0] - R[4]);
        q[0] = (R[3] - R[1]) / s; q[1] = (R[2] + R[6]) / s;
        q[2] = (R[5] + R[7]) / s; q[3] = 0.25 * s;
    }
    if (q[0] < 0) for (int k = 0; k < 4; k++) q[k] = -q[k];
}

ModelGeometry geometry_of(const ColModel& m) {
    ModelGeometry g;
    for (const auto& [id, im] : m.images) {
        ModelGeometry::View v;
        v.name = im.name;
        const Mat3 Rw2c = quat_to_R(im.q);
        v.R = sfm::transpose(Rw2c);
        v.centre = sfm::mul(v.R, Vec3{im.t[0], im.t[1], im.t[2]}) * -1.0;
        g.views.push_back(v);
    }
    for (const auto& [id, p] : m.points) {
        g.points.push_back({p.xyz[0], p.xyz[1], p.xyz[2]});
        g.track.push_back((int)(p.track.size() / 2));
    }
    return g;
}

// x' = s R x + t on every pose and point.
void transform_model(ColModel& m, const sfm::Sim3& T) {
    for (auto& [id, im] : m.images) {
        sfm::Pose p{quat_to_R(im.q), {im.t[0], im.t[1], im.t[2]}};
        const sfm::Pose q = sfm::transformPose(T, p);
        R_to_quat(q.R, im.q);
        im.t[0] = q.t.x; im.t[1] = q.t.y; im.t[2] = q.t.z;
    }
    for (auto& [id, p] : m.points) {
        const Vec3 x = sfm::transformPoint(T, {p.xyz[0], p.xyz[1], p.xyz[2]});
        p.xyz[0] = x.x; p.xyz[1] = x.y; p.xyz[2] = x.z;
    }
}

// The models of a run: sparse/ itself, or its numbered subfolders in order.
std::vector<fs::path> model_dirs(const fs::path& root) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (fs::exists(root / "cameras.bin", ec)) return {root};
    std::vector<std::pair<int, fs::path>> numbered;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        const std::string n = e.path().filename().string();
        if (!e.is_directory(ec) || n.empty() ||
            !std::all_of(n.begin(), n.end(), [](char c) { return c >= '0' && c <= '9'; }))
            continue;
        if (fs::exists(e.path() / "cameras.bin", ec) && fs::exists(e.path() / "images.bin", ec))
            numbered.push_back({std::stoi(n), e.path()});
    }
    std::sort(numbered.begin(), numbered.end());
    for (auto& [k, p] : numbered) out.push_back(p);
    return out;
}

// sparse/<k> subfolders that hold a binary model, in order.
std::vector<fs::path> numbered_models(const fs::path& root) {
    std::vector<std::pair<int, fs::path>> found;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        const std::string n = e.path().filename().string();
        if (!e.is_directory(ec) || n.empty() ||
            !std::all_of(n.begin(), n.end(), [](char c) { return c >= '0' && c <= '9'; }))
            continue;
        if (fs::exists(e.path() / "cameras.bin", ec) && fs::exists(e.path() / "images.bin", ec))
            found.push_back({std::stoi(n), e.path()});
    }
    std::sort(found.begin(), found.end());
    std::vector<fs::path> out;
    for (auto& [k, p] : found) out.push_back(p);
    return out;
}

// `name` under `root`, or the file beside it with the same stem; "" if none.
std::string resolve_image(const fs::path& root, const std::string& name) {
    std::error_code ec;
    if (fs::exists(root / name, ec)) return name;
    const fs::path rel(name);
    const fs::path dir = root / rel.parent_path();
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file(ec) && e.path().stem() == rel.stem())
            return (rel.parent_path() / e.path().filename()).generic_string();
    return "";
}

bool holds_files(const fs::path& dir) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec))
        if (it->is_regular_file(ec)) return true;
    return false;
}

std::string stem_of(const std::string& s) {
    const size_t dot = s.find_last_of('.'), slash = s.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return s;
    return s.substr(0, dot);
}

void write_normal_png(const fs::path& path, const std::vector<float>& n, int w, int h) {
    std::vector<uint8_t> px(n.size(), 0);
    for (size_t k = 0; k * 3 < n.size(); k++) {
        const float* v = &n[k * 3];
        if (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < 0.25f) continue;
        for (int c = 0; c < 3; c++)
            px[k * 3 + c] = (uint8_t)std::lround(std::clamp(127.5f + 127.5f * v[c], 0.0f, 255.0f));
    }
    if (!stbi_write_png(path.string().c_str(), w, h, 3, px.data(), w * 3))
        throw std::runtime_error("cannot write " + path.string());
}

// Images whose name (by stem) is in `drop` taken out, with their observations;
// a point left with fewer than two goes too.
void remove_images(ColModel& m, const std::set<std::string>& drop) {
    for (auto it = m.images.begin(); it != m.images.end();) {
        if (!drop.count(stem_of(it->second.name))) { ++it; continue; }
        for (uint64_t pid : it->second.point) {
            auto p = m.points.find(pid);
            if (p == m.points.end()) continue;
            std::vector<uint32_t>& tr = p->second.track;
            for (size_t k = 0; k < tr.size();)
                if (tr[k] == it->first) tr.erase(tr.begin() + (long)k, tr.begin() + (long)k + 2);
                else k += 2;
        }
        it = m.images.erase(it);
    }
    for (auto it = m.points.begin(); it != m.points.end();) {
        if (it->second.track.size() >= 4) { ++it; continue; }
        for (size_t k = 0; k < it->second.track.size(); k += 2) {
            auto im = m.images.find(it->second.track[k]);
            if (im != m.images.end() && it->second.track[k + 1] < im->second.point.size())
                im->second.point[it->second.track[k + 1]] = ~0ull;
        }
        it = m.points.erase(it);
    }
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ (uint8_t)c) * 1099511628211ull;
    return h;
}

// FNV-1a of a file: what tells the model this step wrote from one a new
// reconstruction wrote over it.
uint64_t file_hash(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    uint64_t h = 1469598103934665603ull;
    std::vector<char> buf(1 << 20);
    while (f) {
        f.read(buf.data(), (std::streamsize)buf.size());
        for (std::streamsize i = 0; i < f.gcount(); i++)
            h = (h ^ (uint8_t)buf[(size_t)i]) * 1099511628211ull;
    }
    return h;
}

std::string hex(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
    return b;
}

// Everything the result depends on but the reconstruction: the clouds as
// files, the anchors, the options.
std::string signature(const DatasetOptions& opt, const std::string& anchors_path) {
    std::string s;
    std::error_code ec;
    for (const std::string& c : opt.clouds) {
        s += c + ":" + std::to_string(fs::file_size(c, ec)) + ":" +
             std::to_string((long long)fs::last_write_time(c, ec).time_since_epoch().count()) + ";";
    }
    s += "anchors:" + (fs::exists(anchors_path, ec) ? hex(file_hash(anchors_path)) : "none") + ";";
    s += "mode:" + std::to_string((int)opt.mode) + ";seeds:" + std::to_string(opt.seed_points) +
         (opt.all_points ? "all" : "") + ";depth:" + std::to_string(opt.depth_maps) +
         ";cap:" + std::to_string(opt.track_cap) + ";gaps:" + std::to_string(opt.sfm_points_in_gaps) +
         ";flip:" + std::to_string(opt.flip_masks) + ";images:" + opt.image_dir +
         ";scanner:" + std::to_string(opt.scanner_poses_only);
    return hex(fnv1a(s));
}

// One scan point's best `cap` observations, by a hash of (point, image) so the
// images kept are spread evenly and a rerun keeps the same ones.
struct Seen {
    uint32_t hash;
    uint32_t image;
    float x, y;
};

}  // namespace

// ================
// Anchors
// ================

std::vector<Anchor> read_anchors(const std::string& path) {
    std::vector<Anchor> out;
    const JsonValue j = json_parse_file(path);
    const JsonValue* list = j.find("anchors");
    if (!list || !list->is_array()) return out;
    for (const JsonValue& a : list->arr) {
        const JsonValue* c2w = a.find("c2w");
        const JsonValue* name = a.find("name");
        if (!c2w || !name || c2w->arr.size() != 3) continue;
        Anchor an;
        an.name = name->as_string();
        double t[3];
        bool whole = true;
        for (int r = 0; r < 3; r++) whole = whole && c2w->arr[(size_t)r].arr.size() == 4;
        if (!whole) continue;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) an.R[r * 3 + c] = c2w->arr[(size_t)r].arr[(size_t)c].as_double();
            t[r] = c2w->arr[(size_t)r].arr[3].as_double();
        }
        an.R = sfm::nearestRotation(an.R);
        an.centre = {t[0], t[1], t[2]};
        if (const JsonValue* r = a.find("rendered")) an.rendered = r->as_bool();
        if (const JsonValue* c = a.find("camera")) {
            an.camera_model = (int)c->get_double("model", -1);
            an.width = (int64_t)c->get_double("width", 0);
            an.height = (int64_t)c->get_double("height", 0);
            if (const JsonValue* ps = c->find("params"))
                for (const JsonValue& v : ps->arr) an.params.push_back(v.as_double());
        }
        out.push_back(an);
    }
    return out;
}

void write_anchors(const std::string& path, const std::vector<Anchor>& anchors) {
    JsonWriter w;
    w.object();
    w.field("frame", "scan");
    w.key("anchors").array();
    for (const Anchor& a : anchors) {
        w.object();
        w.field("name", a.name);
        std::string m = "[";
        const double t[3] = {a.centre.x, a.centre.y, a.centre.z};
        for (int r = 0; r < 3; r++) {
            m += r ? ", [" : "[";
            for (int c = 0; c < 4; c++)
                m += (c ? ", " : "") + json_number_exact(c < 3 ? a.R[r * 3 + c] : t[r]);
            m += "]";
        }
        w.field_raw("c2w", m + "]");
        if (a.rendered) w.field("rendered", true);
        if (a.camera_model >= 0) {
            w.key("camera").object();
            w.field("model", a.camera_model);
            w.field("width", (long long)a.width);
            w.field("height", (long long)a.height);
            std::string ps = "[";
            for (size_t k = 0; k < a.params.size(); k++)
                ps += (k ? ", " : "") + json_number_exact(a.params[k]);
            w.field_raw("params", ps + "]");
            w.end();
        }
        w.end();
    }
    w.end();
    w.end();
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::trunc);
    f << w.str();
    if (!f) throw std::runtime_error("cannot write " + path);
}

ExtractedPhotos extract_e57_anchors(const std::string& e57_path, const std::string& images_dir,
                                    const std::string& subdir, const std::string& anchors_path) {
    spirula::e57::Reader reader(e57_path);
    std::vector<Anchor> anchors;
    std::error_code ec;
    if (fs::exists(anchors_path, ec)) anchors = read_anchors(anchors_path);
    // A rerun replaces what this subfolder held.
    const std::string prefix = subdir + "/";
    anchors.erase(std::remove_if(anchors.begin(), anchors.end(),
                                 [&](const Anchor& a) { return a.name.rfind(prefix, 0) == 0; }),
                  anchors.end());
    const auto& images = reader.images();
    int64_t n_pin = 0, n_sph = 0;
    for (const auto& im : images) {
        E57Camera c;
        if (e57_camera(im, c) != E57Skip::None) continue;
        (std::strcmp(c.model, "PINHOLE") == 0 ? n_pin : n_sph)++;
    }
    // Pinhole faces and panoramas are different cameras to the reconstruction.
    const bool split = n_pin > 0 && n_sph > 0;
    ExtractedPhotos got;
    got.split = split;
    std::vector<double> focals, factors;
    for (size_t i = 0; i < images.size(); i++) {
        const auto& im = images[i];
        E57Camera c;
        if (e57_camera(im, c) != E57Skip::None) continue;
        const bool pin = std::strcmp(c.model, "PINHOLE") == 0;
        char stem[32];
        std::snprintf(stem, sizeof stem, "%05zu", i);
        std::string rel = subdir + "/";
        if (split) rel += pin ? "pinhole/" : "panorama/";
        rel += std::string(stem) + (im.png.empty() ? ".jpg" : ".png");
        const fs::path out = fs::path(images_dir) / rel;
        fs::create_directories(out.parent_path(), ec);
        const spirula::e57::Blob& blob = im.png.empty() ? im.jpeg : im.png;
        // An unchanged file is left alone, so a rerun does not look like new frames.
        if (fs::file_size(out, ec) != blob.length || ec) {
            const std::vector<uint8_t> bytes = reader.read_blob(blob);
            std::ofstream f(out, std::ios::binary | std::ios::trunc);
            f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
            if (!f) throw std::runtime_error("cannot write " + out.string());
        }
        Anchor a;
        a.name = rel;
        // transforms.json's OpenGL axes -> OpenCV's.
        for (int r = 0; r < 3; r++) {
            a.R[r * 3] = c.c2w[r * 4];
            a.R[r * 3 + 1] = -c.c2w[r * 4 + 1];
            a.R[r * 3 + 2] = -c.c2w[r * 4 + 2];
        }
        a.centre = {c.c2w[3], c.c2w[7], c.c2w[11]};
        a.width = c.width;
        a.height = c.height;
        if (pin) {
            a.camera_model = 1;   // COLMAP PINHOLE
            a.params = {c.fx, c.fy, c.cx, c.cy};
            focals.push_back(0.5 * (c.fx + c.fy));
            factors.push_back(0.5 * (c.fx + c.fy) / (double)c.width);
        } else {
            a.camera_model = 17;  // EQUIRECTANGULAR, (w, h)
            a.params = {(double)c.width, (double)c.height};
        }
        anchors.push_back(a);
        (pin ? got.pinhole : got.panorama)++;
    }
    write_anchors(anchors_path, anchors);
    if (!focals.empty()) {
        std::nth_element(focals.begin(), focals.begin() + focals.size() / 2, focals.end());
        got.focal = focals[focals.size() / 2];
        std::nth_element(factors.begin(), factors.begin() + factors.size() / 2, factors.end());
        got.focal_factor = factors[factors.size() / 2];
    }
    return got;
}

namespace {

// Where to stand: the scanner's own stations, else a trajectory file beside the
// cloud thinned to one position every `step` metres.
std::vector<Vec3> view_positions(const std::vector<std::string>& clouds, bool& tripod) {
    std::vector<Vec3> out;
    tripod = false;
    for (const std::string& c : clouds) {
        spirula::cloud::Reader r(c);
        for (const auto& st : r.info().stations) out.push_back({st.origin[0], st.origin[1], st.origin[2]});
    }
    if (!out.empty()) { tripod = true; return out; }
    constexpr double kStep = 2.0;
    for (const std::string& c : clouds) {
        fs::path traj = fs::path(c);
        traj.replace_extension(".trajectory" + fs::path(c).extension().string());
        std::error_code ec;
        if (!fs::exists(traj, ec)) continue;
        spirula::cloud::Reader r(traj.string());
        r.read([&](const spirula::e57::Point* p, size_t n) {
            for (size_t i = 0; i < n; i++) {
                const Vec3 x{p[i].xyz[0], p[i].xyz[1], p[i].xyz[2]};
                if (out.empty() || (x - out.back()).norm() >= kStep) out.push_back(x);
            }
        });
    }
    return out;
}

}  // namespace

int64_t render_anchor_views(const std::vector<std::string>& clouds, const std::string& images_dir,
                            const std::string& subdir, const std::string& anchors_path,
                            const std::function<void(const std::string&)>& log,
                            const std::atomic<bool>* cancel) {
    bool tripod = false;
    const std::vector<Vec3> where = view_positions(clouds, tripod);
    if (where.empty()) return 0;
    std::vector<double> xyz;
    std::vector<uint8_t> rgb;
    int64_t total = 0;
    for (const std::string& c : clouds) total += spirula::cloud::Reader(c).info().points;
    const uint64_t below = draw_threshold(kDrawCap, total);
    uint64_t serial = 0;
    for (const std::string& c : clouds) {
        log(format(lmsg::reading_cloud, {c}));
        spirula::cloud::Reader r(c);
        if (!r.read([&](const spirula::e57::Point* p, size_t n) {
                for (size_t i = 0; i < n; i++) {
                    if (splitmix64(serial++) > below) continue;
                    xyz.insert(xyz.end(), p[i].xyz, p[i].xyz + 3);
                    rgb.insert(rgb.end(), p[i].rgb, p[i].rgb + 3);
                }
            }, cancel))
            return 0;
    }
    const ScanCloud cloud = build_scan_cloud(xyz, rgb, 2.0);
    std::vector<double>().swap(xyz);
    std::vector<uint8_t>().swap(rgb);

    std::vector<Anchor> anchors;
    std::error_code ec;
    if (fs::exists(anchors_path, ec)) anchors = read_anchors(anchors_path);
    const std::string prefix = subdir + "/";
    anchors.erase(std::remove_if(anchors.begin(), anchors.end(),
                                 [&](const Anchor& a) { return a.name.rfind(prefix, 0) == 0; }),
                  anchors.end());
    fs::remove_all(fs::path(images_dir) / subdir, ec);
    fs::create_directories(fs::path(images_dir) / subdir, ec);
    // Level views all round, and a ring tilted up for the facades above a
    // tripod; a walk's positions are many, so each gets fewer.
    const int yaws = tripod ? 8 : 4;
    const std::vector<double> pitches = tripod ? std::vector<double>{0.0, 30.0}
                                               : std::vector<double>{0.0};
    struct View { Vec3 c; Mat3 R; std::string name; };
    std::vector<View> views;
    for (size_t si = 0; si < where.size(); si++)
        for (double pitch : pitches)
            for (int yi = 0; yi < yaws; yi++) {
                const double yaw = 2 * kPi * yi / yaws, pt = pitch * kPi / 180.0;
                const Vec3 fw{std::cos(pt) * std::cos(yaw), std::cos(pt) * std::sin(yaw), std::sin(pt)};
                const Vec3 right = fw.cross({0, 0, 1}).normalized();
                const Vec3 down = fw.cross(right);
                View v;
                v.c = where[si];
                v.R = {right.x, down.x, fw.x, right.y, down.y, fw.y, right.z, down.z, fw.z};
                char name[64];
                std::snprintf(name, sizeof name, "%s/%04zu_%02d_%03d.jpg", subdir.c_str(), si,
                              (int)pitch, yi * 360 / yaws);
                v.name = name;
                views.push_back(v);
            }
    constexpr int kSide = 1024;
    int64_t written = 0;
    std::mutex mu;
#pragma omp parallel for schedule(dynamic, 1)
    for (int k = 0; k < (int)views.size(); k++) {
        if (cancel && cancel->load()) continue;
        const View& v = views[(size_t)k];
        MapCamera m;
        m.model = (int)CameraModelType::PINHOLE;
        m.width = m.height = kSide;
        m.fx = m.fy = kSide / 2.0;
        m.cx = m.cy = kSide / 2.0;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) m.c2w[r * 4 + c] = v.R[r * 3 + c];
            m.c2w[r * 4 + 3] = r == 0 ? v.c.x : r == 1 ? v.c.y : v.c.z;
        }
        std::vector<uint8_t> px;
        render_color(cloud, m, px);
        // A view mostly of nothing (into the sky, or inside a wall) is no anchor.
        int64_t lit = 0;
        for (size_t j = 0; j < px.size(); j += 3) lit += px[j] | px[j + 1] | px[j + 2];
        if (lit < (int64_t)kSide * kSide / 4) continue;
        const fs::path out = fs::path(images_dir) / v.name;
        if (!stbi_write_jpg(out.string().c_str(), kSide, kSide, 3, px.data(), 95)) continue;
        Anchor a;
        a.name = v.name;
        a.R = v.R;
        a.centre = v.c;
        a.rendered = true;
        std::lock_guard<std::mutex> lk(mu);
        anchors.push_back(a);
        written++;
    }
    std::sort(anchors.begin(), anchors.end(),
              [](const Anchor& a, const Anchor& b) { return a.name < b.name; });
    write_anchors(anchors_path, anchors);
    return written;
}

// ================
// The dataset
// ================

DatasetResult write_lidar_dataset(const DatasetOptions& opt,
                                  const std::function<void(const std::string&)>& log,
                                  const std::atomic<bool>* cancel) {
    DatasetResult res;
    auto cancelled = [&] { return cancel && cancel->load(); };
    const fs::path ds(opt.dataset);
    std::error_code ec;

    const std::string anchors_path =
        opt.anchors.empty() ? (ds / "lidar" / "anchors.json").string() : opt.anchors;
    std::vector<Anchor> anchors;
    if (fs::exists(anchors_path, ec)) anchors = read_anchors(anchors_path);
    if (!anchors.empty()) log(format(lmsg::anchors_read, {(long long)anchors.size()}));
    bool placeable = false;
    for (const Anchor& a : anchors) placeable = placeable || (a.camera_model >= 0 && !a.rendered);

    // Nothing moves until the result is complete in sparse.lidar_tmp/: a run
    // killed half way leaves the dataset as it was. sparse/0 is ours only
    // while its images.bin is the one we wrote.
    const fs::path sparse = ds / "sparse", kept = ds / kUnalignedDir;
    const fs::path marker = sparse / "0" / kAlignedMarker;
    JsonValue last;
    if (fs::exists(marker, ec)) {
        try {
            last = json_parse_file(marker.string());
        } catch (const std::exception&) {
        }
    }
    const JsonValue* last_hash = last.find("images_bin");
    const bool ours = fs::exists(marker, ec) &&
                      (!last_hash || last_hash->as_string() ==
                                         hex(file_hash(sparse / "0" / "images.bin")));
    // An export's model written flat into sparse/ is read where it is and
    // never moved; numbered models not yet aligned are set aside at the end.
    const bool flat = fs::exists(sparse / "cameras.bin", ec);
    std::vector<fs::path> fresh = numbered_models(sparse);
    if (ours) fresh.erase(std::remove(fresh.begin(), fresh.end(), sparse / "0"), fresh.end());
    std::vector<fs::path> sources;
    if (!fresh.empty()) sources = fresh;
    else if (fs::exists(kept, ec)) sources = model_dirs(kept);
    else if (flat) sources = {sparse};
    else if (!ours && fs::exists(sparse, ec) && holds_files(sparse))
        throw std::runtime_error(format(lmsg::err_unreadable_model, {sparse.string()}));
    if (opt.scanner_poses_only) sources.clear();
    const std::string sig = signature(opt, anchors_path);
    if (ours && fresh.empty() && !opt.overwrite) {
        const JsonValue* last_sig = last.find("signature");
        if (last_sig && last_sig->as_string() == sig) {
            log(format(lmsg::reused, {(sparse / "0").string()}));
            res.images = (int64_t)read_model(sparse / "0").images.size();
            return res;
        }
    }
    std::vector<ColModel> models;
    for (const fs::path& d : sources) models.push_back(read_model(d));
    if (models.empty() && !placeable)
        throw std::runtime_error(format(lmsg::err_no_model, {ds.string()}));
    res.models = (int)models.size();

    // ---- one pass over the clouds ----
    int64_t total = 0;
    bool any_color = false;
    std::vector<std::unique_ptr<spirula::cloud::Reader>> readers;
    for (const std::string& c : opt.clouds) {
        readers.push_back(std::make_unique<spirula::cloud::Reader>(c));
        total += readers.back()->info().points;
        any_color = any_color || readers.back()->info().has_color;
    }
    const uint64_t draw_below = draw_threshold(kDrawCap, total);
    std::vector<double> draw_xyz, all_xyz;
    std::vector<uint8_t> draw_rgb, all_rgb;
    uint64_t serial = 0;
    for (size_t k = 0; k < readers.size(); k++) {
        log(format(lmsg::reading_cloud, {opt.clouds[k]}));
        const bool whole = readers[k]->read([&](const spirula::e57::Point* p, size_t n) {
            for (size_t i = 0; i < n; i++) {
                if (opt.all_points) {
                    all_xyz.insert(all_xyz.end(), p[i].xyz, p[i].xyz + 3);
                    all_rgb.insert(all_rgb.end(), p[i].rgb, p[i].rgb + 3);
                }
                if (splitmix64(serial++) > draw_below) continue;
                draw_xyz.insert(draw_xyz.end(), p[i].xyz, p[i].xyz + 3);
                draw_rgb.insert(draw_rgb.end(), p[i].rgb, p[i].rgb + 3);
            }
        }, cancel);
        if (!whole) { res.cancelled = true; return res; }
    }
    readers.clear();
    const size_t n_draw = draw_xyz.size() / 3;
    if (n_draw == 0) throw std::runtime_error(lmsg::err_no_points.get());

    // Random subsets of the draw: every k-th by a hash, so they are repeatable.
    auto subset = [&](int64_t want, std::vector<double>& xyz, std::vector<uint8_t>& rgb,
                      uint64_t salt) {
        const uint64_t below = draw_threshold(want, (int64_t)n_draw);
        for (size_t i = 0; i < n_draw; i++) {
            if (splitmix64(i ^ salt) > below) continue;
            xyz.insert(xyz.end(), &draw_xyz[i * 3], &draw_xyz[i * 3] + 3);
            rgb.insert(rgb.end(), &draw_rgb[i * 3], &draw_rgb[i * 3] + 3);
        }
    };
    std::vector<double> a_xyz;
    std::vector<uint8_t> a_rgb;
    subset(kAlignDraw, a_xyz, a_rgb, 0x51ED270B);
    const AlignCloud cloud = make_align_cloud(a_xyz, a_rgb, kAlignTarget);
    log(format(lmsg::cloud_read, {(long long)serial, (long long)cloud.size(),
                                  fixed(cloud.voxel, 3)}));

    // ---- align each model ----
    std::vector<int> aligned;
    std::vector<sfm::Sim3> transforms(models.size());
    double worst_centre = 0, worst_rot = 0;
    double best_residual = 0;
    size_t largest = 0;
    for (size_t mi = 0; mi < models.size(); mi++) {
        if (cancelled()) { res.cancelled = true; return res; }
        const ModelGeometry g = geometry_of(models[mi]);
        if (g.views.empty()) continue;
        log(format(lmsg::model_head, {(long long)mi, (long long)g.views.size(),
                                      (long long)g.points.size()}));
        sfm::Sim3 T;
        IcpStats st;
        if (opt.mode == AlignMode::Refine) {
            T = refine_icp(g, cloud, T, &st);
            if (st.iterations > 0)
                log(format(lmsg::model_icp, {fixed(st.median_m, 3),
                                             fixed(100.0 * st.inlier_frac, 1)}));
        } else if (opt.mode == AlignMode::Keep) {
            const std::vector<float> d = scan_distances(g, cloud, T, 1.0);
            std::vector<float> v;
            for (float x : d) if (x == x) v.push_back(x);
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            log(format(lmsg::model_kept, {(long long)mi, fixed(v.empty() ? 0.0 : v[v.size() / 2], 3)}));
        } else {
            const AnchorFit fit = fit_anchors(g, anchors);
            if (!fit.ok) {
                log(format(lmsg::model_dropped, {(long long)mi}));
                continue;
            }
            log(format(lmsg::model_anchors, {(long long)fit.registered, (long long)fit.inliers,
                                             fixed(fit.rot_err_deg, 2),
                                             fixed(fit.centre_err_m, 3)}));
            worst_centre = std::max(worst_centre, fit.centre_err_m);
            worst_rot = std::max(worst_rot, fit.rot_err_deg);
            T = fit.T;
            if (opt.mode == AlignMode::Auto) {
                const sfm::Sim3 Ti = refine_icp(g, cloud, T, &st);
                // The fit to the surface should keep the anchors where they were.
                double spread = 0, drift = 0;
                int n = 0;
                for (const Anchor& a : anchors) {
                    const int v = find_view(g, a.name);
                    if (v < 0) continue;
                    const Vec3 c0 = sfm::transformPoint(T, g.views[(size_t)v].centre);
                    const Vec3 c1 = sfm::transformPoint(Ti, g.views[(size_t)v].centre);
                    drift += (c1 - c0).norm();
                    n++;
                    for (const Anchor& b : anchors) spread = std::max(spread, (a.centre - b.centre).norm());
                }
                drift = n ? drift / n : 0;
                if (st.iterations == 0) {
                    // Too few points to fit a surface to: the anchors' fit stands.
                } else if (fit.scale_known && drift > std::max(0.1, kAnchorDrift * spread) &&
                           drift > 5 * std::max(fit.centre_err_m, 0.02)) {
                    log(format(lmsg::model_icp_rejected, {fixed(drift, 3)}));
                } else {
                    T = Ti;
                    log(format(lmsg::model_icp, {fixed(st.median_m, 3),
                                                 fixed(100.0 * st.inlier_frac, 1)}));
                }
            }
        }
        const double ang = std::acos(std::clamp((T.R[0] + T.R[4] + T.R[8] - 1.0) / 2.0, -1.0, 1.0));
        log(format(lmsg::model_transform, {fixed(T.scale, 6), fixed(ang * 180.0 / kPi, 3),
                                           fixed(T.t.x, 3), fixed(T.t.y, 3), fixed(T.t.z, 3)}));
        transforms[mi] = T;
        if (aligned.empty() || g.views.size() > models[largest].images.size()) {
            largest = mi;
            best_residual = st.median_m;
        }
        aligned.push_back((int)mi);
    }
    res.aligned = (int)aligned.size();
    if (aligned.empty() && !placeable) throw std::runtime_error(lmsg::err_none_aligned.get());

    // ---- one model in the scan's frame ----
    ColModel out;
    uint32_t next_cam = 1, next_img = 1;
    uint64_t next_pt = 1;
    std::set<std::string> merged_names;
    for (int mi : aligned) {
        ColModel& m = models[(size_t)mi];
        // Models may share images; the larger one, earlier, keeps them.
        std::set<std::string> dup;
        for (const auto& [id, im] : m.images)
            if (merged_names.count(stem_of(im.name))) dup.insert(stem_of(im.name));
        remove_images(m, dup);
        for (const auto& [id, im] : m.images) merged_names.insert(stem_of(im.name));
        transform_model(m, transforms[(size_t)mi]);
        std::map<uint32_t, uint32_t> cam_id, img_id;
        for (auto& [id, c] : m.cameras) { cam_id[id] = next_cam; out.cameras[next_cam++] = c; }
        for (auto& [id, im] : m.images) img_id[id] = next_img++;
        std::map<uint64_t, uint64_t> pt_id;
        for (auto& [id, p] : m.points) pt_id[id] = next_pt++;
        for (auto& [id, im] : m.images) {
            ColImage n = im;
            n.camera = cam_id[im.camera];
            for (uint64_t& p : n.point)
                if (p != ~0ull) {
                    auto it = pt_id.find(p);
                    p = it == pt_id.end() ? ~0ull : it->second;
                }
            out.images[img_id[id]] = std::move(n);
        }
        for (auto& [id, p] : m.points) {
            ColPoint n = p;
            for (size_t k = 0; k < n.track.size(); k += 2) n.track[k] = img_id[n.track[k]];
            out.points[pt_id[id]] = std::move(n);
        }
    }
    models.clear();
    // The scan's own renders anchored the fit; they are not photographs.
    std::set<std::string> rendered;
    for (const Anchor& a : anchors)
        if (a.rendered) rendered.insert(stem_of(a.name));
    remove_images(out, rendered);
    // Photographs the reconstruction did not place go where the scanner says,
    // unless the anchors disagree with the fit (Oxford: faces of one station
    // 0.5 m apart in the SfM) and they would not line up with it.
    const fs::path image_root = fs::path(opt.image_dir).is_absolute()
                                    ? fs::path(opt.image_dir) : ds / opt.image_dir;
    {
        std::set<std::string> have;
        for (const auto& [id, im] : out.images) have.insert(stem_of(im.name));
        std::map<std::vector<double>, uint32_t> cams;
        int64_t placed = 0, skipped = 0;
        for (const Anchor& a : anchors) {
            if (a.rendered || a.camera_model < 0 || have.count(stem_of(a.name))) continue;
            if (!aligned.empty() && (worst_centre > kPlaceCentre || worst_rot > kPlaceRot)) {
                skipped++;
                continue;
            }
            // The import may have re-encoded it (a PNG becomes a JPEG).
            const std::string file = resolve_image(image_root, a.name);
            if (file.empty()) {
                skipped++;
                continue;
            }
            std::vector<double> key = a.params;
            key.push_back(a.camera_model);
            key.push_back((double)a.width);
            key.push_back((double)a.height);
            auto it = cams.find(key);
            if (it == cams.end()) {
                ColCamera c;
                c.model = a.camera_model;
                c.width = (uint64_t)a.width;
                c.height = (uint64_t)a.height;
                c.params = a.params;
                out.cameras[next_cam] = c;
                it = cams.emplace(key, next_cam++).first;
            }
            ColImage im;
            im.camera = it->second;
            im.name = file;
            const Mat3 Rw2c = sfm::transpose(a.R);
            R_to_quat(Rw2c, im.q);
            const Vec3 t = sfm::mul(Rw2c, a.centre) * -1.0;
            im.t[0] = t.x; im.t[1] = t.y; im.t[2] = t.z;
            out.images[next_img++] = std::move(im);
            placed++;
        }
        if (placed) log(format(lmsg::placed_by_scanner, {(long long)placed}));
        if (skipped)
            log(format(lmsg::not_placed, {(long long)skipped, fixed(worst_centre, 3)}));
    }
    res.images = (int64_t)out.images.size();
    log(format(lmsg::aligned_summary, {(long long)res.aligned, (long long)res.models,
                                       (long long)res.images}));
    res.residual_m = best_residual;
    const fs::path tmp = ds / "sparse.lidar_tmp";
    const fs::path model_dir = tmp / "0";
    fs::remove_all(tmp, ec);
    write_model(out, model_dir);

    // ---- the cameras as the trainer reads them ----
    DatasetParserConfig pc;
    pc.recon_dir = "sparse.lidar_tmp/0";
    pc.image_dir = opt.image_dir;
    pc.probe_image_size = probe_image_size;
    const ParsedDataset pd = parse_colmap_dataset(ds.string(), pc);
    const fs::path image_canon = fs::weakly_canonical(image_root, ec);
    std::map<std::string, uint32_t> by_stem;
    for (const auto& [id, im] : out.images) by_stem[stem_of(im.name)] = id;
    struct Frame {
        uint32_t image;
        size_t parsed;
        std::string rel;
    };
    std::vector<Frame> frames;
    for (size_t i = 0; i < pd.image_filenames.size(); i++) {
        std::string rel = fs::weakly_canonical(pd.image_filenames[i], ec)
                              .lexically_relative(image_canon).generic_string();
        auto it = by_stem.find(stem_of(rel));
        if (it != by_stem.end()) frames.push_back({it->second, i, rel});
    }
    int64_t n_ray = 0;
    for (const Frame& f : frames)
        n_ray += camhost::splits_to_pinhole_faces(pd.camera_models[f.parsed], pd.widths[f.parsed],
                                                  pd.heights[f.parsed], pd.intrins[f.parsed * 4],
                                                  pd.intrins[f.parsed * 4 + 1]);
    const bool ray = n_ray * 2 > (int64_t)frames.size();

    // ---- the seed points ----
    std::vector<double> seed_xyz;
    std::vector<uint8_t> seed_rgb;
    double voxel = 0;
    if (opt.all_points) {
        seed_xyz.swap(all_xyz);
        seed_rgb.swap(all_rgb);
    } else if (opt.seed_points > 0) {
        subset(std::clamp<int64_t>(8 * opt.seed_points, 8000000, kDrawCap), seed_xyz, seed_rgb,
               0x5EED5EED);
        voxel = thin_to_voxels(seed_xyz, seed_rgb, opt.seed_points);
    }
    const size_t n_seed = seed_xyz.size() / 3;
    const ScanCloud depth_cloud = build_scan_cloud(draw_xyz, draw_rgb, 2.0);
    std::vector<double>().swap(draw_xyz);
    std::vector<uint8_t>().swap(draw_rgb);
    if (!any_color && n_seed) log(lmsg::colorizing.get());

    // ---- per image: depth and normals, who sees which scan point, and
    // whether the scan has anything where the model's own points are ----
    // Every point of a big scan with full tracks would not fit in memory.
    const int cap = (int)std::clamp<int64_t>(
        std::min<int64_t>(opt.track_cap, (int64_t)(2e9 / 16) / std::max<int64_t>(1, (int64_t)n_seed)),
        1, 64);
    std::vector<Seen> seen(n_seed * (size_t)cap, Seen{UINT32_MAX, 0, 0, 0});
    std::vector<uint8_t> seen_n(n_seed, 0);
    std::vector<uint32_t> color_sum(any_color ? 0 : n_seed * 4, 0);
    constexpr size_t kStripes = 4096;
    std::vector<std::mutex> stripes(kStripes);
    std::map<uint64_t, std::pair<int, int>> gap_votes;   // point -> (no scan, scan)
    std::mutex mu;
    std::exception_ptr failure;
    int64_t done = 0;
    if (opt.depth_maps) {
        fs::create_directories(ds / "depths", ec);
        fs::create_directories(ds / "normals", ec);
    }
    const int nf = (int)frames.size();
#pragma omp parallel for schedule(dynamic, 1)
    for (int fi = 0; fi < nf; fi++) {
        if (cancelled()) continue;
        {
            std::lock_guard<std::mutex> lk(mu);
            if (failure) continue;
        }
        try {
            const Frame& f = frames[(size_t)fi];
            const ColImage& im = out.images.at(f.image);
            const size_t pi = f.parsed;
            const int W = pd.widths[pi], H = pd.heights[pi];
            const double scale = std::min(1.0, (double)kMapSide / (double)std::max(W, H));
            MapCamera m;
            m.model = pd.camera_models[pi];
            m.tier = pd.camera_distortions[pi];
            for (int k = 0; k < 8; k++) m.dist[k] = pd.dist_coeffs[pi * 8 + k];
            m.width = std::max(1, (int)std::lround(W * scale));
            m.height = std::max(1, (int)std::lround(H * scale));
            const double sx = (double)m.width / W, sy = (double)m.height / H;
            m.fx = pd.intrins[pi * 4] * sx;
            m.fy = pd.intrins[pi * 4 + 1] * sy;
            m.cx = pd.intrins[pi * 4 + 2] * sx;
            m.cy = pd.intrins[pi * 4 + 3] * sy;
            const Mat3 Rw2c = quat_to_R(im.q);
            const Mat3 Rc2w = sfm::transpose(Rw2c);
            const Vec3 C = sfm::mul(Rc2w, Vec3{im.t[0], im.t[1], im.t[2]}) * -1.0;
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) m.c2w[r * 4 + c] = Rc2w[r * 3 + c];
                m.c2w[r * 4 + 3] = r == 0 ? C.x : r == 1 ? C.y : C.z;
            }
            std::vector<float> depth, normal;
            render_depth_normal(depth_cloud, m, ray, depth, normal);
            const std::string stem = stem_of(f.rel);
            if (opt.depth_maps) {
                std::vector<uint16_t> mm(depth.size(), 0);
                for (size_t j = 0; j < depth.size(); j++) {
                    const double v = depth[j] * 1000.0;
                    if (depth[j] > 0 && v <= 65535.0)
                        mm[j] = (uint16_t)std::max<long>(1, std::lround(v));
                }
                const fs::path dp = ds / "depths" / (stem + ".png");
                const fs::path np = ds / "normals" / (stem + ".png");
                fs::create_directories(dp.parent_path());
                fs::create_directories(np.parent_path());
                if (!save_depth_png16(dp.string(), mm.data(), m.width, m.height))
                    throw std::runtime_error("cannot write " + dp.string());
                write_normal_png(np, normal, m.width, m.height);
            }
            auto depth_at = [&](double px, double py) {
                const int x = (int)px, y = (int)py;
                if (x < 0 || y < 0 || x >= m.width || y >= m.height) return -1.0f;
                return depth[(size_t)y * m.width + x];
            };
            // The model's points this image sees: does the scan have a surface there?
            std::vector<std::pair<uint64_t, bool>> votes;
            for (size_t k = 0; k < im.point.size(); k++) {
                if (im.point[k] == ~0ull) continue;
                const float d = depth_at(im.xy[k * 2] * sx, im.xy[k * 2 + 1] * sy);
                if (d >= 0) votes.push_back({im.point[k], d > 0});
            }
            // The scan points this image sees.
            unsigned char* rgb = nullptr;
            int iw = 0, ih = 0, ic = 0;
            std::vector<uint8_t> keep;
            if (!any_color && n_seed) {
                rgb = stbi_load(pd.image_filenames[pi].c_str(), &iw, &ih, &ic, 3);
                if (!pd.mask_filenames.empty() && !pd.mask_filenames[pi].empty()) {
                    int mw = 0, mh = 0, mc = 0;
                    unsigned char* mk = stbi_load(pd.mask_filenames[pi].c_str(), &mw, &mh, &mc, 1);
                    if (mk && mw == iw && mh == ih) keep.assign(mk, mk + (size_t)mw * mh);
                    if (mk) stbi_image_free(mk);
                }
            }
            const Vec3 o{depth_cloud.origin[0], depth_cloud.origin[1], depth_cloud.origin[2]};
            for (size_t s = 0; s < n_seed; s++) {
                const Vec3 X{seed_xyz[s * 3], seed_xyz[s * 3 + 1], seed_xyz[s * 3 + 2]};
                const Vec3 p = sfm::mul(Rw2c, X - C);
                const double range = p.norm();
                if (m.model == (int)CameraModelType::PINHOLE && p.z < 0.01) continue;
                const double pr[3] = {p.x, p.y, p.z};
                double uv[2];
                if (!camhost::project_ray(pr, m.model, m.tier, m.dist, uv)) continue;
                const double px = m.fx * uv[0] + m.cx, py = m.fy * uv[1] + m.cy;
                const float d = depth_at(px, py);
                if (!(d > 0)) continue;
                const double want = ray ? range : p.z;
                if (std::fabs(want - d) > kSeenRatio * d + kSeenSlack) continue;
                const float ox = (float)(px / sx), oy = (float)(py / sy);
                const uint32_t h = (uint32_t)splitmix64(((uint64_t)s << 24) ^ f.image);
                std::lock_guard<std::mutex> lk(stripes[s % kStripes]);
                Seen* slot = &seen[s * (size_t)cap];
                uint8_t& cnt = seen_n[s];
                if (cnt < 255) cnt++;
                int worst = 0;
                for (int k = 1; k < cap; k++)
                    if (slot[k].hash > slot[worst].hash) worst = k;
                if (h < slot[worst].hash) slot[worst] = Seen{h, f.image, ox, oy};
                if (rgb) {
                    const int x = std::clamp((int)ox, 0, iw - 1), y = std::clamp((int)oy, 0, ih - 1);
                    if (keep.empty() || (keep[(size_t)y * iw + x] >= 128) != opt.flip_masks) {
                        const unsigned char* c = rgb + ((size_t)y * iw + x) * 3;
                        uint32_t* acc = &color_sum[s * 4];
                        acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]; acc[3]++;
                    }
                }
            }
            if (rgb) stbi_image_free(rgb);
            (void)o;
            std::lock_guard<std::mutex> lk(mu);
            for (auto& [id, has] : votes) (has ? gap_votes[id].second : gap_votes[id].first)++;
            res.depth_maps += opt.depth_maps;
            log(format(lmsg::progress_maps, {(long long)++done, (long long)nf}));
        } catch (...) {
            std::lock_guard<std::mutex> lk(mu);
            if (!failure) failure = std::current_exception();
        }
    }
    if (failure) std::rethrow_exception(failure);
    if (cancelled()) { res.cancelled = true; return res; }

    // ---- the final model: the reconstruction's points where the scan has
    // none, then the scan's points with their tracks ----
    if (n_seed > 0 || !opt.sfm_points_in_gaps) {
        for (auto it = out.points.begin(); it != out.points.end();) {
            const auto v = gap_votes.find(it->first);
            const bool gap = v != gap_votes.end() && v->second.first > v->second.second;
            if (opt.sfm_points_in_gaps && gap) { ++it; continue; }
            for (size_t k = 0; k < it->second.track.size(); k += 2) {
                auto im = out.images.find(it->second.track[k]);
                if (im != out.images.end() && it->second.track[k + 1] < im->second.point.size())
                    im->second.point[it->second.track[k + 1]] = ~0ull;
            }
            it = out.points.erase(it);
        }
    }
    res.sfm_points = (int64_t)out.points.size();
    int64_t observed = 0, observations = 0;
    for (size_t s = 0; s < n_seed; s++) {
        ColPoint p;
        for (int a = 0; a < 3; a++) p.xyz[a] = seed_xyz[s * 3 + a];
        if (any_color) {
            for (int a = 0; a < 3; a++) p.rgb[a] = seed_rgb[s * 3 + a];
        } else {
            const uint32_t* acc = &color_sum[s * 4];
            for (int a = 0; a < 3; a++)
                p.rgb[a] = acc[3] ? (uint8_t)(acc[a] / acc[3]) : seed_rgb[s * 3 + a];
        }
        const uint64_t id = next_pt++;
        const Seen* slot = &seen[s * (size_t)cap];
        for (int k = 0; k < cap; k++) {
            if (slot[k].hash == UINT32_MAX) continue;
            ColImage& im = out.images[slot[k].image];
            p.track.push_back(slot[k].image);
            p.track.push_back((uint32_t)im.point.size());
            im.xy.push_back(slot[k].x);
            im.xy.push_back(slot[k].y);
            im.point.push_back(id);
            observations++;
        }
        observed += !p.track.empty();
        out.points[id] = std::move(p);
    }
    res.seed_points = (int64_t)n_seed;
    log(format(lmsg::seed_summary, {(long long)n_seed, fixed(voxel, 3), (long long)res.sfm_points}));
    if (n_seed)
        log(format(lmsg::tracks_summary, {fixed(100.0 * observed / (double)n_seed, 1),
                                          (long long)observations}));
    write_model(out, model_dir);

    {
        std::ofstream f(model_dir / "gauge.txt", std::ios::trunc);
        f << "# The frame of the laser scan this model was aligned to.\n"
             "oriented 1\nmetric 1\nup scan\nscale scan\n";
    }
    {
        JsonWriter w;
        w.object();
        w.key("clouds").array();
        for (const std::string& c : opt.clouds) w.value(c);
        w.end();
        w.field("mode", opt.mode == AlignMode::Keep ? "keep"
                        : opt.mode == AlignMode::Refine ? "refine"
                        : opt.mode == AlignMode::Anchors ? "anchors" : "auto");
        w.key("models").array();
        for (int mi : aligned) {
            const sfm::Sim3& T = transforms[(size_t)mi];
            w.object();
            w.field("index", mi);
            w.field_raw("scale", json_number_exact(T.scale));
            std::string r = "[";
            for (int k = 0; k < 9; k++) r += (k ? ", " : "") + json_number_exact(T.R[k]);
            w.field_raw("rotation", r + "]");
            w.field_raw("translation", "[" + json_number_exact(T.t.x) + ", " +
                                           json_number_exact(T.t.y) + ", " +
                                           json_number_exact(T.t.z) + "]");
            w.end();
        }
        w.end();
        w.field("residual_m", res.residual_m);
        w.field("signature", sig);
        w.field("images_bin", hex(file_hash(model_dir / "images.bin")));
        w.end();
        std::ofstream f(model_dir / kAlignedMarker, std::ios::trunc);
        f << w.str();
    }
    // The result is complete: set the reconstruction it came from aside (never
    // an export's flat files), then put it in place.
    if (!fresh.empty()) {
        fs::remove_all(kept, ec);
        fs::create_directories(kept, ec);
        for (const fs::path& d : fresh) {
            fs::rename(d, kept / d.filename(), ec);
            if (ec) throw std::runtime_error("cannot move " + d.string() + ": " + ec.message());
        }
    } else if (ours) {
        fs::remove_all(sparse / "0", ec);
    }
    fs::create_directories(sparse, ec);
    fs::rename(model_dir, sparse / "0", ec);
    if (ec) throw std::runtime_error("cannot write " + (sparse / "0").string() + ": " + ec.message());
    fs::remove_all(tmp, ec);
    log(format(lmsg::done, {ds.string(), (long long)res.images,
                            (long long)out.points.size()}));
    return res;
}

}  // namespace app::lidar
