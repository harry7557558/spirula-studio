#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "sfm/core/Features.h"

namespace sfm {

inline std::vector<uint32_t> spatiallyBalancedFeatures(
    const std::vector<Keypoint>& keypoints, const std::vector<uint32_t>& ranked,
    int width, int height, uint32_t grid, size_t count) {
    if (!grid || width <= 0 || height <= 0) return {};
    std::vector<std::vector<uint32_t>> cells(static_cast<size_t>(grid) * grid);
    for (uint32_t i : ranked) {
        const auto& p = keypoints.at(i);
        const uint32_t x = static_cast<uint32_t>(std::clamp(p.x * grid / width, 0.0f, float(grid - 1)));
        const uint32_t y = static_cast<uint32_t>(std::clamp(p.y * grid / height, 0.0f, float(grid - 1)));
        cells[y * grid + x].push_back(i);
    }
    std::vector<uint32_t> selected;
    count = std::min(count, ranked.size());
    selected.reserve(count);
    for (size_t depth = 0; selected.size() < count; ++depth)
        for (const auto& cell : cells) {
            if (depth < cell.size()) selected.push_back(cell[depth]);
            if (selected.size() == count) break;
        }
    return selected;
}

} // namespace sfm
