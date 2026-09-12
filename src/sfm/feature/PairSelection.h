// GPU pair selection: decide which pairs are worth full matching by matching a
// small top-scale descriptor subset of one image against the other image's
// descriptors, then keeping only each image's best-scoring partners. This is
// the "pair selection" step commercial SfM pipelines run instead of exhaustive
// matching; it fills the roadmap's retrieval slot without a vocabulary tree
// (docs/notes/sfm-design.md D35).
//
// Why this form: exhaustive full matching is quadratic in *pair count* -- at
// the measured 3.5 ms/pair, 1000 images is 500k pairs, ~30 min -- and on
// sparse collections most of those pairs die in verification anyway.
// The score used here is a subsample of the true objective:
// the number of ratio-test survivors when the K largest-scale features of one
// image are matched against the other image's features, computed by the same
// GPU brute-force matcher. Scoring stays O(N^2) but with a constant small
// enough that full matching, now O(N*k), dominates again. A vocabulary tree
// only becomes interesting when even the reduced O(N^2) sweep hurts (~10k
// images); its score is also a strictly weaker pair-relevance signal than
// actually matching descriptors.
//
// The scoring is deliberately asymmetric (mini query vs full train). The
// symmetric mini-vs-mini variant is ~30x cheaper again but was measured and
// rejected *as the score*: requiring both endpoints of a correspondence to fall
// in their image's top-K squares the selection loss, and a 256-descriptor train
// side makes the ratio test toothless (the second-best distance is far), so
// real pairs score low while junk scores nonzero.
//
// It is a perfectly good *shortlist*, though, and that is what it is used for
// here (D56). The asymmetric score is what decides, but it only has to decide
// among a few times as many candidates as it will keep -- so the cheap
// symmetric pass runs over all N^2 pairs and the expensive reliable one over a
// list linear in N. At 1000 images that is 54 s of the match stage's 107 s
// turned into single digits, and the saving grows with N because the quadratic
// term is the one that shrank.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/feature/Matcher.h"
#include "sfm/feature/Pairing.h"

namespace sfm {

struct PairSelectionOptions {
    uint32_t num_features = 512;  // K: per-image top-scale query subset
    // Train-side cap. Scoring cost is num_features * train_features; the
    // default full-resolution train side is what makes the score reliable.
    // 0 = no cap (use every descriptor).
    uint32_t train_features = 0;
    uint32_t num_neighbors = 32;  // k: keep each image's k best-scoring partners
    uint32_t min_score = 4;       // mini-matches below this never qualify a pair
    // Coarse shortlist pass: both sides capped to this many descriptors, over
    // every pair, keeping each image's `coarse_neighbors` best. Only the
    // survivors get the reliable asymmetric score above. 0 scores every pair
    // the expensive way, which is what a small capture wants -- the shortlist
    // is only worth its own pass once N^2 is the problem.
    uint32_t coarse_features = 256;
    uint32_t coarse_neighbors = 128;
    uint32_t coarse_min_images = 200;
    // Lowe ratio for the scoring pass. Independent of the full matcher's
    // ratio: scoring only ranks pairs, so it can afford to be looser.
    float ratio = 0.8f;
    // Scoring problems are ~1/32 the size of full matching, so batch more
    // pairs per submit than the full matcher does.
    int batch_pairs = 256;
    int device = -1;
};

// Gather f's K best-ranked features (K = 0 or >= count keeps everything, but
// still gathered in rank order). The canonical feature order is by position,
// deliberately not by rank (D16), so this is an explicit host-side gather;
// ties break by index.
//
// "Rank" is scale for SIFT and the detection score for a detector that has no
// scale -- FeatureSet::rank picks. Reading `scale` unconditionally, as this
// did, silently selected an arbitrary 512 keypoints out of an ALIKED set,
// because every one of them has scale 0. That is the failure mode this whole
// stage is least able to show: the shortlist still looks populated.
inline FeatureSet topScaleSubset(const FeatureSet& f, uint32_t K) {
    if (K == 0 || K > f.count()) K = f.count();
    std::vector<uint32_t> idx(f.count());
    for (uint32_t j = 0; j < f.count(); j++) idx[j] = j;
    std::partial_sort(idx.begin(), idx.begin() + K, idx.end(), [&](uint32_t a, uint32_t b) {
        if (f.rank(a) != f.rank(b)) return f.rank(a) > f.rank(b);
        return a < b;
    });
    FeatureSet m;
    m.width = f.width;
    m.height = f.height;
    m.dim = f.dim;
    m.dtype = f.dtype;
    const uint32_t dsz = f.dim * dtypeSize(f.dtype);
    m.keypoints.reserve(K);
    m.descriptors.resize((size_t)K * dsz);
    for (uint32_t k = 0; k < K; k++) {
        m.keypoints.push_back(f.keypoints[idx[k]]);
        std::memcpy(&m.descriptors[(size_t)k * dsz], &f.descriptors[(size_t)idx[k] * dsz], dsz);
    }
    return m;
}

namespace detail {

// Count the ratio-test survivors of every ordered (query, train) pair on the
// GPU. `sets` is the matcher's one index space: [0, n) query subsets,
// [n, 2n) train sides. Only the counts are wanted, so the matcher counts on the
// spot rather than building (and immediately discarding) half a million match
// lists.
inline std::vector<uint32_t> scoreOrderedPairs(
    const std::vector<const FeatureSet*>& sets,
    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, const PairSelectionOptions& opt,
    size_t progress_base, size_t progress_total,
    const std::function<void(size_t, size_t)>& progress) {
    MatchOptions mo;
    mo.device = opt.device;
    mo.batch_pairs = opt.batch_pairs;
    mo.max_num_matches = 0;
    mo.max_ratio = opt.ratio;
    // No cross-check: the ratio test against a full-size train side is already
    // selective, and skipping it means the matcher never dispatches the column
    // reduction or downloads the train-side results -- at K=256 queries that
    // cuts the readback per pair from (K + n_train) to K entries, ~33x here.
    mo.cross_check = false;
    BruteForceMatcher matcher(mo);

    std::vector<uint32_t> score(pairs.size(), 0);
    // How often progress is reported, and how much of the list the matcher sees
    // at once -- it chunks the range itself. A hundredth of the list, so a
    // selection that is minutes long reports through it and not once at the end.
    const size_t batch = (size_t)std::max(1, opt.batch_pairs);
    const size_t chunk = std::max(batch, std::min(batch * 64, pairs.size() / 100 + 1));
    std::vector<uint32_t> out;
    for (size_t b = 0; b < pairs.size(); b += chunk) {
        const size_t e = std::min(b + chunk, pairs.size());
        matcher.countBatch(sets, pairs, b, e, out);
        for (size_t p = b; p < e; p++) score[p] = out[p - b];
        if (progress) progress(progress_base + e, progress_total);
    }
    return score;
}

// Union of each image's `k` best partners, from unordered (i<j, score) edges.
inline std::vector<std::pair<uint32_t, uint32_t>> topPartners(
    uint32_t n, const std::vector<std::pair<uint32_t, uint32_t>>& cand,
    const std::vector<uint32_t>& edge_score, uint32_t k, uint32_t min_score) {
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> adj(n);  // (score, partner)
    for (size_t e = 0; e < cand.size(); e++) {
        const uint32_t s = edge_score[e];
        if (s < min_score) continue;
        adj[cand[e].first].push_back({s, cand[e].second});
        adj[cand[e].second].push_back({s, cand[e].first});
    }
    std::vector<std::pair<uint32_t, uint32_t>> sel;
    for (uint32_t i = 0; i < n; i++) {
        auto& a = adj[i];
        const size_t take = std::min<size_t>(k, a.size());
        std::partial_sort(a.begin(), a.begin() + take, a.end(),
                          [](const std::pair<uint32_t, uint32_t>& x,
                             const std::pair<uint32_t, uint32_t>& y) {
                              if (x.first != y.first) return x.first > y.first;
                              return x.second < y.second;
                          });
        for (size_t t = 0; t < take; t++) {
            const uint32_t j = a[t].second;
            sel.emplace_back(std::min(i, j), std::max(i, j));
        }
    }
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    return sel;
}

}  // namespace detail

// Score candidate pairs and return the union of each image's top-k partners,
// in lexicographic pair order. Deterministic: the matcher is exact, subset
// selection and every tie-break are index-ordered. `progress(done, total)` is
// optional.
inline std::vector<std::pair<uint32_t, uint32_t>> prefilterPairs(
    const std::vector<FeatureSet>& feats, const PairSelectionOptions& opt,
    const std::function<void(size_t, size_t)>& progress = nullptr) {
    const uint32_t n = (uint32_t)feats.size();
    if (n < 2) return {};
    const size_t all_ordered = (size_t)n * (n - 1);
    const bool coarse = opt.coarse_features > 0 && n >= opt.coarse_min_images;

    // ---- candidates ------------------------------------------------------
    // Either every pair, or the shortlist a symmetric mini-vs-mini pass keeps.
    std::vector<std::pair<uint32_t, uint32_t>> cand;
    size_t done_pairs = 0;
    if (coarse) {
        std::vector<FeatureSet> mini(n);
        std::vector<const FeatureSet*> sets(2 * (size_t)n);
        for (uint32_t i = 0; i < n; i++) {
            mini[i] = topScaleSubset(feats[i], opt.coarse_features);
            sets[i] = sets[(size_t)n + i] = &mini[i];
        }
        // Both sides are a few tens of kilobytes, so the whole coarse train
        // side is resident and the order is free; one direction per pair is
        // enough to shortlist.
        std::vector<std::pair<uint32_t, uint32_t>> ordered;
        ordered.reserve(all_ordered / 2);
        for (uint32_t i = 0; i < n; i++)
            for (uint32_t j = i + 1; j < n; j++) ordered.emplace_back(i, (uint32_t)(n + j));
        std::vector<uint32_t> s =
            detail::scoreOrderedPairs(sets, ordered, opt, 0, all_ordered, progress);
        for (auto& p : ordered) p.second -= n;
        // min_score belongs to the deciding pass; a shortlist that applies it
        // would throw away pairs the reliable score has not seen yet.
        cand = detail::topPartners(n, ordered, s, opt.coarse_neighbors, 1);
        done_pairs = all_ordered / 2;
    } else {
        cand.reserve(all_ordered / 2);
        for (uint32_t i = 0; i < n; i++)
            for (uint32_t j = i + 1; j < n; j++) cand.emplace_back(i, j);
    }

    // ---- the deciding score ----------------------------------------------
    // One index space drives the matcher: [0, n) are the query subsets,
    // [n, 2n) the train sides, and every scoring pair is (query i, train j).
    // The train side is the memory cost (the queries are ~32 KB each), so it
    // gets the optional cap -- and when it is uncapped the view points straight
    // at the caller's feature sets instead of copying a gigabyte of
    // descriptors.
    std::vector<FeatureSet> owned(n + (opt.train_features == 0 ? 0 : (size_t)n));
    std::vector<const FeatureSet*> sets(2 * (size_t)n);
    for (uint32_t i = 0; i < n; i++) {
        owned[i] = topScaleSubset(feats[i], opt.num_features);
        sets[i] = &owned[i];
        if (opt.train_features == 0) {
            sets[(size_t)n + i] = &feats[i];
        } else {
            owned[(size_t)n + i] = topScaleSubset(feats[i], opt.train_features);
            sets[(size_t)n + i] = &owned[(size_t)n + i];
        }
    }

    // Both directions of every candidate -- (i queries, j trains) and the
    // reverse -- and a pair's score is the max of the two. One direction alone
    // misses pairs whose shared content is top-scale-prominent in only one of
    // the two images. Train-major order: all queries against one train side
    // before moving to the next, so the matcher's resident-descriptor allocator
    // streams each full-size train set through VRAM once instead of thrashing.
    std::vector<std::pair<uint32_t, uint32_t>> ordered;
    std::vector<size_t> edge_of;  // parallel: which candidate each direction is
    ordered.reserve(2 * cand.size());
    edge_of.reserve(2 * cand.size());
    {
        std::vector<std::vector<std::pair<uint32_t, size_t>>> by_train(n);  // (query, edge)
        for (size_t e = 0; e < cand.size(); e++) {
            by_train[cand[e].second].emplace_back(cand[e].first, e);
            by_train[cand[e].first].emplace_back(cand[e].second, e);
        }
        for (uint32_t t = 0; t < n; t++)
            for (const auto& q : by_train[t]) {
                ordered.emplace_back(q.first, (uint32_t)(n + t));
                edge_of.push_back(q.second);
            }
    }
    const size_t total = coarse ? done_pairs + ordered.size() : ordered.size();
    std::vector<uint32_t> s =
        detail::scoreOrderedPairs(sets, ordered, opt, done_pairs, total, progress);

    std::vector<uint32_t> edge_score(cand.size(), 0);
    for (size_t k = 0; k < s.size(); k++)
        edge_score[edge_of[k]] = std::max(edge_score[edge_of[k]], s[k]);
    return detail::topPartners(n, cand, edge_score, opt.num_neighbors, opt.min_score);
}

}  // namespace sfm
