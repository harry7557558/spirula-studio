#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sfm/geometry/LinAlg.h"

namespace sfm {

struct SpatialBlockPlan {
    std::vector<uint32_t> owner;
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    size_t blocks = 0;
};

class SpatialBlockIndex {
public:
    explicit SpatialBlockIndex(const std::vector<Vec3>& positions) {
        if (positions.size() > UINT32_MAX) throw std::runtime_error("too many GPS positions");
        points_.reserve(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
            const Vec3& p = positions[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                throw std::runtime_error("invalid GPS position for image " + std::to_string(i));
            points_.push_back({p.x, p.y, (uint32_t)i});
        }
        build(0, points_.size(), 0);
    }

    SpatialBlockPlan plan(size_t block_size, size_t neighbours, double radius,
                          const std::function<void(size_t, size_t)>& progress = {}) const {
        if (block_size < 2 || neighbours == 0 || radius < 0 || !std::isfinite(radius))
            throw std::runtime_error("invalid spatial block options");
        SpatialBlockPlan out;
        out.owner.resize(points_.size());
        out.blocks = (points_.size() + block_size - 1) / block_size;
        for (size_t i = 0; i < points_.size(); ++i)
            out.owner[points_[i].id] = (uint32_t)(i / block_size);
        const double cap = radius > 0 ? radius * radius : std::numeric_limits<double>::infinity();
        size_t done = 0;
        for (const Point& p : points_) {
            Heap heap;
            query(p, 0, points_.size(), 0, neighbours, cap, heap);
            while (!heap.empty()) {
                const uint32_t j = heap.top().second;
                out.pairs.emplace_back(std::min(p.id, j), std::max(p.id, j));
                heap.pop();
            }
            ++done;
            if (progress && (done % 128 == 0 || done == points_.size()))
                progress(done, points_.size());
        }
        std::sort(out.pairs.begin(), out.pairs.end());
        out.pairs.erase(std::unique(out.pairs.begin(), out.pairs.end()), out.pairs.end());
        return out;
    }

private:
    struct Point { double x, y; uint32_t id; };
    using Heap = std::priority_queue<std::pair<double, uint32_t>>;
    std::vector<Point> points_;

    void build(size_t b, size_t e, int axis) {
        if (b >= e) return;
        const size_t m = b + (e - b) / 2;
        std::nth_element(points_.begin() + b, points_.begin() + m, points_.begin() + e,
                         [axis](const Point& a, const Point& c) {
                             const double av = axis ? a.y : a.x, cv = axis ? c.y : c.x;
                             return av != cv ? av < cv : a.id < c.id;
                         });
        build(b, m, 1 - axis);
        build(m + 1, e, 1 - axis);
    }

    void query(const Point& p, size_t b, size_t e, int axis, size_t k, double cap,
               Heap& heap) const {
        if (b >= e) return;
        const size_t m = b + (e - b) / 2;
        const Point& q = points_[m];
        const double dx = p.x - q.x, dy = p.y - q.y;
        const double d2 = dx * dx + dy * dy;
        if (p.id != q.id && d2 <= cap) {
            const std::pair<double, uint32_t> candidate{d2, q.id};
            if (heap.size() < k) heap.push(candidate);
            else if (candidate < heap.top()) { heap.pop(); heap.push(candidate); }
        }
        const double d = axis ? dy : dx;
        if (d <= 0) query(p, b, m, 1 - axis, k, cap, heap);
        else query(p, m + 1, e, 1 - axis, k, cap, heap);
        const double limit = heap.size() < k ? cap : std::min(cap, heap.top().first);
        // Equality keeps the lower image id when two candidates are equidistant.
        if (d * d <= limit) {
            if (d <= 0) query(p, m + 1, e, 1 - axis, k, cap, heap);
            else query(p, b, m, 1 - axis, k, cap, heap);
        }
    }
};

inline void orderSpatialPairs(std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                               const std::vector<uint32_t>& owner) {
    std::sort(pairs.begin(), pairs.end(), [&](const auto& a, const auto& b) {
        const uint32_t ab = std::min(owner.at(a.first), owner.at(a.second));
        const uint32_t bb = std::min(owner.at(b.first), owner.at(b.second));
        return ab != bb ? ab < bb : a < b;
    });
}

}  // namespace sfm
