// PPISP (9-channel) bilateral-grid samplers (Vulkan backend), mirroring the
// kernels/bilagrid/BilagridPpisp*_kernel.cuh ones (uniform + PATCHED + PACKED).
//
// Per-cell PPISP math comes from the canonical shaders/ppisp.slang
// (apply_exposure + apply_color_correction_ppisp); the backward uses Slang
// autodiff of the composite, which matches the hand-written CUDA chain in
// BilagridPpispMathBwd.cuh analytically (it recomputes the same forward and
// backprops every term) — rounding differs, parity is tolerance-based.
//
// Spec constants (IDs by declaration order — dispatches must pass ALL):
//   0: kValueQuant  1: kPatched  2: kCoordsGrad  3: kPacked  4: kRgbFmt  5: kOutFmt

#include "int64_compat.slang"
#include "bilagrid_common.slang"
#include "rgb_format.slang"

#define CudaDeviceExport ForceInline
#include "shaders/ppisp.slang"
#undef CudaDeviceExport

[SpecializationConstant]
const int kValueQuant = 0;
[SpecializationConstant]
const int kPatched = 0;
[SpecializationConstant]
const int kCoordsGrad = 0;
[SpecializationConstant]
const int kPacked = 0;
// The image formats (core/PixelFormat.h), fixed per pipeline: read per load
// from the params, they cost the v1 grid kernel ~15%.
[SpecializationConstant]
const int kRgbFmt = 0;
[SpecializationConstant]
const int kOutFmt = 0;

// exposure(ci=0) + color params (ci=1..8: b.x b.y r.x r.y g.x g.y n.x n.y)
[ForceInline]
void _bp_set_param(inout float e, inout ColorPPISPParams cp, int ci,
                   float val) {
    if (ci == 0) e = val;
    else if (ci == 1) cp.b.x = val;
    else if (ci == 2) cp.b.y = val;
    else if (ci == 3) cp.r.x = val;
    else if (ci == 4) cp.r.y = val;
    else if (ci == 5) cp.g.x = val;
    else if (ci == 6) cp.g.y = val;
    else if (ci == 7) cp.n.x = val;
    else cp.n.y = val;
}

[ForceInline]
float _bp_get_param(float e, ColorPPISPParams cp, int ci) {
    if (ci == 0) return e;
    else if (ci == 1) return cp.b.x;
    else if (ci == 2) return cp.b.y;
    else if (ci == 3) return cp.r.x;
    else if (ci == 4) return cp.r.y;
    else if (ci == 5) return cp.g.x;
    else if (ci == 6) return cp.g.y;
    else if (ci == 7) return cp.n.x;
    return cp.n.y;
}

[Differentiable]
[ForceInline]
float3 _bp_transform(float3 rgb, float e, ColorPPISPParams cp) {
    return apply_color_correction_ppisp(apply_exposure(rgb, e), cp);
}

[ForceInline]
void _bp_transform_bwd(float3 rgb, float e, ColorPPISPParams cp, float3 v_out,
                       out float3 v_rgb, out float v_e,
                       out ColorPPISPParams v_cp) {
    var dp_rgb = diffPair(rgb);
    var dp_e = diffPair(e);
    var dp_cp = diffPair(cp);
    bwd_diff(_bp_transform)(dp_rgb, dp_e, dp_cp, v_out);
    v_rgb = dp_rgb.d;
    v_e = dp_e.d;
    v_cp.b = dp_cp.d.b;
    v_cp.r = dp_cp.d.r;
    v_cp.g = dp_cp.d.g;
    v_cp.n = dp_cp.d.n;
}

// ---------------------------------------------------------------------------
// per-pixel-coords sample forward / backward (+ PACKED)
// ---------------------------------------------------------------------------

struct BpSampleParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    i64mem_t* image_indices;  // kPacked: [nnz]
    float* coords;           // [.,2]
    float* rgb;              // [.,3]
    float* out_or_vout;
    float* v_bilagrid;
    float* v_coords;
    float* v_rgb;
    int N, L, H, W, m, h, w;
    uint total;              // N*m*h*w or nnz
    uint wgs_per_row;
    uint _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_ppisp_sample_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                               uniform BpSampleParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int g_off = int(idx);
    int ni;
    if (kPacked != 0) {
        ni = int(i64mem_get(p.image_indices[idx]));
    } else {
        int tmp = g_off;
        tmp /= p.w; tmp /= p.h; tmp /= p.m;
        ni = tmp;
    }
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = p.rgb[3 * g_off + 0];
    float sg = p.rgb[3 * g_off + 1];
    float sb = p.rgb[3 * g_off + 2];
    float gx = p.coords[2 * g_off + 0];
    float gy = p.coords[2 * g_off + 1];
    float gz = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    gz = min(max(gz, 0.0f), 1.0f);
    float x = gx * float(p.W - 1);
    float y = gy * float(p.H - 1);
    float z = gz * float(p.L - 1);

    int base = ni * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_clamped(base, p.L, p.H, p.W, 9, x, y, z);

    float exposure = 0.0f;
    ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++)
        _bp_set_param(exposure, cp, ci, bg_trilerp(kValueQuant != 0, r, c, ci));

    float3 rgb = _bp_transform(float3(sr, sg, sb), exposure, cp);
    p.out_or_vout[3 * g_off + 0] = bg_finite(rgb.x) ? rgb.x : 0.5f;
    p.out_or_vout[3 * g_off + 1] = bg_finite(rgb.y) ? rgb.y : 0.5f;
    p.out_or_vout[3 * g_off + 2] = bg_finite(rgb.z) ? rgb.z : 0.5f;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_ppisp_sample_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                               uniform BpSampleParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int g_off = int(idx);
    int ni;
    if (kPacked != 0) {
        ni = int(i64mem_get(p.image_indices[idx]));
    } else {
        int tmp = g_off;
        tmp /= p.w; tmp /= p.h; tmp /= p.m;
        ni = tmp;
    }
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

    int base = ni * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_clamped(base, p.L, p.H, p.W, 9, x, y, z);
    float w8[8];
    bg_corner_weights(c, w8);

    float exposure = 0.0f;
    ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++)
        _bp_set_param(exposure, cp, ci, bg_trilerp(kValueQuant != 0, r, c, ci));

    float3 v_out = float3(p.out_or_vout[3 * g_off + 0],
                          p.out_or_vout[3 * g_off + 1],
                          p.out_or_vout[3 * g_off + 2]);
    float3 v_rgb;
    float v_e;
    ColorPPISPParams v_cp;
    _bp_transform_bwd(float3(sr, sg, sb), exposure, cp, v_out, v_rgb, v_e,
                      v_cp);

    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++) {
        float grad_weight = _bp_get_param(v_e, v_cp, ci);
        [ForceUnroll]
        for (int corner = 0; corner < 8; corner++)
            atomic_add_f32(&p.v_bilagrid[c.o[corner] + ci],
                           w8[corner] * grad_weight);
    }

    float d8[8];
    bg_dwdz(c, d8);
    float gx_grad = 0.0f, gy_grad = 0.0f, gz_grad = 0.0f;
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        float trilerp = 0.0f;
        [ForceUnroll]
        for (int ci = 0; ci < 9; ci++)
            trilerp += bg_read(kValueQuant != 0, r, c.o[corner] + ci) *
                       _bp_get_param(v_e, v_cp, ci);
        if (kCoordsGrad != 0) {
            float wx = (corner & 1) != 0 ? c.fx : 1.0f - c.fx;
            float wy = (corner & 2) != 0 ? c.fy : 1.0f - c.fy;
            float wz = (corner & 4) != 0 ? c.fz : 1.0f - c.fz;
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
    p.v_rgb[3 * g_off + 0] = v_rgb.x + kC2G_r * gz_grad;
    p.v_rgb[3 * g_off + 1] = v_rgb.y + kC2G_g * gz_grad;
    p.v_rgb[3 * g_off + 2] = v_rgb.z + kC2G_b * gz_grad;
}

// ---------------------------------------------------------------------------
// uniform / patched forward
// ---------------------------------------------------------------------------

struct BpUniformParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    uint* rgb;
    uint* output;
    int* offsets;
    int* grid_indices;
    int N, L, H, W, m, h, w, h0, w0;
    int has_grid_indices;
    uint total;
    uint wgs_per_row;
};

[ForceInline]
void _bp_pix(int idx, BpUniformParams p, out int ni, out int g_id,
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
void bilagrid_ppisp_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform BpUniformParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    // No early return: rgb_store3's float16 pairs need every lane.
    bool active = idx < p.total;
    int ni, g_id, g_off3;
    float gx, gy;
    _bp_pix(int(active ? idx : 0u), p, ni, g_id, g_off3, gx, gy);
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = rgb_load1(p.rgb, g_off3 + 0, uint(kRgbFmt));
    float sg = rgb_load1(p.rgb, g_off3 + 1, uint(kRgbFmt));
    float sb = rgb_load1(p.rgb, g_off3 + 2, uint(kRgbFmt));
    float gz = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    gz = min(max(gz, 0.0f), 1.0f);
    float x = gx * float(p.W - 1);
    float y = gy * float(p.H - 1);
    float z = gz * float(p.L - 1);

    int base = g_id * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 9, x, y, z);

    float exposure = 0.0f;
    ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++)
        _bp_set_param(exposure, cp, ci, bg_trilerp(kValueQuant != 0, r, c, ci));

    float3 rgb = _bp_transform(float3(sr, sg, sb), exposure, cp);
    float3 out_rgb = float3(bg_finite(rgb.x) ? rgb.x : 0.5f,
                            bg_finite(rgb.y) ? rgb.y : 0.5f,
                            bg_finite(rgb.z) ? rgb.z : 0.5f);
    rgb_store3(p.output, idx, out_rgb, uint(kOutFmt), active);
}

// ---------------------------------------------------------------------------
// backward v1: grid-grad kernel (8x8x1, shmem corner preload)
// ---------------------------------------------------------------------------

static const int kBpFootX = kBgV1BlockX + 2;  // 10
static const int kBpFootY = kBgV1BlockY + 2;  // 10
static const int kBpFootZ = 4;
static const int kBpChans = 9;
static const int kBpCells = kBpFootX * kBpFootY * kBpFootZ;

groupshared float _bp_shmem[kBpFootZ * kBpFootY * kBpFootX * kBpChans];

[ForceInline]
int _bp_sh_idx(int lz, int ly, int lx, int ci) {
    return ((lz * kBpFootY + ly) * kBpFootX + lx) * kBpChans + ci;
}

struct BpV1GridParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    uint* rgb;
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
void bilagrid_ppisp_bwd_v1_grid(uint3 bid: SV_GroupID, uint ltid: SV_GroupThreadID,
                                uniform BpV1GridParams p) {
    uint3 t = uint3(ltid % 8u, ltid / 8u, 0u);
    // Block-uniform decode BEFORE any per-thread early return (the preload
    // barrier below must be reached by all 64 threads).
    // Two-plane dispatch must match backend/vulkan/kernels/Bilagrid.cpp.
    const int layers = (p.L + 1) / 2;
    int block_z_total =
        kPatched != 0 ? p.N * p.m_batch_stride * layers : p.N * layers;
    if (int(bid.z) >= block_z_total) return;

    int idx_block = int(bid.z);
    int zi = 2 * (idx_block % layers); idx_block /= layers;
    int m_batch_i = 0;
    if (kPatched != 0) {
        m_batch_i = idx_block % p.m_batch_stride;
        idx_block /= p.m_batch_stride;
    }
    int ni = idx_block;
    int g_id = kPatched != 0
                   ? ni
                   : (p.has_grid_indices != 0 ? p.grid_indices[ni] : ni);
    int corner_base = g_id * p.L * p.H * p.W * kBpChans;

    // Cooperative preload of the block's corner footprint.
    int base_xi = (int(bid.x) * kBgV1BlockX) / p.mult_x - 1;
    int base_yi = (int(bid.y) * kBgV1BlockY) / p.mult_y - 1;
    int base_zi = zi - 1;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    for (int local = int(ltid); local < kBpCells; local += 64) {
        int lx = local % kBpFootX;
        int ly = (local / kBpFootX) % kBpFootY;
        int lz = local / (kBpFootX * kBpFootY);
        int gxc = min(max(base_xi + lx, 0), p.W - 1);
        int gyc = min(max(base_yi + ly, 0), p.H - 1);
        int gzc = min(max(base_zi + lz, 0), p.L - 1);
        int g_off = corner_base + ((gzc * p.H + gyc) * p.W + gxc) * kBpChans;
        for (int ci = 0; ci < kBpChans; ci++)
            _bp_shmem[_bp_sh_idx(lz, ly, lx, ci)] =
                bg_read(kValueQuant != 0, r, g_off + ci);
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

    float accum[2][kBpChans];
    [ForceUnroll]
    for (int dz = 0; dz < 2; dz++)
    [ForceUnroll]
    for (int ci = 0; ci < kBpChans; ci++) accum[dz][ci] = 0.0f;

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
                float sr = rgb_load1(p.rgb, g_off + 0, uint(kRgbFmt));
                float sg = rgb_load1(p.rgb, g_off + 1, uint(kRgbFmt));
                float sb = rgb_load1(p.rgb, g_off + 2, uint(kRgbFmt));
                float z = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
                z = min(max(z, 0.0f), 1.0f) * float(p.L - 1);

                BgCells c = bg_cells_uniform(0, p.L, p.H, p.W, kBpChans, x, y,
                                             z);
                if (zi != c.z0 && zi != c.z1 && zi + 1 != c.z0 && zi + 1 != c.z1) continue;

                // Every grid gradient this pixel gives is linear in its
                // incoming one, so a zero one -- a masked pixel, mostly --
                // gives nothing and skips the interpolation below.
                float3 v_out = float3(p.v_output[g_off + 0],
                                      p.v_output[g_off + 1],
                                      p.v_output[g_off + 2]);
                if (all(v_out == float3(0.0f))) continue;

                // Defensive shmem-index clamps (fast-math integer-boundary
                // rationale — see the CUDA kernel).
                int sx0 = max(c.x0 - base_xi, 0);
                int sx1 = min(c.x1 - base_xi, kBpFootX - 1);
                int sy0 = max(c.y0 - base_yi, 0);
                int sy1 = min(c.y1 - base_yi, kBpFootY - 1);
                int sz0 = max(c.z0 - base_zi, 0);
                int sz1 = min(c.z1 - base_zi, kBpFootZ - 1);

                float exposure = 0.0f;
                ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
                [ForceUnroll]
                for (int ci = 0; ci < kBpChans; ci++) {
                    float v000 = _bp_shmem[_bp_sh_idx(sz0, sy0, sx0, ci)];
                    float v001 = _bp_shmem[_bp_sh_idx(sz0, sy0, sx1, ci)];
                    float v010 = _bp_shmem[_bp_sh_idx(sz0, sy1, sx0, ci)];
                    float v011 = _bp_shmem[_bp_sh_idx(sz0, sy1, sx1, ci)];
                    float v100 = _bp_shmem[_bp_sh_idx(sz1, sy0, sx0, ci)];
                    float v101 = _bp_shmem[_bp_sh_idx(sz1, sy0, sx1, ci)];
                    float v110 = _bp_shmem[_bp_sh_idx(sz1, sy1, sx0, ci)];
                    float v111 = _bp_shmem[_bp_sh_idx(sz1, sy1, sx1, ci)];
                    float c00 = v000 * (1.0f - c.fx) + v001 * c.fx;
                    float c01 = v010 * (1.0f - c.fx) + v011 * c.fx;
                    float c10 = v100 * (1.0f - c.fx) + v101 * c.fx;
                    float c11 = v110 * (1.0f - c.fx) + v111 * c.fx;
                    float c0 = c00 * (1.0f - c.fy) + c01 * c.fy;
                    float c1 = c10 * (1.0f - c.fy) + c11 * c.fy;
                    _bp_set_param(exposure, cp, ci,
                                  c0 * (1.0f - c.fz) + c1 * c.fz);
                }

                float3 v_rgb;
                float v_e;
                ColorPPISPParams v_cp;
                _bp_transform_bwd(float3(sr, sg, sb), exposure, cp, v_out,
                                  v_rgb, v_e, v_cp);

                [ForceUnroll]
                for (int dz = 0; dz < 2; dz++) {
                    if (zi + dz >= p.L || (zi + dz != c.z0 && zi + dz != c.z1)) continue;
                    float accum_t = bg_v1_accum_t(xi, yi, zi + dz, c);
                    [ForceUnroll]
                    for (int ci = 0; ci < kBpChans; ci++)
                        accum[dz][ci] += accum_t * _bp_get_param(v_e, v_cp, ci);
                }
            }
        }
    }

    [ForceUnroll]
    for (int dz = 0; dz < 2; dz++) {
        if (zi + dz >= p.L) continue;
        int out_idx_start = (((ni * p.L + zi + dz) * p.H + yi) * p.W + xi) * kBpChans;
        [ForceUnroll]
        for (int ci = 0; ci < kBpChans; ci++) {
            if (slow_path) {
                if (bg_finite(accum[dz][ci]) && accum[dz][ci] != 0.0f)
                    atomic_add_f32(&p.v_bilagrid[out_idx_start + ci], accum[dz][ci]);
            } else {
                bg_block_add_64(&p.v_bilagrid[out_idx_start + ci], accum[dz][ci], ltid);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// backward v1: rgb-grad kernel (flat 256)
// ---------------------------------------------------------------------------

struct BpV1RgbParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    uint* rgb;
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
void bilagrid_ppisp_bwd_v1_rgb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                               uniform BpV1RgbParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;

    BpUniformParams u;
    u.fp32 = p.fp32; u.q16 = p.q16; u.vbounds = p.vbounds;
    u.rgb = p.rgb;
    u.offsets = p.offsets; u.grid_indices = p.grid_indices;
    u.N = p.N; u.L = p.L; u.H = p.H; u.W = p.W;
    u.m = p.m; u.h = p.h; u.w = p.w; u.h0 = p.h0; u.w0 = p.w0;
    u.has_grid_indices = p.has_grid_indices;
    u.total = p.total; u.wgs_per_row = p.wgs_per_row;
    int ni, g_id, g_off3;
    float gx, gy;
    _bp_pix(int(idx), u, ni, g_id, g_off3, gx, gy);
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = rgb_load1(p.rgb, g_off3 + 0, uint(kRgbFmt));
    float sg = rgb_load1(p.rgb, g_off3 + 1, uint(kRgbFmt));
    float sb = rgb_load1(p.rgb, g_off3 + 2, uint(kRgbFmt));
    float3 v_out = float3(p.v_output[g_off3 + 0], p.v_output[g_off3 + 1],
                          p.v_output[g_off3 + 2]);

    // The image gradient is linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- is zero out and skips the corner loads.
    if (all(v_out == float3(0.0f))) {
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

    float exposure = 0.0f;
    ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
    float corners[8][9];
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        [ForceUnroll]
        for (int ci = 0; ci < 9; ci++)
            corners[corner][ci] = bg_read(kValueQuant != 0, r, c.o[corner] + ci);
    }
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++) {
        float c00 = corners[0][ci] * (1.0f - c.fx) + corners[1][ci] * c.fx;
        float c01 = corners[2][ci] * (1.0f - c.fx) + corners[3][ci] * c.fx;
        float c10 = corners[4][ci] * (1.0f - c.fx) + corners[5][ci] * c.fx;
        float c11 = corners[6][ci] * (1.0f - c.fx) + corners[7][ci] * c.fx;
        float c0 = c00 * (1.0f - c.fy) + c01 * c.fy;
        float c1 = c10 * (1.0f - c.fy) + c11 * c.fy;
        _bp_set_param(exposure, cp, ci, c0 * (1.0f - c.fz) + c1 * c.fz);
    }

    float3 v_rgb;
    float v_e;
    ColorPPISPParams v_cp;
    _bp_transform_bwd(float3(sr, sg, sb), exposure, cp, v_out, v_rgb, v_e,
                      v_cp);

    float d8[8];
    bg_dwdz(c, d8);
    float gz_grad = 0.0f;
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        float trilerp = 0.0f;
        [ForceUnroll]
        for (int ci = 0; ci < 9; ci++)
            trilerp += corners[corner][ci] *
                       _bp_get_param(v_e, v_cp, ci);
        gz_grad += d8[corner] * float(p.L - 1) * trilerp;
    }
    if (!gz_in_range) gz_grad = 0.0f;
    float vr = v_rgb.x + kC2G_r * gz_grad;
    float vg = v_rgb.y + kC2G_g * gz_grad;
    float vb = v_rgb.z + kC2G_b * gz_grad;
    p.v_rgb[g_off3 + 0] = bg_finite(vr) ? vr : 0.0f;
    p.v_rgb[g_off3 + 1] = bg_finite(vg) ? vg : 0.0f;
    p.v_rgb[g_off3 + 2] = bg_finite(vb) ? vb : 0.0f;
}

// ---------------------------------------------------------------------------
// backward, tile-reduced: v1's per-pixel work once per pixel, then the tile's
// grid gradient summed by one thread a (cell, level) row over a 32-pixel
// chunk, so the cost does not depend on how many levels the tile spans.
// ---------------------------------------------------------------------------

static const int kBptTile = 16;
static const int kBptRows = 32;     // rows a pass; 256 / 32 chunks of 32 pixels

groupshared uint _bpt_xy[256];      // kx0 | kx1 << 8 | ky0 << 16 | ky1 << 24, tile-relative
groupshared uint _bpt_z[256];       // z0 | z1 << 16
groupshared float _bpt_f[256 * 12]; // fx, fy, fz, then the 9 parameter gradients
groupshared float _bpt_part[256 * kBpChans];

struct BpTileParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    uint* rgb;
    float* v_output;
    float* v_bilagrid;
    float* v_rgb;
    int* grid_indices;
    int N, L, H, W, h, w;
    int has_grid_indices;
    int _pad0;
};

[ForceInline]
float _bpt_axis_w(int k, uint k0, uint k1, float f) {
    return (uint(k) == k0 ? 1.0f - f : 0.0f) + (uint(k) == k1 ? f : 0.0f);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_ppisp_bwd_tile(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform BpTileParams p) {
    const int ni = int(wg.z);
    const int tx0 = int(wg.x) * kBptTile, ty0 = int(wg.y) * kBptTile;
    const int wi = tx0 + int(tid) % kBptTile, hi = ty0 + int(tid) / kBptTile;
    // The per-pixel cell expression is monotonic in the pixel, so the tile's
    // cells run from its first pixel's to one past its last pixel's.
    const int bx = int(floor(float(tx0) / float(p.w - 1) * float(p.W - 1)));
    const int by = int(floor(float(ty0) / float(p.h - 1) * float(p.H - 1)));
    const int ex = int(floor(float(min(tx0 + kBptTile - 1, p.w - 1)) / float(p.w - 1) * float(p.W - 1)));
    const int ey = int(floor(float(min(ty0 + kBptTile - 1, p.h - 1)) / float(p.h - 1) * float(p.H - 1)));
    const int npx = min(ex - bx + 2, p.W - bx), npy = min(ey - by + 2, p.H - by);

    const bool inside = wi < p.w && hi < p.h;
    const int g_id = p.has_grid_indices != 0 ? p.grid_indices[ni] : ni;
    const int g_off3 = inside ? ((ni * p.h + hi) * p.w + wi) * 3 : 0;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float gw[kBpChans];
    [ForceUnroll]
    for (int ci = 0; ci < kBpChans; ci++) gw[ci] = 0.0f;
    uint code_xy = 0u, code_z = 0u;
    float3 frac = float3(0.0f);
    if (inside) {
        float sr = rgb_load1(p.rgb, g_off3 + 0, uint(kRgbFmt));
        float sg = rgb_load1(p.rgb, g_off3 + 1, uint(kRgbFmt));
        float sb = rgb_load1(p.rgb, g_off3 + 2, uint(kRgbFmt));
        float3 v_out = float3(p.v_output[g_off3 + 0], p.v_output[g_off3 + 1],
                              p.v_output[g_off3 + 2]);
        float vr = 0.0f, vg = 0.0f, vb = 0.0f;
        if (any(v_out != float3(0.0f))) {
            float x = float(wi) / float(p.w - 1) * float(p.W - 1);
            float y = float(hi) / float(p.h - 1) * float(p.H - 1);
            float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
            float gz = min(max(gz_raw, 0.0f), 1.0f);
            bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
            float z = gz * float(p.L - 1);
            int base = g_id * p.L * p.H * p.W * 9;
            BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 9, x, y, z);

            float exposure = 0.0f;
            ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
            float corners[8][9];
            [ForceUnroll]
            for (int corner = 0; corner < 8; corner++) {
                [ForceUnroll]
                for (int ci = 0; ci < 9; ci++)
                    corners[corner][ci] = bg_read(kValueQuant != 0, r, c.o[corner] + ci);
            }
            [ForceUnroll]
            for (int ci = 0; ci < 9; ci++) {
                float c00 = corners[0][ci] * (1.0f - c.fx) + corners[1][ci] * c.fx;
                float c01 = corners[2][ci] * (1.0f - c.fx) + corners[3][ci] * c.fx;
                float c10 = corners[4][ci] * (1.0f - c.fx) + corners[5][ci] * c.fx;
                float c11 = corners[6][ci] * (1.0f - c.fx) + corners[7][ci] * c.fx;
                float c0 = c00 * (1.0f - c.fy) + c01 * c.fy;
                float c1 = c10 * (1.0f - c.fy) + c11 * c.fy;
                _bp_set_param(exposure, cp, ci, c0 * (1.0f - c.fz) + c1 * c.fz);
            }

            float3 v_rgb;
            float v_e;
            ColorPPISPParams v_cp;
            _bp_transform_bwd(float3(sr, sg, sb), exposure, cp, v_out, v_rgb, v_e,
                              v_cp);

            float d8[8];
            bg_dwdz(c, d8);
            float gz_grad = 0.0f;
            [ForceUnroll]
            for (int corner = 0; corner < 8; corner++) {
                float trilerp = 0.0f;
                [ForceUnroll]
                for (int ci = 0; ci < 9; ci++)
                    trilerp += corners[corner][ci] * _bp_get_param(v_e, v_cp, ci);
                gz_grad += d8[corner] * float(p.L - 1) * trilerp;
            }
            if (!gz_in_range) gz_grad = 0.0f;
            vr = v_rgb.x + kC2G_r * gz_grad;
            vg = v_rgb.y + kC2G_g * gz_grad;
            vb = v_rgb.z + kC2G_b * gz_grad;
            [ForceUnroll]
            for (int ci = 0; ci < kBpChans; ci++) {
                float g = _bp_get_param(v_e, v_cp, ci);
                gw[ci] = bg_finite(g) ? g : 0.0f;
            }
            code_xy = uint(c.x0 - bx) | uint(c.x1 - bx) << 8 | uint(c.y0 - by) << 16 |
                      uint(c.y1 - by) << 24;
            code_z = uint(c.z0) | uint(c.z1) << 16;
            frac = float3(c.fx, c.fy, c.fz);
        }
        // In place: v_output and v_rgb may be the same buffer, and only this
        // thread reads this pixel.
        p.v_rgb[g_off3 + 0] = bg_finite(vr) ? vr : 0.0f;
        p.v_rgb[g_off3 + 1] = bg_finite(vg) ? vg : 0.0f;
        p.v_rgb[g_off3 + 2] = bg_finite(vb) ? vb : 0.0f;
    }
    _bpt_xy[tid] = code_xy;
    _bpt_z[tid] = code_z;
    _bpt_f[tid * 12 + 0] = frac.x;
    _bpt_f[tid * 12 + 1] = frac.y;
    _bpt_f[tid * 12 + 2] = frac.z;
    [ForceUnroll]
    for (int ci = 0; ci < kBpChans; ci++) _bpt_f[tid * 12 + 3 + uint(ci)] = gw[ci];

    // Row = (cell column, cell row, level); a pixel with no gradient adds 0.
    const int rows = npx * npy * p.L;
    const int lane = int(tid) % kBptRows, chunk = int(tid) / kBptRows;
    const int out_base = ni * p.L * p.H * p.W * kBpChans;
    for (int r0 = 0; r0 < rows; r0 += kBptRows) {
        GroupMemoryBarrierWithGroupSync();
        const int row = r0 + lane;
        const int l = row % p.L, pos = row / p.L;
        const int kx = pos % npx, ky = pos / npx;
        float acc[kBpChans];
        [ForceUnroll]
        for (int ci = 0; ci < kBpChans; ci++) acc[ci] = 0.0f;
        if (row < rows) {
            for (int i = 0; i < kBptRows; i++) {
                const uint q = uint(chunk * kBptRows + i);
                const uint xy = _bpt_xy[q], zz = _bpt_z[q];
                float wgt = _bpt_axis_w(kx, xy & 255u, (xy >> 8) & 255u, _bpt_f[q * 12 + 0]) *
                            _bpt_axis_w(ky, (xy >> 16) & 255u, xy >> 24, _bpt_f[q * 12 + 1]) *
                            _bpt_axis_w(l, zz & 0xffffu, zz >> 16, _bpt_f[q * 12 + 2]);
                [ForceUnroll]
                for (int ci = 0; ci < kBpChans; ci++) acc[ci] += wgt * _bpt_f[q * 12 + 3 + uint(ci)];
            }
        }
        [ForceUnroll]
        for (int ci = 0; ci < kBpChans; ci++) _bpt_part[tid * uint(kBpChans) + uint(ci)] = acc[ci];
        GroupMemoryBarrierWithGroupSync();
        for (int b = int(tid); b < kBptRows * kBpChans; b += 256) {
            const int rr = b / kBpChans, ci = b % kBpChans;
            const int orow = r0 + rr;
            if (orow >= rows) continue;
            float tot = 0.0f;
            for (int ch = 0; ch < 256 / kBptRows; ch++)
                tot += _bpt_part[(ch * kBptRows + rr) * kBpChans + ci];
            if (tot == 0.0f || !bg_finite(tot)) continue;
            const int ol = orow % p.L, opos = orow / p.L;
            const int xi = bx + opos % npx, yi = by + opos / npx;
            atomic_add_f32(&p.v_bilagrid[out_base + ((ol * p.H + yi) * p.W + xi) * kBpChans + ci], tot);
        }
    }
}

// ---------------------------------------------------------------------------
// backward v2: fused SCATTER (thread per pixel) -- alternative arm to the
// v1 grid+rgb split. Reuses the v1 `_rgb` per-pixel transform-backward and
// additionally scatters w_corner * grad_weight to the 8 corners (atomicAdd),
// so it computes grid-grad and image-grad in one pass. Image-grad is always
// on for RGB PPISP (mirrors the CUDA v2 <NEEDS_IMAGE_GRAD=true>). Uniform
// only (the engine path); supports grid_indices.
// ---------------------------------------------------------------------------

struct BpV2Params {
    float* fp32;
    uint* q16;
    float2* vbounds;
    uint* rgb;
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
void bilagrid_ppisp_bwd_v2(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                           uniform BpV2Params p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    int tmp = int(idx);
    int wi = tmp % p.w; tmp /= p.w;
    int hi = tmp % p.h; tmp /= p.h;
    int ni = tmp;
    int g_id = p.has_grid_indices != 0 ? p.grid_indices[ni] : ni;
    int g_off3 = ((ni * p.h + hi) * p.w + wi) * 3;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float sr = rgb_load1(p.rgb, g_off3 + 0, uint(kRgbFmt));
    float sg = rgb_load1(p.rgb, g_off3 + 1, uint(kRgbFmt));
    float sb = rgb_load1(p.rgb, g_off3 + 2, uint(kRgbFmt));
    float3 v_out = float3(p.v_output[g_off3 + 0], p.v_output[g_off3 + 1],
                          p.v_output[g_off3 + 2]);

    // The image gradient is linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- is zero out and skips the corner loads.
    if (all(v_out == float3(0.0f))) {
        p.v_rgb[g_off3 + 0] = 0.0f;
        p.v_rgb[g_off3 + 1] = 0.0f;
        p.v_rgb[g_off3 + 2] = 0.0f;
        return;
    }

    float x = float(wi) / float(p.w - 1) * float(p.W - 1);
    float y = float(hi) / float(p.h - 1) * float(p.H - 1);
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = min(max(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * float(p.L - 1);

    // Grid VALUES are read from the g_id slot (`base`); the grid GRADIENT is
    // scattered into the per-batch buffer indexed by ni (`out_base`) -- matches
    // v1's split (reads g_id, writes ni). With grid_indices these differ and
    // the grad buffer is only [C_batch, ...], so scattering to g_id is OOB.
    int base = g_id * p.L * p.H * p.W * 9;
    int out_base = ni * p.L * p.H * p.W * 9;
    BgCells c = bg_cells_uniform(base, p.L, p.H, p.W, 9, x, y, z);
    float w8[8];
    bg_corner_weights(c, w8);

    float exposure = 0.0f;
    ColorPPISPParams cp = {float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)};
    [ForceUnroll]
    for (int ci = 0; ci < 9; ci++)
        _bp_set_param(exposure, cp, ci, bg_trilerp(kValueQuant != 0, r, c, ci));

    float3 v_rgb;
    float v_e;
    ColorPPISPParams v_cp;
    _bp_transform_bwd(float3(sr, sg, sb), exposure, cp, v_out, v_rgb, v_e, v_cp);

    // scatter param grad to the 8 corners + accumulate the gz (color->z) chain.
    float d8[8];
    bg_dwdz(c, d8);
    float gz_grad = 0.0f;
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        int wbase = c.o[corner] - base + out_base;  // per-batch scatter target
        float trilerp = 0.0f;
        [ForceUnroll]
        for (int ci = 0; ci < 9; ci++) {
            float gw = _bp_get_param(v_e, v_cp, ci);
            trilerp += bg_read(kValueQuant != 0, r, c.o[corner] + ci) * gw;
            float aa = w8[corner] * gw;
            if (aa != 0.0f && bg_finite(aa))
                atomic_add_f32(&p.v_bilagrid[wbase + ci], aa);
        }
        gz_grad += d8[corner] * float(p.L - 1) * trilerp;
    }

    if (!gz_in_range) gz_grad = 0.0f;
    float vr = v_rgb.x + kC2G_r * gz_grad;
    float vg = v_rgb.y + kC2G_g * gz_grad;
    float vb = v_rgb.z + kC2G_b * gz_grad;
    p.v_rgb[g_off3 + 0] = bg_finite(vr) ? vr : 0.0f;
    p.v_rgb[g_off3 + 1] = bg_finite(vg) ? vg : 0.0f;
    p.v_rgb[g_off3 + 2] = bg_finite(vb) ? vb : 0.0f;
}
