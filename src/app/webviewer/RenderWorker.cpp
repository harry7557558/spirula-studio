// RenderWorker.cpp -- see RenderWorker.h.

#include "app/webviewer/RenderWorker.h"

#include "backend/api/BackendRuntime.h"
#include "engine/Engine.h"    // engine render + viewer entry points
#include "core/Camera.h"    // camera_model_from_name
#include "app/DepthColor.h"  // error_scale -- shared with the error pane
#include "data/FrustumSize.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

TorchTensorView tvp(const void* p, uint32_t elem_size, std::vector<int64_t> shape) {
    return {(uint64_t)(uintptr_t)p, elem_size, std::move(shape)};
}
TorchTensorView tv_null() { return {0, 0, {}}; }

// Grow-only device buffer (viewer scratch; a handful per worker, so no pool).
struct DevBuf {
    void* ptr = nullptr;
    size_t cap = 0;
    void* ensure(size_t bytes) {
        if (bytes > cap) {
            if (ptr) backend::device_free(ptr);
            ptr = nullptr;
            cap = 0;
            ptr = backend::device_malloc_checked(bytes, "viewer buffer");
            cap = bytes;
        }
        return ptr;
    }
    void* upload(const void* host, size_t bytes) {
        void* p = ensure(bytes);
        backend::memcpy_sync(p, host, bytes, backend::MemcpyKind::HostToDevice);
        if (backend::last_error())
            throw std::runtime_error("viewer: H2D copy failed");
        return p;
    }
    ~DevBuf() { if (ptr) backend::device_free(ptr); }
};

struct PendingReq {
    uint64_t id = 0;
    ViewRequest q;
};

}  // namespace


// ===========================================================================
// Impl
// ===========================================================================

struct RenderWorker::Impl {
    ViewerRenderConfig cfg;
    ViewerHooks hooks;

    std::thread worker;
    std::atomic<bool> running{false};
    std::mutex mu;
    std::condition_variable cv_work, cv_result;
    bool has_pending = false;
    PendingReq pending;
    ViewResult result;
    uint64_t next_id = 1;

    // Device scratch (worker thread only).
    DevBuf d_buffer, d_depth, d_alpha, d_out;
    DevBuf d_intr, d_vm, d_dist, d_normals, d_depth_in;

    // ---- render worker -----------------------------------------------------

    uint64_t submit(const ViewRequest& q) {
        std::unique_lock<std::mutex> lk(mu);
        pending.q = q;
        pending.id = next_id++;
        has_pending = true;
        uint64_t id = pending.id;
        lk.unlock();
        // Synchronously flip the "render desired" flag so the training loop
        // yields at its next iteration boundary.
        if (hooks.set_render_pending) hooks.set_render_pending(true);
        cv_work.notify_one();
        return id;
    }

    bool wait_result(uint64_t id, ViewResult& out, double timeout_s) {
        std::unique_lock<std::mutex> lk(mu);
        bool ok = cv_result.wait_for(lk, std::chrono::duration<double>(timeout_s),
                                     [&] { return result.id == id; });
        if (ok) out = result;
        return ok;
    }

    bool try_get_result(uint64_t id, ViewResult& out) {
        std::lock_guard<std::mutex> lk(mu);
        if (result.id != id) return false;
        out = result;
        return true;
    }

    void worker_loop() {
        while (running) {
            PendingReq p;
            {
                std::unique_lock<std::mutex> lk(mu);
                cv_work.wait(lk, [&] { return has_pending || !running; });
                if (!running) break;
                p = pending;
                has_pending = false;
            }
            ViewResult res;
            res.id = p.id;
            res.W = p.q.W;
            res.H = p.q.H;
            const auto t0 = std::chrono::steady_clock::now();
            try {
#ifndef SS_BACKEND_VULKAN
                // CUDA current-device state is per thread; bind before device work.
                if (!backend::device_bind())
                    throw std::runtime_error(
                        "viewer: the selected GPU is not usable on this thread");
#endif
                res.rgb8 = render_once(p.q, res);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[viewer] render error: %s\n", e.what());
                res.error = e.what();
            }
            res.secs = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t0).count();
            {
                std::lock_guard<std::mutex> lk(mu);
                result = std::move(res);
            }
            cv_result.notify_all();
            {
                std::lock_guard<std::mutex> lk(mu);
                if (!has_pending && hooks.set_render_pending)
                    hooks.set_render_pending(false);
            }
        }
    }

    // ---- the actual render (trainer._render + get_outputs viewer subset) ---

    std::vector<uint8_t> render_once(const ViewRequest& q, ViewResult& res) {
        const int W = q.W, H = q.H;
        const int64_t npx = (int64_t)W * H;
        static const float D[3] = {1.f, -1.f, -1.f};

        // Client c2w is in the legacy normalized frame; remap to the training
        // frame. T is a similarity (scale*R | t): full similarity on the
        // position, unit rotation on the basis.
        float c2w[12];
        std::memcpy(c2w, q.c2w, sizeof c2w);
        if (cfg.train_frame_scale != 1.0f) {
            const auto& T = cfg.train_to_normalized;
            double s = std::sqrt((double)T[0]*T[0] + (double)T[4]*T[4] + (double)T[8]*T[8]);
            float out[12];
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    double v = 0.0;
                    for (int m = 0; m < 3; m++)
                        v += (double)T[r*4 + m] / s * q.c2w[m*4 + c];
                    out[r*4 + c] = (float)v;
                }
                double t = T[r*4 + 3];
                for (int m = 0; m < 3; m++) t += (double)T[r*4 + m] * q.c2w[m*4 + 3];
                out[r*4 + 3] = (float)t;
            }
            std::memcpy(c2w, out, sizeof c2w);
        }
        // Grid overlay inputs, mapped like the c2w: distance scales by the
        // similarity, the orbit target maps through it.
        float grid_dist = q.grid_dist * cfg.train_frame_scale;
        float grid_target[3] = {q.grid_target[0], q.grid_target[1],
                                q.grid_target[2]};
        if (cfg.train_frame_scale != 1.0f) {
            const auto& T = cfg.train_to_normalized;
            for (int r = 0; r < 3; r++)
                grid_target[r] = T[r*4+0]*q.grid_target[0] +
                                 T[r*4+1]*q.grid_target[1] +
                                 T[r*4+2]*q.grid_target[2] + T[r*4+3];
        }

        // c2w -> engine viewmat.
        double t[3] = {c2w[3], c2w[7], c2w[11]};
        if (cfg.relative_scale.has_value())
            for (auto& v : t) v *= *cfg.relative_scale;
        double Rf[3][3];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                Rf[r][c] = c2w[r*4 + c] * D[c];
        float vm[16] = {0};
        for (int r = 0; r < 3; r++) {
            double ti = 0.0;
            for (int c = 0; c < 3; c++) {
                vm[r*4 + c] = (float)Rf[c][r];
                ti -= Rf[c][r] * t[c];
            }
            vm[r*4 + 3] = (float)ti;
        }
        vm[15] = 1.f;

        float intr[4] = {q.fx, q.fy, q.cx, q.cy};
        float dist0[8] = {0};
        // Distortion images are FULL render resolution (never-freed pool
        // slots) -- emit them only when the user is actually LOOKING at a
        // distortion buffer, and only offer those buffers when a distortion
        // regularizer is configured (buffer_keys). Zero reg weights = zero
        // extra VRAM.
        bool want_dist = cfg.distortion_reg_on &&
                         q.key.find("distortion") != std::string::npos;
        bool want_median = cfg.output_median;
        int sh_deg = q.sh_degree >= 0
            ? q.sh_degree
            : (hooks.current_step
                   ? hooks.current_step() / std::max(cfg.sh_degree_warmup_every, 1)
                   : 100);
        const std::string& primitive =
            q.primitive.empty() ? cfg.primitive : q.primitive;

        const bool want_raw = cfg.color_space_on && q.key == "rgb_raw";
        std::vector<float> rgb(npx * 3), depth(npx), Ts(npx);
        std::vector<float> median(want_median ? npx : 0);
        std::vector<float> rgbd, depthd;
        std::vector<float> rgb_raw(want_raw ? npx * 3 : 0);

        std::vector<uint8_t> out_host(npx * 3);
        {
            // Engine access: exclusive with the training step. Everything up
            // to the annotated-u8 download stays under the lock, mirroring
            // Trainer.render's "post-processor + D->H inside the lock".
            std::lock_guard<std::mutex> lk(*hooks.engine_mutex);

            // Several models can be resident at once (a comparison viewer);
            // binding one is a pointer swap, so it is done per render rather
            // than by whoever last touched the engine.
            if (cfg.scene_slot >= 0) engine_scene_activate(cfg.scene_slot);

            set_camera_params(W, H, q.model, "NONE",
                              tvp(vm, 4, {1, 4, 4}),
                              tvp(intr, 4, {1, 4}),
                              tvp(dist0, 4, {1, 8}));
            forward_3dgs(primitive, sh_deg, cfg.packed,
                         want_median, want_dist ? 2 : 0);
            engine_copy_render_to_host(
                tvp(rgb.data(), 4, {1, H, W, 3}),
                tvp(depth.data(), 4, {1, H, W, 1}),
                tvp(Ts.data(), 4, {1, H, W, 1}),
                want_raw ? tvp(rgb_raw.data(), 4, {1, H, W, 3}) : tv_null(),
                want_median ? tvp(median.data(), 4, {1, H, W, 1}) : tv_null());
            if (want_dist) {
                rgbd.resize(npx * 3);
                depthd.resize(npx);
                engine_copy_distortion_to_host(
                    tvp(rgbd.data(), 4, {1, H, W, 3}),
                    tvp(depthd.data(), 4, {1, H, W, 1}));
            }

            // Display transforms.
            float inv_rs = cfg.relative_scale.has_value() ? 1.0f / *cfg.relative_scale : 1.0f;
            for (auto& v : depth) v *= inv_rs;
            if (want_median) for (auto& v : median) v *= inv_rs;
            auto dist_display = [](std::vector<float>& v) {
                constexpr float e = 1.0f / 255.0f;
                for (auto& x : v) x = std::sqrt(x + e * e) - e;
            };
            if (want_dist) { dist_display(rgbd); dist_display(depthd); }

            std::vector<float> alpha3(npx * 3);
            for (int64_t i = 0; i < npx; i++) {
                float a = 1.0f - Ts[i];
                alpha3[i*3] = alpha3[i*3+1] = alpha3[i*3+2] = a;
            }

            // Double-click pick: the 3D point under the requested pixel,
            // from buffers this render already downloaded (no extra VRAM).
            // depth holds ray depth in TRAIN units here (inv_rs applied),
            // alpha-premultiplied like the blit kernel reads it.
            if (q.pick_px >= 0 && q.pick_px < W &&
                q.pick_py >= 0 && q.pick_py < H) {
                int64_t pi = (int64_t)q.pick_py * W + q.pick_px;
                float a = alpha3[pi*3];
                float d = depth[pi] /
                          std::min(std::max(a, 1e-6f) / 0.5f, 1.0f);
                float dcv[3];
                if (a > 0.1f && d > 0.0f &&
                    viewer_pixel_ray((int)camera_model_from_name(q.model),
                                     ((float)q.pick_px + 0.5f - q.cx) / q.fx,
                                     ((float)q.pick_py + 0.5f - q.cy) / q.fy,
                                     dcv)) {
                    // CV ray -> train frame through the remapped c2w (the
                    // y/z column flip turns CV into the OpenGL basis).
                    float p[3];
                    for (int r = 0; r < 3; r++)
                        p[r] = c2w[r*4+3] + d * (c2w[r*4+0]*dcv[0] -
                                                 c2w[r*4+1]*dcv[1] -
                                                 c2w[r*4+2]*dcv[2]);
                    // Train -> client normalized frame (inverse similarity,
                    // same gating as the forward remap above).
                    if (cfg.train_frame_scale != 1.0f) {
                        const auto& T = cfg.train_to_normalized;
                        double s2 = (double)T[0]*T[0] + (double)T[4]*T[4] +
                                    (double)T[8]*T[8];
                        float e[3] = {p[0]-T[3], p[1]-T[7], p[2]-T[11]};
                        for (int r = 0; r < 3; r++)
                            p[r] = (float)((T[0*4+r]*e[0] + T[1*4+r]*e[1] +
                                            T[2*4+r]*e[2]) / s2);
                    }
                    res.pick_hit = true;
                    for (int r = 0; r < 3; r++) res.pick_point[r] = p[r];
                }
            }

            // depth -> normal display (0.5 + 0.5n), via the engine kernel on
            // device copies.
            auto depth_normal_display = [&](const std::vector<float>& d_host) {
                float* dd = (float*)d_depth_in.upload(d_host.data(), npx * 4);
                float* dn = (float*)d_normals.ensure(npx * 12);
                float* di = (float*)d_intr.upload(intr, sizeof intr);
                float* dc = (float*)d_dist.upload(dist0, sizeof dist0);
                depth_to_normal_forward_tv(
                    q.model, "NONE", tvp(di, 4, {1, 4}), tvp(dc, 4, {1, 8}),
                    /*is_ray_depth=*/true,
                    tvp(dd, 4, {1, H, W, 1}), tvp(dn, 4, {1, H, W, 3}));
                std::vector<float> n_host(npx * 3);
                backend::memcpy_sync(n_host.data(), dn, npx * 12,
                                     backend::MemcpyKind::DeviceToHost);
                if (backend::last_error())
                    throw std::runtime_error("viewer: normal D2H failed");
                for (auto& v : n_host) v = 0.5f + 0.5f * v;
                return n_host;
            };

            // Debug renders: re-run the forward with overridden splat DC
            // colors. engine_debug_forward requires the forward_3dgs above
            // and copies the result to host.
            auto debug_render = [&](const std::vector<float>& dc, int deg) {
                std::vector<float> out(npx * 3);
                int64_t nsp = (int64_t)dc.size() / 3;
                engine_debug_forward(tvp(dc.data(), 4, {nsp, 3}), deg,
                                     tvp(out.data(), 4, {1, H, W, 3}));
                return out;
            };

            // Select the display buffer for the requested key.
            std::vector<float> local;
            const std::vector<float>* buf = nullptr;
            int C = 3;
            if (q.key == "rgb")               { buf = &rgb; }
            else if (q.key == "rgb_raw")      { buf = want_raw ? &rgb_raw : &rgb; }
            else if (q.key == "depth")        { buf = &depth; C = 1; }
            else if (q.key == "alpha")        { buf = &alpha3; }
            else if (q.key == "depth_normal") { local = depth_normal_display(depth); buf = &local; }
            else if (q.key == "depth_median" && want_median)
                { buf = &median; C = 1; }
            else if (q.key == "normal_median" && want_median)
                { local = depth_normal_display(median); buf = &local; }
            else if (q.key == "rgb_distortion" && want_dist)   { buf = &rgbd; }
            else if (q.key == "depth_distortion" && want_dist) { buf = &depthd; C = 1; }
            else if (q.key == "sh") {
                // SH-only view: render with the DC color zeroed. The
                // override must be MAX-splat sized (the engine validates
                // world-buffer sizes against max_num_splats).
                int64_t nmax = engine_get_max_num_splats();
                std::vector<float> zero_dc((size_t)nmax * 3, 0.0f);
                local = debug_render(zero_dc, sh_deg);
                buf = &local;
            }
            else if (q.key == "refinement_score") {
                // Per-splat accum weight as a flat colour, single-channel
                // (turbo), padded to max splats. app::error_scale is the
                // error pane's normalizer: one scale across both views.
                int64_t nsp = engine_get_cur_num_splats();
                int64_t nmax = engine_get_max_num_splats();
                std::vector<float> aw((size_t)nsp * 2, 0.0f);
                engine_copy_accum_buffer(tvp(aw.data(), 4, {nsp, 2}));
                std::vector<float> score((size_t)nsp);
                for (int64_t i = 0; i < nsp; i++) score[i] = aw[i*2];
                const float hi = app::error_scale(score.data(), (size_t)nsp);
                std::vector<float> dc((size_t)nmax * 3, 0.0f);
                for (int64_t i = 0; i < nsp; i++) {
                    float x = (score[i] / hi - 0.5f) / 0.28f;
                    dc[i*3] = dc[i*3+1] = dc[i*3+2] = x;
                }
                std::vector<float> vis = debug_render(dc, 0);
                local.resize(npx);
                for (int64_t i = 0; i < npx; i++)
                    local[i] = (vis[i*3] + vis[i*3+1] + vis[i*3+2]) / 3.0f;
                buf = &local;
                C = 1;
            }
            else throw std::runtime_error("unsupported buffer_key: " + q.key);

            // Live frustum-size control: cheap host-side set, serialized by
            // the engine's internal viewer mutex.
            if (q.show_cams && cfg.base_camera_size > 0.0f)
                engine_viewer_set_camera_size(
                    cfg.base_camera_size * std::max(q.cam_size_scale, 1e-3f));

            // Annotate + colormap on GPU (annotation.annotate_train_cameras).
            float* db = (float*)d_buffer.upload(buf->data(), npx * C * 4);
            float* dz = (float*)d_depth.upload(depth.data(), npx * 4);
            float* da = (float*)d_alpha.upload(alpha3.data(), npx * 12);
            float* di = (float*)d_intr.upload(intr, sizeof intr);
            float* dv = (float*)d_vm.upload(vm, sizeof vm);
            float* dc = (float*)d_dist.upload(dist0, sizeof dist0);
            uint8_t* dout = (uint8_t*)d_out.ensure(npx * 3);
            engine_blit_view(
                q.key,
                tvp(db, 4, {H, W, C}),
                tvp(dz, 4, {H, W, 1}),
                tvp(da, 4, {H, W, 3}),
                (int)camera_model_from_name(q.model), "NONE",
                tvp(di, 4, {1, 4}),
                tvp(dv, 4, {4, 4}),
                tvp(dc, 4, {1, kCameraDistortionParams}),
                q.show_cams,
                q.show_grid,
                grid_dist,
                grid_target[0], grid_target[1], grid_target[2],
                tvp(dout, 1, {H, W, 3}));
            backend::memcpy_sync(out_host.data(), dout, npx * 3,
                                 backend::MemcpyKind::DeviceToHost);
            if (backend::last_error())
                throw std::runtime_error("viewer: blit D2H failed");
        }
        return out_host;
    }
};


// ===========================================================================
// RenderWorker
// ===========================================================================

RenderWorker::RenderWorker() : _impl(new Impl) {}
RenderWorker::~RenderWorker() { stop(); }

void RenderWorker::start(ViewerRenderConfig cfg, ViewerHooks hooks) {
    Impl& im = *_impl;
    im.cfg = std::move(cfg);
    im.hooks = std::move(hooks);
    im.running = true;
    im.worker = std::thread([&im] { im.worker_loop(); });
}

void RenderWorker::stop() {
    Impl& im = *_impl;
    if (!im.running.exchange(false)) return;
    im.cv_work.notify_all();
    if (im.worker.joinable()) im.worker.join();
}

uint64_t RenderWorker::submit(const ViewRequest& q) { return _impl->submit(q); }

bool RenderWorker::wait_result(uint64_t id, ViewResult& out, double timeout_s) {
    return _impl->wait_result(id, out, timeout_s);
}

bool RenderWorker::try_get_result(uint64_t id, ViewResult& out) {
    return _impl->try_get_result(id, out);
}

const ViewerRenderConfig& RenderWorker::config() const { return _impl->cfg; }

std::vector<std::string> RenderWorker::buffer_keys() const {
    std::vector<std::string> keys = {"rgb"};
    if (_impl->cfg.color_space_on) keys.push_back("rgb_raw");
    for (const char* k : {"depth", "alpha", "depth_normal"}) keys.push_back(k);
    if (_impl->cfg.output_median) {
        keys.push_back("depth_median");
        keys.push_back("normal_median");
    }
    // Distortion buffers only when a distortion regularizer is configured:
    // they are full-resolution never-freed pool allocations, so with zero
    // reg weights they are neither offered nor computed (no VRAM).
    if (_impl->cfg.distortion_reg_on) {
        keys.push_back("rgb_distortion");
        keys.push_back("depth_distortion");
    }
    keys.push_back("sh");
    keys.push_back("refinement_score");
    return keys;
}

float viewer_camera_size_heuristic(const PostSplitCameras& post) {
    return (float)camhost::frustum_display_size(post.c2w_flip.data(), post.n_post);
}

float viewer_upload_cameras(const PostSplitCameras& post) {
    // No training cameras is a legitimate state: serving a bare PLY has none.
    // Skip the upload rather than initializing an empty camera table -- the
    // frustum overlay and the thumbnail cache simply have nothing to draw,
    // and engine_viewer_init rejects N_post=0.
    if (post.n_post <= 0) return 0.0f;
    float camera_size = viewer_camera_size_heuristic(post);
    engine_viewer_init(
        tvp(post.post_models.data(), 4, {post.n_post}),
        tvp(post.intrins.data(), 4, {post.n_post, 4}),
        tvp(post.dist_coeffs.data(), 4, {post.n_post, kCameraDistortionParams}),
        tvp(post.post_distortions.data(), 4, {post.n_post}),
        tvp(post.c2w_flip.data(), 4, {post.n_post, 3, 4}),
        tvp(post.post_widths.data(), 4, {post.n_post}),
        tvp(post.post_heights.data(), 4, {post.n_post}),
        camera_size);
    return camera_size;
}

bool viewer_pixel_ray(int camera_model, float u, float v, float dir[3]) {
    constexpr float kPi = 3.14159265358979323846f;
    if (camera_model == 3) {                       // equirectangular
        if (std::abs(u) > kPi || std::abs(v) > 0.5f * kPi) return false;
        float cl = std::cos(v);
        dir[0] = cl * std::sin(u);
        dir[1] = std::sin(v);
        dir[2] = cl * std::cos(u);
        return true;
    }
    if (camera_model == 1) {                       // fisheye (equidistant)
        float r = std::sqrt(u*u + v*v);
        if (r >= kPi) return false;
        float s = r < 1e-3f ? 1.0f - r*r/6.0f : std::sin(r) / r;
        dir[0] = u * s;
        dir[1] = v * s;
        dir[2] = std::cos(r);
        return true;
    }
    if (camera_model == 2) {                       // equisolid
        float r2 = u*u + v*v;
        if (r2 >= 4.0f) return false;
        float s = std::sqrt(std::max(0.0f, 1.0f - 0.25f * r2));
        dir[0] = u * s;
        dir[1] = v * s;
        dir[2] = 1.0f - 0.5f * r2;
        return true;
    }
    float n = std::sqrt(u*u + v*v + 1.0f);         // pinhole
    dir[0] = u / n;
    dir[1] = v / n;
    dir[2] = 1.0f / n;
    return true;
}

void viewer_upload_grid(const PostSplitCameras& post) {
    // Scene radius in the engine (training/saved) frame = max camera |p|;
    // camera positions are the translation column of c2w (the y/z flip
    // leaves it untouched).
    double radius = 0.0;
    for (int64_t i = 0; i < post.n_post; i++) {
        const float* M = &post.c2w_flip[i*12];
        radius = std::max(radius,
                          std::sqrt((double)M[3]*M[3] + (double)M[7]*M[7] +
                                    (double)M[11]*M[11]));
    }
    engine_viewer_set_grid((float)std::max(radius, 1e-6), 0.0f);
}
