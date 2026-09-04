// Dataset GT warp + re-distort + byte->float conversion kernels (Vulkan
// backend), mirroring kernels/pixelwise/ImageWarp.cu + GtDepthNormalWarp.cu
// + ImageRedistort.cu and the raw converters (uint8/16_*_to_float_raw),
// through the same canonical projection_utils.slang the CUDA kernels call.
//
// Layouts match the CUDA launchers: input [B, Hin, Win, C], warp output
// [B, K, Hout, Wout, C] float, post camera p = bid * K + ki with its unit
// frame in axes[p] and its face intrinsics in post_intrins[p]
// (kernels/pixelwise/WarpFace.cuh). Sub-word elements come out of u32 words
// and the mask warp writes whole u32 words (allocations are 16-byte rounded).

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "int8_compat.slang"
#include "shaders/projection_utils.slang"
#include "shaders/camera_source.slang"
#undef CudaDeviceExport
#include "dist_spec.slang"

// Input element kinds (the CUDA T_in template axis / elem_size dispatch).
static const int kElemU8 = 0;
static const int kElemU16 = 1;
static const int kElemF32 = 2;

static const float kWarpPi = 3.14159265358979323846f;

// Load element `idx` of the raw input buffer as an un-normalized float.
[ForceInline]
float _wp_load(uint* buf, gindex_t idx, int kind) {
    if (kind == kElemU8)
        return float(u8_load(buf, idx));
    if (kind == kElemU16) {
        uint w = buf[idx >> 1];
        return float((w >> ((uint(idx) & 1u) * 16u)) & 0xffffu);
    }
    return asfloat(buf[idx]);
}

[ForceInline]
float _wp_tap(uint* buf, int kind, gindex_t plane, int H, int W, int C,
              int cid, int x_, int y_, float norm_inv, float padding) {
    if (x_ < 0 || x_ >= W || y_ < 0 || y_ >= H) return padding;
    gindex_t idx = (plane + gindex_t(y_) * W + x_) * C + cid;
    return _wp_load(buf, idx, kind) * norm_inv;
}

// wrap_u variant: x is already wrapped, and y clamps rather than pads (the
// pole half-row -- see bilinear_byte_norm_wrap_u).
[ForceInline]
float _wp_tap_wrap(uint* buf, int kind, gindex_t plane, int H, int W, int C,
                   int cid, int x_, int y_, float norm_inv) {
    y_ = clamp(y_, 0, H - 1);
    gindex_t idx = (plane + gindex_t(y_) * W + x_) * C + cid;
    return _wp_load(buf, idx, kind) * norm_inv;
}

[ForceInline]
int _wp_wrap_x(int v, int W) {
    int r = v % W;
    if (r < 0) r += W;
    return r;
}

// bilinear_byte_norm / bilinear_byte_norm_wrap_u mirror: half-open taps with
// a constant padding value outside the image (wrap_u wraps x and clamps y,
// keeping the equirectangular seam at u = 0 / u = W continuous). `plane` is
// the [bid] pixel offset (bid * H * W). Non-wrap coordinates are pre-clamped
// to a window that preserves the all-taps-padding result -- float->int
// conversion of an unbounded projected uv is undefined, and the CUDA kernels
// only avoid it by accident of the `valid` guard.
[ForceInline]
float _wp_bilinear(uint* buf, int kind, gindex_t plane, int H, int W, int C,
                   int cid, float x, float y, float norm_inv, float padding,
                   bool wrap_u) {
    x -= 0.5f; y -= 0.5f;
    if (!wrap_u) {
        x = clamp(x, -8.0f, float(W) + 8.0f);
        y = clamp(y, -8.0f, float(H) + 8.0f);
    }
    int x0 = int(floor(x)), x1 = x0 + 1;
    int y0 = int(floor(y)), y1 = y0 + 1;
    float wx1 = x - float(x0), wx0 = 1.0f - wx1;
    float wy1 = y - float(y0), wy0 = 1.0f - wy1;
    if (wrap_u) {
        x0 = _wp_wrap_x(x0, W);
        x1 = _wp_wrap_x(x1, W);
    }
    if (wrap_u)
        return _wp_tap_wrap(buf, kind, plane, H, W, C, cid, x0, y0, norm_inv) * (wx0 * wy0)
             + _wp_tap_wrap(buf, kind, plane, H, W, C, cid, x1, y0, norm_inv) * (wx1 * wy0)
             + _wp_tap_wrap(buf, kind, plane, H, W, C, cid, x0, y1, norm_inv) * (wx0 * wy1)
             + _wp_tap_wrap(buf, kind, plane, H, W, C, cid, x1, y1, norm_inv) * (wx1 * wy1);
    return _wp_tap(buf, kind, plane, H, W, C, cid, x0, y0, norm_inv, padding) * (wx0 * wy0)
         + _wp_tap(buf, kind, plane, H, W, C, cid, x1, y0, norm_inv, padding) * (wx1 * wy0)
         + _wp_tap(buf, kind, plane, H, W, C, cid, x0, y1, norm_inv, padding) * (wx0 * wy1)
         + _wp_tap(buf, kind, plane, H, W, C, cid, x1, y1, norm_inv, padding) * (wx1 * wy1);
}

// Mirrors bilinear_depth_valid / bilinear_normal_valid in
// core/Interpolation.cuh: a GT sentinel tap is dropped, not averaged in.
// `w` is indexed q = 2*dy + dx.
struct _WpTaps {
    int x[2], y[2];
    float w[4];
    bool x_ok[2];
};

[ForceInline]
_WpTaps _wp_taps(float x, float y, int W, bool wrap_u) {
    _WpTaps t;
    x -= 0.5f; y -= 0.5f;
    if (!wrap_u) {
        // float->int of an unbounded projected uv is undefined; the window is
        // wide enough that every tap still lands out of frame.
        x = clamp(x, -8.0f, float(W) + 8.0f);
        y = clamp(y, -1e6f, 1e6f);
    }
    int x0 = int(floor(x)), y0 = int(floor(y));
    float fx = x - float(x0), fy = y - float(y0);
    t.y[0] = y0; t.y[1] = y0 + 1;
    t.w[0] = (1.0f - fx) * (1.0f - fy);
    t.w[1] = fx * (1.0f - fy);
    t.w[2] = (1.0f - fx) * fy;
    t.w[3] = fx * fy;
    [ForceUnroll]
    for (int k = 0; k < 2; k++) {
        int xv = x0 + k;
        if (wrap_u) {
            t.x[k] = _wp_wrap_x(xv, W);
            t.x_ok[k] = true;
        } else {
            t.x[k] = xv;
            t.x_ok[k] = xv >= 0 && xv < W;
        }
    }
    return t;
}

// False (and out 0) when no tap carries depth.
[ForceInline]
bool _wp_bilinear_depth(uint* buf, int kind, gindex_t plane, int H, int W,
                        float x, float y, float norm_inv, bool wrap_u,
                        out float outv) {
    _WpTaps t = _wp_taps(x, y, W, wrap_u);
    float acc = 0.0f, wsum = 0.0f;
    for (int q = 0; q < 4; q++) {
        int yv = t.y[q >> 1], xi = q & 1;
        if (!t.x_ok[xi] || yv < 0 || yv >= H) continue;
        gindex_t idx = plane + gindex_t(yv) * W + t.x[xi];
        float d = _wp_load(buf, idx, kind) * norm_inv;
        if (d <= 0.0f) continue;
        acc += t.w[q] * d;
        wsum += t.w[q];
    }
    outv = wsum > 0.0f ? acc / wsum : 0.0f;
    return wsum > 0.0f;
}

// Not renormalized here: the callers rotate into their own frame first.
[ForceInline]
bool _wp_bilinear_normal(uint* buf, int kind, gindex_t plane, int H, int W,
                         float x, float y, float norm_inv, float decode_off,
                         bool wrap_u, out float3 outn) {
    _WpTaps t = _wp_taps(x, y, W, wrap_u);
    float3 acc = float3(0.0f);
    float wsum = 0.0f;
    for (int q = 0; q < 4; q++) {
        int yv = t.y[q >> 1], xi = q & 1;
        if (!t.x_ok[xi] || yv < 0 || yv >= H) continue;
        gindex_t base = (plane + gindex_t(yv) * W + t.x[xi]) * 3;
        float3 n = float3(_wp_load(buf, base + 0, kind),
                          _wp_load(buf, base + 1, kind),
                          _wp_load(buf, base + 2, kind)) * norm_inv + decode_off;
        // GT normal validity: core/Interpolation.cuh's gt_normal_valid.
        if (n.x + n.y + n.z <= -2.366f || dot(n, n) <= 0.25f) continue;
        acc += t.w[q] * n;
        wsum += t.w[q];
    }
    outn = wsum > 0.0f ? acc / wsum : float3(-1.0f);
    return wsum > 0.0f;
}

[ForceInline]
float4 _wp_intrin(float* intrins, int bid) {
    return float4(intrins[bid * 4 + 0], intrins[bid * 4 + 1],
                  intrins[bid * 4 + 2], intrins[bid * 4 + 3]);
}

[ForceInline]
void _wp_axes(float* axes, int p, out float3 ax, out float3 ay,
              out float3 az) {
    int b = 9 * p;
    ax = float3(axes[b + 0], axes[b + 1], axes[b + 2]);
    ay = float3(axes[b + 3], axes[b + 4], axes[b + 5]);
    az = float3(axes[b + 6], axes[b + 7], axes[b + 8]);
}

// face_pixel_ray mirror: pixel (i, j) of post camera p through the face's own
// pinhole intrinsics, then its frame.
[ForceInline]
float3 _wp_face_ray(float* axes, float* post_intrins, int p, int i, int j) {
    float4 k = _wp_intrin(post_intrins, p);
    float u = (float(i) + 0.5f - k.z) / k.x;
    float v = (float(j) + 0.5f - k.w) / k.y;
    float3 ax, ay, az;
    _wp_axes(axes, p, ax, ay, az);
    return az + u * ax + v * ay;
}

// Camera-model dispatch matching the CUDA kernels' ternary chain; the
// distortion tier comes from kDistortion.
[ForceInline]
bool _wp_proj<D : ICameraDistortion>(int camera_model, float3 raydir,
                                     float4 intrin, float* dist_coeffs,
                                     int bid, out float2 uv) {
    float dc[D.kNumParams];
    load_dist_coeffs<D>(dist_coeffs, uint(bid), dc);
    if (camera_model == int(CameraModelType::FISHEYE))
        return fisheye_proj<D, false>(raydir, intrin, dc, uv);
    if (camera_model == int(CameraModelType::EQUISOLID))
        return equisolid_proj<D, false>(raydir, intrin, dc, uv);
    return persp_proj<D, false>(raydir, intrin, dc, uv);
}

// Ray -> pixel of image `bid` through its TRUE COLMAP lens model, for cameras
// the parser could only fit onto a tier (see shaders/camera_source.slang).
[ForceInline]
bool _wp_source_proj(int* source_models, float* source_params, int bid,
                     float3 raydir, out float2 uv) {
    float sp[kSourceMaxParams];
    [ForceUnroll]
    for (int k = 0; k < kSourceMaxParams; k++)
        sp[k] = source_params[kSourceMaxParams * bid + k];
    return source_project(source_models[bid], sp, raydir, uv);
}

// RayToPixel mirror: kFromSource resamples from the source projection instead
// of the fitted one.
[ForceInline]
bool _wp_to_pixel<D : ICameraDistortion>(int camera_model, float3 raydir,
                                         float4 intrin, float* dist_coeffs,
                                         int* source_models,
                                         float* source_params, int bid,
                                         out float2 uv) {
    if (kFromSource != 0)
        return _wp_source_proj(source_models, source_params, bid, raydir, uv);
    return _wp_proj<D>(camera_model, raydir, intrin, dist_coeffs, bid, uv);
}

// equi_ray_to_uv mirror: false past the rows the panorama holds, with a
// pixel of slack at the poles.
[ForceInline]
bool _wp_equi_uv(float3 raydir, int h, int w, out float2 uv) {
    float f = float(w) * (0.5f / kWarpPi);
    uv = float2(
        0.5f * float(w) + f * atan2(raydir.x, raydir.z),
        0.5f * float(h) + f * atan2(raydir.y, length(float2(raydir.x, raydir.z))));
    return uv.y >= -1.0f && uv.y <= float(h) + 1.0f;
}

// _wide_depth_to_face_ray_depth mirror: linear (z) input divides by cos(theta)
// off the INPUT axis, and past 85 degrees that secant is too steep to supervise
// on. d <= 0 is the invalid sentinel.
[ForceInline]
float _wp_face_ray_depth(float d, float3 raydir, bool input_is_ray_depth) {
    if (d <= 0.0f) return 0.0f;
    if (input_is_ray_depth) return d;
    float rl = length(raydir);
    float rz = raydir.z;
    if (rz <= 0.08715574f * rl) return 0.0f;   // cos(85 deg)
    return d * rl / rz;
}

// _warp_one_normal mirror: sample a wide-frame normal (INPUT camera frame)
// and rotate it into the face frame; (-1,-1,-1) sentinel for "no data" /
// degenerate samples.
[ForceInline]
float3 _wp_one_normal(uint* src, int kind, gindex_t plane, int H, int W,
                      float x, float y, float norm_inv, float decode_off,
                      bool wrap_u, float* axes, int p) {
    float3 n;
    if (!_wp_bilinear_normal(src, kind, plane, H, W, x, y, norm_inv,
                             decode_off, wrap_u, n))
        return float3(-1.0f);
    float3 ax, ay, az;
    _wp_axes(axes, p, ax, ay, az);
    float3 r = float3(dot(ax, n), dot(ay, n), dot(az, n));
    float rl = length(r);
    if (rl <= 1e-8f) return float3(-1.0f);
    return r / rl;
}

// Shared params for the image / depth / normal warp and re-distort entries.
// 128 bytes: pushed directly (by value). Pointer fields are never null (zeros
// fallback); the *_equi entries statically ignore intrins / dist_coeffs.
struct WarpParams {
    float* intrins;        // [B, 4]
    float* dist_coeffs;    // [B, 8]
    int* source_models;    // [B] COLMAP model id (kFromSource / redistort)
    float* source_params;  // [B, 16] that model's parameters
    uint* src;             // [B, Hin, Win, C] u8 / u16 / f32 elements
    float* dst;            // [B, K, Hout, Wout, C] float
    float* axes;           // [B*K, 3, 3] face frames
    float* post_intrins;   // [B*K, 4] face intrinsics
    int B;
    int Hin;
    int Win;
    int C;
    int K;
    int Hout;
    int Wout;
    int elem_kind;     // kElemU8 / kElemU16 / kElemF32
    int camera_model;  // CameraModelType (wide entries)
    int ref_H;         // intrinsics reference resolution (the projected uv is
    int ref_W;         //   in this pixel space; every grid rescales from it)
    int ray_depth;     // depth entries: input already stores ray depth
    float norm_inv;
    float decode_off;  // normal entries: post-normalization offset
    float invalid;     // redistort entries: out-of-frame fill
    int _pad0;
};

// 16x16 logical tile decode: flat 256-wide workgroups (the pinned 32-wide
// subgroup divides 256), grid = (ceil(Wout/16), ceil(Hout/16), B).
[ForceInline]
bool _wp_tile(WarpParams p, uint3 wg, uint tid, out int i, out int j,
              out int bid) {
    i = int(wg.x) * 16 + int(tid % 16u);
    j = int(wg.y) * 16 + int(tid / 16u);
    bid = int(wg.z);
    return i < p.Wout && j < p.Hout;
}

[ForceInline]
void _wp_img_wide<D : ICameraDistortion>(WarpParams p, int i, int j, int bid) {
    float4 intrin = _wp_intrin(p.intrins, bid);
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    for (int ki = 0; ki < p.K; ki++) {
        float3 raydir = _wp_face_ray(p.axes, p.post_intrins, bid * p.K + ki, i, j);
        float2 uv;
        bool valid = _wp_to_pixel<D>(p.camera_model, raydir, intrin,
                                     p.dist_coeffs, p.source_models,
                                     p.source_params, bid, uv);
        gindex_t base = (((gindex_t(bid) * p.K + ki) * p.Hout + j) * p.Wout + i) * p.C;
        for (int c = 0; c < p.C; c++) {
            float v = 0.5f;
            if (valid)
                v = _wp_bilinear(p.src, p.elem_kind, plane, p.Hin, p.Win,
                                 p.C, c, uv.x, uv.y, p.norm_inv, 0.5f, false);
            p.dst[base + c] = v;
        }
    }
}

#define _wp_img_call(D) _wp_img_wide<D>(p, i, j, bid)

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_img_wide(uniform WarpParams p, uint3 wg: SV_GroupID,
                   uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    SS_DISPATCH_DIST(_wp_img_call)
}
#undef _wp_img_call

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_img_equi(uniform WarpParams p, uint3 wg: SV_GroupID,
                   uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    for (int ki = 0; ki < p.K; ki++) {
        float3 raydir = _wp_face_ray(p.axes, p.post_intrins, bid * p.K + ki, i, j);
        float2 uv;
        bool valid = _wp_equi_uv(raydir, p.Hin, p.Win, uv);
        gindex_t base = (((gindex_t(bid) * p.K + ki) * p.Hout + j) * p.Wout + i) * p.C;
        for (int c = 0; c < p.C; c++) {
            float v = 0.5f;
            if (valid)
                v = _wp_bilinear(p.src, p.elem_kind, plane, p.Hin, p.Win, p.C,
                                 c, uv.x, uv.y, p.norm_inv, 0.5f, true);
            p.dst[base + c] = v;
        }
    }
}

[ForceInline]
void _wp_depth_wide<D : ICameraDistortion>(WarpParams p, int i, int j,
                                           int bid) {
    float4 intrin = _wp_intrin(p.intrins, bid);
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    // Projected uv is in intrinsics-reference (RGB input) pixel space;
    // rescale to the GT depth map's own resolution.
    float sx = float(p.Win) / float(p.ref_W), sy = float(p.Hin) / float(p.ref_H);
    for (int ki = 0; ki < p.K; ki++) {
        float3 raydir = _wp_face_ray(p.axes, p.post_intrins, bid * p.K + ki, i, j);
        float2 uv;
        bool valid = _wp_to_pixel<D>(p.camera_model, raydir, intrin,
                                     p.dist_coeffs, p.source_models,
                                     p.source_params, bid, uv);
        float outv = 0.0f;
        float d;
        if (valid && _wp_bilinear_depth(p.src, p.elem_kind, plane, p.Hin,
                                        p.Win, uv.x * sx, uv.y * sy,
                                        p.norm_inv, false, d))
            outv = _wp_face_ray_depth(d, raydir, p.ray_depth != 0);
        p.dst[((gindex_t(bid) * p.K + ki) * p.Hout + j) * p.Wout + i] = outv;
    }
}

#define _wp_depth_call(D) _wp_depth_wide<D>(p, i, j, bid)

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_depth_wide(uniform WarpParams p, uint3 wg: SV_GroupID,
                     uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    SS_DISPATCH_DIST(_wp_depth_call)
}
#undef _wp_depth_call

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_depth_equi(uniform WarpParams p, uint3 wg: SV_GroupID,
                     uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    for (int ki = 0; ki < p.K; ki++) {
        float3 raydir = _wp_face_ray(p.axes, p.post_intrins, bid * p.K + ki, i, j);
        float2 uv;
        float d;
        bool ok = _wp_equi_uv(raydir, p.Hin, p.Win, uv) &&
                  _wp_bilinear_depth(p.src, p.elem_kind, plane, p.Hin, p.Win,
                                     uv.x, uv.y, p.norm_inv, true, d);
        p.dst[((gindex_t(bid) * p.K + ki) * p.Hout + j) * p.Wout + i] =
            ok ? _wp_face_ray_depth(d, raydir, p.ray_depth != 0) : 0.0f;
    }
}

[ForceInline]
void _wp_normal_wide<D : ICameraDistortion>(WarpParams p, int i, int j,
                                            int bid) {
    float4 intrin = _wp_intrin(p.intrins, bid);
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    float sx = float(p.Win) / float(p.ref_W), sy = float(p.Hin) / float(p.ref_H);
    for (int ki = 0; ki < p.K; ki++) {
        int pc = bid * p.K + ki;
        float3 raydir = _wp_face_ray(p.axes, p.post_intrins, pc, i, j);
        float2 uv;
        bool valid = _wp_to_pixel<D>(p.camera_model, raydir, intrin,
                                     p.dist_coeffs, p.source_models,
                                     p.source_params, bid, uv);
        float3 nf = float3(-1.0f);
        if (valid)
            nf = _wp_one_normal(p.src, p.elem_kind, plane, p.Hin, p.Win,
                                uv.x * sx, uv.y * sy, p.norm_inv,
                                p.decode_off, false, p.axes, pc);
        gindex_t base = (((gindex_t(bid) * p.K + ki) * p.Hout + j) * p.Wout + i) * 3;
        p.dst[base + 0] = nf.x;
        p.dst[base + 1] = nf.y;
        p.dst[base + 2] = nf.z;
    }
}

#define _wp_normal_call(D) _wp_normal_wide<D>(p, i, j, bid)

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_normal_wide(uniform WarpParams p, uint3 wg: SV_GroupID,
                      uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    SS_DISPATCH_DIST(_wp_normal_call)
}
#undef _wp_normal_call

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_normal_equi(uniform WarpParams p, uint3 wg: SV_GroupID,
                      uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    for (int ki = 0; ki < p.K; ki++) {
        int pc = bid * p.K + ki;
        float3 raydir = _wp_face_ray(p.axes, p.post_intrins, pc, i, j);
        float2 uv;
        float3 nf = float3(-1.0f);
        if (_wp_equi_uv(raydir, p.Hin, p.Win, uv))
            nf = _wp_one_normal(p.src, p.elem_kind, plane, p.Hin, p.Win,
                                uv.x, uv.y, p.norm_inv, p.decode_off,
                                true, p.axes, pc);
        gindex_t base = (((gindex_t(bid) * p.K + ki) * p.Hout + j) * p.Wout + i) * 3;
        p.dst[base + 0] = nf.x;
        p.dst[base + 1] = nf.y;
        p.dst[base + 2] = nf.z;
    }
}

// Nearest-neighbor mask warps (uint8 0/nonzero in, uint8 0/1 out). Threads
// map to whole u32 OUTPUT words in the flat [B, K, Hout, Wout] byte space:
// each thread decodes its 4 byte positions independently and stores one
// word, so there is exactly one writer per word and no atomics.
struct WarpMaskParams {    // 104 bytes: pushed directly
    float* intrins;        // [B, 4]
    float* dist_coeffs;    // [B, 8]
    int* source_models;    // [B]
    float* source_params;  // [B, 16]
    uint* src;             // [B, Hin, Win] uint8
    uint* dst;             // [B, K, Hout, Wout] uint8 (word writes)
    float* axes;           // [B*K, 3, 3]
    float* post_intrins;   // [B*K, 4]
    int B;
    int Hin;
    int Win;
    int K;
    int Hout;
    int Wout;
    int camera_model;
    int ref_H;  // intrinsics reference resolution (redistort entry)
    int ref_W;
    uint wgs_per_row;
};

[ForceInline]
uint _wp_mask_byte<D : ICameraDistortion>(WarpMaskParams p, uint n,
                                          bool equi) {
    uint i = n % uint(p.Wout);
    uint t = n / uint(p.Wout);
    uint j = t % uint(p.Hout);
    t /= uint(p.Hout);
    uint ki = t % uint(p.K);
    uint bid = t / uint(p.K);
    float3 raydir = _wp_face_ray(p.axes, p.post_intrins, int(bid) * p.K + int(ki),
                                 int(i), int(j));
    int xs, ys;
    if (equi) {
        float2 uv;
        if (!_wp_equi_uv(raydir, p.Hin, p.Win, uv)) return 0u;
        xs = int(floor(uv.x));
        ys = int(floor(uv.y));
        // wrap u for equirectangular (panoramic continuity); clamp v.
        if (xs < 0) xs += p.Win; else if (xs >= p.Win) xs -= p.Win;
        ys = clamp(ys, 0, p.Hin - 1);
    } else {
        float4 intrin = _wp_intrin(p.intrins, int(bid));
        float2 uv;
        if (!_wp_to_pixel<D>(p.camera_model, raydir, intrin, p.dist_coeffs,
                             p.source_models, p.source_params, int(bid), uv))
            return 0u;
        xs = int(floor(clamp(uv.x, -8.0f, float(p.Win) + 8.0f)));
        ys = int(floor(clamp(uv.y, -8.0f, float(p.Hin) + 8.0f)));
        if (xs < 0 || xs >= p.Win || ys < 0 || ys >= p.Hin) return 0u;
    }
    gindex_t idx = gindex_t(bid) * p.Hin * p.Win + gindex_t(ys) * p.Win + xs;
    return _wp_load(p.src, idx, kElemU8) != 0.0f ? 1u : 0u;
}

[ForceInline]
void _wp_mask_main<D : ICameraDistortion>(WarpMaskParams p, uint3 wg,
                                          uint tid, bool equi) {
    uint word = (wg.y * p.wgs_per_row + wg.x) * 256u + tid;
    uint total = uint(p.B * p.K) * uint(p.Hout) * uint(p.Wout);
    if (word >= (total + 3u) / 4u) return;
    uint outw = 0u;
    for (uint b = 0; b < 4u; b++) {
        uint n = word * 4u + b;
        if (n < total)
            outw |= _wp_mask_byte<D>(p, n, equi) << (b * 8u);
    }
    p.dst[word] = outw;
}

#define _wp_mask_call(D) _wp_mask_main<D>(p, wg, tid, false)

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_mask_wide(uniform WarpMaskParams p, uint3 wg: SV_GroupID,
                    uint tid: SV_GroupIndex) {
    SS_DISPATCH_DIST(_wp_mask_call)
}
#undef _wp_mask_call

[shader("compute")]
[numthreads(256, 1, 1)]
void warp_mask_equi(uniform WarpMaskParams p, uint3 wg: SV_GroupID,
                    uint tid: SV_GroupIndex) {
    _wp_mask_main<DistNone>(p, wg, tid, true);
}

// Re-distort: resample a GT image from a camera whose lens model no distortion
// tier represents onto the tier the parser fitted for it (mirrors
// kernels/pixelwise/ImageRedistort.cu). The fit keeps the pose, so a
// destination pixel and its source pixel are the same ray. Both grids are
// expressed relative to the intrinsics reference (ref_H/ref_W, the RGB
// resolution) and need not match: a depth or normal map carries its own size,
// so scale_out (destination step) and scale_in (source lookup) differ.
[ForceInline]
bool _rd_lookup<D : ICameraDistortion>(float* intrins, float* dist_coeffs,
                                       int* source_models,
                                       float* source_params, int camera_model,
                                       int bid, int i, int j, float2 scale_out,
                                       float2 scale_in, out float2 uv_src) {
    uv_src = float2(0.0f);
    float4 intrin = _wp_intrin(intrins, bid);
    float2 uv = float2(((float(i) + 0.5f) / scale_out.x - intrin.z) / intrin.x,
                       ((float(j) + 0.5f) / scale_out.y - intrin.w) / intrin.y);
    float dc[D.kNumParams];
    load_dist_coeffs<D>(dist_coeffs, uint(bid), dc);
    float3 raydir;
    if (!unproject_point<D>(uv, camera_model, dc, raydir))
        return false;
    if (!_wp_source_proj(source_models, source_params, bid, raydir, uv_src))
        return false;
    uv_src *= scale_in;
    return true;
}

[ForceInline]
float2 _rd_scale(int W, int H, int ref_W, int ref_H) {
    return float2(float(W) / float(ref_W), float(H) / float(ref_H));
}

[ForceInline]
void _rd_img<D : ICameraDistortion>(WarpParams p, int i, int j, int bid) {
    float2 uv_src;
    bool valid = _rd_lookup<D>(p.intrins, p.dist_coeffs, p.source_models,
                               p.source_params, p.camera_model, bid, i, j,
                               _rd_scale(p.Wout, p.Hout, p.ref_W, p.ref_H),
                               _rd_scale(p.Win, p.Hin, p.ref_W, p.ref_H),
                               uv_src);
    gindex_t plane = gindex_t(bid) * p.Hin * p.Win;
    gindex_t base = ((gindex_t(bid) * p.Hout + j) * p.Wout + i) * p.C;
    for (int c = 0; c < p.C; c++) {
        float v = p.invalid;
        if (valid)
            v = _wp_bilinear(p.src, p.elem_kind, plane, p.Hin, p.Win, p.C, c,
                             uv_src.x, uv_src.y, p.norm_inv, p.invalid, false);
        p.dst[base + c] = v;
    }
}

#define _rd_img_call(D) _rd_img<D>(p, i, j, bid)

[shader("compute")]
[numthreads(256, 1, 1)]
void redistort_img(uniform WarpParams p, uint3 wg: SV_GroupID,
                   uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    SS_DISPATCH_DIST(_rd_img_call)
}
#undef _rd_img_call

// Depth keeps its 0 = "no ground truth here" sentinel out of the blend, which
// redistort_img cannot do -- there the padding value IS a colour.
[ForceInline]
void _rd_depth<D : ICameraDistortion>(WarpParams p, int i, int j, int bid) {
    float2 uv_src;
    float d = 0.0f;
    if (_rd_lookup<D>(p.intrins, p.dist_coeffs, p.source_models,
                      p.source_params, p.camera_model, bid, i, j,
                      _rd_scale(p.Wout, p.Hout, p.ref_W, p.ref_H),
                      _rd_scale(p.Win, p.Hin, p.ref_W, p.ref_H), uv_src))
        _wp_bilinear_depth(p.src, p.elem_kind, gindex_t(bid) * p.Hin * p.Win,
                           p.Hin, p.Win, uv_src.x, uv_src.y, p.norm_inv,
                           false, d);
    p.dst[(gindex_t(bid) * p.Hout + j) * p.Wout + i] = d;
}

#define _rd_depth_call(D) _rd_depth<D>(p, i, j, bid)

[shader("compute")]
[numthreads(256, 1, 1)]
void redistort_depth(uniform WarpParams p, uint3 wg: SV_GroupID,
                     uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    SS_DISPATCH_DIST(_rd_depth_call)
}
#undef _rd_depth_call

[ForceInline]
void _rd_normal<D : ICameraDistortion>(WarpParams p, int i, int j, int bid) {
    float2 uv_src;
    float3 n = float3(-1.0f);
    if (_rd_lookup<D>(p.intrins, p.dist_coeffs, p.source_models,
                      p.source_params, p.camera_model, bid, i, j,
                      _rd_scale(p.Wout, p.Hout, p.ref_W, p.ref_H),
                      _rd_scale(p.Win, p.Hin, p.ref_W, p.ref_H), uv_src)) {
        float3 s;
        if (_wp_bilinear_normal(p.src, p.elem_kind,
                                gindex_t(bid) * p.Hin * p.Win, p.Hin, p.Win,
                                uv_src.x, uv_src.y, p.norm_inv, p.decode_off,
                                false, s)) {
            float sl = length(s);
            // The pose is unchanged, so a camera-frame normal needs no rotation.
            if (sl > 1e-8f) n = s / sl;
        }
    }
    gindex_t base = ((gindex_t(bid) * p.Hout + j) * p.Wout + i) * 3;
    p.dst[base + 0] = n.x;
    p.dst[base + 1] = n.y;
    p.dst[base + 2] = n.z;
}

#define _rd_normal_call(D) _rd_normal<D>(p, i, j, bid)

[shader("compute")]
[numthreads(256, 1, 1)]
void redistort_normal(uniform WarpParams p, uint3 wg: SV_GroupID,
                      uint tid: SV_GroupIndex) {
    int i, j, bid;
    if (!_wp_tile(p, wg, tid, i, j, bid)) return;
    SS_DISPATCH_DIST(_rd_normal_call)
}
#undef _rd_normal_call

// Out-of-frame is masked OUT rather than padded: the fitted camera can see a
// little past the source image. Word-per-thread output, as warp_mask_wide.
[ForceInline]
uint _rd_mask_byte<D : ICameraDistortion>(WarpMaskParams p, uint n) {
    uint i = n % uint(p.Wout);
    uint t = n / uint(p.Wout);
    uint j = t % uint(p.Hout);
    uint bid = t / uint(p.Hout);
    float2 uv;
    if (!_rd_lookup<D>(p.intrins, p.dist_coeffs, p.source_models,
                       p.source_params, p.camera_model, int(bid), int(i),
                       int(j), _rd_scale(p.Wout, p.Hout, p.ref_W, p.ref_H),
                       _rd_scale(p.Win, p.Hin, p.ref_W, p.ref_H), uv))
        return 0u;
    int xs = int(floor(clamp(uv.x, -8.0f, float(p.Win) + 8.0f)));
    int ys = int(floor(clamp(uv.y, -8.0f, float(p.Hin) + 8.0f)));
    if (xs < 0 || xs >= p.Win || ys < 0 || ys >= p.Hin) return 0u;
    gindex_t idx = gindex_t(bid) * p.Hin * p.Win + gindex_t(ys) * p.Win + xs;
    return _wp_load(p.src, idx, kElemU8) != 0.0f ? 1u : 0u;
}

[ForceInline]
void _rd_mask_main<D : ICameraDistortion>(WarpMaskParams p, uint3 wg,
                                          uint tid) {
    uint word = (wg.y * p.wgs_per_row + wg.x) * 256u + tid;
    uint total = uint(p.B) * uint(p.Hout) * uint(p.Wout);
    if (word >= (total + 3u) / 4u) return;
    uint outw = 0u;
    for (uint b = 0; b < 4u; b++) {
        uint n = word * 4u + b;
        if (n < total)
            outw |= _rd_mask_byte<D>(p, n) << (b * 8u);
    }
    p.dst[word] = outw;
}

#define _rd_mask_call(D) _rd_mask_main<D>(p, wg, tid)

[shader("compute")]
[numthreads(256, 1, 1)]
void redistort_mask(uniform WarpMaskParams p, uint3 wg: SV_GroupID,
                    uint tid: SV_GroupIndex) {
    SS_DISPATCH_DIST(_rd_mask_call)
}
#undef _rd_mask_call

// Flat byte/u16 -> float conversion: out[i] = elem(i) * scale + offset.
// Covers uint8/16_image_to_float (1/255, 1/65535), uint8_normal_to_float
// (1/127.5, -1) and uint16_depth_to_float (1, 0).
struct BytesToFloatParams {  // 40 bytes: pushed directly
    uint* src;
    float* dst;
    uint total;
    int elem_kind;
    float scale;
    float offset;
    uint wgs_per_row;
    int _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bytes_to_float(uniform BytesToFloatParams p, uint3 wg: SV_GroupID,
                    uint tid: SV_GroupIndex) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256u + tid;
    if (idx >= p.total) return;
    p.dst[idx] = _wp_load(p.src, gindex_t(idx), p.elem_kind) * p.scale + p.offset;
}

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
// kFromSource trails kDistortion so the launches that never re-distort keep
// passing a one-element SpecList.
[SpecializationConstant]
const int kDistortion = 0;
[SpecializationConstant]
const int kFromSource = 0;
