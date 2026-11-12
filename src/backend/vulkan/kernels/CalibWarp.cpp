// Vulkan implementations of the calibration-warp launch APIs
// (kernels/pixelwise/CalibWarp.cuh). Device work: shaders/calib_warp.slang.

#include <kernels/pixelwise/CalibWarp.cuh>
#include <core/PixelFormat.h>
#include <core/Tensor.h>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors CalibWarpParams in shaders/calib_warp.slang.
struct CalibWarpParams {
    uint64_t gt, render, v_rgb, depth, Ts, params, intrins, cam_indices, grad, hess, out, mask;
    int32_t B, H, W, n;
    int32_t Hm, Wm, has_mask, has_Ts;
    uint32_t out_fmt;
    uint32_t gt_fmt, render_fmt;
    int32_t has_v, has_depth, has_cam;
    int32_t sub_blocks, chunk;
    float v_scale, eps;
    float step, damping, smooth, prior, max_step;
    uint32_t wgs_per_row;
    int32_t _pad0;
    float f_scale;
    int32_t has_intr;
    int32_t gt_warped;
};
static_assert(sizeof(CalibWarpParams) == 12 * 8 + 28 * 4, "layout");

void check(int n, int D) {
    if (n < 2 || n * n > 256)
        throw std::runtime_error("calib_warp: grid size must be in [2, 16]");
    if (D != 2 && D != 4)
        throw std::runtime_error("calib_warp: D must be 2 or 4");
}

uint64_t ptr(const TorchTensorView& t) { return std::get<0>(t); }

CalibWarpParams base_params(TorchTensorView gt_rgb, TorchTensorView render_rgb,
                            TorchTensorView render_depth, TorchTensorView render_Ts,
                            TorchTensorView params,
                            TorchTensorView intrins, TorchTensorView cam_indices,
                            int n) {
    const auto& s = std::get<2>(render_rgb);
    CalibWarpParams p{};
    p.gt = vkk::or_fallback(ptr(gt_rgb));
    p.render = vkk::or_fallback(ptr(render_rgb));
    p.v_rgb = vkk::or_fallback(nullptr);
    p.depth = vkk::or_fallback(ptr(render_depth));
    p.Ts = vkk::or_fallback(ptr(render_Ts));
    p.has_Ts = ptr(render_Ts) != 0 ? 1 : 0;
    p.params = ptr(params);
    p.intrins = vkk::or_fallback(ptr(intrins));
    p.cam_indices = vkk::or_fallback(ptr(cam_indices));
    p.grad = vkk::or_fallback(nullptr);
    p.hess = vkk::or_fallback(nullptr);
    p.out = vkk::or_fallback(nullptr);
    p.mask = vkk::or_fallback(nullptr);
    p.B = (int32_t)s[0];
    p.H = (int32_t)s[1];
    p.W = (int32_t)s[2];
    p.n = n;
    p.gt_fmt = (uint32_t)pixel_format(gt_rgb);
    p.render_fmt = (uint32_t)pixel_format(render_rgb);
    p.has_depth = ptr(render_depth) != 0 ? 1 : 0;
    p.has_cam = ptr(cam_indices) != 0 ? 1 : 0;
    p.f_scale = 1.0f;
    return p;
}

// Every dispatch passes both constants; aux_kind is calib_warp_aux_fwd's.
backend::vk::SpecList spec(int D, uint32_t aux_kind = 0) {
    return backend::vk::SpecList{D == 4 ? 1u : 0u, aux_kind};
}

}  // namespace

void calib_warp_gt_forward(
    TorchTensorView gt_rgb, TorchTensorView render_rgb, TorchTensorView render_depth,
    TorchTensorView render_Ts, TorchTensorView params, TorchTensorView intrins,
    TorchTensorView cam_indices, int grid_n, int D, TorchTensorView out_rgb
) {
    check(grid_n, D);
    CalibWarpParams p = base_params(gt_rgb, render_rgb, render_depth, render_Ts, params,
                                    intrins, cam_indices, grid_n);
    if (p.B <= 0 || p.H <= 0 || p.W <= 0) return;
    p.out = ptr(out_rgb);
    p.out_fmt = (uint32_t)pixel_format(out_rgb);
    vkk::Fold f = vkk::fold_1d((int64_t)p.B * p.H * p.W, 256);
    p.wgs_per_row = f.per_row;
    if (f.rows > 65535)
        throw std::runtime_error("calib_warp: grid dimension exceeds 65535");
    vkk::dispatch_ring("calib_warp.calib_warp_gt_fwd", spec(D), f.per_row, f.rows, 1, &p,
                       sizeof(p));
}

void calib_warp_aux_forward(
    TorchTensorView src, TorchTensorView render_depth, TorchTensorView render_Ts,
    TorchTensorView params, TorchTensorView intrins, TorchTensorView cam_indices,
    int grid_n, int D, int render_H, int render_W, TorchTensorView out
) {
    check(grid_n, D);
    const auto& s = std::get<2>(src);
    const uint32_t elem = std::get<1>(src);
    const int64_t ch = s.size() >= 4 ? s[3] : 1;
    if (!((elem == 1 && ch == 1) || (elem == 4 && (ch == 1 || ch == 3))))
        throw std::runtime_error("calib_warp_aux_forward: a uint8 mask, depth or normal map");
    CalibWarpParams p{};
    p.render = p.v_rgb = p.grad = p.hess = p.mask = vkk::or_fallback(nullptr);
    p.gt = ptr(src);
    p.out = ptr(out);
    p.depth = vkk::or_fallback(ptr(render_depth));
    p.Ts = vkk::or_fallback(ptr(render_Ts));
    p.has_depth = ptr(render_depth) != 0 ? 1 : 0;
    p.has_Ts = ptr(render_Ts) != 0 ? 1 : 0;
    p.params = ptr(params);
    p.intrins = vkk::or_fallback(ptr(intrins));
    p.cam_indices = vkk::or_fallback(ptr(cam_indices));
    p.has_cam = ptr(cam_indices) != 0 ? 1 : 0;
    p.B = (int32_t)s[0];
    p.Hm = (int32_t)s[1];
    p.Wm = (int32_t)s[2];
    p.H = render_H;
    p.W = render_W;
    p.n = grid_n;
    if (p.B <= 0 || p.Hm <= 0 || p.Wm <= 0 || p.H <= 0 || p.W <= 0) return;
    vkk::Fold f = vkk::fold_1d((int64_t)p.B * p.Hm * p.Wm, 256);
    p.wgs_per_row = f.per_row;
    if (f.rows > 65535)
        throw std::runtime_error("calib_warp: grid dimension exceeds 65535");
    const uint32_t kind = elem == 1 ? 0u : ch == 1 ? 1u : 2u;
    vkk::dispatch_ring("calib_warp.calib_warp_aux_fwd", spec(D, kind), f.per_row, f.rows, 1,
                       &p, sizeof(p));
}

void calib_warp_accumulate(
    TorchTensorView gt_rgb, TorchTensorView render_rgb, TorchTensorView render_depth,
    TorchTensorView render_Ts, TorchTensorView v_render_rgb, float v_scale,
    TorchTensorView params, TorchTensorView intrins, TorchTensorView cam_indices,
    TorchTensorView mask, int grid_n, int D, float robust_eps, float f_scale, bool gt_is_warped,
    TorchTensorView grad, TorchTensorView hess
) {
    check(grid_n, D);
    CalibWarpParams p = base_params(gt_rgb, render_rgb, render_depth, render_Ts, params,
                                    intrins, cam_indices, grid_n);
    if (p.B <= 0 || p.H <= 0 || p.W <= 0) return;
    const int n = grid_n;
    const int64_t bw = (p.W + n - 2) / (n - 1) + 3, bh = (p.H + n - 2) / (n - 1) + 3;
    const int64_t npx = bw * bh;
    p.sub_blocks = (int32_t)std::max<int64_t>(1, (npx + 2047) / 2048);
    p.chunk = (int32_t)((npx + p.sub_blocks - 1) / p.sub_blocks);
    p.v_rgb = vkk::or_fallback(ptr(v_render_rgb));
    p.has_v = ptr(v_render_rgb) != 0 ? 1 : 0;
    p.v_scale = v_scale;
    const auto& ms = std::get<2>(mask);
    if (ptr(mask) != 0 && ms.size() >= 3) {
        p.mask = ptr(mask);
        p.has_mask = 1;
        p.Hm = (int32_t)ms[1];
        p.Wm = (int32_t)ms[2];
    }
    p.eps = robust_eps;
    p.f_scale = f_scale;
    p.gt_warped = gt_is_warped ? 1 : 0;
    p.grad = ptr(grad);
    p.hess = ptr(hess);
    const int64_t groups = (int64_t)(n - 1) * (n - 1) * p.sub_blocks;
    if (groups > 65535 || p.B > 65535)
        throw std::runtime_error("calib_warp: grid dimension exceeds 65535");
    vkk::dispatch_ring("calib_warp.calib_warp_accum", spec(D), (uint32_t)groups,
                       (uint32_t)p.B, 1, &p, sizeof(p));
}

void calib_warp_update(
    TorchTensorView params, TorchTensorView grad, TorchTensorView hess,
    TorchTensorView cam_indices, TorchTensorView intrins, float f_scale, int batch, int grid_n,
    int D, float step, float damping, float smooth, float prior, float max_step
) {
    check(grid_n, D);
    if (batch <= 0) return;
    CalibWarpParams p{};
    p.gt = p.render = p.v_rgb = p.depth = p.Ts = p.out = p.mask = vkk::or_fallback(nullptr);
    p.intrins = vkk::or_fallback(ptr(intrins));
    p.has_intr = ptr(intrins) != 0 ? 1 : 0;
    p.f_scale = f_scale;
    p.params = ptr(params);
    p.grad = ptr(grad);
    p.hess = ptr(hess);
    p.cam_indices = vkk::or_fallback(ptr(cam_indices));
    p.has_cam = ptr(cam_indices) != 0 ? 1 : 0;
    p.B = batch;
    p.n = grid_n;
    p.step = step;
    p.damping = damping;
    p.smooth = smooth;
    p.prior = prior;
    p.max_step = max_step;
    if (batch > 65535)
        throw std::runtime_error("calib_warp: grid dimension exceeds 65535");
    vkk::dispatch_ring("calib_warp.calib_warp_update", spec(D), (uint32_t)batch, 1, 1,
                       &p, sizeof(p));
}

void calib_warp_downsample2(TorchTensorView in_rgb, TorchTensorView out_rgb) {
    const auto& si = std::get<2>(in_rgb);
    const auto& so = std::get<2>(out_rgb);
    CalibWarpParams p{};
    p.render = p.v_rgb = p.depth = p.Ts = p.params = p.intrins = p.cam_indices = p.grad =
        p.hess = p.mask = vkk::or_fallback(nullptr);
    p.gt = ptr(in_rgb);
    p.gt_fmt = (uint32_t)pixel_format(in_rgb);
    p.out = ptr(out_rgb);
    p.B = (int32_t)si[0];
    p.H = (int32_t)si[1];
    p.W = (int32_t)si[2];
    p.Hm = (int32_t)so[1];
    p.Wm = (int32_t)so[2];
    const int64_t n = (int64_t)p.B * p.Hm * p.Wm;
    if (n <= 0) return;
    vkk::Fold f = vkk::fold_1d(n, 256);
    p.wgs_per_row = f.per_row;
    if (f.rows > 65535) throw std::runtime_error("calib_warp: grid dimension exceeds 65535");
    vkk::dispatch_ring("calib_warp.calib_warp_downsample2", spec(2), f.per_row, f.rows, 1, &p,
                       sizeof(p));
}
