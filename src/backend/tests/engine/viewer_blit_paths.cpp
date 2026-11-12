// RenderWorker's device-side blit (engine_blit_render) against its host
// round trip: same bytes, same double-click pick. relative_scale = 1 changes
// nothing but forces the host path.
#include "engine/Engine.h"
#include "app/webviewer/RenderWorker.h"

#include <cmath>
#include <cstdio>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

TorchTensorView tvf(std::vector<float>& v, std::vector<int64_t> s) {
    return {(uint64_t)(uintptr_t)v.data(), 4, std::move(s)};
}
TorchTensorView tvi(std::vector<int32_t>& v, std::vector<int64_t> s) {
    return {(uint64_t)(uintptr_t)v.data(), 4, std::move(s)};
}

ViewResult render(RenderWorker& w, bool grid) {
    ViewRequest q;
    const float eye[3] = {0.0f, -2.2f, 1.2f};
    // Looking at the origin, z up (OpenGL camera axes in the columns).
    const float f[3] = {0.0f, 2.2f / std::sqrt(2.2f * 2.2f + 1.44f),
                        -1.2f / std::sqrt(2.2f * 2.2f + 1.44f)};
    const float x[3] = {1.0f, 0.0f, 0.0f};
    const float y[3] = {0.0f, -f[2], f[1]};
    for (int r = 0; r < 3; r++) {
        q.c2w[r * 4 + 0] = x[r];
        q.c2w[r * 4 + 1] = y[r];
        q.c2w[r * 4 + 2] = -f[r];
        q.c2w[r * 4 + 3] = eye[r];
    }
    q.W = 320;
    q.H = 240;
    q.fx = q.fy = 260.0f;
    q.cx = 160.0f;
    q.cy = 120.0f;
    q.show_grid = grid;
    q.grid_dist = 2.5f;
    q.pick_px = 160;
    q.pick_py = 120;
    ViewResult r;
    require(w.wait_result(w.submit(q), r, 120.0), "render timed out");
    require(r.error.empty(), "render failed: " + r.error);
    return r;
}
}  // namespace

int main() {
    try {
        engine_reset();
        const int64_t n = 30000;
        std::mt19937 rng(3);
        std::uniform_real_distribution<float> u(-1.f, 1.f);
        std::vector<float> means(n * 3), quats(n * 4, 0.f), scales(n * 3), op(n), dc(n * 3),
            sh(n * 3 * 3);
        for (int64_t i = 0; i < n; i++) {
            for (int a = 0; a < 3; a++) means[3 * i + a] = 0.6f * u(rng);
            quats[4 * i] = 1;
            for (int a = 0; a < 3; a++) scales[3 * i + a] = -3.5f + 0.5f * u(rng);
            op[i] = 2.0f * u(rng);
            for (int a = 0; a < 3; a++) dc[3 * i + a] = u(rng);
        }
        for (auto& v : sh) v = 0.2f * u(rng);
        engine_scene_set_data_3dgs(0, n, tvf(means, {n, 3}), tvf(quats, {n, 4}),
                                   tvf(scales, {n, 3}), tvf(op, {n, 1}), tvf(dc, {n, 3}),
                                   tvf(sh, {n, 3, 3}), 0);
        std::vector<float> intrins{32, 32, 32, 32}, dist(kCameraDistortionParams, 0.f),
            c2w{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        std::vector<int32_t> models{0}, ws{64}, hs{64}, tiers{0};
        engine_viewer_init(tvi(models, {1}), tvf(intrins, {1, 4}),
                           tvf(dist, {1, kCameraDistortionParams}), tvi(tiers, {1}),
                           tvf(c2w, {1, 3, 4}), tvi(ws, {1}), tvi(hs, {1}), 1e-3f);
        engine_viewer_set_grid(1.0f, 2.5f);

        std::mutex mu;
        ViewerHooks hooks;
        hooks.engine_mutex = &mu;
        hooks.current_step = [] { return 1 << 20; };
        hooks.set_render_pending = [](bool) {};
        ViewerRenderConfig cfg;
        cfg.primitive = "3dgs";
        cfg.packed = true;
        cfg.sh_degree_warmup_every = 1;
        cfg.scene_slot = 0;
        ViewerRenderConfig host_cfg = cfg;
        host_cfg.relative_scale = 1.0f;
        RenderWorker device_w, host_w;
        device_w.start(cfg, hooks);
        host_w.start(host_cfg, hooks);

        const ViewResult d = render(device_w, true);
        const ViewResult h = render(host_w, true);
        require(render(device_w, false).rgb8 != d.rgb8, "the grid drew nothing");
        size_t diff = 0;
        for (size_t i = 0; i < d.rgb8.size(); i++) diff += d.rgb8[i] != h.rgb8[i];
        require(d.rgb8.size() == h.rgb8.size() && diff == 0,
                std::to_string(diff) + " bytes differ between the device and host blits");
        require(d.pick_hit && h.pick_hit, "the pick missed the model");
        for (int k = 0; k < 3; k++)
            require(std::fabs(d.pick_point[k] - h.pick_point[k]) <= 1e-6f,
                    "the device pick differs from the host pick");
        device_w.stop();
        host_w.stop();
        engine_reset();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "viewer_blit_paths: %s\n", e.what());
        return 1;
    }
    std::puts("viewer_blit_paths: OK");
}
