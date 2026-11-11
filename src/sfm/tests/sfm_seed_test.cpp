#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <thread>

#include "sfm/Pipeline.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/map/Mapper.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

struct SeedFound {};

struct SeedResult {
    uint32_t a = kNoImage, b = kNoImage;
    std::string text;
    double seconds = 0;
};

static SeedResult firstSeed(const MatchesDatabase& db, const std::vector<FeatureSet>& features,
                            MapperOptions options, PriorSource* priors = nullptr) {
    SeedResult result;
    options.verbose = true;
    RunContext context;
    context.set_log([&](slog::Tag, slog::Level, const std::string& line) {
        std::vector<std::string> fields;
        if (spirula::i18n::scan(spirula::i18n::msg::sfm::map_init_pair, line, fields)) {
            result.a = (uint32_t)std::stoul(fields[0]);
            result.b = (uint32_t)std::stoul(fields[1]);
            result.text = line;
            throw SeedFound{};
        }
    });
    Mapper mapper(db, features, options, db.camera_ids, nullptr, nullptr, priors);
    const auto start = std::chrono::steady_clock::now();
    try { mapper.run(); } catch (const SeedFound&) {}
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return result;
}

class InterruptPrior : public PriorSource {
public:
    std::atomic<bool>& stop;
    const std::thread::id caller = std::this_thread::get_id();
    bool worker;
    mutable std::atomic<bool> fired{false};
    InterruptPrior(std::atomic<bool>& flag, bool in_worker) : stop(flag), worker(in_worker) {}
    bool has(uint32_t) const override { return false; }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override {
        if ((std::this_thread::get_id() != caller) == worker) {
            fired = true;
            stop = true;
        }
        return false;
    }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    PosePriors factors(const std::vector<PosedImage>&) override { return {}; }
};

static int seedTest(int argc, char** argv) {
    if (argc >= 4 && std::string(argv[1]) == "--probe") {
        const auto db = readMatches(argv[2]);
        std::vector<FeatureSet> features(db.images.size());
        for (size_t i = 0; i < features.size(); ++i)
            features[i] = readFeatures((std::filesystem::path(argv[3]) /
                                       (db.images[i].name + ".bin")).string(), false);
        CameraSetup cameras;
        if (!loadCameraSetup(db, cameras)) return 1;
        MapperOptions options;
        options.initial_cameras = cameras.cameras;
        options.known_focal_cameras = cameras.focal_known;
        options.given_focal_cameras = cameras.focal_given;
        options.measured_focal_cameras = cameras.focal_measured;
        options.threads = argc > 4 ? std::stoi(argv[4]) : 0;
        const auto result = firstSeed(db, features, options);
        std::printf("seed search %.3f s: %s\n", result.seconds, result.text.c_str());
        return result.a == kNoImage;
    }

    int failures = 0;
    auto check = [&](bool ok, const char* label) {
        std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
        failures += !ok;
    };
    constexpr uint32_t images = 10, points = 180;
    const Camera camera = Camera::defaultFor(1, 1600, 1200, 1000);
    MatchesDatabase db;
    std::vector<FeatureSet> features(images);
    std::mt19937 rng(19);
    std::uniform_real_distribution<double> xy(-1.5, 1.5), depth(4, 8);
    std::normal_distribution<double> noise(0, 0.03);
    for (uint32_t i = 0; i < images; ++i) {
        db.images.push_back({"view" + std::to_string(i), points});
        features[i].width = camera.width;
        features[i].height = camera.height;
    }
    for (uint32_t p = 0; p < points; ++p) {
        const Vec3 x{xy(rng), xy(rng), depth(rng)};
        for (uint32_t i = 0; i < images; ++i) {
            const double baseline = i == images - 1 ? 2.5 : 0.005 * i;
            const Vec2 pixel = camera.project(x - Vec3{baseline, 0, 0});
            features[i].keypoints.push_back({(float)(pixel.x + noise(rng)),
                                             (float)(pixel.y + noise(rng)), 1, 0, 0});
        }
    }
    for (uint32_t i = 1; i < images; ++i) {
        TwoViewMatches pair;
        pair.image1 = 0;
        pair.image2 = i;
        pair.config = (int)TwoViewConfig::Uncalibrated;
        for (uint32_t p = 0; p < points; ++p) pair.matches.push_back({p, p});
        db.pairs.push_back(std::move(pair));
    }
    if (argc == 3 && std::string(argv[1]) == "--write-fixture") {
        const std::filesystem::path path(argv[2]);
        std::filesystem::create_directories(path / "features");
        db.camera_ids.assign(images, 1);
        db.cameras = {camera};
        db.focal_prior = {1};
        writeMatches((path / "matches.bin").string(), db);
        for (uint32_t i = 0; i < images; ++i) {
            features[i].descriptors.resize((size_t)points * features[i].dim);
            writeFeatures((path / "features" / (db.images[i].name + ".bin")).string(), features[i]);
        }
        return 0;
    }
    MapperOptions options;
    options.initial_cameras = {{1, camera}};
    options.given_focal_cameras = options.known_focal_cameras = {1};
    options.focal_trials = 0;
    options.ba_real = options.ba_real_coarse = "cpu";
    std::vector<Vec2> pixels1, pixels2;
    std::vector<Vec3> rays1, rays2;
    for (uint32_t p = 0; p < points; ++p) {
        const auto& a = features[0].keypoints[p];
        const auto& b = features.back().keypoints[p];
        pixels1.push_back({a.x, a.y});
        pixels2.push_back({b.x, b.y});
        rays1.push_back(camera.bearing(pixels1.back()));
        rays2.push_back(camera.bearing(pixels2.back()));
    }
    for (int mode : {0, 1, 2}) {
        TwoViewOptions geometry;
        geometry.K1 = geometry.K2 = camera.K();
        geometry.recover_pose = true;
        geometry.calibrated_essential = mode == 2;
        if (mode) geometry.ransac.max_error = camera.errRad(geometry.ransac.max_error);
        auto estimate = [&] {
            return mode ? estimateTwoViewBearing(rays1, rays2, geometry) :
                          estimateTwoView(pixels1, pixels2, geometry);
        };
        const auto full = estimate();
        geometry.estimate_homography = false;
        const auto deferred = estimate();
        check(full.has_pose && full.config == TwoViewConfig::Uncalibrated &&
              full.F == deferred.F && full.inlier_mask == deferred.inlier_mask &&
              full.pose.R == deferred.pose.R && (full.pose.t - deferred.pose.t).norm() == 0,
              mode == 0 ? "pixel pose is unchanged by deferred homography" :
              mode == 1 ? "bearing pose is unchanged by deferred homography" :
                          "five-point pose is unchanged by deferred homography");
    }
    for (int threads : {1, 4}) {
        options.threads = threads;
        const auto seed = firstSeed(db, features, options);
        check(seed.a == 0 && seed.b == images - 1,
              threads == 1 ? "serial search rejects dense low-parallax pairs" :
                             "parallel search preserves seed ordering");
        std::atomic<bool> stop{false};
        InterruptPrior prior(stop, threads > 1);
        bool cancelled = false;
        RunContext context;
        context.set_cancel(&stop);
        Mapper mapper(db, features, options, {}, nullptr, nullptr, &prior);
        const auto start = std::chrono::steady_clock::now();
        try { mapper.run(); } catch (const Cancelled&) { cancelled = true; }
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        check(cancelled && prior.fired && elapsed < 2,
              threads == 1 ? "serial seed RANSAC cancellation" :
                             "worker cancellation joins the pool and reaches the caller");
    }
    {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto path = std::filesystem::temp_directory_path() /
                          ("spirula_seed_parallel_" + std::to_string(nonce));
        if (!std::filesystem::create_directory(path)) return 1;
        options.verbose = false;
        options.max_init_trials = 1;
        bool complete = true;
        for (int threads : {1, 4}) {
            options.threads = threads;
            Mapper mapper(db, features, options);
            auto models = mapper.run();
            complete = complete && models.size() == 1 && models[0].numRegistered() == images;
            const auto output = path / std::to_string(threads);
            std::filesystem::create_directory(output);
            if (!models.empty()) models[0].writeBinary(output.string());
        }
        bool identical = complete;
        for (const char* file : {"cameras.bin", "images.bin", "points3D.bin"}) {
            std::ifstream a(path / "1" / file, std::ios::binary), b(path / "4" / file, std::ios::binary);
            identical = identical && a.good() && b.good() &&
                std::string(std::istreambuf_iterator<char>(a), {}) ==
                std::string(std::istreambuf_iterator<char>(b), {});
        }
        std::filesystem::remove_all(path);
        check(identical, "serial and parallel mapping produce identical cameras, tracks and points");
    }
    MatchesDatabase planar;
    planar.images = {db.images[0], db.images[1]};
    planar.pairs = {db.pairs[0]};
    std::vector<FeatureSet> plane(features.begin(), features.begin() + 2);
    for (uint32_t p = 0; p < points; ++p) {
        const Vec3 x{xy(rng), xy(rng), p < 160 ? 5 : depth(rng)};
        for (uint32_t i = 0; i < 2; ++i) {
            const Vec2 pixel = camera.project(x - Vec3{2.5 * i, 0, 0});
            plane[i].keypoints[p] = {(float)(pixel.x + noise(rng)),
                                     (float)(pixel.y + noise(rng)), 1, 0, 0};
        }
    }
    options.threads = 1;
    check(firstSeed(planar, plane, options).a == kNoImage,
          "planar pair mislabeled by verification cannot become a seed");
    options.seed_homography = false;
    check(firstSeed(planar, plane, options).a != kNoImage,
          "planar fixture reaches the deferred homography gate");

    RansacOptions ransac;
    std::atomic<bool> stop{false};
    RunContext context;
    context.set_cancel(&stop);
    ransac.check_interrupt = cancel::check;
    int fits = 0;
    auto fit = [&](const std::vector<int>&) {
        if (++fits == 5) stop = true;
        return std::vector<int>{0};
    };
    bool cancelled = false;
    try {
        loransac<int>(100, 2, fit, fit, [](int, int) { return 1e6; }, ransac);
    } catch (const Cancelled&) { cancelled = true; }
    check(cancelled && fits >= 5 && fits <= 32, "RANSAC polls cancellation during a long fit");
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, seedTest); }
