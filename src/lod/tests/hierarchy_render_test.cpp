// hierarchy_render_test.cpp -- end-to-end GPU rasterization & image quality validation.

#include "checkpoint/SplatPly.h"
#include "engine/Engine.h"
#include "engine/EngineState.h"
#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyRenderer.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

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

static double compute_psnr(const std::vector<float>& imgA, const std::vector<float>& imgB) {
    if (imgA.size() != imgB.size() || imgA.empty()) return 0.0;
    double mse = 0.0;
    for (size_t i = 0; i < imgA.size(); i++) {
        double diff = (double)imgA[i] - (double)imgB[i];
        mse += diff * diff;
    }
    mse /= (double)imgA.size();
    if (mse < 1e-12) return 100.0;
    return 10.0 * std::log10(1.0 / mse);
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

// ---------------------------------------------------------------------------
// Test: Coplanar Grid & Multi-Level Approximation
// ---------------------------------------------------------------------------
static void test_coplanar_grid_render() {
    std::printf("--- Test: Coplanar Gaussian Grid Multi-Level Render ---\n");

    const int side = 20;
    const int64_t n = side * side; // 400 splats
    spirula::SplatCloud c;
    c.num = n;
    c.sh_degree = 0;
    c.sb_lobes = 0;
    c.means.resize((size_t)(n * 3));
    c.quats.resize((size_t)(n * 4));
    c.scales.resize((size_t)(n * 3));
    c.opacities.resize((size_t)n, 2.0f); // opaque
    c.features_dc.resize((size_t)(n * 3));

    int64_t idx = 0;
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            c.means[idx*3 + 0] = (float)x * 0.2f - 2.0f;
            c.means[idx*3 + 1] = (float)y * 0.2f - 2.0f;
            c.means[idx*3 + 2] = 5.0f + (float)y * 0.05f + (float)x * 0.002f;

            c.quats[idx*4 + 0] = 1.0f;
            c.scales[idx*3 + 0] = -2.2f;
            c.scales[idx*3 + 1] = -2.2f;
            c.scales[idx*3 + 2] = -2.2f;

            // Checkerboard color
            float col = ((x + y) % 2 == 0) ? 0.8f : -0.8f;
            c.features_dc[idx*3 + 0] = col;
            c.features_dc[idx*3 + 1] = col;
            c.features_dc[idx*3 + 2] = col;
            idx++;
        }
    }

    std::vector<float> vm = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    auto img_orig = render_full_cloud(c, vm);

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

    // ---- Max detail ----
    engine_reset();
    spirula::LodRenderConfig rcfg_max;
    rcfg_max.enabled = true;
    rcfg_max.quality_threshold = 0.001f;
    auto stats_max = renderer.update_and_upload(view, rcfg_max);
    auto img_max = render_current(vm);

    double psnr_max = compute_psnr(img_orig, img_max);
    check(psnr_max > 45.0,
          "maximum-detail LOD achieves near-perfect fidelity to original model (PSNR: " +
          std::to_string(psnr_max) + " dB)");

    // ---- Medium detail ----
    engine_reset();
    spirula::LodRenderConfig rcfg_med;
    rcfg_med.enabled = true;
    rcfg_med.quality_threshold = 5.0f;
    auto stats_med = renderer.update_and_upload(view, rcfg_med);
    auto img_med = render_current(vm);

    double psnr_med = compute_psnr(img_orig, img_med);
    check(psnr_med > 15.0 && stats_med.total_rendered_splats > 1,
          "medium-detail LOD provides faithful representation (PSNR: " +
          std::to_string(psnr_med) + " dB, splats: " +
          std::to_string(stats_med.total_rendered_splats) + "/" + std::to_string(n) + ")");

    // ---- Coarse detail ----
    engine_reset();
    spirula::LodRenderConfig rcfg_coarse;
    rcfg_coarse.enabled = true;
    rcfg_coarse.quality_threshold = 500.0f;
    auto stats_coarse = renderer.update_and_upload(view, rcfg_coarse);
    auto img_coarse = render_current(vm);

    check(stats_coarse.total_rendered_splats <= 16,
          "coarse LOD achieves massive splat count reduction (rendered: " +
          std::to_string(stats_coarse.total_rendered_splats) + "/" + std::to_string(n) + ")");

    bool all_finite = true;
    for (float v : img_coarse) if (!std::isfinite(v)) all_finite = false;
    check(all_finite, "rendered pixels are all finite and free of NaNs");
}

int main() {
    test_coplanar_grid_render();

    if (g_failures == 0) std::printf("hierarchy_render_test: OK\n");
    return g_failures ? 1 : 0;
}
