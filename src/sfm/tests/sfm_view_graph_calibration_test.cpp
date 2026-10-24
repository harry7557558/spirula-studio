#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

#include "sfm/core/Pose.h"
#include "sfm/map/ViewGraphCalibration.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static Mat3 fundamental(const Camera& a, const Camera& b, const Mat3& R, const Vec3& t) {
    const Mat3 cross{0, -t.z, t.y, t.z, 0, -t.x, -t.y, t.x, 0};
    return mul(mul(transpose(inverse3(b.K())), mul(cross, R)), inverse3(a.K()));
}

static int testViewGraphCalibration(int, char**) {
    Camera a = Camera::defaultFor(1, 1600, 1200, 800, CamModel::Pinhole);
    Camera b = Camera::defaultFor(2, 1600, 1200, 1200, CamModel::Pinhole);
    std::vector<FocalGraphEdge> edges;
    for (int i = 0; i < 18; i++) {
        const Mat3 R = angleAxisToRotation({0.05 * std::sin(i), 0.08 + 0.02 * i,
                                            -0.03 * std::cos(i * 0.7)});
        const Vec3 t{0.4 + 0.03 * i, 0.2 * std::sin(i * 0.4), 0.3 + 0.01 * i};
        if (i % 3 == 0) edges.push_back({1, 1, fundamental(a, a, R, t)});
        else if (i % 3 == 1) edges.push_back({2, 2, fundamental(b, b, R, t)});
        else edges.push_back({1, 2, fundamental(a, b, R, t)});
    }
    Camera guess_a = a, guess_b = b;
    guess_a.setFocal(950);
    guess_b.setFocal(1050);
    const std::map<uint32_t, Camera> guesses{{1, guess_a}, {2, guess_b}};
    focal_graph_detail::Constraint constraint;
    const bool decomposed = focal_graph_detail::decompose(edges.back().F, guess_a, guess_b, constraint);
    focal_graph_detail::Linearization analytic, left, right;
    bool jacobian = decomposed && focal_graph_detail::linearize(constraint, 0.1, -0.1, analytic);
    constexpr double eps = 1e-6;
    for (int axis = 0; axis < 2 && jacobian; axis++) {
        const double da = axis == 0 ? eps : 0, db = axis == 1 ? eps : 0;
        jacobian = focal_graph_detail::linearize(constraint, 0.1 - da, -0.1 - db, left) &&
                   focal_graph_detail::linearize(constraint, 0.1 + da, -0.1 + db, right);
        for (int k = 0; k < 2 && jacobian; k++) {
            const double numerical = (right.residual[k] - left.residual[k]) / (2 * eps);
            const double exact = axis == 0 ? analytic.ja[k] : analytic.jb[k];
            jacobian = std::abs(numerical - exact) < 1e-5 * std::max(1.0, std::abs(exact));
        }
    }
    const auto both = calibrateFocalGraph(guesses, {}, edges);
    std::printf("constraints: %zu, cost %.9g -> %.9g, calibrated: %zu\n",
                both.edges, both.initial_cost, both.final_cost, both.calibrated.size());
    const bool recovered = both.calibrated.size() == 2 && both.final_cost < both.initial_cost &&
        std::abs(both.cameras.at(1).focal() / a.focal() - 1) < 0.02 &&
        std::abs(both.cameras.at(2).focal() / b.focal() - 1) < 0.02;
    auto fixed_cameras = guesses;
    fixed_cameras[1] = a;
    const auto fixed = calibrateFocalGraph(fixed_cameras, {1}, edges);
    const bool prior = fixed.cameras.at(1).focal() == a.focal() && fixed.calibrated.count(2) &&
                       std::abs(fixed.cameras.at(2).focal() / b.focal() - 1) < 0.02;
    auto contaminated = edges;
    Camera wrong_a = a, wrong_b = b;
    wrong_a.setFocal(500); wrong_b.setFocal(1700);
    for (int i = 0; i < 4; i++) {
        const Mat3 R = angleAxisToRotation({0.1, 0.1 + 0.05 * i, -0.08});
        contaminated.push_back({1, 2, fundamental(wrong_a, wrong_b, R, {0.7, 0.2, 0.3})});
    }
    const auto robust = calibrateFocalGraph(guesses, {}, contaminated);
    const bool outliers = robust.calibrated.size() == 2 &&
        std::abs(robust.cameras.at(1).focal() / a.focal() - 1) < 0.03 &&
        std::abs(robust.cameras.at(2).focal() / b.focal() - 1) < 0.03;
    const auto too_few = calibrateFocalGraph(guesses, {}, {edges[0]});
    const bool underconstrained = too_few.calibrated.empty() &&
                                  too_few.cameras.at(1).focal() == guess_a.focal();
    Camera wide = guess_a;
    wide.model = CamModel::OpenCVFisheye;
    auto mixed = guesses;
    mixed[1] = wide;
    const auto fisheye = calibrateFocalGraph(mixed, {}, edges);
    const bool excluded = !fisheye.calibrated.count(1) && fisheye.cameras.at(1).focal() == wide.focal();
    std::printf("Fetzer jacobians: %s\n", jacobian ? "PASS" : "FAIL");
    std::printf("shared and different camera focal recovery: %s (%.3f %.3f)\n",
                recovered ? "PASS" : "FAIL", both.cameras.at(1).focal(), both.cameras.at(2).focal());
    std::printf("fixed focal prior: %s\n", prior ? "PASS" : "FAIL");
    std::printf("robust to inconsistent edges: %s (%.3f %.3f)\n", outliers ? "PASS" : "FAIL",
                robust.cameras.at(1).focal(), robust.cameras.at(2).focal());
    std::printf("insufficient edges and fisheye: %s\n", underconstrained && excluded ? "PASS" : "FAIL");
    return jacobian && recovered && prior && outliers && underconstrained && excluded ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testViewGraphCalibration); }
