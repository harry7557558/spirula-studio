// Spatial resampling on channel-last [H, W, C] maps.
//
// The bilinear kernel follows torch's `align_corners=False` mapping exactly --
// src = (dst + 0.5) * scale - 0.5, clamped, with the low index taken by floor
// and the high index clamped to H-1. Mask upsampling runs through it on every
// frame of a video, and an off-by-half here shifts every boundary by a pixel.
//
// `align_corners=True` is the OTHER mapping -- src = dst * (Hi-1)/(Ho-1) -- and
// is not interchangeable with it: ALIKED's encoder upsamples every block that
// way, and its keypoints are normalized on the same convention, so mixing the
// two moves features by up to half a pixel at every pyramid level. It is a
// specialization constant so each caller states which it means and neither
// pays a branch.

#include "_common.slang"

// Spec ids 0..2.
[SpecializationConstant] const int kInputF16     = 0;
[SpecializationConstant] const int kAct          = 0;
[SpecializationConstant] const int kAlignCorners = 0;

static const int WG = 256;

struct ResizeParams {
    float* out;
    uint*  x;
    uint   Ho;
    uint   Wo;
    uint   Hi;
    uint   Wi;
    uint   C;
    uint   groups_per_row;
};

struct ScaledResizeParams {
    float* out;
    uint*  x;
    uint   Ho;
    uint   Wo;
    uint   Hi;
    uint   Wi;
    uint   C;
    uint   groups_per_row;
    float  inv_scale_y;
    float  inv_scale_x;
};

// Source coordinate for output index `o` along one axis.
float src_coord(uint o, uint n_out, uint n_in) {
    if (kAlignCorners != 0) {
        // A single output sample maps to 0, matching torch (which divides by
        // max(n_out - 1, 1) and would otherwise produce a NaN).
        if (n_out <= 1u) return 0.0f;
        return float(o) * (float(n_in) - 1.0f) / (float(n_out) - 1.0f);
    }
    return max((float(o) + 0.5f) * (float(n_in) / float(n_out)) - 0.5f, 0.0f);
}

[shader("compute")]
[numthreads(WG, 1, 1)]
void resize_bilinear(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                     uniform ResizeParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.Ho * p.Wo * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint xo = rest % p.Wo;
    uint yo = rest / p.Wo;

    float sy = src_coord(yo, p.Ho, p.Hi);
    float sx = src_coord(xo, p.Wo, p.Wi);
    store_at(p.out, i, sample_bilinear(p.x, kInputF16 != 0, int(p.Hi), int(p.Wi),
                                      int(p.C), int(c), sy, sx));
}

// Nearest-neighbour resample at an arbitrary scale.
//
// `scale` is the factor the CALLER asked torch for, not `n_out / n_in`, and
// the two differ: F.interpolate(scale_factor=s, mode='nearest') sizes the
// output floor(n_in * s) and then maps src = floor(dst / s) with the original
// s. Metric3D's DPT read_0 upsamples by exactly 7/2, where reconstructing the
// scale from the sizes lands on a different source row every few outputs.
[shader("compute")]
[numthreads(WG, 1, 1)]
void resize_nearest(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                    uniform ScaledResizeParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.Ho * p.Wo * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint xo = rest % p.Wo;
    uint yo = rest / p.Wo;
    uint ys = min(uint(floor(float(yo) * p.inv_scale_y)), p.Hi - 1u);
    uint xs = min(uint(floor(float(xo) * p.inv_scale_x)), p.Wi - 1u);
    store_at(p.out, i, load_at(p.x, (ys * p.Wi + xs) * p.C + c, kInputF16 != 0));
}

// Nearest-neighbour 2x upsample -- what the seg head's PixelDecoder uses
// between FPN levels (F.interpolate(mode="nearest")).
[shader("compute")]
[numthreads(WG, 1, 1)]
void upsample_nearest2x(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                        uniform ResizeParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.Ho * p.Wo * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint xo = rest % p.Wo;
    uint yo = rest / p.Wo;
    uint ys = min(yo >> 1, p.Hi - 1u);
    uint xs = min(xo >> 1, p.Wi - 1u);
    store_at(p.out, i, load_at(p.x, (ys * p.Wi + xs) * p.C + c, kInputF16 != 0));
}

// 2x2 max pool, stride 2 -- the neck's 0.5x scale.
[shader("compute")]
[numthreads(WG, 1, 1)]
void maxpool2x2(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                uniform ResizeParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.Ho * p.Wo * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint xo = rest % p.Wo;
    uint yo = rest / p.Wo;
    float best = -3.402823466e+38f;
    for (uint dy = 0; dy < 2u; ++dy) {
        uint y = yo * 2u + dy;
        if (y >= p.Hi) continue;
        for (uint dx = 0; dx < 2u; ++dx) {
            uint x = xo * 2u + dx;
            if (x >= p.Wi) continue;
            best = max(best, load_at(p.x, (y * p.Wi + x) * p.C + c, kInputF16 != 0));
        }
    }
    store_at(p.out, i, best);
}

// nn.AvgPool2d(kernel, stride, padding). ALIKED's encoder feeds blocks 3 and 4
// from 2x and 4x pooled copies of the input; Metric3D's GRU cascade pools with
// padding=1 between resolutions.
//
// The divisor is always kh*kw, never the number of taps actually inside the
// image -- that is torch's `count_include_pad=True` default, and it is what
// makes an edge window darker than an interior one. ceil_mode=False sizes the
// output so no window starts past the padded edge.
struct PoolParams {
    float* out;
    uint*  x;
    uint   Ho;
    uint   Wo;
    uint   Hi;
    uint   Wi;
    uint   C;
    uint   kh;
    uint   kw;
    uint   stride_y;
    uint   stride_x;
    uint   pad_y;
    uint   pad_x;
    uint   groups_per_row;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void avgpool(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
             uniform PoolParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.Ho * p.Wo * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint xo = rest % p.Wo;
    uint yo = rest / p.Wo;

    float sum = 0.0f;
    for (uint ky = 0; ky < p.kh; ++ky) {
        int y = int(yo * p.stride_y + ky) - int(p.pad_y);
        if (y < 0 || y >= int(p.Hi)) continue;
        for (uint kx = 0; kx < p.kw; ++kx) {
            int x = int(xo * p.stride_x + kx) - int(p.pad_x);
            if (x < 0 || x >= int(p.Wi)) continue;
            sum += load_at(p.x, (uint(y) * p.Wi + uint(x)) * p.C + c, kInputF16 != 0);
        }
    }
    store_at(p.out, i, sum / float(p.kh * p.kw));
}

// ---------------------------------------------------------------------------
// Mask export: resize + threshold + pack to bytes, in one pass
// ---------------------------------------------------------------------------

struct MaskExportParams {
    uint*  out;    // ceil(Ho*Wo/4) u32 words, 4 mask bytes each
    uint*  x;      // [Hi, Wi] single-channel logits
    uint   Ho;
    uint   Wo;
    uint   Hi;
    uint   Wi;
    float  threshold;
    uint   groups_per_row;
};

// A tracked video frame can carry 200 instance masks at 1920x1080; pulling
// those back as floats would be 1.6 GB. Thresholding on device and returning
// packed bytes (0 / 255) makes it 400 MB of u8, and the host never sees a
// float mask. Four consecutive x per thread so the store is a whole word --
// byte stores would need the 8-bit-storage feature, which is off-baseline.
[shader("compute")]
[numthreads(WG, 1, 1)]
void resize_binarize(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                     uniform MaskExportParams p) {
    uint w = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint total = p.Ho * p.Wo;
    uint n_words = (total + 3u) / 4u;
    if (w >= n_words) return;

    uint packed = 0u;
    for (uint b = 0; b < 4u; ++b) {
        uint idx = w * 4u + b;
        if (idx >= total) break;
        uint xo = idx % p.Wo;
        uint yo = idx / p.Wo;
        float sy = max((float(yo) + 0.5f) * (float(p.Hi) / float(p.Ho)) - 0.5f, 0.0f);
        float sx = max((float(xo) + 0.5f) * (float(p.Wi) / float(p.Wo)) - 0.5f, 0.0f);
        float v = sample_bilinear(p.x, kInputF16 != 0, int(p.Hi), int(p.Wi), 1, 0, sy, sx);
        if (v > p.threshold) packed |= 255u << (b * 8u);
    }
    store_u32_at(p.out, w, packed);
}
