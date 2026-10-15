// hierarchy_math_stress_test.cpp -- numerical geometry and aggregation stress tests.

#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyNode.h"
#include "checkpoint/SplatPly.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static int g_failures = 0;

static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

// Helper: build covariance from log-scale and quaternion
static void cov_from_scale_quat(const float log_scale[3], const float q[4], float S[6]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float R[9] = {
        1 - 2*(y*y + z*z), 2*(x*y - w*z),     2*(x*z + w*y),
        2*(x*y + w*z),     1 - 2*(x*x + z*z), 2*(y*z - w*x),
        2*(x*z - w*y),     2*(y*z + w*x),     1 - 2*(x*x + y*y)
    };
    float s0 = std::exp(log_scale[0]), s1 = std::exp(log_scale[1]), s2 = std::exp(log_scale[2]);
    float d[3] = {s0*s0, s1*s1, s2*s2};
    S[0] = R[0]*R[0]*d[0] + R[1]*R[1]*d[1] + R[2]*R[2]*d[2];
    S[1] = R[0]*R[3]*d[0] + R[1]*R[4]*d[1] + R[2]*R[5]*d[2];
    S[2] = R[0]*R[6]*d[0] + R[1]*R[7]*d[1] + R[2]*R[8]*d[2];
    S[3] = R[3]*R[3]*d[0] + R[4]*R[4]*d[1] + R[5]*R[5]*d[2];
    S[4] = R[3]*R[6]*d[0] + R[4]*R[7]*d[1] + R[5]*R[8]*d[2];
    S[5] = R[6]*R[6]*d[0] + R[7]*R[7]*d[1] + R[8]*R[8]*d[2];
}

// ---------------------------------------------------------------------------
// Test: Covariance Reconstruction and Eigenvector Orientation
// ---------------------------------------------------------------------------
static void test_covariance_eigen_orientation() {
    std::printf("--- Test: Covariance Reconstruction & Eigenvector Orientation ---\n");

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> scale_dist(-4.0f, 1.0f);
    std::uniform_real_distribution<float> quat_dist(-1.0f, 1.0f);

    float max_rec_error = 0.0f;

    for (int t = 0; t < 200; t++) {
        spirula::SplatCloud c;
        c.num = 2;
        c.sh_degree = 0;
        c.sb_lobes = 0;
        c.means = {0,0,0, 0,0,0};
        c.opacities = {1.0f, 1.0f};
        c.features_dc = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        c.scales.resize(6);
        c.quats.resize(8);

        for (int i = 0; i < 2; i++) {
            for (int k = 0; k < 3; k++) c.scales[i*3 + k] = scale_dist(rng);
            float q[4], qlen = 0;
            for (int k = 0; k < 4; k++) { q[k] = quat_dist(rng); qlen += q[k]*q[k]; }
            qlen = std::sqrt(qlen);
            for (int k = 0; k < 4; k++) c.quats[i*4 + k] = q[k] / qlen;
        }

        spirula::LodBuildConfig cfg;
        cfg.max_leaf_size = 2;
        spirula::GaussianHierarchy h = spirula::build_gaussian_hierarchy(c, cfg);

        // Check root summary Gaussian covariance vs child covariances
        const auto& sg = h.nodes[(size_t)h.root].summary[0];
        float S_parent[6];
        cov_from_scale_quat(sg.scale, sg.quat, S_parent);

        float S_c0[6], S_c1[6];
        cov_from_scale_quat(&c.scales[0], &c.quats[0], S_c0);
        cov_from_scale_quat(&c.scales[3], &c.quats[4], S_c1);

        // Since both means are collocated at origin and have equal opacity:
        // Expected parent covariance is exactly 0.5 * (S_c0 + S_c1)
        for (int k = 0; k < 6; k++) {
            float exp_s = 0.5f * (S_c0[k] + S_c1[k]);
            float err = std::abs(S_parent[k] - exp_s);
            max_rec_error = std::max(max_rec_error, err);
        }
    }

    check(max_rec_error < 1e-4f,
          "covariance moment matching and decomposition reconstructs accurately (max err: " +
          std::to_string(max_rec_error) + ")");
}

// ---------------------------------------------------------------------------
// Test: Degenerate Ellipsoids (Planar & Needle Gaussians)
// ---------------------------------------------------------------------------
static void test_degenerate_ellipsoids() {
    std::printf("--- Test: Degenerate Ellipsoids (Planar & Needle Gaussians) ---\n");

    spirula::SplatCloud c;
    c.num = 4;
    c.sh_degree = 0;
    c.sb_lobes = 0;
    c.means = {0,0,0,  1,0,0,  0,1,0,  1,1,0};
    c.opacities = {2.0f, 2.0f, 2.0f, 2.0f};
    c.features_dc.assign(12, 0.5f);
    // Extreme anisotropy: s_z is tiny (planar sheet)
    c.scales = {
        0.0f,  0.0f, -12.0f,
        0.0f,  0.0f, -12.0f,
        0.0f,  0.0f, -12.0f,
        0.0f,  0.0f, -12.0f
    };
    c.quats = {
        1,0,0,0,  1,0,0,0,  1,0,0,0,  1,0,0,0
    };

    spirula::LodBuildConfig cfg;
    cfg.max_leaf_size = 2;
    spirula::GaussianHierarchy h = spirula::build_gaussian_hierarchy(c, cfg);

    bool all_finite = true;
    for (const auto& nd : h.nodes) {
        if (!nd.summary.empty()) {
            for (int k = 0; k < 3; k++) {
                if (!std::isfinite(nd.summary[0].scale[k])) all_finite = false;
                if (!std::isfinite(nd.summary[0].mean[k])) all_finite = false;
            }
            for (int k = 0; k < 4; k++) {
                if (!std::isfinite(nd.summary[0].quat[k])) all_finite = false;
            }
        }
    }
    check(all_finite, "planar degenerate Gaussians produce finite non-NaN summary parameters");
}

// ---------------------------------------------------------------------------
// Test: Extreme Opacity Ranges (Near-Zero and Near-One)
// ---------------------------------------------------------------------------
static void test_extreme_opacity() {
    std::printf("--- Test: Extreme Opacity Ranges ---\n");

    spirula::SplatCloud c;
    c.num = 4;
    c.sh_degree = 0;
    c.sb_lobes = 0;
    c.means = {0,0,0,  1,0,0,  2,0,0,  3,0,0};
    // Opacities: near zero (logit -12 -> prob 6e-6) and near one (logit 12 -> prob 0.999994)
    c.opacities = {-12.0f, -12.0f, 12.0f, 12.0f};
    c.features_dc.assign(12, 0.5f);
    c.scales.assign(12, -2.0f);
    c.quats.assign(16, 0.0f);
    for (int i = 0; i < 4; i++) c.quats[i*4] = 1.0f;

    spirula::LodBuildConfig cfg;
    cfg.max_leaf_size = 2;
    spirula::GaussianHierarchy h = spirula::build_gaussian_hierarchy(c, cfg);

    bool valid_opacity = true;
    for (const auto& nd : h.nodes) {
        if (!nd.summary.empty()) {
            float op = nd.summary[0].opacity;
            if (!std::isfinite(op) || op < -20.0f || op > 20.0f) valid_opacity = false;
        }
    }
    check(valid_opacity, "extreme opacities handled without overflow or NaN");
}

// ---------------------------------------------------------------------------
// Test: No Duplicate Summaries in Internal Nodes
// ---------------------------------------------------------------------------
static void test_no_duplicate_summaries() {
    std::printf("--- Test: No Duplicate Summaries in Internal Nodes ---\n");

    spirula::SplatCloud c;
    c.num = 32;
    c.sh_degree = 0;
    c.sb_lobes = 0;
    c.means.resize(96, 0.0f);
    c.opacities.resize(32, 1.0f);
    c.features_dc.resize(96, 0.5f);
    c.scales.resize(96, -2.0f);
    c.quats.resize(128, 0.0f);
    for (int i = 0; i < 32; i++) {
        c.means[i*3] = (float)i;
        c.quats[i*4] = 1.0f;
    }

    spirula::LodBuildConfig cfg;
    cfg.max_leaf_size = 4;
    spirula::GaussianHierarchy h = spirula::build_gaussian_hierarchy(c, cfg);

    bool single_summary_per_node = true;
    for (const auto& nd : h.nodes) {
        if (!nd.is_leaf()) {
            if (nd.summary.size() != 1) single_summary_per_node = false;
        }
    }
    check(single_summary_per_node,
          "internal nodes contain exactly one consolidated summary Gaussian (no double-counting)");
}

int main() {
    test_covariance_eigen_orientation();
    test_degenerate_ellipsoids();
    test_extreme_opacity();
    test_no_duplicate_summaries();

    if (g_failures == 0) std::printf("hierarchy_math_stress_test: OK\n");
    return g_failures ? 1 : 0;
}

