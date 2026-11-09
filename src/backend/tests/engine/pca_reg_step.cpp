// PCA opacity gradients must reach each optimizer without counting as camera observations.
#include "engine/Engine.h"
#include "engine/EngineState.h"
#include "core/SplatVisitState.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
TorchTensorView view(const void* p, std::vector<int64_t> shape) {
    return TorchTensorView((uint64_t)p, 4u, std::move(shape));
}

bool run(const char* primitive, bool fused, bool quantized) {
    engine_reset();
    constexpr int n = 14, width = 32, height = 32;
    std::vector<float> means, quats, scales, opacities(n, 0), dc(n * 3, 0.5f), sh(n * 9, 0);
    const float points[7][2] = {{0,0}, {0.3f,0}, {-0.3f,0}, {0,0.4f}, {0,-0.4f}, {0.3f,0.4f}, {-0.3f,-0.4f}};
    for (int i = 0; i < n; i++) {
        means.insert(means.end(), {points[i % 7][0] + (i < 7 ? 0 : 40), points[i % 7][1], 3.0f});
        quats.insert(quats.end(), {1,0,0,0});
        scales.insert(scales.end(), {-1,-1.2f,-2});
    }
    set_data_3dgs(n, view(means.data(), {n,3}), view(quats.data(), {n,4}),
        view(scales.data(), {n,3}), view(opacities.data(), {n,1}), view(dc.data(), {n,3}),
        view(sh.data(), {n,3,3}), 0);
    EngineStepConfig cfg{};
    cfg.optim.use_fused_proj_bwd_optim = fused;
    cfg.optim.quantization_level = quantized ? 1 : 0;
    cfg.optim.keep_fp32_grad = true;
    cfg.optim.sh_value_bits = quantized ? 16 : 32;
    cfg.optim.sh_optim_bits = quantized ? 8 : 32;
    cfg.optim.non_sh_optim_bits = quantized ? 16 : 32;
    cfg.optim.lr_opacities = 0.02f;
    cfg.optim.reg_rendered_only = true;
    cfg.pca_reg.neighbors = 6;
    cfg.pca_reg.knn = PcaRegKnn::Exact;
    cfg.pca_reg.w_transparency = 0.01f;
    cfg.pca_reg.covariance = PcaRegCovariance::GaussianMixture;
    cfg.densify.refine_start_iter = 10000;
    cfg.densify.refine_every = 100;
    cfg.densify.max_screen_size = std::numeric_limits<float>::infinity();
    cfg.densify.max_world_size = std::numeric_limits<float>::infinity();
    const float camera[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const float intrinsics[4] = {30,30,16,16};
    const float distortion[kCameraDistortionParams] = {};
    std::vector<float> rgb(width * height * 3, 0);
    const auto empty = view(nullptr, {0});
    for (int step = 1; step <= 2; step++)
        engine_train_step(step, 100, primitive, 1, false, width, height, "PINHOLE", "NONE",
            view(camera, {1,4,4}), view(intrinsics, {1,4}), view(distortion, {1,kCameraDistortionParams}),
            view(rgb.data(), {1,height,width,3}), empty, empty, empty, empty, cfg);
    backend::device_synchronize();
    backend::memcpy_sync(opacities.data(), engine().world.opacities.data_ptr(), n * sizeof(float),
                         backend::MemcpyKind::DeviceToHost);
    std::vector<uint32_t> visits(n);
    backend::memcpy_sync(visits.data(), engine().optim.visit_counters.data_ptr(), n * sizeof(uint32_t),
                         backend::MemcpyKind::DeviceToHost);
    bool ok = true;
    for (int i = 0; i < n; i++)
        ok &= std::isfinite(opacities[i]) && opacities[i] > 0.01f && opacities[i] < 0.1f &&
              visit_renders(visits[i]) == 0 && visit_unrendered_steps(visits[i]) == 2;
    std::printf("%s %s fused=%d quantized=%d: opacity updates, no photometric observations\n",
                ok ? "ok" : "FAIL", primitive, fused, quantized);
    if (!ok) for (int i = 0; i < n; i++)
        std::printf("  %d opacity=%g visits=%u\n", i, opacities[i], visits[i]);
    return ok;
}
}  // namespace

int main() {
    bool ok = true;
    for (const char* primitive : {"3dgs", "mip", "3dgut"}) {
        ok &= run(primitive, false, false);
        ok &= run(primitive, false, true);
        ok &= run(primitive, true, false);
        ok &= run(primitive, true, true);
    }
    engine_reset();
    if (const char* error = backend::last_error()) { std::fprintf(stderr, "%s\n", error); ok = false; }
    return ok ? 0 : 1;
}
