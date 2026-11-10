#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

#include "sfm/feature/PairExpansion.h"
#include "sfm/feature/SpatialSelection.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int frontendPolicyTest(int, char**) {
    int failures = 0;
    auto check = [&](bool condition, const char* label) {
        std::printf("%s: %s\n", condition ? "PASS" : "FAIL", label);
        if (!condition) ++failures;
    };
    MatchesDatabase db;
    db.images.resize(6);
    for (auto p : {std::make_pair(0u, 1u), {1u, 2u}, {3u, 4u}}) {
        TwoViewMatches pair;
        pair.image1 = p.first;
        pair.image2 = p.second;
        pair.matches = MatchList(std::vector<FeatureMatch>(30));
        db.pairs.push_back(std::move(pair));
    }
    const std::vector<std::pair<uint32_t, uint32_t>> attempted = {{0, 1}, {1, 2}, {3, 4}, {2, 5}};
    const std::vector<std::pair<uint32_t, uint32_t>> retrieved = {{2, 3}, {2, 5}, {4, 5}};
    const auto expanded = expandVerifiedPairs(db, attempted, retrieved, 2);
    check(std::find(expanded.begin(), expanded.end(), std::make_pair(0u, 2u)) != expanded.end(),
          "two-hop evidence proposes an untested pair");
    check(std::find(expanded.begin(), expanded.end(), std::make_pair(2u, 3u)) != expanded.end(),
          "broader retrieval proposes a component bridge");
    bool untested = true;
    std::vector<int> counts(6, 0);
    for (auto p : expanded) {
        untested &= std::find(attempted.begin(), attempted.end(), p) == attempted.end();
        ++counts[p.first]; ++counts[p.second];
    }
    check(untested && *std::max_element(counts.begin(), counts.end()) <= 2,
          "expansion respects attempted pairs and both endpoint budgets");
    check(expandVerifiedPairs(db, attempted, retrieved, 0).empty(), "zero expansion budget");
    std::reverse(db.pairs.begin(), db.pairs.end());
    check(expandVerifiedPairs(db, attempted, retrieved, 2) == expanded,
          "pair proposal order is deterministic");

    std::vector<Keypoint> keypoints = {
        {10, 10, 10, 0, 0}, {11, 11, 9, 0, 0}, {12, 12, 8, 0, 0},
        {60, 10, 7, 0, 0}, {10, 60, 6, 0, 0}, {60, 60, 5, 0, 0}};
    const std::vector<uint32_t> ranked = {0, 1, 2, 3, 4, 5};
    auto selected = spatiallyBalancedFeatures(keypoints, ranked, 100, 100, 2, 4);
    check(selected == std::vector<uint32_t>({0, 3, 4, 5}), "balanced features cover occupied cells");
    selected = spatiallyBalancedFeatures(keypoints, ranked, 100, 100, 2, 10);
    check(selected.size() == 6 && std::set<uint32_t>(selected.begin(), selected.end()).size() == 6,
          "sparse cells redistribute the budget without duplicate features");
    MatchesDatabase isolated;
    isolated.images.resize(3);
    const auto ranked_bridges = expandVerifiedPairs(isolated, {}, {{0,1},{0,2}}, 1, {2,100});
    check(ranked_bridges == std::vector<std::pair<uint32_t,uint32_t>>{{0,2}},
          "retrieval evidence orders equally supported component bridges");
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, frontendPolicyTest); }
