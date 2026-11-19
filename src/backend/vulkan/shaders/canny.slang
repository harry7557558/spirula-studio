// Canny edge filters + RobustNeRF-style residual preprocessing (Vulkan
// backend), mirroring the edge-aware densification-map kernels in
// kernels/densify/DensifySplitFilter.cu (canny_edge_filter_kernel, canny_edge_filter_kernel_scalar,
// _robust_residual_luma_kernel, _robust_tukey_inplace_kernel).
//
// CUDA runs 32x32 blocks; here 16x16 logical tiles run as flat 256-wide
// workgroups (pinned-subgroup rule). All loads are globally clamped, so every
// output pixel is a pure function of the input image regardless of tile size
// -> per-pixel outputs land in the tight parity channel.

#include "int64_compat.slang"
#include "int8_compat.slang"

static const int CB = 16;      // logical tile
static const int CH = 4;       // input halo (blur 5x5 + canny 3x3 + NMS 1)
static const int CH1 = 2;      // post-blur halo
static const int CH2 = 1;      // post-canny halo (NMS reach)

static const float kBlur5[5][5] = {
    { 2.f/159.f,  4.f/159.f,  5.f/159.f,  4.f/159.f, 2.f/159.f },
    { 4.f/159.f,  9.f/159.f, 12.f/159.f,  9.f/159.f, 4.f/159.f },
    { 5.f/159.f, 12.f/159.f, 15.f/159.f, 12.f/159.f, 5.f/159.f },
    { 4.f/159.f,  9.f/159.f, 12.f/159.f,  9.f/159.f, 4.f/159.f },
    { 2.f/159.f,  4.f/159.f,  5.f/159.f,  4.f/159.f, 2.f/159.f },
};

static const float kCanny3[3][3] = {
    { -1.0f, 0.0f, 1.0f },
    { -2.0f, 0.0f, 2.0f },
    { -1.0f, 0.0f, 1.0f },
};

struct CannyParams {
    float* img_in;   // rgb: [B,H,W,3]; scalar: [B,H,W,1]
    uint* mask_in;   // [B,H,W] bool bytes
    float* img_out;  // [B,H,W,1]
    int B, H, W;
    int has_mask;
};

groupshared float _cn_pixels[(CB + 2 * CH) * (CB + 2 * CH)];       // 24x24
groupshared float _cn_valid[(CB + 2 * CH) * (CB + 2 * CH)];        // 24x24
groupshared float _cn_blurred[(CB + 2 * CH1) * (CB + 2 * CH1)];    // 20x20
groupshared float2 _cn_filtered[(CB + 2 * CH2) * (CB + 2 * CH2)];  // 18x18

#define PIX(y, x) _cn_pixels[(y) * (CB + 2 * CH) + (x)]
#define VLD(y, x) _cn_valid[(y) * (CB + 2 * CH) + (x)]
#define BLR(y, x) _cn_blurred[(y) * (CB + 2 * CH1) + (x)]
#define FLT(y, x) _cn_filtered[(y) * (CB + 2 * CH2) + (x)]

// Shared canny body after the luma tile is loaded. tx/ty = logical thread.
[ForceInline]
void _cn_body(CannyParams p, uint3 wg, uint tid, int tx, int ty) {
    int xid = int(wg.x) * CB + tx;
    int yid = int(wg.y) * CB + ty;
    int bid = int(wg.z);

    // 5x5 blur over the unmasked samples only. Gating canny's output at the
    // end instead lets the mask silhouette -- a step from distractor to scene
    // -- register as the strongest edge in the image.
    for (int t = int(tid); t < (CB + 2 * CH1) * (CB + 2 * CH1); t += 256) {
        int y = t / (CB + 2 * CH1);
        int x = t % (CB + 2 * CH1);
        float total = 0.0f, weight = 0.0f;
        [ForceUnroll]
        for (int cy = -2; cy <= 2; cy++)
            [ForceUnroll]
            for (int cx = -2; cx <= 2; cx++) {
                int yi = y - CH1 + cy;
                int xi = x - CH1 + cx;
                float w = kBlur5[cy + 2][cx + 2] * VLD(yi + CH, xi + CH);
                total += w * PIX(yi + CH, xi + CH);
                weight += w;
            }
        // Zero weight is >2px inside the mask, out of reach of any unmasked
        // output pixel: canny reads blurred values at most 2px away.
        BLR(y, x) = weight > 0.0f ? total / weight : 0.0f;
    }
    GroupMemoryBarrierWithGroupSync();

    // 3x3 canny (both orientations).
    for (int t = int(tid); t < (CB + 2 * CH2) * (CB + 2 * CH2); t += 256) {
        int y = t / (CB + 2 * CH2);
        int x = t % (CB + 2 * CH2);
        float total1 = 0.0f, total2 = 0.0f;
        [ForceUnroll]
        for (int cy = -1; cy <= 1; cy++)
            [ForceUnroll]
            for (int cx = -1; cx <= 1; cx++) {
                int yi = y - CH2 + cy;
                int xi = x - CH2 + cx;
                float value = BLR(yi + CH1, xi + CH1);
                total1 += kCanny3[cy + 1][cx + 1] * value;
                total2 += kCanny3[cx + 1][cy + 1] * value;
            }
        FLT(y, x) = float2(total1, total2);
    }
    GroupMemoryBarrierWithGroupSync();

    // Non-maximum suppression + masked write.
    float2 total = FLT(ty + CH2, tx + CH2);
    float mag = length(total);
    if (mag > 0.0f) {
        int dx = min(max(int(round(total.x / mag)), -CH2), CH2);
        int dy = min(max(int(round(total.y / mag)), -CH2), CH2);
        float total1 = length(FLT(ty + dy + CH2, tx + dx + CH2));
        float total2 = length(FLT(ty - dy + CH2, tx - dx + CH2));
        if (mag < total1 || mag < total2) mag = 0.0f;
    }
    if (yid < p.H && xid < p.W) {
        uint pix = (uint(bid) * uint(p.H) + uint(yid)) * uint(p.W) + uint(xid);
        if (p.has_mask != 0 &&
            u8_load(p.mask_in, pix) != 1u)
            mag = 0.0f;
        p.img_out[pix] = mag;
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void canny_rgb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
               uniform CannyParams p) {
    int tx = int(tid % 16u);
    int ty = int(tid / 16u);
    int bid = int(wg.z);

    for (int t = int(tid); t < (CB + 2 * CH) * (CB + 2 * CH); t += 256) {
        int y = t / (CB + 2 * CH);
        int x = t % (CB + 2 * CH);
        int yi = min(max(int(wg.y) * CB + y - CH, 0), p.H - 1);
        int xi = min(max(int(wg.x) * CB + x - CH, 0), p.W - 1);
        gindex_t pix =
            ((gindex_t)bid * p.H + yi) * (gindex_t)p.W + xi;
        float3 rgb = float3(p.img_in[3 * pix], p.img_in[3 * pix + 1],
                            p.img_in[3 * pix + 2]);
        PIX(y, x) = dot(rgb, float3(0.299f, 0.587f, 0.114f));
        VLD(y, x) = (p.has_mask != 0 && u8_load(p.mask_in, (uint)pix) != 1u)
            ? 0.0f : 1.0f;
    }
    GroupMemoryBarrierWithGroupSync();
    _cn_body(p, wg, tid, tx, ty);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void canny_scalar(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                  uniform CannyParams p) {
    int tx = int(tid % 16u);
    int ty = int(tid / 16u);
    int bid = int(wg.z);

    for (int t = int(tid); t < (CB + 2 * CH) * (CB + 2 * CH); t += 256) {
        int y = t / (CB + 2 * CH);
        int x = t % (CB + 2 * CH);
        int yi = min(max(int(wg.y) * CB + y - CH, 0), p.H - 1);
        int xi = min(max(int(wg.x) * CB + x - CH, 0), p.W - 1);
        gindex_t pix = ((gindex_t)bid * p.H + yi) * (gindex_t)p.W + xi;
        PIX(y, x) = p.img_in[pix];
        VLD(y, x) = (p.has_mask != 0 && u8_load(p.mask_in, (uint)pix) != 1u)
            ? 0.0f : 1.0f;
    }
    GroupMemoryBarrierWithGroupSync();
    _cn_body(p, wg, tid, tx, ty);
}

#undef PIX
#undef VLD
#undef BLR
#undef FLT

// |render - GT| projected onto BT.601 luma.
struct ResidLumaParams {
    float* render;  // [B,H,W,3]
    float* ref;     // [B,H,W,3]
    uint* mask_in;  // [B,H,W] bool bytes, or fallback
    float* out;     // [B,H,W]
    int B, H, W;
    int has_mask;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void residual_luma(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform ResidLumaParams p) {
    int xid = int(wg.x) * 16 + int(tid % 16u);
    int yid = int(wg.y) * 16 + int(tid / 16u);
    int bid = int(wg.z);
    if (xid >= p.W || yid >= p.H || bid >= p.B) return;
    gindex_t idx = ((gindex_t)bid * p.H + yid) * (gindex_t)p.W + xid;
    // Infinity, not zero: the quantile counts only strictly positive finite
    // values, so a masked residual is dropped instead of dragging the Tukey
    // cutoff around. The Tukey pass turns it into a real zero.
    if (p.has_mask != 0 && u8_load(p.mask_in, (uint)idx) != 1u) {
        p.out[idx] = 1.0f / 0.0f;
        return;
    }
    float dx = p.render[3 * idx] - p.ref[3 * idx];
    float dy = p.render[3 * idx + 1] - p.ref[3 * idx + 1];
    float dz = p.render[3 * idx + 2] - p.ref[3 * idx + 2];
    p.out[idx] = abs(0.299f * dx + 0.587f * dy + 0.114f * dz);
}

// In-place Tukey biweight: data <- |r| * (1 - (r/c)^2)^2, zero past c.
struct TukeyParams {
    float* data;   // [B, N]
    float* c_buf;  // [B]
    int B, N;
    uint wgs_per_row;
    int _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void tukey_inplace(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform TukeyParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    uint total = uint(p.B) * uint(p.N);
    if (idx >= total) return;
    int b = int(idx / uint(p.N));
    float c = p.c_buf[b];
    float r = p.data[idx];
    if (!(c > 0.0f) || !(r < c)) {  // !(r < c) also catches the masked infinity
        p.data[idx] = 0.0f;
        return;
    }
    float u = r / c;
    float w = 1.0f - u * u;
    p.data[idx] = r * w * w;
}
