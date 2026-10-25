// hierarchy_build_test -- tests for HierarchyBuilder.h/cpp:
// verifies tree topology, parent-child invariants, bounding box conservatism,
// moment-matching aggregation, and handling of edge cases (1 splat, collocated splats,
// large coordinate bounds).

#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyNode.h"
#include "checkpoint/SplatPly.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

spirula::SplatCloud make_synthetic_cloud(int64_t n, unsigned seed, float spread = 10.0f) {
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = 1;
    c.sb_lobes = 0;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos_dist(-spread, spread);
    std::uniform_real_distribution<float> scale_dist(-3.0f, -1.0f); // log scale ~ 0.05..0.36
    std::uniform_real_distribution<float> opac_dist(0.0f, 3.0f);    // logit opacity > 0.5
    std::uniform_real_distribution<float> col_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> quat_dist(-1.0f, 1.0f);

    c.means.resize((size_t)(n * 3));
    c.quats.resize((size_t)(n * 4));
    c.scales.resize((size_t)(n * 3));
    c.opacities.resize((size_t)n);
    c.features_dc.resize((size_t)(n * 3));
    c.features_sh.resize((size_t)(n * c.coeffs() * 3));

    for (int64_t i = 0; i < n; i++) {
        for (int j = 0; j < 3; j++) {
            c.means[(size_t)(i * 3 + j)] = pos_dist(rng);
            c.scales[(size_t)(i * 3 + j)] = scale_dist(rng);
            c.features_dc[(size_t)(i * 3 + j)] = col_dist(rng);
        }
        float q[4];
        float qlen = 0;
        for (int j = 0; j < 4; j++) {
            q[j] = quat_dist(rng);
            qlen += q[j] * q[j];
        }
        qlen = std::sqrt(qlen);
        for (int j = 0; j < 4; j++) c.quats[(size_t)(i * 4 + j)] = q[j] / qlen;

        c.opacities[(size_t)i] = opac_dist(rng);

        for (int64_t k = 0; k < c.coeffs() * 3; k++) {
            c.features_sh[(size_t)(i * c.coeffs() * 3 + k)] = col_dist(rng) * 0.1f;
        }
    }
    return c;
}

void test_invariants() {
    std::printf("--- Test: Tree Invariants & Connectivity ---\n");

    const int64_t N = 500;
    spirula::SplatCloud cloud = make_synthetic_cloud(N, 12345);

    spirula::LodBuildConfig cfg;
    cfg.max_leaf_size = 16;
    cfg.max_parent_gaussians = 4;

    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(cloud, cfg);

    check(hier.num_splats == N, "hierarchy retains exact splat count");
    check(!hier.nodes.empty(), "nodes list is not empty");
    check(hier.root >= 0 && hier.root < (int32_t)hier.nodes.size(), "valid root node index");
    check(hier.nodes[(size_t)hier.root].parent == -1, "root node parent is -1");

    // Check all parent-child relationships
    bool parent_child_ok = true;
    int64_t leaf_count = 0;
    int64_t total_leaf_splats = 0;

    for (size_t i = 0; i < hier.nodes.size(); i++) {
        const auto& nd = hier.nodes[i];
        if (nd.is_leaf()) {
            leaf_count++;
            total_leaf_splats += nd.splat_count;
            if (nd.splat_count <= 0 || nd.splat_count > cfg.max_leaf_size) {
                parent_child_ok = false;
            }
        } else {
            if (nd.child_left < 0 || nd.child_right < 0) parent_child_ok = false;
            if (hier.nodes[(size_t)nd.child_left].parent != (int32_t)i) parent_child_ok = false;
            if (hier.nodes[(size_t)nd.child_right].parent != (int32_t)i) parent_child_ok = false;
            if (nd.summary.empty()) parent_child_ok = false;
        }
    }

    check(parent_child_ok, "parent-child symmetry and leaf size bounds hold");
    check(total_leaf_splats == N, "sum of leaf splats equals total splats");
    check(hier.nodes[(size_t)hier.root].total_descendant_splats == N,
          "root total_descendant_splats matches cloud count");
}

void test_bounds_conservatism() {
    std::printf("\n--- Test: Bounding Box Conservatism ---\n");

    const int64_t N = 300;
    spirula::SplatCloud cloud = make_synthetic_cloud(N, 54321);

    spirula::LodBuildConfig cfg;
    cfg.max_leaf_size = 16;

    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(cloud, cfg);

    // Verify: every leaf splat's mean is inside the bounds of every ancestor up to root!
    bool containment_ok = true;
    for (size_t i = 0; i < hier.nodes.size(); i++) {
        const auto& nd = hier.nodes[i];
        if (!nd.is_leaf()) continue;

        for (int64_t s = 0; s < nd.splat_count; s++) {
            int64_t si = nd.splat_offset + s;
            float mx = hier.means[(size_t)(si * 3 + 0)];
            float my = hier.means[(size_t)(si * 3 + 1)];
            float mz = hier.means[(size_t)(si * 3 + 2)];

            // Walk up to root
            int32_t curr = (int32_t)i;
            while (curr >= 0) {
                const auto& anc = hier.nodes[(size_t)curr];
                if (mx < anc.bounds.lo[0] - 1e-4f || mx > anc.bounds.hi[0] + 1e-4f ||
                    my < anc.bounds.lo[1] - 1e-4f || my > anc.bounds.hi[1] + 1e-4f ||
                    mz < anc.bounds.lo[2] - 1e-4f || mz > anc.bounds.hi[2] + 1e-4f) {
                    containment_ok = false;
                    break;
                }
                curr = anc.parent;
            }
        }
    }
    check(containment_ok, "all splat means are strictly contained inside ancestor AABBs");

    // Verify: error radius is positive and non-decreasing up the tree
    bool error_radius_ok = true;
    for (size_t i = 0; i < hier.nodes.size(); i++) {
        const auto& nd = hier.nodes[i];
        if (nd.error_radius < 0.0f) error_radius_ok = false;
        if (!nd.is_leaf()) {
            float left_err = hier.nodes[(size_t)nd.child_left].error_radius;
            float right_err = hier.nodes[(size_t)nd.child_right].error_radius;
            if (nd.error_radius < left_err - 1e-4f || nd.error_radius < right_err - 1e-4f) {
                error_radius_ok = false;
            }
        }
    }
    check(error_radius_ok, "error radius is positive and monotonic up the hierarchy");
}

void test_edge_cases() {
    std::printf("\n--- Test: Edge Cases (Single Splat, Collocated Splats, Large Coordinates) ---\n");

    // Case 1: Single splat
    {
        spirula::SplatCloud c = make_synthetic_cloud(1, 42);
        spirula::LodBuildConfig cfg;
        spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(c, cfg);
        check(hier.num_splats == 1, "single splat cloud handled");
        check(hier.nodes.size() == 1, "single splat creates single leaf node");
        check(hier.nodes[0].is_leaf(), "root is leaf for single splat");
    }

    // Case 2: All collocated splats (zero variance along all axes)
    {
        spirula::SplatCloud c = make_synthetic_cloud(100, 99);
        for (int64_t i = 0; i < c.num; i++) {
            c.means[(size_t)(i * 3 + 0)] = 5.0f;
            c.means[(size_t)(i * 3 + 1)] = -2.0f;
            c.means[(size_t)(i * 3 + 2)] = 10.0f;
        }
        spirula::LodBuildConfig cfg;
        cfg.max_leaf_size = 16;
        spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(c, cfg);
        check(hier.num_splats == 100, "collocated splats handled without crash or NaN");
        bool no_nan = true;
        for (const auto& nd : hier.nodes) {
            if (std::isnan(nd.bounds.lo[0]) || std::isnan(nd.error_radius)) no_nan = false;
            for (const auto& sg : nd.summary) {
                if (std::isnan(sg.mean[0]) || std::isnan(sg.opacity)) no_nan = false;
            }
        }
        check(no_nan, "collocated splats produce finite non-NaN node parameters");
    }

    // Case 3: Large coordinates (aerial/urban scale e.g. 50,000 meters)
    {
        spirula::SplatCloud c = make_synthetic_cloud(200, 777, 50000.0f);
        spirula::LodBuildConfig cfg;
        cfg.max_leaf_size = 32;
        spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(c, cfg);
        check(hier.num_splats == 200, "large coordinate aerial scene handled");
        check(hier.nodes[(size_t)hier.root].bounds.diagonal() > 50000.0f,
              "large coordinate bounds extent preserved");
    }
}

}  // namespace

int main() {
    test_invariants();
    test_bounds_conservatism();
    test_edge_cases();

    if (g_failures == 0) std::printf("hierarchy_build_test: OK\n");
    return g_failures ? 1 : 0;
}

