#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "sfm/map/GlobalTracks.h"
#include "sfm/map/GlobalPositionGpu.h"
#include "sfm/map/GlobalReconstruction.h"
#include "sfm/map/RotationAverage.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

class RotationPrior final : public PriorSource {
public:
    bool has(uint32_t image) const override { return image < 2; }
    bool relativeRotation(uint32_t i, uint32_t j, Mat3& R, double& sigma) const override {
        if (i != 0 || j != 1) return false;
        R = angleAxisToRotation({0.02, -0.03, 0.04});
        sigma = 0.01;
        return true;
    }
    std::vector<uint32_t> neighbours(uint32_t image) const override {
        return image < 2 ? std::vector<uint32_t>{1 - image} : std::vector<uint32_t>{};
    }
    PosePriors factors(const std::vector<PosedImage>&) override { return {}; }
};

class AbsolutePositionPrior final : public PriorSource {
public:
    explicit AbsolutePositionPrior(std::vector<Vec3> centres) : centres_(std::move(centres)) {}
    bool has(uint32_t image) const override { return image < centres_.size(); }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override { return false; }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    PosePriors factors(const std::vector<PosedImage>& images) override {
        calls++;
        PosePriors out;
        for (const PosedImage& image : images) {
            if (!has(image.image)) continue;
            PriorCentre q;
            q.n = 1;
            q.img[0] = image.image;
            q.b = centres_[image.image];
            q.sigma = {0.02, 0.02, 0.02};
            q.cauchy = 3.0;
            out.centres.push_back(q);
        }
        return out;
    }
    int calls = 0;

private:
    std::vector<Vec3> centres_;
};

static bool checkRotationPrior(const Camera& camera, const RelativeViewOptions& relative) {
    MatchesDatabase db;
    db.images.resize(2);
    std::vector<FeatureSet> features(2);
    for (FeatureSet& f : features) {
        f.width = 1280;
        f.height = 960;
    }
    RotationPrior prior;
    GlobalReconstructionOptions opt;
    opt.relative = relative;
    opt.rotation_priors = &prior;
    GlobalReconstructionStats stats;
    const auto models = buildGlobalReconstructions(db, features, {1, 1}, {{1, camera}}, {1}, opt, stats);
    return models.empty() && stats.rotation_priors == 1 && stats.edges == 0 && stats.rotation_converged;
}

static bool checkUnknownFocal(const RelativeViewOptions& relative) {
    constexpr uint32_t images = 6, points = 160;
    Camera truth;
    truth.model = CamModel::Pinhole;
    truth.fx = truth.fy = 1000;
    truth.cx = 640; truth.cy = 480;
    MatchesDatabase db;
    db.images.resize(images);
    std::vector<FeatureSet> features(images);
    std::vector<Pose> poses(images);
    for (uint32_t i = 0; i < images; i++) {
        const double a = 0.8 * i;
        poses[i].R = angleAxisToRotation({0.04 * std::sin(a), 0.06 * i, 0.025 * std::cos(a)});
        const Vec3 center{0.6 * i, 0.35 * std::sin(a), 0.3 * std::cos(a)};
        poses[i].t = mul(poses[i].R, center) * -1.0;
        features[i].width = 1280; features[i].height = 960;
    }
    std::mt19937 rng(183);
    std::uniform_real_distribution<double> xy(-2, 2), z(5, 11);
    std::normal_distribution<double> noise(0, 0.02);
    for (uint32_t k = 0; k < points; k++) {
        const Vec3 X{xy(rng), xy(rng), z(rng)};
        for (uint32_t i = 0; i < images; i++) {
            const Vec2 p = truth.project(mul(poses[i].R, X) + poses[i].t);
            Keypoint kp; kp.x = (float)(p.x + noise(rng)); kp.y = (float)(p.y + noise(rng));
            features[i].keypoints.push_back(kp);
        }
    }
    for (uint32_t i = 0; i < images; i++) for (uint32_t j = i + 1; j < images; j++) {
        TwoViewMatches pair;
        pair.image1 = i; pair.image2 = j;
        pair.config = (int)TwoViewConfig::Uncalibrated;
        for (uint32_t k = 0; k < points; k++) pair.matches.push_back({k, k});
        db.pairs.push_back(std::move(pair));
    }
    Camera initial = truth;
    initial.fx = initial.fy = 1250;
    GlobalReconstructionOptions options;
    options.relative = relative;
    GlobalReconstructionStats stats;
    const auto models = buildGlobalReconstructions(db, features, std::vector<uint32_t>(images, 1),
                                                    {{1, initial}}, {}, options, stats);
    return models.size() == 1 && models[0].numRegistered() == images &&
           models[0].points3D.size() >= 100 &&
           std::abs(models[0].cameras.at(1).focal() / truth.focal() - 1) < 0.05;
}

static bool checkPositionPrior(const MatchesDatabase& db, const std::vector<FeatureSet>& features,
                               const std::vector<Pose>& poses, const std::vector<Camera>& cameras,
                               const RelativeViewOptions& relative, bool gpu) {
    std::vector<Vec3> centres;
    centres.reserve(poses.size());
    for (const Pose& pose : poses) centres.push_back(cameraCenter(pose));
    AbsolutePositionPrior prior(std::move(centres));
    GlobalReconstructionOptions opt;
    opt.relative = relative;
    opt.position_priors = &prior;
    if (gpu) opt.position_solver = [](GlobalPositionProblem& problem, const GlobalPositionOptions& options) {
        return solveGlobalPositionGpu(problem, options);
    };
    GlobalReconstructionStats stats;
    const auto models = buildGlobalReconstructions(db, features,
        std::vector<uint32_t>(db.images.size(), 1), {{1, cameras[0]}}, {1}, opt, stats);
    double error = 0;
    if (models.size() == 1)
        for (uint32_t i = 0; i < poses.size(); i++)
            error = std::max(error, (cameraCenter(models[0].images.at(i).pose) - cameraCenter(poses[i])).norm());
    const bool ok = models.size() == 1 && models[0].numRegistered() == poses.size() && prior.calls == 1 &&
                    stats.position_priors == poses.size() && error < 0.08;
    std::printf("position priors%s: %zu models, %d calls, %zu factors, %.5g error\n",
                gpu ? " Vulkan" : "", models.size(), prior.calls, stats.position_priors, error);
    return ok;
}

static bool checkFixedRigReconstruction(const Camera& camera, const RelativeViewOptions& relative,
                                        bool gpu) {
    constexpr uint32_t frames = 3, sensors = 2, points = 120, images = frames * sensors;
    RigSpec spec;
    spec.members.resize(sensors);
    spec.members[0].has_ext = spec.members[1].has_ext = true;
    spec.members[0].ext = {mat3Identity(), {0, 0, 0}};
    spec.members[1].ext = {angleAxisToRotation({0.01, -0.02, 0.005}), {-0.3, 0.02, 0}};
    MatchesDatabase db;
    db.images.resize(images);
    std::vector<FeatureSet> features(images);
    std::vector<Pose> poses(images);
    for (uint32_t f = 0; f < frames; f++) {
        const Mat3 R = angleAxisToRotation({0.02 * f, 0.04 * f, -0.01 * f});
        const Vec3 center{0.7 * f, 0.12 * f, 0.04 * f};
        const Pose frame{R, mul(R, center) * -1.0};
        spec.frames.push_back({2 * f, 2 * f + 1});
        for (uint32_t s = 0; s < sensors; s++) {
            const uint32_t image = sensors * f + s;
            poses[image] = composePose(spec.members[s].ext, frame);
            features[image].width = 1280;
            features[image].height = 960;
        }
    }
    std::mt19937 rng(9);
    std::uniform_real_distribution<double> xy(-2, 2), depth(5, 10);
    for (uint32_t p = 0; p < points; p++) {
        const Vec3 X{xy(rng), xy(rng), depth(rng)};
        for (uint32_t image = 0; image < images; image++) {
            const Vec3 x = mul(poses[image].R, X) + poses[image].t;
            Keypoint kp;
            kp.x = (float)(camera.fx * x.x / x.z + camera.cx);
            kp.y = (float)(camera.fy * x.y / x.z + camera.cy);
            features[image].keypoints.push_back(kp);
        }
    }
    for (uint32_t i = 0; i < images; i++)
        for (uint32_t j = i + 1; j < images; j++) {
            TwoViewMatches pair;
            pair.image1 = i;
            pair.image2 = j;
            pair.config = (int)TwoViewConfig::Uncalibrated;
            for (uint32_t p = 0; p < points; p++) pair.matches.push_back({p, p});
            db.pairs.push_back(std::move(pair));
        }
    RigTable rigs;
    rigs.rigs.push_back(spec);
    rigs.index(images);
    GlobalReconstructionOptions opt;
    opt.relative = relative;
    opt.rigs = &rigs;
    if (gpu) opt.position_solver = [](GlobalPositionProblem& problem, const GlobalPositionOptions& options) {
        return solveGlobalPositionGpu(problem, options);
    };
    GlobalReconstructionStats stats;
    const auto models = buildGlobalReconstructions(db, features, std::vector<uint32_t>(images, 1),
                                                    {{1, camera}}, {1}, opt, stats);
    bool ok = models.size() == 1 && models[0].numRegistered() == images &&
              models[0].points3D.size() >= 90;
    if (ok) {
        for (uint32_t f = 0; f < frames; f++) {
            const Pose actual = relativePose(models[0].images.at(2 * f).pose,
                                             models[0].images.at(2 * f + 1).pose);
            ok = ok && (actual.t - spec.members[1].ext.t).norm() < 1e-8 &&
                 rotationToAngleAxis(mul(actual.R, transpose(spec.members[1].ext.R))).norm() < 1e-8;
        }
        const Vec3 a = mul(transpose(models[0].images.at(0).pose.R),
                           models[0].images.at(0).pose.t) * -1.0;
        const Vec3 b = mul(transpose(models[0].images.at(4).pose.R),
                           models[0].images.at(4).pose.t) * -1.0;
        ok = ok && std::abs((b - a).norm() - Vec3{1.4, 0.24, 0.08}.norm()) < 0.15;
    }
    return ok;
}

static int testGlobalFrontend(int argc, char** argv) {
    bool gpu = false;
    for (int i = 1; i < argc; i++) gpu = gpu || std::strcmp(argv[i], "--vulkan") == 0;
    constexpr uint32_t nimages = 4, npoints = 120;
    MatchesDatabase db;
    db.images.resize(nimages);
    std::vector<FeatureSet> features(nimages);
    std::vector<Camera> cameras(nimages);
    std::vector<Pose> poses(nimages);
    for (uint32_t i = 0; i < nimages; i++) {
        cameras[i].model = CamModel::Pinhole;
        cameras[i].fx = cameras[i].fy = 1000;
        cameras[i].cx = 640;
        cameras[i].cy = 480;
        features[i].width = 1280;
        features[i].height = 960;
        poses[i].R = angleAxisToRotation({0.02 * i, 0.045 * i, -0.01 * i});
        poses[i].t = {0.7 * i, 0.12 * i, 0.04 * i};
    }
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> xy(-2, 2), z(5, 10);
    std::normal_distribution<double> noise(0, 0.08);
    std::vector<Vec3> truth_points;
    for (uint32_t k = 0; k < npoints; k++) {
        const Vec3 X{xy(rng), xy(rng), z(rng)};
        truth_points.push_back(X);
        for (uint32_t i = 0; i < nimages; i++) {
            const Vec3 p = mul(poses[i].R, X) + poses[i].t;
            Keypoint kp;
            kp.x = (float)(1000 * p.x / p.z + 640 + noise(rng));
            kp.y = (float)(1000 * p.y / p.z + 480 + noise(rng));
            features[i].keypoints.push_back(kp);
        }
    }
    for (uint32_t i = 0; i < nimages; i++)
        for (uint32_t j = i + 1; j < nimages; j++) {
            TwoViewMatches pair;
            pair.image1 = i;
            pair.image2 = j;
            pair.config = (int)TwoViewConfig::Uncalibrated;
            for (uint32_t k = 0; k < npoints; k++)
                pair.matches.push_back({k, i == 0 && j == 1 && k < 15 ? (k + 40) % npoints : k});
            db.pairs.push_back(std::move(pair));
        }
    RelativeViewOptions options;
    options.threads = 2;
    options.geometry.ransac.min_num_trials = 50;
    options.geometry.ransac.max_num_trials = 300;
    const auto edges = buildRelativeViewGraph(db, features, cameras, options);
    const auto rotations = averageViewRotations(db, edges);
    const auto tracks = buildGlobalTracks(db, features, edges, 3, &rotations.accepted);
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
        failures += !ok;
    };
    size_t accepted = 0;
    for (uint8_t a : rotations.accepted) accepted += a;
    check(accepted == db.pairs.size(), "relative views survive rotation averaging");
    double worst = 0;
    for (uint32_t i = 0; i < nimages; i++) {
        const Mat3 expected = mul(poses[i].R, transpose(poses[0].R));
        worst = std::max(worst,
                         rotationToAngleAxis(mul(rotations.rotations[i], transpose(expected))).norm());
    }
    check(rotations.converged && worst < 0.03, "end-to-end orientations");
    size_t correct = 0;
    for (size_t t = 0; t < tracks.size(); t++) {
        bool same = true;
        for (size_t k = tracks.offsets[t] + 1; k < tracks.offsets[t + 1]; k++)
            same = same && tracks.observations[k].point2D_idx ==
                           tracks.observations[tracks.offsets[t]].point2D_idx;
        correct += same;
    }
    check(correct >= 100 && correct == tracks.size(), "end-to-end tracks reject outliers");
    GlobalReconstructionOptions reconstruction_opt;
    reconstruction_opt.relative = options;
    GlobalReconstructionStats stats;
    const auto models = buildGlobalReconstructions(db, features, std::vector<uint32_t>(nimages, 1),
                                                    {{1, cameras[0]}}, {1}, reconstruction_opt, stats);
    check(models.size() == 1 && models[0].numRegistered() == nimages &&
          models[0].points3D.size() >= 90, "global reconstruction from matches");
    if (!models.empty()) {
        double numerator = 0, denominator = 0;
        std::vector<Vec3> estimated, truth;
        for (uint32_t i = 0; i < nimages; i++) {
            const Pose& pose = models[0].images.at(i).pose;
            estimated.push_back(mul(transpose(pose.R), pose.t) * -1.0);
            truth.push_back(mul(transpose(poses[i].R), poses[i].t) * -1.0);
        }
        for (uint32_t i = 1; i < nimages; i++) {
            numerator += (estimated[i] - estimated[0]).dot(truth[i] - truth[0]);
            denominator += (estimated[i] - estimated[0]).dot(estimated[i] - estimated[0]);
        }
        const double scale = numerator / denominator;
        double error = 0;
        for (uint32_t i = 1; i < nimages; i++)
            error = std::max(error, ((estimated[i] - estimated[0]) * scale -
                                     (truth[i] - truth[0])).norm());
        std::printf("camera position error: %.5g\n", error);
        check(std::isfinite(error) && error < 0.15, "global camera positions");
    }
    GlobalReconstructionOptions filter_opt = reconstruction_opt;
    filter_opt.position_solver = [&](GlobalPositionProblem& problem, const GlobalPositionOptions&) {
        for (uint32_t i = 0; i < nimages; i++)
            problem.cameras[i] = mul(transpose(poses[i].R), poses[i].t) * -1.0;
        problem.cameras[1].x += 0.2;
        for (size_t t = 0; t < tracks.size(); t++)
            problem.points[t] = truth_points[tracks.observations[tracks.offsets[t]].point2D_idx];
        GlobalPositionResult result;
        result.final_cost = 1;
        result.converged = true;
        return result;
    };
    const std::vector<uint32_t> groups{1, 2, 1, 1};
    const std::map<uint32_t, Camera> camera_groups{{1, cameras[0]}, {2, cameras[1]}};
    const auto calibrated = buildGlobalReconstructions(db, features, groups, camera_groups,
                                                        {1, 2}, filter_opt, stats);
    const auto uncalibrated = buildGlobalReconstructions(db, features, groups, camera_groups,
                                                          {1}, filter_opt, stats);
    auto observations_in_image = [](const std::vector<Reconstruction>& result, uint32_t image) {
        size_t count = 0;
        for (const auto& model : result)
            for (const auto& point : model.points3D)
                for (const auto& observation : point.second.track)
                    count += observation.image_id == image;
        return count;
    };
    const size_t strict = observations_in_image(calibrated, 1);
    const size_t relaxed = observations_in_image(uncalibrated, 1);
    check(!calibrated.empty() && !uncalibrated.empty() && strict < relaxed && relaxed > 50,
          "unknown focal receives relaxed angular filtering");
    GlobalReconstructionOptions stalled_opt = filter_opt;
    stalled_opt.position_solver = [](GlobalPositionProblem&, const GlobalPositionOptions&) {
        GlobalPositionResult result;
        result.final_cost = 1;
        return result;
    };
    check(buildGlobalReconstructions(db, features, groups, camera_groups, {1, 2},
                                     stalled_opt, stats).empty(),
          "nonconverged positioning with no progress is rejected");
    check(checkFixedRigReconstruction(cameras[0], options, false),
          "global reconstruction preserves fixed rig extrinsics and scale");
    check(checkRotationPrior(cameras[0], options), "rotation prior enters global averaging");
    check(checkPositionPrior(db, features, poses, cameras, options, false),
          "metric position prior enters global positioning");
    check(checkUnknownFocal(options), "global reconstruction calibrates unknown focal");
    if (gpu)
        check(checkFixedRigReconstruction(cameras[0], options, true),
              "Vulkan global reconstruction preserves fixed rig extrinsics and scale");
    if (gpu)
        check(checkPositionPrior(db, features, poses, cameras, options, true),
              "Vulkan metric position prior enters global positioning");
    MatchesDatabase separate = db;
    separate.images.insert(separate.images.end(), db.images.begin(), db.images.end());
    for (const TwoViewMatches& source : db.pairs) {
        TwoViewMatches pair = source;
        pair.image1 += nimages;
        pair.image2 += nimages;
        separate.pairs.push_back(std::move(pair));
    }
    std::vector<FeatureSet> separate_features = features;
    separate_features.insert(separate_features.end(), features.begin(), features.end());
    const auto parts = buildGlobalReconstructions(separate, separate_features,
        std::vector<uint32_t>(2 * nimages, 1), {{1, cameras[0]}}, {1}, reconstruction_opt, stats);
    check(parts.size() == 2 && parts[0].numRegistered() == nimages &&
          parts[1].numRegistered() == nimages, "separate global components");
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testGlobalFrontend); }
