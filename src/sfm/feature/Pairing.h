// Which image pairs to match. Kept separate from the matcher so the pairing
// strategy and the descriptor-matching algorithm compose independently: any
// IFeatureMatcher can be driven over any pair list.
//
// Exhaustive (all i<j) and sequential (a sliding window over a video) are
// generated here; Prefilter (GPU pair selection by mini-matching top-scale
// subsets) needs the features and lives in sfm/feature/PairSelection.h.
//
// Sequential and Prefilter each miss what the other finds, so by default each
// takes the other's list too: `--loop-closure` and `--prefilter-sequential`,
// see matchFeatureDir.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/Sequence.h"

namespace sfm {

enum class PairMode { Exhaustive, Sequential, Prefilter };

// Each image with the next `overlap` of its sequence and, with `quadratic`,
// the ones 2^k ahead for k < overlap (COLMAP's quadratic_overlap). A window
// never crosses from one `run` (folder) into another; empty = one sequence.
inline std::vector<std::pair<uint32_t, uint32_t>> sequentialPairs(
    uint32_t n, int overlap, bool quadratic, const std::vector<uint32_t>& run = {}) {
    const uint32_t ov = overlap > 0 ? (uint32_t)overlap : 10u;
    std::map<uint32_t, std::vector<uint32_t>> seqs;
    for (uint32_t i = 0; i < n; i++) seqs[run.size() == n ? run[i] : 0].push_back(i);
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    for (const auto& kv : seqs) {
        const std::vector<uint32_t>& s = kv.second;
        for (size_t a = 0; a < s.size(); a++) {
            for (uint32_t k = 1; k <= ov && a + k < s.size(); k++)
                pairs.emplace_back(s[a], s[a + k]);
            for (uint32_t k = 0; quadratic && k < ov && k < 31; k++) {
                const size_t b = a + ((size_t)1 << k);
                if (b >= s.size()) break;
                pairs.emplace_back(s[a], s[b]);
            }
        }
    }
    for (auto& p : pairs)
        if (p.first > p.second) std::swap(p.first, p.second);
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    return pairs;
}

// The sequence id of each image name: its parent folder.
inline std::vector<uint32_t> folderRuns(const std::vector<std::string>& names) {
    std::map<std::string, uint32_t> ids;
    std::vector<uint32_t> run(names.size());
    for (size_t i = 0; i < names.size(); i++) {
        const size_t slash = names[i].find_last_of('/');
        const std::string dir = slash == std::string::npos ? std::string() : names[i].substr(0, slash);
        run[i] = ids.emplace(dir, (uint32_t)ids.size()).first->second;
    }
    return run;
}

// Each image's folder and its position within it, in the name order the
// images are indexed in.
struct FileOrder {
    std::vector<uint32_t> run, pos;

    // 1 + weight * exp(-gap / decay) for two images of one folder, else 1.
    double boost(uint32_t a, uint32_t b, double weight, double decay) const {
        if (weight <= 0 || a >= run.size() || b >= run.size() || run[a] != run[b]) return 1.0;
        const double gap = pos[a] > pos[b] ? pos[a] - pos[b] : pos[b] - pos[a];
        return 1.0 + weight * std::exp(-gap / std::max(decay, 1e-6));
    }
};

inline FileOrder fileOrder(const std::vector<std::string>& names) {
    FileOrder o;
    o.run = folderRuns(names);
    o.pos.resize(names.size());
    std::map<uint32_t, uint32_t> next;
    for (size_t i = 0; i < names.size(); i++) o.pos[i] = next[o.run[i]]++;
    return o;
}

// The window along each sequence of a SequenceTable, one chain per member so
// a rig's lenses each pair with their own; the rig-mate pass links across.
inline std::vector<std::pair<uint32_t, uint32_t>> sequenceWindowPairs(const SequenceTable& st,
                                                                       int overlap,
                                                                       bool quadratic) {
    const uint32_t n = (uint32_t)st.seq.size();
    std::vector<uint32_t> order;
    for (uint32_t i = 0; i < n; i++)
        if (st.has(i)) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        if (st.seq[a] != st.seq[b]) return st.seq[a] < st.seq[b];
        if (st.member[a] != st.member[b]) return st.member[a] < st.member[b];
        return st.pos[a] != st.pos[b] ? st.pos[a] < st.pos[b] : a < b;
    });
    // sequentialPairs walks each run in index order, so the chains are laid
    // out over a renumbering that follows the sequence positions.
    std::vector<uint32_t> chain_of(n, UINT32_MAX);
    for (uint32_t k = 0; k < order.size(); k++) chain_of[order[k]] = k;
    std::vector<uint32_t> runs(order.size());
    std::map<std::pair<int32_t, int32_t>, uint32_t> ids;
    for (uint32_t k = 0; k < order.size(); k++)
        runs[k] = ids.emplace(std::make_pair(st.seq[order[k]], st.member[order[k]]),
                              (uint32_t)ids.size()).first->second;
    std::vector<std::pair<uint32_t, uint32_t>> pairs =
        sequentialPairs((uint32_t)order.size(), overlap, quadratic, runs);
    for (auto& p : pairs) {
        p.first = order[p.first];
        p.second = order[p.second];
        if (p.first > p.second) std::swap(p.first, p.second);
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

// Every pair for Exhaustive; the plain window (no quadratic links, one
// sequence) for Sequential.
inline std::vector<std::pair<uint32_t, uint32_t>> generatePairs(uint32_t n, PairMode mode,
                                                                int overlap = 10) {
    if (mode != PairMode::Exhaustive) return sequentialPairs(n, overlap, false);
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    pairs.reserve((size_t)n * (n - 1) / 2);
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = i + 1; j < n; j++) pairs.emplace_back(i, j);
    return pairs;
}

}  // namespace sfm
