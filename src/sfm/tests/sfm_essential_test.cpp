#include <chrono>
#include <cstdio>
#include <random>

#include "sfm/core/Pose.h"
#include "sfm/geometry/EssentialFivePoint.h"
#include "sfm/geometry/TwoView.h"

using namespace sfm;

int main() {
    std::mt19937 rng(351);
    std::normal_distribution<double> random(0, 1);
    const std::vector<int> sample{0,1,2,3,4};
    int recovered = 0, returned = 0;
    double worst = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int trial = 0; trial < 1500; ++trial) {
        const Mat3 rotation = angleAxisToRotation(Vec3{random(rng), random(rng), random(rng)} * 0.2);
        const Vec3 translation = Vec3{random(rng), random(rng), random(rng)}.normalized();
        const Mat3 skew = {0,-translation.z,translation.y,translation.z,0,-translation.x,
                           -translation.y,translation.x,0};
        Mat3 truth = mul(skew, rotation);
        for (double& v : truth) v /= std::sqrt(2.0);
        std::vector<Vec3> b1, b2;
        for (int i = 0; i < 5; ++i) {
            Vec3 x{random(rng), random(rng), trial % 3 ? 4 + std::fabs(random(rng)) : random(rng)};
            if (trial % 3 == 0) x = x.normalized() * (4 + std::fabs(random(rng)));
            b1.push_back(x.normalized());
            b2.push_back((mul(rotation, x) + translation).normalized());
        }
        const auto models = estimateEssential5Bearing(b1, b2, sample);
        returned += (int)models.size();
        double best = 2;
        for (const Mat3& e : models) {
            double plus = 0, minus = 0;
            for (int i = 0; i < 9; ++i) {
                plus += std::pow(e[i] - truth[i], 2);
                minus += std::pow(e[i] + truth[i], 2);
            }
            best = std::min(best, std::sqrt(std::min(plus, minus)));
        }
        recovered += best < 1e-4;
        worst = std::max(worst, best);
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    printf("five-point: recovered %d/1500, models %d, worst %.5g, %.3f s\n", recovered, returned, worst, seconds);
    std::vector<Vec3> repeated(5, Vec3{0,0,1});
    const auto degenerate = estimateEssential5Bearing(repeated, repeated, sample);
    bool finite = true;
    for (const Mat3& e : degenerate) for (double v : e) finite = finite && std::isfinite(v);
    double polynomial[11] = {-120, 274, -225, 85, -15, 1};
    double roots[10];
    const int count = realPolynomialRoots(polynomial, 5, roots);
    std::sort(roots, roots + count);
    bool root_ok = count == 5;
    for (int i = 0; i < count; ++i) root_ok = root_ok && std::fabs(roots[i] - i - 1) < 1e-9;
    printf("%s: real polynomial roots, %s: degenerate finite output\n",
           root_ok ? "PASS" : "FAIL", finite ? "PASS" : "FAIL");
    bool robust_ok = true;
    for (int outliers : {0, 100, 300}) {
        const Mat3 rotation = angleAxisToRotation({0.05,0.12,-0.03});
        const Vec3 translation = Vec3{1,0.2,0.1}.normalized();
        std::vector<Vec3> b1, b2;
        for (int i = 0; i < 200 + outliers; ++i) {
            const Vec3 x{random(rng),random(rng),4+std::fabs(random(rng))};
            const Vec3 perturbation{0.0002*random(rng),0.0002*random(rng),0};
            b1.push_back(x.normalized());
            b2.push_back(i < 200 ? ((mul(rotation,x)+translation).normalized()+perturbation).normalized()
                                : Vec3{random(rng),random(rng),4}.normalized());
        }
        TwoViewOptions options;
        options.calibrated_essential = true;
        options.recover_pose = true;
        options.estimate_homography = false;
        options.ransac.max_error = 0.001;
        const TwoViewGeometry fit = estimateTwoViewBearing(b1,b2,options);
        const Mat3 delta = mul(fit.pose.R,transpose(rotation));
        const double rotation_error = std::acos(std::clamp((delta[0]+delta[4]+delta[8]-1)*0.5,-1.0,1.0));
        const double translation_error = std::acos(std::clamp(fit.pose.t.normalized().dot(translation),-1.0,1.0));
        int correct = 0, incorrect = 0;
        for (size_t i = 0; i < fit.inlier_mask.size(); ++i)
            if (fit.inlier_mask[i]) (i < 200 ? correct : incorrect)++;
        const bool pass = fit.has_pose && correct >= 190 && incorrect < 15 &&
                          rotation_error < 0.01 && translation_error < 0.08;
        robust_ok &= pass;
        printf("%s: calibrated RANSAC outliers %d, true %d false %d, R %.5f t %.5f rad\n",
               pass ? "PASS" : "FAIL",outliers,correct,incorrect,rotation_error,translation_error);
    }
    bool planar_ok = true;
    for (int scene = 0; scene < 3; ++scene) {
        const Mat3 rotation = angleAxisToRotation({0.07, 0.15, -0.04});
        const Vec3 translation = scene == 0 ? Vec3{0.7, 0.1, 0.05} : Vec3{0, 0, 0};
        std::vector<Vec3> b1, b2;
        for (int i = 0; i < 150; ++i) {
            Vec3 x{random(rng), random(rng), scene == 0 ? 4 : 3 + std::fabs(random(rng))};
            if (scene == 2) x = Vec3{random(rng), random(rng), random(rng)}.normalized() * 4;
            b1.push_back(x.normalized());
            b2.push_back((mul(rotation, x) + translation).normalized());
        }
        TwoViewOptions options;
        options.calibrated_essential = true;
        options.recover_pose = true;
        options.ransac.max_error = 0.001;
        const auto fit = estimateTwoViewBearing(b1, b2, options);
        const bool pass = fit.config == TwoViewConfig::PlanarOrPanoramic &&
                          fit.num_inliers >= 145 && !fit.has_pose;
        planar_ok &= pass;
        printf("%s: calibrated degeneracy scene %d, inliers %d, %s\n",
               pass ? "PASS" : "FAIL", scene, fit.num_inliers, twoViewConfigName(fit.config));
    }
    return recovered >= 1490 && finite && root_ok && robust_ok && planar_ok ? 0 : 1;
}
