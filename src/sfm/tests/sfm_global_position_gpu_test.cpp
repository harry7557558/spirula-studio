#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "sfm/map/GlobalPositionGpu.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static bool checkSingleStep(bool metric) {
    GlobalPositionProblem p;
    p.cameras = {{0, 0, 0}, {0.99, 0.03, 0.01}};
    p.points = {{0.24, 0.08, 1.04}};
    p.observations = {{0, 0, Vec3{0.2, 0.1, 1}.normalized(), 1},
                      {1, 0, Vec3{-0.8, 0.1, 1}.normalized(), 0.7}};
    p.log_scales = {0, -0.2};
    if (metric) {
        p.metric_scale = true;
        p.observations.push_back({0, 0, Vec3{-0.1, 0.1, 1}.normalized(), 0.9, {0.3, 0, 0}});
        p.log_scales.push_back(-0.1);
    }
    const int dim = metric ? 9 : 7;
    GlobalPositionOptions opt;
    opt.max_iterations = 1;
    opt.cg_iterations = 100;
    opt.cg_tolerance = 1e-7;
    opt.initial_damping = 0.5;
    opt.huber = 10;
    double H[9][9] = {}, b[9] = {};
    for (size_t i = 0; i < p.observations.size(); i++) {
        const auto& o = p.observations[i];
        const Vec3 d = p.points[0] - p.cameras[o.camera] - o.camera_offset;
        const double s = std::exp(p.log_scales[i]);
        const Vec3 r = o.ray - d * s;
        const double delta[3] = {d.x, d.y, d.z};
        const double residual[3] = {r.x, r.y, r.z};
        for (int axis = 0; axis < 3; axis++) {
            double J[9] = {};
            if (o.camera) J[axis] = s;
            J[3 + axis] = -s;
            if (metric || i) J[metric ? 6 + i : 5 + i] = -s * delta[axis];
            for (int a = 0; a < dim; a++) {
                b[a] -= o.weight * J[a] * residual[axis];
                for (int c = 0; c < dim; c++) H[a][c] += o.weight * J[a] * J[c];
            }
        }
    }
    for (int i = 0; i < dim; i++) H[i][i] += opt.initial_damping * std::max(H[i][i], 1e-8);
    for (int i = 0; i < dim; i++) {
        for (int j = i + 1; j < dim; j++) {
            const double factor = H[j][i] / H[i][i];
            for (int k = i; k < dim; k++) H[j][k] -= factor * H[i][k];
            b[j] -= factor * b[i];
        }
    }
    double step[9];
    for (int i = dim - 1; i >= 0; i--) {
        step[i] = b[i];
        for (int j = i + 1; j < dim; j++) step[i] -= H[i][j] * step[j];
        step[i] /= H[i][i];
    }
    const auto before = p;
    const auto result = solveGlobalPositionGpu(p, opt);
    double actual[9] = {p.cameras[1].x - before.cameras[1].x,
                              p.cameras[1].y - before.cameras[1].y,
                              p.cameras[1].z - before.cameras[1].z,
                              p.points[0].x - before.points[0].x,
                              p.points[0].y - before.points[0].y,
                              p.points[0].z - before.points[0].z};
    for (size_t i = metric ? 0 : 1; i < p.log_scales.size(); i++)
        actual[metric ? 6 + i : 5 + i] = p.log_scales[i] - before.log_scales[i];
    double error = 0;
    for (int i = 0; i < dim; i++) error = std::max(error, std::abs(actual[i] - step[i]));
    std::printf("Schur step versus full normal equations%s: %.3g %s\n", metric ? " with rig" : "", error,
                result.accepted && error < 2e-5 ? "PASS" : "FAIL");
    return result.accepted && error < 2e-5;
}

static bool checkRigPositioning() {
    const std::vector<Vec3> cameras{{0, 0, 0}, {0.7, 0.2, 0.1}, {1.3, -0.1, 0.2}};
    const std::vector<Vec3> offsets{{0.3, 0, 0}, {0.28, 0.1, 0.04}, {0.23, -0.1, 0.16}};
    GlobalPositionProblem input;
    input.metric_scale = true;
    for (size_t i = 0; i < cameras.size(); i++)
        input.cameras.push_back(cameras[i] + (i ? Vec3{0.02, -0.01, 0.015} : Vec3{}));
    std::mt19937 rng(43);
    std::uniform_real_distribution<double> xy(-1, 1), depth(2, 4);
    for (uint32_t j = 0; j < 40; j++) {
        const Vec3 point{xy(rng), xy(rng), depth(rng)};
        input.points.push_back(point + Vec3{0.01, -0.02, 0.03});
        for (uint32_t i = 0; i < cameras.size(); i++)
            for (int sensor = 0; sensor < 2; sensor++) {
                const Vec3 offset = sensor ? offsets[i] : Vec3{};
                const Vec3 ray = (point - cameras[i] - offset).normalized();
                input.observations.push_back({i, j, ray, 1, offset});
                input.log_scales.push_back(-std::log((input.points[j] - input.cameras[i] - offset).norm()));
            }
    }
    GlobalPositionProblem cpu = input, gpu = input;
    GlobalPositionOptions opt;
    opt.max_iterations = 100;
    opt.cg_iterations = 150;
    const auto ref = solveGlobalPosition(cpu, opt);
    const auto actual = solveGlobalPositionGpu(gpu, opt);
    double error = 0;
    for (size_t i = 0; i < cameras.size(); i++)
        error = std::max(error, (gpu.cameras[i] - cameras[i]).norm());
    const bool ok = actual.final_cost < 1e-7 && error < 0.03 &&
                    std::abs(gpu.log_scales[0] - input.log_scales[0]) > 1e-5;
    std::printf("rig positioning CPU %.6g, GPU %.6g, camera error %.4g: %s\n",
                ref.final_cost, actual.final_cost, error, ok ? "PASS" : "FAIL");
    return ok;
}

static bool checkPositionPriorsGpu() {
    const std::vector<Vec3> truth = {{4, -2, 1}, {4.9, -1.8, 1.1}, {4.2, -1, 1.2},
                                     {5, -1.1, 1.3}};
    GlobalPositionProblem input;
    input.cameras.resize(truth.size());
    input.metric_scale = true;
    std::mt19937 rng(113);
    std::uniform_real_distribution<double> xy(2, 6), depth(5, 9);
    for (uint32_t j = 0; j < 60; j++) {
        const Vec3 point{xy(rng), xy(rng), depth(rng)};
        input.points.push_back(point);
        for (uint32_t i = 0; i < truth.size(); i++)
            input.observations.push_back({i, j, (point - truth[i]).normalized(), 1});
    }
    for (uint32_t i = 0; i < truth.size(); i++)
        input.position_priors.push_back({i, {}, mat3Identity(), truth[i], {0.02, 0.02, 0.02}, 3.0});
    GlobalPositionPrior outlier = input.position_priors[2];
    outlier.b = outlier.b + Vec3{10, -8, 6};
    outlier.cauchy = 1.0;
    input.position_priors.push_back(outlier);
    initializeGlobalPosition(input, 41);
    GlobalPositionProblem cpu = input, gpu = input;
    GlobalPositionOptions opt;
    opt.max_iterations = 200;
    opt.cg_iterations = 200;
    const auto ref = solveGlobalPosition(cpu, opt);
    const auto actual = solveGlobalPositionGpu(gpu, opt);
    double error = 0;
    for (size_t i = 0; i < truth.size(); i++)
        error = std::max(error, (gpu.cameras[i] - truth[i]).norm());
    const bool ok = actual.accepted > 0 && actual.final_cost < actual.initial_cost * 0.1 &&
                    actual.final_cost < ref.final_cost * 1.1 + 1e-8 && error < 3e-3 &&
                    (gpu.cameras[0] - input.cameras[0]).norm() > 1;
    std::printf("GPU metric position priors CPU %.6g, GPU %.6g, camera error %.4g: %s\n",
                ref.final_cost, actual.final_cost, error, ok ? "PASS" : "FAIL");
    return ok;
}

static bool checkGpuReduction() {
    GlobalPositionProblem input;
    input.cameras = {{0, 0, 0}, {1, -1, 0}};
    for (uint32_t j = 0; j < 1500; j++) {
        input.points.push_back({1, 0, 0});
        input.observations.push_back({0, j, {1, 0, 0}, 1});
        input.observations.push_back({1, j, {0, 1, 0}, 1});
        input.log_scales.push_back(0);
        input.log_scales.push_back(0);
    }
    GlobalPositionOptions opt;
    opt.max_iterations = 1;
    opt.cg_iterations = 20;
    GlobalPositionProblem zero = input;
    const auto stationary = solveGlobalPositionGpu(zero, opt);
    input.points.back().x = 1.05;
    const auto tail = solveGlobalPositionGpu(input, opt);
    const bool ok = stationary.converged && stationary.accepted == 0 && stationary.final_cost == 0 &&
                    tail.accepted == 1 && tail.final_cost < tail.initial_cost;
    std::printf("GPU reduction zero residual and final partial group: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

static int testGlobalPositionGpu(int, char**) {
    const std::vector<Vec3> truth = {{0, 0, 0}, {1, 0.1, 0.05}, {0.1, 1, 0.1},
                                     {0.8, 0.9, 0.2}, {0.4, -0.3, 0.3}};
    GlobalPositionProblem input;
    input.cameras.resize(truth.size());
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> xy(-2, 2), depth(3, 8);
    std::normal_distribution<double> noise(0, 0.0005);
    for (uint32_t j = 0; j < 80; j++) {
        const Vec3 point{xy(rng), xy(rng), depth(rng)};
        input.points.push_back(point);
        for (uint32_t i = 0; i < truth.size(); i++) {
            if (j >= 40 && i >= 3) continue;
            Vec3 ray = (point - truth[i]).normalized();
            ray = (ray + Vec3{noise(rng), noise(rng), noise(rng)}).normalized();
            if ((j * truth.size() + i) % 23 == 0)
                ray = Vec3{xy(rng), xy(rng), depth(rng)}.normalized();
            input.observations.push_back({i, j, ray, 1});
        }
    }
    initializeGlobalPosition(input, 17);
    GlobalPositionProblem cpu = input, gpu = input;
    GlobalPositionOptions opt;
    opt.max_iterations = 150;
    opt.huber = 0.03;
    const auto ref = solveGlobalPosition(cpu, opt);
    const auto actual = solveGlobalPositionGpu(gpu, opt);
    double numer = 0, denom = 0;
    for (size_t i = 1; i < truth.size(); i++) {
        numer += gpu.cameras[i].dot(truth[i]);
        denom += gpu.cameras[i].dot(gpu.cameras[i]);
    }
    const double scale = numer / denom;
    double error = 0;
    for (size_t i = 1; i < truth.size(); i++)
        error = std::max(error, (gpu.cameras[i] * scale - truth[i]).norm());
    bool invalid_weight = false, invalid_option = false;
    GlobalPositionProblem invalid = input;
    invalid.observations[0].weight = std::numeric_limits<double>::denorm_min();
    try { solveGlobalPositionGpu(invalid, opt); }
    catch (const std::invalid_argument&) { invalid_weight = true; }
    GlobalPositionOptions invalid_opt = opt;
    invalid_opt.huber = std::numeric_limits<double>::infinity();
    try { solveGlobalPositionGpu(invalid, invalid_opt); }
    catch (const std::invalid_argument&) { invalid_option = true; }
    const bool gauge = (gpu.cameras[0] - input.cameras[0]).norm() == 0 &&
                       gpu.log_scales[0] == input.log_scales[0];
    const bool ok = std::isfinite(actual.final_cost) && actual.accepted > 0 &&
                    actual.final_cost < actual.initial_cost &&
                    actual.final_cost < ref.final_cost * 1.05 && error < 0.15 &&
                    gauge && invalid_weight && invalid_option;
    std::printf("global position CPU %.6g, GPU %.6g, camera error %.5g, iterations %d: %s\n",
                ref.final_cost, actual.final_cost, error, actual.iterations, ok ? "PASS" : "FAIL");
    if (!ok) std::printf("gauge %d, invalid weight %d, invalid option %d\n",
                         gauge, invalid_weight, invalid_option);
    GlobalPositionProblem long_input;
    const uint32_t camera_count = 66, point_count = 10;
    std::vector<Vec3> long_cameras;
    for (uint32_t i = 0; i < camera_count; i++) {
        const double a = 6.283185307179586 * i / camera_count;
        long_cameras.push_back({std::cos(a), std::sin(a), 0.3 * std::sin(2 * a)});
    }
    std::vector<Vec3> long_points;
    for (uint32_t i = 0; i < point_count; i++)
        long_points.push_back({xy(rng), xy(rng), depth(rng)});
    const Vec3 origin = long_cameras[0];
    const double factor = 1.0 / (long_points[0] - origin).norm();
    for (uint32_t i = 0; i < camera_count; i++)
        long_input.cameras.push_back((long_cameras[i] - origin) * factor +
            (i ? Vec3{0.001, -0.002, 0.001} : Vec3{}));
    for (uint32_t j = 0; j < point_count; j++)
        long_input.points.push_back((long_points[j] - origin) * factor + Vec3{-0.002, 0.001, 0.002});
    for (uint32_t j = 0; j < point_count; j++)
        for (uint32_t i = 0; i < camera_count; i++) {
            long_input.observations.push_back({i, j, (long_points[j] - long_cameras[i]).normalized(), 1});
            long_input.log_scales.push_back(-std::log((long_points[j] - long_cameras[i]).norm() * factor));
        }
    GlobalPositionProblem long_cpu = long_input, long_gpu = long_input;
    GlobalPositionOptions long_opt;
    long_opt.max_iterations = 40;
    long_opt.cg_iterations = 80;
    const auto long_ref = solveGlobalPosition(long_cpu, long_opt);
    const auto long_actual = solveGlobalPositionGpu(long_gpu, long_opt);
    const bool long_ok = std::isfinite(long_actual.final_cost) &&
        long_actual.final_cost < long_actual.initial_cost * 0.1 &&
        long_actual.final_cost < std::max(1e-9, long_ref.final_cost * 2);
    std::printf("long tracks CPU %.6g, GPU %.6g: %s\n", long_ref.final_cost,
                long_actual.final_cost, long_ok ? "PASS" : "FAIL");
    return checkSingleStep(false) && checkSingleStep(true) && checkRigPositioning() &&
           checkPositionPriorsGpu() && checkGpuReduction() && ok && long_ok ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testGlobalPositionGpu); }
