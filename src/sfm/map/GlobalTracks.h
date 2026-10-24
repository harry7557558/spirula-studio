// Tracks from the inliers of accepted relative-view edges.
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/map/RelativeViewGraph.h"

namespace sfm {

struct GlobalTracks {
    std::vector<TrackElement> observations;
    std::vector<size_t> offsets{0};
    size_t conflicting = 0;
    size_t too_short = 0;

    size_t size() const { return offsets.size() - 1; }
};

inline GlobalTracks buildGlobalTracks(const MatchesDatabase& db,
                                      const std::vector<FeatureSet>& features,
                                      const std::vector<RelativeViewEdge>& edges,
                                      size_t min_views = 2,
                                      const std::vector<uint8_t>* accepted = nullptr) {
    if (features.size() != db.images.size() || edges.size() != db.pairs.size() ||
        (accepted && accepted->size() != edges.size()))
        throw std::invalid_argument("global tracks: input counts differ");
    std::vector<size_t> offsets(features.size() + 1, 0);
    for (size_t i = 0; i < features.size(); i++) {
        offsets[i + 1] = offsets[i] + features[i].keypoints.size();
        if (offsets[i + 1] >= std::numeric_limits<uint32_t>::max())
            throw std::length_error("global tracks: too many features");
    }
    std::vector<uint32_t> vertices;
    for (size_t p = 0; p < db.pairs.size(); p++) {
        cancel::check();
        const RelativeViewEdge& edge = edges[p];
        if (edge.status != RelativeViewStatus::Accepted || (accepted && !(*accepted)[p])) continue;
        const TwoViewMatches& pair = db.pairs[p];
        if (pair.image1 >= features.size() || pair.image2 >= features.size() ||
            pair.image1 == pair.image2)
            throw std::invalid_argument("global tracks: invalid accepted pair");
        for (size_t k = 0; k < pair.matches.size(); k++) {
            if (!edge.isInlier(k)) continue;
            const FeatureMatch& m = pair.matches[k];
            if (m.idx1 >= features[pair.image1].keypoints.size() ||
                m.idx2 >= features[pair.image2].keypoints.size())
                throw std::invalid_argument("global tracks: invalid feature index");
            vertices.push_back((uint32_t)(offsets[pair.image1] + m.idx1));
            vertices.push_back((uint32_t)(offsets[pair.image2] + m.idx2));
        }
    }
    if (vertices.empty()) return {};
    const size_t link_count = vertices.size() / 2;
    std::sort(vertices.begin(), vertices.end());
    vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
    auto index = [&](uint32_t vertex) {
        return (uint32_t)(std::lower_bound(vertices.begin(), vertices.end(), vertex) - vertices.begin());
    };
    struct Link { uint32_t a, b, quality; };
    std::vector<uint32_t> parent(vertices.size()), support(vertices.size());
    std::vector<uint8_t> rank(vertices.size(), 0);
    std::vector<Link> links;
    links.reserve(link_count);
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](uint32_t v) {
        uint32_t r = v;
        while (parent[r] != r) r = parent[r];
        while (parent[v] != v) {
            const uint32_t next = parent[v];
            parent[v] = r;
            v = next;
        }
        return r;
    };
    for (size_t p = 0; p < db.pairs.size(); p++) {
        cancel::check();
        const RelativeViewEdge& edge = edges[p];
        if (edge.status != RelativeViewStatus::Accepted || (accepted && !(*accepted)[p])) continue;
        const TwoViewMatches& pair = db.pairs[p];
        for (size_t k = 0; k < pair.matches.size(); k++) {
            if (!edge.isInlier(k)) continue;
            const FeatureMatch& m = pair.matches[k];
            uint32_t a = index((uint32_t)(offsets[pair.image1] + m.idx1));
            uint32_t b = index((uint32_t)(offsets[pair.image2] + m.idx2));
            links.push_back({a, b, std::max<uint32_t>(1, edge.inliers)});
            support[a]++;
            support[b]++;
            a = root(a);
            b = root(b);
            if (a == b) continue;
            if (rank[a] < rank[b]) std::swap(a, b);
            parent[b] = a;
            if (rank[a] == rank[b]) rank[a]++;
        }
    }
    struct Item { uint32_t root, image, feature, vertex; };
    std::vector<Item> items;
    items.reserve(vertices.size());
    for (uint32_t vertex = 0; vertex < vertices.size(); vertex++) {
        const size_t global = vertices[vertex];
        const uint32_t image = (uint32_t)(std::upper_bound(offsets.begin(), offsets.end(), global) -
                                          offsets.begin() - 1);
        items.push_back({root(vertex), image, (uint32_t)(global - offsets[image]), vertex});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.root != b.root) return a.root < b.root;
        if (a.image != b.image) return a.image < b.image;
        return a.feature < b.feature;
    });
    struct RootLink { uint32_t root, a, b, quality; };
    std::vector<RootLink> root_links;
    root_links.reserve(links.size());
    for (const Link& link : links)
        root_links.push_back({root(link.a), link.a, link.b, link.quality});
    std::vector<Link>().swap(links);
    std::sort(root_links.begin(), root_links.end(), [](const RootLink& a, const RootLink& b) {
        if (a.root != b.root) return a.root < b.root;
        if (a.a != b.a) return a.a < b.a;
        return a.b < b.b;
    });
    GlobalTracks out;
    size_t link_begin = 0;
    for (size_t begin = 0; begin < items.size();) {
        size_t end = begin + 1;
        while (end < items.size() && items[end].root == items[begin].root) end++;
        while (link_begin < root_links.size() && root_links[link_begin].root < items[begin].root)
            link_begin++;
        size_t link_end = link_begin;
        while (link_end < root_links.size() && root_links[link_end].root == items[begin].root)
            link_end++;
        const size_t count = end - begin;
        std::vector<std::pair<uint32_t, uint32_t>> local_by_vertex;
        local_by_vertex.reserve(count);
        for (uint32_t i = 0; i < count; i++) local_by_vertex.push_back({items[begin + i].vertex, i});
        std::sort(local_by_vertex.begin(), local_by_vertex.end());
        const auto local = [&](uint32_t vertex) {
            return std::lower_bound(local_by_vertex.begin(), local_by_vertex.end(),
                                    std::make_pair(vertex, uint32_t{0}))->second;
        };
        struct WeightedLink { uint32_t a, b, quality, weight; };
        std::vector<WeightedLink> local_links;
        local_links.reserve(link_end - link_begin);
        for (size_t i = link_begin; i < link_end; i++) {
            const RootLink& link = root_links[i];
            local_links.push_back({local(link.a), local(link.b), link.quality,
                                   std::min(support[link.a], support[link.b])});
        }
        std::sort(local_links.begin(), local_links.end(), [](const WeightedLink& a, const WeightedLink& b) {
            if (a.quality != b.quality) return a.quality > b.quality;
            if (a.weight != b.weight) return a.weight > b.weight;
            if (a.a != b.a) return a.a < b.a;
            return a.b < b.b;
        });
        std::vector<uint32_t> local_parent(count);
        std::vector<uint8_t> local_rank(count, 0);
        std::iota(local_parent.begin(), local_parent.end(), 0);
        auto local_root = [&](uint32_t vertex) {
            uint32_t result = vertex;
            while (local_parent[result] != result) result = local_parent[result];
            while (local_parent[vertex] != vertex) {
                const uint32_t next = local_parent[vertex];
                local_parent[vertex] = result;
                vertex = next;
            }
            return result;
        };
        std::vector<std::vector<uint32_t>> representatives(count);
        for (uint32_t i = 0; i < count; i++) representatives[i].push_back(i);
        const auto better = [&](uint32_t a, uint32_t b) {
            const Item& ia = items[begin + a];
            const Item& ib = items[begin + b];
            return support[ia.vertex] > support[ib.vertex] ||
                   (support[ia.vertex] == support[ib.vertex] && ia.feature < ib.feature);
        };
        const auto compatible = [&](uint32_t a, uint32_t b) {
            const std::vector<uint32_t>& left = representatives[a];
            const std::vector<uint32_t>& right = representatives[b];
            size_t i = 0, j = 0;
            while (i < left.size() && j < right.size()) {
                const Item& li = items[begin + left[i]], ri = items[begin + right[j]];
                if (li.image < ri.image) { i++; continue; }
                if (ri.image < li.image) { j++; continue; }
                const Keypoint& x = features[li.image].keypoints[li.feature];
                const Keypoint& y = features[ri.image].keypoints[ri.feature];
                const double dx = x.x - y.x, dy = x.y - y.y;
                if (dx * dx + dy * dy > 100.0) return false;
                i++;
                j++;
            }
            return true;
        };
        const auto merge = [&](uint32_t a, uint32_t b) {
            std::vector<uint32_t> joined;
            joined.reserve(representatives[a].size() + representatives[b].size());
            size_t i = 0, j = 0;
            while (i < representatives[a].size() || j < representatives[b].size()) {
                if (j == representatives[b].size() ||
                    (i < representatives[a].size() &&
                     items[begin + representatives[a][i]].image < items[begin + representatives[b][j]].image)) {
                    joined.push_back(representatives[a][i++]);
                } else if (i == representatives[a].size() ||
                           items[begin + representatives[b][j]].image < items[begin + representatives[a][i]].image) {
                    joined.push_back(representatives[b][j++]);
                } else {
                    joined.push_back(better(representatives[a][i], representatives[b][j])
                                         ? representatives[a][i] : representatives[b][j]);
                    i++;
                    j++;
                }
            }
            representatives[a].swap(joined);
        };
        bool split = false;
        for (const WeightedLink& link : local_links) {
            uint32_t a = local_root(link.a), b = local_root(link.b);
            if (a == b) continue;
            if (!compatible(a, b)) { split = true; continue; }
            if (local_rank[a] < local_rank[b]) std::swap(a, b);
            merge(a, b);
            representatives[b].clear();
            local_parent[b] = a;
            if (local_rank[a] == local_rank[b]) local_rank[a]++;
        }
        if (split) out.conflicting++;
        for (uint32_t i = 0; i < count; i++) {
            if (local_root(i) != i) continue;
            const std::vector<uint32_t>& selected = representatives[i];
            if (selected.size() < std::max<size_t>(2, min_views)) {
                out.too_short++;
                continue;
            }
            for (uint32_t selected_index : selected) {
                const Item& item = items[begin + selected_index];
                out.observations.push_back({item.image, item.feature});
            }
            out.offsets.push_back(out.observations.size());
        }
        link_begin = link_end;
        begin = end;
    }
    return out;
}

}  // namespace sfm
