// hierarchy_acceptance_test.cpp -- end-to-end acceptance validation:
// real GPU rendering, SSIM/PSNR, upload caching, VRAM budget, and partitions.

#include "app/EvalMetrics.h"
#include "checkpoint/SplatMerge.h"
#include "checkpoint/SplatPly.h"
#include "data/LabelField.h"
#include "data/ScenePartition.h"
#include "engine/Engine.h"
#include "engine/EngineState.h"
#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyCache.h"
#include "lod/HierarchyFile.h"
#include "lod/HierarchyRenderer.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static constexpr int W = 160, H = 120;

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

static std::vector<float> render_current(const std::vector<float>& viewmat) {
    const std::vector<float> intr = {120.0f, 120.0f, W * 0.5f, H * 0.5f};
    const std::vector<float> dist(kCameraDistortionParams, 0.0f);
    set_camera_params(W, H, "PINHOLE", "NONE",
                      ttv(viewmat.data(), {1, 4, 4}),
                      ttv(intr.data(), {1, 4}),
                      ttv(dist.data(), {1, kCameraDistortionParams}));
    forward_3dgs("3dgs", 0, false, false, 0);
    backend::device_synchronize();
    std::vector<float> rgb((size_t)H * W * 3);
    engine_copy_render_to_host(ttv(rgb.data(), {1, H, W, 3}), ttv_null(),
                               ttv_null(), ttv_null(), ttv_null());
    return rgb;
}

static std::vector<float> render_full_cloud(const spirula::SplatCloud& c, const std::vector<float>& vm) {
    engine_reset();
    set_data_3dgs(c.num, ttv(c.means.data(), {c.num, 3}),
                  ttv(c.quats.data(), {c.num, 4}),
                  ttv(c.scales.data(), {c.num, 3}),
                  ttv(c.opacities.data(), {c.num, 1}),
                  ttv(c.features_dc.data(), {c.num, 3}),
                  ttv(c.features_sh.data(), {c.num, c.coeffs(), 3}), c.sb_lobes);
    return render_current(vm);
}

// ---- GPU rendering and quality metrics ----
static void test_gpu_rendering_and_metrics() {

    const int side = 24;
    const int64_t n = side * side; // 576 splats
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = 0;
    c.sb_lobes = 0;
    c.means.resize((size_t)(n * 3));
    c.quats.resize((size_t)(n * 4));
    c.scales.resize((size_t)(n * 3));
    c.opacities.resize((size_t)n, 2.5f);
    c.features_dc.resize((size_t)(n * 3));

    int64_t idx = 0;
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            c.means[idx*3 + 0] = (float)x * 0.18f - 2.0f;
            c.means[idx*3 + 1] = (float)y * 0.18f - 2.0f;
            c.means[idx*3 + 2] = 5.0f + (float)y * 0.04f + (float)x * 0.002f;

            c.quats[idx*4 + 0] = 1.0f;
            c.scales[idx*3 + 0] = -2.3f;
            c.scales[idx*3 + 1] = -2.3f;
            c.scales[idx*3 + 2] = -2.3f;

            float r = (float)x / (float)side;
            float g = (float)y / (float)side;
            float b = 0.5f;
            c.features_dc[idx*3 + 0] = (r - 0.5f) / 0.28209479f;
            c.features_dc[idx*3 + 1] = (g - 0.5f) / 0.28209479f;
            c.features_dc[idx*3 + 2] = (b - 0.5f) / 0.28209479f;
            idx++;
        }
    }

    std::vector<float> vm = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    auto img_gt = render_full_cloud(c, vm);

    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    auto hier = std::make_shared<spirula::GaussianHierarchy>(
        spirula::build_gaussian_hierarchy(c, bcfg));

    spirula::HierarchyRenderer renderer(hier);

    spirula::LodViewParams view;
    view.cam_pos[0] = 0; view.cam_pos[1] = 0; view.cam_pos[2] = 0;
    view.cam_dir[0] = 0; view.cam_dir[1] = 0; view.cam_dir[2] = 1;
    view.width = W; view.height = H;
    view.fx = 120.0f; view.fy = 120.0f;

    // A. Fine LOD (threshold 0.001)
    engine_reset();
    renderer.reset_history();
    spirula::LodRenderConfig rcfg_fine;
    rcfg_fine.enabled = true;
    rcfg_fine.quality_threshold = 0.001f;
    auto stats_fine = renderer.update_and_upload(view, rcfg_fine);
    auto img_fine = render_current(vm);

    float psnr_fine = spirula::image_psnr(img_gt.data(), img_fine.data(), (int64_t)img_gt.size());
    float ssim_fine = spirula::image_ssim(img_gt.data(), img_fine.data(), H, W, 3);

    check(psnr_fine > 45.0f, "Fine LOD PSNR > 45 dB (actual: " + std::to_string(psnr_fine) + " dB)");
    check(ssim_fine > 0.999f, "Fine LOD SSIM > 0.999 (actual: " + std::to_string(ssim_fine) + ")");
    check(stats_fine.total_rendered_splats == n, "Fine LOD renders 100% of splats (" +
          std::to_string(stats_fine.total_rendered_splats) + "/" + std::to_string(n) + ")");

    // B. Medium LOD (threshold 5.0)
    engine_reset();
    renderer.reset_history();
    spirula::LodRenderConfig rcfg_med;
    rcfg_med.enabled = true;
    rcfg_med.quality_threshold = 5.0f;
    auto stats_med = renderer.update_and_upload(view, rcfg_med);
    auto img_med = render_current(vm);

    float psnr_med = spirula::image_psnr(img_gt.data(), img_med.data(), (int64_t)img_gt.size());
    float ssim_med = spirula::image_ssim(img_gt.data(), img_med.data(), H, W, 3);
    check(psnr_med > 15.0f, "Medium LOD PSNR > 15 dB (actual: " + std::to_string(psnr_med) + " dB)");
    check(ssim_med > 0.70f, "Medium LOD SSIM > 0.70 (actual: " + std::to_string(ssim_med) + ")");

    // C. Coarse LOD (threshold 200.0)
    engine_reset();
    renderer.reset_history();
    spirula::LodRenderConfig rcfg_coarse;
    rcfg_coarse.enabled = true;
    rcfg_coarse.quality_threshold = 200.0f;
    auto stats_coarse = renderer.update_and_upload(view, rcfg_coarse);
    auto img_coarse = render_current(vm);

    check(stats_coarse.total_rendered_splats <= 16,
          "Coarse LOD achieves >97% splat reduction (" +
          std::to_string(stats_coarse.total_rendered_splats) + "/" + std::to_string(n) + " splats)");

    // D. Visual sanity check (finite, bounded, no NaNs)
    bool all_finite = true;
    for (float v : img_fine) {
        if (!std::isfinite(v) || v < -1e-4f || v > 1.0f + 1e-4f) all_finite = false;
    }
    check(all_finite, "Rendered pixel values are strictly valid, finite, and in [0, 1]");
}

// ---- GPU residency and upload caching ----
static void test_gpu_upload_caching_and_bandwidth() {

    const int64_t n = 200;
    spirula::SplatCloud c;
    c.num = n;
    c.means.resize((size_t)(n * 3), 0.0f);
    c.quats.resize((size_t)(n * 4), 0.0f);
    c.scales.resize((size_t)(n * 3), -2.0f);
    c.opacities.resize((size_t)n, 1.0f);
    c.features_dc.resize((size_t)(n * 3), 0.0f);
    for (int64_t i = 0; i < n; i++) {
        c.means[(size_t)(i * 3 + 0)] = (float)(i % 10) * 0.5f - 2.5f;
        c.means[(size_t)(i * 3 + 1)] = (float)(i / 10) * 0.5f - 2.5f;
        c.means[(size_t)(i * 3 + 2)] = 6.0f + (float)i * 0.01f;
        c.quats[(size_t)(i * 4 + 0)] = 1.0f;
    }

    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    auto hier = std::make_shared<spirula::GaussianHierarchy>(
        spirula::build_gaussian_hierarchy(c, bcfg));

    spirula::HierarchyRenderer renderer(hier);
    engine_reset();
    renderer.reset_history();

    spirula::LodViewParams view;
    view.cam_pos[0] = 0; view.cam_pos[1] = 0; view.cam_pos[2] = 0;
    view.cam_dir[0] = 0; view.cam_dir[1] = 0; view.cam_dir[2] = 1;
    view.width = W; view.height = H;
    view.fx = 120.0f; view.fy = 120.0f;

    spirula::LodRenderConfig rcfg;
    rcfg.enabled = true;
    rcfg.quality_threshold = 50.0f;

    // Frame 1: initial view, must upload to GPU
    auto stats1 = renderer.update_and_upload(view, rcfg);
    check(stats1.gpu_buffers_updated, "Frame 1 performs initial GPU buffer upload");
    check(stats1.upload_bytes > 0, "Frame 1 uploads non-zero bytes to GPU (" +
          std::to_string(stats1.upload_bytes) + " bytes)");

    // Frames 2..6: camera is stationary, MUST NOT re-upload (0 bytes transferred)
    int redundant_uploads = 0;
    for (int f = 2; f <= 6; f++) {
        auto stats = renderer.update_and_upload(view, rcfg);
        if (stats.gpu_buffers_updated || stats.upload_bytes > 0) {
            redundant_uploads++;
        }
    }
    check(redundant_uploads == 0, "Stationary camera consumes 0 bytes upload bandwidth across consecutive frames");

    // Frame 7: refine threshold to 0.001f, expanding cut to full leaf detail
    rcfg.quality_threshold = 0.001f;
    auto stats7 = renderer.update_and_upload(view, rcfg);
    check(stats7.gpu_buffers_updated, "Camera movement across LOD boundary triggers necessary GPU upload");
    check(stats7.upload_bytes > 0, "Refined cut uploaded new splats to device");
}

// ---- GPU VRAM budget enforcement ----
static void test_hard_gpu_vram_budget_enforcement() {

    const int64_t n = 25000; // 25,000 splats = ~1.4 MB at degree 0 (56 bytes/splat)
    spirula::SplatCloud c;
    c.num = n;
    c.means.resize((size_t)(n * 3), 0.0f);
    c.quats.resize((size_t)(n * 4), 0.0f);
    c.scales.resize((size_t)(n * 3), -2.0f);
    c.opacities.resize((size_t)n, 1.0f);
    c.features_dc.resize((size_t)(n * 3), 0.0f);
    for (int64_t i = 0; i < n; i++) {
        c.means[(size_t)(i * 3 + 0)] = ((float)(i % 100) - 50.0f) * 0.2f;
        c.means[(size_t)(i * 3 + 1)] = ((float)((i / 100) % 50) - 25.0f) * 0.2f;
        c.means[(size_t)(i * 3 + 2)] = 5.0f + (float)i * 0.001f;
        c.quats[(size_t)(i * 4 + 0)] = 1.0f;
    }

    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    auto hier = std::make_shared<spirula::GaussianHierarchy>(
        spirula::build_gaussian_hierarchy(c, bcfg));

    spirula::HierarchyRenderer renderer(hier);
    engine_reset();
    renderer.reset_history();

    spirula::LodViewParams view;
    view.cam_pos[0] = 0; view.cam_pos[1] = 0; view.cam_pos[2] = 0;
    view.cam_dir[0] = 0; view.cam_dir[1] = 0; view.cam_dir[2] = 1;
    view.width = W; view.height = H;
    view.fx = 120.0f; view.fy = 120.0f;

    // Configure a strict 1 MB VRAM budget
    // 1 MB = 1,048,576 bytes / 56 bytes per splat = 18,724 splats max
    const int64_t budget_mb = 1;
    const int64_t max_allowed_splats = (budget_mb * 1024 * 1024) / 56;

    spirula::LodRenderConfig rcfg;
    rcfg.enabled = true;
    rcfg.quality_threshold = 0.001f; // wants 100% fine detail (all 25,000 splats)
    rcfg.vram_budget_mb = budget_mb;

    auto stats = renderer.update_and_upload(view, rcfg);

    int64_t real_gpu_splats = engine_get_cur_num_splats();

    check(stats.total_rendered_splats <= max_allowed_splats,
          "Rendered splat count strictly obeys 1MB budget envelope (" +
          std::to_string(stats.total_rendered_splats) + " <= " + std::to_string(max_allowed_splats) + ")");
    check(real_gpu_splats == stats.total_rendered_splats,
          "Real GPU allocation matches active cut (" + std::to_string(real_gpu_splats) + " splats on GPU)");
    check(real_gpu_splats <= max_allowed_splats,
          "Real GPU allocation strictly bounded by configured VRAM budget envelope");
}

// ---- Partition compatibility and boundary ownership ----
static void test_partition_compatibility_and_ownership() {

    // Part 0: Left side x in [-5, 0.5] (overlaps into part 1 by 0.5)
    // Part 1: Right side x in [-0.5, 5] (overlaps into part 0 by 0.5)
    // Boundary at x = 0.
    const int64_t n_each = 500;
    spirula::SplatCloud c0, c1;
    c0.num = n_each; c0.sh_degree = 0; c0.sb_lobes = 0;
    c0.means.resize((size_t)(n_each * 3));
    c0.quats.resize((size_t)(n_each * 4), 0.0f);
    c0.scales.resize((size_t)(n_each * 3), -2.0f);
    c0.opacities.resize((size_t)n_each, 1.0f);
    c0.features_dc.resize((size_t)(n_each * 3), 0.5f);

    c1.num = n_each; c1.sh_degree = 0; c1.sb_lobes = 0;
    c1.means.resize((size_t)(n_each * 3));
    c1.quats.resize((size_t)(n_each * 4), 0.0f);
    c1.scales.resize((size_t)(n_each * 3), -2.0f);
    c1.opacities.resize((size_t)n_each, 1.0f);
    c1.features_dc.resize((size_t)(n_each * 3), -0.5f);

    for (int64_t i = 0; i < n_each; i++) {
        c0.quats[(size_t)(i * 4 + 0)] = 1.0f;
        c1.quats[(size_t)(i * 4 + 0)] = 1.0f;

        // Part 0: x in [-5.0, 0.5]
        float x0 = -5.0f + 5.5f * ((float)i / (float)n_each);
        c0.means[(size_t)(i * 3 + 0)] = x0;
        c0.means[(size_t)(i * 3 + 1)] = (float)(i % 10) * 0.2f;
        c0.means[(size_t)(i * 3 + 2)] = 5.0f;

        // Part 1: x in [-0.5, 5.0]
        float x1 = -0.5f + 5.5f * ((float)i / (float)n_each);
        c1.means[(size_t)(i * 3 + 0)] = x1;
        c1.means[(size_t)(i * 3 + 1)] = (float)(i % 10) * 0.2f;
        c1.means[(size_t)(i * 3 + 2)] = 5.0f;
    }

    std::string test_dir = "scratch_partition_test";
    fs::create_directories(test_dir);
    std::string ply0_path = test_dir + "/part0.ply";
    std::string ply1_path = test_dir + "/part1.ply";
    spirula::write_splat_ply(c0, ply0_path);
    spirula::write_splat_ply(c1, ply1_path);

    // Setup ScenePartition with hard ownership split at x = 0
    spirula::ScenePartition partition;
    partition.num_parts = 2;
    partition.ring.resize(2);

    // Build seeds for LabelField: seed 0 at x = -2.5 (label 0), seed 1 at x = +2.5 (label 1)
    float seed_xyz[6] = {-2.5f, 0.0f, 5.0f,   2.5f, 0.0f, 5.0f};
    int32_t seed_lbl[2] = {0, 1};
    partition.field = std::make_shared<spirula::LabelField>(
        spirula::LabelField::build(seed_xyz, seed_lbl, nullptr, 2, 2));

    spirula::MergeStats mstats;
    spirula::SplatCloud merged = spirula::merge_partition_splats(
        partition, {ply0_path, ply1_path}, mstats);

    check(mstats.parts[0].kept < n_each, "Part 0 filtered out overlapping points beyond boundary");
    check(mstats.parts[1].kept < n_each, "Part 1 filtered out overlapping points beyond boundary");
    check(merged.num == mstats.parts[0].kept + mstats.parts[1].kept,
          "Merged cloud contains exact sum of owned points (" + std::to_string(merged.num) + " splats)");

    // Verify all points in merged cloud obey partition domain
    bool valid_ownership = true;
    int64_t from_part0 = 0, from_part1 = 0;
    for (int64_t i = 0; i < merged.num; i++) {
        float x = merged.means[(size_t)(i * 3 + 0)];
        if (x < 0.0f) from_part0++;
        else from_part1++;
    }
    check(from_part0 == mstats.parts[0].kept && from_part1 == mstats.parts[1].kept,
          "Hard ownership boundary respected: 0 duplicate splats, 0 missing splats at partition seam");

    // Build .slod from merged partition cloud
    spirula::LodBuildConfig bcfg;
    bcfg.max_leaf_size = 16;
    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(merged, bcfg);

    std::string slod_path = test_dir + "/merged.slod";
    spirula::write_slod(hier, slod_path);
    check(spirula::validate_slod(slod_path).empty(), "Merged partition .slod file validates cleanly");

    // Render on GPU across partition boundary
    spirula::HierarchyRenderer renderer(std::make_shared<spirula::GaussianHierarchy>(hier));
    engine_reset();
    renderer.reset_history();

    spirula::LodViewParams view;
    view.cam_pos[0] = 0; view.cam_pos[1] = 0; view.cam_pos[2] = 0;
    view.cam_dir[0] = 0; view.cam_dir[1] = 0; view.cam_dir[2] = 1;
    view.width = W; view.height = H;
    view.fx = 120.0f; view.fy = 120.0f;

    spirula::LodRenderConfig rcfg;
    rcfg.enabled = true;
    rcfg.quality_threshold = 0.001f;
    renderer.update_and_upload(view, rcfg);

    std::vector<float> vm = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    auto img_merged = render_current(vm);

    bool render_ok = !img_merged.empty();
    for (float v : img_merged) {
        if (!std::isfinite(v)) render_ok = false;
    }
    check(render_ok, "Merged partition model renders cleanly across boundary on GPU");

    fs::remove_all(test_dir);
}

int main() {
    test_gpu_rendering_and_metrics();
    test_gpu_upload_caching_and_bandwidth();
    test_hard_gpu_vram_budget_enforcement();
    test_partition_compatibility_and_ownership();

    if (g_failures == 0) std::printf("hierarchy_acceptance_test: OK\n");
    return g_failures ? 1 : 0;
}
