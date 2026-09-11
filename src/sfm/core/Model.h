// Reconstruction model + COLMAP binary IO (docs/notes/sfm-design.md D4).
//
// Cameras, images (poses + 2D points), and 3D points with tracks, written to
// and read from COLMAP's cameras.bin / images.bin / points3D.bin so the output
// drops into the dataset parser and can be diffed against COLMAP. Little-endian
// host assumed (as the rest of this repo does).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Pose.h"
#include "sfm/core/Rig.h"
#include "sfm/geometry/LinAlg.h"

namespace sfm {

static constexpr uint64_t kInvalidPoint3D = ~0ull;

struct TrackElement {
    uint32_t image_id = 0;
    uint32_t point2D_idx = 0;
};

struct Point3D {
    Vec3 xyz;
    uint8_t rgb[3] = {128, 128, 128};
    double error = 0;
    std::vector<TrackElement> track;
};

struct Image {
    uint32_t id = 0;
    uint32_t camera_id = 0;
    std::string name;
    Pose pose;                 // world -> camera
    bool registered = false;
    std::vector<Vec2> points2D;              // keypoint coords (all features)
    std::vector<uint64_t> point3D_ids;       // parallel; kInvalidPoint3D if none

    uint32_t numPoint3D() const {
        uint32_t n = 0;
        for (uint64_t id : point3D_ids)
            if (id != kInvalidPoint3D) n++;
        return n;
    }
};

// COLMAP camera model ids come from sfm/core/Camera.h's kCamModelInfo table
// (camColmapId / camFromColmapId), so cameras.bin IO needs no local enum.

struct Reconstruction {
    std::map<uint32_t, Camera> cameras;
    std::map<uint32_t, Image> images;
    std::map<uint64_t, Point3D> points3D;
    uint64_t next_point3D_id = 1;
    // Per rig of the run's RigTable, the extrinsics this model has settled on;
    // empty until a rig's frames register. In this model's units (sfm/core/Rig.h).
    std::vector<RigCalib> rigs;
    // Images left outside their frame, with a pose of their own. The mapper
    // keeps frames whole and never fills it; the solvers and the merge honour it.
    std::set<uint32_t> rig_detached;

    uint32_t numRegistered() const {
        uint32_t n = 0;
        for (const auto& kv : images)
            if (kv.second.registered) n++;
        return n;
    }

    uint64_t addPoint3D(const Vec3& xyz, const std::vector<TrackElement>& track) {
        uint64_t id = next_point3D_id++;
        Point3D p;
        p.xyz = xyz;
        p.track = track;
        points3D[id] = p;
        for (const TrackElement& e : track) images[e.image_id].point3D_ids[e.point2D_idx] = id;
        return id;
    }

    void writeBinary(const std::string& dir) const;
    static Reconstruction readBinary(const std::string& dir);
};

// A copy holding only `keep`, with the tracks trimmed to match: the operation
// every "this model is really two models" decision ends in (D45). Images not
// kept go back to the unregistered state the mapper starts them in, and points
// whose track falls below two observations go away entirely.
inline Reconstruction subsetModel(const Reconstruction& m, const std::set<uint32_t>& keep) {
    Reconstruction out = m;
    for (auto& kv : out.images) {
        if (keep.count(kv.first)) continue;
        kv.second.registered = false;
        kv.second.pose = {mat3Identity(), {0, 0, 0}};
        std::fill(kv.second.point3D_ids.begin(), kv.second.point3D_ids.end(), kInvalidPoint3D);
    }
    std::vector<uint64_t> dead;
    for (auto& kv : out.points3D) {
        std::vector<TrackElement> t;
        for (const TrackElement& e : kv.second.track)
            if (keep.count(e.image_id)) t.push_back(e);
        if (t.size() < 2) { dead.push_back(kv.first); continue; }
        kv.second.track = std::move(t);
    }
    for (uint64_t pid : dead) {
        for (const TrackElement& e : m.points3D.at(pid).track) {
            auto it = out.images.find(e.image_id);
            if (it != out.images.end() && e.point2D_idx < it->second.point3D_ids.size() &&
                it->second.point3D_ids[e.point2D_idx] == pid)
                it->second.point3D_ids[e.point2D_idx] = kInvalidPoint3D;
        }
        out.points3D.erase(pid);
    }
    return out;
}

// "Have these two images been matched to each other, with real support?" --
// supplied by whoever holds the correspondence graph (the mapper does; the
// merger does not). See findDuplicateStructure (D45).
using MatchedFn = std::function<bool(uint32_t, uint32_t)>;

// The 3D points an image observes, sorted (for cheap intersections).
inline std::vector<uint64_t> observedPoints(const Image& im) {
    std::vector<uint64_t> v;
    for (uint64_t p : im.point3D_ids)
        if (p != kInvalidPoint3D) v.push_back(p);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

inline size_t sharedPoints(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
    size_t n = 0, i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) i++;
        else if (b[j] < a[i]) j++;
        else { n++; i++; j++; }
    }
    return n;
}

// ---- little-endian binary helpers ----
namespace detail {
template <class T>
void wr(std::ostream& f, T v) {
    f.write((const char*)&v, sizeof(T));
}
template <class T>
T rd(std::istream& f) {
    T v{};
    f.read((char*)&v, sizeof(T));
    return v;
}
}  // namespace detail

inline void Reconstruction::writeBinary(const std::string& dir) const {
    using namespace detail;
    {
        std::ofstream f(dir + "/cameras.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot write cameras.bin in " + dir);
        wr<uint64_t>(f, cameras.size());
        for (const auto& kv : cameras) {
            const Camera& c = kv.second;
            wr<uint32_t>(f, c.id);
            wr<int32_t>(f, camColmapId(c.model));
            wr<uint64_t>(f, c.width);
            wr<uint64_t>(f, c.height);
            double ps[12];
            packColmap(c, ps);
            for (int i = 0; i < camColmapParams(c.model); i++) wr<double>(f, ps[i]);
        }
    }
    {
        std::ofstream f(dir + "/images.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot write images.bin in " + dir);
        uint64_t nreg = numRegistered();
        wr<uint64_t>(f, nreg);
        for (const auto& kv : images) {
            const Image& im = kv.second;
            if (!im.registered) continue;
            Quat q = rotationToQuaternion(im.pose.R);
            wr<uint32_t>(f, im.id);
            for (double v : {q[0], q[1], q[2], q[3]}) wr<double>(f, v);
            for (double v : {im.pose.t.x, im.pose.t.y, im.pose.t.z}) wr<double>(f, v);
            wr<uint32_t>(f, im.camera_id);
            f.write(im.name.c_str(), im.name.size() + 1);  // NUL-terminated
            wr<uint64_t>(f, im.points2D.size());
            for (size_t i = 0; i < im.points2D.size(); i++) {
                wr<double>(f, im.points2D[i].x);
                wr<double>(f, im.points2D[i].y);
                wr<uint64_t>(f, im.point3D_ids[i]);
            }
        }
    }
    {
        std::ofstream f(dir + "/points3D.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot write points3D.bin in " + dir);
        wr<uint64_t>(f, points3D.size());
        for (const auto& kv : points3D) {
            const Point3D& p = kv.second;
            wr<uint64_t>(f, kv.first);
            for (double v : {p.xyz.x, p.xyz.y, p.xyz.z}) wr<double>(f, v);
            f.write((const char*)p.rgb, 3);
            wr<double>(f, p.error);
            wr<uint64_t>(f, p.track.size());
            for (const TrackElement& e : p.track) {
                wr<uint32_t>(f, e.image_id);
                wr<uint32_t>(f, e.point2D_idx);
            }
        }
    }
}

inline Reconstruction Reconstruction::readBinary(const std::string& dir) {
    using namespace detail;
    Reconstruction r;
    {
        std::ifstream f(dir + "/cameras.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot read cameras.bin");
        uint64_t n = rd<uint64_t>(f);
        for (uint64_t i = 0; i < n; i++) {
            Camera c;
            c.id = rd<uint32_t>(f);
            int32_t model = rd<int32_t>(f);
            c.width = (int)rd<uint64_t>(f);
            c.height = (int)rd<uint64_t>(f);
            c.model = camFromColmapId(model);
            double ps[12];
            for (int k = 0; k < camColmapParams(c.model); k++) ps[k] = rd<double>(f);
            unpackColmap(c, ps);
            r.cameras[c.id] = c;
        }
    }
    {
        std::ifstream f(dir + "/images.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot read images.bin");
        uint64_t n = rd<uint64_t>(f);
        for (uint64_t i = 0; i < n; i++) {
            Image im;
            im.registered = true;
            im.id = rd<uint32_t>(f);
            Quat q = {rd<double>(f), rd<double>(f), rd<double>(f), rd<double>(f)};
            im.pose.R = quaternionToRotation(q);
            im.pose.t = {rd<double>(f), rd<double>(f), rd<double>(f)};
            im.camera_id = rd<uint32_t>(f);
            std::string name;
            char ch;
            while (f.get(ch) && ch != '\0') name += ch;
            im.name = name;
            uint64_t np = rd<uint64_t>(f);
            im.points2D.resize(np);
            im.point3D_ids.resize(np);
            for (uint64_t k = 0; k < np; k++) {
                im.points2D[k] = {rd<double>(f), rd<double>(f)};
                im.point3D_ids[k] = rd<uint64_t>(f);
            }
            r.images[im.id] = im;
        }
    }
    {
        std::ifstream f(dir + "/points3D.bin", std::ios::binary);
        if (!f) throw std::runtime_error("cannot read points3D.bin");
        uint64_t n = rd<uint64_t>(f);
        for (uint64_t i = 0; i < n; i++) {
            uint64_t id = rd<uint64_t>(f);
            Point3D p;
            p.xyz = {rd<double>(f), rd<double>(f), rd<double>(f)};
            p.rgb[0] = rd<uint8_t>(f); p.rgb[1] = rd<uint8_t>(f); p.rgb[2] = rd<uint8_t>(f);
            p.error = rd<double>(f);
            uint64_t tl = rd<uint64_t>(f);
            p.track.resize(tl);
            for (uint64_t k = 0; k < tl; k++) {
                p.track[k].image_id = rd<uint32_t>(f);
                p.track[k].point2D_idx = rd<uint32_t>(f);
            }
            r.points3D[id] = p;
            r.next_point3D_id = std::max(r.next_point3D_id, id + 1);
        }
    }
    return r;
}

}  // namespace sfm
