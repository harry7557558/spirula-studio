#include <algorithm>
#include <cstdio>
#include <vector>

#include "sfm/map/Mapper.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int trackPolicyTest(int, char**) {
    int fails = 0;
    auto check = [&](bool ok, const char* label) {
        std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok) ++fails;
    };
    std::vector<AssociationVote> votes = {
        {9, 1, 3, false}, {4, 2, 10, false}, {4, 2, 10, false},
        {9, 3, 3, false}, {9, 4, 3, false}};
    auto a = supportedAssociation(votes);
    check(a.point == 9 && a.support == 3 && a.alternatives == 2 && !a.tied,
          "distinct supporting images outweigh track length and duplicate edges");
    std::reverse(votes.begin(), votes.end());
    a = supportedAssociation(votes);
    check(a.point == 9 && a.support == 3, "association is independent of pair order");
    votes.push_back({4, 7, 10, true});
    a = supportedAssociation(votes);
    check(a.point == 4 && a.nearby, "sequence support retains precedence");
    votes = {{1, 1, 3, false}, {2, 2, 20, false}};
    a = supportedAssociation(votes);
    check(a.point == 2 && a.tied, "track-length tie break retains ambiguity");
    TrackPolicy policy;
    check(policy.allowsMerge(10, 10) && !policy.allowsMerge(11, 10), "fusion cap boundary");
    policy.merge_max_track = 0;
    check(policy.allowsMerge(1000, 1000), "unbounded fusion policy");
    std::vector<PointAssociation> candidates = {
        {10, 4, 1, false}, {11, 6, 9, false}, {10, 7, 3, false}, {11, 8, 1, true}};
    check(uniquePointAssociations(candidates) == std::vector<size_t>({2, 3}),
          "one feature per landmark favors distinct views and sequence support");
    candidates = {{10, 8, 2, false}, {10, 4, 2, false}, {12, 5, 1, false}};
    check(uniquePointAssociations(candidates) == std::vector<size_t>({1, 2}),
          "equal-support landmark assignment uses a stable feature tie break");
    check(bestPointObservations({10, 10, 11, 11, 12},
                                {3, 0.2, 0.1, 0.1, std::numeric_limits<double>::infinity()}) ==
              std::vector<size_t>({1, 2}),
          "post-pose assignment favors residuals, rejects invalid observations, and breaks ties stably");

    MatchesDatabase pair_database;
    pair_database.images.resize(3);
    TwoViewMatches ab, bc;
    ab.image1 = 0; ab.image2 = 1;
    ab.matches.push_back({0, 0}); ab.matches.push_back({1, 1});
    bc.image1 = 1; bc.image2 = 2; bc.matches.push_back({1, 1});
    pair_database.pairs = {ab, bc};
    CorrespondenceGraph pair_graph;
    pair_graph.build(pair_database, {2, 2, 2});
    check(pair_graph.isolatedPair(0, 0) && pair_graph.isolatedPair(1, 0) &&
          !pair_graph.isolatedPair(0, 1) && !pair_graph.isolatedPair(2, 1),
          "isolated pair exclusion preserves tracks that can grow through a third view");

    constexpr uint32_t n = 40, views = 4;
    Camera cam = Camera::defaultFor(1, 800, 600, 500, CamModel::Pinhole);
    MatchesDatabase db;
    Reconstruction initial;
    initial.cameras[1] = cam;
    db.cameras = {cam};
    db.camera_ids.assign(views, 1);
    db.focal_prior = {1};
    std::vector<FeatureSet> features(views);
    std::vector<Vec3> xyz;
    for (uint32_t p = 0; p < n; ++p)
        xyz.push_back({0.25 * (int(p % 8) - 4), 0.25 * (int(p / 8) - 2),
                       4.0 + 0.07 * p});
    for (uint32_t i = 0; i < views; ++i) {
        Image im;
        im.id = i;
        im.name = "view" + std::to_string(i) + ".jpg";
        im.camera_id = 1;
        im.registered = true;
        im.pose = {mat3Identity(), {0.4 * i - 0.6, 0, 0}};
        im.point3D_ids.assign(n, kInvalidPoint3D);
        features[i].width = 800;
        features[i].height = 600;
        for (const Vec3& X : xyz) {
            Vec2 px = cam.project(X + im.pose.t);
            im.points2D.push_back(px);
            features[i].keypoints.push_back({(float)px.x, (float)px.y, 1, 0, 0});
        }
        db.images.push_back({im.name, n});
        initial.images[i] = std::move(im);
        for (uint32_t j = 0; j < i; ++j) {
            TwoViewMatches pair;
            pair.image1 = j;
            pair.image2 = i;
            pair.config = (int)TwoViewConfig::Uncalibrated;
            for (uint32_t p = 0; p < n; ++p) pair.matches.push_back({p, p});
            db.pairs.push_back(std::move(pair));
        }
    }
    for (uint32_t p = 0; p < n; ++p) {
        initial.addPoint3D(xyz[p], {{0, p}, {1, p}});
        initial.addPoint3D(xyz[p], {{2, p}, {3, p}});
    }
    auto refine = [&](int completion, size_t cap, int min_model_size = 10) {
        MapperOptions opt;
        opt.verbose = false;
        opt.focal = 500;
        opt.ba_real = opt.ba_real_coarse = "cpu";
        opt.threads = 1;
        opt.min_model_size = min_model_size;
        opt.tracks.completion = completion;
        opt.tracks.merge_max_track = cap;
        Mapper mapper(db, features, opt, db.camera_ids);
        return mapper.refine(initial);
    };
    const auto legacy = refine(0, 20);
    const auto complete = refine(1, 20);
    const auto capped = refine(1, 3);
    const auto guarded_seed = refine(3, 20);
    const auto guarded_model = refine(3, 20, views);
    check(legacy.points3D.size() == 2 * n, "legacy refinement stops before a late track union");
    check(complete.points3D.size() == n && complete.numRegistered() == views,
          "final completion joins fragmented tracks without losing cameras");
    bool full_tracks = true;
    for (const auto& p : complete.points3D) full_tracks &= p.second.track.size() == views;
    check(full_tracks, "completed tracks keep all four independent observations");
    check(capped.points3D.size() == 2 * n, "configured fusion cap is enforced in refinement");
    check(guarded_seed.points3D.size() == 2 * n && guarded_seed.numRegistered() == views,
          "established-model completion leaves a smaller model on the legacy schedule");
    check(guarded_model.points3D.size() == n && guarded_model.numRegistered() == views,
          "established-model completion joins tracks at the configured image threshold");

    Reconstruction seed;
    seed.cameras = initial.cameras;
    for (uint32_t i = 0; i < 2; ++i) {
        seed.images[i] = initial.images.at(i);
        seed.images[i].point3D_ids.assign(n, kInvalidPoint3D);
    }
    for (uint32_t p = 0; p < n; ++p) seed.addPoint3D(xyz[p], {{0, p}, {1, p}});
    db.images.resize(3);
    db.camera_ids.resize(3);
    features.resize(3);
    for (uint32_t p = 0; p < n; ++p) {
        Keypoint duplicate = features[2].keypoints[p];
        duplicate.x += 0.15f;
        features[2].keypoints.push_back(duplicate);
    }
    db.images[2].num_features = 2 * n;
    db.pairs.erase(std::remove_if(db.pairs.begin(), db.pairs.end(),
                                 [](const TwoViewMatches& p) { return p.image2 == 3; }), db.pairs.end());
    for (auto& pair : db.pairs)
        if (pair.image1 == 1 && pair.image2 == 2) {
            std::vector<FeatureMatch> shifted(pair.matches.begin(), pair.matches.end());
            for (auto& match : shifted) match.idx2 += n;
            pair.matches = std::move(shifted);
        }
    auto grow = [&](int unique) {
        MapperOptions opt;
        opt.verbose = false;
        opt.focal = 500;
        opt.ba_real = opt.ba_real_coarse = "cpu";
        opt.threads = 1;
        opt.tracks.unique_points = unique;
        Mapper mapper(db, features, opt, db.camera_ids);
        Mapper::GrowStats stats;
        return mapper.growByPnP(seed, &stats, {}, 3);
    };
    const auto repeated = grow(0), unique = grow(1), late = grow(2);
    auto duplicateViews = [](const Reconstruction& rec) {
        size_t count = 0;
        for (const auto& p : rec.points3D) {
            std::vector<uint32_t> images;
            for (const auto& t : p.second.track) images.push_back(t.image_id);
            std::sort(images.begin(), images.end());
            count += images.end() - std::unique(images.begin(), images.end());
        }
        return count;
    };
    check(repeated.numRegistered() == 3 && duplicateViews(repeated) == n,
          "registration fixture exposes repeated features voting for the same landmark");
    check(unique.numRegistered() == 3 && duplicateViews(unique) == 0,
          "unique landmark registration preserves cameras and one observation per image");
    check(late.numRegistered() == 3 && duplicateViews(late) == 0,
          "post-pose selection preserves registration and one observation per image");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, trackPolicyTest); }
