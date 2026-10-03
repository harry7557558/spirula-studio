#include "sfm/core/FixedPoses.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace sfm {

namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p.filename().string());
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

struct Bytes {
    const std::string& s;
    const char* file;
    size_t at = 0;

    [[noreturn]] void truncated() const {
        throw std::runtime_error(std::string(file) + " is truncated");
    }
    template <class T>
    T take() {
        if (s.size() - at < sizeof(T)) truncated();
        T v;
        std::memcpy(&v, s.data() + at, sizeof(T));
        at += sizeof(T);
        return v;
    }
    std::string cstr() {
        const size_t end = s.find('\0', at);
        if (end == std::string::npos) truncated();
        std::string v = s.substr(at, end - at);
        at = end + 1;
        return v;
    }
    void skip(uint64_t count, size_t each) {
        if (count > (s.size() - at) / each) truncated();
        at += (size_t)count * each;
    }
};

// COLMAP's SIMPLE_RADIAL (2), SIMPLE_RADIAL_FISHEYE (8) and RADIAL_FISHEYE (9)
// are RADIAL and OPENCV_FISHEYE with the coefficients they lack at zero.
int colmapParamCount(int32_t model) {
    switch (model) {
        case 2: case 8: return 4;
        case 9: return 5;
        default: return camColmapParams(camFromColmapId(model));
    }
}

Camera fixedCamera(uint32_t id, int32_t model, uint64_t w, uint64_t h, const double* p) {
    if (w == 0 || h == 0 || w > (uint64_t)INT_MAX || h > (uint64_t)INT_MAX)
        throw std::runtime_error("cameras.bin: camera " + std::to_string(id) + " has size " +
                                 std::to_string(w) + "x" + std::to_string(h));
    Camera c;
    c.id = id;
    c.width = (int)w;
    c.height = (int)h;
    if (model == 2) {
        c.model = CamModel::Radial;
        const double q[5] = {p[0], p[1], p[2], p[3], 0.0};
        unpackColmap(c, q);
    } else if (model == 8 || model == 9) {
        c.model = CamModel::OpenCVFisheye;
        const double q[8] = {p[0], p[0], p[1], p[2], p[3], model == 9 ? p[4] : 0.0, 0.0, 0.0};
        unpackColmap(c, q);
    } else {
        c.model = camFromColmapId(model);
        unpackColmap(c, p);
    }
    return c;
}

FixedImage takeImage(Bytes& b) {
    FixedImage im;
    im.id = b.take<uint32_t>();
    for (uint64_t& v : im.pose_bits) v = b.take<uint64_t>();
    im.camera_id = b.take<uint32_t>();
    im.name = b.cstr();
    b.skip(b.take<uint64_t>(), 2 * sizeof(double) + sizeof(uint64_t));
    return im;
}

}  // namespace

Pose FixedImage::pose() const {
    double v[7];
    std::memcpy(v, pose_bits, sizeof v);
    return {quaternionToRotation({v[0], v[1], v[2], v[3]}), {v[4], v[5], v[6]}};
}

FixedPoses readFixedPoses(const std::string& dir) {
    const fs::path d(dir);
    std::error_code ec;
    for (const char* f : {"cameras", "images"})
        if (!fs::exists(d / (std::string(f) + ".bin"), ec) &&
            fs::exists(d / (std::string(f) + ".txt"), ec))
            throw std::runtime_error(std::string(f) +
                                     ".txt: only a binary model is read; save it as .bin");
    FixedPoses fp;
    fp.cameras_bin = slurp(d / "cameras.bin");
    {
        Bytes b{fp.cameras_bin, "cameras.bin"};
        const uint64_t n = b.take<uint64_t>();
        for (uint64_t i = 0; i < n; i++) {
            const uint32_t id = b.take<uint32_t>();
            const int32_t model = b.take<int32_t>();
            const uint64_t w = b.take<uint64_t>(), h = b.take<uint64_t>();
            int np = 0;
            try {
                np = colmapParamCount(model);
            } catch (const std::exception& e) {
                throw std::runtime_error("cameras.bin: camera " + std::to_string(id) + ": " +
                                         e.what());
            }
            double p[16] = {};
            for (int k = 0; k < np; k++) p[k] = b.take<double>();
            if (!fp.cameras.emplace(id, fixedCamera(id, model, w, h, p)).second)
                throw std::runtime_error("cameras.bin: camera " + std::to_string(id) +
                                         " appears twice");
        }
    }
    const std::string images = slurp(d / "images.bin");
    Bytes b{images, "images.bin"};
    const uint64_t n = b.take<uint64_t>();
    std::set<uint32_t> ids;
    std::set<std::string> names;
    for (uint64_t i = 0; i < n; i++) {
        FixedImage im = takeImage(b);
        const std::string what =
            "images.bin: image " + std::to_string(im.id) + " ('" + im.name + "')";
        if (!fp.cameras.count(im.camera_id))
            throw std::runtime_error(what + " uses camera " + std::to_string(im.camera_id) +
                                     ", which cameras.bin does not have");
        if (!ids.insert(im.id).second || !names.insert(im.name).second)
            throw std::runtime_error(what + " appears twice");
        double v[7];
        std::memcpy(v, im.pose_bits, sizeof v);
        bool finite = true;
        for (double x : v) finite = finite && std::isfinite(x);
        if (!finite || v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3] == 0.0)
            throw std::runtime_error(what + " has no usable pose");
        fp.images.push_back(std::move(im));
    }
    if (fp.images.size() < 2)
        throw std::runtime_error("images.bin holds " + std::to_string(fp.images.size()) +
                                 " image(s); triangulating needs two");
    return fp;
}

std::string fixedImageStem(const std::string& name) {
    std::string stem = name;
    std::replace(stem.begin(), stem.end(), '\\', '/');
    const size_t dot = stem.rfind('.');
    if (dot != std::string::npos && stem.find('/', dot) == std::string::npos) stem.erase(dot);
    return stem;
}

std::vector<int64_t> matchFixedImages(const FixedPoses& fp, const MatchesDatabase& db) {
    std::map<std::string, int64_t> by_stem;
    for (size_t i = 0; i < db.images.size(); i++) by_stem.emplace(db.images[i].name, (int64_t)i);
    std::set<int64_t> taken;
    std::vector<int64_t> out;
    out.reserve(fp.images.size());
    for (const FixedImage& im : fp.images) {
        auto it = by_stem.find(fixedImageStem(im.name));
        // Two of the model's names on one stem ("a.jpg", "a.png") cannot both be this file.
        out.push_back(it != by_stem.end() && taken.insert(it->second).second ? it->second : -1);
    }
    return out;
}

FixedSetup fixedSetup(const FixedPoses& fp, const std::vector<int64_t>& db_index,
                      const std::vector<FeatureSet>& feats) {
    FixedSetup s;
    uint32_t unposed = 1;
    for (const auto& kv : fp.cameras) unposed = std::max(unposed, kv.first + 1);
    s.camera_ids.assign(feats.size(), unposed);
    std::map<uint32_t, std::vector<double>> scales;
    for (size_t k = 0; k < fp.images.size(); k++) {
        if (db_index[k] < 0) continue;
        const FixedImage& fi = fp.images[k];
        const uint32_t i = (uint32_t)db_index[k];
        s.camera_ids[i] = fi.camera_id;
        Image im;
        im.id = i;
        im.camera_id = fi.camera_id;
        im.pose = fi.pose();
        im.registered = true;
        // Sized like the run's features, which is what adopt() checks a model by.
        im.points2D.resize(feats[i].count());
        im.point3D_ids.assign(feats[i].count(), kInvalidPoint3D);
        s.model.images[i] = std::move(im);
        scales[fi.camera_id].push_back(feats[i].pixelScale());
    }
    for (const auto& kv : fp.cameras) {
        Camera c = kv.second;
        std::vector<double>& v = scales[kv.first];
        if (!v.empty()) {
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            c.pixel_scale = std::max(1.0, v[v.size() / 2]);
        }
        s.model.cameras[kv.first] = c;
    }
    return s;
}

void writeFixedModel(const std::string& dir, const FixedPoses& fp,
                     const std::vector<int64_t>& db_index, const Reconstruction& rec) {
    using namespace detail;
    const fs::path d(dir);
    fs::create_directories(d);
    std::error_code ec;
    for (const char* stale : {"gauge.txt", "rigs.txt"}) fs::remove(d / stale, ec);
    {
        std::ofstream f(d / "cameras.bin", std::ios::binary | std::ios::trunc);
        f.write(fp.cameras_bin.data(), (std::streamsize)fp.cameras_bin.size());
        if (!f) throw std::runtime_error("cannot write cameras.bin in " + dir);
    }
    std::map<uint32_t, uint32_t> file_id;
    for (size_t k = 0; k < fp.images.size(); k++)
        if (db_index[k] >= 0) file_id[(uint32_t)db_index[k]] = fp.images[k].id;
    {
        std::ofstream f(d / "images.bin", std::ios::binary | std::ios::trunc);
        wr<uint64_t>(f, fp.images.size());
        for (size_t k = 0; k < fp.images.size(); k++) {
            const FixedImage& fi = fp.images[k];
            wr<uint32_t>(f, fi.id);
            for (uint64_t v : fi.pose_bits) wr<uint64_t>(f, v);
            wr<uint32_t>(f, fi.camera_id);
            f.write(fi.name.c_str(), (std::streamsize)fi.name.size() + 1);
            const Image* im = nullptr;
            if (db_index[k] >= 0) {
                auto it = rec.images.find((uint32_t)db_index[k]);
                if (it != rec.images.end() && it->second.registered) im = &it->second;
            }
            wr<uint64_t>(f, im ? im->points2D.size() : 0);
            for (size_t p = 0; im && p < im->points2D.size(); p++) {
                wr<double>(f, im->points2D[p].x);
                wr<double>(f, im->points2D[p].y);
                wr<uint64_t>(f, im->point3D_ids[p]);
            }
        }
        if (!f) throw std::runtime_error("cannot write images.bin in " + dir);
    }
    {
        std::ofstream f(d / "points3D.bin", std::ios::binary | std::ios::trunc);
        wr<uint64_t>(f, rec.points3D.size());
        for (const auto& kv : rec.points3D) {
            const Point3D& p = kv.second;
            wr<uint64_t>(f, kv.first);
            for (double v : {p.xyz.x, p.xyz.y, p.xyz.z}) wr<double>(f, v);
            f.write((const char*)p.rgb, 3);
            wr<double>(f, p.error);
            wr<uint64_t>(f, p.track.size());
            for (const TrackElement& e : p.track) {
                wr<uint32_t>(f, file_id.at(e.image_id));
                wr<uint32_t>(f, e.point2D_idx);
            }
        }
        if (!f) throw std::runtime_error("cannot write points3D.bin in " + dir);
    }
}

std::string checkFixedModel(const std::string& dir, const FixedPoses& fp) {
    const fs::path d(dir);
    if (slurp(d / "cameras.bin") != fp.cameras_bin) return "cameras.bin";
    const std::string images = slurp(d / "images.bin");
    Bytes b{images, "images.bin"};
    std::map<uint32_t, const FixedImage*> want;
    for (const FixedImage& im : fp.images) want[im.id] = &im;
    const uint64_t n = b.take<uint64_t>();
    for (uint64_t i = 0; i < n; i++) {
        const FixedImage got = takeImage(b);
        auto it = want.find(got.id);
        if (it == want.end()) return "images.bin: image " + std::to_string(got.id);
        const FixedImage& w = *it->second;
        if (std::memcmp(got.pose_bits, w.pose_bits, sizeof got.pose_bits) != 0 ||
            got.camera_id != w.camera_id || got.name != w.name)
            return "images.bin: " + w.name;
        want.erase(it);
    }
    if (!want.empty()) return "images.bin: " + want.begin()->second->name;
    return {};
}

}  // namespace sfm
