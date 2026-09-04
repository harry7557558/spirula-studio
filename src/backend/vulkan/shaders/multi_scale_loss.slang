// Per-pixel fused training losses + image-pyramid helpers (Vulkan backend),
// mirroring kernels/loss/PerPixelLoss.cu. All per-pixel loss math comes from the
// canonical shaders/per_pixel_losses.slang (the same functions the CUDA build
// calls through generated/per_pixel_losses.cuh), so per-pixel outputs and
// gradients match up to fast-math rounding.
//
// Optional buffers: params pointer fields are never null (host launchers pass
// the zeroed fallback page); presence moves to the in_flags / out_flags bit
// fields. Bool buffers ([B,H,W] masks) are read/written through u32 words --
// 8-bit storage access is not baseline SPIR-V.
//
// The raw-loss accumulation mirrors the CUDA kernel's routing exactly
// (global row 0 vs per-image rows 1..N selected by atomicAddWhich), but the
// CUDA 1024-thread block reduce becomes a flat 256-wide groupshared tree +
// one float CAS-atomic per workgroup per index: accumulated sums are
// order-dependent on both backends -> loose parity channel.

#include "int64_compat.slang"
#include "int8_compat.slang"
#include "atomic_float.slang"

#define CudaDeviceExport ForceInline
#include "shaders/per_pixel_losses.slang"
#undef CudaDeviceExport

static const int kNRaw = int(RawLossIndex::length);        // 31
static const int kNW = int(LossWeightIndex::length);       // 19
static const int kNL = int(LossIndex::length);             // 14

// in_flags bits (buffer present)
static const uint kInCam = 1u << 0;
static const uint kInRenderRgb = 1u << 1;
static const uint kInRefRgb = 1u << 2;
static const uint kInRenderDepth = 1u << 3;
static const uint kInRefDepth = 1u << 4;
static const uint kInRenderNormal = 1u << 5;
static const uint kInDepthNormal = 1u << 6;
static const uint kInRefNormal = 1u << 7;
static const uint kInRenderTs = 1u << 8;
static const uint kInRgbDist = 1u << 9;
static const uint kInDepthDist = 1u << 10;
static const uint kInNormalDist = 1u << 11;
static const uint kInMedianDepth = 1u << 12;
static const uint kInMedianNormal = 1u << 13;
static const uint kInRefAlpha = 1u << 14;
static const uint kInLossMap = 1u << 15;

// out_flags bits (v_* buffer present), in PerPixelGrads field order.
static const uint kOutVRenderRgb = 1u << 0;
static const uint kOutVRefRgb = 1u << 1;
static const uint kOutVRenderDepth = 1u << 2;
static const uint kOutVRefDepth = 1u << 3;
static const uint kOutVRenderNormal = 1u << 4;
static const uint kOutVDepthNormal = 1u << 5;
static const uint kOutVRefNormal = 1u << 6;
static const uint kOutVRenderTs = 1u << 7;
static const uint kOutVRgbDist = 1u << 8;
static const uint kOutVDepthDist = 1u << 9;
static const uint kOutVNormalDist = 1u << 10;
static const uint kOutVMedianDepth = 1u << 11;
static const uint kOutVMedianNormal = 1u << 12;

struct PplParams {
    i64mem_t* camera_indices;
    float* render_rgb;
    float* ref_rgb;
    float* render_depth;
    float* ref_depth;
    float* render_normal;
    float* depth_normal;
    float* ref_normal;
    float* render_Ts;
    float* rgb_dist;
    float* depth_dist;
    float* normal_dist;
    float* median_depth;
    float* median_normal;
    uint* ref_alpha;      // bool bytes, word-read
    float* loss_map;      // fwd only
    float* losses;        // fwd: raw-loss accum rows; bwd: v_raw_losses rows
    float* v_render_rgb;
    float* v_ref_rgb;
    float* v_render_depth;
    float* v_ref_depth;
    float* v_render_normal;
    float* v_depth_normal;
    float* v_ref_normal;
    float* v_render_Ts;
    float* v_rgb_dist;
    float* v_depth_dist;
    float* v_normal_dist;
    float* v_median_depth;
    float* v_median_normal;
    float weights[kNW];
    int B;
    uint ppi;             // pixels per image (H_render * W_render)
    int W_render, H_render;
    int W_ref_depth, H_ref_depth;
    int W_ref_normal, H_ref_normal;
    int W_ref_alpha, H_ref_alpha;
    int has_mask;
    uint in_flags;
    uint out_flags;
    uint wgs_per_row;     // pixel axis folds across (wg.x, wg.z); wg.y = batch
};

[ForceInline]
bool _ms_finite(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

[ForceInline]
uint _ms_load_u8(uint* buf, uint idx) {
    return u8_load(buf, idx);
}

// ---------------------------------------------------------------------------
// Bilinear sampling of GT modalities at a different resolution, mirroring
// core/Interpolation.cuh (align-corners=false half-pixel-center taps with
// border clamping). Equal shapes decode to a single-tap diagonal read.
// ---------------------------------------------------------------------------

[ForceInline]
void _ms_taps(int x_dst, int y_dst, int W_dst, int H_dst, int W_src, int H_src,
              out int x0, out int y0, out int x1, out int y1,
              out float w00, out float w01, out float w10, out float w11) {
    float u = (float(x_dst) + 0.5f) * float(W_src) / float(W_dst) - 0.5f;
    float v = (float(y_dst) + 0.5f) * float(H_src) / float(H_dst) - 0.5f;
    int x0_raw = int(floor(u));
    int y0_raw = int(floor(v));
    float fx = u - float(x0_raw);
    float fy = v - float(y0_raw);
    x0 = max(0, min(W_src - 1, x0_raw));
    y0 = max(0, min(H_src - 1, y0_raw));
    x1 = max(0, min(W_src - 1, x0_raw + 1));
    y1 = max(0, min(H_src - 1, y0_raw + 1));
    w00 = (1.0f - fx) * (1.0f - fy);
    w10 = fx * (1.0f - fy);
    w01 = (1.0f - fx) * fy;
    w11 = fx * fy;
}

[ForceInline]
float3 _ms_load_f3(float* src, gindex_t i) {
    return float3(src[3 * i], src[3 * i + 1], src[3 * i + 2]);
}

// Interpolation.cuh revalidate_weights mirror: the "no ground truth here"
// sentinel (0 depth, black normal) is dropped from the blend rather than
// averaged into it. False means the destination pixel has no GT at all.
[ForceInline]
bool _ms_revalidate(bool v00, bool v10, bool v01, bool v11, inout float w00,
                    inout float w10, inout float w01, inout float w11) {
    if (!v00) w00 = 0.0f;
    if (!v10) w10 = 0.0f;
    if (!v01) w01 = 0.0f;
    if (!v11) w11 = 0.0f;
    float sw = w00 + w10 + w01 + w11;
    if (sw <= 0.0f) return false;
    float inv = 1.0f / sw;
    w00 *= inv; w10 *= inv; w01 *= inv; w11 *= inv;
    return true;
}

[ForceInline]
bool _ms_gt_depth_valid(float d) { return d > 0.0f; }

[ForceInline]
bool _ms_gt_normal_valid(float3 n) {
    // core/Interpolation.cuh's gt_normal_valid.
    return n.x + n.y + n.z > -2.366f && dot(n, n) > 0.25f;
}

[ForceInline]
float _ms_sample_gt_depth(float* src, int b, int x_dst, int y_dst, int W_dst,
                          int H_dst, int W_src, int H_src) {
    int x0, y0, x1, y1;
    float w00, w01, w10, w11;
    _ms_taps(x_dst, y_dst, W_dst, H_dst, W_src, H_src, x0, y0, x1, y1, w00,
             w01, w10, w11);
    gindex_t plane = (gindex_t)b * (gindex_t)H_src * (gindex_t)W_src;
    float v00 = src[plane + y0 * W_src + x0];
    float v10 = src[plane + y0 * W_src + x1];
    float v01 = src[plane + y1 * W_src + x0];
    float v11 = src[plane + y1 * W_src + x1];
    if (!_ms_revalidate(_ms_gt_depth_valid(v00), _ms_gt_depth_valid(v10),
                        _ms_gt_depth_valid(v01), _ms_gt_depth_valid(v11),
                        w00, w10, w01, w11))
        return 0.0f;
    return w00 * v00 + w10 * v10 + w01 * v01 + w11 * v11;
}

[ForceInline]
float3 _ms_sample_gt_normal(float* src, int b, int x_dst, int y_dst, int W_dst,
                            int H_dst, int W_src, int H_src) {
    int x0, y0, x1, y1;
    float w00, w01, w10, w11;
    _ms_taps(x_dst, y_dst, W_dst, H_dst, W_src, H_src, x0, y0, x1, y1, w00,
             w01, w10, w11);
    gindex_t plane = (gindex_t)b * (gindex_t)H_src * (gindex_t)W_src;
    float3 v00 = _ms_load_f3(src, plane + y0 * W_src + x0);
    float3 v10 = _ms_load_f3(src, plane + y0 * W_src + x1);
    float3 v01 = _ms_load_f3(src, plane + y1 * W_src + x0);
    float3 v11 = _ms_load_f3(src, plane + y1 * W_src + x1);
    if (!_ms_revalidate(_ms_gt_normal_valid(v00), _ms_gt_normal_valid(v10),
                        _ms_gt_normal_valid(v01), _ms_gt_normal_valid(v11),
                        w00, w10, w01, w11))
        return float3(-1.0f);
    return w00 * v00 + w10 * v10 + w01 * v01 + w11 * v11;
}

[ForceInline]
void _ms_scatter_add_gt_depth(float* src, float* dst, int b, int x_dst,
                              int y_dst, int W_dst, int H_dst, int W_src,
                              int H_src, float dvalue) {
    int x0, y0, x1, y1;
    float w00, w01, w10, w11;
    _ms_taps(x_dst, y_dst, W_dst, H_dst, W_src, H_src, x0, y0, x1, y1, w00,
             w01, w10, w11);
    gindex_t plane = (gindex_t)b * (gindex_t)H_src * (gindex_t)W_src;
    gindex_t i00 = plane + y0 * W_src + x0;
    gindex_t i10 = plane + y0 * W_src + x1;
    gindex_t i01 = plane + y1 * W_src + x0;
    gindex_t i11 = plane + y1 * W_src + x1;
    if (!_ms_revalidate(_ms_gt_depth_valid(src[i00]), _ms_gt_depth_valid(src[i10]),
                        _ms_gt_depth_valid(src[i01]), _ms_gt_depth_valid(src[i11]),
                        w00, w10, w01, w11))
        return;
    if (w00 != 0.0f) atomic_add_f32(dst + i00, w00 * dvalue);
    if (w10 != 0.0f) atomic_add_f32(dst + i10, w10 * dvalue);
    if (w01 != 0.0f) atomic_add_f32(dst + i01, w01 * dvalue);
    if (w11 != 0.0f) atomic_add_f32(dst + i11, w11 * dvalue);
}

[ForceInline]
void _ms_scatter_add_gt_normal(float* src, float* dst, int b, int x_dst,
                               int y_dst, int W_dst, int H_dst, int W_src,
                               int H_src, float3 dvalue) {
    int x0, y0, x1, y1;
    float w00, w01, w10, w11;
    _ms_taps(x_dst, y_dst, W_dst, H_dst, W_src, H_src, x0, y0, x1, y1, w00,
             w01, w10, w11);
    gindex_t plane = (gindex_t)b * (gindex_t)H_src * (gindex_t)W_src;
    gindex_t idx[4] = { plane + y0 * W_src + x0, plane + y0 * W_src + x1,
                        plane + y1 * W_src + x0, plane + y1 * W_src + x1 };
    if (!_ms_revalidate(_ms_gt_normal_valid(_ms_load_f3(src, idx[0])),
                        _ms_gt_normal_valid(_ms_load_f3(src, idx[1])),
                        _ms_gt_normal_valid(_ms_load_f3(src, idx[2])),
                        _ms_gt_normal_valid(_ms_load_f3(src, idx[3])),
                        w00, w10, w01, w11))
        return;
    float w[4] = { w00, w10, w01, w11 };
    for (int k = 0; k < 4; k++) {
        if (w[k] == 0.0f) continue;
        float* q = dst + 3 * idx[k];
        atomic_add_f32(q + 0, w[k] * dvalue.x);
        atomic_add_f32(q + 1, w[k] * dvalue.y);
        atomic_add_f32(q + 2, w[k] * dvalue.z);
    }
}

[ForceInline]
bool _ms_nearest_b(uint* src, int b, int x_dst, int y_dst, int W_dst,
                   int H_dst, int W_src, int H_src) {
    float u = (float(x_dst) + 0.5f) * float(W_src) / float(W_dst) - 0.5f;
    float v = (float(y_dst) + 0.5f) * float(H_src) / float(H_dst) - 0.5f;
    int x = max(0, min(W_src - 1, int(floor(u + 0.5f))));
    int y = max(0, min(H_src - 1, int(floor(v + 0.5f))));
    uint plane = uint(b) * uint(H_src) * uint(W_src);
    return _ms_load_u8(src, plane + uint(y) * uint(W_src) + uint(x)) != 0u;
}

// Which accumulator row(s) a raw-loss index adds into (mirrors
// PerPixelLoss.cu atomicAddWhich): 0 = global row only, 1 = per-image row
// only, 2 = both.
[ForceInline]
int _ms_add_which(int i) {
    if (i == int(RawLossIndex::DepthSupX) || i == int(RawLossIndex::DepthSupY) ||
        i == int(RawLossIndex::DepthSupXX) ||
        i == int(RawLossIndex::DepthSupYY) || i == int(RawLossIndex::DepthSupXY))
        return 1;
    if (i == int(RawLossIndex::DepthMaskTotal))
        return 2;
    return 0;
}

// Shared input loading for fwd/bwd: buffer reads guarded by presence flags
// with the CUDA kernels' per-modality null defaults, GT modalities bilinear-
// sampled at their own resolution.
[ForceInline]
void _ms_load_pixel(PplParams p, int batch_idx, gindex_t idx, int x_dst,
                    int y_dst, out float3 render_rgb, out float3 ref_rgb,
                    out float render_depth, out float ref_depth_v,
                    out float3 render_normal, out float3 depth_normal,
                    out float3 ref_normal_v, out float render_Ts,
                    out float3 rgb_dist, out float depth_dist,
                    out float3 normal_dist, out float median_depth,
                    out float3 median_normal, out bool ref_alpha_v) {
    render_rgb = (p.in_flags & kInRenderRgb) != 0 ? _ms_load_f3(p.render_rgb, idx) : float3(0.0f);
    ref_rgb = (p.in_flags & kInRefRgb) != 0 ? _ms_load_f3(p.ref_rgb, idx) : float3(0.0f);
    render_depth = (p.in_flags & kInRenderDepth) != 0 ? p.render_depth[idx] : 1.0f;
    ref_depth_v = 1.0f;
    if ((p.in_flags & kInRefDepth) != 0)
        ref_depth_v = _ms_sample_gt_depth(p.ref_depth, batch_idx, x_dst,
                                          y_dst, p.W_render, p.H_render,
                                          p.W_ref_depth, p.H_ref_depth);
    render_normal = (p.in_flags & kInRenderNormal) != 0 ? _ms_load_f3(p.render_normal, idx) : float3(0.0f);
    depth_normal = (p.in_flags & kInDepthNormal) != 0 ? _ms_load_f3(p.depth_normal, idx) : float3(0.0f);
    ref_normal_v = float3(0.0f);
    if ((p.in_flags & kInRefNormal) != 0)
        ref_normal_v = _ms_sample_gt_normal(p.ref_normal, batch_idx, x_dst,
                                            y_dst, p.W_render, p.H_render,
                                            p.W_ref_normal, p.H_ref_normal);
    render_Ts = (p.in_flags & kInRenderTs) != 0 ? p.render_Ts[idx] : 1.0f;
    rgb_dist = (p.in_flags & kInRgbDist) != 0 ? _ms_load_f3(p.rgb_dist, idx) : float3(0.0f);
    depth_dist = (p.in_flags & kInDepthDist) != 0 ? p.depth_dist[idx] : 0.0f;
    normal_dist = (p.in_flags & kInNormalDist) != 0 ? _ms_load_f3(p.normal_dist, idx) : float3(0.0f);
    median_depth = (p.in_flags & kInMedianDepth) != 0 ? p.median_depth[idx] : 0.0f;
    median_normal = (p.in_flags & kInMedianNormal) != 0 ? _ms_load_f3(p.median_normal, idx) : float3(0.0f);
    ref_alpha_v = false;
    if ((p.in_flags & kInRefAlpha) != 0)
        ref_alpha_v = _ms_nearest_b(p.ref_alpha, batch_idx, x_dst, y_dst,
                                    p.W_render, p.H_render, p.W_ref_alpha,
                                    p.H_ref_alpha);
}

groupshared float _ms_red[256];

// Workgroup sum of `v`, valid on thread 0. Two barriers, against the
// groupshared tree's one per level -- ppl_fwd reduces once per raw loss, so
// the tree cost ~200 workgroup barriers a launch and this costs 40.
float _ms_block_sum(float v, uint tid) {
    uint wsize = WaveGetLaneCount();
    float wsum = WaveActiveSum(v);
    GroupMemoryBarrierWithGroupSync();  // previous call done reading _ms_red
    if (WaveIsFirstLane()) _ms_red[tid / wsize] = wsum;
    GroupMemoryBarrierWithGroupSync();
    float total = 0.0f;
    if (tid < wsize) {  // wave 0, whole-wave uniform
        float part = 0.0f;
        for (uint k = tid; k < 256 / wsize; k += wsize) part += _ms_red[k];
        total = WaveActiveSum(part);
    }
    return total;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ppl_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
             uniform PplParams* ppp) {
    PplParams p = *ppp;
    uint pixel_idx = (wg.z * p.wgs_per_row + wg.x) * 256 + tid;
    uint batch_idx = wg.y;
    if (batch_idx >= uint(p.B)) return;
    gindex_t idx = (gindex_t)batch_idx * (gindex_t)p.ppi + (gindex_t)pixel_idx;

    int x_dst = int(pixel_idx % uint(p.W_render));
    int y_dst = int(pixel_idx / uint(p.W_render));

    float losses[kNRaw];
    for (int i = 0; i < kNRaw; i++) losses[i] = 0.0f;

    bool inside = pixel_idx < p.ppi;
    if (inside) {
        float3 render_rgb, ref_rgb, render_normal, depth_normal, ref_normal_v;
        float3 rgb_dist, normal_dist, median_normal;
        float render_depth, ref_depth_v, render_Ts, depth_dist, median_depth;
        bool ref_alpha_v;
        _ms_load_pixel(p, int(batch_idx), idx, x_dst, y_dst, render_rgb,
                       ref_rgb, render_depth, ref_depth_v, render_normal,
                       depth_normal, ref_normal_v, render_Ts, rgb_dist,
                       depth_dist, normal_dist, median_depth, median_normal,
                       ref_alpha_v);

        losses = per_pixel_losses(
            render_rgb, ref_rgb, render_depth, ref_depth_v, render_normal,
            depth_normal, ref_normal_v, render_Ts, rgb_dist, depth_dist,
            normal_dist, median_depth, median_normal, ref_alpha_v,
            p.has_mask != 0, p.weights);

        if ((p.in_flags & kInLossMap) != 0) {
            p.loss_map[idx] = abs(
                losses[int(RawLossIndex::RgbLoss)] +
                losses[int(RawLossIndex::RenderNormalSup)] +
                losses[int(RawLossIndex::DepthNormalSup)] +
                losses[int(RawLossIndex::AlphaSup)] +
                losses[int(RawLossIndex::AlphaSupUnder)] +
                losses[int(RawLossIndex::NormalReg)] +
                losses[int(RawLossIndex::AlphaReg)] +
                losses[int(RawLossIndex::RgbDistReg)] +
                losses[int(RawLossIndex::DepthDistReg)] +
                losses[int(RawLossIndex::NormalDistReg)] +
                losses[int(RawLossIndex::MeanMedianDepthSup)] +
                losses[int(RawLossIndex::MedianDepthNormalReg)] +
                losses[int(RawLossIndex::MedianNormalSup)] +
                losses[int(RawLossIndex::MedianRenderNormalReg)]);
        }
    }

    uint row_b = batch_idx;
    if ((p.in_flags & kInCam) != 0)
        row_b = uint(int(i64mem_get(p.camera_indices[batch_idx])));

    for (int i = 0; i < kNRaw; i++) {
        float loss = inside ? losses[i] : 0.0f;
        loss = _ms_finite(loss) ? loss : 0.0f;
        float sum = _ms_block_sum(loss, tid);
        if (tid == 0) {
            if (sum != 0.0f && _ms_finite(sum)) {
                int which = _ms_add_which(i);
                if (which == 0 || which == 2)
                    atomic_add_f32(p.losses + i, sum);
                if (which == 1 || which == 2)
                    atomic_add_f32(p.losses + (row_b + 1) * uint(kNRaw) + i,
                                   sum);
            }
        }
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ppl_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
             uniform PplParams* ppp) {
    PplParams p = *ppp;
    uint pixel_idx = (wg.z * p.wgs_per_row + wg.x) * 256 + tid;
    uint batch_idx = wg.y;
    if (batch_idx >= uint(p.B) || pixel_idx >= p.ppi) return;
    gindex_t idx = (gindex_t)batch_idx * (gindex_t)p.ppi + (gindex_t)pixel_idx;

    int x_dst = int(pixel_idx % uint(p.W_render));
    int y_dst = int(pixel_idx / uint(p.W_render));

    uint row_b = batch_idx;
    if ((p.in_flags & kInCam) != 0)
        row_b = uint(int(i64mem_get(p.camera_indices[batch_idx])));

    float v_losses[kNRaw];
    for (int i = 0; i < kNRaw; i++) {
        float temp = 0.0f;
        int which = _ms_add_which(i);
        if (which == 0 || which == 2) temp += p.losses[i];
        if (which == 1 || which == 2)
            temp += p.losses[(row_b + 1) * uint(kNRaw) + i];
        v_losses[i] = temp;
    }

    float3 render_rgb, ref_rgb, render_normal, depth_normal, ref_normal_v;
    float3 rgb_dist, normal_dist, median_normal;
    float render_depth, ref_depth_v, render_Ts, depth_dist, median_depth;
    bool ref_alpha_v;
    _ms_load_pixel(p, int(batch_idx), idx, x_dst, y_dst, render_rgb, ref_rgb,
                   render_depth, ref_depth_v, render_normal, depth_normal,
                   ref_normal_v, render_Ts, rgb_dist, depth_dist, normal_dist,
                   median_depth, median_normal, ref_alpha_v);

    float3 v_render_rgb, v_ref_rgb, v_render_normal, v_depth_normal;
    float3 v_ref_normal, v_rgb_dist, v_normal_dist, v_median_normal;
    float v_render_depth, v_ref_depth, v_render_Ts, v_depth_dist,
        v_median_depth;
    per_pixel_losses_bwd(
        render_rgb, ref_rgb, render_depth, ref_depth_v, render_normal,
        depth_normal, ref_normal_v, render_Ts, rgb_dist, depth_dist,
        normal_dist, median_depth, median_normal, ref_alpha_v,
        p.has_mask != 0, p.weights, v_losses, v_render_rgb, v_ref_rgb,
        v_render_depth, v_ref_depth, v_render_normal, v_depth_normal,
        v_ref_normal, v_render_Ts, v_rgb_dist, v_depth_dist, v_normal_dist,
        v_median_depth, v_median_normal);

    // Render-resolution outputs: one writer per pixel.
    if ((p.out_flags & kOutVRenderRgb) != 0) {
        p.v_render_rgb[3 * idx] = v_render_rgb.x;
        p.v_render_rgb[3 * idx + 1] = v_render_rgb.y;
        p.v_render_rgb[3 * idx + 2] = v_render_rgb.z;
    }
    if ((p.out_flags & kOutVRefRgb) != 0) {
        p.v_ref_rgb[3 * idx] = v_ref_rgb.x;
        p.v_ref_rgb[3 * idx + 1] = v_ref_rgb.y;
        p.v_ref_rgb[3 * idx + 2] = v_ref_rgb.z;
    }
    if ((p.out_flags & kOutVRenderDepth) != 0)
        p.v_render_depth[idx] = v_render_depth;
    if ((p.out_flags & kOutVRenderNormal) != 0) {
        p.v_render_normal[3 * idx] = v_render_normal.x;
        p.v_render_normal[3 * idx + 1] = v_render_normal.y;
        p.v_render_normal[3 * idx + 2] = v_render_normal.z;
    }
    if ((p.out_flags & kOutVDepthNormal) != 0) {
        p.v_depth_normal[3 * idx] = v_depth_normal.x;
        p.v_depth_normal[3 * idx + 1] = v_depth_normal.y;
        p.v_depth_normal[3 * idx + 2] = v_depth_normal.z;
    }
    if ((p.out_flags & kOutVRenderTs) != 0)
        p.v_render_Ts[idx] = v_render_Ts;
    if ((p.out_flags & kOutVRgbDist) != 0) {
        p.v_rgb_dist[3 * idx] = v_rgb_dist.x;
        p.v_rgb_dist[3 * idx + 1] = v_rgb_dist.y;
        p.v_rgb_dist[3 * idx + 2] = v_rgb_dist.z;
    }
    if ((p.out_flags & kOutVDepthDist) != 0)
        p.v_depth_dist[idx] = v_depth_dist;
    if ((p.out_flags & kOutVNormalDist) != 0) {
        p.v_normal_dist[3 * idx] = v_normal_dist.x;
        p.v_normal_dist[3 * idx + 1] = v_normal_dist.y;
        p.v_normal_dist[3 * idx + 2] = v_normal_dist.z;
    }
    if ((p.out_flags & kOutVMedianDepth) != 0)
        p.v_median_depth[idx] = v_median_depth;
    if ((p.out_flags & kOutVMedianNormal) != 0) {
        p.v_median_normal[3 * idx] = v_median_normal.x;
        p.v_median_normal[3 * idx + 1] = v_median_normal.y;
        p.v_median_normal[3 * idx + 2] = v_median_normal.z;
    }

    // GT-resolution outputs: bilinear atomic scatter (buffers pre-zeroed by
    // the host launcher).
    if ((p.out_flags & kOutVRefDepth) != 0)
        _ms_scatter_add_gt_depth(p.ref_depth, p.v_ref_depth, int(batch_idx),
                                 x_dst, y_dst, p.W_render, p.H_render,
                                 p.W_ref_depth, p.H_ref_depth, v_ref_depth);
    if ((p.out_flags & kOutVRefNormal) != 0)
        _ms_scatter_add_gt_normal(p.ref_normal, p.v_ref_normal,
                                  int(batch_idx), x_dst, y_dst, p.W_render,
                                  p.H_render, p.W_ref_normal, p.H_ref_normal,
                                  v_ref_normal);
}

// ---------------------------------------------------------------------------
// Raw-loss -> weighted-loss reduce (fwd + bwd), mirroring the
// per_pixel_losses_reduce_* kernels. Thread = accumulator row (0 = global,
// 1..N = per train image); grid covers num_train_images + 1 rows.
// ---------------------------------------------------------------------------

struct PplReduceParams {
    float* raw_losses;   // [N+1, kNRaw]
    float* io;           // fwd: out losses [kNL]; bwd: in v_losses [kNL]
    float* out;          // bwd only: v_raw_losses [N+1, kNRaw]
    float weights[kNW];
    int batch_size;      // num_train_images
};

[ForceInline]
bool _ms_is_total_reduce(int i) {
    return i != int(LossIndex::DepthSup);
}

[shader("compute")]
[numthreads(32, 1, 1)]
void ppl_reduce_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform PplReduceParams p) {
    uint batch_idx = wg.x * 32 + tid;
    if (batch_idx > uint(p.batch_size)) return;

    float raw[kNRaw];
    for (int i = 0; i < kNRaw; i++)
        raw[i] = p.raw_losses[batch_idx * uint(kNRaw) + i];

    float losses[kNL] = per_pixel_losses_reduce(raw, p.weights);
    for (int i = 0; i < kNL; i++) {
        if (losses[i] != 0.0f && _ms_finite(losses[i])) {
            if (_ms_is_total_reduce(i)) {
                if (batch_idx == 0) p.io[i] = losses[i];
            } else if (batch_idx != 0) {
                atomic_add_f32(p.io + i, losses[i] / float(p.batch_size));
            }
        }
    }
}

[shader("compute")]
[numthreads(32, 1, 1)]
void ppl_reduce_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform PplReduceParams p) {
    uint batch_idx = wg.x * 32 + tid;
    if (batch_idx > uint(p.batch_size)) return;

    float v_losses[kNL];
    for (int i = 0; i < kNL; i++) {
        float temp = 0.0f;
        if (_ms_is_total_reduce(i)) {
            if (batch_idx == 0) temp = p.io[i];
        } else if (batch_idx != 0) {
            temp = p.io[i] / float(p.batch_size);
        }
        v_losses[i] = temp;
    }

    float raw[kNRaw];
    for (int i = 0; i < kNRaw; i++)
        raw[i] = p.raw_losses[batch_idx * uint(kNRaw) + i];

    float v_raw[kNRaw] = per_pixel_losses_reduce_bwd(raw, p.weights, v_losses);
    for (int i = 0; i < kNRaw; i++)
        p.out[batch_idx * uint(kNRaw) + i] = v_raw[i];
}

// ---------------------------------------------------------------------------
// Image-pyramid helpers: 2x average-pool down/up sampling + the tiny
// loss-vector accumulate, mirroring the avg_pool_* / vector_add_scaled
// kernels. 16x16 logical tiles run as flat 256-wide workgroups.
// ---------------------------------------------------------------------------

struct MsPoolParams {
    float* hs;   // high-res image
    float* ls;   // low-res image
    int B, C;
    int hsH, hsW;
    int lsH, lsW;
    int scale;   // upsample only: 1 << scale_index
    float a, b;  // upsample only
    int _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_downsample_f(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform MsPoolParams p) {
    uint xid = wg.x * 16 + (tid % 16u);
    uint yid = wg.y * 16 + (tid / 16u);
    uint bid = wg.z;
    if (yid >= uint(p.lsH) || xid >= uint(p.lsW)) return;
    // Clamp the odd-row/col tail (a floor-halved dst never reads it when the
    // src dim is even; clamping keeps the odd-dim edge case in-bounds).
    uint y0 = min(2 * yid, uint(p.hsH - 1)), y1 = min(2 * yid + 1, uint(p.hsH - 1));
    uint x0 = min(2 * xid, uint(p.hsW - 1)), x1 = min(2 * xid + 1, uint(p.hsW - 1));
    for (int c = 0; c < p.C; c++) {
        gindex_t base = ((gindex_t)bid * p.hsH) * (gindex_t)p.hsW;
        float v = p.hs[((base + y0 * p.hsW + x0)) * p.C + c] +
                  p.hs[((base + y0 * p.hsW + x1)) * p.C + c] +
                  p.hs[((base + y1 * p.hsW + x0)) * p.C + c] +
                  p.hs[((base + y1 * p.hsW + x1)) * p.C + c];
        gindex_t obase = ((gindex_t)bid * p.lsH + yid) * (gindex_t)p.lsW + xid;
        p.ls[obase * p.C + c] = 0.25f * v;
    }
}

// GT geometry does not pool like an image: a 2x2 block that mixes real values
// with the "no ground truth here" sentinel (0 depth, black normal) must not
// average the two. C picks the rule -- 1 = depth, 3 = normal.
[shader("compute")]
[numthreads(256, 1, 1)]
void ms_downsample_gt_geo(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform MsPoolParams p) {
    uint xid = wg.x * 16 + (tid % 16u);
    uint yid = wg.y * 16 + (tid / 16u);
    uint bid = wg.z;
    if (yid >= uint(p.lsH) || xid >= uint(p.lsW)) return;
    uint ys[2] = { min(2 * yid, uint(p.hsH - 1)), min(2 * yid + 1, uint(p.hsH - 1)) };
    uint xs[2] = { min(2 * xid, uint(p.hsW - 1)), min(2 * xid + 1, uint(p.hsW - 1)) };
    gindex_t base = ((gindex_t)bid * p.hsH) * (gindex_t)p.hsW;
    float3 acc = float3(0.0f);
    int n = 0;
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++) {
            gindex_t i = (base + ys[dy] * p.hsW + xs[dx]) * p.C;
            float3 v = float3(p.hs[i], 0.0f, 0.0f);
            if (p.C == 3) v = float3(p.hs[i], p.hs[i + 1], p.hs[i + 2]);
            bool ok = p.C == 1 ? _ms_gt_depth_valid(v.x) : _ms_gt_normal_valid(v);
            if (!ok) continue;
            acc += v;
            n++;
        }
    gindex_t obase = (((gindex_t)bid * p.lsH + yid) * (gindex_t)p.lsW + xid) * p.C;
    float3 outv = n != 0 ? acc / float(n) : float3(p.C == 1 ? 0.0f : -1.0f);
    p.ls[obase] = outv.x;
    if (p.C == 3) { p.ls[obase + 1] = outv.y; p.ls[obase + 2] = outv.z; }
}


// RGB does not pool across a mask edge either: a box mean of two background
// pixels and two distractor pixels is a colour the render is then asked to
// match. Averaging only the unmasked children keeps every scale comparable.
struct MsPoolMaskedParams {
    float* hs;
    float* ls;
    uint* mask;  // [B,hsH,hsW] bool bytes
    int B, C;
    int hsH, hsW;
    int lsH, lsW;
    int _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_downsample_masked_f(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform MsPoolMaskedParams p) {
    uint xid = wg.x * 16 + (tid % 16u);
    uint yid = wg.y * 16 + (tid / 16u);
    uint bid = wg.z;
    if (yid >= uint(p.lsH) || xid >= uint(p.lsW)) return;
    uint ys[2] = { min(2 * yid, uint(p.hsH - 1)), min(2 * yid + 1, uint(p.hsH - 1)) };
    uint xs[2] = { min(2 * xid, uint(p.hsW - 1)), min(2 * xid + 1, uint(p.hsW - 1)) };
    gindex_t base = ((gindex_t)bid * p.hsH) * (gindex_t)p.hsW;
    float acc[3] = { 0.0f, 0.0f, 0.0f };
    float box[3] = { 0.0f, 0.0f, 0.0f };
    int n = 0;
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++) {
            gindex_t i = base + ys[dy] * p.hsW + xs[dx];
            bool valid = u8_load(p.mask, (uint)i) != 0u;
            for (int c = 0; c < p.C; c++) {
                float v = p.hs[i * p.C + c];
                box[c] += 0.25f * v;
                if (valid) acc[c] += v;
            }
            n += valid ? 1 : 0;
        }
    gindex_t obase = ((gindex_t)bid * p.lsH + yid) * (gindex_t)p.lsW + xid;
    // All four masked: the coarse pixel is masked too, so the value is unused.
    for (int c = 0; c < p.C; c++)
        p.ls[obase * p.C + c] = n != 0 ? acc[c] / float(n) : box[c];
}

// The coarse-scale grad is scattered back uniformly over all four children,
// masked ones included -- the one path that lets a masked pixel be trained
// toward the distractor the mask exists to ignore.
struct MsZeroMaskedParams {
    float* grad;
    uint* mask;
    int B, C, H, W;
    int _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_zero_masked_grad(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform MsZeroMaskedParams p) {
    uint xid = wg.x * 16 + (tid % 16u);
    uint yid = wg.y * 16 + (tid / 16u);
    uint bid = wg.z;
    if (yid >= uint(p.H) || xid >= uint(p.W)) return;
    gindex_t i = ((gindex_t)bid * p.H + yid) * (gindex_t)p.W + xid;
    if (u8_load(p.mask, (uint)i) != 0u) return;
    for (int c = 0; c < p.C; c++)
        p.grad[i * p.C + c] = 0.0f;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_upsample_f(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform MsPoolParams p) {
    uint xid = wg.x * 16 + (tid % 16u);
    uint yid = wg.y * 16 + (tid / 16u);
    uint bid = wg.z;
    if (yid >= uint(p.hsH) || xid >= uint(p.hsW)) return;
    for (int c = 0; c < p.C; c++) {
        gindex_t hidx = (((gindex_t)bid * p.hsH + yid) * (gindex_t)p.hsW + xid) *
                           p.C + c;
        float v = (p.a == 0.0f) ? 0.0f : p.a * p.hs[hidx];
        uint yl = yid / uint(p.scale), xl = xid / uint(p.scale);
        if (yl < uint(p.lsH) && xl < uint(p.lsW)) {
            gindex_t lidx =
                (((gindex_t)bid * p.lsH + yl) * (gindex_t)p.lsW + xl) * p.C + c;
            v += p.b * p.ls[lidx];
        }
        p.hs[hidx] = v;
    }
}

// Bool ([B,H,W] bytes) 2x downsample: majority (>= 2 of 4). One thread per
// output WORD so no byte-granular writes race; the tail word is patched with
// And/Or atomics to leave bytes past the logical end untouched.
struct MsPoolBoolParams {
    uint* src;   // bool bytes
    uint* dst;   // bool bytes
    int B;
    int hsH, hsW;
    int lsH, lsW;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_downsample_b(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform MsPoolBoolParams p) {
    uint total = uint(p.B) * uint(p.lsH) * uint(p.lsW);
    uint n_words = (total + 3u) / 4u;
    uint word = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (word >= n_words) return;

    uint out_w = 0u;
    uint covered = 0u;
    for (uint k = 0; k < 4u; k++) {
        uint byte_idx = 4u * word + k;
        if (byte_idx >= total) break;
        covered |= 0xffu << (8u * k);
        uint hw = uint(p.lsH) * uint(p.lsW);
        uint bid = byte_idx / hw;
        uint rem = byte_idx - bid * hw;
        uint yid = rem / uint(p.lsW);
        uint xid = rem - yid * uint(p.lsW);
        uint y0 = min(2 * yid, uint(p.hsH - 1)),
             y1 = min(2 * yid + 1, uint(p.hsH - 1));
        uint x0 = min(2 * xid, uint(p.hsW - 1)),
             x1 = min(2 * xid + 1, uint(p.hsW - 1));
        uint plane = bid * uint(p.hsH) * uint(p.hsW);
        uint v = _ms_load_u8(p.src, plane + y0 * uint(p.hsW) + x0) +
                 _ms_load_u8(p.src, plane + y0 * uint(p.hsW) + x1) +
                 _ms_load_u8(p.src, plane + y1 * uint(p.hsW) + x0) +
                 _ms_load_u8(p.src, plane + y1 * uint(p.hsW) + x1);
        if (v >= 2u) out_w |= 1u << (8u * k);
    }
    if (covered == 0xffffffffu) {
        p.dst[word] = out_w;
    } else {
        // Tail word: only touch the covered bytes.
        InterlockedAnd(p.dst[word], ~covered);
        InterlockedOr(p.dst[word], out_w);
    }
}

struct MsAddScaledParams {
    float* dst;
    float* src;
    float scale;
    int n;
};

[shader("compute")]
[numthreads(32, 1, 1)]
void ms_add_scaled(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform MsAddScaledParams p) {
    uint i = wg.x * 32 + tid;
    if (i < uint(p.n)) p.dst[i] += p.src[i] * p.scale;
}


// ---------------------------------------------------------------------------
// Loss-map NMS (kernels/loss/PerPixelLoss.cu loss_map_nms_*_kernel)
// ---------------------------------------------------------------------------

// Sobel x; transposed for y. Must match _nms_sobel_3x3 in PerPixelLoss.cu.
static const float kNmsSobel[3][3] = {
    { -1.0f, 0.0f, 1.0f },
    { -2.0f, 0.0f, 2.0f },
    { -1.0f, 0.0f, 1.0f },
};

struct MsNmsParams {
    float* img;    // [B,H,W,1]
    uint* fails;   // [B,H,W] bytes, 0..2 sides outranking the pixel
    int B, H, W;
    float falloff;
};

static const int NMS_BLOCK = 16;
static const int NMS_HALO = 1;
static const int NMS_TILE = NMS_BLOCK + 2 * NMS_HALO;
groupshared float _nms_tile[NMS_TILE * NMS_TILE];

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_nms_mask(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                 uniform MsNmsParams p) {
    int lx = int(tid % 16u), ly = int(tid / 16u);
    int x0 = int(wg.x) * NMS_BLOCK, y0 = int(wg.y) * NMS_BLOCK;
    int bid = int(wg.z);

    for (int i = int(tid); i < NMS_TILE * NMS_TILE; i += 256) {
        int ty = i / NMS_TILE, tx = i % NMS_TILE;
        int yi = clamp(y0 + ty - NMS_HALO, 0, p.H - 1);
        int xi = clamp(x0 + tx - NMS_HALO, 0, p.W - 1);
        _nms_tile[i] = p.img[((gindex_t)bid * p.H + yi) * (gindex_t)p.W + xi];
    }
    GroupMemoryBarrierWithGroupSync();

    int xid = x0 + lx, yid = y0 + ly;
    if (yid >= p.H || xid >= p.W) return;

    int ty = ly + NMS_HALO, tx = lx + NMS_HALO;
    float gx = 0.0f, gy = 0.0f;
    for (int cy = -1; cy <= 1; cy++)
        for (int cx = -1; cx <= 1; cx++) {
            float v = _nms_tile[(ty + cy) * NMS_TILE + (tx + cx)];
            gx += kNmsSobel[cy + 1][cx + 1] * v;
            gy += kNmsSobel[cx + 1][cy + 1] * v;
        }

    uint n_fail = 0;
    float g = sqrt(gx * gx + gy * gy);
    if (g > 0.0f) {
        int dx = clamp(int(round(gx / g)), -NMS_HALO, NMS_HALO);
        int dy = clamp(int(round(gy / g)), -NMS_HALO, NMS_HALO);
        float v = _nms_tile[ty * NMS_TILE + tx];
        // Masked-out neighbours sit at 0, so they can only protect a pixel.
        n_fail = (v < _nms_tile[(ty + dy) * NMS_TILE + (tx + dx)] ? 1u : 0u) +
                 (v < _nms_tile[(ty - dy) * NMS_TILE + (tx - dx)] ? 1u : 0u);
    }
    gindex_t idx = ((gindex_t)bid * p.H + yid) * (gindex_t)p.W + xid;
    u8_store(p.fails, (uint)idx, n_fail);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ms_nms_apply(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                  uniform MsNmsParams p) {
    uint xid = wg.x * 16 + (tid % 16u);
    uint yid = wg.y * 16 + (tid / 16u);
    if (yid >= uint(p.H) || xid >= uint(p.W)) return;
    gindex_t idx = ((gindex_t)wg.z * p.H + yid) * (gindex_t)p.W + xid;
    uint n = u8_load(p.fails, (uint)idx);
    float m = n == 0u ? 1.0f : (n == 1u ? p.falloff : p.falloff * p.falloff);
    p.img[idx] *= m;
}
