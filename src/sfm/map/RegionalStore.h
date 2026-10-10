#pragma once

#include <filesystem>
#include <fstream>
#include <list>
#include <queue>
#include <unordered_map>

#include "sfm/core/Cancel.h"
#include "sfm/core/Features.h"
#include "sfm/map/Merge.h"
#include "sfm/map/ModelStore.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace sfm::regional {
namespace fs = std::filesystem;

inline void replaceFile(const fs::path& tmp, const fs::path& path) {
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error(int(GetLastError()), std::system_category(), "regional checkpoint replacement failed");
#else
    fs::rename(tmp, path);
#endif
}

inline void atomicModel(const fs::path& path, const Reconstruction& model) {
    const fs::path tmp = path.string() + ".part";
    model_store_detail::writeModel(model, tmp);
    replaceFile(tmp, path);
}

template<class T> class PagedArray {
    static constexpr size_t rows = sizeof(T) < 4096 ? 4096 / sizeof(T) : 1;
    struct Page { std::vector<T> data; bool dirty = false; std::list<uint64_t>::iterator age; };
public:
    PagedArray(const fs::path& path, size_t cache_bytes)
        : file_(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc),
          capacity_(std::max<size_t>(1, cache_bytes / (rows * sizeof(T)))) {
        if (!file_) throw std::runtime_error("cannot create regional point index");
    }
    ~PagedArray() = default;
    uint64_t size() const { return size_; }
    T get(uint64_t i) { if (i >= size_) throw std::out_of_range("regional point index"); return page(i / rows).data[i % rows]; }
    void set(uint64_t i, const T& v) { if (i >= size_) throw std::out_of_range("regional point index"); auto& p = page(i / rows); p.data[i % rows] = v; p.dirty = true; }
    uint64_t append(const T& v) {
        uint64_t i = size_++;
        auto& p = page(i / rows); p.data[i % rows] = v; p.dirty = true;
        return i;
    }
    void flush() {
        for (auto& kv : pages_) save(kv.first, kv.second);
        file_.flush(); if (!file_) throw std::runtime_error("cannot flush regional point index");
    }
private:
    void save(uint64_t id, Page& p) {
        if (!p.dirty) return;
        file_.clear(); file_.seekp(std::streamoff(id * rows * sizeof(T)));
        file_.write(reinterpret_cast<const char*>(p.data.data()), rows * sizeof(T));
        if (!file_) throw std::runtime_error("cannot write regional point index");
        p.dirty = false;
    }
    Page& page(uint64_t id) {
        auto found = pages_.find(id);
        if (found != pages_.end()) {
            age_.splice(age_.end(), age_, found->second.age); return found->second;
        }
        if (pages_.size() >= capacity_) {
            uint64_t old = age_.front(); save(old, pages_.at(old)); pages_.erase(old); age_.pop_front();
        }
        Page p; p.data.resize(rows);
        file_.clear(); file_.seekg(0, std::ios::end);
        auto bytes = file_.tellg();
        if (bytes > std::streamoff(id * rows * sizeof(T))) {
            file_.seekg(std::streamoff(id * rows * sizeof(T)));
            file_.read(reinterpret_cast<char*>(p.data.data()), rows * sizeof(T));
            if (!file_) throw std::runtime_error("truncated regional point index");
        }
        age_.push_back(id); p.age = std::prev(age_.end());
        return pages_.emplace(id, std::move(p)).first->second;
    }
    std::fstream file_;
    size_t capacity_;
    uint64_t size_ = 0;
    std::list<uint64_t> age_;
    std::unordered_map<uint64_t, Page> pages_;
};

struct PointNode {
    uint64_t fragments = 1, observations = 0;
    Vec3 xyz;
    double rgb[3] = {128, 128, 128};
};

class PointUnion {
public:
    PointUnion(const fs::path& path, size_t cache)
        : nodes(path, cache * 3 / 4),
          parents_(path.string() + ".parents", cache / 4) {}
    uint64_t add(const Point3D& p) {
        PointNode n; n.xyz = p.xyz;
        n.observations = p.track.size(); for (int k = 0; k < 3; ++k) n.rgb[k] = p.rgb[k];
        uint64_t id = nodes.append(n); parents_.append(id); return id;
    }
    uint64_t root(uint64_t i) {
        uint64_t r = i;
        for (;;) { auto next = parents_.get(r); if (next == r) break; r = next; }
        while (i != r) { auto next = parents_.get(i); parents_.set(i, r); i = next; }
        return r;
    }
    void join(uint64_t a, uint64_t b) {
        a = root(a); b = root(b); if (a == b) return;
        PointNode x = nodes.get(a), y = nodes.get(b);
        if (x.fragments < y.fragments) { std::swap(a, b); std::swap(x, y); }
        const double f = double(y.observations) / std::max<uint64_t>(1, x.observations + y.observations);
        x.xyz = x.xyz * (1 - f) + y.xyz * f;
        for (int k = 0; k < 3; ++k) x.rgb[k] = x.rgb[k] * (1 - f) + y.rgb[k] * f;
        x.observations += y.observations; x.fragments += y.fragments;
        nodes.set(a, x); parents_.set(b, a);
    }
    PagedArray<PointNode> nodes;
    void flush() { nodes.flush(); parents_.flush(); }
private:
    PagedArray<uint64_t> parents_;
};

struct Observation {
    uint64_t key = 0, point = 0;
    Vec2 xy{};
    bool operator<(const Observation& b) const { return key != b.key ? key < b.key : point < b.point; }
};

template<class T> inline void record(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    if (!out) throw std::runtime_error("cannot write regional export records");
}
template<class T> inline bool nextRecord(std::istream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (in.gcount() == sizeof(value)) return true;
    if (in.gcount() || !in.eof()) throw std::runtime_error("truncated regional export records");
    return false;
}

// The fan-in bounds file handles and per-stream buffers as well as the sort chunks.
inline fs::path sortObservations(const fs::path& input, const fs::path& dir, size_t memory) {
    fs::create_directories(dir);
    std::vector<fs::path> runs;
    uint64_t sequence = 0;
    std::ifstream in(input, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open regional export records");
    const size_t count = std::max<size_t>(1, memory / sizeof(Observation));
    for (;;) {
        cancel::check();
        std::vector<Observation> chunk; chunk.reserve(count);
        Observation e;
        while (chunk.size() < count && nextRecord(in, e)) chunk.push_back(e);
        if (chunk.empty()) break;
        std::sort(chunk.begin(), chunk.end());
        fs::path p = dir / (std::to_string(sequence++) + ".bin");
        std::ofstream out(p, std::ios::binary);
        for (const auto& x : chunk) record(out, x);
        runs.push_back(p);
    }
    if (runs.empty()) {
        fs::path p = dir / "empty.bin"; std::ofstream out(p, std::ios::binary); return p;
    }
    while (runs.size() > 1) {
        std::vector<fs::path> merged;
        for (size_t begin = 0; begin < runs.size(); begin += 32) {
            cancel::check();
            const size_t end = std::min(runs.size(), begin + 32);
            struct Head { Observation row; size_t stream; bool operator<(const Head& b) const { return b.row < row; } };
            std::priority_queue<Head> heap;
            std::vector<std::ifstream> streams(end - begin);
            for (size_t i = begin; i < end; ++i) {
                streams[i - begin].open(runs[i], std::ios::binary);
                if (!streams[i - begin]) throw std::runtime_error("cannot open regional sort run");
                Observation row; if (nextRecord(streams[i - begin], row)) heap.push({row, i - begin});
            }
            fs::path p = dir / (std::to_string(sequence++) + ".bin");
            std::ofstream out(p, std::ios::binary);
            uint64_t n = 0;
            while (!heap.empty()) {
                if (++n % 65536 == 0) cancel::check();
                Head h = heap.top(); heap.pop(); record(out, h.row);
                if (nextRecord(streams[h.stream], h.row)) heap.push(h);
            }
            out.close(); streams.clear();
            for (size_t i = begin; i < end; ++i) fs::remove(runs[i]);
            merged.push_back(p);
        }
        runs = std::move(merged);
    }
    return runs.front();
}

inline Vec3 refineUnifiedPoint(Vec3 initial, const std::vector<Observation>& track,
                               const Reconstruction& poses, double gate) {
    auto cost = [&](const Vec3& xyz) {
        double sum = 0;
        for (const auto& obs : track) {
            const Image& im = poses.images.at(uint32_t(obs.point >> 32));
            const Camera& cam = poses.cameras.at(im.camera_id);
            double e = reprojErrorAt(cam, im.pose, obs.xy, xyz) / cam.pixel_scale;
            if (!std::isfinite(e)) return std::numeric_limits<double>::infinity();
            sum += e <= gate ? e * e : gate * (2 * e - gate);
        }
        return sum;
    };
    Mat3 H{}; Vec3 g{0, 0, 0};
    for (const auto& obs : track) {
        const Image& im = poses.images.at(uint32_t(obs.point >> 32));
        const Camera& cam = poses.cameras.at(im.camera_id);
        Vec3 c = cameraCenter(im.pose), d = mul(transpose(im.pose.R), cam.bearing(obs.xy)).normalized();
        double weight = std::pow(cam.focal() / std::max(1., (initial - c).norm()), 2);
        double v[3] = {d.x, d.y, d.z}; Mat3 A;
        for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b) {
            A[3 * a + b] = ((a == b ? 1. : 0.) - v[a] * v[b]) * weight;
            H[3 * a + b] += A[3 * a + b];
        }
        g = g + mul(A, c);
    }
    bool ok = false; Vec3 ray = mul(inverse3(H, &ok), g);
    double current = cost(initial);
    if (ok && cost(ray) < current) { initial = ray; current = cost(ray); }
    for (int iteration = 0; iteration < 5; ++iteration) {
        H = {}; g = {0, 0, 0};
        for (const auto& obs : track) {
            const Image& im = poses.images.at(uint32_t(obs.point >> 32));
            const Camera& cam = poses.cameras.at(im.camera_id);
            Vec3 pc = mul(im.pose.R, initial) + im.pose.t; Vec2 px = cam.project(pc);
            double r[2] = {(px.x - obs.xy.x) / cam.pixel_scale, (px.y - obs.xy.y) / cam.pixel_scale};
            double norm = std::hypot(r[0], r[1]), weight = norm > gate ? gate / norm : 1;
            double eps = std::max(1e-6, pc.norm() * 1e-6), J[2][3];
            for (int a = 0; a < 3; ++a) {
                Vec3 moved = initial; if (a == 0) moved.x += eps; if (a == 1) moved.y += eps; if (a == 2) moved.z += eps;
                Vec2 q = cam.project(mul(im.pose.R, moved) + im.pose.t);
                J[0][a] = (q.x - px.x) / (eps * cam.pixel_scale); J[1][a] = (q.y - px.y) / (eps * cam.pixel_scale);
            }
            double gradient[3]{};
            for (int a = 0; a < 3; ++a) {
                gradient[a] = weight * (J[0][a] * r[0] + J[1][a] * r[1]);
                for (int b = 0; b < 3; ++b) H[3 * a + b] += weight * (J[0][a] * J[0][b] + J[1][a] * J[1][b]);
            }
            g = g + Vec3{gradient[0], gradient[1], gradient[2]};
        }
        for (int a = 0; a < 3; ++a) H[3 * a + a] += 1e-6 * std::max(1., H[3 * a + a]);
        Vec3 step = mul(inverse3(H, &ok), g); if (!ok || !std::isfinite(step.norm())) break;
        bool accepted = false;
        for (double factor : {1., 0.5, 0.25}) {
            Vec3 candidate = initial - step * factor; double next = cost(candidate);
            if (next < current) { initial = candidate; current = next; accepted = true; break; }
        }
        if (!accepted || step.norm() < 1e-7) break;
    }
    return initial;
}

struct ExportStats {
    uint64_t images = 0, points = 0, observations = 0, conflicts = 0, cross_region_points = 0;
    size_t components = 0;
    double error = 0, extraction_error = 0, median = 0;
};

// Models are supplied one at a time; point identities and observation sorts live on disk.
template<class ReadModel, class FeaturePath> inline ExportStats exportUnified(
    size_t models, ReadModel read, const Reconstruction& poses, FeaturePath feature_path,
    const fs::path& scratch, const fs::path& output, size_t cache, double max_error,
    const std::vector<uint32_t>* owners = nullptr) {
    fs::create_directories(scratch); fs::create_directories(output);
    PointUnion points(scratch / "point-index.bin", cache);
    const fs::path records = scratch / "observations.bin";
    {
        std::ofstream out(records, std::ios::binary);
        for (size_t m = 0; m < models; ++m) {
            cancel::check(); Reconstruction rec = read(m);
            for (const auto& kv : rec.points3D) {
                uint64_t id = points.add(kv.second);
                for (const TrackElement& e : kv.second.track) {
                    auto im = poses.images.find(e.image_id);
                    if (im == poses.images.end() || !im->second.registered) continue;
                    const auto local = rec.images.find(e.image_id);
                    if (local == rec.images.end() || e.point2D_idx >= local->second.points2D.size())
                        throw std::runtime_error("invalid regional checkpoint observation");
                    const Camera& cam = poses.cameras.at(im->second.camera_id);
                    double err = reprojErrorAt(cam, im->second.pose, local->second.points2D[e.point2D_idx], kv.second.xyz);
                    if (!std::isfinite(err) || err > max_error * cam.pixel_scale) continue;
                    record(out, Observation{(uint64_t(e.image_id) << 32) | e.point2D_idx, id, local->second.points2D[e.point2D_idx]});
                }
            }
        }
    }
    fs::path sorted = sortObservations(records, scratch / "by-feature", cache);
    {
        std::ifstream in(sorted, std::ios::binary); Observation row, previous; bool have = false;
        uint64_t n = 0;
        while (nextRecord(in, row)) {
            if (++n % 65536 == 0) cancel::check();
            if (have && row.key == previous.key) points.join(previous.point, row.point);
            previous = row; have = true;
        }
    }
    const fs::path tracks = scratch / "tracks.bin", images_records = scratch / "image-records.bin";
    {
        std::ifstream in(sorted, std::ios::binary); std::ofstream tr(tracks, std::ios::binary);
        Observation row; uint64_t previous = UINT64_MAX, n = 0;
        while (nextRecord(in, row)) {
            if (++n % 65536 == 0) cancel::check();
            if (row.key == previous) continue; previous = row.key;
            record(tr, Observation{points.root(row.point), row.key, row.xy});
        }
    }
    fs::path ordered_tracks = sortObservations(tracks, scratch / "by-point", cache);
    ExportStats result; result.images = poses.numRegistered();
    std::map<uint32_t, uint32_t> connected;
    if (owners) for (const auto& kv : poses.images) if (kv.second.registered) connected[owners->at(kv.first)] = owners->at(kv.first);
    auto component = [&](uint32_t i) { while (connected.at(i) != i) i = connected.at(i); return i; };
    std::vector<uint64_t> histogram(16384);
    double radius = max_error;
    for (const auto& kv : poses.cameras) radius = std::max(radius, max_error * kv.second.pixel_scale);
    {
        std::ifstream in(ordered_tracks, std::ios::binary);
        std::ofstream out(output / "points3D.bin", std::ios::binary), imout(images_records, std::ios::binary);
        detail::wr<uint64_t>(out, 0);
        Observation row; bool have = nextRecord(in, row);
        while (have) {
            cancel::check(); uint64_t id = row.key; std::vector<Observation> track; bool conflict = false;
            do {
                if (!track.empty() && uint32_t(track.back().point >> 32) == uint32_t(row.point >> 32)) conflict = true;
                track.push_back(row); have = nextRecord(in, row);
            } while (have && row.key == id);
            PointNode node = points.nodes.get(id);
            if (track.size() < 2 || conflict) { if (conflict) result.conflicts++; continue; }
            node.xyz = refineUnifiedPoint(node.xyz, track, poses, max_error);
            std::vector<Observation> accepted; std::vector<double> residuals; double sum = 0, scaled = 0;
            for (const auto& obs : track) {
                const Image& im = poses.images.at(uint32_t(obs.point >> 32)); const Camera& cam = poses.cameras.at(im.camera_id);
                double error = reprojErrorAt(cam, im.pose, obs.xy, node.xyz);
                if (!std::isfinite(error) || error > max_error * cam.pixel_scale) continue;
                accepted.push_back(obs); residuals.push_back(error); sum += error; scaled += error / cam.pixel_scale;
            }
            if (accepted.size() < 2) continue;
            if (owners) {
                uint32_t first = owners->at(uint32_t(accepted.front().point >> 32)); bool bridge = false;
                for (const auto& obs : accepted) {
                    uint32_t other = owners->at(uint32_t(obs.point >> 32)); bridge = bridge || other != first;
                    connected[component(other)] = component(first);
                }
                if (bridge) result.cross_region_points++;
            }
            detail::wr(out, id + 1);
            for (double v : {node.xyz.x, node.xyz.y, node.xyz.z}) detail::wr(out, v);
            for (int k = 0; k < 3; ++k) detail::wr<uint8_t>(out, uint8_t(std::clamp(node.rgb[k], 0., 255.)));
            detail::wr(out, sum / accepted.size()); detail::wr<uint64_t>(out, accepted.size());
            for (size_t k = 0; k < accepted.size(); ++k) {
                const auto& obs = accepted[k];
                detail::wr<uint32_t>(out, uint32_t(obs.point >> 32)); detail::wr<uint32_t>(out, uint32_t(obs.point));
                record(imout, Observation{obs.point, id, obs.xy});
                size_t bin = std::min(histogram.size() - 1, size_t(residuals[k] / std::max(1e-6, radius) * histogram.size()));
                histogram[bin]++;
            }
            result.points++; result.observations += accepted.size(); result.error += sum; result.extraction_error += scaled;
        }
        out.seekp(0); detail::wr(out, result.points); out.flush();
        if (!out) throw std::runtime_error("cannot write regional points3D.bin");
    }
    fs::path by_image = sortObservations(images_records, scratch / "by-image", cache);
    {
        std::ifstream in(by_image, std::ios::binary); Observation row; bool have = nextRecord(in, row);
        std::ofstream out(output / "images.bin", std::ios::binary); detail::wr(out, result.images);
        for (const auto& kv : poses.images) {
            cancel::check(); const Image& im = kv.second; if (!im.registered) continue;
            FeatureSet features = readFeatures(feature_path(im.id).string(), false);
            std::vector<uint64_t> ids(features.count(), kInvalidPoint3D);
            while (have && uint32_t(row.key >> 32) < im.id) have = nextRecord(in, row);
            while (have && uint32_t(row.key >> 32) == im.id) {
                const uint32_t f = uint32_t(row.key); ids.at(f) = row.point + 1;
                if (row.xy.x != features.keypoints[f].x || row.xy.y != features.keypoints[f].y)
                    throw std::runtime_error("regional checkpoint feature coordinates changed");
                have = nextRecord(in, row);
            }
            detail::wr(out, im.id); Quat q = rotationToQuaternion(im.pose.R);
            for (double v : {q[0], q[1], q[2], q[3], im.pose.t.x, im.pose.t.y, im.pose.t.z}) detail::wr(out, v);
            detail::wr(out, im.camera_id); out.write(im.name.c_str(), im.name.size() + 1);
            detail::wr<uint64_t>(out, features.count());
            for (size_t f = 0; f < features.count(); ++f) {
                detail::wr<double>(out, features.keypoints[f].x); detail::wr<double>(out, features.keypoints[f].y); detail::wr(out, ids[f]);
            }
        }
        out.flush(); if (!out) throw std::runtime_error("cannot write regional images.bin");
    }
    uint64_t cumulative = 0;
    for (size_t bin = 0; bin < histogram.size(); ++bin) {
        cumulative += histogram[bin];
        if (cumulative > result.observations / 2) { result.median = (bin + 0.5) * radius / histogram.size(); break; }
    }
    {
        std::ofstream out(output / "cameras.bin", std::ios::binary); detail::wr<uint64_t>(out, poses.cameras.size());
        for (const auto& kv : poses.cameras) {
            const Camera& c = kv.second; detail::wr(out, c.id); detail::wr<int32_t>(out, camColmapId(c.model));
            detail::wr<uint64_t>(out, c.width); detail::wr<uint64_t>(out, c.height); double p[12]; packColmap(c, p);
            for (int k = 0; k < camColmapParams(c.model); ++k) detail::wr(out, p[k]);
        }
        out.flush(); if (!out) throw std::runtime_error("cannot write regional cameras.bin");
    }
    points.flush();
    for (const auto& kv : connected) if (kv.first == component(kv.first)) result.components++;
    result.error = result.observations ? result.error / result.observations : 0;
    result.extraction_error = result.observations ? result.extraction_error / result.observations : 0;
    return result;
}
} // namespace sfm::regional
