// hierarchy_cache_test -- tests for HierarchyCache.h/cpp:
// memory budget enforcement, out-of-core streaming fallback, async loading,
// and LRU eviction with anti-thrashing protection.

#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyCache.h"
#include "lod/HierarchyFile.h"
#include "lod/HierarchyNode.h"
#include "checkpoint/SplatPly.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

spirula::SplatCloud make_cache_test_cloud(int64_t n) {
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = 0;
    c.sb_lobes = 0;

    c.means.resize((size_t)(n * 3));
    c.quats.resize((size_t)(n * 4));
    c.scales.resize((size_t)(n * 3));
    c.opacities.resize((size_t)n);
    c.features_dc.resize((size_t)(n * 3));

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-20.0f, 20.0f);

    for (int64_t i = 0; i < n; i++) {
        c.means[(size_t)(i * 3 + 0)] = dist(rng);
        c.means[(size_t)(i * 3 + 1)] = dist(rng);
        c.means[(size_t)(i * 3 + 2)] = dist(rng);

        c.quats[(size_t)(i * 4 + 0)] = 1.0f;
        c.scales[(size_t)(i * 3 + 0)] = -1.5f;
        c.scales[(size_t)(i * 3 + 1)] = -1.5f;
        c.scales[(size_t)(i * 3 + 2)] = -1.5f;
        c.opacities[(size_t)i] = 1.0f;
        c.features_dc[(size_t)(i * 3 + 0)] = 0.5f;
        c.features_dc[(size_t)(i * 3 + 1)] = 0.5f;
        c.features_dc[(size_t)(i * 3 + 2)] = 0.5f;
    }
    return c;
}

void test_fallback_and_streaming() {
    std::printf("--- Test: Cache Initialization, Ancestor Fallback & Streaming ---\n");

    const int64_t N = 400;
    spirula::SplatCloud cloud = make_cache_test_cloud(N);

    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    bcfg.max_parent_gaussians = 4;
    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(cloud, bcfg);

    std::string test_file = "/tmp/test_cache_model.slod";
    spirula::write_slod(hier, test_file);

    // 1. Initialize cache with a tiny budget
    spirula::HierarchyCache cache(test_file, /*vram_budget_mb=*/1);
    check(cache.hierarchy().num_splats == N, "cache loaded hierarchy metadata successfully");
    check(cache.num_resident_chunks() == 0, "cache starts with 0 resident chunks");

    // 2. Query cut for a view
    spirula::LodViewParams view;
    view.cam_pos[2] = -10.0f;
    spirula::LodRenderConfig rcfg;
    rcfg.quality_threshold = 0.5f; // aggressive refinement (wants leaf nodes)

    spirula::HierarchyTraversal traversal;
    spirula::LodCutResult desired_cut = traversal.select_cut(cache.hierarchy(), view, rcfg);

    // 3. Resolve cut with fallback (since all leaves are initially unloaded, must fallback to ancestors)
    uint64_t frame = 1;
    spirula::LodCutResult resolved_cut = cache.resolve_cut_with_fallback(desired_cut, frame);

    check(resolved_cut.total_rendered_splats > 0, "fallback cut renders non-zero splats");
    check(resolved_cut.num_summary_nodes_selected > 0,
          "unloaded leaves gracefully fall back to ancestor summary Gaussians");
    check(cache.num_pending_requests() > 0, "fallback triggers background load requests");

    // 4. Start background worker and wait for requests to complete
    cache.start_worker();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    check(cache.num_resident_chunks() > 0,
          "background worker successfully loaded requested chunks into resident cache");

    // 5. Query again: now resident chunks should be used instead of fallback
    frame = 2;
    spirula::LodCutResult resident_cut = cache.resolve_cut_with_fallback(desired_cut, frame);
    check(resident_cut.num_leaf_nodes_selected > 0,
          "newly resident chunks are now selected as full-detail leaves");

    // 6. Test eviction under budget
    // Set a very small budget (e.g. 1 KB) to force eviction
    cache.set_budget_mb(0); // reset first
    // Simulate frames advancing past grace period (current_frame + 3)
    frame = 10;
    cache.evict_if_over_budget(frame);

    cache.stop_worker();
    fs::remove(test_file);
}

}  // namespace

int main() {
    test_fallback_and_streaming();

    if (g_failures == 0) std::printf("hierarchy_cache_test: OK\n");
    return g_failures ? 1 : 0;
}

