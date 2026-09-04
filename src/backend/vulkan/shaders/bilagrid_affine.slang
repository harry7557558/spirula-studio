// Affine (12-channel) bilateral-grid samplers (Vulkan backend), mirroring
// kernels/bilagrid/BilagridSampleFwd.cu, BilagridSampleBwd_kernel.cuh,
// BilagridUniformSampleFwd_kernel.cuh, BilagridUniformSampleBwdV1_kernel.cuh
// and BilagridUniformSampleBwdV2_kernel.cuh (uniform + PATCHED variants).
//
// Spec constants (IDs by declaration order — dispatches must pass ALL):
//   0: kValueQuant  — 0 = fp32 grid, 1 = 16-bit packed + bounds
//   1: kPatched     — 0 = uniform, 1 = patched (offsets + h0/w0)
//   2: kCoordsGrad  — bilagrid_sample_bwd only: also write v_coords

#include "bilagrid_common.slang"

[SpecializationConstant]
const int kValueQuant = 0;
[SpecializationConstant]
const int kPatched = 0;
[SpecializationConstant]
const int kCoordsGrad = 0;

// ---------------------------------------------------------------------------
// bilagrid_sample_forward / backward (per-pixel coords, [N,m,h,w])
// ---------------------------------------------------------------------------

struct BgSampleParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* coords;      // [N,m,h,w,2]
    float* rgb;         // [N,m,h,w,3]
    float* out_or_vout; // fwd: output; bwd: v_output
    float* v_bilagrid;  // bwd only
    float* v_coords;    // bwd only (kCoordsGrad)
    float* v_rgb;       // bwd only
    int N, L, H, W, m, h, w;
    uint total;
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_sample_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform BgSampleParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int g_off = int(idx);

    BgReader r = {p.fp32, p.q16, p.vbounds};
    float sr = p.rgb[3 * g_off + 0];
    float sg = p.rgb[3 * g_off + 1];
    float sb = p.rgb[3 * g_off + 2];

    int tmp = g_off;
    tmp /= p.w; tmp /= p.h; tmp /= p.m;
    int ni = tmp;

    float gx = p.coords[2 * g_off + 0];
    float gy = p.coords[2 * g_off + 1];
    float gz = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float x = gx * float(p.W - 1);
    float y = gy * float(p.H - 1);
    float z = gz * float(p.L - 1);

    int base = ni * p.L * p.H * p.W * 12;
    BgCells c = bg_cells_clamped(base, p.L, p.H, p.W, 12, x, y, z);

    float d[3] = {0.0f, 0.0f, 0.0f};
    float coeff[4] = {sr, sg, sb, 1.0f};
    [ForceUnroll]
    for (int ci = 0; ci < 12; ci++) {
        float val = bg_trilerp(kValueQuant != 0, r, c, ci);
        d[ci / 4] += val * coeff[ci % 4];
    }

    p.out_or_vout[3 * g_off + 0] = d[0];
    p.out_or_vout[3 * g_off + 1] = d[1];
    p.out_or_vout[3 * g_off + 2] = d[2];
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_sample_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform BgSampleParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int g_off = int(idx);
    int tmp = g_off;
    tmp /= p.w; tmp /= p.h; tmp /= p.m;
    int ni = tmp;

    BgReader r = {p.fp32, p.q16, p.vbounds};
    float sr = p.rgb[3 * g_off + 0];
    float sg = p.rgb[3 * g_off + 1];
    float sb = p.rgb[3 * g_off + 2];
    float gx = p.coords[2 * g_off + 0];
    float gy = p.coords[2 * g_off + 1];
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = min(max(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float x = gx * float(p.W - 1);
    float y = gy * float(p.H - 1);
    float z = gz * float(p.L - 1);

    int base = ni * p.L * p.H * p.W * 12;
    BgCells c = bg_cells_clamped(base, p.L, p.H, p.W, 12, x, y, z);

    float w8[8];
    bg_corner_weights(c, w8);

    float dcol[3] = {p.out_or_vout[3 * g_off + 0], p.out_or_vout[3 * g_off + 1],
                     p.out_or_vout[3 * g_off + 2]};
    float coeff[4] = {sr, sg, sb, 1.0f};
    float v[3] = {0.0f, 0.0f, 0.0f};

    [ForceUnroll]
    for (int ci = 0; ci < 12; ci++) {
        int si = ci % 4, di = ci / 4;
        float grad_weight = coeff[si] * dcol[di];
        [ForceUnroll]
        for (int corner = 0; corner < 8; corner++)
            atomic_add_f32(&p.v_bilagrid[c.o[corner] + ci],
                           w8[corner] * grad_weight);
        if (si < 3) {
            float val = bg_trilerp(kValueQuant != 0, r, c, ci);
            v[si] += val * dcol[di];
        }
    }

    float d8[8];
    bg_dwdz(c, d8);
    float gx_grad = 0.0f, gy_grad = 0.0f, gz_grad = 0.0f;
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        float trilerp = 0.0f;
        [ForceUnroll]
        for (int ci = 0; ci < 12; ci++) {
            float gv = bg_read(kValueQuant != 0, r, c.o[corner] + ci);
            trilerp += gv * coeff[ci % 4] * dcol[ci / 4];
        }
        if (kCoordsGrad != 0) {
            float wy = (corner & 2) != 0 ? c.fy : 1.0f - c.fy;
            float wz = (corner & 4) != 0 ? c.fz : 1.0f - c.fz;
            float wx = (corner & 1) != 0 ? c.fx : 1.0f - c.fx;
            float dwdx = ((corner & 1) != 0 ? 1.0f : -1.0f) * wy * wz;
            float dwdy = ((corner & 2) != 0 ? 1.0f : -1.0f) * wx * wz;
            gx_grad += dwdx * float(p.W - 1) * trilerp;
            gy_grad += dwdy * float(p.H - 1) * trilerp;
        }
        gz_grad += d8[corner] * float(p.L - 1) * trilerp;
    }
    if (kCoordsGrad != 0) {
        p.v_coords[2 * g_off + 0] =
            gx_grad * ((float(c.x0) != x && float(c.x1) != x) ? 1.0f : 0.0f);
        p.v_coords[2 * g_off + 1] =
            gy_grad * ((float(c.y0) != y && float(c.y1) != y) ? 1.0f : 0.0f);
    }
    if (!gz_in_range) gz_grad = 0.0f;
    p.v_rgb[3 * g_off + 0] = v[0] + kC2G_r * gz_grad;
    p.v_rgb[3 * g_off + 1] = v[1] + kC2G_g * gz_grad;
    p.v_rgb[3 * g_off + 2] = v[2] + kC2G_b * gz_grad;
}

// ---------------------------------------------------------------------------
// uniform / patched forward
// ---------------------------------------------------------------------------

struct BgUniformParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* rgb;           // [N,(m,)h,w,3]
    float* output;        // fwd out / bwd v_output
    int* offsets;         // [N,m,2] (kPatched)
    int* grid_indices;    // [N] (!kPatched; has_grid_indices)
    int N, L, H, W, m, h, w, h0, w0;
    int has_grid_indices;
    uint total;
    uint wgs_per_row;
};

// Decodes (ni, mi, hi, wi, g_id, g_offset(*3), gx, gy) for the fused
// uniform/patched samplers. `kPatched` selects the layout at specialization
// time; callers pass or_fallback'd (never-null) offsets / grid_indices.
struct BgPix {
    int ni, mi, hi, wi, g_id, g_off3;
    float gx, gy;
};

[ForceInline]
BgPix _bg_pix(int idx, int m, int h, int w, int h0, int w0, int* offsets,
              int* grid_indices, int has_grid_indices) {
    BgPix q;
    int tmp = idx;
    q.wi = tmp % w; tmp /= w;
    q.hi = tmp % h; tmp /= h;
    if (kPatched != 0) {
        q.mi = tmp % m; tmp /= m;
    } else {
        q.mi = 0;
    }
    q.ni = tmp;
    if (kPatched != 0) {
        q.g_id = q.ni;
        int o = (q.ni * m + q.mi) * 2;
        q.gx = float(q.wi + offsets[o + 0]) / float(w0 - 1);
        q.gy = float(q.hi + offsets[o + 1]) / float(h0 - 1);
        q.g_off3 = (((q.ni * m + q.mi) * h + q.hi) * w + q.wi) * 3;
    } else {
        q.g_id = has_grid_indices != 0 ? grid_indices[q.ni] : q.ni;
        q.gx = float(q.wi) / float(w - 1);
        q.gy = float(q.hi) / float(h - 1);
        q.g_off3 = ((q.ni * h + q.hi) * w + q.wi) * 3;
    }
    return q;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_affine_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform BgUniformParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    BgPix q = _bg_pix(int(idx), p.m, p.h, p.w, p.h0, p.w0, p.offsets,
                      p.grid_indices, p.has_grid_indices);
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = p.rgb[q.g_off3 + 0];
    float sg = p.rgb[q.g_off3 + 1];
    float sb = p.rgb[q.g_off3 + 2];

    float gz = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    gz = min(max(gz, 0.0f), 1.0f);
    float x = q.gx * float(p.W - 1);
    float y = q.gy * float(p.H - 1);
    float z = gz * float(p.L - 1);

    int base = q.g_id * p.L * p.H * p.W * 12;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 12, x, y, z);

    float d[3] = {0.0f, 0.0f, 0.0f};
    float coeff[4] = {sr, sg, sb, 1.0f};
    [ForceUnroll]
    for (int ci = 0; ci < 12; ci++) {
        float val = bg_trilerp(kValueQuant != 0, r, c, ci);
        d[ci / 4] += val * coeff[ci % 4];
    }

    p.output[q.g_off3 + 0] = bg_finite(d[0]) ? d[0] : 0.5f;
    p.output[q.g_off3 + 1] = bg_finite(d[1]) ? d[1] : 0.5f;
    p.output[q.g_off3 + 2] = bg_finite(d[2]) ? d[2] : 0.5f;
}

// ---------------------------------------------------------------------------
// backward v1: grid-grad kernel (8x8x1 blocks, exact CUDA geometry)
// ---------------------------------------------------------------------------

struct BgV1GridParams {
    float* rgb;            // [N,(m,)h,w,3]
    float* v_output;
    float* v_bilagrid;     // [N,L,H,W,12] (batch-slot indexed)
    int* offsets;          // kPatched
    int* grid_indices;     // unused by the affine grid kernel (mirrors CUDA)
    int N, L, H, W, m, h, w, h0, w0;
    int mult_x, mult_y, m_batch_stride;
    int has_grid_indices;
    int _pad0;
};

[shader("compute")]
// Flat 64-thread workgroup (X must be a multiple of the pinned subgroup
// size); logical 8x8 decode below matches the CUDA block geometry.
[numthreads(64, 1, 1)]
void bilagrid_affine_bwd_v1_grid(uint3 bid: SV_GroupID, uint ltid: SV_GroupThreadID,
                                 uniform BgV1GridParams p) {
    uint3 t = uint3(ltid % 8u, ltid / 8u, 0u);
    int idx = int(bid.z);
    bool inside;
    int zi, m_batch_i = 0, ni;
    if (kPatched != 0) {
        inside = idx < p.N * p.m_batch_stride * p.L;
        zi = idx % p.L; idx /= p.L;
        m_batch_i = idx % p.m_batch_stride; idx /= p.m_batch_stride;
        ni = idx;
    } else {
        inside = idx < p.N * p.L;
        zi = idx % p.L; idx /= p.L;
        ni = idx;
    }

    int x_idx = int(bid.x) * kBgV1BlockX + int(t.x);
    int y_idx = int(bid.y) * kBgV1BlockY + int(t.y);
    int xi = x_idx / p.mult_x, xf = x_idx % p.mult_x;
    int yi = y_idx / p.mult_y, yf = y_idx % p.mult_y;
    inside = inside && (xi >= 0 && xi < p.W && yi >= 0 && yi < p.H);

    bool slow_path = p.mult_x * p.mult_y == 1 ||
                     (p.mult_x % kBgV1BlockX != 0 ||
                      p.mult_y % kBgV1BlockY != 0);
    if (!inside && slow_path) return;

    float accum[12];
    [ForceUnroll]
    for (int ci = 0; ci < 12; ci++) accum[ci] = 0.0f;

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

                BgCells c = bg_cells_uniform(0, p.L, p.H, p.W, 12, x, y, z);
                if (zi != c.z0 && zi != c.z1) continue;

                float accum_t = bg_v1_accum_t(xi, yi, zi, c);

                float dcol[3] = {p.v_output[g_off + 0], p.v_output[g_off + 1],
                                 p.v_output[g_off + 2]};
                // Linear in the incoming gradient: a zero one -- a masked
                // pixel, mostly -- adds nothing to any channel.
                if (dcol[0] == 0.0f && dcol[1] == 0.0f && dcol[2] == 0.0f)
                    continue;
                float coeff[4] = {sr, sg, sb, 1.0f};
                [ForceUnroll]
                for (int ci = 0; ci < 12; ci++)
                    accum[ci] += accum_t * coeff[ci % 4] * dcol[ci / 4];
            }
        }
    }

    int out_idx_start = (((ni * p.L + zi) * p.H + yi) * p.W + xi) * 12;

    if (slow_path) {
        [ForceUnroll]
        for (int ci = 0; ci < 12; ci++) {
            if (bg_finite(accum[ci]) && accum[ci] != 0.0f)
                atomic_add_f32(&p.v_bilagrid[out_idx_start + ci], accum[ci]);
        }
        return;
    }

    [ForceUnroll]
    for (int ci = 0; ci < 12; ci++)
        bg_block_add_64(&p.v_bilagrid[out_idx_start + ci], accum[ci], ltid);
}

// ---------------------------------------------------------------------------
// backward v1: rgb-grad kernel (flat 256)
// ---------------------------------------------------------------------------

struct BgV1RgbParams {
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
void bilagrid_affine_bwd_v1_rgb(uint3 wg: SV_GroupID,
                                uint tid: SV_GroupThreadID,
                                uniform BgV1RgbParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    BgPix q = _bg_pix(int(idx), p.m, p.h, p.w, p.h0, p.w0, p.offsets,
                      p.grid_indices, p.has_grid_indices);
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = p.rgb[q.g_off3 + 0];
    float sg = p.rgb[q.g_off3 + 1];
    float sb = p.rgb[q.g_off3 + 2];
    float dcol[3] = {p.v_output[q.g_off3 + 0], p.v_output[q.g_off3 + 1],
                     p.v_output[q.g_off3 + 2]};

    // The image gradient is linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- is zero out and skips the corner loads.
    if (dcol[0] == 0.0f && dcol[1] == 0.0f && dcol[2] == 0.0f) {
        p.v_rgb[q.g_off3 + 0] = 0.0f;
        p.v_rgb[q.g_off3 + 1] = 0.0f;
        p.v_rgb[q.g_off3 + 2] = 0.0f;
        return;
    }

    float x = q.gx * float(p.W - 1);
    float y = q.gy * float(p.H - 1);
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = min(max(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * float(p.L - 1);

    int base = q.g_id * p.L * p.H * p.W * 12;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 12, x, y, z);
    float w8[8];
    bg_corner_weights(c, w8);

    float coeff[4] = {sr, sg, sb, 1.0f};
    float v[3] = {0.0f, 0.0f, 0.0f};
    [ForceUnroll]
    for (int si = 0; si < 3; si++) {
        [ForceUnroll]
        for (int di = 0; di < 3; di++) {
            int ci = 4 * di + si;
            float val = 0.0f;
            [ForceUnroll]
            for (int corner = 0; corner < 8; corner++)
                val += bg_read(kValueQuant != 0, r, c.o[corner] + ci) *
                       w8[corner];
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
        for (int ci = 0; ci < 12; ci++) {
            float gv = bg_read(kValueQuant != 0, r, c.o[corner] + ci);
            trilerp += gv * coeff[ci % 4] * dcol[ci / 4];
        }
        gz_grad += d8[corner] * float(p.L - 1) * trilerp;
    }
    if (!gz_in_range) gz_grad = 0.0f;
    float vr = v[0] + kC2G_r * gz_grad;
    float vg = v[1] + kC2G_g * gz_grad;
    float vb = v[2] + kC2G_b * gz_grad;
    p.v_rgb[q.g_off3 + 0] = bg_finite(vr) ? vr : 0.0f;
    p.v_rgb[q.g_off3 + 1] = bg_finite(vg) ? vg : 0.0f;
    p.v_rgb[q.g_off3 + 2] = bg_finite(vb) ? vb : 0.0f;
}

// ---------------------------------------------------------------------------
// backward v2 (flat; per-corner atomic scatter + fused rgb grad)
// ---------------------------------------------------------------------------

struct BgV2Params {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* rgb;
    float* v_output;
    float* v_bilagrid;
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
void bilagrid_affine_bwd_v2(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform BgV2Params p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int tmp = int(idx);
    int wi = tmp % p.w; tmp /= p.w;
    int hi = tmp % p.h; tmp /= p.h;
    int mi = 0;
    if (kPatched != 0) { mi = tmp % p.m; tmp /= p.m; }
    int ni = tmp;
    // Reads use g_id (camera grid slot); writes use ni (per-batch grad buffer).
    int g_id = (kPatched == 0 && p.has_grid_indices != 0) ? p.grid_indices[ni] : ni;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    int g_off;
    float x, y;
    if (kPatched != 0) {
        g_off = (((ni * p.m + mi) * p.h + hi) * p.w + wi) * 3;
        int o = (ni * p.m + mi) * 2;
        x = float(wi + p.offsets[o + 0]) / float(p.w0 - 1) * float(p.W - 1);
        y = float(hi + p.offsets[o + 1]) / float(p.h0 - 1) * float(p.H - 1);
    } else {
        g_off = ((ni * p.h + hi) * p.w + wi) * 3;
        x = float(wi) / float(p.w - 1) * float(p.W - 1);
        y = float(hi) / float(p.h - 1) * float(p.H - 1);
    }

    float sr = p.rgb[g_off + 0];
    float sg = p.rgb[g_off + 1];
    float sb = p.rgb[g_off + 2];
    sr = bg_finite(sr) ? sr : 0.5f;
    sg = bg_finite(sg) ? sg : 0.5f;
    sb = bg_finite(sb) ? sb : 0.5f;

    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = min(max(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * float(p.L - 1);

    int x0 = int(floor(x)), y0 = int(floor(y)), z0 = int(floor(z));
    int x1 = min(x0 + 1, p.W - 1);
    int y1 = min(y0 + 1, p.H - 1);
    int z1 = min(z0 + 1, p.L - 1);
    float fx = x - float(x0), fy = y - float(y0), fz = z - float(z0);

    float dr = p.v_output[g_off + 0];
    float dg = p.v_output[g_off + 1];
    float db = p.v_output[g_off + 2];
    dr = bg_finite(dr) ? dr : 0.0f;
    dg = bg_finite(dg) ? dg : 0.0f;
    db = bg_finite(db) ? db : 0.0f;
    float dcol[3] = {dr, dg, db};
    float coeff[4] = {sr, sg, sb, 1.0f};

    float v[3] = {0.0f, 0.0f, 0.0f};
    float gz_grad = 0.0f;

    for (int corner = 0; corner < 8; corner++) {
        int xi = (corner & 1) != 0 ? x1 : x0;
        int yi = (corner & 2) != 0 ? y1 : y0;
        int zi = (corner & 4) != 0 ? z1 : z0;

        float dfdz = ((corner & 1) != 0 ? fx : (1.0f - fx)) *
                     ((corner & 2) != 0 ? fy : (1.0f - fy)) *
                     ((corner & 4) != 0 ? 1.0f : -1.0f);
        float f = dfdz * ((corner & 4) != 0 ? fz : (fz - 1.0f));

        // READ grid values from the g_id slot; SCATTER grad into the per-batch
        // buffer indexed by ni (g_id == ni for PATCHED).
        int cellidx = ((zi * p.H + yi) * p.W + xi);
        int rbase = (g_id * (p.L * p.H * p.W) + cellidx) * 12;
        int wbase = (ni  * (p.L * p.H * p.W) + cellidx) * 12;

        if (kPatched != 0) {
            // PATCHED CUDA ordering: gz chain accumulated inline per channel.
            [ForceUnroll]
            for (int ci = 0; ci < 12; ci++) {
                int si = ci % 4, di = ci / 4;
                float gv = bg_read(kValueQuant != 0, r, rbase + ci);
                if (si < 3) v[si] += gv * f * dcol[di];
                float grad_weight = coeff[si] * dcol[di];
                gz_grad += dfdz * float(p.L - 1) * gv * grad_weight;
                atomic_add_f32(&p.v_bilagrid[wbase + ci], f * grad_weight);
            }
        } else {
            float trilerp = 0.0f;
            [ForceUnroll]
            for (int ci = 0; ci < 12; ci++) {
                int si = ci % 4, di = ci / 4;
                float gv = bg_read(kValueQuant != 0, r, rbase + ci);
                if (si < 3) v[si] += gv * f * dcol[di];
                float grad_weight = coeff[si] * dcol[di];
                trilerp += gv * grad_weight;
                atomic_add_f32(&p.v_bilagrid[wbase + ci], f * grad_weight);
            }
            gz_grad += dfdz * float(p.L - 1) * trilerp;
        }
    }

    if (!gz_in_range) gz_grad = 0.0f;
    p.v_rgb[g_off + 0] = v[0] + kC2G_r * gz_grad;
    p.v_rgb[g_off + 1] = v[1] + kC2G_g * gz_grad;
    p.v_rgb[g_off + 2] = v[2] + kC2G_b * gz_grad;
}
