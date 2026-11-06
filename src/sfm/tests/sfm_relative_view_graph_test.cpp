#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <vector>

#include "sfm/core/Pose.h"
#include "sfm/map/RelativeViewGraph.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int testRelativeViewGraph(int, char**) {
    MatchesDatabase db;
    db.images.resize(3);
    std::vector<FeatureSet> features(3);
    std::vector<Camera> cameras(3);
    for (size_t i = 0; i < cameras.size(); i++) {
        cameras[i].model = CamModel::Pinhole;
        cameras[i].fx = cameras[i].fy = 1000;
        cameras[i].cx = 640;
        cameras[i].cy = 480;
        features[i].width = 1280;
        features[i].height = 960;
    }
    const Mat3 R = angleAxisToRotation({0.03, 0.12, -0.05});
    const Vec3 t{0.8, 0.1, 0.2};
    std::mt19937 rng(41);
    std::uniform_real_distribution<double> xy(-2, 2), z(4, 10);
    TwoViewMatches pair;
    pair.image1 = 0;
    pair.image2 = 1;
    pair.config = (int)TwoViewConfig::Uncalibrated;
    for (uint32_t i = 0; i < 150; i++) {
        Vec3 p{xy(rng), xy(rng), z(rng)};
        Vec3 q = mul(R, p) + t;
        Keypoint a, b;
        a.x = (float)(1000 * p.x / p.z + 640);
        a.y = (float)(1000 * p.y / p.z + 480);
        b.x = (float)(1000 * q.x / q.z + 640);
        b.y = (float)(1000 * q.y / q.z + 480);
        features[0].keypoints.push_back(a);
        features[1].keypoints.push_back(b);
        pair.matches.push_back({i, i});
    }
    db.pairs.push_back(pair);
    TwoViewMatches reversed;
    reversed.image1 = 1;
    reversed.image2 = 0;
    reversed.config = pair.config;
    reversed.matches = pair.matches;
    db.pairs.push_back(reversed);
    TwoViewMatches rejected = pair;
    rejected.config = (int)TwoViewConfig::PlanarOrPanoramic;
    db.pairs.push_back(rejected);
    rejected.config = 0;
    db.pairs.push_back(rejected);
    rejected.config = pair.config;
    rejected.matches.mut()[0].idx1 = 9999;
    db.pairs.push_back(rejected);
    rejected.image2 = 10;
    db.pairs.push_back(rejected);
    rejected = pair;
    rejected.matches.resize(7);
    db.pairs.push_back(rejected);

    RelativeViewOptions opt;
    opt.threads = 1;
    opt.geometry.ransac.min_num_trials = 30;
    opt.geometry.ransac.max_num_trials = 200;
    const auto edges = buildRelativeViewGraph(db, features, cameras, opt);
    opt.threads = 3;
    const auto parallel = buildRelativeViewGraph(db, features, cameras, opt);
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
        failures += !ok;
    };
    check(edges[0].status == RelativeViewStatus::Accepted && edges[0].inliers >= 140,
          "non-planar pair");
    const double rotation_error = rotationToAngleAxis(mul(edges[0].pose.R, transpose(R))).norm();
    check(rotation_error < 0.02 && edges[0].pose.t.normalized().dot(t.normalized()) > 0.99,
          "relative pose convention");
    const Mat3 inverse = transpose(edges[0].pose.R);
    const Vec3 inverse_t = mul(inverse, edges[0].pose.t) * -1;
    check(edges[1].status == RelativeViewStatus::Accepted &&
          rotationToAngleAxis(mul(edges[1].pose.R, transpose(inverse))).norm() < 0.02 &&
          edges[1].pose.t.normalized().dot(inverse_t.normalized()) > 0.99,
          "reversed pair convention");
    check(edges[2].status == RelativeViewStatus::Degenerate &&
          edges[3].status == RelativeViewStatus::Unverified &&
          edges[4].status == RelativeViewStatus::InvalidFeature &&
          edges[5].status == RelativeViewStatus::InvalidPair &&
          edges[6].status == RelativeViewStatus::TooFewMatches,
          "reject invalid and unsupported pairs");
    std::vector<FeatureSet> rotation_features(2);
    for (FeatureSet& set : rotation_features) { set.width = 1280; set.height = 960; }
    const Mat3 panoramic_rotation = angleAxisToRotation({0.12, -0.07, 0.04});
    TwoViewMatches panoramic;
    panoramic.image1 = 0;
    panoramic.image2 = 1;
    panoramic.config = (int)TwoViewConfig::PlanarOrPanoramic;
    for (uint32_t i = 0; i < 160; i++) {
        const Vec3 p{xy(rng), xy(rng), z(rng)};
        Vec3 q = mul(panoramic_rotation, p);
        if (i >= 150) q = {xy(rng), xy(rng), z(rng)};
        const Vec2 a = cameras[0].project(p), b = cameras[1].project(q);
        rotation_features[0].keypoints.push_back({(float)a.x, (float)a.y});
        rotation_features[1].keypoints.push_back({(float)b.x, (float)b.y});
        panoramic.matches.push_back({i, i});
    }
    const RelativeViewEdge panoramic_edge =
        estimateRelativeView(panoramic, rotation_features, std::vector<Camera>{cameras[0], cameras[1]}, opt);
    check(panoramic_edge.status == RelativeViewStatus::Accepted && panoramic_edge.inliers >= 145 &&
              panoramic_edge.pose.t.norm() == 0 &&
              rotationToAngleAxis(mul(panoramic_edge.pose.R, transpose(panoramic_rotation))).norm() < 0.01,
          "pure rotation planar/panoramic edge");
    std::vector<FeatureSet> planar_features(2);
    for (FeatureSet& set : planar_features) { set.width = 1280; set.height = 960; }
    TwoViewMatches planar;
    planar.image1 = 0;
    planar.image2 = 1;
    planar.config = (int)TwoViewConfig::PlanarOrPanoramic;
    std::normal_distribution<double> planar_noise(0, 0.3);
    for (uint32_t i = 0; i < 180; i++) {
        const double x = xy(rng), y = xy(rng);
        const Vec3 p{x, y, 6.0 + 0.3 * x - 0.2 * y};
        Vec3 q = mul(R, p) + t;
        if (i >= 160) q = {xy(rng), xy(rng), z(rng)};
        const Vec2 a = cameras[0].project(p), b = cameras[1].project(q);
        planar_features[0].keypoints.push_back({(float)(a.x + planar_noise(rng)),
                                                 (float)(a.y + planar_noise(rng))});
        planar_features[1].keypoints.push_back({(float)(b.x + planar_noise(rng)),
                                                 (float)(b.y + planar_noise(rng))});
        planar.matches.push_back({i, i});
    }
    const RelativeViewEdge planar_edge =
        estimateRelativeView(planar, planar_features, std::vector<Camera>{cameras[0], cameras[1]}, opt);
    check(planar_edge.status == RelativeViewStatus::Accepted && planar_edge.inliers >= 150 &&
              rotationToAngleAxis(mul(planar_edge.pose.R, transpose(R))).norm() < 0.02 &&
              planar_edge.pose.t.normalized().dot(t.normalized()) > 0.99,
          "planar homography translation edge");
    bool equal = edges.size() == parallel.size();
    for (size_t i = 0; i < edges.size(); i++)
        equal = equal && edges[i].status == parallel[i].status &&
                edges[i].match_mask == parallel[i].match_mask &&
                edges[i].pose.R == parallel[i].pose.R;
    check(equal, "deterministic across thread counts");
    auto wideCamera = [&](CamModel model) {
        std::vector<FeatureSet> fs(2);
        std::vector<Camera> cams(2);
        for (Camera& c : cams) {
            c.model = model;
            c.fx = c.fy = 600;
            c.cx = 1200;
            c.cy = 600;
            c.width = 2400;
            c.height = 1200;
            if (model == CamModel::OpenCVFisheye) c.k1 = 0.01;
        }
        TwoViewMatches pm;
        pm.image1 = 0;
        pm.image2 = 1;
        pm.config = (int)TwoViewConfig::Uncalibrated;
        std::uniform_real_distribution<double> xyz(-1, 1), radius(4, 10);
        for (uint32_t k = 0; k < 200; k++) {
            const Vec3 X = Vec3{xyz(rng), xyz(rng), xyz(rng)}.normalized() * radius(rng);
            const Vec3 q = mul(R, X) + t;
            const Vec2 a = cams[0].project(X), b = cams[1].project(q);
            Keypoint ka, kb;
            ka.x = (float)a.x; ka.y = (float)a.y;
            kb.x = (float)b.x; kb.y = (float)b.y;
            fs[0].keypoints.push_back(ka);
            fs[1].keypoints.push_back(kb);
            pm.matches.push_back({k, k});
        }
        const RelativeViewEdge e = estimateRelativeView(pm, fs, cams, opt);
        return e.status == RelativeViewStatus::Accepted && e.inliers >= 180 &&
               rotationToAngleAxis(mul(e.pose.R, transpose(R))).norm() < 0.02 &&
               e.pose.t.normalized().dot(t.normalized()) > 0.99;
    };
    check(wideCamera(CamModel::OpenCVFisheye), "fisheye bearings behind camera");
    check(wideCamera(CamModel::Equirect), "equirectangular bearings behind camera");
    auto widePlanar = [&](CamModel model) {
        std::vector<FeatureSet> fs(2);
        std::vector<Camera> cams(2);
        for (Camera& c : cams) {
            c = Camera::defaultFor(0, 2400, 1200, 0, model);
            if (model == CamModel::OpenCVFisheye) c.k1 = 0.01;
        }
        fs[0].width = fs[1].width = 2400;
        fs[0].height = fs[1].height = 1200;
        TwoViewMatches pm;
        pm.image1 = 0;
        pm.image2 = 1;
        pm.config = (int)TwoViewConfig::PlanarOrPanoramic;
        for (uint32_t k = 0; k < 180; k++) {
            const double x = xy(rng), y = xy(rng);
            const Vec3 X{x, y, 6.0 + 0.2 * x - 0.1 * y};
            const Vec3 q = mul(R, X) + t;
            const Vec2 a = cams[0].project(X), b = cams[1].project(q);
            fs[0].keypoints.push_back({(float)a.x, (float)a.y});
            fs[1].keypoints.push_back({(float)b.x, (float)b.y});
            pm.matches.push_back({k, k});
        }
        const RelativeViewEdge e = estimateRelativeView(pm, fs, cams, opt);
        return e.status == RelativeViewStatus::Accepted && e.inliers >= 170 &&
               rotationToAngleAxis(mul(e.pose.R, transpose(R))).norm() < 0.02 &&
               e.pose.t.normalized().dot(t.normalized()) > 0.99;
    };
    check(widePlanar(CamModel::OpenCVFisheye), "planar fisheye homography pose");
    check(widePlanar(CamModel::Equirect), "planar equirectangular homography pose");
    std::vector<Camera> invalid_cameras = cameras;
    invalid_cameras[0].fx = 0;
    check(estimateRelativeView(pair, features, invalid_cameras, opt).status ==
              RelativeViewStatus::InvalidCamera,
          "invalid camera calibration");
    bool threw = false;
    try { buildRelativeViewGraph(db, features, std::vector<Camera>{}, opt); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "mismatched input sizes");
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testRelativeViewGraph); }
