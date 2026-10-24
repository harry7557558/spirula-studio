#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "sfm/map/GlobalPosition.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static bool checkPositionPriors() {
    const std::vector<Vec3> cameras = {{5, -3, 2}, {6, -2.8, 2.1}, {5.1, -2, 2.2},
                                       {5.8, -2.1, 2.3}};
    GlobalPositionProblem p;
    p.cameras.resize(cameras.size());
    p.metric_scale = true;
    std::mt19937 rng(71);
    std::uniform_real_distribution<double> xy(3, 7), depth(6, 10);
    for (uint32_t j = 0; j < 60; j++) {
        const Vec3 point{xy(rng), xy(rng), depth(rng)};
        p.points.push_back(point);
        for (uint32_t i = 0; i < cameras.size(); i++)
            p.observations.push_back({i, j, (point - cameras[i]).normalized(), 1});
    }
    for (uint32_t i = 0; i < cameras.size(); i++)
        p.position_priors.push_back({i, {}, mat3Identity(), cameras[i], {0.02, 0.02, 0.02}, 3.0});
    GlobalPositionPrior outlier = p.position_priors[2];
    outlier.b = outlier.b + Vec3{10, -8, 6};
    outlier.cauchy = 1.0;
    p.position_priors.push_back(outlier);
    initializeGlobalPosition(p, 31);
    const Vec3 initial = p.cameras[0];
    GlobalPositionOptions opt;
    opt.max_iterations = 250;
    opt.cg_iterations = 200;
    const GlobalPositionResult result = solveGlobalPosition(p, opt);
    double error = 0;
    for (size_t i = 0; i < cameras.size(); i++)
        error = std::max(error, (p.cameras[i] - cameras[i]).norm());
    GlobalPositionProblem invalid = p;
    invalid.position_priors[0].sigma = {};
    bool rejected = false;
    try { solveGlobalPosition(invalid, opt); }
    catch (const std::invalid_argument&) { rejected = true; }
    const bool ok = result.accepted > 0 && result.final_cost < result.initial_cost * 0.1 && error < 1e-3 &&
                    (p.cameras[0] - initial).norm() > 1 && rejected;
    std::printf("metric position priors: %.4g -> %.4g, camera error %.4g, %s\n",
                result.initial_cost, result.final_cost, error, ok ? "PASS" : "FAIL");
    return ok;
}

static int testGlobalPosition(int, char**) {
    std::vector<Vec3> cameras = {{0, 0, 0}, {1, 0.1, 0.05}, {0.1, 1, 0.1},
                                 {0.8, 0.9, 0.2}, {0.4, -0.3, 0.3}};
    GlobalPositionProblem p;
    p.cameras.resize(cameras.size());
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> xy(-2, 2), depth(3, 8);
    for (uint32_t j = 0; j < 80; j++) {
        const Vec3 point{xy(rng), xy(rng), depth(rng)};
        p.points.push_back(point);
        for (uint32_t i = 0; i < cameras.size(); i++)
            p.observations.push_back({i, j, (point - cameras[i]).normalized(), 1});
    }
    const GlobalPositionProblem clean = p;
    initializeGlobalPosition(p);
    GlobalPositionOptions opt;
    opt.max_iterations = 300;
    opt.cg_iterations = 250;
    const GlobalPositionResult result = solveGlobalPosition(p, opt);
    double numer = 0, denom = 0;
    for (size_t i = 1; i < cameras.size(); i++) {
        numer += p.cameras[i].dot(cameras[i]);
        denom += p.cameras[i].dot(p.cameras[i]);
    }
    const double scale = numer / denom;
    double error = 0;
    for (size_t i = 1; i < cameras.size(); i++)
        error = std::max(error, (p.cameras[i] * scale - cameras[i]).norm());
    std::printf("global position: %.4g -> %.4g, %d accepted, camera error %.4g\n",
                result.initial_cost, result.final_cost, result.accepted, error);
    const bool clean_ok = std::isfinite(result.final_cost) && result.final_cost < 1e-5 && error < 0.02;
    p = clean;
    std::normal_distribution<double> noise(0, 0.0005);
    for (size_t i = 0; i < p.observations.size(); i++) {
        auto& o = p.observations[i];
        if (i % 23 == 0) o.ray = Vec3{xy(rng), xy(rng), depth(rng)}.normalized();
        else o.ray = (o.ray + Vec3{noise(rng), noise(rng), noise(rng)}).normalized();
    }
    initializeGlobalPosition(p, 17);
    opt.huber = 0.03;
    const GlobalPositionResult robust = solveGlobalPosition(p, opt);
    numer = denom = 0;
    for (size_t i = 1; i < cameras.size(); i++) {
        numer += p.cameras[i].dot(cameras[i]);
        denom += p.cameras[i].dot(p.cameras[i]);
    }
    const double robust_scale = numer / denom;
    error = 0;
    for (size_t i = 1; i < cameras.size(); i++)
        error = std::max(error, (p.cameras[i] * robust_scale - cameras[i]).norm());
    std::printf("noisy/outliers: %.4g -> %.4g, camera error %.4g\n",
                robust.initial_cost, robust.final_cost, error);
    const bool robust_ok = std::isfinite(robust.final_cost) && error < 0.15;
    auto rejected = [&](GlobalPositionProblem bad) {
        try { solveGlobalPosition(bad, opt); }
        catch (const std::invalid_argument&) { return true; }
        return false;
    };
    GlobalPositionProblem invalid = clean;
    initializeGlobalPosition(invalid);
    invalid.observations[0].ray = {2, 0, 0};
    const bool invalid_ray = rejected(invalid);
    invalid = clean;
    initializeGlobalPosition(invalid);
    invalid.cameras.push_back({0, 0, 0});
    const bool disconnected = rejected(invalid);
    std::atomic<bool> stop{true};
    cancel::set_token(&stop);
    bool cancelled = false;
    try { solveGlobalPosition(p, opt); }
    catch (const Cancelled&) { cancelled = true; }
    cancel::set_token(nullptr);
    std::printf("invalid ray: %s, disconnected: %s, cancellation: %s\n",
                invalid_ray ? "PASS" : "FAIL", disconnected ? "PASS" : "FAIL",
                cancelled ? "PASS" : "FAIL");
    return clean_ok && robust_ok && invalid_ray && disconnected && cancelled &&
                   checkPositionPriors() ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testGlobalPosition); }
