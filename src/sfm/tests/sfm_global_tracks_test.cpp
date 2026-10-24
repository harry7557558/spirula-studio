#include <cstdio>
#include <stdexcept>
#include <vector>

#include "sfm/map/GlobalTracks.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int testGlobalTracks(int, char**) {
    MatchesDatabase db;
    db.images.resize(3);
    std::vector<FeatureSet> features(3);
    for (FeatureSet& f : features) {
        f.keypoints.resize(5);
        for (size_t i = 0; i < f.keypoints.size(); i++) f.keypoints[i].x = (float)i * 20;
    }
    db.pairs.push_back({0, 1, (int)TwoViewConfig::Uncalibrated,
                        {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}}});
    db.pairs.push_back({1, 2, (int)TwoViewConfig::Uncalibrated,
                        {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}}});
    db.pairs.push_back({0, 2, (int)TwoViewConfig::Uncalibrated, {{1, 0}}});
    std::vector<RelativeViewEdge> edges(3);
    for (RelativeViewEdge& e : edges) {
        e.status = RelativeViewStatus::Accepted;
        e.inliers = 100;
    }
    edges[2].inliers = 1;
    edges[0].match_mask = {0b11011};
    edges[1].match_mask = {0b10111};
    edges[2].match_mask = {1};

    const GlobalTracks tracks = buildGlobalTracks(db, features, edges);
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
        failures += !ok;
    };
    check(tracks.size() == 5 && tracks.conflicting == 1 && tracks.observations.size() == 13,
          "connected tracks and conflicts");
    bool has_three_view = false, has_split_zero = false, has_split_one = false;
    bool has_short_12 = false, has_short_01 = false;
    for (size_t t = 0; t < tracks.size(); t++) {
        const size_t begin = tracks.offsets[t], end = tracks.offsets[t + 1];
        if (end - begin == 3) {
            has_three_view = true;
            bool zero = true, one = true;
            for (size_t i = begin; i < end; i++)
                has_three_view = has_three_view && tracks.observations[i].image_id == i - begin &&
                                 tracks.observations[i].point2D_idx == 4;
            for (size_t i = begin; i < end; i++) {
                zero = zero && tracks.observations[i].image_id == i - begin &&
                       tracks.observations[i].point2D_idx == 0;
                one = one && tracks.observations[i].image_id == i - begin &&
                      tracks.observations[i].point2D_idx == 1;
            }
            has_split_zero = has_split_zero || zero;
            has_split_one = has_split_one || one;
        } else if (end - begin == 2) {
            const auto& a = tracks.observations[begin];
            const auto& b = tracks.observations[begin + 1];
            has_short_12 = has_short_12 || (a.image_id == 1 && b.image_id == 2 &&
                                             a.point2D_idx == 2 && b.point2D_idx == 2);
            has_short_01 = has_short_01 || (a.image_id == 0 && b.image_id == 1 &&
                                             a.point2D_idx == 3 && b.point2D_idx == 3);
        }
    }
    check(has_three_view && has_short_12 && has_short_01, "inlier masks and observation indices");
    check(has_split_zero && has_split_one, "split conflict retains both long tracks");
    const GlobalTracks longer = buildGlobalTracks(db, features, edges, 3);
    check(longer.size() == 3 && longer.conflicting == 1 && longer.too_short == 2,
          "minimum views");
    edges[2].status = RelativeViewStatus::NoPose;
    const GlobalTracks no_conflict = buildGlobalTracks(db, features, edges, 3);
    check(no_conflict.size() == 3 && no_conflict.conflicting == 0,
          "ignore rejected pair");
    edges[2].status = RelativeViewStatus::Accepted;
    const std::vector<uint8_t> accepted{1, 1, 0};
    const GlobalTracks filtered = buildGlobalTracks(db, features, edges, 3, &accepted);
    check(filtered.size() == 3 && filtered.conflicting == 0,
          "ignore rotation outliers");
    for (FeatureSet& f : features) f.keypoints[1].x = f.keypoints[0].x + 2;
    const GlobalTracks close_duplicates = buildGlobalTracks(db, features, edges);
    check(close_duplicates.size() == 4 && close_duplicates.conflicting == 0 &&
          close_duplicates.observations.size() == 10,
          "nearby duplicates retain one observation per image");
    bool threw = false;
    db.pairs[0].matches.mut()[0].idx1 = 999;
    try { buildGlobalTracks(db, features, edges); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "invalid feature rejected");
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testGlobalTracks); }
