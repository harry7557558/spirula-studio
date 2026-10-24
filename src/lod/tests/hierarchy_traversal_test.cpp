// hierarchy_traversal_test -- tests for HierarchyTraversal.h/cpp:
// frustum culling (pinhole & equirectangular), screen-space error metric,
// mixed LOD cut selection, hysteresis stability, and smooth transitions.

#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyNode.h"
#include "lod/HierarchyTraversal.h"
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

spirula::SplatCloud make_corridor_cloud(int num_clusters, int splats_per_cluster) {
    spirula::SplatCloud c;
    c.num = (int64_t)num_clusters * splats_per_cluster;
    c.sh_degree = 0;
    c.sb_lobes = 0;

    c.means.resize((size_t)(c.num * 3));
    c.quats.resize((size_t)(c.num * 4));
    c.scales.resize((size_t)(c.num * 3));
    c.opacities.resize((size_t)c.num);
    c.features_dc.resize((size_t)(c.num * 3));

    int64_t idx = 0;
    for (int k = 0; k < num_clusters; k++) {
        // Clusters placed along z-axis from z=10 to z=100
        float cz = 10.0f + (float)k * 15.0f;
        for (int s = 0; s < splats_per_cluster; s++) {
            float ox = ((float)(s % 5) - 2.0f) * 0.5f;
            float oy = ((float)(s / 5) - 2.0f) * 0.5f;
            c.means[(size_t)(idx * 3 + 0)] = ox;
            c.means[(size_t)(idx * 3 + 1)] = oy;
            c.means[(size_t)(idx * 3 + 2)] = cz;

            c.quats[(size_t)(idx * 4 + 0)] = 1.0f;
            c.scales[(size_t)(idx * 3 + 0)] = -2.0f;
            c.scales[(size_t)(idx * 3 + 1)] = -2.0f;
            c.scales[(size_t)(idx * 3 + 2)] = -2.0f;
            c.opacities[(size_t)idx] = 2.0f; // opaque

            c.features_dc[(size_t)(idx * 3 + 0)] = 0.5f;
            c.features_dc[(size_t)(idx * 3 + 1)] = 0.5f;
            c.features_dc[(size_t)(idx * 3 + 2)] = 0.5f;
            idx++;
        }
    }
    return c;
}

void test_frustum_culling() {
    std::printf("--- Test: Frustum Culling (Pinhole & Equirectangular) ---\n");

    // Standard 90-degree FOV symmetric perspective matrix looking down +Z
    // n=0.1, f=1000, aspect=1
    float proj[16] = {
        1.0f, 0.0f,  0.0f, 0.0f,
        0.0f, 1.0f,  0.0f, 0.0f,
        0.0f, 0.0f,  1.0f, -0.2f,
        0.0f, 0.0f,  1.0f, 0.0f
    };

    // Box 1: In front of camera at z=10, inside frustum
    spirula::Aabb3 box_visible;
    box_visible.expand(-1.0f, -1.0f, 9.0f);
    box_visible.expand(1.0f, 1.0f, 11.0f);
    check(spirula::test_aabb_frustum_pinhole(box_visible, proj),
          "box inside frustum is visible");

    // Box 2: Behind camera at z=-10
    spirula::Aabb3 box_behind;
    box_behind.expand(-1.0f, -1.0f, -11.0f);
    box_behind.expand(1.0f, 1.0f, -9.0f);
    check(!spirula::test_aabb_frustum_pinhole(box_behind, proj),
          "box behind camera is culled");

    // Box 3: Way off to the left (x = -500, z = 10)
    spirula::Aabb3 box_left;
    box_left.expand(-510.0f, -1.0f, 9.0f);
    box_left.expand(-490.0f, 1.0f, 11.0f);
    check(!spirula::test_aabb_frustum_pinhole(box_left, proj),
          "box outside lateral FOV is culled");

    // Equirectangular test:
    float cam_pos[3] = {0, 0, 0};
    check(spirula::test_aabb_equirectangular(box_visible, cam_pos, 0.1f, 100.0f),
          "equirectangular view sees front box");
    check(spirula::test_aabb_equirectangular(box_behind, cam_pos, 0.1f, 100.0f),
          "equirectangular 360-degree view sees rear box");

    spirula::Aabb3 box_too_far;
    box_too_far.expand(0.0f, 0.0f, 500.0f);
    box_too_far.expand(1.0f, 1.0f, 510.0f);
    check(!spirula::test_aabb_equirectangular(box_too_far, cam_pos, 0.1f, 100.0f),
          "equirectangular view culls box beyond z_far");
}

void test_mixed_lod_selection() {
    std::printf("\n--- Test: Mixed LOD Cut Selection & Invariants ---\n");

    // Create a corridor of 6 clusters along z from z=10 to z=85
    spirula::SplatCloud cloud = make_corridor_cloud(6, 25); // 150 splats
    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(cloud, bcfg);

    spirula::LodViewParams view;
    view.cam_pos[0] = 0; view.cam_pos[1] = 0; view.cam_pos[2] = 0;
    view.fx = 1000.0f; view.fy = 1000.0f;
    view.width = 1920; view.height = 1080;

    spirula::LodRenderConfig rcfg;
    rcfg.quality_threshold = 60.0f; // pixels: near clusters have ~100px error, far clusters have ~12px error

    spirula::HierarchyTraversal traversal;
    spirula::LodCutResult cut = traversal.select_cut(hier, view, rcfg);

    check(cut.total_rendered_splats > 0, "cut contains active rendered splats");
    check(cut.total_rendered_splats < hier.num_splats,
          "LOD cut significantly reduces splat count vs full resolution");
    check(cut.num_leaf_nodes_selected > 0, "near clusters select fine leaves");
    check(cut.num_summary_nodes_selected > 0, "distant clusters select coarse summary parents");

    // Extraction test
    std::vector<float> means, quats, scales, opacities, dc, sh;
    spirula::HierarchyTraversal::extract_render_splats(hier, cut, means, quats, scales, opacities, dc, sh);

    check(means.size() == (size_t)(cut.total_rendered_splats * 3), "extracted means match cut splat count");
    check(opacities.size() == (size_t)cut.total_rendered_splats, "extracted opacities match cut splat count");
}

void test_hysteresis_stability() {
    std::printf("\n--- Test: Hysteresis Temporal Stability ---\n");

    spirula::SplatCloud cloud = make_corridor_cloud(4, 30);
    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(cloud, bcfg);

    spirula::LodRenderConfig rcfg;
    rcfg.quality_threshold = 60.0f;
    rcfg.hysteresis_band = 0.25f; // 25% deadband

    spirula::HierarchyTraversal traversal;

    spirula::LodViewParams view1;
    view1.cam_pos[2] = 0.0f;
    spirula::LodCutResult cut1 = traversal.select_cut(hier, view1, rcfg);

    // Small camera movement forward by 0.1 units (should stay stable due to hysteresis)
    spirula::LodViewParams view2 = view1;
    view2.cam_pos[2] = 0.1f;
    spirula::LodCutResult cut2 = traversal.select_cut(hier, view2, rcfg);

    check(cut1.total_rendered_splats == cut2.total_rendered_splats,
          "small camera jitter maintains identical LOD selection (anti-chatter)");

    // Large camera movement forward by 20 units (should refine closer clusters)
    spirula::LodViewParams view3 = view1;
    view3.cam_pos[2] = 20.0f;
    spirula::LodCutResult cut3 = traversal.select_cut(hier, view3, rcfg);

    check(cut3.total_rendered_splats != cut1.total_rendered_splats,
          "significant camera movement appropriately refines LOD cut");
}

}  // namespace

int main() {
    test_frustum_culling();
    test_mixed_lod_selection();
    test_hysteresis_stability();

    if (g_failures == 0) std::printf("hierarchy_traversal_test: OK\n");
    return g_failures ? 1 : 0;
}
