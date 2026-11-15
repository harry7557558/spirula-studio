// RANSAC search, scoring parity, and two-view camera coverage; --bench times verification.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>
#include <random>

#include "sfm/core/Camera.h"
#include "sfm/core/Matches.h"
#include "sfm/geometry/TwoView.h"

using namespace sfm;

static int failures = 0;

static void check(bool ok, const char* name) {
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

template <class Res>
static bool scoringParity(const Res& residual, int n) {
    const double threshold = 1.0;
    std::vector<char> expected(n), mask(n, 7);
    int expected_count = 0;
    double expected_score = 0;
    for (int i = 0; i < n; i++) {
        const double r = residual(0, i);
        expected[i] = r < threshold;
        expected_count += expected[i];
        expected_score += expected[i] ? r : threshold;
    }
    for (int need : {0, expected_count, std::min(n, expected_count + 1), n}) {
        int count;
        const double score = scoreModel(0, n, threshold, residual, mask, count, need);
        if (count < 0) {
            if (expected_count >= need) return false;
        } else if (count != expected_count || score != expected_score || mask != expected) {
            return false;
        }
    }
    return true;
}

static void checkTransferScoring() {
    std::mt19937 rng(502);
    std::normal_distribution<double> random(0, 1);
    bool parity = true;
    int inliers = 0, outliers = 0;
    for (int trial = 0; trial < 10000; ++trial) {
        Mat3 h = mat3Identity();
        for (double& v : h) v += 0.2 * random(rng);
        Mat3 inverse = inverse3(h);
        const Vec3 a = Vec3{random(rng), random(rng), random(rng)}.normalized();
        const double noise = trial % 3 == 0 ? 0 : trial % 3 == 1 ? 1e-4 : 1;
        Vec3 b = (mul(h, a) + Vec3{random(rng), random(rng), random(rng)} * noise).normalized();
        if (trial % 5 == 0) b = b * -1;
        if (trial % 7 == 0) for (double& v : inverse) v = -v;
        if (trial % 13 == 0) h = {};
        if (trial % 17 == 0) inverse[0] = std::numeric_limits<double>::quiet_NaN();
        const Vec3 hb = mul(h, a), ha = mul(inverse, b);
        const double d1 = hb.dot(b), d2 = ha.dot(a);
        const Vec3 c1 = hb.cross(b), c2 = ha.cross(a);
        const double reference = d1 <= 0 || d2 <= 0 ? 1e30 :
            c1.dot(c1) / (d1 * d1) + c2.dot(c2) / (d2 * d2);
        const double full = angularTransferSq(h, inverse, a, b);
        parity &= full == reference || (std::isnan(full) && std::isnan(reference));
        for (double threshold : {0.0, 1e-12, 9e-6, 1.0, 1e30, 1e31,
                                 std::numeric_limits<double>::infinity()}) {
            const double clipped = angularTransferSq(h, inverse, a, b, threshold);
            const bool expected = reference < threshold, actual = clipped < threshold;
            parity &= expected == actual;
            parity &= (expected ? reference : threshold) == (actual ? clipped : threshold);
            (actual ? inliers : outliers)++;
        }
    }
    check(parity && inliers > 0 && outliers > 0,
          "clipped angular transfer preserves MSAC, backward rays and nonfinite rejection");
}

static void checkRansac() {
    std::mt19937 rng(734);
    std::uniform_real_distribution<double> random(0, 2);
    bool parity = true;
    for (int n : {0, 1, 7, 31, 32, 33, 1000}) {
        std::vector<double> residuals(n);
        for (double& r : residuals) r = random(rng);
        if (n > 3) {
            residuals[0] = 1;
            residuals[1] = std::numeric_limits<double>::quiet_NaN();
            residuals[2] = std::numeric_limits<double>::infinity();
        }
        parity &= scoringParity([&](int, int i) { return residuals[i]; }, n);
    }
    check(parity, "scoring matches scalar MSAC exactly, including ties, tails and nonfinite residuals");

    struct PreparedResidual {
        mutable int prepared = -1, calls = 0;
        void prepare(const int& model) const { prepared = model; calls++; }
        double operator()(int model, int i) const { return prepared == model && i < model ? 0 : 2; }
    } residual;
    std::vector<char> mask;
    for (int model : {15, 2, 30}) {
        int count;
        scoreModel(model, 40, 1, residual, mask, count);
        parity &= count == model && mask.size() == 40;
    }
    check(parity && residual.calls == 3, "model preparation refreshes once per score and sizes fresh masks");

    bool nullspaces = true;
    for (int rows : {17, 128, 1000}) {
        for (double scale : {1e-5, 1.0, 1e5}) {
            std::vector<double> matrix(rows * 9);
            for (double& value : matrix) value = (random(rng) - 1) * scale;
            const auto expected = nullspaceVector(matrix, rows, 9);
            const auto actual = nullVectors9(matrix, rows, 1);
            nullspaces &= actual.size() == 1 && std::equal(expected.begin(), expected.end(), actual[0].begin());
        }
    }
    check(nullspaces, "streamed symmetric normal matrix matches the general solver exactly");

    RansacOptions options;
    options.min_num_trials = options.max_num_trials = 1;
    options.lo_iters = 0;
    options.max_error = 1;
    const auto fit_tail = [](const std::vector<int>& sample) {
        for (int i : sample) if (i < 400) return std::vector<int>{};
        return std::vector<int>{1};
    };
    const auto tail_residual = [](int, int i) { return i >= 400 ? 0.0 : 2.0; };
    auto report = loransac<int>(1000, 2, fit_tail, fit_tail, tail_residual, options);
    check(report.success && report.num_inliers == 600, "unordered inputs sample the full population");

    options.min_num_trials = 5;
    options.max_num_trials = 125;
    const auto fit = [](const std::vector<int>&) { return std::vector<int>{0}; };
    const auto sparse_residual = [](int, int i) { return i < 10 ? 0.0 : 2.0; };
    report = loransac<int>(1000, 7, fit, fit, sparse_residual, options);
    check(report.trials == 125, "tiny inlier ratio does not overflow the adaptive trial bound");
    report = loransac<int>(1000, 7, fit, fit, [](int, int) { return 0.0; }, options);
    check(report.trials == 5, "all-inlier model respects minimum trials");
    const auto empty = [](const std::vector<int>&) { return std::vector<int>{}; };
    report = loransac<int>(1000, 7, empty, empty, sparse_residual, options);
    check(!report.success && report.trials == 125, "empty solvers exhaust the budget without reading a mask");
    check(TwoViewOptions{}.ransac.max_num_trials == RansacOptions{}.max_num_trials,
          "two-view verification retains the full default search budget");

    std::vector<FeatureMatch> matches{{0, 0}, {1, 1}, {2, 2}};
    const std::vector<float> distances{3, 1, 2};
    keepClosest(matches, distances, 0);
    bool order = matches[0].idx1 == 0 && matches[1].idx1 == 1;
    keepClosest(matches, distances, 3);
    order &= matches[0].idx1 == 0 && matches[1].idx1 == 1;
    keepClosest(matches, distances, 2);
    check(order && matches.size() == 2 && matches[0].idx1 == 1 && matches[1].idx1 == 2,
          "match capping leaves uncapped input order intact");

    const std::vector<Vec2> a{{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    const std::vector<Vec2> b{{10, 20}, {110, 20}, {110, 120}, {10, 120}};
    TwoViewOptions tv;
    tv.min_num_inliers = 4;
    auto geometry = estimateTwoView(a, b, tv);
    check(geometry.config == TwoViewConfig::PlanarOrPanoramic && geometry.num_inliers == 4,
          "homography survives an unavailable fundamental fit");
    std::vector<Vec3> ba, bb;
    for (size_t i = 0; i < a.size(); i++) {
        ba.push_back(Vec3{a[i].x, a[i].y, 500}.normalized());
        bb.push_back(Vec3{b[i].x, b[i].y, 500}.normalized());
    }
    tv.ransac.max_error = 0.001;
    geometry = estimateTwoViewBearing(ba, bb, tv);
    check(geometry.config == TwoViewConfig::PlanarOrPanoramic && geometry.num_inliers == 4,
          "bearing homography survives an unavailable epipolar fit");
}

struct Scene {
    std::vector<Vec2> p1, p2;
    std::vector<Vec3> b1, b2;
    std::vector<char> truth;
};

static Scene makeScene(CamModel model, int n, double ratio, bool planar, unsigned seed) {
    const Camera camera = Camera::defaultFor(0, 4096, model == CamModel::Equirect ? 2048 : 4096,
                                            model == CamModel::Pinhole ? 1800 : 900, model);
    const Mat3 rotation = angleAxisToRotation({0.05, 0.15, -0.03});
    const Vec3 translation = planar ? Vec3{} : Vec3{0.8, 0.1, 0.2};
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> u(-1, 1), depth(4, 12);
    std::normal_distribution<double> noise(0, 0.25);
    Scene scene;
    for (int i = 0; i < n; i++) {
        const auto point = [&] {
            Vec3 ray{u(rng), u(rng), model == CamModel::Pinhole ? 1.5 : u(rng)};
            if (model == CamModel::OpenCVFisheye) ray.z = std::max(-0.15, ray.z);
            return ray.normalized() * depth(rng);
        };
        const Vec3 x = point();
        Vec2 a = camera.project(x);
        Vec2 b = camera.project(i < n * ratio ? mul(rotation, x) + translation : point());
        a.x += noise(rng); a.y += noise(rng);
        b.x += noise(rng); b.y += noise(rng);
        scene.p1.push_back(a); scene.p2.push_back(b);
        scene.b1.push_back(camera.bearing(a)); scene.b2.push_back(camera.bearing(b));
        scene.truth.push_back(i < n * ratio);
    }
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    Scene shuffled;
    for (int i : order) {
        shuffled.p1.push_back(scene.p1[i]); shuffled.p2.push_back(scene.p2[i]);
        shuffled.b1.push_back(scene.b1[i]); shuffled.b2.push_back(scene.b2[i]);
        shuffled.truth.push_back(scene.truth[i]);
    }
    return shuffled;
}

static void checkCameras(bool bench) {
    for (CamModel model : {CamModel::Pinhole, CamModel::OpenCVFisheye, CamModel::Equirect}) {
        for (int scenario = 0; scenario < (bench ? 4 : 2); scenario++) {
            const double ratio = scenario == 2 ? 0.25 : scenario == 3 ? 0 : 0.65;
            const int n = bench ? 3000 : 400;
            const Scene scene = makeScene(model, n, ratio, scenario == 1, 418 + scenario);
            TwoViewOptions options;
            if (model != CamModel::Pinhole)
                options.ransac.max_error /= model == CamModel::Equirect ? 4096 / (2 * M_PI) : 900;
            double elapsed = 0;
            TwoViewGeometry geometry;
            const int repeats = bench ? 5 : 1;
            for (int repeat = 0; repeat < repeats; repeat++) {
                const auto start = std::chrono::steady_clock::now();
                geometry = model == CamModel::Pinhole ? estimateTwoView(scene.p1, scene.p2, options)
                                                     : estimateTwoViewBearing(scene.b1, scene.b2, options);
                elapsed += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            }
            int correct = 0, incorrect = 0;
            unsigned long long hash = 0;
            for (size_t i = 0; i < geometry.inlier_mask.size(); i++) {
                if (geometry.inlier_mask[i]) (scene.truth[i] ? correct : incorrect)++;
                hash = hash * 131 + geometry.inlier_mask[i];
            }
            std::printf("camera %d scenario %d: %.3f ms, true %d false %d, config %d, mask %llu\n",
                        (int)model, scenario, elapsed / repeats, correct, incorrect, (int)geometry.config, hash);
            if (!bench) check(correct >= n * ratio * 0.95 && incorrect <= 5 &&
                              geometry.config == (scenario == 1 ? TwoViewConfig::PlanarOrPanoramic
                                                                : TwoViewConfig::Uncalibrated),
                              "shuffled camera correspondences retain geometry and inliers");
        }
    }
}

static void checkDifficultPair() {
    const Scene scene = makeScene(CamModel::Pinhole, 3000, 0.25, false, 420);
    TwoViewOptions options;
    options.estimate_homography = false;
    const auto geometry = estimateTwoView(scene.p1, scene.p2, options);
    int correct = 0, incorrect = 0;
    for (size_t i = 0; i < geometry.inlier_mask.size(); i++)
        if (geometry.inlier_mask[i]) (scene.truth[i] ? correct : incorrect)++;
    std::printf("high-outlier pair: true %d/750, false %d\n", correct, incorrect);
    check(correct >= 720 && incorrect <= 20, "shuffled high-outlier pair retains recall");
}

int main(int argc, char** argv) {
    const bool bench = argc > 1 && std::strcmp(argv[1], "--bench") == 0;
    if (!bench) {
        checkTransferScoring();
        checkRansac();
        checkDifficultPair();
    }
    checkCameras(bench);
    return failures ? 1 : 0;
}
