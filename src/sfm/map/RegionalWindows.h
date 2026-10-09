#pragma once

#include "sfm/map/RegionalLandmarks.h"
#include <array>
#include <queue>
#include "sfm/core/Cancel.h"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace sfm::regional {
struct BAWindow { std::set<uint32_t> core, images, support; };

class WindowGraph {
public:
    explicit WindowGraph(const Reconstruction& rec, size_t memory) {
        for (const auto& im : rec.images) if (im.second.registered) centres_[im.first] = cameraCenter(im.second.pose);
        const size_t allowance = memory / std::max<size_t>(1, centres_.size());
        if (allowance < 1024) throw std::length_error("regional window graph exceeds working budget");
        degree_ = std::min<size_t>(32, (allowance - 256) / 128);
        for (const auto& item : rec.points3D) {
            cancel::check(); std::vector<uint32_t> ids;
            for (const auto& e : item.second.track) if (centres_.count(e.image_id)) ids.push_back(e.image_id);
            std::sort(ids.begin(), ids.end(), [&](uint32_t a, uint32_t b) { return less(a, b); });
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            for (size_t k = 0; k < ids.size(); ++k) {
                uint32_t a = ids[k], b = ids[(k + 1) % ids.size()];
                if (a != b) { vote(a, b); vote(b, a); }
            }
        }
    }

    std::set<uint32_t> remaining() const {
        std::set<uint32_t> out; for (const auto& kv : centres_) out.insert(kv.first); return out;
    }

    BAWindow select(const std::set<uint32_t>& remaining, size_t limit, size_t halo) const {
        if (!limit || remaining.empty()) throw std::invalid_argument("empty regional BA window");
        BAWindow out;
        uint32_t seed = *std::min_element(remaining.begin(), remaining.end(), [&](uint32_t a, uint32_t b) { return less(a, b); });
        std::map<uint32_t, uint64_t> frontier;
        auto add = [&](uint32_t id) {
            out.core.insert(id); frontier.erase(id);
            auto found = edges_.find(id);
            if (found != edges_.end()) for (const auto& e : found->second)
                if (remaining.count(e.first) && !out.core.count(e.first)) frontier[e.first] += e.second;
        };
        add(seed);
        auto better = [&](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            double da = (centres_.at(a.first) - centres_.at(seed)).norm();
            double db = (centres_.at(b.first) - centres_.at(seed)).norm();
            return da != db ? da < db : less(a.first, b.first);
        };
        while (out.core.size() < limit && !frontier.empty()) {
            auto best = std::min_element(frontier.begin(), frontier.end(), better); add(best->first);
        }
        std::map<uint32_t, uint64_t> support;
        for (uint32_t id : out.core) {
            auto found = edges_.find(id);
            if (found != edges_.end()) for (const auto& e : found->second)
                if (!out.core.count(e.first)) support[e.first] += e.second;
        }
        std::vector<std::pair<uint32_t, uint64_t>> ranked(support.begin(), support.end());
        std::sort(ranked.begin(), ranked.end(), better);
        for (size_t k = 0; k < std::min(halo, ranked.size()); ++k) out.support.insert(ranked[k].first);
        out.images = out.core; out.images.insert(out.support.begin(), out.support.end()); return out;
    }

private:
    bool less(uint32_t a, uint32_t b) const {
        const auto& ca = centres_.at(a); const auto& cb = centres_.at(b);
        return std::tie(ca.x, ca.y, ca.z, a) < std::tie(cb.x, cb.y, cb.z, b);
    }
    void vote(uint32_t a, uint32_t b) {
        auto& row = edges_[a]; auto found = row.find(b);
        if (found != row.end()) { ++found->second; return; }
        if (row.size() < degree_) { row[b] = 1; return; }
        for (auto it = row.begin(); it != row.end();) {
            if (--it->second == 0) it = row.erase(it); else ++it;
        }
    }
    size_t degree_ = 32;
    std::map<uint32_t, Vec3> centres_;
    std::map<uint32_t, std::map<uint32_t, uint64_t>> edges_;
};
inline Reconstruction windowModel(const Reconstruction& rec, const std::set<uint32_t>& images,
                           std::set<uint64_t>& fixed, int threads = 0) {
    Reconstruction sub; sub.cameras = rec.cameras; sub.next_point3D_id = rec.next_point3D_id;
    std::set<uint64_t> selected = fixed;
    std::vector<uint32_t> ids(images.begin(), images.end()); std::vector<std::vector<uint64_t>> samples(ids.size());
    auto& pool = bacpu::Pool::get(); const int workers = threads > 0 ? std::min(threads, pool.size()) : pool.size();
    pool.runChecked(int(ids.size()), workers, [&](int task, int) {
        cancel::check(); uint32_t id = ids[task];
        const auto& im = rec.images.at(id); const auto& cam = rec.cameras.at(im.camera_id);
        std::array<std::priority_queue<std::pair<uint64_t, uint64_t>>, 256> cells;
        for (size_t f = 0; f < im.point3D_ids.size(); ++f) {
            uint64_t point = im.point3D_ids[f]; if (point == kInvalidPoint3D || !rec.points3D.count(point)) continue;
            int x = std::clamp(int(im.points2D[f].x * 16 / std::max(1, cam.width)), 0, 15);
            int y = std::clamp(int(im.points2D[f].y * 16 / std::max(1, cam.height)), 0, 15);
            auto& q = cells[y * 16 + x]; q.emplace(regional::sharedHash(point), point); if (q.size() > 8) q.pop();
        }
        for (auto& q : cells) while (!q.empty()) { samples[task].push_back(q.top().second); q.pop(); }
    });
    for (const auto& sample : samples) selected.insert(sample.begin(), sample.end());
    for (uint32_t i : images) {
        auto found = rec.images.find(i); if (found == rec.images.end() || !found->second.registered) continue;
        sub.images[i] = found->second;
        std::fill(sub.images[i].point3D_ids.begin(), sub.images[i].point3D_ids.end(), kInvalidPoint3D);
    }
    for (uint64_t id : selected) {
        auto found = rec.points3D.find(id); if (found == rec.points3D.end()) continue;
        const auto& kv = *found;
        Point3D p; p.xyz = kv.second.xyz; p.error = kv.second.error; std::copy(kv.second.rgb, kv.second.rgb + 3, p.rgb);
        for (const auto& e : kv.second.track) if (sub.images.count(e.image_id)) p.track.push_back(e);
        if (p.track.empty()) continue;
        if (p.track.size() != kv.second.track.size()) fixed.insert(kv.first);
        if (p.track.size() < 2 && !fixed.count(kv.first)) continue;
        for (const auto& e : p.track) sub.images[e.image_id].point3D_ids.at(e.point2D_idx) = kv.first;
        sub.points3D.emplace(kv.first, std::move(p));
    }
    return sub;
}

} // namespace sfm::regional
