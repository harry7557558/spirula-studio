#include <cmath>
#include <cstdio>
#include <vector>

#include "sfm/map/RotationAverage.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int testRotationAverage(int, char**) {
    MatchesDatabase db;
    db.images.resize(8);
    std::vector<Mat3> truth;
    for (int i = 0; i < 8; i++)
        truth.push_back(angleAxisToRotation({0.04 * i, -0.07 * i, 0.025 * i}));
    std::vector<RelativeViewEdge> edges;
    auto add = [&](uint32_t a, uint32_t b, const Mat3& rotation, uint32_t weight) {
        TwoViewMatches p;
        p.image1 = a;
        p.image2 = b;
        db.pairs.push_back(p);
        RelativeViewEdge e;
        e.pose.R = rotation;
        e.inliers = weight;
        e.status = RelativeViewStatus::Accepted;
        edges.push_back(e);
    };
    for (uint32_t i = 0; i < 5; i++)
        for (uint32_t j = i + 1; j < 5; j++) {
            const Mat3 relative = mul(truth[j], transpose(truth[i]));
            const double d = 0.002 * (int(j - i) - 2);
            add(i, j, mul(angleAxisToRotation({d, -d * 0.5, d * 0.3}), relative), 100);
        }
    const size_t bad_edge = edges.size();
    add(0, 4, mul(angleAxisToRotation({0.5, 0, 0}),
                  mul(truth[4], transpose(truth[0]))), 1000);
    add(6, 5, mul(truth[5], transpose(truth[6])), 100);
    RotationAverageOptions opt;
    opt.irls_sigma_radians = 0.025;
    opt.max_error_radians = 0.10;
    const RotationAverageResult result = averageViewRotations(db, edges, opt);
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
        failures += !ok;
    };
    check(result.converged, "rotation solver converged");
    double worst = 0;
    for (int i = 0; i < 5; i++) {
        const Mat3 expected = mul(truth[i], transpose(truth[0]));
        worst = std::max(worst,
                         rotationToAngleAxis(mul(result.rotations[i], transpose(expected))).norm());
    }
    check(worst < 0.01, "noisy rotations recovered up to gauge");
    check(!result.accepted[bad_edge], "inconsistent relative rotation removed");
    RotationAverageOptions l1_only = opt;
    l1_only.irls_iterations = 0;
    const RotationAverageResult l1_result = averageViewRotations(db, edges, l1_only);
    double l1_worst = 0;
    for (int i = 0; i < 5; i++) {
        const Mat3 expected = mul(truth[i], transpose(truth[0]));
        l1_worst = std::max(l1_worst,
                            rotationToAngleAxis(mul(l1_result.rotations[i], transpose(expected))).norm());
    }
    check(l1_result.converged && l1_worst < 0.01 && !l1_result.accepted[bad_edge],
          "L1 ADMM rejects a high-weight outlier");
    check(result.component[0] == 0 && result.component[4] == 0 &&
          result.component[5] == 5 && result.component[6] == 5 &&
          result.component[7] == UINT32_MAX,
          "disconnected components and isolated image");
    const Mat3 expected = mul(truth[6], transpose(truth[5]));
    check(rotationToAngleAxis(mul(result.rotations[6], transpose(expected))).norm() < 1e-8,
          "reversed edge convention");
    MatchesDatabase rig_db;
    rig_db.images.resize(6);
    RigSpec spec;
    spec.members.resize(2);
    spec.members[0].has_ext = true;
    spec.members[0].ext = {mat3Identity(), {0, 0, 0}};
    spec.members[1].has_ext = true;
    spec.members[1].ext = {angleAxisToRotation({0.1, -0.3, 0.2}), {0.2, 0, 0}};
    spec.frames = {{0, 1}, {2, 3}, {4, 5}};
    RigTable rigs;
    rigs.rigs.push_back(spec);
    rigs.index(rig_db.images.size());
    std::vector<Mat3> rig_truth;
    for (uint32_t f = 0; f < 3; f++) {
        const Mat3 frame_rotation = angleAxisToRotation({0.04 * f, -0.08 * f, 0.03 * f});
        for (uint32_t m = 0; m < 2; m++)
            rig_truth.push_back(mul(spec.members[m].ext.R, frame_rotation));
    }
    std::vector<RelativeViewEdge> rig_edges;
    auto rig_edge = [&](uint32_t a, uint32_t b, const Mat3& noise) {
        TwoViewMatches pair;
        pair.image1 = a;
        pair.image2 = b;
        rig_db.pairs.push_back(pair);
        RelativeViewEdge edge;
        edge.pose.R = mul(noise, mul(rig_truth[b], transpose(rig_truth[a])));
        edge.inliers = 100;
        edge.status = RelativeViewStatus::Accepted;
        rig_edges.push_back(edge);
    };
    for (uint32_t a = 0; a < 3; a++)
        for (uint32_t b = a + 1; b < 3; b++)
            for (uint32_t m = 0; m < 2; m++)
                rig_edge(2 * a + m, 2 * b + m,
                         angleAxisToRotation({m ? 0.004 : -0.004, 0.002, 0}));
    rig_edge(0, 1, mat3Identity());
    rig_edge(2, 3, angleAxisToRotation({0.5, 0, 0}));
    const auto rig_result = averageRigViewRotations(rig_db, rig_edges, &rigs, opt);
    bool rig_ok = true;
    for (uint32_t f = 0; f < 3; f++) {
        const Mat3 actual = mul(rig_result.rotations[2 * f + 1],
                                transpose(rig_result.rotations[2 * f]));
        rig_ok = rig_ok && rotationToAngleAxis(mul(actual, transpose(spec.members[1].ext.R))).norm()
                           < 1e-8;
    }
    check(rig_ok && rig_result.accepted[6] && !rig_result.accepted[7],
          "fixed rig rotations share frame orientation and reject inconsistent edges");
    rigs.rigs[0].members[1].ext_fixed = false;
    const auto free_result = averageRigViewRotations(rig_db, rig_edges, &rigs, opt);
    const auto independent = averageViewRotations(rig_db, rig_edges, opt);
    bool free_ok = free_result.accepted == independent.accepted &&
                   free_result.component == independent.component;
    for (size_t i = 0; i < rig_db.images.size(); i++)
        free_ok = free_ok && rotationToAngleAxis(mul(free_result.rotations[i],
                                       transpose(independent.rotations[i]))).norm() < 1e-10;
    check(free_ok, "refinable rig rotation is not treated as fixed");
    MatchesDatabase loop_db;
    constexpr uint32_t loop_size = 128;
    loop_db.images.resize(loop_size);
    std::vector<RelativeViewEdge> loop_edges;
    for (uint32_t i = 0; i < loop_size; i++) {
        TwoViewMatches pair;
        pair.image1 = i;
        pair.image2 = (i + 1) % loop_size;
        loop_db.pairs.push_back(pair);
        RelativeViewEdge edge;
        edge.status = RelativeViewStatus::Accepted;
        edge.inliers = 100;
        edge.pose.R = i + 1 == loop_size ? mat3Identity() : angleAxisToRotation({0, 0, 0.002});
        loop_edges.push_back(edge);
    }
    RotationAverageOptions loop_opt;
    loop_opt.max_iterations = 20;
    loop_opt.max_error_radians = 0.3;
    const auto loop = averageViewRotations(loop_db, loop_edges, loop_opt);
    double loop_error = 0;
    for (const Mat3& R : loop.rotations)
        loop_error = std::max(loop_error, rotationToAngleAxis(R).norm());
    check(loop.converged && loop_error < 0.01 && loop.accepted.back(),
          "long loop distributes rotational drift globally");
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testRotationAverage); }
