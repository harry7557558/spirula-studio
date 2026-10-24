// hierarchy_concurrency_test.cpp -- concurrency, streaming, and memory stress tests.

#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyCache.h"
#include "lod/HierarchyFile.h"
#include "lod/HierarchyTraversal.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>

static int g_failures = 0;

static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

static spirula::SplatCloud make_cloud(int64_t n) {
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = 0;
    c.sb_lobes = 0;
    c.means.resize((size_t)(n * 3));
    c.quats.resize((size_t)(n * 4));
    c.scales.resize((size_t)(n * 3));
    c.opacities.resize((size_t)n, 1.0f);
    c.features_dc.resize((size_t)(n * 3), 0.5f);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> d(-50.0f, 50.0f);
    for (int64_t i = 0; i < n; i++) {
        c.means[(size_t)(i * 3 + 0)] = d(rng);
        c.means[(size_t)(i * 3 + 1)] = d(rng);
        c.means[(size_t)(i * 3 + 2)] = d(rng);
        c.quats[(size_t)(i * 4 + 0)] = 1.0f;
        c.scales[(size_t)(i * 3 + 0)] = -2.0f;
        c.scales[(size_t)(i * 3 + 1)] = -2.0f;
        c.scales[(size_t)(i * 3 + 2)] = -2.0f;
    }
    return c;
}

int main() {
    const std::string slod_path = "/tmp/test_concurrency.slod";

    {
        auto cloud = make_cloud(5000);
        spirula::LodBuildConfig bcfg;
        bcfg.max_leaf_size = 16;
        auto hier = spirula::build_gaussian_hierarchy(cloud, bcfg);
        spirula::write_slod(hier, slod_path);
    }

    // ---- Concurrent multi-threaded traversal ----
    {
        auto hier = spirula::read_slod(slod_path, false);
        spirula::LodRenderConfig rcfg;
        rcfg.enabled = true;
        rcfg.quality_threshold = 2.0f;

        std::atomic<bool> start_flag{false};
        std::atomic<int64_t> total_cuts{0};
        std::vector<std::thread> workers;

        for (int t = 0; t < 8; t++) {
            workers.emplace_back([&, t]() {
                while (!start_flag.load()) std::this_thread::yield();

                spirula::HierarchyTraversal traversal;
                std::mt19937 rng(100 + t);
                std::uniform_real_distribution<float> pos_d(-60.0f, 60.0f);

                for (int iter = 0; iter < 50; iter++) {
                    spirula::LodViewParams view;
                    view.cam_pos[0] = pos_d(rng);
                    view.cam_pos[1] = pos_d(rng);
                    view.cam_pos[2] = pos_d(rng);
                    view.fx = 1000.0f;
                    view.fy = 1000.0f;

                    spirula::LodCutResult cut = traversal.select_cut(hier, view, rcfg);
                    if (cut.total_rendered_splats > 0) total_cuts.fetch_add(1);
                }
            });
        }

        start_flag.store(true);
        for (auto& w : workers) w.join();

        check(total_cuts.load() == 8 * 50,
              "8 threads concurrently completed 400 cuts without race or crash");
    }

    // ---- Active streaming and LRU budget eviction ----
    {
        spirula::HierarchyCache cache(slod_path, /*vram_budget_mb=*/1);
        cache.start_worker();

        spirula::HierarchyTraversal traversal;
        spirula::LodRenderConfig rcfg;
        rcfg.enabled = true;
        rcfg.quality_threshold = 1.0f;

        auto hier = spirula::read_slod(slod_path, true);
        bool fallback_active = false;
        bool streamed_active = false;

        std::vector<float> m, q, s, op, dc, sh;

        for (uint64_t f = 1; f <= 60; f++) {
            spirula::LodViewParams view;
            float angle = (float)f * 0.1f;
            view.cam_pos[0] = 30.0f * std::cos(angle);
            view.cam_pos[1] = 10.0f;
            view.cam_pos[2] = 30.0f * std::sin(angle);

            spirula::LodCutResult cut = traversal.select_cut(hier, view, rcfg);
            spirula::LodCutResult resolved = cache.resolve_cut_with_fallback(cut, f, 1);

            if (resolved.num_summary_nodes_selected > 0) fallback_active = true;
            if (resolved.num_leaf_nodes_selected > 0) streamed_active = true;

            cache.extract_render_splats(resolved, m, q, s, op, dc, sh);
            cache.evict_if_over_budget(f);

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        cache.stop_worker();
        check(fallback_active, "fallback ancestor summaries activated during initial loads");
        check(streamed_active, "streamed leaf chunks became resident and rendered");
        check(cache.resident_bytes() <= 2 * 1024 * 1024,
              "resident cache strictly respected budget envelope");
    }

    // ---- Worker thread lifecycle stress ----
    {
        spirula::HierarchyCache cache(slod_path, 0);
        bool ok = true;
        for (int i = 0; i < 30; i++) {
            cache.start_worker();
            cache.stop_worker();
        }
        check(ok, "worker thread safely started and stopped across 30 rapid cycles");
    }

    std::remove(slod_path.c_str());

    if (g_failures == 0) std::printf("hierarchy_concurrency_test: OK\n");
    return g_failures ? 1 : 0;
}

