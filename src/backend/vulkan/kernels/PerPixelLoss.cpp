// Vulkan implementation of the multi-scale per-pixel training loss
// (kernels/loss/PerPixelLoss.cuh compute_multi_scale_per_pixel_losses), including its
// internal fused-SSIM backward (kernels/loss/FusedSSIM.cu inplace path) and the
// edge-aware densification maps (kernels/densify/DensifySplitFilter.cu canny / robust-residual
// path). Device work: shaders/{multi_scale_loss,fused_ssim,canny}.slang
// plus the quantile radix-select in Quantile.cpp.
//
// The host orchestration (pyramid construction, per-scale scratch aliasing,
// upsample accumulation, async loss readout) mirrors PerPixelLoss.cu
// line-for-line; DevicePool and AsyncReadout are backend-portable already.

#include <kernels/loss/PerPixelLoss.cuh>
#include <core/Tensor.h>

#include "backend/vulkan/kernels/KernelCommon.h"

#include <array>
#include <cmath>

// Quantile.cpp
template <bool invert_quantile>
int batch_quantile_masked_radix_select(
    const float* d_x, int B, int N, float q, float* d_out, uint32_t* temp,
    backend::Stream stream);

namespace {

using backend::MemcpyKind;

constexpr int kNRaw = (int)RawLossIndex::length;   // 31
constexpr int kNW = (int)LossWeightIndex::length;  // 19
constexpr int kNL = (int)LossIndex::length;        // 14

// ---------------------------------------------------------------------------
// Param-struct mirrors (shaders/multi_scale_loss.slang etc.)
// ---------------------------------------------------------------------------

struct PplParams {
    uint64_t camera_indices;
    uint64_t render_rgb, ref_rgb, render_depth, ref_depth, render_normal,
        depth_normal, ref_normal, render_Ts, rgb_dist, depth_dist,
        normal_dist, median_depth, median_normal;
    uint64_t ref_alpha, loss_map, losses;
    uint64_t v_render_rgb, v_ref_rgb, v_render_depth, v_ref_depth,
        v_render_normal, v_depth_normal, v_ref_normal, v_render_Ts,
        v_rgb_dist, v_depth_dist, v_normal_dist, v_median_depth,
        v_median_normal;
    float weights[kNW];
    int32_t B;
    uint32_t ppi;
    int32_t W_render, H_render;
    int32_t W_ref_depth, H_ref_depth;
    int32_t W_ref_normal, H_ref_normal;
    int32_t W_ref_alpha, H_ref_alpha;
    int32_t has_mask;
    uint32_t in_flags, out_flags, wgs_per_row;
};
static_assert(sizeof(PplParams) == 30 * 8 + (kNW + 14) * 4 + 4, "layout");

// in_flags bits (mirror multi_scale_loss.slang)
constexpr uint32_t kInCam = 1u << 0;
constexpr uint32_t kInRefDepth = 1u << 4;
constexpr uint32_t kInRefNormal = 1u << 7;
constexpr uint32_t kInRefAlpha = 1u << 14;
constexpr uint32_t kInLossMap = 1u << 15;
// out_flags bits
constexpr uint32_t kOutVRefDepth = 1u << 3;
constexpr uint32_t kOutVRefNormal = 1u << 6;

struct PplReduceParams {
    uint64_t raw_losses, io, out;
    float weights[kNW];
    int32_t batch_size;
};
static_assert(sizeof(PplReduceParams) == 3 * 8 + (kNW + 1) * 4, "layout");

struct MsPoolParams {
    uint64_t hs, ls;
    int32_t B, C;
    int32_t hsH, hsW;
    int32_t lsH, lsW;
    int32_t scale;
    float a, b;
    int32_t _pad0;
};
static_assert(sizeof(MsPoolParams) == 2 * 8 + 10 * 4, "layout");

struct MsPoolMaskedParams {
    uint64_t hs, ls, mask;
    int32_t B, C;
    int32_t hsH, hsW;
    int32_t lsH, lsW;
    int32_t _pad0;
};
static_assert(sizeof(MsPoolMaskedParams) == 3 * 8 + 8 * 4, "layout");

struct MsZeroMaskedParams {
    uint64_t grad, mask;
    int32_t B, C, H, W;
    int32_t _pad0;
};
static_assert(sizeof(MsZeroMaskedParams) == 2 * 8 + 6 * 4, "layout");

struct MsPoolBoolParams {
    uint64_t src, dst;
    int32_t B;
    int32_t hsH, hsW;
    int32_t lsH, lsW;
    uint32_t wgs_per_row;
};
static_assert(sizeof(MsPoolBoolParams) == 2 * 8 + 6 * 4, "layout");

struct MsAddScaledParams {
    uint64_t dst, src;
    float scale;
    int32_t n;
};
static_assert(sizeof(MsAddScaledParams) == 2 * 8 + 2 * 4, "layout");

struct SsimParams {
    uint64_t img1, img2, masks, mask_w, dL_dimg1, ssim_val, loss_map;
    int32_t B, H, W;
    int32_t Bm, Hm, Wm;
    float dL_dmap, map_weight;
    int32_t mode;
    uint32_t flags;
};
static_assert(sizeof(SsimParams) == 7 * 8 + 10 * 4, "layout");

struct SsimCovParams {
    uint64_t masks, tmp, out;
    int32_t B, H, W;
    int32_t Bm, Hm, Wm;
};
static_assert(sizeof(SsimCovParams) == 3 * 8 + 6 * 4, "layout");

constexpr uint32_t kSsimHasMask = 1u << 0;
constexpr uint32_t kSsimHasVal = 1u << 1;
constexpr uint32_t kSsimWriteGrad = 1u << 2;
constexpr uint32_t kSsimHasLossMap = 1u << 3;

struct CannyParams {
    uint64_t img_in, mask_in, img_out;
    int32_t B, H, W;
    int32_t has_mask;
};
static_assert(sizeof(CannyParams) == 3 * 8 + 4 * 4, "layout");

struct ResidLumaParams {
    uint64_t render, ref, mask_in, out;
    int32_t B, H, W;
    int32_t has_mask;
};
static_assert(sizeof(ResidLumaParams) == 4 * 8 + 4 * 4, "layout");

struct TukeyParams {
    uint64_t data, c_buf;
    int32_t B, N;
    uint32_t wgs_per_row;
    int32_t _pad0;
};
static_assert(sizeof(TukeyParams) == 2 * 8 + 4 * 4, "layout");

// ---------------------------------------------------------------------------
// Small helpers (mirror PerPixelLoss.cu's)
// ---------------------------------------------------------------------------

inline float* _fptr(const TorchTensorView& tv) {
    return (float*)std::get<0>(tv);
}
inline bool _has(const TorchTensorView& tv) { return std::get<0>(tv) != 0; }

inline TorchTensorView _pool_alloc_f(const std::string& key, long B, long H,
                                     long W, long C) {
    float* p = (float*)DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, (size_t)(B * H * W * C) * sizeof(float));
    return TorchTensorView((uint64_t)p, 4, {B, H, W, C});
}
inline TorchTensorView _pool_alloc_f_zero(PoolSlot key, long B, long H, long W,
                                          long C) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)(B * H * W * C));
    backend::memset_async(p, 0, (size_t)(B * H * W * C) * sizeof(float),
                          backend::kDefaultStream);
    return TorchTensorView((uint64_t)p, 4, {B, H, W, C});
}
inline TorchTensorView _pool_alloc_b(const std::string& key, long B, long H,
                                     long W) {
    // Word-rounded: the bool downsample kernel writes whole u32 words.
    bool* p = (bool*)DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, ((size_t)(B * H * W) + 3) / 4 * 4);
    return TorchTensorView((uint64_t)p, 1, {B, H, W, 1});
}

inline void _hw_or_zero(const TorchTensorView& tv, int& H, int& W) {
    if (!_has(tv)) {
        H = 0;
        W = 0;
        return;
    }
    const auto& s = std::get<2>(tv);
    H = (int)s[1];
    W = (int)s[2];
}

// Fold the pixel axis across (gx, gz); gy carries the batch index. Params go
// through the ring (PplParams is above the push floor).
void dispatch_ppl(const char* entry, int64_t pixels, int64_t B, PplParams& p) {
    if (pixels <= 0 || B <= 0) return;
    vkk::Fold f = vkk::fold_1d(pixels, 256);
    p.wgs_per_row = f.per_row;
    if (B > 65535 || f.rows > 65535)
        throw std::runtime_error("ppl: image grid dimension exceeds 65535");
    vkk::dispatch_ring(entry, {}, f.per_row, (uint32_t)B, f.rows, &p,
                       sizeof(p));
}

// Grid over 16x16 logical tiles of a (W, H, B) image.
void dispatch_tiles(const char* entry, int64_t W, int64_t H, int64_t B,
                    const void* params, uint32_t size) {
    if (W <= 0 || H <= 0 || B <= 0) return;
    uint32_t gx = (uint32_t)((W + 15) / 16), gy = (uint32_t)((H + 15) / 16);
    if (gx > 65535 || gy > 65535 || B > 65535)
        throw std::runtime_error("ppl: tile grid dimension exceeds 65535");
    vkk::dispatch(entry, {}, gx, gy, (uint32_t)B, params, size);
}

// ---------------------------------------------------------------------------
// Per-pixel loss fwd/bwd (mirrors _compute_per_pixel_losses_forward/backward)
// ---------------------------------------------------------------------------

// Common PplParams fields from the per-scale input views.
PplParams build_ppl_params(
    long B, long ppi, int W_render, int H_render,
    const TorchTensorView& render_rgb, const TorchTensorView& ref_rgb,
    const TorchTensorView& render_depth, const TorchTensorView& ref_depth,
    const TorchTensorView& render_normal, const TorchTensorView& depth_normal,
    const TorchTensorView& ref_normal, const TorchTensorView& render_Ts,
    const TorchTensorView& rgb_dist, const TorchTensorView& depth_dist,
    const TorchTensorView& normal_dist, const TorchTensorView& median_depth,
    const TorchTensorView& median_normal, const TorchTensorView& ref_alpha,
    bool has_mask,
    const std::array<float, kNW>& loss_weights,
    const TorchTensorView& camera_indices
) {
    PplParams p{};
    uint32_t in_flags = 0;
    int bit = 1;  // bit 0 = camera_indices
    if (_has(camera_indices)) in_flags |= kInCam;
    auto set_in = [&](const TorchTensorView& tv, uint64_t& field) {
        if (_has(tv)) in_flags |= 1u << bit;
        field = vkk::or_fallback(std::get<0>(tv));
        bit++;
    };
    p.camera_indices = vkk::or_fallback(std::get<0>(camera_indices));
    set_in(render_rgb, p.render_rgb);
    set_in(ref_rgb, p.ref_rgb);
    set_in(render_depth, p.render_depth);
    set_in(ref_depth, p.ref_depth);
    set_in(render_normal, p.render_normal);
    set_in(depth_normal, p.depth_normal);
    set_in(ref_normal, p.ref_normal);
    set_in(render_Ts, p.render_Ts);
    set_in(rgb_dist, p.rgb_dist);
    set_in(depth_dist, p.depth_dist);
    set_in(normal_dist, p.normal_dist);
    set_in(median_depth, p.median_depth);
    set_in(median_normal, p.median_normal);
    if (_has(ref_alpha)) in_flags |= kInRefAlpha;
    p.ref_alpha = vkk::or_fallback(std::get<0>(ref_alpha));
    p.loss_map = vkk::or_fallback(nullptr);
    p.losses = vkk::or_fallback(nullptr);
    for (int i = 0; i < 13; i++)
        (&p.v_render_rgb)[i] = vkk::or_fallback(nullptr);
    for (int i = 0; i < kNW; i++) p.weights[i] = loss_weights[i];
    p.B = (int32_t)B;
    p.ppi = (uint32_t)ppi;
    p.W_render = W_render;
    p.H_render = H_render;
    _hw_or_zero(ref_depth, p.H_ref_depth, p.W_ref_depth);
    _hw_or_zero(ref_normal, p.H_ref_normal, p.W_ref_normal);
    _hw_or_zero(ref_alpha, p.H_ref_alpha, p.W_ref_alpha);
    p.has_mask = has_mask ? 1 : 0;
    p.in_flags = in_flags;
    return p;
}

void reduce_dispatch(const char* entry, long num_train_images,
                     float* raw_losses, float* io, float* out,
                     const std::array<float, kNW>& loss_weights) {
    PplReduceParams rp{};
    rp.raw_losses = (uint64_t)raw_losses;
    rp.io = (uint64_t)io;
    rp.out = vkk::or_fallback(out);
    for (int i = 0; i < kNW; i++) rp.weights[i] = loss_weights[i];
    rp.batch_size = (int32_t)num_train_images;
    uint32_t groups = (uint32_t)((num_train_images + 1 + 31) / 32);
    vkk::dispatch(entry, {}, groups, 1, 1, &rp, sizeof(rp));
}

// ---------------------------------------------------------------------------
// Fused SSIM inplace (mirrors FusedSSIM.cu's memory-efficient inplace path)
// ---------------------------------------------------------------------------

void launch_fused_ssim_inplace(
    const TorchTensorView& img1, const TorchTensorView& img2,
    const TorchTensorView& mask, float dL_dmap,
    const TorchTensorView& dL_dimg1, float* ssim_buf,
    const TorchTensorView& ssim_loss_map, float ssim_loss_map_weight,
    int ssim_loss_map_mode
) {
    const auto& s = std::get<2>(img1);
    int B = (int)s[0], H = (int)s[1], W = (int)s[2];

    SsimParams p{};
    p.img1 = std::get<0>(img1);
    p.img2 = std::get<0>(img2);
    p.masks = vkk::or_fallback(std::get<0>(mask));
    p.mask_w = vkk::or_fallback(nullptr);
    p.dL_dimg1 = vkk::or_fallback(std::get<0>(dL_dimg1));
    p.ssim_val = vkk::or_fallback(ssim_buf);
    p.loss_map = vkk::or_fallback(std::get<0>(ssim_loss_map));
    p.B = B;
    p.H = H;
    p.W = W;
    if (_has(mask)) {
        const auto& ms = std::get<2>(mask);
        if (ms.size() >= 3) {
            p.Bm = (int32_t)ms[0];
            p.Hm = (int32_t)ms[1];
            p.Wm = (int32_t)ms[2];
        }
        p.flags |= kSsimHasMask;

        // Coverage is what turns the masked window sums into means over the
        // observed pixels; without a mask the kernel skips the divide.
        const size_t n = (size_t)B * H * W;
        float* cov = DevicePool::global().acquire<float>(PoolSlot::SsimMaskWeight, n);
        float* tmp = DevicePool::global().acquire<float>(PoolSlot::SsimMaskWeightTmp, n);
        SsimCovParams cp{};
        cp.masks = p.masks;
        cp.tmp = (uint64_t)tmp;
        cp.out = (uint64_t)cov;
        cp.B = B; cp.H = H; cp.W = W;
        cp.Bm = p.Bm; cp.Hm = p.Hm; cp.Wm = p.Wm;
        dispatch_tiles("fused_ssim.ssim_mask_cov_x", W, H, B, &cp, sizeof(cp));
        dispatch_tiles("fused_ssim.ssim_mask_cov_y", W, H, B, &cp, sizeof(cp));
        p.mask_w = (uint64_t)cov;
    }
    p.dL_dmap = dL_dmap;
    p.map_weight = ssim_loss_map_weight;
    p.mode = ssim_loss_map_mode;
    if (ssim_buf) p.flags |= kSsimHasVal;
    if (_has(dL_dimg1)) p.flags |= kSsimWriteGrad;
    if (_has(ssim_loss_map)) p.flags |= kSsimHasLossMap;

    dispatch_tiles("fused_ssim.ssim_bwd_inplace", W, H, B, &p, sizeof(p));
}

float fused_ssim_inplace_vk(
    const TorchTensorView& img1, const TorchTensorView& img2,
    const TorchTensorView& mask, float dL_dmap,
    const TorchTensorView& dL_dimg1, bool return_ssim_val,
    const TorchTensorView& ssim_loss_map, float ssim_loss_map_weight,
    int ssim_loss_map_mode
) {
    float* ssim_buf = nullptr;
    if (return_ssim_val) {
        ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
        backend::memset_async(ssim_buf, 0, sizeof(float),
                              backend::kDefaultStream);
    }
    launch_fused_ssim_inplace(img1, img2, mask, dL_dmap, dL_dimg1, ssim_buf,
                              ssim_loss_map, ssim_loss_map_weight,
                              ssim_loss_map_mode);
    if (return_ssim_val) {
        float val;
        backend::memcpy_sync(&val, ssim_buf, sizeof(float),
                             MemcpyKind::DeviceToHost);
        return val;
    }
    return -1.0f;
}

float fused_ssim_inplace_async_vk(
    const TorchTensorView& img1, const TorchTensorView& img2,
    const TorchTensorView& mask, float dL_dmap,
    const TorchTensorView& dL_dimg1, const TorchTensorView& ssim_loss_map,
    float ssim_loss_map_weight, int ssim_loss_map_mode,
    AsyncReadout<float>& readout
) {
    float* ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
    backend::memset_async(ssim_buf, 0, sizeof(float), backend::kDefaultStream);
    launch_fused_ssim_inplace(img1, img2, mask, dL_dmap, dL_dimg1, ssim_buf,
                              ssim_loss_map, ssim_loss_map_weight,
                              ssim_loss_map_mode);
    const float* prev = readout.read_previous();
    float val = prev ? prev[0] : 0.0f;
    readout.issue(ssim_buf);
    return val;
}

// ---------------------------------------------------------------------------
// Edge-aware loss maps (mirror DensifySplitFilter.cu's canny / robust-residual path)
// ---------------------------------------------------------------------------

void canny_edge_filter_vk(const TorchTensorView& rgb_in, uint64_t mask_ptr,
                          const TorchTensorView& img_out) {
    const auto& s = std::get<2>(rgb_in);
    CannyParams p{};
    p.img_in = std::get<0>(rgb_in);
    p.mask_in = vkk::or_fallback(mask_ptr);
    p.img_out = std::get<0>(img_out);
    p.B = (int32_t)s[0];
    p.H = (int32_t)s[1];
    p.W = (int32_t)s[2];
    p.has_mask = mask_ptr != 0 ? 1 : 0;
    dispatch_tiles("canny.canny_rgb", p.W, p.H, p.B, &p, sizeof(p));
}

void robust_canny_residual_vk(const TorchTensorView& render,
                              const TorchTensorView& ref, uint64_t mask_ptr,
                              float quantile, const TorchTensorView& img_out) {
    const auto& s = std::get<2>(render);
    int B = (int)s[0], H = (int)s[1], W = (int)s[2];
    if (B <= 0 || H <= 0 || W <= 0) return;
    int64_t N = (int64_t)H * W;

    float* resid = DevicePool::global().acquire<float>(
        PoolSlot::DensifyRobustResid, (size_t)B * N);
    float* c_buf =
        DevicePool::global().acquire<float>(PoolSlot::DensifyTukeyC, (size_t)B);

    ResidLumaParams lp{};
    lp.render = std::get<0>(render);
    lp.ref = std::get<0>(ref);
    lp.mask_in = vkk::or_fallback(mask_ptr);
    lp.out = (uint64_t)resid;
    lp.B = B;
    lp.H = H;
    lp.W = W;
    lp.has_mask = mask_ptr != 0 ? 1 : 0;
    dispatch_tiles("canny.residual_luma", W, H, B, &lp, sizeof(lp));

    float* temp = DevicePool::global().acquire<float>(
        PoolSlot::DensifyQuantileTemp, 1024 * (size_t)B);
    batch_quantile_masked_radix_select<false>(
        resid, B, (int)N, quantile, c_buf, (uint32_t*)temp,
        backend::kDefaultStream);

    TukeyParams tp{};
    tp.data = (uint64_t)resid;
    tp.c_buf = (uint64_t)c_buf;
    tp.B = B;
    tp.N = (int32_t)N;
    vkk::dispatch_flat("canny.tukey_inplace", {}, (int64_t)B * N, 256, &tp,
                       sizeof(tp), &tp.wgs_per_row);

    CannyParams cp{};
    cp.img_in = (uint64_t)resid;
    cp.mask_in = vkk::or_fallback(mask_ptr);
    cp.img_out = std::get<0>(img_out);
    cp.B = B;
    cp.H = H;
    cp.W = W;
    cp.has_mask = mask_ptr != 0 ? 1 : 0;
    dispatch_tiles("canny.canny_scalar", W, H, B, &cp, sizeof(cp));
}

// ---------------------------------------------------------------------------
// Pyramid helpers
// ---------------------------------------------------------------------------

void avg_pool_downsample_float_vk(const TorchTensorView& src,
                                  const TorchTensorView& dst) {
    const auto& ss = std::get<2>(src);
    const auto& ds = std::get<2>(dst);
    MsPoolParams p{};
    p.hs = std::get<0>(src);
    p.ls = std::get<0>(dst);
    p.B = (int32_t)ds[0];
    p.C = (int32_t)ds[3];
    p.hsH = (int32_t)ss[1];
    p.hsW = (int32_t)ss[2];
    p.lsH = (int32_t)ds[1];
    p.lsW = (int32_t)ds[2];
    p.scale = 1;
    dispatch_tiles("multi_scale_loss.ms_downsample_f", p.lsW, p.lsH, p.B, &p,
                   sizeof(p));
}

void avg_pool_downsample_masked_float_vk(const TorchTensorView& src,
                                         const TorchTensorView& mask,
                                         const TorchTensorView& dst) {
    const auto& ss = std::get<2>(src);
    const auto& ds = std::get<2>(dst);
    MsPoolMaskedParams p{};
    p.hs = std::get<0>(src);
    p.ls = std::get<0>(dst);
    p.mask = std::get<0>(mask);
    p.B = (int32_t)ds[0];
    p.C = (int32_t)ds[3];
    p.hsH = (int32_t)ss[1];
    p.hsW = (int32_t)ss[2];
    p.lsH = (int32_t)ds[1];
    p.lsW = (int32_t)ds[2];
    dispatch_tiles("multi_scale_loss.ms_downsample_masked_f", p.lsW, p.lsH,
                   p.B, &p, sizeof(p));
}

void zero_masked_grad_vk(const TorchTensorView& grad,
                         const TorchTensorView& mask) {
    const auto& gs = std::get<2>(grad);
    MsZeroMaskedParams p{};
    p.grad = std::get<0>(grad);
    p.mask = std::get<0>(mask);
    p.B = (int32_t)gs[0];
    p.C = (int32_t)gs[3];
    p.H = (int32_t)gs[1];
    p.W = (int32_t)gs[2];
    dispatch_tiles("multi_scale_loss.ms_zero_masked_grad", p.W, p.H, p.B, &p,
                   sizeof(p));
}

void avg_pool_downsample_gt_geometry_vk(const TorchTensorView& src,
                                        const TorchTensorView& dst) {
    const auto& ss = std::get<2>(src);
    const auto& ds = std::get<2>(dst);
    MsPoolParams p{};
    p.hs = std::get<0>(src);
    p.ls = std::get<0>(dst);
    p.B = (int32_t)ds[0];
    p.C = (int32_t)ds[3];
    p.hsH = (int32_t)ss[1];
    p.hsW = (int32_t)ss[2];
    p.lsH = (int32_t)ds[1];
    p.lsW = (int32_t)ds[2];
    p.scale = 1;
    dispatch_tiles("multi_scale_loss.ms_downsample_gt_geo", p.lsW, p.lsH, p.B,
                   &p, sizeof(p));
}

void avg_pool_downsample_bool_vk(const TorchTensorView& src,
                                 const TorchTensorView& dst) {
    const auto& ss = std::get<2>(src);
    const auto& ds = std::get<2>(dst);
    MsPoolBoolParams p{};
    p.src = std::get<0>(src);
    p.dst = std::get<0>(dst);
    p.B = (int32_t)ds[0];
    p.hsH = (int32_t)ss[1];
    p.hsW = (int32_t)ss[2];
    p.lsH = (int32_t)ds[1];
    p.lsW = (int32_t)ds[2];
    int64_t words = ((int64_t)ds[0] * ds[1] * ds[2] + 3) / 4;
    vkk::dispatch_flat("multi_scale_loss.ms_downsample_b", {}, words, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void avg_pool_upsample_float_vk(const TorchTensorView& hs_dst,
                                long dB, long dH, long dW, long C,
                                const float* ls_src, long sH, long sW,
                                int scale, float a, float b) {
    MsPoolParams p{};
    p.hs = std::get<0>(hs_dst);
    p.ls = (uint64_t)ls_src;
    p.B = (int32_t)dB;
    p.C = (int32_t)C;
    p.hsH = (int32_t)dH;
    p.hsW = (int32_t)dW;
    p.lsH = (int32_t)sH;
    p.lsW = (int32_t)sW;
    p.scale = scale;
    p.a = a;
    p.b = b;
    dispatch_tiles("multi_scale_loss.ms_upsample_f", dW, dH, dB, &p, sizeof(p));
}

// Mirrors MsNmsParams in shaders/multi_scale_loss.slang.
struct MsNmsParams {
    uint64_t img, fails;
    int32_t B, H, W;
    float falloff;
};
static_assert(sizeof(MsNmsParams) == 2 * 8 + 4 * 4, "layout");

// Two passes so the suppression is in place; see PerPixelLoss.cu.
void loss_map_nms_inplace_vk(const TorchTensorView& img,
                             const std::string& key, float falloff) {
    const auto& sh = std::get<2>(img);
    long B = sh[0], H = sh[1], W = sh[2];
    if (B <= 0 || H <= 0 || W <= 0 || falloff >= 1.0f) return;
    TorchTensorView fails = _pool_alloc_b(key, B, H, W);
    MsNmsParams p{};
    p.img = std::get<0>(img);
    p.fails = std::get<0>(fails);
    p.falloff = std::max(falloff, 0.0f);
    p.B = (int32_t)B;
    p.H = (int32_t)H;
    p.W = (int32_t)W;
    dispatch_tiles("multi_scale_loss.ms_nms_mask", W, H, B, &p, sizeof(p));
    dispatch_tiles("multi_scale_loss.ms_nms_apply", W, H, B, &p, sizeof(p));
}

void vector_add_scaled_vk(float* dst, const float* src, float scale, int n) {
    MsAddScaledParams p{};
    p.dst = (uint64_t)dst;
    p.src = (uint64_t)src;
    p.scale = scale;
    p.n = n;
    vkk::dispatch("multi_scale_loss.ms_add_scaled", {},
                  (uint32_t)((n + 31) / 32), 1, 1, &p, sizeof(p));
}

}  // namespace

LossValues compute_multi_scale_per_pixel_losses(
    int num_loss_scales,
    TorchTensorView render_rgb,
    TorchTensorView ref_rgb,
    TorchTensorView render_depth,
    TorchTensorView ref_depth,
    TorchTensorView render_normal,
    TorchTensorView depth_normal,
    TorchTensorView ref_normal,
    TorchTensorView render_Ts,
    TorchTensorView rgb_dist,
    TorchTensorView depth_dist,
    TorchTensorView normal_dist,
    TorchTensorView median_depth,
    TorchTensorView median_normal,
    TorchTensorView ref_alpha,
    bool has_mask,
    const std::array<float, (int)LossWeightIndex::length> loss_weights_0,
    const float w_ssim,
    TorchTensorView v_losses,
    std::vector<bool> needs_input_grad,
    long num_train_images,
    TorchTensorView camera_indices,
    TorchTensorView loss_map_out,
    int loss_map_mode,
    float robust_edge_aware_quantile,
    float nms_falloff,
    PerPixelGrads& grads_out
) {
    const auto _mode = densify_loss_map_base((DensifyLossMapMode)loss_map_mode);
    const bool _nms =
        densify_loss_map_has_nms((DensifyLossMapMode)loss_map_mode);
    const bool _per_pixel_write = (_mode == DensifyLossMapMode::LossFull);
    const int _ssim_mode = [&]() -> int {
        switch (_mode) {
            case DensifyLossMapMode::LossFull:
            case DensifyLossMapMode::SsimFull:
                return (int)SsimLossMapMode::SsimFull;
            case DensifyLossMapMode::SsimContrastStruct:
                return (int)SsimLossMapMode::SsimCs;
            case DensifyLossMapMode::SsimStructure:
                return (int)SsimLossMapMode::SsimStr;
            default:
                return (int)SsimLossMapMode::SsimNone;
        }
    }();
    const auto& s = std::get<2>(render_rgb);
    long B = s[0], H = s[1], W = s[2];

    if (!_has(camera_indices)) num_train_images = B;

    constexpr int MAX_SCALES = 4;
    if (num_loss_scales > MAX_SCALES)
        throw std::runtime_error("num_loss_scales > MAX_SCALES");

    TorchTensorView render_rgb_s[MAX_SCALES], ref_rgb_s[MAX_SCALES];
    TorchTensorView render_depth_s[MAX_SCALES], ref_depth_s[MAX_SCALES];
    TorchTensorView render_normal_s[MAX_SCALES], depth_normal_s[MAX_SCALES],
        ref_normal_s[MAX_SCALES];
    TorchTensorView render_Ts_s[MAX_SCALES];
    TorchTensorView rgb_dist_s[MAX_SCALES], depth_dist_s[MAX_SCALES],
        normal_dist_s[MAX_SCALES];
    TorchTensorView median_depth_s[MAX_SCALES], median_normal_s[MAX_SCALES];
    TorchTensorView ref_alpha_s[MAX_SCALES];

    render_rgb_s[0] = render_rgb;
    ref_rgb_s[0] = ref_rgb;
    render_depth_s[0] = render_depth;
    ref_depth_s[0] = ref_depth;
    render_normal_s[0] = render_normal;
    depth_normal_s[0] = depth_normal;
    ref_normal_s[0] = ref_normal;
    render_Ts_s[0] = render_Ts;
    rgb_dist_s[0] = rgb_dist;
    depth_dist_s[0] = depth_dist;
    normal_dist_s[0] = normal_dist;
    median_depth_s[0] = median_depth;
    median_normal_s[0] = median_normal;
    ref_alpha_s[0] = ref_alpha;

    // Downsample to create scales; each modality halves its own shape.
    for (int sc = 1; sc < num_loss_scales; ++sc) {
        std::string pfx = "ppl.s" + std::to_string(sc) + ".";

        auto ds_f = [&](TorchTensorView& prev, TorchTensorView& curr,
                        const std::string& name, int C) {
            if (_has(prev)) {
                const auto& pps = std::get<2>(prev);
                long nH = std::max((long)1, (long)pps[1] / 2);
                long nW = std::max((long)1, (long)pps[2] / 2);
                curr = _pool_alloc_f(pfx + name, B, nH, nW, C);
                avg_pool_downsample_float_vk(prev, curr);
            }
        };
        auto ds_geo = [&](TorchTensorView& prev, TorchTensorView& curr,
                          const std::string& name, int C) {
            if (_has(prev)) {
                const auto& pps = std::get<2>(prev);
                long nH = std::max((long)1, (long)pps[1] / 2);
                long nW = std::max((long)1, (long)pps[2] / 2);
                curr = _pool_alloc_f(pfx + name, B, nH, nW, C);
                avg_pool_downsample_gt_geometry_vk(prev, curr);
            }
        };
        auto ds_b = [&](TorchTensorView& prev, TorchTensorView& curr,
                        const std::string& name) {
            if (_has(prev)) {
                const auto& pps = std::get<2>(prev);
                long nH = std::max((long)1, (long)pps[1] / 2);
                long nW = std::max((long)1, (long)pps[2] / 2);
                curr = _pool_alloc_b(pfx + name, B, nH, nW);
                avg_pool_downsample_bool_vk(prev, curr);
            }
        };

        // Everything the loss compares pools over the same unmasked children,
        // so a masked pixel reaches no coarse value -- which is what lets a
        // fully-masked tile go unrendered (docs/datasets.md, "Skipping tiles").
        auto ds_m = [&](TorchTensorView& prev, TorchTensorView& curr,
                        const std::string& name, int C) {
            if (!_has(prev)) return;
            const TorchTensorView& mk = ref_alpha_s[sc - 1];
            const auto& pps = std::get<2>(prev);
            const auto& mks = std::get<2>(mk);
            // A mask at its own resolution cannot index this image's children.
            const bool same = _has(mk) && mks[0] == pps[0] &&
                              mks[1] == pps[1] && mks[2] == pps[2];
            if (!has_mask || !same) {
                ds_f(prev, curr, name, C);
                return;
            }
            curr = _pool_alloc_f(pfx + name, B, std::max((long)1, (long)pps[1] / 2),
                                 std::max((long)1, (long)pps[2] / 2), C);
            avg_pool_downsample_masked_float_vk(prev, mk, curr);
        };
        ds_m(render_rgb_s[sc - 1], render_rgb_s[sc], "rrgb", 3);
        ds_m(ref_rgb_s[sc - 1], ref_rgb_s[sc], "frgb", 3);
        ds_m(render_depth_s[sc - 1], render_depth_s[sc], "rd", 1);
        ds_geo(ref_depth_s[sc - 1], ref_depth_s[sc], "fd", 1);
        ds_m(render_normal_s[sc - 1], render_normal_s[sc], "rn", 3);
        ds_m(depth_normal_s[sc - 1], depth_normal_s[sc], "dn", 3);
        ds_geo(ref_normal_s[sc - 1], ref_normal_s[sc], "fn", 3);
        ds_m(render_Ts_s[sc - 1], render_Ts_s[sc], "rT", 1);
        ds_m(rgb_dist_s[sc - 1], rgb_dist_s[sc], "rgbd", 3);
        ds_m(depth_dist_s[sc - 1], depth_dist_s[sc], "dd", 1);
        ds_m(normal_dist_s[sc - 1], normal_dist_s[sc], "nd", 3);
        ds_m(median_depth_s[sc - 1], median_depth_s[sc], "md", 1);
        ds_m(median_normal_s[sc - 1], median_normal_s[sc], "mn", 3);
        ds_b(ref_alpha_s[sc - 1], ref_alpha_s[sc], "ra");
    }

    float* total_losses_ptr =
        DevicePool::global().acquire<float>(PoolSlot::PplTotalLosses, kNL);
    backend::memset_async(total_losses_ptr, 0, kNL * sizeof(float),
                          backend::kDefaultStream);

    float ssim_val = 0.0f;

    for (int scale = 0; scale < num_loss_scales; ++scale) {
        const auto& ss = std::get<2>(render_rgb_s[scale]);
        long Hs = ss[1], Ws = ss[2];
        long ppi = Hs * Ws;

        float* raw_losses_ptr = DevicePool::global().acquire<float>(
            PoolSlot::PplRawLosses, (size_t)(num_train_images + 1) * kNRaw);
        backend::memset_async(raw_losses_ptr, 0,
                              (size_t)(num_train_images + 1) * kNRaw *
                                  sizeof(float),
                              backend::kDefaultStream);

        float* losses_ptr =
            DevicePool::global().acquire<float>(PoolSlot::PplLosses, kNL);
        backend::memset_async(losses_ptr, 0, kNL * sizeof(float),
                              backend::kDefaultStream);

        float* loss_map_ptr = nullptr;
        TorchTensorView loss_map_scale = {};
        if (_has(loss_map_out)) {
            loss_map_scale =
                _pool_alloc_f_zero(PoolSlot::PplLossMapScale, B, Hs, Ws, 1);
            loss_map_ptr = _fptr(loss_map_scale);
        }

        // Forward: per-pixel raw losses (+ optional loss-map fold), then the
        // weighted reduce.
        PplParams fp = build_ppl_params(
            B, ppi, (int)Ws, (int)Hs, render_rgb_s[scale], ref_rgb_s[scale],
            render_depth_s[scale], ref_depth_s[scale], render_normal_s[scale],
            depth_normal_s[scale], ref_normal_s[scale], render_Ts_s[scale],
            rgb_dist_s[scale], depth_dist_s[scale], normal_dist_s[scale],
            median_depth_s[scale], median_normal_s[scale], ref_alpha_s[scale],
            has_mask, loss_weights_0, camera_indices);
        if (_per_pixel_write && loss_map_ptr) {
            fp.loss_map = (uint64_t)loss_map_ptr;
            fp.in_flags |= kInLossMap;
        }
        fp.losses = (uint64_t)raw_losses_ptr;
        dispatch_ppl("multi_scale_loss.ppl_fwd", ppi, B, fp);

        reduce_dispatch("multi_scale_loss.ppl_reduce_fwd", num_train_images,
                        raw_losses_ptr, losses_ptr, nullptr, loss_weights_0);

        // Backward. Per-scale grad scratches: at scale 0 alias grads_out;
        // at scale > 0 allocate per-scale-keyed scratch at the input's own
        // shape and upsample-accumulate afterwards.
        PerPixelGrads scale_grads = {};
        auto alloc_grad_f = [&](TorchTensorView& out,
                                TorchTensorView grads_out_field, bool need,
                                TorchTensorView& input,
                                const std::string& name, int C) {
            if (!(need && _has(input))) return;
            if (scale == 0 && _has(grads_out_field)) {
                out = grads_out_field;
            } else {
                const auto& is = std::get<2>(input);
                out = _pool_alloc_f(
                    "ppl.g." + name + ".s" + std::to_string(scale), is[0],
                    is[1], is[2], C);
            }
        };
        alloc_grad_f(scale_grads.v_render_rgb, grads_out.v_render_rgb,     needs_input_grad[0],  render_rgb_s[scale],   "vrgb",  3);
        alloc_grad_f(scale_grads.v_ref_rgb,    grads_out.v_ref_rgb,        needs_input_grad[1],  ref_rgb_s[scale],      "vfrgb", 3);
        alloc_grad_f(scale_grads.v_render_depth, grads_out.v_render_depth, needs_input_grad[2],  render_depth_s[scale], "vrd",   1);
        alloc_grad_f(scale_grads.v_ref_depth,  grads_out.v_ref_depth,      needs_input_grad[3],  ref_depth_s[scale],    "vfd",   1);
        alloc_grad_f(scale_grads.v_render_normal, grads_out.v_render_normal, needs_input_grad[4],  render_normal_s[scale], "vrn",   3);
        alloc_grad_f(scale_grads.v_depth_normal, grads_out.v_depth_normal, needs_input_grad[5],  depth_normal_s[scale], "vdn",   3);
        alloc_grad_f(scale_grads.v_ref_normal, grads_out.v_ref_normal,     needs_input_grad[6],  ref_normal_s[scale],   "vfn",   3);
        alloc_grad_f(scale_grads.v_render_Ts,  grads_out.v_render_Ts,      needs_input_grad[7],  render_Ts_s[scale],    "vrT",   1);
        alloc_grad_f(scale_grads.v_rgb_dist,   grads_out.v_rgb_dist,       needs_input_grad[8],  rgb_dist_s[scale],     "vrgbd", 3);
        alloc_grad_f(scale_grads.v_depth_dist, grads_out.v_depth_dist,     needs_input_grad[9],  depth_dist_s[scale],   "vdd",   1);
        alloc_grad_f(scale_grads.v_normal_dist, grads_out.v_normal_dist,   needs_input_grad[10], normal_dist_s[scale],  "vnd",   3);
        bool need_md = needs_input_grad.size() > 11 && needs_input_grad[11];
        bool need_mn = needs_input_grad.size() > 12 && needs_input_grad[12];
        alloc_grad_f(scale_grads.v_median_depth, grads_out.v_median_depth, need_md, median_depth_s[scale], "vmd", 1);
        alloc_grad_f(scale_grads.v_median_normal, grads_out.v_median_normal, need_mn, median_normal_s[scale], "vmn", 3);

        // GT-resolution grads accumulate atomically -> pre-zero.
        auto zero_view = [](TorchTensorView tv) {
            if (_has(tv)) {
                const auto& sh = std::get<2>(tv);
                size_t n = (size_t)sh[0] * sh[1] * sh[2] * sh[3];
                backend::memset_async((void*)std::get<0>(tv), 0,
                                      n * sizeof(float),
                                      backend::kDefaultStream);
            }
        };
        zero_view(scale_grads.v_ref_depth);
        zero_view(scale_grads.v_ref_normal);

        {
            float* v_raw_losses = DevicePool::global().acquire<float>(
                PoolSlot::PplVRawLosses,
                (size_t)(num_train_images + 1) * kNRaw);
            reduce_dispatch("multi_scale_loss.ppl_reduce_bwd",
                            num_train_images, raw_losses_ptr, _fptr(v_losses),
                            v_raw_losses, loss_weights_0);

            PplParams bp = build_ppl_params(
                B, ppi, (int)Ws, (int)Hs, render_rgb_s[scale],
                ref_rgb_s[scale], render_depth_s[scale], ref_depth_s[scale],
                render_normal_s[scale], depth_normal_s[scale],
                ref_normal_s[scale], render_Ts_s[scale], rgb_dist_s[scale],
                depth_dist_s[scale], normal_dist_s[scale],
                median_depth_s[scale], median_normal_s[scale],
                ref_alpha_s[scale], has_mask, loss_weights_0, camera_indices);
            bp.losses = (uint64_t)v_raw_losses;
            uint32_t out_flags = 0;
            const TorchTensorView* gs[13] = {
                &scale_grads.v_render_rgb, &scale_grads.v_ref_rgb,
                &scale_grads.v_render_depth, &scale_grads.v_ref_depth,
                &scale_grads.v_render_normal, &scale_grads.v_depth_normal,
                &scale_grads.v_ref_normal, &scale_grads.v_render_Ts,
                &scale_grads.v_rgb_dist, &scale_grads.v_depth_dist,
                &scale_grads.v_normal_dist, &scale_grads.v_median_depth,
                &scale_grads.v_median_normal};
            for (int i = 0; i < 13; i++) {
                if (_has(*gs[i])) out_flags |= 1u << i;
                (&bp.v_render_rgb)[i] = vkk::or_fallback(std::get<0>(*gs[i]));
            }
            bp.out_flags = out_flags;
            dispatch_ppl("multi_scale_loss.ppl_bwd", ppi, B, bp);
        }

        // SSIM backward (fused forward+backward); scalar routed through an
        // async readout at scale 0 for the verbose display.
        static AsyncReadout<float> ssim_readout(1);
        float ssim;
        if (scale == 0) {
            ssim = fused_ssim_inplace_async_vk(
                render_rgb_s[scale], ref_rgb_s[scale], ref_alpha_s[scale],
                -w_ssim, scale_grads.v_render_rgb, loss_map_scale, w_ssim,
                _ssim_mode, ssim_readout);
        } else {
            ssim = fused_ssim_inplace_vk(
                render_rgb_s[scale], ref_rgb_s[scale], ref_alpha_s[scale],
                -w_ssim, scale_grads.v_render_rgb, /*return_ssim_val=*/false,
                loss_map_scale, w_ssim, _ssim_mode);
        }

        // Edge-aware loss maps overwrite the (still zero) per-scale map.
        if (_has(loss_map_scale)) {
            if (_mode == DensifyLossMapMode::EdgeAware) {
                canny_edge_filter_vk(ref_rgb_s[scale],
                                     std::get<0>(ref_alpha_s[scale]),
                                     loss_map_scale);
            } else if (_mode == DensifyLossMapMode::RobustEdgeAware) {
                robust_canny_residual_vk(
                    render_rgb_s[scale], ref_rgb_s[scale],
                    std::get<0>(ref_alpha_s[scale]),
                    robust_edge_aware_quantile, loss_map_scale);
            }
        }

        // Thin the finished map before the pyramid folds it in, so each
        // scale is suppressed against its own neighbourhood.
        if (_nms && _has(loss_map_scale))
            loss_map_nms_inplace_vk(loss_map_scale,
                                    "ppl.nms.s" + std::to_string(scale),
                                    nms_falloff);

        if (scale == 0) ssim_val = ssim;

        vector_add_scaled_vk(total_losses_ptr, losses_ptr, 1.0f, kNL);

        // Upsample loss map into the full-resolution output.
        if (_has(loss_map_out) && loss_map_ptr) {
            if (scale == 0) {
                backend::memcpy_async(_fptr(loss_map_out), loss_map_ptr,
                                      (size_t)B * H * W * sizeof(float),
                                      MemcpyKind::DeviceToDevice,
                                      backend::kDefaultStream);
            } else {
                avg_pool_upsample_float_vk(
                    loss_map_out, B, H, W, 1, loss_map_ptr, Hs, Ws, 1 << scale,
                    scale == 1 ? 1.0f / num_loss_scales : 1.0f,
                    1.0f / num_loss_scales);
            }
        }

        // Upsample gradients and accumulate into grads_out (scale-0 buffers
        // alias grads_out; the copy is skipped via pointer check).
        auto upsample_grad = [&](TorchTensorView& grad_scale,
                                 TorchTensorView& grad_acc, int C) {
            if (_has(grad_acc) && scale == 0) {
                if (_fptr(grad_scale) == _fptr(grad_acc)) return;
                const auto& ds = std::get<2>(grad_acc);
                long dB = ds[0], dH = ds[1], dW = ds[2];
                backend::memcpy_async(_fptr(grad_acc), _fptr(grad_scale),
                                      (size_t)dB * dH * dW * C * sizeof(float),
                                      MemcpyKind::DeviceToDevice,
                                      backend::kDefaultStream);
                return;
            }
            if (_has(grad_scale) && _has(grad_acc)) {
                const auto& ds = std::get<2>(grad_acc);
                const auto& sshape = std::get<2>(grad_scale);
                float a = (scale == 1 ? 1.0f / num_loss_scales : 1.0f);
                float b = powf(0.25f, (float)scale) / num_loss_scales;
                avg_pool_upsample_float_vk(grad_acc, ds[0], ds[1], ds[2], C,
                                           _fptr(grad_scale), sshape[1],
                                           sshape[2], 1 << scale, a, b);
            }
        };

        upsample_grad(scale_grads.v_render_rgb, grads_out.v_render_rgb, 3);
        upsample_grad(scale_grads.v_ref_rgb, grads_out.v_ref_rgb, 3);
        upsample_grad(scale_grads.v_render_depth, grads_out.v_render_depth, 1);
        upsample_grad(scale_grads.v_ref_depth, grads_out.v_ref_depth, 1);
        upsample_grad(scale_grads.v_render_normal, grads_out.v_render_normal, 3);
        upsample_grad(scale_grads.v_depth_normal, grads_out.v_depth_normal, 3);
        upsample_grad(scale_grads.v_ref_normal, grads_out.v_ref_normal, 3);
        upsample_grad(scale_grads.v_render_Ts, grads_out.v_render_Ts, 1);
        upsample_grad(scale_grads.v_rgb_dist, grads_out.v_rgb_dist, 3);
        upsample_grad(scale_grads.v_depth_dist, grads_out.v_depth_dist, 1);
        upsample_grad(scale_grads.v_normal_dist, grads_out.v_normal_dist, 3);
        upsample_grad(scale_grads.v_median_depth, grads_out.v_median_depth, 1);
        upsample_grad(scale_grads.v_median_normal, grads_out.v_median_normal, 3);
    }

    // Scale 0 already leaves masked pixels at zero gradient; only the
    // upsampled coarse scales put anything there.
    if (num_loss_scales > 1 && has_mask && _has(grads_out.v_render_rgb) &&
            _has(ref_alpha)) {
        const auto& gs = std::get<2>(grads_out.v_render_rgb);
        const auto& ms = std::get<2>(ref_alpha);
        if (gs[0] == ms[0] && gs[1] == ms[1] && gs[2] == ms[2])
            zero_masked_grad_vk(grads_out.v_render_rgb, ref_alpha);
    }

    // Scale total losses by 1/num_scales (device-side).
    if (num_loss_scales > 1) {
        vector_add_scaled_vk(total_losses_ptr, total_losses_ptr,
                             1.0f / (float)num_loss_scales - 1.0f, kNL);
    }

    // Async D->H of accumulated losses; the verbose display is one iteration
    // behind (see PerPixelLoss.cu).
    static AsyncReadout<float> losses_readout(kNL);
    const float* h_prev = losses_readout.read_previous();

    LossValues out = {};
    if (h_prev) {
        out.rgb_loss        = h_prev[(int)LossIndex::RgbLoss];
        out.rgb_psnr        = h_prev[(int)LossIndex::RgbPSNR];
        out.depth_sup       = h_prev[(int)LossIndex::DepthSup];
        out.normal_sup      = h_prev[(int)LossIndex::NormalSup];
        out.alpha_sup       = h_prev[(int)LossIndex::AlphaSup];
        out.normal_reg      = h_prev[(int)LossIndex::NormalReg];
        out.alpha_reg       = h_prev[(int)LossIndex::AlphaReg];
        out.rgb_dist_reg    = h_prev[(int)LossIndex::RgbDistReg];
        out.depth_dist_reg  = h_prev[(int)LossIndex::DepthDistReg];
        out.normal_dist_reg = h_prev[(int)LossIndex::NormalDistReg];
        out.mean_median_depth_sup    = h_prev[(int)LossIndex::MeanMedianDepthSup];
        out.median_depth_normal_reg  = h_prev[(int)LossIndex::MedianDepthNormalReg];
        out.median_normal_sup        = h_prev[(int)LossIndex::MedianNormalSup];
        out.median_render_normal_reg = h_prev[(int)LossIndex::MedianRenderNormalReg];
    }
    out.ssim = ssim_val;
    losses_readout.issue(total_losses_ptr);
    return out;
}
