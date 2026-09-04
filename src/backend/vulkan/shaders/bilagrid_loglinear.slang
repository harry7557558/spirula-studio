// Log-linear (9-channel, exp on the 3 diagonal channels) bilateral-grid
// samplers (Vulkan backend), mirroring BilagridLoglinearSampleFwd_kernel.cuh
// and BilagridLoglinearSampleBwdV1_kernel.cuh (uniform + PATCHED).
//
// Spec constants (IDs by declaration order — dispatches must pass ALL):
//   0: kValueQuant  1: kPatched

#include "bilagrid_common.slang"

[SpecializationConstant]
const int kValueQuant = 0;
[SpecializationConstant]
const int kPatched = 0;

struct BlUniformParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* rgb;
    float* output;      // fwd out / bwd_v1_rgb v_output
    int* offsets;
    int* grid_indices;
    int N, L, H, W, m, h, w, h0, w0;
    int has_grid_indices;
    uint total;
    uint wgs_per_row;
};

[ForceInline]
void _bl_pix(int idx, BlUniformParams p, out int ni, out int g_id,
             out int g_off3, out float gx, out float gy) {
    int tmp = idx;
    int wi = tmp % p.w; tmp /= p.w;
    int hi = tmp % p.h; tmp /= p.h;
    int mi = 0;
    if (kPatched != 0) { mi = tmp % p.m; tmp /= p.m; }
    ni = tmp;
    if (kPatched != 0) {
        g_id = ni;
        int o = (ni * p.m + mi) * 2;
        gx = float(wi + p.offsets[o + 0]) / float(p.w0 - 1);
        gy = float(hi + p.offsets[o + 1]) / float(p.h0 - 1);
        g_off3 = (((ni * p.m + mi) * p.h + hi) * p.w + wi) * 3;
    } else {
        g_id = p.has_grid_indices != 0 ? p.grid_indices[ni] : ni;
        gx = float(wi) / float(p.w - 1);
        gy = float(hi) / float(p.h - 1);
        g_off3 = ((ni * p.h + hi) * p.w + wi) * 3;
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_loglinear_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform BlUniformParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int ni, g_id, g_off3;
    float gx, gy;
    _bl_pix(int(idx), p, ni, g_id, g_off3, gx, gy);
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = p.rgb[g_off3 + 0];
    float sg = p.rgb[g_off3 + 1];
    float sb = p.rgb[g_off3 + 2];
    float gz = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    gz = min(max(gz, 0.0f), 1.0f);
    float x = gx * float(p.W - 1);
    float y = gy * float(p.H - 1);
    float z = gz * float(p.L - 1);

    int base = g_id * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 9, x, y, z);

    float d[3] = {0.0f, 0.0f, 0.0f};
    float coeff[3] = {sr, sg, sb};
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++) {
        float val = bg_trilerp(kValueQuant != 0, r, c, ci);
        int si = ci % 3, di = ci / 3;
        if (si == di) val = exp(val);
        d[di] += val * coeff[si];
    }

    p.output[g_off3 + 0] = bg_finite(d[0]) ? d[0] : 0.5f;
    p.output[g_off3 + 1] = bg_finite(d[1]) ? d[1] : 0.5f;
    p.output[g_off3 + 2] = bg_finite(d[2]) ? d[2] : 0.5f;
}

// ---------------------------------------------------------------------------
// backward v1: grid-grad kernel (8x8x1, 3-diagonal shmem preload)
// ---------------------------------------------------------------------------

static const int kBlFootX = kBgV1BlockX + 2;
static const int kBlFootY = kBgV1BlockY + 2;
static const int kBlFootZ = 3;
static const int kBlFullChans = 9;
static const int kBlDiags = 3;
static const int kBlCells = kBlFootX * kBlFootY * kBlFootZ;

groupshared float _bl_shmem[kBlFootZ * kBlFootY * kBlFootX * kBlDiags];

[ForceInline]
int _bl_sh_idx(int lz, int ly, int lx, int diag) {
    return ((lz * kBlFootY + ly) * kBlFootX + lx) * kBlDiags + diag;
}

struct BlV1GridParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* rgb;
    float* v_output;
    float* v_bilagrid;
    int* offsets;
    int* grid_indices;
    int N, L, H, W, m, h, w, h0, w0;
    int mult_x, mult_y, m_batch_stride;
    int has_grid_indices;
    int _pad0;
};

[shader("compute")]
// Flat 64-thread workgroup (X must be a multiple of the pinned subgroup
// size); logical 8x8 decode below matches the CUDA block geometry.
[numthreads(64, 1, 1)]
void bilagrid_loglinear_bwd_v1_grid(uint3 bid: SV_GroupID, uint ltid: SV_GroupThreadID,
                                    uniform BlV1GridParams p) {
    uint3 t = uint3(ltid % 8u, ltid / 8u, 0u);
    int block_z_total =
        kPatched != 0 ? p.N * p.m_batch_stride * p.L : p.N * p.L;
    if (int(bid.z) >= block_z_total) return;

    int idx_block = int(bid.z);
    int zi = idx_block % p.L; idx_block /= p.L;
    int m_batch_i = 0;
    if (kPatched != 0) {
        m_batch_i = idx_block % p.m_batch_stride;
        idx_block /= p.m_batch_stride;
    }
    int ni = idx_block;
    int g_id = kPatched != 0
                   ? ni
                   : (p.has_grid_indices != 0 ? p.grid_indices[ni] : ni);
    int corner_base = g_id * p.L * p.H * p.W * kBlFullChans;

    int base_xi = (int(bid.x) * kBgV1BlockX) / p.mult_x - 1;
    int base_yi = (int(bid.y) * kBgV1BlockY) / p.mult_y - 1;
    int base_zi = zi - 1;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    for (int local = int(ltid); local < kBlCells; local += 64) {
        int lx = local % kBlFootX;
        int ly = (local / kBlFootX) % kBlFootY;
        int lz = local / (kBlFootX * kBlFootY);
        int gxc = min(max(base_xi + lx, 0), p.W - 1);
        int gyc = min(max(base_yi + ly, 0), p.H - 1);
        int gzc = min(max(base_zi + lz, 0), p.L - 1);
        int g_off = corner_base + ((gzc * p.H + gyc) * p.W + gxc) * kBlFullChans;
        // Diagonal full-channel indices: ci_full = 4 * diag (= 3*d + d).
        for (int diag = 0; diag < kBlDiags; diag++)
            _bl_shmem[_bl_sh_idx(lz, ly, lx, diag)] =
                bg_read(kValueQuant != 0, r, g_off + 4 * diag);
    }
    GroupMemoryBarrierWithGroupSync();

    int x_idx = int(bid.x) * kBgV1BlockX + int(t.x);
    int y_idx = int(bid.y) * kBgV1BlockY + int(t.y);
    int xi = x_idx / p.mult_x, xf = x_idx % p.mult_x;
    int yi = y_idx / p.mult_y, yf = y_idx % p.mult_y;
    bool inside = (xi >= 0 && xi < p.W && yi >= 0 && yi < p.H);

    bool slow_path = p.mult_x * p.mult_y == 1 ||
                     (p.mult_x % kBgV1BlockX != 0 ||
                      p.mult_y % kBgV1BlockY != 0);
    if (!inside && slow_path) return;

    float accum[kBlFullChans];
    [ForceUnroll]
    for (int ci = 0; ci < kBlFullChans; ci++) accum[ci] = 0.0f;

    if (inside) {
        int mi_begin = kPatched != 0 ? m_batch_i : 0;
        int mi_end = kPatched != 0 ? p.m : 1;
        int mi_step = kPatched != 0 ? p.m_batch_stride : 1;
        for (int mi = mi_begin; mi < mi_end; mi += mi_step) {
            BgV1Range rng;
            int off_x = 0, off_y = 0;
            if (kPatched != 0) {
                int o = (ni * p.m + mi) * 2;
                off_x = p.offsets[o + 0];
                off_y = p.offsets[o + 1];
                rng = bg_v1_range(xi, yi, xf, yf, p.mult_x, p.mult_y, p.W, p.H,
                                  p.w0, p.h0, off_x, off_y,
                                  min(off_x + p.w, p.w0) - 1,
                                  min(off_y + p.h, p.h0) - 1);
                if (!rng.nonempty && slow_path) continue;
            } else {
                rng = bg_v1_range(xi, yi, xf, yf, p.mult_x, p.mult_y, p.W, p.H,
                                  p.w, p.h, 0, 0, p.w - 1, p.h - 1);
            }

            for (int hi = rng.hi0; hi < rng.hi1; hi++)
            for (int wi = rng.wi0; wi < rng.wi1; wi++) {
                int g_off;
                float x, y;
                if (kPatched != 0) {
                    g_off = (((ni * p.m + mi) * p.h + (hi - off_y)) * p.w +
                             (wi - off_x)) * 3;
                    x = float(wi) / float(p.w0 - 1) * float(p.W - 1);
                    y = float(hi) / float(p.h0 - 1) * float(p.H - 1);
                } else {
                    g_off = ((ni * p.h + hi) * p.w + wi) * 3;
                    x = float(wi) / float(p.w - 1) * float(p.W - 1);
                    y = float(hi) / float(p.h - 1) * float(p.H - 1);
                }
                float sr = p.rgb[g_off + 0];
                float sg = p.rgb[g_off + 1];
                float sb = p.rgb[g_off + 2];
                float z = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
                z = min(max(z, 0.0f), 1.0f) * float(p.L - 1);

                BgCells c = bg_cells_uniform(0, p.L, p.H, p.W, kBlFullChans, x,
                                             y, z);
                if (zi != c.z0 && zi != c.z1) continue;

                float accum_t = bg_v1_accum_t(xi, yi, zi, c);

                int sx0 = max(c.x0 - base_xi, 0);
                int sx1 = min(c.x1 - base_xi, kBlFootX - 1);
                int sy0 = max(c.y0 - base_yi, 0);
                int sy1 = min(c.y1 - base_yi, kBlFootY - 1);
                int sz0 = max(c.z0 - base_zi, 0);
                int sz1 = min(c.z1 - base_zi, kBlFootZ - 1);

                float diags[kBlDiags];
                [ForceUnroll]
                for (int diag = 0; diag < kBlDiags; diag++) {
                    float v000 = _bl_shmem[_bl_sh_idx(sz0, sy0, sx0, diag)];
                    float v001 = _bl_shmem[_bl_sh_idx(sz0, sy0, sx1, diag)];
                    float v010 = _bl_shmem[_bl_sh_idx(sz0, sy1, sx0, diag)];
                    float v011 = _bl_shmem[_bl_sh_idx(sz0, sy1, sx1, diag)];
                    float v100 = _bl_shmem[_bl_sh_idx(sz1, sy0, sx0, diag)];
                    float v101 = _bl_shmem[_bl_sh_idx(sz1, sy0, sx1, diag)];
                    float v110 = _bl_shmem[_bl_sh_idx(sz1, sy1, sx0, diag)];
                    float v111 = _bl_shmem[_bl_sh_idx(sz1, sy1, sx1, diag)];
                    float c00 = v000 * (1.0f - c.fx) + v001 * c.fx;
                    float c01 = v010 * (1.0f - c.fx) + v011 * c.fx;
                    float c10 = v100 * (1.0f - c.fx) + v101 * c.fx;
                    float c11 = v110 * (1.0f - c.fx) + v111 * c.fx;
                    float c0 = c00 * (1.0f - c.fy) + c01 * c.fy;
                    float c1 = c10 * (1.0f - c.fy) + c11 * c.fy;
                    diags[diag] = c0 * (1.0f - c.fz) + c1 * c.fz;
                }

                float dcol[3] = {p.v_output[g_off + 0], p.v_output[g_off + 1],
                                 p.v_output[g_off + 2]};
                // Linear in the incoming gradient: a zero one -- a masked
                // pixel, mostly -- adds nothing to any channel.
                if (dcol[0] == 0.0f && dcol[1] == 0.0f && dcol[2] == 0.0f)
                    continue;
                float coeff[3] = {sr, sg, sb};
                [ForceUnroll]
                for (int ci = 0; ci < kBlFullChans; ci++) {
                    int si = ci % 3, di = ci / 3;
                    float grad_weight = coeff[si] * dcol[di];
                    if (si == di) grad_weight *= exp(diags[si]);
                    accum[ci] += accum_t * grad_weight;
                }
            }
        }
    }

    int out_idx_start =
        (((ni * p.L + zi) * p.H + yi) * p.W + xi) * kBlFullChans;

    if (slow_path) {
        [ForceUnroll]
        for (int ci = 0; ci < kBlFullChans; ci++) {
            if (bg_finite(accum[ci]) && accum[ci] != 0.0f)
                atomic_add_f32(&p.v_bilagrid[out_idx_start + ci], accum[ci]);
        }
        return;
    }

    [ForceUnroll]
    for (int ci = 0; ci < kBlFullChans; ci++)
        bg_block_add_64(&p.v_bilagrid[out_idx_start + ci], accum[ci], ltid);
}

// ---------------------------------------------------------------------------
// backward v1: rgb-grad kernel (flat 256)
// ---------------------------------------------------------------------------

struct BlV1RgbParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* rgb;
    float* v_output;
    float* v_rgb;
    int* offsets;
    int* grid_indices;
    int N, L, H, W, m, h, w, h0, w0;
    int has_grid_indices;
    uint total;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_loglinear_bwd_v1_rgb(uint3 wg: SV_GroupID,
                                   uint tid: SV_GroupThreadID,
                                   uniform BlV1RgbParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;

    BlUniformParams u;
    u.fp32 = p.fp32; u.q16 = p.q16; u.vbounds = p.vbounds;
    u.rgb = p.rgb; u.output = p.v_output;
    u.offsets = p.offsets; u.grid_indices = p.grid_indices;
    u.N = p.N; u.L = p.L; u.H = p.H; u.W = p.W;
    u.m = p.m; u.h = p.h; u.w = p.w; u.h0 = p.h0; u.w0 = p.w0;
    u.has_grid_indices = p.has_grid_indices;
    u.total = p.total; u.wgs_per_row = p.wgs_per_row;
    int ni, g_id, g_off3;
    float gx, gy;
    _bl_pix(int(idx), u, ni, g_id, g_off3, gx, gy);
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = p.rgb[g_off3 + 0];
    float sg = p.rgb[g_off3 + 1];
    float sb = p.rgb[g_off3 + 2];
    float dcol[3] = {p.v_output[g_off3 + 0], p.v_output[g_off3 + 1],
                     p.v_output[g_off3 + 2]};

    // The image gradient is linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- is zero out and skips the corner loads.
    if (dcol[0] == 0.0f && dcol[1] == 0.0f && dcol[2] == 0.0f) {
        p.v_rgb[g_off3 + 0] = 0.0f;
        p.v_rgb[g_off3 + 1] = 0.0f;
        p.v_rgb[g_off3 + 2] = 0.0f;
        return;
    }

    float x = gx * float(p.W - 1);
    float y = gy * float(p.H - 1);
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = min(max(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * float(p.L - 1);

    int base = g_id * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 9, x, y, z);
    float w8[8];
    bg_corner_weights(c, w8);

    float coeff[3] = {sr, sg, sb};
    float v[3] = {0.0f, 0.0f, 0.0f};
    float post_exp[3] = {0.0f, 0.0f, 0.0f};
    [ForceUnroll]
    for (int si = 0; si < 3; si++) {
        [ForceUnroll]
        for (int di = 0; di < 3; di++) {
            int ci = 3 * di + si;
            float val = 0.0f;
            [ForceUnroll]
            for (int corner = 0; corner < 8; corner++)
                val += bg_read(kValueQuant != 0, r, c.o[corner] + ci) *
                       w8[corner];
            if (si == di) {
                val = exp(val);
                post_exp[si] = val;
            }
            v[si] += val * dcol[di];
        }
    }

    float d8[8];
    bg_dwdz(c, d8);
    float gz_grad = 0.0f;
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        float trilerp = 0.0f;
        [ForceUnroll]
        for (int ci = 0; ci < 9; ci++) {
            float gv = bg_read(kValueQuant != 0, r, c.o[corner] + ci);
            int si = ci % 3, di = ci / 3;
            if (si == di) gv *= post_exp[si];
            trilerp += gv * coeff[si] * dcol[di];
        }
        gz_grad += d8[corner] * float(p.L - 1) * trilerp;
    }
    if (!gz_in_range) gz_grad = 0.0f;
    float vr = v[0] + kC2G_r * gz_grad;
    float vg = v[1] + kC2G_g * gz_grad;
    float vb = v[2] + kC2G_b * gz_grad;
    p.v_rgb[g_off3 + 0] = bg_finite(vr) ? vr : 0.0f;
    p.v_rgb[g_off3 + 1] = bg_finite(vg) ? vg : 0.0f;
    p.v_rgb[g_off3 + 2] = bg_finite(vb) ? vb : 0.0f;
}

// ---------------------------------------------------------------------------
// backward v2: thread-per-pixel SCATTER (fused grid + image grad). 9 channels
// = 3 out x 3 in (ci = 3*di + si); diagonal (si==di) passes through exp().
// Reads grid values from the g_id slot, scatters grad into the per-batch ni
// buffer. See BilagridBackwardSelection.md and
// BilagridLoglinearSampleBwdV2_kernel.cuh.
// ---------------------------------------------------------------------------

struct BlV2Params {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* rgb;
    float* v_output;
    float* v_bilagrid;
    float* v_rgb;
    int* grid_indices;
    int N, L, H, W, h, w;
    int has_grid_indices;
    uint total;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_loglinear_bwd_v2(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                               uniform BlV2Params p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int tmp = int(idx);
    int wi = tmp % p.w; tmp /= p.w;
    int hi = tmp % p.h; tmp /= p.h;
    int ni = tmp;
    int g_id = p.has_grid_indices != 0 ? p.grid_indices[ni] : ni;
    int g_off = ((ni * p.h + hi) * p.w + wi) * 3;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = p.rgb[g_off + 0], sg = p.rgb[g_off + 1], sb = p.rgb[g_off + 2];
    float dcol[3] = {p.v_output[g_off + 0], p.v_output[g_off + 1],
                     p.v_output[g_off + 2]};
    float coeff[3] = {sr, sg, sb};

    float x = float(wi) / float(p.w - 1) * float(p.W - 1);
    float y = float(hi) / float(p.h - 1) * float(p.H - 1);
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = min(max(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * float(p.L - 1);

    int base = g_id * p.L * p.H * p.W * 9;
    int out_base = ni * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 9, x, y, z);
    float w8[8];
    bg_corner_weights(c, w8);

    // interp (sum w8*v, matching v1 rgb), exp on diagonal, image grad + gradwt
    float v[3] = {0.0f, 0.0f, 0.0f};
    float grad_weight[9];
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++) {
        int si = ci % 3, di = ci / 3;
        float val = 0.0f;
        [ForceUnroll]
        for (int corner = 0; corner < 8; corner++)
            val += bg_read(kValueQuant != 0, r, c.o[corner] + ci) * w8[corner];
        // On the diagonal the stored value is a log; exp() it once and reuse
        // the same `val` for both the image gradient and the weight gradient.
        if (si == di) val = exp(val);
        v[si] += val * dcol[di];
        grad_weight[ci] = coeff[si] * dcol[di];
        if (si == di) grad_weight[ci] *= val;
    }

    float d8[8];
    bg_dwdz(c, d8);
    float gz_grad = 0.0f;
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        int wbase = c.o[corner] - base + out_base;
        float trilerp = 0.0f;
        [ForceUnroll]
        for (int ci = 0; ci < 9; ci++) {
            float gw = grad_weight[ci];
            trilerp += bg_read(kValueQuant != 0, r, c.o[corner] + ci) * gw;
            float aa = w8[corner] * gw;
            if (aa != 0.0f && bg_finite(aa))
                atomic_add_f32(&p.v_bilagrid[wbase + ci], aa);
        }
        gz_grad += d8[corner] * float(p.L - 1) * trilerp;
    }

    if (!gz_in_range) gz_grad = 0.0f;
    float vr = v[0] + kC2G_r * gz_grad;
    float vg = v[1] + kC2G_g * gz_grad;
    float vb = v[2] + kC2G_b * gz_grad;
    p.v_rgb[g_off + 0] = bg_finite(vr) ? vr : 0.0f;
    p.v_rgb[g_off + 1] = bg_finite(vg) ? vg : 0.0f;
    p.v_rgb[g_off + 2] = bg_finite(vb) ? vb : 0.0f;
}
