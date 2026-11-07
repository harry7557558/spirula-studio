#pragma once

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "sfm/core/Matches.h"

namespace sfm {

inline std::vector<std::pair<uint32_t, uint32_t>> expandVerifiedPairs(
    const MatchesDatabase& db,
    const std::vector<std::pair<uint32_t, uint32_t>>& attempted,
    const std::vector<std::pair<uint32_t, uint32_t>>& retrieval, uint32_t budget,
    const std::vector<uint32_t>& retrieval_scores = {}) {
    const size_t n = db.images.size();
    std::vector<uint32_t> parent(n), used(n, 0), degree(n, 0);
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](uint32_t i) {
        while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; }
        return i;
    };
    std::set<std::pair<uint32_t, uint32_t>> seen;
    for (auto p : attempted) seen.emplace(std::min(p.first, p.second), std::max(p.first, p.second));
    std::vector<std::vector<std::pair<size_t, uint32_t>>> neighbors(n);
    for (const auto& p : db.pairs) {
        if (p.image1 >= n || p.image2 >= n || p.image1 == p.image2) continue;
        seen.emplace(std::min(p.image1, p.image2), std::max(p.image1, p.image2));
        parent[root(p.image1)] = root(p.image2);
        neighbors[p.image1].emplace_back(p.matches.size(), p.image2);
        neighbors[p.image2].emplace_back(p.matches.size(), p.image1);
    }
    for (size_t i = 0; i < n; ++i) {
        degree[i] = static_cast<uint32_t>(neighbors[i].size());
        std::sort(neighbors[i].begin(), neighbors[i].end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        if (neighbors[i].size() > 16) neighbors[i].resize(16);
    }
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> support;
    for (uint32_t i = 0; i < n; ++i)
        for (const auto& j : neighbors[i])
            for (const auto& k : neighbors[j.second]) {
                if (i == k.second) continue;
                const auto p = std::minmax(i, k.second);
                if (!seen.count(p)) ++support[p];
            }
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> scores;
    for (size_t i = 0; i < retrieval.size(); ++i) {
        const auto& p = retrieval[i];
        if (p.first >= n || p.second >= n || p.first == p.second) continue;
        const auto key = std::minmax(p.first, p.second);
        if (i < retrieval_scores.size()) scores[key] = retrieval_scores[i];
        if (!seen.count(key) && (root(p.first) != root(p.second) ||
                                degree[p.first] < 12 || degree[p.second] < 12))
            support.try_emplace(key, 0);
    }
    struct Candidate { uint32_t a, b, score, retrieval_score; bool bridge, weak; };
    std::vector<Candidate> candidates;
    for (const auto& p : support)
        candidates.push_back({p.first.first, p.first.second, p.second, scores[p.first],
                              root(p.first.first) != root(p.first.second),
                              std::min(degree[p.first.first], degree[p.first.second]) < 12});
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.bridge != b.bridge) return a.bridge > b.bridge;
        if (a.weak != b.weak) return a.weak > b.weak;
        if (a.score != b.score) return a.score > b.score;
        if (a.retrieval_score != b.retrieval_score) return a.retrieval_score > b.retrieval_score;
        return std::make_pair(a.a, a.b) < std::make_pair(b.a, b.b);
    });
    std::vector<std::pair<uint32_t, uint32_t>> out;
    for (const auto& c : candidates) {
        if (used[c.a] >= budget || used[c.b] >= budget) continue;
        out.emplace_back(c.a, c.b);
        ++used[c.a];
        ++used[c.b];
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace sfm
