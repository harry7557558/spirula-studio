#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/ModelMemory.h"

namespace sfm {

namespace model_store_detail {

using model_memory_detail::addBytes;

class Writer {
public:
    explicit Writer(const std::filesystem::path& path) : file_(path, std::ios::binary) {
        if (!file_) throw std::runtime_error("cannot create spilled model");
    }
    template <class T> void scalar(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        file_.write(reinterpret_cast<const char*>(&v), sizeof(T));
        if (!file_) throw std::runtime_error("cannot write spilled model");
    }
    template <class T> void vector(const std::vector<T>& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        scalar<uint64_t>(v.size());
        if (!v.empty()) file_.write(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(T));
        if (!file_) throw std::runtime_error("cannot write spilled model vector");
    }
    void string(const std::string& v) {
        scalar<uint64_t>(v.size());
        file_.write(v.data(), v.size());
        if (!file_) throw std::runtime_error("cannot write spilled model name");
    }
    void finish() {
        file_.flush();
        if (!file_) throw std::runtime_error("cannot flush spilled model");
        file_.close();
        if (!file_) throw std::runtime_error("cannot close spilled model");
    }
private:
    std::ofstream file_;
};

class Reader {
public:
    explicit Reader(const std::filesystem::path& path)
        : file_(path, std::ios::binary), remaining_(std::filesystem::file_size(path)) {
        if (!file_) throw std::runtime_error("cannot open spilled model");
    }
    template <class T> T scalar() {
        static_assert(std::is_trivially_copyable_v<T>);
        require(sizeof(T));
        T v{};
        file_.read(reinterpret_cast<char*>(&v), sizeof(T));
        if (!file_) throw std::runtime_error("cannot read spilled model");
        return v;
    }
    template <class T> void read(T* destination, size_t count) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (count > remaining_ / sizeof(T)) throw std::runtime_error("truncated spilled model array");
        const size_t bytes = count * sizeof(T);
        require(bytes);
        if (bytes) file_.read(reinterpret_cast<char*>(destination), bytes);
        if (!file_) throw std::runtime_error("cannot read spilled model array");
    }
    size_t count(size_t minimum_bytes) {
        const uint64_t n = scalar<uint64_t>();
        if (n > SIZE_MAX || (minimum_bytes && n > remaining_ / minimum_bytes))
            throw std::runtime_error("invalid spilled model count");
        return static_cast<size_t>(n);
    }
    template <class T> std::vector<T> vector() {
        static_assert(std::is_trivially_copyable_v<T>);
        const size_t n = count(sizeof(T));
        require(n * sizeof(T));
        std::vector<T> v(n);
        if (n) file_.read(reinterpret_cast<char*>(v.data()), n * sizeof(T));
        if (!file_) throw std::runtime_error("cannot read spilled model vector");
        return v;
    }
    std::string string() {
        const size_t n = count(1);
        require(n);
        std::string v(n, '\0');
        if (n) file_.read(v.data(), n);
        if (!file_) throw std::runtime_error("cannot read spilled model name");
        return v;
    }
    void finish() {
        if (remaining_) throw std::runtime_error("unexpected trailing spilled model data");
    }
private:
    void require(uint64_t n) {
        if (n > remaining_) throw std::runtime_error("truncated spilled model");
        remaining_ -= n;
    }
    std::ifstream file_;
    uint64_t remaining_ = 0;
};

inline void writeCamera(Writer& out, const Camera& c) {
    out.scalar(c.id); out.scalar(c.width); out.scalar(c.height);
    out.scalar<int32_t>(static_cast<int32_t>(c.model));
    for (double v : {c.fx, c.fy, c.cx, c.cy, c.k1, c.k2, c.p1, c.p2, c.k3, c.k4,
                     c.k5, c.k6, c.sx1, c.sy1, c.pixel_scale}) out.scalar(v);
}

inline Camera readCamera(Reader& in) {
    Camera c;
    c.id = in.scalar<uint32_t>(); c.width = in.scalar<int>(); c.height = in.scalar<int>();
    c.model = static_cast<CamModel>(in.scalar<int32_t>());
    for (double* v : {&c.fx, &c.fy, &c.cx, &c.cy, &c.k1, &c.k2, &c.p1, &c.p2,
                      &c.k3, &c.k4, &c.k5, &c.k6, &c.sx1, &c.sy1, &c.pixel_scale})
        *v = in.scalar<double>();
    return c;
}

inline void writeModel(const Reconstruction& m, const std::filesystem::path& path) {
    Writer out(path);
    out.scalar<uint64_t>(0x31504c4c49505353ull);
    out.scalar<uint32_t>(1);
    out.scalar(m.next_point3D_id);
    out.scalar<uint64_t>(m.cameras.size());
    for (const auto& kv : m.cameras) {
        out.scalar(kv.first); writeCamera(out, kv.second);
    }
    out.scalar<uint64_t>(m.images.size());
    for (const auto& kv : m.images) {
        const Image& im = kv.second;
        out.scalar(kv.first); out.scalar(im.id); out.scalar(im.camera_id); out.string(im.name);
        out.scalar(im.pose); out.scalar<uint8_t>(im.registered ? 1 : 0);
        out.scalar(im.exif_orientation); out.vector(im.points2D); out.vector(im.point3D_ids);
    }
    out.scalar<uint64_t>(m.points3D.size());
    for (const auto& kv : m.points3D) {
        const Point3D& p = kv.second;
        out.scalar(kv.first); out.scalar(p.xyz);
        for (uint8_t v : p.rgb) out.scalar(v);
        out.scalar(p.error); out.vector(p.track);
    }
    out.scalar<uint64_t>(m.rigs.size());
    for (const RigCalib& r : m.rigs) {
        out.scalar(r.ref); out.vector(r.cam_from_rig); out.vector(r.established);
        out.vector(r.fixed); out.vector(r.support); out.vector(r.spread_deg);
        out.vector(r.declined_at); out.scalar(r.user_scale);
    }
    out.scalar<uint64_t>(m.rig_detached.size());
    for (uint32_t id : m.rig_detached) out.scalar(id);
    out.finish();
}

inline Reconstruction readModel(const std::filesystem::path& path) {
    Reader in(path);
    if (in.scalar<uint64_t>() != 0x31504c4c49505353ull || in.scalar<uint32_t>() != 1)
        throw std::runtime_error("unsupported spilled model format");
    Reconstruction m;
    m.next_point3D_id = in.scalar<uint64_t>();
    const size_t cameras = in.count(sizeof(uint32_t) * 5 + sizeof(double) * 15);
    for (size_t i = 0; i < cameras; ++i) {
        const auto id = in.scalar<uint32_t>();
        if (!m.cameras.emplace(id, readCamera(in)).second)
            throw std::runtime_error("duplicate spilled camera id");
    }
    const size_t images = in.count(sizeof(uint32_t) * 3 + sizeof(Pose) + 26);
    for (size_t i = 0; i < images; ++i) {
        const auto id = in.scalar<uint32_t>();
        Image im;
        im.id = in.scalar<uint32_t>(); im.camera_id = in.scalar<uint32_t>(); im.name = in.string();
        im.pose = in.scalar<Pose>(); im.registered = in.scalar<uint8_t>() != 0;
        im.exif_orientation = in.scalar<uint8_t>();
        im.points2D = in.vector<Vec2>(); im.point3D_ids = in.vector<uint64_t>();
        if (im.points2D.size() != im.point3D_ids.size() || !m.images.emplace(id, std::move(im)).second)
            throw std::runtime_error("invalid spilled image");
    }
    const size_t points = in.count(sizeof(uint64_t) * 2 + sizeof(Vec3) + sizeof(double) + 3);
    for (size_t i = 0; i < points; ++i) {
        const auto id = in.scalar<uint64_t>();
        Point3D p;
        p.xyz = in.scalar<Vec3>();
        for (uint8_t& v : p.rgb) v = in.scalar<uint8_t>();
        p.error = in.scalar<double>(); p.track = in.vector<TrackElement>();
        if (!m.points3D.emplace(id, std::move(p)).second)
            throw std::runtime_error("duplicate spilled point id");
    }
    const size_t rigs = in.count(sizeof(int) + sizeof(uint64_t) * 6 + sizeof(double));
    m.rigs.resize(rigs);
    for (RigCalib& r : m.rigs) {
        r.ref = in.scalar<int>(); r.cam_from_rig = in.vector<Pose>();
        r.established = in.vector<uint8_t>(); r.fixed = in.vector<uint8_t>();
        r.support = in.vector<uint32_t>(); r.spread_deg = in.vector<double>();
        r.declined_at = in.vector<uint32_t>(); r.user_scale = in.scalar<double>();
    }
    const size_t detached = in.count(sizeof(uint32_t));
    for (size_t i = 0; i < detached; ++i)
        if (!m.rig_detached.insert(in.scalar<uint32_t>()).second)
            throw std::runtime_error("duplicate spilled detached image id");
    in.finish();
    return m;
}

}  // namespace model_store_detail

struct StoredModel {
    uint64_t file = 0;
    size_t resident_bytes = 0;
    std::vector<uint32_t> registered_images;
};

class ModelStore {
public:
    explicit ModelStore(const std::string& parent) {
        namespace fs = std::filesystem;
        const fs::path base = parent.empty() ? fs::temp_directory_path() : fs::path(parent);
        fs::create_directories(base);
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        static std::atomic<uint64_t> serial{0};
        for (int trial = 0; trial < 100; ++trial) {
            root_ = fs::absolute(base) / ("bup-models-" + std::to_string(tick) + "-" +
                                         std::to_string(serial++));
            if (fs::create_directory(root_)) return;
        }
        throw std::runtime_error("cannot create an exclusive model scratch directory");
    }
    ModelStore(const ModelStore&) = delete;
    ModelStore& operator=(const ModelStore&) = delete;
    ~ModelStore() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    StoredModel put(const Reconstruction& m) {
        StoredModel meta;
        meta.file = next_++;
        meta.resident_bytes = modelResidentBytes(m);
        for (const auto& kv : m.images)
            if (kv.second.registered) meta.registered_images.push_back(kv.first);
        model_store_detail::writeModel(m, path(meta));
        return meta;
    }
    Reconstruction load(const StoredModel& meta, size_t limit = SIZE_MAX) const {
        if (meta.resident_bytes > limit)
            throw std::runtime_error("spilled model exceeds the model memory budget");
        if (std::filesystem::file_size(path(meta)) > meta.resident_bytes)
            throw std::runtime_error("spilled model file exceeds its memory estimate");
        Reconstruction m = model_store_detail::readModel(path(meta));
        if (modelResidentBytes(m) > limit)
            throw std::runtime_error("loaded model exceeds the model memory budget");
        return m;
    }
    void erase(const StoredModel& meta) { std::filesystem::remove(path(meta)); }
    std::filesystem::path path(const StoredModel& meta) const {
        return root_ / (std::to_string(meta.file) + ".model");
    }
    const std::filesystem::path& directory() const { return root_; }
private:
    std::filesystem::path root_;
    std::atomic<uint64_t> next_{0};
};

inline size_t storedModelBytes(const std::vector<StoredModel>& models) {
    size_t bytes = 0;
    for (const StoredModel& m : models)
        bytes = model_store_detail::addBytes(bytes, m.resident_bytes);
    return bytes;
}

inline std::vector<std::vector<size_t>> storedModelGroups(
    const std::vector<StoredModel>& models, size_t limit) {
    std::map<uint32_t, std::vector<size_t>> owners;
    for (size_t i = 0; i < models.size(); ++i) {
        if (models[i].resident_bytes > limit)
            throw std::runtime_error("one atom model exceeds the model memory budget");
        for (uint32_t id : models[i].registered_images) owners[id].push_back(i);
    }
    std::vector<uint8_t> used(models.size(), 0);
    std::vector<std::vector<size_t>> groups;
    for (size_t done = 0; done < models.size();) {
        size_t seed = models.size();
        for (size_t i = 0; i < models.size(); ++i)
            if (!used[i] && (seed == models.size() ||
                models[i].registered_images.size() > models[seed].registered_images.size())) seed = i;
        std::vector<size_t> group;
        std::map<size_t, size_t> overlap;
        size_t bytes = 0, chosen = seed;
        for (;;) {
            group.push_back(chosen); used[chosen] = 1; ++done;
            bytes += models[chosen].resident_bytes;
            for (uint32_t id : models[chosen].registered_images)
                for (size_t peer : owners[id])
                    if (!used[peer]) ++overlap[peer];
            chosen = models.size();
            size_t best = 0;
            for (const auto& kv : overlap)
                if (!used[kv.first] && kv.second > best &&
                    models[kv.first].resident_bytes <= limit - bytes) {
                    chosen = kv.first; best = kv.second;
                }
            if (chosen == models.size()) break;
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

}  // namespace sfm
