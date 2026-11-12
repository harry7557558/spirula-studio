// The per-image ground-truth warp that absorbs a camera's calibration
// residual (Vulkan twin of kernels/pixelwise/CalibWarp.cu; the layout of the
// table is in kernels/pixelwise/CalibWarp.cuh).
//
// Spec constants (IDs by declaration order -- dispatches must pass ALL):
//   0: kParallax -- 1: four floats per control point, the last two divided
//                   by the rendered depth; 0: two.
//   1: kAuxKind  -- calib_warp_aux_fwd only: 0 uint8 mask, 1 depth, 2 normal.

#include "atomic_float.slang"
#include "int64_compat.slang"
#include "rgb_format.slang"

[SpecializationConstant]
const int kParallax = 0;
[SpecializationConstant]
const int kAuxKind = 0;

static const int kMaxQ = 13;   // 4 gradient + 9 Hessian floats with parallax
static const uint kBlock = 256;

int cw_D() { return kParallax != 0 ? 4 : 2; }
int cw_DH() { return kParallax != 0 ? 9 : 3; }

// Mirrors CalibWarpParams in backend/vulkan/kernels/CalibWarp.cpp.
struct CalibWarpParams {
    uint* gt;           // [B,H,W,3] as gt_fmt
    uint* render;       // [B,H,W,3] as render_fmt
    float* v_rgb;       // [B,H,W,3] (has_v)
    float* depth;       // [B,H,W] sum(w d) (has_depth)
    float* Ts;          // [B,H,W] transmittance (has_Ts)
    float* params;      // [N, n*n*D]
    float4* intrins;    // [B]
    int* cam_indices;   // [B] (has_cam)
    float* grad;        // [B, n*n*D]
    float* hess;        // [B, n*n*DH]
    uint* out;          // [B,H,W,3] as out_fmt (f32 or f16)
    uint* mask;         // [B,Hm,Wm] bytes (has_mask)
    int B, H, W, n;
    int Hm, Wm, has_mask, has_Ts;
    uint out_fmt;
    uint gt_fmt, render_fmt;
    int has_v, has_depth, has_cam;
    int sub_blocks, chunk;
    float v_scale, eps;
    float step, damping, smooth, prior, max_step;
    uint wgs_per_row;
    int _pad0;
    float f_scale;      // focal multiplier of the pyramid level
    int has_intr;
    int gt_warped;      // gt already resampled at p + flow (accumulate)
};

struct CwCell {
    int i0, j0;
    float fu, fv;
};

CwCell cw_cell(int x, int y, int W, int H, int n) {
    float u = (float(x) + 0.5f) * float(n - 1) / float(W);
    float v = (float(y) + 0.5f) * float(n - 1) / float(H);
    CwCell c;
    c.i0 = min(max(int(floor(u)), 0), n - 2);
    c.j0 = min(max(int(floor(v)), 0), n - 2);
    c.fu = u - float(c.i0);
    c.fv = v - float(c.j0);
    return c;
}

float4 cw_weights(CwCell c) {
    return float4((1.0f - c.fu) * (1.0f - c.fv), c.fu * (1.0f - c.fv),
                  (1.0f - c.fu) * c.fv, c.fu * c.fv);
}

int cw_corner(CwCell c, int k, int n) {
    return (c.j0 + (k >> 1)) * n + c.i0 + (k & 1);
}

float cw_inv_depth(CalibWarpParams p, gindex_t pix) {
    if (kParallax == 0 || p.has_depth == 0) return 0.0f;
    float d = p.depth[pix];
    float a = p.has_Ts != 0 ? 1.0f - p.Ts[pix] : 1.0f;
    return d > 1e-4f && a > 0.0f ? min(a / d, 1e4f) : 0.0f;
}

float2 cw_flow(CalibWarpParams p, gindex_t row, CwCell c, float e, float sx, float sy) {
    float4 w = cw_weights(c);
    int D = cw_D();
    float dx = 0.0f, dy = 0.0f;
    [ForceUnroll]
    for (int k = 0; k < 4; ++k) {
        gindex_t o = row + gindex_t(cw_corner(c, k, p.n) * D);
        dx += w[k] * p.params[o];
        dy += w[k] * p.params[o + 1];
        if (kParallax != 0) {
            dx += w[k] * e * p.params[o + 2];
            dy += w[k] * e * p.params[o + 3];
        }
    }
    return float2(dx * sx, dy * sy);
}

float3 cw_tap(uint* img, uint fmt, gindex_t base, int W, int H, int x, int y) {
    x = min(max(x, 0), W - 1);
    y = min(max(y, 0), H - 1);
    return rgb_load3(img, base + gindex_t(y) * gindex_t(W) + gindex_t(x), fmt);
}

float3 cw_sample(uint* img, uint fmt, gindex_t base, int W, int H, float x, float y) {
    float xf = floor(x), yf = floor(y);
    int x0 = int(xf), y0 = int(yf);
    float ax = x - xf, ay = y - yf;
    float3 a = cw_tap(img, fmt, base, W, H, x0, y0);
    float3 b = cw_tap(img, fmt, base, W, H, x0 + 1, y0);
    float3 c = cw_tap(img, fmt, base, W, H, x0, y0 + 1);
    float3 d = cw_tap(img, fmt, base, W, H, x0 + 1, y0 + 1);
    return (1 - ax) * (1 - ay) * a + ax * (1 - ay) * b + (1 - ax) * ay * c + ax * ay * d;
}

float4 cw_catmull_rom(float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * float4(-t3 + 2.0f * t2 - t, 3.0f * t3 - 5.0f * t2 + 2.0f,
                         -3.0f * t3 + 4.0f * t2 + t, t3 - t2);
}

// Catmull-Rom clamped to the four nearest taps; see cw_sample_cubic in CalibWarp.cu.
float3 cw_sample_cubic(uint* img, uint fmt, gindex_t base, int W, int H, float x, float y) {
    float xf = floor(x), yf = floor(y);
    int x0 = int(xf), y0 = int(yf);
    float4 wx = cw_catmull_rom(x - xf), wy = cw_catmull_rom(y - yf);
    float3 acc = float3(0.0f), lo = float3(1e30f), hi = float3(-1e30f);
    [ForceUnroll]
    for (int j = 0; j < 4; ++j)
        [ForceUnroll]
        for (int i = 0; i < 4; ++i) {
            float3 t = cw_tap(img, fmt, base, W, H, x0 - 1 + i, y0 - 1 + j);
            acc += wx[i] * wy[j] * t;
            if ((i == 1 || i == 2) && (j == 1 || j == 2)) {
                lo = min(lo, t);
                hi = max(hi, t);
            }
        }
    return clamp(acc, lo, hi);
}

int cw_cam(CalibWarpParams p, uint b) {
    return p.has_cam != 0 ? p.cam_indices[b] : int(b);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void calib_warp_gt_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform CalibWarpParams* pp) {
    CalibWarpParams p = *pp;
    // Flat over every pixel of the batch, and no early return: a float16
    // store pairs each even lane with the next pixel.
    uint gid = (wg.y * p.wgs_per_row + wg.x) * kBlock + tid;
    uint hw = uint(p.H * p.W);
    bool active = gid < uint(p.B) * hw;
    uint flat = active ? gid : 0u;
    uint b = flat / hw, pix = flat % hw;
    int x = int(pix % uint(p.W)), y = int(pix / uint(p.W));
    int cam = cw_cam(p, b);
    float4 intr = p.intrins[b];
    gindex_t base = gindex_t(b) * gindex_t(p.H) * gindex_t(p.W);
    CwCell c = cw_cell(x, y, p.W, p.H, p.n);
    float e = cw_inv_depth(p, base + gindex_t(pix));
    gindex_t row = gindex_t(cam) * gindex_t(p.n * p.n * cw_D());
    float2 fl = cw_flow(p, row, c, e, 1e-3f * intr.x, 1e-3f * intr.y);
    float sx = float(x) + fl.x, sy = float(y) + fl.y;
    float3 v;
    if (sx >= 0.0f && sx <= float(p.W - 1) && sy >= 0.0f && sy <= float(p.H - 1))
        v = cw_sample_cubic(p.gt, p.gt_fmt, base, p.W, p.H, sx, sy);
    else
        v = rgb_load3(p.render, base + gindex_t(pix), p.render_fmt);
    rgb_store3(p.out, flat, v, p.out_fmt, active);
}

groupshared float _cw_red[32 * 4 * kMaxQ];

// One workgroup per (image, cell, slice of the cell's pixels); every pixel of
// a cell feeds the same four control points, so the reduction is per corner.
[shader("compute")]
[numthreads(256, 1, 1)]
void calib_warp_accum(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform CalibWarpParams* pp) {
    CalibWarpParams p = *pp;
    int D = cw_D(), DH = cw_DH(), Q = D + DH;
    int n = p.n, W = p.W, H = p.H;
    uint b = wg.y;
    int cell = int(wg.x) / p.sub_blocks, s = int(wg.x) % p.sub_blocks;
    int ci = cell % (n - 1), cj = cell / (n - 1);
    int bx0 = max(0, int(floor(float(ci) * W / float(n - 1))) - 1);
    int bx1 = min(W, int(ceil(float(ci + 1) * W / float(n - 1))) + 1);
    int by0 = max(0, int(floor(float(cj) * H / float(n - 1))) - 1);
    int by1 = min(H, int(ceil(float(cj + 1) * H / float(n - 1))) + 1);
    int bw = bx1 - bx0;
    int npx = bw * (by1 - by0);
    int start = s * p.chunk, end = min(npx, start + p.chunk);

    int cam = cw_cam(p, b);
    float4 intr = p.intrins[b];
    float scx = 1e-3f * intr.x * p.f_scale, scy = 1e-3f * intr.y * p.f_scale;
    gindex_t row = gindex_t(cam) * gindex_t(n * n * D);
    gindex_t base = gindex_t(b) * gindex_t(H) * gindex_t(W);

    float acc[4 * kMaxQ];
    [ForceUnroll]
    for (int i = 0; i < 4 * kMaxQ; ++i) acc[i] = 0.0f;

    for (int idx = start + int(tid); idx < end; idx += int(kBlock)) {
        int x = bx0 + idx % bw, y = by0 + idx / bw;
        CwCell c = cw_cell(x, y, W, H, n);
        if (c.i0 != ci || c.j0 != cj) continue;
        if (p.has_mask != 0) {
            int my = min(int((float(y) + 0.5f) * float(p.Hm) / float(H)), p.Hm - 1);
            int mx = min(int((float(x) + 0.5f) * float(p.Wm) / float(W)), p.Wm - 1);
            if (u8_load(p.mask, (gindex_t(b) * gindex_t(p.Hm) + gindex_t(my)) * gindex_t(p.Wm) + gindex_t(mx)) == 0u)
                continue;
        }
        gindex_t pix = base + gindex_t(y) * gindex_t(W) + gindex_t(x);
        float e = cw_inv_depth(p, pix);
        float2 fl = cw_flow(p, row, c, e, scx, scy);
        float sx = float(x) + fl.x, sy = float(y) + fl.y;
        // The central difference reads one pixel either side.
        if (!(sx >= 1.0f && sx <= float(W - 2) && sy >= 1.0f && sy <= float(H - 2)))
            continue;
        float3 I, gx, gy;
        if (p.gt_warped != 0) {
            // Already resampled at p + flow; see calib_warp_accum_kernel.
            if (x < 1 || x > W - 2 || y < 1 || y > H - 2) continue;
            I = cw_tap(p.gt, p.gt_fmt, base, W, H, x, y);
            gx = (cw_tap(p.gt, p.gt_fmt, base, W, H, x + 1, y) -
                  cw_tap(p.gt, p.gt_fmt, base, W, H, x - 1, y)) * (0.5f * scx);
            gy = (cw_tap(p.gt, p.gt_fmt, base, W, H, x, y + 1) -
                  cw_tap(p.gt, p.gt_fmt, base, W, H, x, y - 1)) * (0.5f * scy);
        } else {
            I = cw_sample(p.gt, p.gt_fmt, base, W, H, sx, sy);
            gx = (cw_sample(p.gt, p.gt_fmt, base, W, H, sx + 1.0f, sy) -
                  cw_sample(p.gt, p.gt_fmt, base, W, H, sx - 1.0f, sy)) * (0.5f * scx);
            gy = (cw_sample(p.gt, p.gt_fmt, base, W, H, sx, sy + 1.0f) -
                  cw_sample(p.gt, p.gt_fmt, base, W, H, sx, sy - 1.0f)) * (0.5f * scy);
        }
        float3 r = I - rgb_load3(p.render, pix, p.render_fmt);
        float rho = p.eps * rsqrt(dot(r, r) * (1.0f / 3.0f) + p.eps * p.eps);
        float3 rg;
        if (p.has_v != 0) {
            rg = -p.v_scale * float3(p.v_rgb[3 * pix], p.v_rgb[3 * pix + 1], p.v_rgb[3 * pix + 2]);
        } else {
            rg = rho * r;
        }
        float g0 = dot(gx, rg), g1 = dot(gy, rg);
        float hA = rho * dot(gx, gx), hB = rho * dot(gx, gy), hC = rho * dot(gy, gy);
        float4 w = cw_weights(c);
        [ForceUnroll]
        for (int k = 0; k < 4; ++k) {
            int a = k * kMaxQ;
            acc[a + 0] += w[k] * g0;
            acc[a + 1] += w[k] * g1;
            if (kParallax != 0) {
                acc[a + 2] += w[k] * e * g0;
                acc[a + 3] += w[k] * e * g1;
                float we = w[k] * e, wee = w[k] * e * e;
                acc[a + 4] += w[k] * hA;
                acc[a + 5] += w[k] * hB;
                acc[a + 6] += w[k] * hC;
                acc[a + 7] += we * hA;
                acc[a + 8] += we * hB;
                acc[a + 9] += we * hC;
                acc[a + 10] += wee * hA;
                acc[a + 11] += wee * hB;
                acc[a + 12] += wee * hC;
            } else {
                acc[a + 2] += w[k] * hA;
                acc[a + 3] += w[k] * hB;
                acc[a + 4] += w[k] * hC;
            }
        }
    }

    uint wsize = WaveGetLaneCount();
    uint wave = tid / wsize;
    [ForceUnroll]
    for (int i = 0; i < 4 * kMaxQ; ++i) {
        float v = WaveActiveSum(acc[i]);
        if (WaveIsFirstLane()) _cw_red[wave * uint(4 * kMaxQ) + uint(i)] = v;
    }
    GroupMemoryBarrierWithGroupSync();
    int t = int(tid);
    if (t < 4 * kMaxQ) {
        int k = t / kMaxQ, q = t % kMaxQ;
        if (q < Q) {
            float tot = 0.0f;
            for (uint wv = 0; wv < kBlock / wsize; ++wv) tot += _cw_red[wv * uint(4 * kMaxQ) + uint(t)];
            if (tot != 0.0f && !isinf(tot) && !isnan(tot)) {
                int kk = (cj + (k >> 1)) * n + ci + (k & 1);
                gindex_t cp = gindex_t(b) * gindex_t(n * n) + gindex_t(kk);
                if (q < D)
                    atomic_add_f32(&p.grad[cp * gindex_t(D) + gindex_t(q)], tot);
                else
                    atomic_add_f32(&p.hess[cp * gindex_t(DH) + gindex_t(q - D)], tot);
            }
        }
    }
}

groupshared float _cw_sum[32];

float cw_block_sum(float v, uint tid) {
    uint wsize = WaveGetLaneCount();
    float ws = WaveActiveSum(v);
    GroupMemoryBarrierWithGroupSync();
    if (WaveIsFirstLane()) _cw_sum[tid / wsize] = ws;
    GroupMemoryBarrierWithGroupSync();
    float t = 0.0f;
    for (uint w = 0; w < kBlock / wsize; ++w) t += _cw_sum[w];
    return t;
}

// Solves M x = g in place for a symmetric positive definite 4x4 M.
bool cw_cholesky_solve4(float4x4 M, inout float4 g) {
    float L[4][4];
    [ForceUnroll]
    for (int i = 0; i < 4; ++i)
        [ForceUnroll]
        for (int j = 0; j < 4; ++j) L[i][j] = 0.0f;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j <= i; ++j) {
            float s = M[i][j];
            for (int k = 0; k < j; ++k) s -= L[i][k] * L[j][k];
            if (i == j) {
                if (!(s > 0.0f)) return false;
                L[i][i] = sqrt(s);
            } else {
                L[i][j] = s / L[j][j];
            }
        }
    }
    float z[4] = {0.0f, 0.0f, 0.0f, 0.0f};
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

// One workgroup per image of the batch, one thread per control point; see
// calib_warp_update_kernel in CalibWarp.cu for the update.
[shader("compute")]
[numthreads(256, 1, 1)]
void calib_warp_update(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform CalibWarpParams* pp) {
    CalibWarpParams p = *pp;
    int D = cw_D(), DH = cw_DH();
    int n = p.n, nn = n * n;
    int cam = cw_cam(p, wg.x);
    int k = int(tid);
    bool active = k < nn;
    // A step is capped in pixels of this level: what the linearization holds.
    float max_step = p.max_step;
    if (p.has_intr != 0) {
        float4 intr = p.intrins[wg.x];
        max_step /= 1e-3f * max(intr.x, intr.y) * p.f_scale;
    }
    gindex_t P0 = gindex_t(cam) * gindex_t(nn * D);
    gindex_t G0 = gindex_t(wg.x) * gindex_t(nn * D);
    gindex_t H0 = gindex_t(wg.x) * gindex_t(nn * DH);

    float4 g = float4(0.0f), th = float4(0.0f);
    float h[9];
    [ForceUnroll]
    for (int i = 0; i < 9; ++i) h[i] = (active && i < DH) ? p.hess[H0 + gindex_t(k * DH + i)] : 0.0f;
    [ForceUnroll]
    for (int i = 0; i < 4; ++i) {
        g[i] = (active && i < D) ? p.grad[G0 + gindex_t(k * D + i)] : 0.0f;
        th[i] = (active && i < D) ? p.params[P0 + gindex_t(k * D + i)] : 0.0f;
    }
    float ta = cw_block_sum(active ? h[0] + h[2] : 0.0f, tid) / (2.0f * float(nn));
    float tb = cw_block_sum((active && kParallax != 0) ? h[6] + h[8] : 0.0f, tid) / (2.0f * float(nn));

    float4 delta = float4(0.0f);
    bool ok = active && ta > 0.0f;
    if (ok) {
        int x = k % n, y = k / n;
        float4 lap = float4(0.0f);
        int deg = 0;
        [ForceUnroll]
        for (int q = 0; q < 4; ++q) {
            int nx = x + (q == 0 ? -1 : q == 1 ? 1 : 0);
            int ny = y + (q == 2 ? -1 : q == 3 ? 1 : 0);
            if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
            ++deg;
            gindex_t o = P0 + gindex_t((ny * n + nx) * D);
            for (int i = 0; i < D; ++i) lap[i] += th[i] - p.params[o + gindex_t(i)];
        }
        float4 rhs = float4(0.0f), diag = float4(0.0f);
        for (int i = 0; i < D; ++i) {
            float tau = i < 2 ? ta : (tb > 0.0f ? tb : ta);
            rhs[i] = g[i] + p.smooth * tau * lap[i] + p.prior * tau * th[i];
            diag[i] = tau * (2.0f * p.smooth * float(deg) + p.prior + p.damping);
        }
        if (kParallax == 0) {
            float a = h[0] + diag[0], bb = h[1], c = h[2] + diag[1];
            float det = a * c - bb * bb;
            ok = det > 0.0f;
            if (ok) {
                delta[0] = (c * rhs[0] - bb * rhs[1]) / det;
                delta[1] = (a * rhs[1] - bb * rhs[0]) / det;
            }
        } else {
            float4x4 M = float4x4(h[0] + diag[0], h[1], h[3], h[4],
                                  h[1], h[2] + diag[1], h[4], h[5],
                                  h[3], h[4], h[6] + diag[2], h[7],
                                  h[4], h[5], h[7], h[8] + diag[3]);
            ok = cw_cholesky_solve4(M, rhs);
            delta = rhs;
        }
        float shrink = 1.0f;
        for (int part = 0; ok && part < D / 2; ++part) {
            float m = sqrt(delta[2 * part] * delta[2 * part] +
                           delta[2 * part + 1] * delta[2 * part + 1]);
            if (isinf(m) || isnan(m)) ok = false;
            else if (m > max_step) shrink = min(shrink, max_step / m);
        }
        delta *= shrink;
    }
    GroupMemoryBarrierWithGroupSync();  // every neighbour read above happens before any write
    if (active) {
        for (int i = 0; i < D; ++i) {
            if (ok) p.params[P0 + gindex_t(k * D + i)] = th[i] - p.step * delta[i];
            p.grad[G0 + gindex_t(k * D + i)] = 0.0f;
        }
        for (int i = 0; i < DH; ++i) p.hess[H0 + gindex_t(k * DH + i)] = 0.0f;
    }
}

// Every output pixel averages its 2x2 source block; see the CUDA twin. The
// output size rides in Hm, Wm.
[shader("compute")]
[numthreads(256, 1, 1)]
void calib_warp_downsample2(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform CalibWarpParams* pp) {
    CalibWarpParams p = *pp;
    uint i = (wg.y * p.wgs_per_row + wg.x) * kBlock + tid;
    uint W2 = uint(p.Wm), H2 = uint(p.Hm);
    if (i >= uint(p.B) * H2 * W2) return;
    int x = int(i % W2), y = int((i / W2) % H2), b = int(i / (W2 * H2));
    gindex_t base = gindex_t(b) * gindex_t(p.H) * gindex_t(p.W);
    float3 acc = float3(0.0f);
    [ForceUnroll]
    for (int dy = 0; dy < 2; ++dy)
        [ForceUnroll]
        for (int dx = 0; dx < 2; ++dx)
            acc += 0.25f * cw_tap(p.gt, p.gt_fmt, base, p.W, p.H, 2 * x + dx, 2 * y + dy);
    p.out[3 * i] = asuint(acc.x);
    p.out[3 * i + 1] = asuint(acc.y);
    p.out[3 * i + 2] = asuint(acc.z);
}

// One aux map at its own size (Hm, Wm); see calib_warp_aux_fwd_kernel in
// CalibWarp.cu for the sampling.
[shader("compute")]
[numthreads(256, 1, 1)]
void calib_warp_aux_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform CalibWarpParams* pp) {
    CalibWarpParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * kBlock + tid;
    uint hw = uint(p.Hm * p.Wm);
    if (gid >= uint(p.B) * hw) return;
    uint b = gid / hw, pix = gid % hw;
    int x = int(pix % uint(p.Wm)), y = int(pix / uint(p.Wm));
    int cam = cw_cam(p, b);
    float4 intr = p.intrins[b];
    CwCell c = cw_cell(x, y, p.Wm, p.Hm, p.n);
    float e = 0.0f;
    if (kParallax != 0) {
        int xr = min(int((float(x) + 0.5f) * float(p.W) / float(p.Wm)), p.W - 1);
        int yr = min(int((float(y) + 0.5f) * float(p.H) / float(p.Hm)), p.H - 1);
        e = cw_inv_depth(p, (gindex_t(b) * gindex_t(p.H) + gindex_t(yr)) * gindex_t(p.W) +
                                gindex_t(xr));
    }
    gindex_t row = gindex_t(cam) * gindex_t(p.n * p.n * cw_D());
    float2 fl = cw_flow(p, row, c, e, 1e-3f * intr.x * float(p.Wm) / float(p.W),
                        1e-3f * intr.y * float(p.Hm) / float(p.H));
    float sx = float(x) + fl.x, sy = float(y) + fl.y;
    gindex_t plane = gindex_t(b) * gindex_t(hw);
    gindex_t o = gindex_t(gid);
    if (kAuxKind == 0) {
        int ix = int(floor(sx + 0.5f)), iy = int(floor(sy + 0.5f));
        bool inb = ix >= 0 && ix < p.Wm && iy >= 0 && iy < p.Hm;
        uint v = 0u;
        if (inb && u8_load(p.gt, o) != 0u)
            v = u8_load(p.gt, plane + gindex_t(iy) * gindex_t(p.Wm) + gindex_t(ix));
        u8_store(p.out, o, v);
        return;
    }
    const int ch = kAuxKind == 1 ? 1 : 3;
    int x0 = int(floor(sx)), y0 = int(floor(sy));
    float fx = sx - float(x0), fy = sy - float(y0);
    float3 acc = float3(0.0f);
    float wsum = 0.0f;
    [ForceUnroll]
    for (int q = 0; q < 4; ++q) {
        int tx = x0 + (q & 1), ty = y0 + (q >> 1);
        if (tx < 0 || tx >= p.Wm || ty < 0 || ty >= p.Hm) continue;
        gindex_t t = (plane + gindex_t(ty) * gindex_t(p.Wm) + gindex_t(tx)) * gindex_t(ch);
        float3 v = float3(asfloat(elem_at(p.gt, t)[0]), 0.0f, 0.0f);
        bool ok;
        if (kAuxKind == 1) {
            ok = v.x > 0.0f;   // core/Interpolation.cuh's gt_depth_valid
        } else {
            v.y = asfloat(elem_at(p.gt, t + 1)[0]);
            v.z = asfloat(elem_at(p.gt, t + 2)[0]);
            // core/Interpolation.cuh's gt_normal_valid.
            ok = v.x + v.y + v.z > -2.366f && dot(v, v) > 0.25f;
        }
        if (!ok) continue;
        float w = ((q & 1) != 0 ? fx : 1.0f - fx) * ((q >> 1) != 0 ? fy : 1.0f - fy);
        acc += w * v;
        wsum += w;
    }
    float3 r = wsum > 0.0f ? acc / wsum : float3(kAuxKind == 1 ? 0.0f : -1.0f);
    elem_at(p.out, o * gindex_t(ch))[0] = asuint(r.x);
    if (kAuxKind == 2) {
        elem_at(p.out, o * gindex_t(ch) + 1)[0] = asuint(r.y);
        elem_at(p.out, o * gindex_t(ch) + 2)[0] = asuint(r.z);
    }
}
