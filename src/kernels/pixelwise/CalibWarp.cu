// CalibWarp.cu -- the per-image ground-truth warp that absorbs a camera's
// calibration residual: resample, Gauss-Newton accumulation, per-image update.
// Vulkan twin: backend/vulkan/shaders/calib_warp.slang.

#include "kernels/pixelwise/CalibWarp.cuh"

#include "core/Common.cuh"
#include "core/Interpolation.cuh"
#include "core/PixelFormat.h"

namespace {

constexpr int kCwBlock = 256;

struct CwCell {
    int i0, j0;
    float fu, fv;
};

// Control points sit at the image corners, so pixel x's cell coordinate is
// (x + 0.5) (n - 1) / W.
__device__ __forceinline__ CwCell cw_cell(int x, int y, int W, int H, int n) {
    const float u = ((float)x + 0.5f) * (float)(n - 1) / (float)W;
    const float v = ((float)y + 0.5f) * (float)(n - 1) / (float)H;
    const int i0 = min(max((int)floorf(u), 0), n - 2);
    const int j0 = min(max((int)floorf(v), 0), n - 2);
    return {i0, j0, u - (float)i0, v - (float)j0};
}

__device__ __forceinline__ void cw_weights(const CwCell& c, float w[4]) {
    w[0] = (1.0f - c.fu) * (1.0f - c.fv);
    w[1] = c.fu * (1.0f - c.fv);
    w[2] = (1.0f - c.fu) * c.fv;
    w[3] = c.fu * c.fv;
}

__device__ __forceinline__ int cw_corner(const CwCell& c, int k, int n) {
    return (c.j0 + (k >> 1)) * n + c.i0 + (k & 1);
}

// 1/depth for the parallax term. The render holds sum(w d), so the expected
// depth divides by 1 - T; an empty pixel counts as infinitely far.
__device__ __forceinline__ float cw_inv_depth(const float* depth, const float* Ts, size_t pix) {
    if (depth == nullptr) return 0.0f;
    const float d = depth[pix];
    const float a = Ts != nullptr ? 1.0f - Ts[pix] : 1.0f;
    return d > 1e-4f && a > 0.0f ? fminf(a / d, 1e4f) : 0.0f;
}

// Displacement in pixels at a pixel of camera row `row`.
__device__ __forceinline__ float2 cw_flow(const float* row, const CwCell& c, int n,
                                          int D, float e, float sx, float sy) {
    float w[4];
    cw_weights(c, w);
    float dx = 0.0f, dy = 0.0f;
    for (int k = 0; k < 4; ++k) {
        const float* p = row + (size_t)cw_corner(c, k, n) * D;
        dx += w[k] * p[0];
        dy += w[k] * p[1];
        if (D == 4) {
            dx += w[k] * e * p[2];
            dy += w[k] * e * p[3];
        }
    }
    return make_float2(dx * sx, dy * sy);
}

__device__ __forceinline__ float3 cw_tap(const void* img, PixelFormat f, size_t base,
                                         int W, int H, int x, int y) {
    x = min(max(x, 0), W - 1);
    y = min(max(y, 0), H - 1);
    return pixel_load3(img, f, base + (size_t)y * W + x);
}

__device__ __forceinline__ float3 cw_sample(const void* img, PixelFormat f, size_t base,
                                            int W, int H, float x, float y) {
    const float xf = floorf(x), yf = floorf(y);
    const int x0 = (int)xf, y0 = (int)yf;
    const float ax = x - xf, ay = y - yf;
    const float3 a = cw_tap(img, f, base, W, H, x0, y0);
    const float3 b = cw_tap(img, f, base, W, H, x0 + 1, y0);
    const float3 c = cw_tap(img, f, base, W, H, x0, y0 + 1);
    const float3 d = cw_tap(img, f, base, W, H, x0 + 1, y0 + 1);
    const float w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay);
    const float w01 = (1 - ax) * ay, w11 = ax * ay;
    return make_float3(w00 * a.x + w10 * b.x + w01 * c.x + w11 * d.x,
                       w00 * a.y + w10 * b.y + w01 * c.y + w11 * d.y,
                       w00 * a.z + w10 * b.z + w01 * c.z + w11 * d.z);
}

__device__ __forceinline__ void cw_catmull_rom(float t, float w[4]) {
    const float t2 = t * t, t3 = t2 * t;
    w[0] = 0.5f * (-t3 + 2.0f * t2 - t);
    w[1] = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
    w[2] = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
    w[3] = 0.5f * (t3 - t2);
}

// Catmull-Rom, clamped to the four nearest taps: bilinear at a half-pixel
// offset halves the response at half Nyquist, and the loss would fit that blur.
__device__ __forceinline__ float3 cw_sample_cubic(const void* img, PixelFormat f, size_t base,
                                                  int W, int H, float x, float y) {
    const float xf = floorf(x), yf = floorf(y);
    const int x0 = (int)xf, y0 = (int)yf;
    float wx[4], wy[4];
    cw_catmull_rom(x - xf, wx);
    cw_catmull_rom(y - yf, wy);
    float3 acc = make_float3(0.0f, 0.0f, 0.0f);
    float3 lo = make_float3(1e30f, 1e30f, 1e30f), hi = make_float3(-1e30f, -1e30f, -1e30f);
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) {
            const float3 t = cw_tap(img, f, base, W, H, x0 - 1 + i, y0 - 1 + j);
            const float w = wx[i] * wy[j];
            acc.x += w * t.x;
            acc.y += w * t.y;
            acc.z += w * t.z;
            if ((i == 1 || i == 2) && (j == 1 || j == 2)) {
                lo = make_float3(fminf(lo.x, t.x), fminf(lo.y, t.y), fminf(lo.z, t.z));
                hi = make_float3(fmaxf(hi.x, t.x), fmaxf(hi.y, t.y), fmaxf(hi.z, t.z));
            }
        }
    return make_float3(fminf(fmaxf(acc.x, lo.x), hi.x), fminf(fmaxf(acc.y, lo.y), hi.y),
                       fminf(fmaxf(acc.z, lo.z), hi.z));
}

__device__ __forceinline__ float cw_dot(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__device__ __forceinline__ float3 cw_sub(float3 a, float3 b) {
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__device__ __forceinline__ float3 cw_scale(float3 a, float s) {
    return make_float3(a.x * s, a.y * s, a.z * s);
}

__global__ void calib_warp_gt_fwd_kernel(
    const void* __restrict__ gt, PixelFormat gt_fmt,
    const void* __restrict__ render, PixelFormat render_fmt,
    const float* __restrict__ depth, const float* __restrict__ Ts,
    const float* __restrict__ params, int P,
    const float4* __restrict__ intrins,
    const int32_t* __restrict__ cam_idx,
    int H, int W, int n, int D,
    PixelOut out
) {
    const int pix = blockIdx.x * blockDim.x + threadIdx.x;
    const int b = blockIdx.y;
    if (pix >= H * W) return;
    const int x = pix % W, y = pix / W;
    const int cam = cam_idx ? cam_idx[b] : b;
    const float4 intr = intrins[b];
    const size_t base = (size_t)b * H * W;
    const CwCell c = cw_cell(x, y, W, H, n);
    const float e = D == 4 ? cw_inv_depth(depth, Ts, base + pix) : 0.0f;
    const float2 fl = cw_flow(params + (size_t)cam * P, c, n, D, e,
                              1e-3f * intr.x, 1e-3f * intr.y);
    const float sx = (float)x + fl.x, sy = (float)y + fl.y;
    float3 v;
    if (sx >= 0.0f && sx <= (float)(W - 1) && sy >= 0.0f && sy <= (float)(H - 1))
        v = cw_sample_cubic(gt, gt_fmt, base, W, H, sx, sy);
    else
        v = pixel_load3(render, render_fmt, base + pix);
    out[3 * (base + pix) + 0] = v.x;
    out[3 * (base + pix) + 1] = v.y;
    out[3 * (base + pix) + 2] = v.z;
}

template <int D>
__global__ void calib_warp_accum_kernel(
    const void* __restrict__ gt, PixelFormat gt_fmt,
    const void* __restrict__ render, PixelFormat render_fmt,
    const float* __restrict__ v_rgb, float v_scale,
    const float* __restrict__ depth, const float* __restrict__ Ts,
    const float* __restrict__ params,
    const float4* __restrict__ intrins,
    const int32_t* __restrict__ cam_idx,
    const uint8_t* __restrict__ mask, int Hm, int Wm,
    int H, int W, int n, float eps, float f_scale, bool warped,
    int sub_blocks, int chunk,
    float* __restrict__ grad, float* __restrict__ hess
) {
    constexpr int DH = D == 4 ? 9 : 3;
    constexpr int Q = D + DH;
    const int b = blockIdx.y;
    const int cell = blockIdx.x / sub_blocks, s = blockIdx.x % sub_blocks;
    const int ci = cell % (n - 1), cj = cell / (n - 1);
    const int bx0 = max(0, (int)floorf((float)ci * W / (float)(n - 1)) - 1);
    const int bx1 = min(W, (int)ceilf((float)(ci + 1) * W / (float)(n - 1)) + 1);
    const int by0 = max(0, (int)floorf((float)cj * H / (float)(n - 1)) - 1);
    const int by1 = min(H, (int)ceilf((float)(cj + 1) * H / (float)(n - 1)) + 1);
    const int bw = bx1 - bx0;
    const int npx = bw * (by1 - by0);
    const int start = s * chunk, end = min(npx, start + chunk);

    const int cam = cam_idx ? cam_idx[b] : b;
    const float4 intr = intrins[b];
    const float scx = 1e-3f * intr.x * f_scale, scy = 1e-3f * intr.y * f_scale;
    const float* row = params + (size_t)cam * n * n * D;
    const size_t base = (size_t)b * H * W;

    float acc[4 * Q];
#pragma unroll
    for (int i = 0; i < 4 * Q; ++i) acc[i] = 0.0f;

    for (int idx = start + (int)threadIdx.x; idx < end; idx += blockDim.x) {
        const int x = bx0 + idx % bw, y = by0 + idx / bw;
        const CwCell c = cw_cell(x, y, W, H, n);
        if (c.i0 != ci || c.j0 != cj) continue;
        if (mask != nullptr) {
            const int my = min((int)(((float)y + 0.5f) * Hm / H), Hm - 1);
            const int mx = min((int)(((float)x + 0.5f) * Wm / W), Wm - 1);
            if (mask[((size_t)b * Hm + my) * Wm + mx] == 0) continue;
        }
        const size_t pix = base + (size_t)y * W + x;
        const float e = D == 4 ? cw_inv_depth(depth, Ts, pix) : 0.0f;
        const float2 fl = cw_flow(row, c, n, D, e, scx, scy);
        const float sx = (float)x + fl.x, sy = (float)y + fl.y;
        // The central difference reads one pixel either side.
        if (!(sx >= 1.0f && sx <= (float)(W - 2) && sy >= 1.0f && sy <= (float)(H - 2)))
            continue;
        float3 I, gx, gy;
        if (warped) {
            // Already resampled at p + flow, so its own neighbours stand in for
            // the source's there: the flow's Jacobian is ~1 over a pixel.
            if (x < 1 || x > W - 2 || y < 1 || y > H - 2) continue;
            I = cw_tap(gt, gt_fmt, base, W, H, x, y);
            gx = cw_scale(cw_sub(cw_tap(gt, gt_fmt, base, W, H, x + 1, y),
                                 cw_tap(gt, gt_fmt, base, W, H, x - 1, y)), 0.5f * scx);
            gy = cw_scale(cw_sub(cw_tap(gt, gt_fmt, base, W, H, x, y + 1),
                                 cw_tap(gt, gt_fmt, base, W, H, x, y - 1)), 0.5f * scy);
        } else {
            I = cw_sample(gt, gt_fmt, base, W, H, sx, sy);
            gx = cw_scale(cw_sub(cw_sample(gt, gt_fmt, base, W, H, sx + 1.0f, sy),
                                 cw_sample(gt, gt_fmt, base, W, H, sx - 1.0f, sy)), 0.5f * scx);
            gy = cw_scale(cw_sub(cw_sample(gt, gt_fmt, base, W, H, sx, sy + 1.0f),
                                 cw_sample(gt, gt_fmt, base, W, H, sx, sy - 1.0f)), 0.5f * scy);
        }
        const float3 r = cw_sub(I, pixel_load3(render, render_fmt, pix));
        const float rho = eps * rsqrtf(cw_dot(r, r) * (1.0f / 3.0f) + eps * eps);
        float3 rg;
        if (v_rgb != nullptr) {
            const float* v = v_rgb + 3 * pix;
            rg = make_float3(-v_scale * v[0], -v_scale * v[1], -v_scale * v[2]);
        } else {
            rg = cw_scale(r, rho);
        }
        const float g0 = cw_dot(gx, rg), g1 = cw_dot(gy, rg);
        const float hA = rho * cw_dot(gx, gx), hB = rho * cw_dot(gx, gy),
                    hC = rho * cw_dot(gy, gy);
        float w[4];
        cw_weights(c, w);
#pragma unroll
        for (int k = 0; k < 4; ++k) {
            float* a = acc + k * Q;
            a[0] += w[k] * g0;
            a[1] += w[k] * g1;
            if constexpr (D == 4) {
                a[2] += w[k] * e * g0;
                a[3] += w[k] * e * g1;
            }
            a[D + 0] += w[k] * hA;
            a[D + 1] += w[k] * hB;
            a[D + 2] += w[k] * hC;
            if constexpr (D == 4) {
                const float we = w[k] * e, wee = w[k] * e * e;
                a[D + 3] += we * hA;
                a[D + 4] += we * hB;
                a[D + 5] += we * hC;
                a[D + 6] += wee * hA;
                a[D + 7] += wee * hB;
                a[D + 8] += wee * hC;
            }
        }
    }

    __shared__ float red[kCwBlock / 32][4 * Q];
    const int lane = threadIdx.x & 31, warp = threadIdx.x >> 5;
#pragma unroll
    for (int i = 0; i < 4 * Q; ++i) {
        float v = acc[i];
        for (int o = 16; o > 0; o >>= 1) v += __shfl_down_sync(0xffffffffu, v, o);
        if (lane == 0) red[warp][i] = v;
    }
    __syncthreads();
    if ((int)threadIdx.x < 4 * Q) {
        float t = 0.0f;
        for (int w = 0; w < kCwBlock / 32; ++w) t += red[w][threadIdx.x];
        if (t != 0.0f && isfinite(t)) {
            const int k = threadIdx.x / Q, q = threadIdx.x % Q;
            const int kk = (cj + (k >> 1)) * n + ci + (k & 1);
            if (q < D)
                atomicAdd(&grad[((size_t)b * n * n + kk) * D + q], t);
            else
                atomicAdd(&hess[((size_t)b * n * n + kk) * DH + (q - D)], t);
        }
    }
}

// Solves M x = g in place for a symmetric positive definite 4x4 M.
__device__ __forceinline__ bool cw_cholesky_solve4(float M[4][4], float g[4]) {
    float L[4][4] = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j <= i; ++j) {
            float s = M[i][j];
            for (int k = 0; k < j; ++k) s -= L[i][k] * L[j][k];
            if (i == j) {
                if (!(s > 0.0f)) return false;
                L[i][i] = sqrtf(s);
            } else {
                L[i][j] = s / L[j][j];
            }
        }
    }
    float z[4];
    for (int i = 0; i < 4; ++i) {
        float s = g[i];
        for (int k = 0; k < i; ++k) s -= L[i][k] * z[k];
        z[i] = s / L[i][i];
    }
    for (int i = 3; i >= 0; --i) {
        float s = z[i];
        for (int k = i + 1; k < 4; ++k) s -= L[k][i] * g[k];
        g[i] = s / L[i][i];
    }
    return true;
}

__device__ float cw_block_sum(float v, float* scratch) {
    const int lane = threadIdx.x & 31, warp = threadIdx.x >> 5;
    for (int o = 16; o > 0; o >>= 1) v += __shfl_down_sync(0xffffffffu, v, o);
    __syncthreads();
    if (lane == 0) scratch[warp] = v;
    __syncthreads();
    float t = 0.0f;
    for (int w = 0; w < kCwBlock / 32; ++w) t += scratch[w];
    return t;
}

// One block per image of the batch, one thread per control point. Jacobi
// over the control points: the smoothness term reads its neighbours' old
// values, and its diagonal is 2 x degree so the step majorizes the Laplacian.
template <int D>
__global__ void calib_warp_update_kernel(
    float* __restrict__ params, float* __restrict__ grad, float* __restrict__ hess,
    const int32_t* __restrict__ cam_idx, const float4* __restrict__ intrins, float f_scale,
    int n, float step, float damping, float smooth, float prior, float max_step
) {
    constexpr int DH = D == 4 ? 9 : 3;
    __shared__ float scratch[kCwBlock / 32];
    const int b = blockIdx.x;
    // A step is capped in pixels of this level: what the linearization holds.
    if (intrins != nullptr)
        max_step /= 1e-3f * fmaxf(intrins[b].x, intrins[b].y) * f_scale;
    const int cam = cam_idx ? cam_idx[b] : b;
    const int k = threadIdx.x;
    const int nn = n * n;
    const bool active = k < nn;
    float* P = params + (size_t)cam * nn * D;
    float* G = grad + (size_t)b * nn * D;
    float* Hs = hess + (size_t)b * nn * DH;

    float g[D], h[DH], th[D];
    for (int i = 0; i < D; ++i) g[i] = active ? G[k * D + i] : 0.0f;
    for (int i = 0; i < DH; ++i) h[i] = active ? Hs[k * DH + i] : 0.0f;
    for (int i = 0; i < D; ++i) th[i] = active ? P[k * D + i] : 0.0f;
    // The priors are relative to this image's mean curvature, so they mean the
    // same at any resolution and contrast.
    const float ta = cw_block_sum(active ? h[0] + h[2] : 0.0f, scratch) / (2.0f * nn);
    const float tb = D == 4 ? cw_block_sum(active ? h[6] + h[8] : 0.0f, scratch) / (2.0f * nn)
                            : 0.0f;
    float delta[D] = {};
    bool ok = active && ta > 0.0f;
    if (ok) {
        const int x = k % n, y = k / n;
        const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
        float lap[D] = {};
        int deg = 0;
        for (int q = 0; q < 4; ++q) {
            const int nx = nb[q][0], ny = nb[q][1];
            if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
            ++deg;
            for (int i = 0; i < D; ++i) lap[i] += th[i] - P[(ny * n + nx) * D + i];
        }
        float rhs[4] = {}, diag[4] = {};
        for (int i = 0; i < D; ++i) {
            const float tau = i < 2 ? ta : (tb > 0.0f ? tb : ta);
            rhs[i] = g[i] + smooth * tau * lap[i] + prior * tau * th[i];
            diag[i] = tau * (2.0f * smooth * deg + prior + damping);
        }
        if constexpr (D == 2) {
            const float a = h[0] + diag[0], bb = h[1], c = h[2] + diag[1];
            const float det = a * c - bb * bb;
            ok = det > 0.0f;
            if (ok) {
                delta[0] = (c * rhs[0] - bb * rhs[1]) / det;
                delta[1] = (a * rhs[1] - bb * rhs[0]) / det;
            }
        } else {
            float M[4][4] = {
                {h[0] + diag[0], h[1], h[3], h[4]},
                {h[1], h[2] + diag[1], h[4], h[5]},
                {h[3], h[4], h[6] + diag[2], h[7]},
                {h[4], h[5], h[7], h[8] + diag[3]},
            };
            ok = cw_cholesky_solve4(M, rhs);
            for (int i = 0; i < 4; ++i) delta[i] = rhs[i];
        }
        // One factor for every part: the displacement and parallax steps of an
        // ill-conditioned point nearly cancel, and clipping them apart does not.
        float shrink = 1.0f;
        for (int part = 0; ok && part < D / 2; ++part) {
            const float m = sqrtf(delta[2 * part] * delta[2 * part] +
                                  delta[2 * part + 1] * delta[2 * part + 1]);
            if (!isfinite(m)) ok = false;
            else if (m > max_step) shrink = fminf(shrink, max_step / m);
        }
        for (int i = 0; i < D; ++i) delta[i] *= shrink;
    }
    __syncthreads();  // every neighbour read above happens before any write
    if (active) {
        if (ok)
            for (int i = 0; i < D; ++i) P[k * D + i] = th[i] - step * delta[i];
        for (int i = 0; i < D; ++i) G[k * D + i] = 0.0f;
        for (int i = 0; i < DH; ++i) Hs[k * DH + i] = 0.0f;
    }
}

// Every output pixel averages its 2x2 source block; an odd last row or column
// repeats its edge.
__global__ void calib_warp_downsample2_kernel(const void* __restrict__ in, PixelFormat fmt,
                                              int B, int H, int W, int H2, int W2,
                                              float* __restrict__ out) {
    const int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= (int64_t)B * H2 * W2) return;
    const int x = (int)(i % W2), y = (int)((i / W2) % H2), b = (int)(i / ((int64_t)W2 * H2));
    const size_t base = (size_t)b * H * W;
    float3 acc = make_float3(0.0f, 0.0f, 0.0f);
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            const float3 t = cw_tap(in, fmt, base, W, H, 2 * x + dx, 2 * y + dy);
            acc.x += 0.25f * t.x;
            acc.y += 0.25f * t.y;
            acc.z += 0.25f * t.z;
        }
    out[3 * i + 0] = acc.x;
    out[3 * i + 1] = acc.y;
    out[3 * i + 2] = acc.z;
}

// One aux map at its own size (Ha, Wa), through the flow of the render pixel
// it covers. Kind 0: uint8 mask, nearest; 1 depth, 2 normal: bilinear over
// the valid taps (core/Interpolation.cuh), so a hole does not bleed.
template <int Kind>
__global__ void calib_warp_aux_fwd_kernel(
    const void* __restrict__ src_v, const float* __restrict__ depth,
    const float* __restrict__ Ts, const float* __restrict__ params, int P,
    const float4* __restrict__ intrins, const int32_t* __restrict__ cam_idx,
    int H, int W, int Ha, int Wa, int n, int D, bool cut_by_source, void* __restrict__ out_v
) {
    const int pix = blockIdx.x * blockDim.x + threadIdx.x;
    const int b = blockIdx.y;
    if (pix >= Ha * Wa) return;
    const int x = pix % Wa, y = pix / Wa;
    const int cam = cam_idx ? cam_idx[b] : b;
    const float4 intr = intrins[b];
    const CwCell c = cw_cell(x, y, Wa, Ha, n);
    float e = 0.0f;
    if (D == 4) {
        const int xr = min((int)(((float)x + 0.5f) * W / Wa), W - 1);
        const int yr = min((int)(((float)y + 0.5f) * H / Ha), H - 1);
        e = cw_inv_depth(depth, Ts, ((size_t)b * H + yr) * W + xr);
    }
    const float2 fl = cw_flow(params + (size_t)cam * P, c, n, D, e,
                              1e-3f * intr.x * Wa / W, 1e-3f * intr.y * Ha / H);
    const float sx = (float)x + fl.x, sy = (float)y + fl.y;
    const size_t plane = (size_t)b * Ha * Wa;
    if constexpr (Kind == 0) {
        const uint8_t* src = (const uint8_t*)src_v;
        // Clamped: alpha supervision reads a 0 as "should be empty", and a 0
        // off the edge carves the scene along the frame border.
        const int ix = min(max((int)floorf(sx + 0.5f), 0), Wa - 1);
        const int iy = min(max((int)floorf(sy + 0.5f), 0), Ha - 1);
        ((uint8_t*)out_v)[plane + pix] =
            cut_by_source && !src[plane + pix] ? 0 : src[plane + (size_t)iy * Wa + ix];
    } else {
        constexpr int Ch = Kind == 1 ? 1 : 3;
        const float* src = (const float*)src_v;
        const int x0 = (int)floorf(sx), y0 = (int)floorf(sy);
        const float fx = sx - (float)x0, fy = sy - (float)y0;
        float acc[Ch] = {};
        float wsum = 0.0f;
        for (int q = 0; q < 4; ++q) {
            const int tx = x0 + (q & 1), ty = y0 + (q >> 1);
            if (tx < 0 || tx >= Wa || ty < 0 || ty >= Ha) continue;
            const float* v = src + (plane + (size_t)ty * Wa + tx) * Ch;
            bool ok;
            if constexpr (Kind == 1) ok = _bilinear_detail::gt_depth_valid(v[0]);
            else ok = _bilinear_detail::gt_normal_valid(make_float3(v[0], v[1], v[2]));
            if (!ok) continue;
            const float w = ((q & 1) ? fx : 1.0f - fx) * ((q >> 1) ? fy : 1.0f - fy);
            for (int k = 0; k < Ch; ++k) acc[k] += w * v[k];
            wsum += w;
        }
        float* out = (float*)out_v + (plane + pix) * Ch;
        for (int k = 0; k < Ch; ++k)
            out[k] = wsum > 0.0f ? acc[k] / wsum : (Kind == 1 ? 0.0f : -1.0f);
    }
}

const void* _cw_ptr(const TorchTensorView& t) { return (const void*)std::get<0>(t); }

void _cw_check(int n, int D) {
    if (n < 2 || n * n > kCwBlock)
        throw std::runtime_error("calib_warp: grid size must be in [2, 16]");
    if (D != 2 && D != 4)
        throw std::runtime_error("calib_warp: D must be 2 or 4");
}

}  // namespace


/*[AutoHeaderGeneratorExport]*/
void calib_warp_gt_forward(
    TorchTensorView gt_rgb, TorchTensorView render_rgb, TorchTensorView render_depth,
    TorchTensorView render_Ts, TorchTensorView params, TorchTensorView intrins,
    TorchTensorView cam_indices, int grid_n, int D, TorchTensorView out_rgb
) {
    _cw_check(grid_n, D);
    const auto& s = std::get<2>(render_rgb);
    const int B = (int)s[0], H = (int)s[1], W = (int)s[2];
    if (B <= 0 || H <= 0 || W <= 0) return;
    const int P = (int)std::get<2>(params)[1];
    dim3 grid(_CEIL_DIV(H * W, kCwBlock), B);
    calib_warp_gt_fwd_kernel<<<grid, kCwBlock>>>(
        _cw_ptr(gt_rgb), pixel_format(gt_rgb), _cw_ptr(render_rgb), pixel_format(render_rgb),
        (const float*)_cw_ptr(render_depth), (const float*)_cw_ptr(render_Ts),
        (const float*)_cw_ptr(params), P,
        (const float4*)_cw_ptr(intrins), (const int32_t*)_cw_ptr(cam_indices),
        H, W, grid_n, D, PixelOut((void*)_cw_ptr(out_rgb), pixel_format(out_rgb)));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void calib_warp_aux_forward(
    TorchTensorView src, TorchTensorView render_depth, TorchTensorView render_Ts,
    TorchTensorView params, TorchTensorView intrins, TorchTensorView cam_indices,
    int grid_n, int D, int render_H, int render_W, bool cut_by_source, TorchTensorView out
) {
    _cw_check(grid_n, D);
    const auto& s = std::get<2>(src);
    const int B = (int)s[0], Ha = (int)s[1], Wa = (int)s[2], Ch = (int)s[3];
    if (B <= 0 || Ha <= 0 || Wa <= 0 || render_H <= 0 || render_W <= 0) return;
    const int P = (int)std::get<2>(params)[1];
    const uint32_t elem = std::get<1>(src);
    if (!((elem == 1 && Ch == 1) || (elem == 4 && (Ch == 1 || Ch == 3))))
        throw std::runtime_error("calib_warp_aux_forward: a uint8 mask, depth or normal map");
    dim3 grid(_CEIL_DIV(Ha * Wa, kCwBlock), B);
    auto launch = [&](auto kernel) {
        kernel<<<grid, kCwBlock>>>(
            _cw_ptr(src), (const float*)_cw_ptr(render_depth), (const float*)_cw_ptr(render_Ts),
            (const float*)_cw_ptr(params), P, (const float4*)_cw_ptr(intrins),
            (const int32_t*)_cw_ptr(cam_indices), render_H, render_W, Ha, Wa, grid_n, D,
            cut_by_source, (void*)_cw_ptr(out));
    };
    if (elem == 1) launch(calib_warp_aux_fwd_kernel<0>);
    else if (Ch == 1) launch(calib_warp_aux_fwd_kernel<1>);
    else launch(calib_warp_aux_fwd_kernel<2>);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void calib_warp_accumulate(
    TorchTensorView gt_rgb, TorchTensorView render_rgb, TorchTensorView render_depth,
    TorchTensorView render_Ts, TorchTensorView v_render_rgb, float v_scale,
    TorchTensorView params, TorchTensorView intrins, TorchTensorView cam_indices,
    TorchTensorView mask, int grid_n, int D, float robust_eps, float f_scale, bool gt_is_warped,
    TorchTensorView grad, TorchTensorView hess
) {
    _cw_check(grid_n, D);
    const auto& ms = std::get<2>(mask);
    const bool has_mask = std::get<0>(mask) != 0 && ms.size() >= 3;
    const int Hm = has_mask ? (int)ms[1] : 0, Wm = has_mask ? (int)ms[2] : 0;
    const auto& s = std::get<2>(render_rgb);
    const int B = (int)s[0], H = (int)s[1], W = (int)s[2];
    if (B <= 0 || H <= 0 || W <= 0) return;
    const int n = grid_n;
    const int64_t bw = (W + n - 2) / (n - 1) + 3, bh = (H + n - 2) / (n - 1) + 3;
    const int64_t npx = bw * bh;
    const int sub_blocks = (int)std::max<int64_t>(1, (npx + 2047) / 2048);
    const int chunk = (int)((npx + sub_blocks - 1) / sub_blocks);
    dim3 grid((n - 1) * (n - 1) * sub_blocks, B);
    auto k = D == 4 ? calib_warp_accum_kernel<4> : calib_warp_accum_kernel<2>;
    k<<<grid, kCwBlock>>>(
        _cw_ptr(gt_rgb), pixel_format(gt_rgb), _cw_ptr(render_rgb), pixel_format(render_rgb),
        (const float*)_cw_ptr(v_render_rgb), v_scale, (const float*)_cw_ptr(render_depth),
        (const float*)_cw_ptr(render_Ts),
        (const float*)_cw_ptr(params), (const float4*)_cw_ptr(intrins),
        (const int32_t*)_cw_ptr(cam_indices),
        has_mask ? (const uint8_t*)_cw_ptr(mask) : nullptr, Hm, Wm,
        H, W, n, robust_eps, f_scale, gt_is_warped, sub_blocks, chunk,
        (float*)_cw_ptr(grad), (float*)_cw_ptr(hess));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void calib_warp_update(
    TorchTensorView params, TorchTensorView grad, TorchTensorView hess,
    TorchTensorView cam_indices, TorchTensorView intrins, float f_scale, int batch, int grid_n,
    int D, float step, float damping, float smooth, float prior, float max_step
) {
    _cw_check(grid_n, D);
    if (batch <= 0) return;
    auto k = D == 4 ? calib_warp_update_kernel<4> : calib_warp_update_kernel<2>;
    k<<<batch, kCwBlock>>>(
        (float*)_cw_ptr(params), (float*)_cw_ptr(grad), (float*)_cw_ptr(hess),
        (const int32_t*)_cw_ptr(cam_indices), (const float4*)_cw_ptr(intrins), f_scale, grid_n,
        step, damping, smooth, prior, max_step);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void calib_warp_downsample2(TorchTensorView in_rgb, TorchTensorView out_rgb) {
    const auto& si = std::get<2>(in_rgb);
    const auto& so = std::get<2>(out_rgb);
    const int B = (int)si[0], H = (int)si[1], W = (int)si[2], H2 = (int)so[1], W2 = (int)so[2];
    const int64_t n = (int64_t)B * H2 * W2;
    if (n <= 0) return;
    calib_warp_downsample2_kernel<<<_LAUNCH_ARGS_1D(n, kCwBlock)>>>(
        _cw_ptr(in_rgb), pixel_format(in_rgb), B, H, W, H2, W2, (float*)_cw_ptr(out_rgb));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
