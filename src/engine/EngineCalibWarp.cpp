// Per-image warp of the ground truth onto the render, absorbing a camera's
// calibration residual (docs/notes/calib-warp.md). Fitted before the loss,
// which then reads the warped copy, so the render side never sees it.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "core/Env.h"
#include "core/PixelFormat.h"
#include "kernels/pixelwise/CalibWarp.cuh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TorchTensorView table_tv(DeviceTensor2D<float>& t) {
    return TorchTensorView((uint64_t)t.data_ptr(), 4, {t.size<0>(), t.size<1>()});
}

TorchTensorView level_image(const std::string& key, int64_t C, int64_t H, int64_t W) {
    float* p = (float*)DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, (size_t)(C * H * W * 3) * sizeof(float));
    return TorchTensorView((uint64_t)p, 4, {C, H, W, 3});
}

// The step's [C, ...] gradient and Hessian rows. The update zeroes what it
// consumed, so only rows this allocation has not handed out before need it.
void batch_tables(int64_t C, TorchTensorView& grad, TorchTensorView& hess) {
    auto& cw = engine().calib_warp;
    const int64_t nn = (int64_t)cw.grid_n * cw.grid_n;
    const int64_t DH = calib_warp_hess_floats(cw.D);
    const float* g0 = cw.grad.data_ptr();
    cw.grad.resize_dynamic(VramCategory::Appearance, "eng.calib_warp.grad", C, nn * cw.D);
    cw.hess.resize_dynamic(VramCategory::Appearance, "eng.calib_warp.hess", C, nn * DH);
    if (cw.grad.data_ptr() != g0 || C > cw.zeroed_rows) {
        backend::memset_async(cw.grad.data_ptr(), 0, (size_t)(C * nn * cw.D) * sizeof(float),
                              (backend::Stream)0);
        backend::memset_async(cw.hess.data_ptr(), 0, (size_t)(C * nn * DH) * sizeof(float),
                              (backend::Stream)0);
        cw.zeroed_rows = C;
    }
    grad = table_tv(cw.grad);
    hess = table_tv(cw.hess);
}

TorchTensorView cam_tv() {
    const int* p = engine().bilagrid_cur_cam_indices.data_ptr();
    if (p == nullptr) return _tv_null();
    return TorchTensorView((uint64_t)p, 4, {(int64_t)engine().bilagrid_cur_cam_indices.size()});
}

TorchTensorView intrins_tv() {
    return TorchTensorView((uint64_t)engine().camera.intrins.data_ptr(), 4,
                           {(int64_t)engine().camera.num, 4});
}

TorchTensorView render_tv() {
    return _engine_image_view(std::get<0>(engine().fwd.renders), engine().fwd.rgb_fmt);
}

TorchTensorView ts_tv() {
    const auto& t = engine().fwd.render_Ts;
    if (engine().calib_warp.D != 4 || t.data_ptr() == nullptr) return _tv_null();
    return TorchTensorView((uint64_t)t.data_ptr(), 4,
                           {(int64_t)engine().camera.num, (int64_t)engine().camera.height,
                            (int64_t)engine().camera.width, 1});
}

TorchTensorView mask_tv() {
    const auto& a = engine().gt.alpha;
    if (!engine().gt.has_mask || a.data_ptr() == nullptr) return _tv_null();
    return TorchTensorView((uint64_t)a.data_ptr(), 1,
                           {a.size<0>(), a.size<1>(), a.size<2>(), 1});
}

TorchTensorView depth_tv() {
    const auto& d = std::get<1>(engine().fwd.renders);
    if (engine().calib_warp.D != 4 || d.data_ptr() == nullptr) return _tv_null();
    return TorchTensorView((uint64_t)d.data_ptr(), 4,
                           {(int64_t)engine().camera.num, (int64_t)engine().camera.height,
                            (int64_t)engine().camera.width, 1});
}

// SS_CALIB_WARP_LOG=<steps>: the table's size, in English like the other deep
// diagnostics; SS_CALIB_WARP_DUMP=<file> also writes it out ([N, P, n, D] int32
// header, then the floats), for reference/python-style analysis.
void log_stats(int step) {
    auto& cw = engine().calib_warp;
    const int64_t N = cw.params.size<0>(), P = cw.params.size<1>();
    std::vector<float> h((size_t)(N * P));
    backend::memcpy_sync(h.data(), cw.params.data_ptr(), h.size() * sizeof(float),
                         backend::MemcpyKind::DeviceToHost);
    double sum = 0.0, mx = 0.0, par = 0.0;
    int64_t pts = 0;
    for (int64_t c = 0; c < N; ++c)
        for (int64_t k = 0; k < P / cw.D; ++k) {
            const float* q = &h[(size_t)(c * P + k * cw.D)];
            const double m = std::hypot(q[0], q[1]);
            sum += m;
            mx = std::max(mx, m);
            if (cw.D == 4) par += std::hypot(q[2], q[3]);
            ++pts;
        }
    std::fprintf(stderr, "[calib-warp] step %d: |d| mean %.3f max %.3f mrad, parallax mean %.3f\n",
                 step, pts ? sum / pts : 0.0, mx, pts ? par / pts : 0.0);
    if (const char* path = spirula::env("CALIB_WARP_DUMP")) {
        if (FILE* f = std::fopen(path, "wb")) {
            const int32_t hdr[4] = {(int32_t)N, (int32_t)P, cw.grid_n, cw.D};
            std::fwrite(hdr, sizeof(hdr), 1, f);
            std::fwrite(h.data(), sizeof(float), h.size(), f);
            std::fclose(f);
        }
    }
}

// Not flags: the values every run in docs/notes/calib-warp.md used. Only the
// prior was varied.
constexpr float kDamping = 0.1f;   // Levenberg-Marquardt, x the mean curvature
constexpr float kPrior = 0.01f;    // pull to zero; 0.1 cost 0.0014 LPIPS-A (calib-warp.md)
constexpr float kMaxStepPx = 1.5f; // what the linearization holds
constexpr float kRobustEps = 0.03f;

}  // namespace

void engine_init_calib_warp(int n_cams, const CalibWarpConfig& cfg) {
    auto& cw = engine().calib_warp;
    cw = CalibWarpState{};
    if (n_cams <= 0) return;
    if (cfg.grid < 2 || cfg.grid > 16)
        throw std::runtime_error("calib_warp_grid must be between 2 and 16");
    cw.grid_n = cfg.grid;
    cw.D = cfg.parallax ? 4 : 2;
    cw.start_step = cfg.start_step;
    cw.step = cfg.step;
    cw.smooth = cfg.smooth;
    const int nn = cw.grid_n * cw.grid_n;
    cw.params.resize(PoolSlot::EngCalibWarpParams, n_cams, nn * cw.D);
    cw.params.zero();
    cw.enabled = true;
}

bool engine_calib_warp_wants_depth() {
    return engine().calib_warp.enabled && engine().calib_warp.D == 4;
}

template <typename T>
static void warp_aux(DeviceTensor3D<T>& map, const char* key) {
    if (map.data_ptr() == nullptr || map.numel() == 0 ||
        map.template size<0>() != engine().camera.num)
        return;
    const TorchTensorView src = _dt3d_tv(map);
    void* out = DevicePool::global().acquire_dynamic(VramCategory::Image, key,
                                                     (size_t)map.numel() * sizeof(T));
    auto& cw = engine().calib_warp;
    calib_warp_aux_forward(src, depth_tv(), ts_tv(), table_tv(cw.params), intrins_tv(), cam_tv(),
                           cw.grid_n, cw.D, (int)engine().camera.height,
                           (int)engine().camera.width,
                           TorchTensorView((uint64_t)out, std::get<1>(src), std::get<2>(src)));
    map = DeviceTensor3D<T>(TorchTensorView((uint64_t)out, std::get<1>(src), std::get<2>(src)));
}

// Point gt.rgb, and the depth, normal and mask beside it, at the ground truth
// warped through the installed cameras' rows;
// _engine_calib_warp_restore_gt() points it back. False when it cannot apply.
static bool _swap_in_warped_gt() {
    auto& cw = engine().calib_warp;
    if (!cw.enabled || !engine().gt.has_gt || !_tv_valid(engine().gt.rgb)) return false;
    const int64_t C = engine().camera.num, H = engine().camera.height,
                  W = engine().camera.width;
    const auto& gs = std::get<2>(engine().gt.rgb);
    if (gs.size() < 3 || gs[0] != C || gs[1] != H || gs[2] != W) return false;
    const TorchTensorView gt = engine().gt.rgb;
    // float16: half the bytes of float32 and still finer than the 8-bit source.
    void* out = DevicePool::global().acquire_dynamic(
        VramCategory::Image, "eng.calib_warp.gt", pixel_buffer_bytes(PixelFormat::F16, C * H * W, 3));
    const TorchTensorView out_tv((uint64_t)out, 2, {C, H, W, 3});
    calib_warp_gt_forward(gt, render_tv(), depth_tv(), ts_tv(), table_tv(cw.params), intrins_tv(),
                          cam_tv(), cw.grid_n, cw.D, out_tv);
    cw.src_gt = gt;
    engine().gt.rgb = out_tv;
    cw.src_depth = engine().gt.depth;
    cw.src_normal = engine().gt.normal;
    cw.src_alpha = engine().gt.alpha;
    warp_aux(engine().gt.depth, "eng.calib_warp.depth");
    warp_aux(engine().gt.normal, "eng.calib_warp.normal");
    if (engine().gt.has_mask) warp_aux(engine().gt.alpha, "eng.calib_warp.mask");
    cw.swapped = true;
    return true;
}

void _engine_calib_warp_before_loss(int step) {
    auto& cw = engine().calib_warp;
    cw.active_this_step = false;
    if (!cw.enabled || step < cw.start_step) return;
    static const int log_every = [] {
        const char* v = spirula::env("CALIB_WARP_LOG");
        return v && *v ? std::atoi(v) : 0;
    }();
    if (log_every > 0 && step % log_every == 0) log_stats(step);
    cw.active_this_step = _swap_in_warped_gt();
}

void _engine_calib_warp_preview() {
    // A preview stays swapped: the next step installs its own ground truth.
    if (_swap_in_warped_gt()) engine().calib_warp.swapped = false;
}

void _engine_calib_warp_after_loss(TorchTensorView v_render_rgb) {
    auto& cw = engine().calib_warp;
    if (!cw.active_this_step || !_tv_valid(v_render_rgb)) return;
    const int64_t C = engine().camera.num, H = engine().camera.height,
                  W = engine().camera.width;
    const TorchTensorView cams = cam_tv(), intr = intrins_tv(), params = table_tv(cw.params);
    TorchTensorView grad, hess;
    batch_tables(C, grad, hess);
    // The loss is a mean over 3 C H W values; this brings an L1 gradient to the
    // eps-scaled sign an IRLS (Charbonnier) residual carries, which the
    // Hessian's weights assume.
    const float v_scale = 3.0f * (float)(C * H * W) * kRobustEps;
    // The warped copy the loss just read, so five plain taps a pixel instead
    // of five bilinear samples of the source.
    calib_warp_accumulate(engine().gt.rgb, render_tv(), depth_tv(), ts_tv(), v_render_rgb, v_scale,
                          params, intr, cams, mask_tv(), cw.grid_n, cw.D, kRobustEps, 1.0f, true,
                          grad, hess);
    calib_warp_update(params, grad, hess, cams, intr, 1.0f, (int)C, cw.grid_n, cw.D, cw.step,
                      kDamping, cw.smooth, kPrior, kMaxStepPx);
}

void _engine_calib_warp_restore_gt() {
    auto& cw = engine().calib_warp;
    if (!cw.swapped) return;
    engine().gt.rgb = cw.src_gt;
    engine().gt.depth = cw.src_depth;
    engine().gt.normal = cw.src_normal;
    engine().gt.alpha = cw.src_alpha;
    cw.swapped = false;
}

void engine_calib_warp_align_to_gt(const float* est, const float* src, float* out,
                                   bool use_mask) {
    const int64_t C = engine().camera.num, H = engine().camera.height,
                  W = engine().camera.width;
    const TorchTensorView gt = engine().gt.rgb;
    const auto& gs = std::get<2>(gt);
    if (C <= 0 || gs.size() < 3 || gs[0] != C || gs[1] != H || gs[2] != W)
        throw std::runtime_error("engine_calib_warp_align_to_gt: no ground truth at the render size");
    const size_t bytes = (size_t)(C * H * W * 3) * sizeof(float);
    std::vector<TorchTensorView> est_l{level_image("eng.calib_align.est0", C, H, W)}, gt_l{gt};
    const TorchTensorView src_tv = level_image("eng.calib_align.src", C, H, W);
    backend::memcpy_sync((void*)std::get<0>(est_l[0]), est, bytes, backend::MemcpyKind::HostToDevice);
    backend::memcpy_sync((void*)std::get<0>(src_tv), src, bytes, backend::MemcpyKind::HostToDevice);
    // Down to 1/8, while the short side keeps 48 pixels: the fit starts where a
    // few pixels of test-view misalignment are inside the linearization.
    std::vector<std::pair<int64_t, int64_t>> dims{{H, W}};
    while (dims.size() < 4 && std::min(dims.back().first, dims.back().second) / 2 >= 48) {
        const int64_t h = (dims.back().first + 1) / 2, w = (dims.back().second + 1) / 2;
        const std::string l = std::to_string(dims.size());
        est_l.push_back(level_image("eng.calib_align.est" + l, C, h, w));
        gt_l.push_back(level_image("eng.calib_align.gt" + l, C, h, w));
        calib_warp_downsample2(est_l[est_l.size() - 2], est_l.back());
        calib_warp_downsample2(gt_l[gt_l.size() - 2], gt_l.back());
        dims.push_back({h, w});
    }

    constexpr int n = 16;
    constexpr float kStep = 1.0f, kDamp = 0.1f, kSmooth = 0.02f, kMaxStep = 1.5f, kEps = 0.03f;
    constexpr int iters = 10;   // per pyramid level
    DeviceTensor2D<float> params, grad, hess;
    params.resize_dynamic(VramCategory::Other, "eng.calib_align.params", C, n * n * 2);
    grad.resize_dynamic(VramCategory::Other, "eng.calib_align.grad", C, n * n * 2);
    hess.resize_dynamic(VramCategory::Other, "eng.calib_align.hess", C, n * n * 3);
    params.zero();
    grad.zero();
    hess.zero();
    const TorchTensorView intr = intrins_tv(), mask = use_mask ? mask_tv() : _tv_null();
    for (int l = (int)dims.size() - 1; l >= 0; --l) {
        const float fs = (float)dims[(size_t)l].second / (float)W;
        for (int it = 0; it < iters; ++it) {
            calib_warp_accumulate(est_l[(size_t)l], gt_l[(size_t)l], _tv_null(), _tv_null(), _tv_null(),
                                  0.0f, table_tv(params), intr, _tv_null(), mask, n, 2, kEps,
                                  fs, false, table_tv(grad), table_tv(hess));
            calib_warp_update(table_tv(params), table_tv(grad), table_tv(hess), _tv_null(), intr, fs,
                              (int)C, n, 2, kStep, kDamp, kSmooth, 0.0f, kMaxStep);
        }
    }
    const TorchTensorView out_tv = level_image("eng.calib_align.out", C, H, W);
    calib_warp_gt_forward(src_tv, gt, _tv_null(), _tv_null(), table_tv(params), intr, _tv_null(), n,
                          2, out_tv);
    backend::memcpy_sync(out, (void*)std::get<0>(out_tv), bytes, backend::MemcpyKind::DeviceToHost);
}
