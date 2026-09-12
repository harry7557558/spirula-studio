// Lossless feature compaction invariants and failure paths.
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/FeatureCompaction.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static FeatureSet features(uint32_t count, float tag) {
    FeatureSet set;
    set.width = 1920;
    set.height = 1080;
    set.extract_width = 960;
    set.extract_height = 540;
    set.exif_focal = 733.25 + tag;
    set.exif_camera = "camera-" + std::to_string((int)tag);
    set.exif_orientation = 6;
    set.dim = 4;
    set.dtype = DType::U8;
    set.keypoints.resize(count);
    set.descriptors.resize(size_t(count) * set.dim);
    set.colors.resize(size_t(count) * 3);
    for (uint32_t i = 0; i < count; i++) {
        set.keypoints[i] = {tag + 10 * i, tag + 20 * i, tag + i, tag - i, tag + 2 * i};
        for (uint32_t d = 0; d < set.dim; d++)
            set.descriptors[size_t(i) * set.dim + d] = uint8_t(tag + 7 * i + d);
        for (uint32_t c = 0; c < 3; c++)
            set.colors[size_t(i) * 3 + c] = uint8_t(tag + 11 * i + c);
    }
    return set;
}

static MatchesDatabase database() {
    MatchesDatabase db;
    db.images = {{"cam0/a", 5}, {"cam1/b", 4}, {"cam0/unused", 3}};
    db.cameras = {Camera::defaultFor(7, 1920, 1080, 900, CamModel::OpenCVFisheye)};
    db.camera_ids = {7, 7, 7};
    db.focal_prior = {1};

    TwoViewMatches verified;
    verified.image1 = 0;
    verified.image2 = 1;
    verified.config = 3;
    verified.matches = {{1, 0, 1.0f}, {3, 1, 2.0f}, {4, 2, 3.0f}};

    TwoViewMatches raw;
    raw.image1 = 0;
    raw.image2 = 1;
    raw.config = 0;
    raw.matches = {{1, 3, 4.0f}};
    db.pairs = {verified, raw};
    return db;
}

static std::vector<FeatureSet> featureSets() {
    return {features(5, 10), features(4, 20), features(3, 30)};
}

static bool sameKeypoint(const Keypoint& a, const Keypoint& b) {
    return a.x == b.x && a.y == b.y && a.scale == b.scale &&
           a.orientation == b.orientation && a.response == b.response;
}

static bool sameCamera(const Camera& a, const Camera& b) {
    return a.id == b.id && a.width == b.width && a.height == b.height &&
           a.model == b.model && a.fx == b.fx && a.fy == b.fy && a.cx == b.cx &&
           a.cy == b.cy && a.k1 == b.k1 && a.k2 == b.k2 && a.p1 == b.p1 &&
           a.p2 == b.p2 && a.k3 == b.k3 && a.k4 == b.k4 && a.sx1 == b.sx1 &&
           a.sy1 == b.sy1 && a.pixel_scale == b.pixel_scale;
}

static bool sameFeatureRow(const FeatureSet& a, uint32_t ai,
                           const FeatureSet& b, uint32_t bi) {
    if (!sameKeypoint(a.keypoints[ai], b.keypoints[bi])) return false;
    for (size_t c = 0; c < 3; c++)
        if (a.colors[size_t(ai) * 3 + c] != b.colors[size_t(bi) * 3 + c]) return false;
    const size_t row = size_t(a.dim) * dtypeSize(a.dtype);
    for (size_t d = 0; d < row; d++)
        if (a.descriptors[size_t(ai) * row + d] != b.descriptors[size_t(bi) * row + d])
            return false;
    return true;
}

static bool throwsWith(const std::function<void()>& body, const std::string& text) {
    try {
        body();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(text) != std::string::npos;
    }
    return false;
}

static void check(bool condition, const char* message, int& fails) {
    if (condition) return;
    std::printf("  FAIL: %s\n", message);
    fails++;
}

static std::vector<FeatureSet> compactAll(const std::vector<FeatureSet>& input,
                                          const FeatureCompactionPlan& plan) {
    std::vector<FeatureSet> compact(input.size());
    for (size_t i = 0; i < input.size(); i++)
        compact[i] = compactFeatureSet(input[i], plan.old_to_new[i], plan.compact_counts[i]);
    return compact;
}

static int cmdFeatureCompactionTest(int, char**) {
    int fails = 0;

    MatchesDatabase db = database();
    const MatchesDatabase original_db = db;
    const std::vector<FeatureSet> original_features = featureSets();
    FeatureCompactionPlan plan = buildFeatureCompactionPlan(db);
    const uint32_t expected0[] = {kUnusedFeature, 0, kUnusedFeature, 1, 2};
    for (uint32_t i = 0; i < 5; i++)
        check(plan.old_to_new[0][i] == expected0[i], "stable feature remap", fails);
    check(plan.compact_counts == std::vector<uint32_t>({3, 4, 0}) &&
              plan.stats.original_features == 12 && plan.stats.compact_features == 7 &&
              plan.stats.correspondences == 4 && plan.stats.pairs == 2 &&
              plan.stats.zero_feature_images == 1,
          "plan counts", fails);

    std::vector<FeatureSet> compact = compactAll(original_features, plan);
    remapMatches(db, plan, compact);

    check(db.images.size() == original_db.images.size() &&
              db.images[2].name == "cam0/unused" && compact[2].count() == 0 &&
              db.images[2].num_features == 0 && compact[2].width == 1920 &&
              compact[2].height == 1080 && compact[2].exif_camera == "camera-30" &&
              compact[2].exif_orientation == 6,
          "zero-active image and metadata", fails);
    check(db.pairs.size() == original_db.pairs.size(), "pair count", fails);
    for (size_t p = 0; p < db.pairs.size(); p++) {
        check(db.pairs[p].image1 == original_db.pairs[p].image1 &&
                  db.pairs[p].image2 == original_db.pairs[p].image2 &&
                  db.pairs[p].config == original_db.pairs[p].config &&
                  db.pairs[p].matches.size() == original_db.pairs[p].matches.size(),
              "pair identity, config and correspondence count", fails);
        for (size_t k = 0; k < db.pairs[p].matches.size(); k++) {
            const FeatureMatch& old = original_db.pairs[p].matches[k];
            const FeatureMatch& now = db.pairs[p].matches[k];
            check(now.distance == old.distance &&
                      sameFeatureRow(original_features[db.pairs[p].image1], old.idx1,
                                     compact[db.pairs[p].image1], now.idx1) &&
                      sameFeatureRow(original_features[db.pairs[p].image2], old.idx2,
                                     compact[db.pairs[p].image2], now.idx2),
                  "feature-row and endpoint identity", fails);
        }
    }
    check(db.pairs[1].config == 0 && db.pairs[1].matches[0].idx1 == 0 &&
              db.pairs[1].matches[0].idx2 == 3,
          "config-zero stored match", fails);
    check(db.camera_ids == original_db.camera_ids &&
              db.focal_prior == original_db.focal_prior &&
              db.cameras.size() == original_db.cameras.size() &&
              sameCamera(db.cameras[0], original_db.cameras[0]),
          "camera setup", fails);

    {
        MatchesDatabase all;
        all.images = {{"a", 3}, {"b", 3}};
        TwoViewMatches pair;
        pair.image1 = 0;
        pair.image2 = 1;
        pair.config = 1;
        pair.matches = {{0, 2, 0}, {1, 1, 0}, {2, 0, 0}};
        all.pairs = {pair};
        FeatureCompactionPlan all_plan = buildFeatureCompactionPlan(all);
        FeatureSet before = features(3, 50);
        FeatureSet after =
            compactFeatureSet(before, all_plan.old_to_new[0], all_plan.compact_counts[0]);
        bool same = after.keypoints.size() == before.keypoints.size() &&
                    after.descriptors == before.descriptors && after.colors == before.colors;
        for (uint32_t i = 0; same && i < before.count(); i++)
            same = sameKeypoint(before.keypoints[i], after.keypoints[i]);
        check(same, "all-used input identity", fails);
    }

    auto badEndpoint = [&](bool second) {
        MatchesDatabase bad;
        bad.images = {{"a", 2}, {"b", 2}};
        TwoViewMatches pair;
        pair.image1 = 0;
        pair.image2 = 1;
        pair.matches = {second ? FeatureMatch{0, 2, 0} : FeatureMatch{2, 0, 0}};
        bad.pairs = {pair};
        return bad;
    };
    check(throwsWith([&] { buildFeatureCompactionPlan(badEndpoint(false)); }, "idx1 2"),
          "invalid idx1", fails);
    check(throwsWith([&] { buildFeatureCompactionPlan(badEndpoint(true)); }, "idx2 2"),
          "invalid idx2", fails);

    auto badImage = [&](bool second) {
        MatchesDatabase bad;
        bad.images = {{"a", 1}, {"b", 1}};
        TwoViewMatches pair;
        pair.image1 = second ? 0 : 2;
        pair.image2 = second ? 2 : 1;
        pair.matches = {{0, 0, 0}};
        bad.pairs = {pair};
        return bad;
    };
    check(throwsWith([&] { buildFeatureCompactionPlan(badImage(false)); }, "image1 2"),
          "invalid image1", fails);
    check(throwsWith([&] { buildFeatureCompactionPlan(badImage(true)); }, "image2 2"),
          "invalid image2", fails);

    check(throwsWith(
              [&] {
                  feature_compaction_detail::checkedCompactIndex(uint64_t(kUnusedFeature));
              },
              "stored match index range"),
          "uint32 endpoint boundary", fails);

    check(throwsWith(
              [&] { compactFeatureSet(features(2, 1), {0}, 1); },
              "feature-file count"),
          "feature-file count mismatch", fails);
    check(throwsWith(
              [&] {
                  FeatureSet malformed = features(2, 1);
                  malformed.colors.pop_back();
                  compactFeatureSet(std::move(malformed), {0, 1}, 2);
              },
              "feature colors"),
          "malformed color rows", fails);
    check(throwsWith(
              [&] {
                  FeatureSet malformed = features(2, 1);
                  malformed.descriptors.pop_back();
                  compactFeatureSet(std::move(malformed), {0, 1}, 2);
              },
              "descriptor data"),
          "malformed descriptor rows", fails);

    auto rejectsMutation = [&](const std::function<void(MatchesDatabase&)>& mutate,
                               const std::string& expected) {
        MatchesDatabase changed = database();
        FeatureCompactionPlan snapshot = buildFeatureCompactionPlan(changed);
        std::vector<FeatureSet> changed_features = compactAll(featureSets(), snapshot);
        mutate(changed);
        return throwsWith([&] { remapMatches(changed, snapshot, changed_features); }, expected);
    };
    check(rejectsMutation([](MatchesDatabase& value) { value.images[0].num_features++; },
                          "feature count changed"),
          "feature-count snapshot", fails);
    check(rejectsMutation([](MatchesDatabase& value) { value.cameras[0].fx += 1; },
                          "camera setup changed"),
          "camera snapshot", fails);
    check(rejectsMutation([](MatchesDatabase& value) { value.pairs[0].image1 = 2; },
                          "pair identity"),
          "pair identity snapshot", fails);
    check(rejectsMutation([](MatchesDatabase& value) { std::swap(value.pairs[0], value.pairs[1]); },
                          "pair identity"),
          "pair-order snapshot", fails);
    check(rejectsMutation([](MatchesDatabase& value) { value.pairs.pop_back(); },
                          "pair count changed"),
          "pair-count snapshot", fails);
    check(rejectsMutation([](MatchesDatabase& value) { value.pairs[0].config = 9; },
                          "pair identity"),
          "pair-config snapshot", fails);
    check(rejectsMutation([](MatchesDatabase& value) { value.pairs[0].matches.pop_back(); },
                          "pair identity"),
          "correspondence-count snapshot", fails);

    std::printf("feature compaction: 12 -> 7 features, 4 stored correspondences retained\n");
    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main() { return sfmTestMain(0, nullptr, cmdFeatureCompactionTest); }
