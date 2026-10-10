#pragma once

#include "sfm/map/RegionalStore.h"
#include "sfm/ba/CpuParallel.h"
#include <array>
#include <set>
#include <sstream>

namespace sfm::regional {

inline std::string regionalBaseSignature(const std::string& settings) {
    std::istringstream in(settings); std::string line, out;
    while (std::getline(in, line)) {
        if (line.rfind("regional-iterations=", 0) == 0 || line.rfind("regional-cost-tolerance=", 0) == 0 ||
            line.rfind("regional-landmarks=", 0) == 0) continue;
        out += line + "\n";
    }
    return out;
}

struct SharedMember { uint32_t model = 0; uint64_t point = 0; Vec3 xyz; };
struct SharedObservation { uint64_t key = 0; Vec2 xy; };
struct SharedLandmark {
    uint64_t key = 0;
    Vec3 xyz;
    bool valid = true;
    std::vector<SharedMember> members;
    std::vector<SharedObservation> observations;
};
struct SharedIndex {
    size_t models = 0;
    uint64_t candidates = 0, conflicts = 0;
    std::vector<SharedLandmark> landmarks;
    std::vector<SharedLandmark> alignment_reference;
    std::vector<std::vector<std::pair<uint64_t, size_t>>> by_model;
    void rebuild() {
        by_model.assign(models, {});
        for (size_t i = 0; i < landmarks.size(); ++i)
            if (landmarks[i].valid) for (const auto& m : landmarks[i].members) {
                if (m.model >= models) throw std::runtime_error("shared landmark model out of range");
                by_model[m.model].emplace_back(m.point, i);
            }
        for (auto& v : by_model) {
            std::sort(v.begin(), v.end());
            for (size_t i = 1; i < v.size(); ++i)
                if (v[i - 1].first == v[i].first) throw std::runtime_error("ambiguous local shared landmark");
        }
    }
    std::set<uint64_t> inject(size_t model, Reconstruction& rec) const {
        std::set<uint64_t> fixed;
        for (const auto& item : by_model.at(model)) {
            auto p = rec.points3D.find(item.first);
            if (p == rec.points3D.end()) throw std::runtime_error("shared landmark checkpoint identity changed");
            p->second.xyz = landmarks[item.second].xyz; fixed.insert(item.first);
        }
        return fixed;
    }
};

inline uint64_t sharedHash(uint64_t x) {
    x += 0x9e3779b97f4a7c15ull; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull; return x ^ (x >> 31);
}

template<class ReadModel> SharedIndex buildSharedIndex(size_t models, ReadModel read,
    const fs::path& scratch, size_t cache, size_t memory_limit, size_t max_points = 100000) {
    fs::create_directories(scratch);
    std::map<uint32_t, uint32_t> uses;
    for (size_t m = 0; m < models; ++m) {
        cancel::check(); auto rec = read(m);
        for (const auto& im : rec.images) if (im.second.registered) ++uses[im.first];
    }
    struct Fragment { SharedMember member; uint64_t observations = 0; };
    SharedIndex result; result.models = models;
    PointUnion points(scratch / "union.bin", cache / 2);
    PagedArray<Fragment> fragments(scratch / "fragments.bin", cache / 2);
    const auto records = scratch / "boundary-observations.bin";
    {
        std::ofstream out(records, std::ios::binary);
        for (size_t m = 0; m < models; ++m) {
            cancel::check(); auto rec = read(m);
            for (const auto& item : rec.points3D) {
                const auto& p = item.second;
                if (std::none_of(p.track.begin(), p.track.end(), [&](const auto& e) { return uses[e.image_id] > 1; })) continue;
                uint64_t id = points.add(p);
                if (id != fragments.append({{uint32_t(m), item.first, p.xyz}, p.track.size()}))
                    throw std::runtime_error("shared fragment identity mismatch");
                for (const auto& e : p.track) if (uses[e.image_id] > 1)
                    record(out, Observation{(uint64_t(e.image_id) << 32) | e.point2D_idx, id});
            }
        }
    }
    auto sorted = sortObservations(records, scratch / "by-feature", cache);
    {
        std::ifstream in(sorted, std::ios::binary); Observation row, previous; bool have = false;
        uint64_t count = 0;
        while (nextRecord(in, row)) {
            if (++count % 65536 == 0) cancel::check();
            if (have && row.key == previous.key) points.join(previous.point, row.point);
            previous = row; have = true;
        }
    }
    const auto groups = scratch / "groups.bin";
    {
        std::ofstream out(groups, std::ios::binary);
        for (uint64_t i = 0; i < fragments.size(); ++i) {
            if (i % 65536 == 0) cancel::check();
            uint64_t root = points.root(i);
            if (points.nodes.get(root).fragments > 1) record(out, Observation{root, i});
        }
    }
    auto ordered = sortObservations(groups, scratch / "by-group", cache);
    struct Candidate {
        uint64_t hash = 0, bytes = 0; SharedLandmark point;
        bool operator<(const Candidate& other) const { return hash != other.hash ? hash < other.hash : point.key < other.point.key; }
    };
    auto visit = [&](auto consume, bool census) {
        std::ifstream in(ordered, std::ios::binary); Observation row; bool have = nextRecord(in, row);
        while (have) {
            cancel::check(); Candidate c; c.point.key = row.key; c.hash = sharedHash(row.key);
            std::set<uint32_t> member_models; bool conflict = false;
            do {
                Fragment f = fragments.get(row.point);
                if (!member_models.insert(f.member.model).second) conflict = true;
                c.point.members.push_back(f.member);
                c.bytes += 2 * f.observations * sizeof(SharedObservation) + 128;
                have = nextRecord(in, row);
            } while (have && row.key == c.point.key);
            if (conflict) { if (census) ++result.conflicts; continue; }
            if (member_models.size() < 2) continue;
            if (census) ++result.candidates;
            for (const auto& m : c.point.members) c.point.xyz = c.point.xyz + m.xyz;
            c.point.xyz = c.point.xyz * (1. / c.point.members.size());
            if (!std::isfinite(c.point.xyz.norm())) throw std::runtime_error("nonfinite regional landmark candidate");
            consume(std::move(c));
        }
    };
    using Edge = std::pair<uint32_t, uint32_t>;
    std::map<Edge, uint64_t> edge_counts;
    Vec3 lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
    visit([&](Candidate c) {
        for (size_t a = 0; a < c.point.members.size(); ++a)
            for (size_t b = a + 1; b < c.point.members.size(); ++b) {
                uint32_t x = c.point.members[a].model, y = c.point.members[b].model;
                ++edge_counts[std::minmax(x, y)];
            }
        const auto& p = c.point.xyz;
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        if (edge_counts.size() > memory_limit / 512) throw std::length_error("regional landmark overlap graph exceeds budget");
    }, true);
    auto primary = [&](const Candidate& c) {
        Edge best{UINT32_MAX, UINT32_MAX}; uint64_t count = UINT64_MAX;
        for (size_t a = 0; a < c.point.members.size(); ++a)
            for (size_t b = a + 1; b < c.point.members.size(); ++b) {
                Edge edge = std::minmax(c.point.members[a].model, c.point.members[b].model);
                uint64_t n = edge_counts.at(edge);
                if (n < count || (n == count && edge < best)) { best = edge; count = n; }
            }
        return best;
    };
    using Stratum = std::pair<Edge, uint32_t>;
    unsigned bins = 4;
    auto stratum = [&](const Candidate& c) {
        auto cell = [&](double p, double min, double max) {
            return max > min ? unsigned(std::clamp((p - min) / (max - min) * bins, 0., double(bins - 1))) : 0u;
        };
        const auto& p = c.point.xyz;
        return Stratum{primary(c), cell(p.x, lo.x, hi.x) + bins * (cell(p.y, lo.y, hi.y) + bins * cell(p.z, lo.z, hi.z))};
    };
    std::set<Stratum> strata;
    const size_t selection_memory = memory_limit - memory_limit / 4;
    const size_t reserved_points = max_points / 2;
    const size_t reserved_memory = selection_memory / 2;
    const size_t slots = std::min(reserved_points, reserved_memory / 1024);
    if (result.candidates && !slots) throw std::length_error("regional landmark budget too small for seam coverage");
    std::map<Edge, size_t> edge_cells;
    while (result.candidates) {
        strata.clear();
        visit([&](Candidate c) { if (strata.size() <= slots) strata.insert(stratum(c)); }, false);
        edge_cells.clear(); size_t most_cells = 0;
        for (const auto& bucket : strata) most_cells = std::max(most_cells, ++edge_cells[bucket.first]);
        if (strata.size() <= slots && most_cells <= slots / edge_cells.size()) break;
        if (bins == 1) throw std::length_error("regional landmark budget too small for overlap graph coverage");
        bins /= 2;
    }
    struct Reservoir { std::priority_queue<Candidate> points; uint64_t bytes = 0; };
    auto insert = [&](Reservoir& r, Candidate c, size_t cap, size_t allowance) {
        r.bytes += c.bytes; r.points.push(std::move(c));
        while (!r.points.empty() && (r.points.size() > cap || r.bytes > allowance)) {
            r.bytes -= r.points.top().bytes; r.points.pop();
        }
    };
    Reservoir global, uniform_sample;
    std::map<Stratum, Reservoir> balanced;
    const size_t balanced_points = std::min(reserved_points, edge_cells.size() * size_t(128));
    if (!strata.empty()) visit([&](Candidate c) {
        auto key = stratum(c); size_t cells = edge_cells.at(key.first);
        insert(balanced[key], std::move(c), balanced_points / edge_cells.size() / cells, reserved_memory / edge_cells.size() / cells);
    }, false);
    std::set<uint64_t> emitted;
    uint64_t selected_bytes = 0;
    auto drain = [&](Reservoir& r) {
        while (!r.points.empty()) {
            const auto& p = r.points.top().point;
            if (emitted.insert(p.key).second) { selected_bytes += r.points.top().bytes; result.landmarks.push_back(p); }
            r.points.pop();
        }
    };
    for (auto& bucket : balanced) drain(bucket.second);
    const size_t remaining_points = max_points - result.landmarks.size(), remaining_bytes = selection_memory - size_t(selected_bytes);
    visit([&](Candidate c) {
        Candidate reference = c;
        reference.bytes = 2 * sizeof(Candidate) + reference.point.members.capacity() * sizeof(SharedMember);
        insert(uniform_sample, std::move(reference), max_points, memory_limit / 4);
        if (!emitted.count(c.point.key)) insert(global, std::move(c), remaining_points, remaining_bytes);
    }, false);
    drain(global);
    std::sort(result.landmarks.begin(), result.landmarks.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
    result.rebuild();
    SharedIndex uniform; uniform.models = models;
    while (!uniform_sample.points.empty()) {
        uniform.landmarks.push_back(uniform_sample.points.top().point); uniform_sample.points.pop();
    }
    std::sort(uniform.landmarks.begin(), uniform.landmarks.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
    uniform.rebuild();
    const fs::path reference_rows = scratch / "alignment-reference.bin";
    std::ofstream reference_out(reference_rows, std::ios::binary);
    if (!reference_out) throw std::runtime_error("cannot create alignment reference observations");
    for (size_t m = 0; m < models; ++m) {
        cancel::check(); auto rec = read(m);
        for (const auto& item : uniform.by_model[m])
            for (const auto& e : rec.points3D.at(item.first).track)
                record(reference_out, Observation{uint64_t(item.second), (uint64_t(e.image_id) << 32) | e.point2D_idx});
        for (const auto& item : result.by_model[m]) {
            auto found = rec.points3D.find(item.first);
            if (found == rec.points3D.end()) throw std::runtime_error("shared point disappeared during indexing");
            for (const auto& e : found->second.track) {
                const auto& im = rec.images.at(e.image_id);
                if (!im.registered || e.point2D_idx >= im.points2D.size()) throw std::runtime_error("invalid shared observation");
                result.landmarks[item.second].observations.push_back({(uint64_t(e.image_id) << 32) | e.point2D_idx, im.points2D[e.point2D_idx]});
            }
        }
    }
    reference_out.close();
    if (!reference_out) throw std::runtime_error("cannot flush alignment reference observations");
    auto sorted_reference = sortObservations(reference_rows, scratch / "reference-by-image", cache);
    {
        std::ifstream in(sorted_reference, std::ios::binary); Observation row, previous; bool have = false;
        while (nextRecord(in, row)) {
            if (have && row.key == previous.key && row.point != previous.point && (row.point >> 32) == (previous.point >> 32))
                uniform.landmarks.at(size_t(row.key)).valid = false;
            previous = row; have = true;
        }
    }
    for (auto& p : result.landmarks) {
        auto& v = p.observations;
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
        v.erase(std::unique(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.key == b.key; }), v.end());
        for (size_t i = 1; i < v.size(); ++i)
            if ((v[i - 1].key >> 32) == (v[i].key >> 32)) p.valid = false;
        if (!p.valid) { ++result.conflicts; p.observations.clear(); }
    }
    result.alignment_reference = std::move(uniform.landmarks);
    result.rebuild(); points.flush(); fragments.flush(); return result;
}

inline void writeSharedIndex(const fs::path& path, const SharedIndex& index) {
    model_store_detail::Writer out(path.string() + ".part");
    out.scalar<uint64_t>(0x31444e414c535353ull); out.scalar<uint64_t>(index.models);
    out.scalar(index.candidates); out.scalar(index.conflicts); out.scalar<uint64_t>(index.landmarks.size());
    for (const auto& p : index.landmarks) {
        out.scalar(p.key); out.scalar(p.xyz); out.scalar<uint8_t>(p.valid);
        out.vector(p.members); out.vector(p.observations);
    }
    out.finish(); replaceFile(path.string() + ".part", path);
}

inline SharedIndex readSharedIndex(const fs::path& path, size_t models, size_t budget) {
    if (fs::file_size(path) > budget) throw std::runtime_error("shared landmark checkpoint exceeds memory budget");
    model_store_detail::Reader in(path);
    if (in.scalar<uint64_t>() != 0x31444e414c535353ull || in.scalar<uint64_t>() != models)
        throw std::runtime_error("incompatible shared landmark checkpoint");
    SharedIndex out; out.models = models; out.candidates = in.scalar<uint64_t>(); out.conflicts = in.scalar<uint64_t>();
    out.landmarks.resize(in.count(64));
    std::set<uint64_t> keys;
    for (auto& p : out.landmarks) {
        p.key = in.scalar<uint64_t>(); p.xyz = in.scalar<Vec3>(); p.valid = in.scalar<uint8_t>() != 0;
        p.members = in.vector<SharedMember>(); p.observations = in.vector<SharedObservation>();
        if (!keys.insert(p.key).second || !std::isfinite(p.xyz.norm()) || p.members.size() < 2)
            throw std::runtime_error("invalid shared landmark checkpoint");
        std::set<uint32_t> seen;
        for (const auto& m : p.members)
            if (!seen.insert(m.model).second || !std::isfinite(m.xyz.norm())) throw std::runtime_error("invalid shared member");
        uint64_t last = UINT64_MAX;
        for (const auto& obs : p.observations) {
            if ((last != UINT64_MAX && (obs.key <= last || (obs.key >> 32) == (last >> 32))) ||
                !std::isfinite(obs.xy.x) || !std::isfinite(obs.xy.y)) throw std::runtime_error("invalid shared observation order");
            last = obs.key;
        }
    }
    in.finish(); out.rebuild(); return out;
}

inline double sharedCost(const SharedIndex& index, const Reconstruction& poses, double gate) {
    double sum = 0;
    for (const auto& p : index.landmarks) if (p.valid) for (const auto& obs : p.observations) {
        const auto& im = poses.images.at(uint32_t(obs.key >> 32)); const auto& cam = poses.cameras.at(im.camera_id);
        double e = reprojErrorAt(cam, im.pose, obs.xy, p.xyz) / cam.pixel_scale;
        if (!std::isfinite(e)) throw std::runtime_error("nonfinite shared landmark residual");
        sum += e <= gate ? e * e : gate * (2 * e - gate);
    }
    return sum;
}

inline void triangulateShared(SharedIndex& index, const Reconstruction& poses, double gate) {
    std::vector<Observation> track;
    for (auto& p : index.landmarks) if (p.valid) {
        cancel::check(); track.clear();
        for (const auto& obs : p.observations) track.push_back({0, obs.key, obs.xy});
        Vec3 updated = refineUnifiedPoint(p.xyz, track, poses, gate);
        if (!std::isfinite(updated.norm())) throw std::runtime_error("shared triangulation produced nonfinite coordinates");
        p.xyz = updated;
    }
}

inline void triangulatePrivate(Reconstruction& rec, const std::set<uint64_t>& shared, double gate, int threads = 0) {
    auto& pool = bacpu::Pool::get(); const int workers = threads > 0 ? std::min(threads, pool.size()) : pool.size();
    std::vector<Point3D*> points; points.reserve(4096);
    const auto& poses = rec;
    auto refine = [&] {
        const int tasks = int((points.size() + 63) / 64);
        pool.runChecked(tasks, workers, [&](int task, int) {
            std::vector<Observation> track;
            for (size_t k = size_t(task) * 64; k < std::min(points.size(), (size_t(task) + 1) * 64); ++k) {
                cancel::check(); auto& point = *points[k]; track.clear();
                for (const auto& e : point.track) {
                    const auto& im = poses.images.at(e.image_id);
                    if (!im.registered || e.point2D_idx >= im.points2D.size()) throw std::runtime_error("invalid private triangulation track");
                    track.push_back({0, (uint64_t(e.image_id) << 32) | e.point2D_idx, im.points2D[e.point2D_idx]});
                }
                Vec3 updated = refineUnifiedPoint(point.xyz, track, poses, gate);
                if (!std::isfinite(updated.norm())) throw std::runtime_error("private triangulation produced nonfinite coordinates");
                point.xyz = updated;
            }
        });
        points.clear();
    };
    for (auto& item : rec.points3D) {
        if (shared.count(item.first) || item.second.track.size() < 2) continue;
        points.push_back(&item.second); if (points.size() == 4096) refine();
    }
    refine();
}

} // namespace sfm::regional
