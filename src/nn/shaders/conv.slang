// Convolution.
//
// Strategy, chosen from the shapes SAM 3 actually runs:
//
//   1x1                -> a Linear. Same kernel, no code here.
//   k x k, few channels-> im2col into a bounded workspace, then the tuned GEMM.
//                         OpConv.cpp chunks the output positions so the column
//                         buffer stays under ~32 MiB: a full im2col of the seg
//                         head's 288x288x256 3x3 conv would be 764 MiB.
//   depthwise          -> direct, below. The memory encoder's 7x7 fuser is the
//                         only user and a GEMM formulation buys nothing.
//   transpose k=2 s=2  -> GEMM + scatter (misc.slang::convt_scatter).
//
// Doing it this way means one well-tuned matmul carries ~95% of the model's
// FLOPs instead of a family of hand-rolled convolutions.

#include "_common.slang"

// Spec ids 0..2.
[SpecializationConstant] const int kInputF16 = 0;
[SpecializationConstant] const int kAct      = 0;
[SpecializationConstant] const int kPadEdge  = 0;   // im2col only

static const int WG = 256;

struct Im2ColParams {
    float* out;   // [P, Ci*kh*kw]
    uint*  x;     // [Hi, Wi, Ci]
    uint   Hi;
    uint   Wi;
    uint   Ci;
    uint   Ho;
    uint   Wo;
    uint   kh;
    uint   kw;
    uint   stride_y;
    uint   stride_x;
    uint   pad_y;
    uint   pad_x;
    uint   p0;    // first output position handled by this chunk
    uint   P;     // positions in this chunk
    uint   groups_per_row;
};

// Column order is ci*kh*kw + ky*kw + kx, which is exactly PyTorch's
// [Cout, Cin, kH, kW] weight memory order -- so the GEMM can use the checkpoint
// weight verbatim, with no repacking.
[shader("compute")]
[numthreads(WG, 1, 1)]
void im2col(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
            uniform Im2ColParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint K = p.Ci * p.kh * p.kw;
    if (i >= p.P * K) return;
    uint col = i % K;
    uint pos = p.p0 + i / K;

    uint kx = col % p.kw;
    uint rest = col / p.kw;
    uint ky = rest % p.kh;
    uint ci = rest / p.kh;

    uint xo = pos % p.Wo;
    uint yo = pos / p.Wo;
    int y = int(yo * p.stride_y + ky) - int(p.pad_y);
    int x = int(xo * p.stride_x + kx) - int(p.pad_x);

    float v = 0.0f;
    if (kPadEdge != 0) {
        y = clamp(y, 0, int(p.Hi) - 1);
        x = clamp(x, 0, int(p.Wi) - 1);
        v = load_at(p.x, (uint(y) * p.Wi + uint(x)) * p.Ci + ci, kInputF16 != 0);
    } else if (y >= 0 && y < int(p.Hi) && x >= 0 && x < int(p.Wi)) {
        v = load_at(p.x, (uint(y) * p.Wi + uint(x)) * p.Ci + ci, kInputF16 != 0);
    }
    store_at(p.out, i, v);
}

// ---------------------------------------------------------------------------
// Deformable convolution
// ---------------------------------------------------------------------------

struct DeformIm2ColParams {
    float* out;      // [P, Ci*kh*kw]
    uint*  x;        // [Hi, Wi, Ci]
    uint*  offset;   // [Ho, Wo, 2*kh*kw] -- (dy, dx) per tap, channel-last
    uint   Hi;
    uint   Wi;
    uint   Ci;
    uint   Ho;
    uint   Wo;
    uint   kh;
    uint   kw;
    uint   stride_y;
    uint   stride_x;
    uint   pad_y;
    uint   pad_x;
    uint   p0;
    uint   P;
    float  max_offset;   // offsets are clamped to +-this; <= 0 disables
    uint   groups_per_row;
};

// The same column layout im2col produces, so the GEMM and the checkpoint
// weight are shared verbatim between the two -- only where a tap reads
// changes.
//
// This is torchvision's `deform_conv2d` with mask = None, which is the form
// ALIKED's checkpoints carry (their offset convs have 2*k*k output channels,
// not 3*k*k; AlikedWeights::load refuses anything else). Two details are
// load-bearing and neither is guessable from the name:
//
//   * The sampling position is p0 + tap + offset, where the offset for tap
//     (ky, kx) lives at offset channels 2*(ky*kw + kx) and +1, in that order,
//     (dy, dx). Swapping them produces a network that trains fine and
//     matches badly.
//   * Sampling reads ZERO outside the input, not the edge pixel. See
//     sample_bilinear_zero.
//
// One bilinear fetch per (position, tap, channel): the four corner loads are
// what a plain im2col would do anyway, and only the weights are recomputed
// per channel.
[shader("compute")]
[numthreads(WG, 1, 1)]
void deform_im2col(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                   uniform DeformIm2ColParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint K = p.Ci * p.kh * p.kw;
    if (i >= p.P * K) return;
    uint col = i % K;
    uint pos = p.p0 + i / K;

    uint kx = col % p.kw;
    uint rest = col / p.kw;
    uint ky = rest % p.kh;
    uint ci = rest / p.kh;

    uint xo = pos % p.Wo;
    uint yo = pos / p.Wo;

    uint tap = ky * p.kw + kx;
    uint obase = (yo * p.Wo + xo) * (2u * p.kh * p.kw) + 2u * tap;
    float dy = asfloat(load_u32_at(p.offset, obase));
    float dx = asfloat(load_u32_at(p.offset, obase + 1u));
    if (p.max_offset > 0.0f) {
        dy = clamp(dy, -p.max_offset, p.max_offset);
        dx = clamp(dx, -p.max_offset, p.max_offset);
    }

    float y = float(int(yo * p.stride_y + ky) - int(p.pad_y)) + dy;
    float x = float(int(xo * p.stride_x + kx) - int(p.pad_x)) + dx;
    store_at(p.out, i, sample_bilinear_zero(p.x, kInputF16 != 0, int(p.Hi), int(p.Wi),
                                            int(p.Ci), int(ci), y, x));
}

// ---------------------------------------------------------------------------
// Patch gather
// ---------------------------------------------------------------------------

struct PatchGatherParams {
    float* out;      // [N, C*k*k]
    uint*  x;        // [Hi, Wi, C]
    uint*  centers;  // [N, 2] i32 (x, y) pixel coordinates
    uint   Hi;
    uint   Wi;
    uint   C;
    uint   N;
    uint   k;
    uint   groups_per_row;
};

// A k x k patch around each of N integer centers, laid out so the result
// multiplies a [Cout, C*k*k] conv weight verbatim -- im2col at arbitrary
// positions rather than at every position.
//
// What ALIKED's descriptor head does first: its offset network is a
// Conv2d(dim, 2M, k=3, padding=0) evaluated at the keypoints only, which is
// this gather and then one GEMM, instead of a convolution over a whole image
// to read a few thousand pixels of.
//
// Out-of-range taps read zero, matching a VALID convolution of a
// zero-padded map.
[shader("compute")]
[numthreads(WG, 1, 1)]
void patch_gather(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                  uniform PatchGatherParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint K = p.C * p.k * p.k;
    if (i >= p.N * K) return;
    uint col = i % K;
    uint n = i / K;

    uint kx = col % p.k;
    uint rest = col / p.k;
    uint ky = rest % p.k;
    uint ci = rest / p.k;

    int cx = int(p.centers[2u * n]);
    int cy = int(p.centers[2u * n + 1u]);
    int r = int(p.k) / 2;
    int y = cy - r + int(ky);
    int x = cx - r + int(kx);

    float v = 0.0f;
    if (y >= 0 && y < int(p.Hi) && x >= 0 && x < int(p.Wi))
        v = load_at(p.x, (uint(y) * p.Wi + uint(x)) * p.C + ci, kInputF16 != 0);
    store_at(p.out, i, v);
}

struct DepthwiseParams {
    float* out;   // [Ho, Wo, C]
    uint*  x;     // [Hi, Wi, C]
    uint*  w;     // [C, kh, kw]
    uint*  bias;  // [C] f32 (never null)
    uint   Hi;
    uint   Wi;
    uint   Ho;
    uint   Wo;
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
void conv2d_depthwise(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                      uniform DepthwiseParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.Ho * p.Wo * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint xo = rest % p.Wo;
    uint yo = rest / p.Wo;

    float sum = asfloat(p.bias[c]);
    for (uint ky = 0; ky < p.kh; ++ky) {
        int y = int(yo * p.stride_y + ky) - int(p.pad_y);
        if (y < 0 || y >= int(p.Hi)) continue;
        for (uint kx = 0; kx < p.kw; ++kx) {
            int x = int(xo * p.stride_x + kx) - int(p.pad_x);
            if (x < 0 || x >= int(p.Wi)) continue;
            float xv = load_at(p.x, (uint(y) * p.Wi + uint(x)) * p.C + c, kInputF16 != 0);
            float wv = load_elem(p.w, (c * p.kh + ky) * p.kw + kx, kInputF16 != 0);
            sum += xv * wv;
        }
    }
    store_at(p.out, i, apply_act(sum, kAct));
}

// ---------------------------------------------------------------------------
// Patch embedding
// ---------------------------------------------------------------------------

struct PatchifyParams {
    float* out;   // [(Hi/P)*(Wi/P), P*P*Ci] with column order ci*P*P + ky*P + kx
    uint*  x;     // [Hi, Wi, Ci]
    uint   Hi;
    uint   Wi;
    uint   Ci;
    uint   patch;
    uint   groups_per_row;
    uint   _pad0;
};

// Non-overlapping patch extraction: the k=stride=patch case of im2col, kept
// separate because the ViT stem runs it on the full 1008x1008x3 input and the
// index math collapses to shifts.
[shader("compute")]
[numthreads(WG, 1, 1)]
void patchify(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
              uniform PatchifyParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint gh = p.Hi / p.patch, gw = p.Wi / p.patch;
    uint K = p.patch * p.patch * p.Ci;
    if (i >= gh * gw * K) return;
    uint col = i % K;
    uint pos = i / K;
    uint kx = col % p.patch;
    uint rest = col / p.patch;
    uint ky = rest % p.patch;
    uint ci = rest / p.patch;
    uint px = pos % gw;
    uint py = pos / gw;
    uint y = py * p.patch + ky;
    uint x = px * p.patch + kx;
    store_at(p.out, i, load_at(p.x, (y * p.Wi + x) * p.Ci + ci, kInputF16 != 0));
}
