// Pairs the GPS proposes: two images taken within a radius of each other are
// matched whatever they look like, which is loop closure on an outdoor walk
// without a shortlist having to find it. Only images a source positions
// take part; the rest of the pair list is untouched.
#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "sfm/core/PriorSource.h"

namespace sfm {

// Each positioned image's `max_per_image` nearest others within `radius`
// metres, as unordered (i < j) pairs, sorted and unique. `positioned` gets
// how many images had a position.
inline std::vector<std::pair<uint32_t, uint32_t>> gpsProximityPairs(const PriorSource& src,
                                                                    uint32_t num_images,
                                                                    double radius,
                                                                    size_t max_per_image,
                                                                    size_t* positioned = nullptr) {
    std::vector<uint32_t> ids;
    std::vector<Vec3> pos;
    for (uint32_t i = 0; i < num_images; i++) {
        Vec3 p;
        if (!src.position(i, p)) continue;
        ids.push_back(i);
        pos.push_back(p);
    }
    if (positioned) *positioned = ids.size();
    std::vector<std::pair<uint32_t, uint32_t>> out;
    if (ids.size() < 2 || !(radius > 0)) return out;
    const double r2 = radius * radius;
    std::vector<std::pair<double, uint32_t>> nearby;
    for (size_t a = 0; a < ids.size(); a++) {
        nearby.clear();
        for (size_t b = 0; b < ids.size(); b++) {
            if (a == b) continue;
            const Vec3 d = pos[a] - pos[b];
            const double dd = d.dot(d);
            if (dd <= r2) nearby.push_back({dd, ids[b]});
        }
        if (nearby.size() > max_per_image) {
            std::partial_sort(nearby.begin(), nearby.begin() + (long)max_per_image, nearby.end());
            nearby.resize(max_per_image);
        }
        for (const auto& n : nearby)
            out.emplace_back(std::min(ids[a], n.second), std::max(ids[a], n.second));
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

}  // namespace sfm
