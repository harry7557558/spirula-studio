// Shared helpers for the bilateral-grid sampler / TV-loss / fused-optimizer
// kernels (Vulkan backend), mirroring kernels/bilagrid/BilagridReader.cuh and
// kernels/bilagrid/BilagridConfig.cuh. Included by the bilagrid_*.slang entry modules.
//
// Value storage: every kernel that reads the grid takes THREE pointer fields
// (fp32, q16, vbounds) plus a kValueQuant spec constant in its own module.
// fp32 mode reads the float buffer; q16 mode decodes 16-bit linear codes
// against per-256-cell float2 bounds (BilagridReader semantics). Launchers
// route unused pointers through vkk::or_fallback — never null (llvmpipe
// speculation rule), and the mode branch is a spec constant so the dead
// path folds away entirely.

#include "atomic_float.slang"

// RGB -> gray constants (BilagridConfig.cuh).
static const float kC2G_r = 0.299f;
static const float kC2G_g = 0.587f;
static const float kC2G_b = 0.114f;

// Block geometry baked into every *_sample_backward_v1 grid-grad kernel and
// its host launcher (kBilagridBwdV1Block* in BilagridConfig.cuh).
static const int kBgV1BlockX = 8;
static const int kBgV1BlockY = 8;
static const int kBgV1RgbThreads = 256;

// Grid value reader (BilagridReader::operator[]).
struct BgReader {
    float* fp32;
    uint* q16;        // halfword-packed 16-bit codes
    float2* vbounds;  // per-256-cell (lo, hi)
};

[ForceInline]
float bg_read(bool vq, BgReader r, int idx) {
    if (!vq) return r.fp32[idx];
    float2 mm = r.vbounds[idx >> 8];
    uint w = r.q16[uint(idx) >> 1];
    float q = float((w >> (16u * (uint(idx) & 1u))) & 0xffffu);
    return mm.x + (mm.y - mm.x) * (q * (1.0f / 65535.0f));
}

// Trilinear cell footprint. o[corner] with corner bit0 -> x1, bit1 -> y1,
// bit2 -> z1 (offsets are in FLOATS: cell index * C).
struct BgCells {
    int o[8];
    float fx, fy, fz;
    int x0, x1, y0, y1, z0, z1;
};

[ForceInline]
int _bg_cell_off(int base, int zi, int yi, int xi, int H, int W, int C) {
    return base + ((zi * H + yi) * W + xi) * C;
}

[ForceInline]
void _bg_fill_offsets(inout BgCells c, int base, int H, int W, int C) {
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        int xi = (corner & 1) != 0 ? c.x1 : c.x0;
        int yi = (corner & 2) != 0 ? c.y1 : c.y0;
        int zi = (corner & 4) != 0 ? c.z1 : c.z0;
        c.o[corner] = _bg_cell_off(base, zi, yi, xi, H, W, C);
    }
}

// Full clamp on both floor and ceil corners (per-pixel-coords sample /
// packed kernels: BilagridSampleFwd.cu style).
[ForceInline]
BgCells bg_cells_clamped(int base, int L, int H, int W, int C,
                         float x, float y, float z) {
    BgCells c;
    int x0 = int(floor(x)), y0 = int(floor(y)), z0 = int(floor(z));
    int x1 = x0 + 1, y1 = y0 + 1, z1 = z0 + 1;
    c.x0 = min(max(x0, 0), W - 1);
    c.x1 = min(max(x1, 0), W - 1);
    c.y0 = min(max(y0, 0), H - 1);
    c.y1 = min(max(y1, 0), H - 1);
    c.z0 = min(max(z0, 0), L - 1);
    c.z1 = min(max(z1, 0), L - 1);
    c.fx = x - float(x0);
    c.fy = y - float(y0);
    c.fz = z - float(z0);
    _bg_fill_offsets(c, base, H, W, C);
    return c;
}

// Uniform/patched sampler corners: floor is unclamped (coords are known
// in-range by construction), ceil clamps high only.
[ForceInline]
BgCells bg_cells_uniform(int base, int L, int H, int W, int C,
                         float x, float y, float z) {
    BgCells c;
    c.x0 = int(floor(x));
    c.y0 = int(floor(y));
    c.z0 = int(floor(z));
    c.x1 = min(c.x0 + 1, W - 1);
    c.y1 = min(c.y0 + 1, H - 1);
    c.z1 = min(c.z0 + 1, L - 1);
    c.fx = x - float(c.x0);
    c.fy = y - float(c.y0);
    c.fz = z - float(c.z0);
    _bg_fill_offsets(c, base, H, W, C);
    return c;
}

// Trilinear interpolation of channel ci across the 8 corners, in the exact
// association order of the CUDA kernels (x, then y, then z lerps).
[ForceInline]
float bg_trilerp(bool vq, BgReader r, BgCells c, int ci) {
    float v000 = bg_read(vq, r, c.o[0] + ci);
    float v001 = bg_read(vq, r, c.o[1] + ci);
    float v010 = bg_read(vq, r, c.o[2] + ci);
    float v011 = bg_read(vq, r, c.o[3] + ci);
    float v100 = bg_read(vq, r, c.o[4] + ci);
    float v101 = bg_read(vq, r, c.o[5] + ci);
    float v110 = bg_read(vq, r, c.o[6] + ci);
    float v111 = bg_read(vq, r, c.o[7] + ci);
    float c00 = v000 * (1.0f - c.fx) + v001 * c.fx;
    float c01 = v010 * (1.0f - c.fx) + v011 * c.fx;
    float c10 = v100 * (1.0f - c.fx) + v101 * c.fx;
    float c11 = v110 * (1.0f - c.fx) + v111 * c.fx;
    float c0 = c00 * (1.0f - c.fy) + c01 * c.fy;
    float c1 = c10 * (1.0f - c.fy) + c11 * c.fy;
    return c0 * (1.0f - c.fz) + c1 * c.fz;
}

// Corner weights w000..w111 in corner-bit order.
[ForceInline]
void bg_corner_weights(BgCells c, out float w8[8]) {
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        float wx = (corner & 1) != 0 ? c.fx : 1.0f - c.fx;
        float wy = (corner & 2) != 0 ? c.fy : 1.0f - c.fy;
        float wz = (corner & 4) != 0 ? c.fz : 1.0f - c.fz;
        w8[corner] = wx * wy * wz;
    }
}

// dw/dz per corner ((L-1) chain factor applied by the caller).
[ForceInline]
void bg_dwdz(BgCells c, out float d8[8]) {
    [ForceUnroll]
    for (int corner = 0; corner < 8; corner++) {
        float wx = (corner & 1) != 0 ? c.fx : 1.0f - c.fx;
        float wy = (corner & 2) != 0 ? c.fy : 1.0f - c.fy;
        d8[corner] = wx * wy * ((corner & 4) != 0 ? 1.0f : -1.0f);
    }
}

[ForceInline]
bool bg_finite(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

// ---------------------------------------------------------------------------
// All 64 threads must participate. The subgroup fold preserves the shared
// tree's addition order; the next call's first barrier protects its scratch.
// ---------------------------------------------------------------------------

groupshared float _bg_red64[64];

[ForceInline]
void bg_block_add_64(float* addr, float val, uint tid) {
    GroupMemoryBarrierWithGroupSync();
    _bg_red64[tid] = bg_finite(val) ? val : 0.0f;
    GroupMemoryBarrierWithGroupSync();
    uint width = WaveGetLaneCount();
    for (uint s = 32; s >= width; s >>= 1) {
        if (tid < s) _bg_red64[tid] += _bg_red64[tid + s];
        GroupMemoryBarrierWithGroupSync();
    }
    uint lane = WaveGetLaneIndex();
    float sum = _bg_red64[lane];
    for (uint s = min(32u, width / 2u); s > 0; s >>= 1) {
        float other = WaveReadLaneAt(sum, min(lane + s, width - 1u));
        if (lane < s) sum += other;
    }
    if (tid == 0 && sum != 0.0f) atomic_add_f32(addr, sum);
}

// ---------------------------------------------------------------------------
// backward_v1 grid-grad pixel-range partition (identical int/float math to
// the CUDA kernels; hoisted here because all five families repeat it).
// ---------------------------------------------------------------------------

struct BgV1Range {
    int wi0, wi1, hi0, hi1;
    bool nonempty;  // PATCHED-mode `block_wi1 > block_wi0 && ...` predicate
};

[ForceInline]
BgV1Range bg_v1_range(int xi, int yi, int xf, int yf, int mult_x, int mult_y,
                      int W, int H, int w_full, int h_full,
                      int wlo, int hlo, int whi, int hhi) {
    // wlo/hlo: lower clamp (0 for uniform, offset for patched);
    // whi/hhi: inclusive upper clamp source (w-1 uniform,
    //          min(offset+w, w0)-1 patched).
    float sw = float(w_full - 1) / float(W - 1);
    int block_wi0 = max(int(ceil(float(xi - 1) * sw)), wlo);
    int block_wi1 = min(int(floor(float(xi + 1) * sw)), whi) + 1;
    float sh = float(h_full - 1) / float(H - 1);
    int block_hi0 = max(int(ceil(float(yi - 1) * sh)), hlo);
    int block_hi1 = min(int(floor(float(yi + 1) * sh)), hhi) + 1;
    int x_step = (block_wi1 - block_wi0 + mult_x - 1) / mult_x;
    int y_step = (block_hi1 - block_hi0 + mult_y - 1) / mult_y;
    BgV1Range r;
    r.nonempty = (block_wi1 > block_wi0 && block_hi1 > block_hi0);
    r.wi0 = block_wi0 + xf * x_step;
    r.hi0 = block_hi0 + yf * y_step;
    r.wi1 = min(block_wi0 + (xf + 1) * x_step, block_wi1);
    r.hi1 = min(block_hi0 + (yf + 1) * y_step, block_hi1);
    return r;
}

// accum_t: sum of trilinear weights this (xi, yi, zi) cell receives from the
// pixel's corner footprint (shared by all five families' grid-grad kernels).
[ForceInline]
float bg_v1_accum_t(int xi, int yi, int zi, BgCells c) {
    float accum_t = 0.0f;
    if (zi == c.z0) {
        if (xi == c.x0 && yi == c.y0)
            accum_t += (1 - c.fx) * (1 - c.fy) * (1 - c.fz);
        if (xi == c.x1 && yi == c.y0) accum_t += c.fx * (1 - c.fy) * (1 - c.fz);
        if (xi == c.x0 && yi == c.y1) accum_t += (1 - c.fx) * c.fy * (1 - c.fz);
        if (xi == c.x1 && yi == c.y1) accum_t += c.fx * c.fy * (1 - c.fz);
    }
    if (zi == c.z1) {
        if (xi == c.x0 && yi == c.y0) accum_t += (1 - c.fx) * (1 - c.fy) * c.fz;
        if (xi == c.x1 && yi == c.y0) accum_t += c.fx * (1 - c.fy) * c.fz;
        if (xi == c.x0 && yi == c.y1) accum_t += (1 - c.fx) * c.fy * c.fz;
        if (xi == c.x1 && yi == c.y1) accum_t += c.fx * c.fy * c.fz;
    }
    return accum_t;
}
