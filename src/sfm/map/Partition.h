// The verified view graph as a graph to cut: one node per image, edge weight
// the inlier matches a pair kept. The cut itself is core/GraphCut.h, shared
// with the dataset partitioner; nothing here looks at geometry, so it runs
// before any reconstruction exists.
#pragma once

#include <cstdint>
#include <vector>

#include "core/GraphCut.h"
#include "sfm/core/Matches.h"

namespace sfm {

using ViewGraph = spirula::graph::WeightedGraph;

inline ViewGraph buildViewGraph(const MatchesDatabase& db) {
    const size_t n = db.images.size();
    std::vector<spirula::graph::Edge> edges;
    edges.reserve(db.pairs.size());
    for (const TwoViewMatches& p : db.pairs) {
        if (p.matches.empty() || p.image1 >= n || p.image2 >= n) continue;
        edges.push_back({p.image1, p.image2, (double)p.matches.size()});
    }
    return spirula::graph::build_graph(n, edges);
}

inline std::vector<std::vector<uint32_t>> connectedComponents(
    const ViewGraph& g, const std::vector<uint32_t>& nodes) {
    return spirula::graph::connected_components(g, nodes);
}

struct PartitionOptions {
    size_t leaf_max_images = 160;  // split until every part is at most this big
    size_t overlap = 30;           // images each part borrows from its sibling
    size_t min_part = 20;          // a part smaller than this is not worth a model
};

inline std::vector<std::vector<uint32_t>> bisect(const ViewGraph& g,
                                                 const std::vector<uint32_t>& nodes,
                                                 const PartitionOptions& opt) {
    return spirula::graph::bisect(g, nodes, {opt.overlap, opt.min_part, nullptr});
}

inline std::vector<std::vector<uint32_t>> partitionViewGraph(const ViewGraph& g,
                                                             const PartitionOptions& opt) {
    return spirula::graph::recursive_bisect(g, {opt.leaf_max_images, opt.overlap, opt.min_part});
}

}  // namespace sfm
