#pragma once

// Spectral partitioning of a weighted undirected graph: connected components,
// the normalized cut of a node set by its Fiedler vector, and the recursive
// bisection built on it. Standard library only, so the SfM view graph, the
// covisibility graph of a parsed dataset and any future graph over a
// reconstruction cut through the same code.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace spirula {
namespace graph {

// Undirected weighted graph in CSR form.
struct WeightedGraph {
    std::vector<uint32_t> offs;   // n+1
    std::vector<uint32_t> adj;    // neighbours
    std::vector<double> w;        // parallel to adj
    size_t n() const { return offs.empty() ? 0 : offs.size() - 1; }
    double degree(uint32_t i) const {
        double d = 0;
        for (uint32_t k = offs[i]; k < offs[i + 1]; k++) d += w[k];
        return d;
    }
};

inline bool cancelled(const std::atomic<bool>* flag) {
    return flag && flag->load(std::memory_order_relaxed);
}

struct Edge {
    uint32_t a = 0, b = 0;
    double w = 0;
};

// Symmetric CSR from an edge list. Edges naming the same pair twice add up;
// self-loops and edges past `n` are dropped.
inline WeightedGraph build_graph(size_t n, const std::vector<Edge>& edges) {
    WeightedGraph g;
    g.offs.assign(n + 1, 0);
    for (const Edge& e : edges) {
        if (e.a == e.b || e.a >= n || e.b >= n || !(e.w > 0)) continue;
        g.offs[e.a + 1]++;
        g.offs[e.b + 1]++;
    }
    for (size_t i = 1; i <= n; i++) g.offs[i] += g.offs[i - 1];
    g.adj.resize(g.offs[n]);
    g.w.resize(g.offs[n]);
    std::vector<uint32_t> fill(g.offs.begin(), g.offs.end() - 1);
    for (const Edge& e : edges) {
        if (e.a == e.b || e.a >= n || e.b >= n || !(e.w > 0)) continue;
        g.adj[fill[e.a]] = e.b;
        g.w[fill[e.a]++] = e.w;
        g.adj[fill[e.b]] = e.a;
        g.w[fill[e.b]++] = e.w;
    }
    return g;
}

// Accumulates pair weights so a graph can be built from many small
// observations (every pair of images sharing a point) without a quadratic
// edge list.
class EdgeAccumulator {
public:
    void add(uint32_t a, uint32_t b, double w) {
        if (a == b) return;
        if (a > b) std::swap(a, b);
        _acc[((uint64_t)a << 32) | b] += w;
    }
    void reserve(size_t n) { _acc.reserve(n); }
    size_t size() const { return _acc.size(); }
    WeightedGraph build(size_t n) const {
        std::vector<Edge> edges;
        edges.reserve(_acc.size());
        for (const auto& kv : _acc)
            edges.push_back({(uint32_t)(kv.first >> 32), (uint32_t)(kv.first & 0xffffffffu),
                             kv.second});
        // Hash order is not stable across runs; the cut below must be.
        std::sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y) {
            return x.a != y.a ? x.a < y.a : x.b < y.b;
        });
        return build_graph(n, edges);
    }

private:
    std::unordered_map<uint64_t, double> _acc;
};

// Connected components of the subgraph induced on `nodes`, largest first.
inline std::vector<std::vector<uint32_t>> connected_components(
    const WeightedGraph& g, const std::vector<uint32_t>& nodes) {
    std::vector<char> inside(g.n(), 0), seen(g.n(), 0);
    for (uint32_t v : nodes) inside[v] = 1;
    std::vector<std::vector<uint32_t>> out;
    std::vector<uint32_t> stack;
    for (uint32_t s : nodes) {
        if (seen[s]) continue;
        std::vector<uint32_t> comp;
        stack.assign(1, s);
        seen[s] = 1;
        while (!stack.empty()) {
            const uint32_t v = stack.back();
            stack.pop_back();
            comp.push_back(v);
            for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++) {
                const uint32_t u = g.adj[k];
                if (inside[u] && !seen[u]) { seen[u] = 1; stack.push_back(u); }
            }
        }
        out.push_back(std::move(comp));
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
                         return a.size() > b.size();
                     });
    return out;
}

namespace detail {

// Fiedler vector of the induced subgraph's normalized Laplacian, by power
// iteration on M = D^-1/2 W D^-1/2 deflated against its top eigenvector
// D^1/2 * 1. Per-node values in `nodes` order; empty when it did not converge.
inline std::vector<double> fiedler(const WeightedGraph& g, const std::vector<uint32_t>& nodes,
                                   const std::vector<uint32_t>& local_of, int iters = 300,
                                   const std::atomic<bool>* cancel = nullptr) {
    const size_t m = nodes.size();
    std::vector<double> deg(m, 0.0);
    for (size_t i = 0; i < m; i++) {
        const uint32_t v = nodes[i];
        for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++)
            if (local_of[g.adj[k]] != UINT32_MAX) deg[i] += g.w[k];
    }
    std::vector<double> isq(m);
    for (size_t i = 0; i < m; i++) isq[i] = deg[i] > 0 ? 1.0 / std::sqrt(deg[i]) : 0.0;
    std::vector<double> v0(m);
    double n0 = 0;
    for (size_t i = 0; i < m; i++) { v0[i] = std::sqrt(deg[i]); n0 += v0[i] * v0[i]; }
    if (n0 <= 0) return {};
    n0 = std::sqrt(n0);
    for (double& x : v0) x /= n0;

    // Alternating signs cannot be orthogonal to the Fiedler vector by accident
    // the way a constant start is.
    std::vector<double> x(m), y(m);
    for (size_t i = 0; i < m; i++) x[i] = (i % 2 ? -1.0 : 1.0) + 1e-3 * (double)(i % 7);
    auto orthonormalize = [&](std::vector<double>& z) {
        double dot = 0;
        for (size_t i = 0; i < m; i++) dot += z[i] * v0[i];
        double nz = 0;
        for (size_t i = 0; i < m; i++) { z[i] -= dot * v0[i]; nz += z[i] * z[i]; }
        return std::sqrt(nz);
    };
    if (orthonormalize(x) <= 0) return {};
    {
        double nz = 0;
        for (double q : x) nz += q * q;
        nz = std::sqrt(nz);
        for (double& q : x) q /= nz;
    }
    // (M + I)/2 makes the wanted eigenvector the dominant mode of a positive
    // operator, so the iteration cannot land on the most negative one.
    for (int it = 0; it < iters; it++) {
        if (cancelled(cancel)) return {};
        std::fill(y.begin(), y.end(), 0.0);
        for (size_t i = 0; i < m; i++) {
            const uint32_t v = nodes[i];
            double acc = 0;
            for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++) {
                const uint32_t j = local_of[g.adj[k]];
                if (j != UINT32_MAX) acc += g.w[k] * isq[j] * x[j];
            }
            y[i] = 0.5 * (isq[i] * acc + x[i]);
        }
        double nz = orthonormalize(y);
        if (nz <= 1e-300) return {};
        for (size_t i = 0; i < m; i++) y[i] /= nz;
        double diff = 0;
        for (size_t i = 0; i < m; i++) diff += std::fabs(y[i] - x[i]);
        x.swap(y);
        if (it > 10 && diff < 1e-7 * (double)m) break;
    }
    for (size_t i = 0; i < m; i++) x[i] *= isq[i];
    return x;
}

}  // namespace detail

struct BisectOptions {
    size_t overlap = 0;    // nodes each side borrows from its sibling
    size_t min_part = 1;   // a side smaller than this is not a cut
    // Optional per-node cost (parallel to the graph's nodes); the sweep then
    // scores balance by summed cost rather than by node count. Null = 1 each.
    const double* cost = nullptr;
    // Each side must hold at least this fraction of the cost. 0 leaves the
    // normalized cut free to shave off a weakly attached clump, which is right
    // for SfM atoms and wrong for parts that should be of a size.
    double balance = 0.0;
    // Set: the split gives up and returns {}.
    const std::atomic<bool>* cancel = nullptr;
};

// Split `nodes` in two along the normalized cut, then give each side the
// `overlap` nodes of the other side best connected to it. {} when the split
// is not worth making.
inline std::vector<std::vector<uint32_t>> bisect(const WeightedGraph& g,
                                                 const std::vector<uint32_t>& nodes,
                                                 const BisectOptions& opt) {
    if (nodes.size() < 2 * std::max<size_t>(1, opt.min_part)) return {};
    std::vector<uint32_t> local_of(g.n(), UINT32_MAX);
    for (size_t i = 0; i < nodes.size(); i++) local_of[nodes[i]] = (uint32_t)i;

    std::vector<double> f = detail::fiedler(g, nodes, local_of, 300, opt.cancel);
    if (cancelled(opt.cancel)) return {};
    std::vector<uint32_t> order(nodes.size());
    std::iota(order.begin(), order.end(), 0u);
    // No Fiedler vector (disconnected or degenerate): the input order, which
    // for a video capture is the capture order and is the cut one would draw
    // by hand.
    if (!f.empty())
        std::stable_sort(order.begin(), order.end(),
                         [&](uint32_t a, uint32_t b) { return f[a] < f[b]; });

    // cut(S) * (1/vol(S) + 1/vol(V\S)) swept along the order; the running
    // sums keep it linear in the edge count. Balance uses the caller's cost
    // when given, so a side is judged by what it will cost to process.
    const size_t m = nodes.size();
    std::vector<uint32_t> rank(m);
    for (size_t i = 0; i < m; i++) rank[order[i]] = (uint32_t)i;
    double total_vol = 0;
    std::vector<double> deg(m, 0.0);
    for (size_t i = 0; i < m; i++) {
        const uint32_t v = nodes[i];
        for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++)
            if (local_of[g.adj[k]] != UINT32_MAX) deg[i] += g.w[k];
        total_vol += opt.cost ? std::max(opt.cost[v], 1e-9) : deg[i];
    }
    double vol = 0, cut = 0, best = 1e300;
    size_t best_split = 0;
    const size_t min_part = std::max<size_t>(1, opt.min_part);
    for (size_t i = 0; i + 1 < m; i++) {
        const uint32_t li = order[i];
        const uint32_t v = nodes[li];
        vol += opt.cost ? std::max(opt.cost[v], 1e-9) : deg[li];
        for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++) {
            const uint32_t j = local_of[g.adj[k]];
            if (j == UINT32_MAX) continue;
            cut += rank[j] <= i ? -g.w[k] : g.w[k];
        }
        if (i + 1 < min_part || m - i - 1 < min_part) continue;
        const double other = total_vol - vol;
        if (vol <= 0 || other <= 0) continue;
        if (opt.balance > 0 && (vol < opt.balance * total_vol || other < opt.balance * total_vol))
            continue;
        const double score = cut * (1.0 / vol + 1.0 / other);
        if (score < best) { best = score; best_split = i + 1; }
    }
    if (best_split == 0) return {};

    std::vector<std::vector<uint32_t>> parts(2);
    std::vector<char> side(m, 1);
    for (size_t i = 0; i < m; i++) {
        const bool left = rank[i] < best_split;
        side[i] = left ? 0 : 1;
        parts[left ? 0 : 1].push_back(nodes[i]);
    }
    if (opt.overlap == 0) return parts;

    // The borrow is bounded so a part is always strictly smaller than what it
    // was cut from; without that a lopsided cut plus the borrow grows a part
    // past its parent and a size-based recursion never ends.
    for (int s = 0; s < 2; s++) {
        const size_t room = m - 1 - std::min(m - 1, parts[s].size());
        std::vector<std::pair<double, uint32_t>> cross;
        for (size_t i = 0; i < m; i++) {
            if (side[i] == s) continue;
            double link = 0;
            const uint32_t v = nodes[i];
            for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++) {
                const uint32_t j = local_of[g.adj[k]];
                if (j != UINT32_MAX && side[j] == s) link += g.w[k];
            }
            if (link > 0) cross.emplace_back(link, v);
        }
        std::sort(cross.begin(), cross.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second < b.second;
        });
        for (size_t k = 0; k < cross.size() && k < std::min(opt.overlap, room); k++)
            parts[s].push_back(cross[k].second);
        std::sort(parts[s].begin(), parts[s].end());
    }
    return parts;
}

struct RecursiveBisectOptions {
    size_t leaf_max = 160;   // split until every part is at most this big
    size_t overlap = 30;     // images each part borrows from its sibling
    size_t min_part = 20;    // a part smaller than this is not worth a model
};

// Recursive bisection down to leaves of at most `leaf_max` (before overlap).
// Disconnected inputs are separated first: a cut inside a component is
// meaningful, a cut between two is free and says nothing.
inline std::vector<std::vector<uint32_t>> recursive_bisect(const WeightedGraph& g,
                                                           const RecursiveBisectOptions& opt) {
    std::vector<uint32_t> all(g.n());
    std::iota(all.begin(), all.end(), 0u);
    std::vector<std::vector<uint32_t>> queue = connected_components(g, all), leaves;
    const BisectOptions bo{opt.overlap, opt.min_part, nullptr};
    while (!queue.empty()) {
        std::vector<uint32_t> part = std::move(queue.back());
        queue.pop_back();
        // Over the leaf size only WITHOUT the borrowed overlap, or the borrow
        // itself would force another split, and every split borrows again.
        if (part.size() <= opt.leaf_max + opt.overlap) {
            if (part.size() >= 2) leaves.push_back(std::move(part));
            continue;
        }
        std::vector<std::vector<uint32_t>> halves = bisect(g, part, bo);
        // bisect bounds the borrow so a half never matches its parent; the
        // check stays because the cost of being wrong is a hang.
        if (halves.size() != 2 || halves[0].size() >= part.size() ||
            halves[1].size() >= part.size()) {
            leaves.push_back(std::move(part));
            continue;
        }
        queue.push_back(std::move(halves[0]));
        queue.push_back(std::move(halves[1]));
    }
    std::stable_sort(leaves.begin(), leaves.end(),
                     [](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
                         return a.size() > b.size();
                     });
    return leaves;
}

struct LabelCutOptions {
    // Exactly this many parts when > 0 (the heaviest part is bisected until
    // the count is reached); otherwise parts of at most `leaf_max` cost.
    size_t parts = 0;
    double leaf_max = 400;
    size_t min_part = 2;
    const double* cost = nullptr;   // per node; null = 1 each
    double balance = 0.3;           // see BisectOptions::balance
    // A finished part below this many nodes is not worth a model and joins
    // the part it shares the most with.
    size_t min_final = 2;
    int refine_passes = 3;
    // Set: the cut stops within one power iteration and returns {}.
    const std::atomic<bool>* cancel = nullptr;
};

namespace detail {

// Weight from `v` into each label among its neighbours; `own` gets the weight
// staying inside its own label.
inline void label_links(const WeightedGraph& g, const std::vector<int32_t>& label, uint32_t v,
                        std::vector<double>& into) {
    for (uint32_t k = g.offs[v]; k < g.offs[v + 1]; k++) {
        const int32_t l = label[g.adj[k]];
        if (l >= 0) into[(size_t)l] += g.w[k];
    }
}

// A part's pieces beyond its largest connected one move to the label they
// are best connected to, when at most `max_share` of the part: stranded nodes
// are repaired, a part that is genuinely two halves is left to be split again.
inline void reconnect_parts(const WeightedGraph& g, std::vector<int32_t>& label, int n_labels,
                            double max_share = 1.0, const std::atomic<bool>* cancel = nullptr) {
    for (int l = 0; l < n_labels && !cancelled(cancel); l++) {
        std::vector<uint32_t> nodes;
        for (uint32_t v = 0; v < g.n(); v++)
            if (label[v] == l) nodes.push_back(v);
        std::vector<std::vector<uint32_t>> comps = connected_components(g, nodes);
        for (size_t c = 1; c < comps.size(); c++) {
            if ((double)comps[c].size() > max_share * (double)nodes.size()) continue;
            std::vector<double> into((size_t)n_labels, 0.0);
            for (uint32_t v : comps[c]) label_links(g, label, v, into);
            into[(size_t)l] = 0;
            int best = -1;
            for (int j = 0; j < n_labels; j++)
                if (into[(size_t)j] > 0 && (best < 0 || into[(size_t)j] > into[(size_t)best])) best = j;
            if (best < 0) continue;
            for (uint32_t v : comps[c]) label[v] = best;
        }
    }
}

}  // namespace detail

// A disjoint partition of every node into dense labels ordered by decreasing
// part cost. A component too small to cut keeps its own label: an isolated
// node is a part of its own, not a stray in someone else's.
inline std::vector<int32_t> cut_labels(const WeightedGraph& g, const LabelCutOptions& opt) {
    auto cost_of = [&](const std::vector<uint32_t>& part) {
        double c = 0;
        for (uint32_t v : part) c += opt.cost ? std::max(opt.cost[v], 0.0) : 1.0;
        return c;
    };
    std::vector<uint32_t> all(g.n());
    std::iota(all.begin(), all.end(), 0u);
    std::vector<std::vector<uint32_t>> parts = connected_components(g, all);
    std::vector<char> final_(parts.size(), 0);
    const BisectOptions bo{0, opt.min_part, opt.cost, opt.balance, opt.cancel};
    for (;;) {
        if (cancelled(opt.cancel)) return {};
        // The part to cut next: the costliest one still cuttable.
        int pick = -1;
        double pick_cost = -1;
        for (size_t i = 0; i < parts.size(); i++) {
            if (final_[i]) continue;
            const double c = cost_of(parts[i]);
            const bool wants = opt.parts > 0 ? parts.size() < opt.parts : c > opt.leaf_max;
            if (!wants) continue;
            if (c > pick_cost) { pick_cost = c; pick = (int)i; }
        }
        if (pick < 0) break;
        std::vector<std::vector<uint32_t>> halves = bisect(g, parts[(size_t)pick], bo);
        if (cancelled(opt.cancel)) return {};
        if (halves.size() != 2 || halves[0].empty() || halves[1].empty()) {
            final_[(size_t)pick] = 1;
            continue;
        }
        // A half the sweep left in pieces: every piece but the largest goes
        // to the other half, where its links are. Both halves stay inside the
        // parent, so the recursion still shrinks.
        for (int side = 0; side < 2; side++) {
            std::vector<std::vector<uint32_t>> comps = connected_components(g, halves[side]);
            if (comps.size() < 2) continue;
            halves[side] = std::move(comps[0]);
            for (size_t c = 1; c < comps.size(); c++)
                halves[1 - side].insert(halves[1 - side].end(), comps[c].begin(), comps[c].end());
            std::sort(halves[1 - side].begin(), halves[1 - side].end());
        }
        parts[(size_t)pick] = std::move(halves[0]);
        parts.push_back(std::move(halves[1]));
        final_.push_back(0);
    }
    std::vector<int32_t> label(g.n(), -1);
    int n_labels = (int)parts.size();
    for (int k = 0; k < n_labels; k++)
        for (uint32_t v : parts[(size_t)k]) label[v] = k;
    auto node_cost = [&](uint32_t v) { return opt.cost ? std::max(opt.cost[v], 0.0) : 1.0; };
    auto sizes = [&]() {
        std::vector<double> sz((size_t)n_labels, 0.0);
        for (uint32_t v = 0; v < g.n(); v++)
            if (label[v] >= 0) sz[(size_t)label[v]] += node_cost(v);
        return sz;
    };

    // A boundary node that shares more with another part moves over while
    // the sizes stay within a quarter of the mean; stranded pieces follow.
    for (int pass = 0; pass < opt.refine_passes; pass++) {
        if (cancelled(opt.cancel)) return {};
        std::vector<double> sz = sizes();
        double mean = 0;
        for (double x : sz) mean += x;
        mean /= std::max<size_t>(1, sz.size());
        size_t moved = 0;
        std::vector<double> into((size_t)n_labels);
        for (uint32_t v = 0; v < g.n(); v++) {
            const int32_t l = label[v];
            if (l < 0) continue;
            std::fill(into.begin(), into.end(), 0.0);
            detail::label_links(g, label, v, into);
            int best = l;
            for (int j = 0; j < n_labels; j++)
                if (into[(size_t)j] > into[(size_t)best]) best = j;
            if (best == l || into[(size_t)best] <= 1.05 * into[(size_t)l]) continue;
            if (sz[(size_t)best] + node_cost(v) > 1.25 * mean) continue;
            if (sz[(size_t)l] - node_cost(v) < 0.5 * mean) continue;
            sz[(size_t)l] -= node_cost(v);
            sz[(size_t)best] += node_cost(v);
            label[v] = best;
            moved++;
        }
        if (moved == 0) break;
        detail::reconnect_parts(g, label, n_labels, 0.25, opt.cancel);
    }

    // Parts too small to be worth a model join their best-connected neighbour;
    // an isolated one stays as it is and is the caller's to place.
    for (;;) {
        if (cancelled(opt.cancel)) return {};
        std::vector<double> sz = sizes();
        int tiny = -1;
        for (int k = 0; k < n_labels; k++)
            if (sz[(size_t)k] > 0 && sz[(size_t)k] < (double)opt.min_final &&
                (tiny < 0 || sz[(size_t)k] < sz[(size_t)tiny]))
                tiny = k;
        if (tiny < 0) break;
        std::vector<double> into((size_t)n_labels, 0.0);
        for (uint32_t v = 0; v < g.n(); v++)
            if (label[v] == tiny) detail::label_links(g, label, v, into);
        into[(size_t)tiny] = 0;
        int best = -1;
        for (int j = 0; j < n_labels; j++)
            if (into[(size_t)j] > 0 && (best < 0 || into[(size_t)j] > into[(size_t)best])) best = j;
        if (best < 0) break;
        for (uint32_t v = 0; v < g.n(); v++)
            if (label[v] == tiny) label[v] = best;
    }

    // Dense labels, costliest part first.
    std::vector<double> sz = sizes();
    std::vector<int> order((size_t)n_labels);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return sz[(size_t)a] > sz[(size_t)b]; });
    std::vector<int32_t> remap((size_t)n_labels, -1);
    int next = 0;
    for (int k : order)
        if (sz[(size_t)k] > 0) remap[(size_t)k] = next++;
    for (int32_t& l : label)
        if (l >= 0) l = remap[(size_t)l];
    return label;
}

}  // namespace graph
}  // namespace spirula
