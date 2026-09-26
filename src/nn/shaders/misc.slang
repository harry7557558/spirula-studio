// The odds and ends: RoPE, embedding gather, window partition, the
// transposed-conv scatter, ROI-align, tiled positional embedding, and strided
// copies. Each is small; keeping them in one module keeps the pipeline cache
// and the build graph small too.

#include "_common.slang"

// Spec ids 0..2 (module-wide).
[SpecializationConstant] const int kInputF16 = 0;
[SpecializationConstant] const int kAct      = 0;
[SpecializationConstant] const int kAlignCorners = 0;

static const int WG = 256;

// ---------------------------------------------------------------------------
// 2-D axial RoPE
// ---------------------------------------------------------------------------

struct RopeParams {
    uint*  x;       // [B, N, n_heads * head_dim], updated in place
    uint*  freqs;   // [N, head_dim/2, 2] f32 -- (cos, sin) per complex pair
    uint   n;       // tokens
    uint   n_heads;
    uint   head_dim;
    uint   batch;
    uint   row_stride;      // elements between tokens
    uint   groups_per_row;
};

// Pairs consecutive dimensions into a complex number and rotates:
//   out[2i]   = x[2i]*cos - x[2i+1]*sin
//   out[2i+1] = x[2i]*sin + x[2i+1]*cos
// which is view_as_real(view_as_complex(x) * freqs_cis) from the reference.
// The frequency table is shared by every head and every batch element, exactly
// as in SAM 3's ViT (all 16 heads see the same axial grid).
[shader("compute")]
[numthreads(WG, 1, 1)]
void rope_apply(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                uniform RopeParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint half_d = p.head_dim / 2u;
    uint per_token = p.n_heads * half_d;
    uint total = p.batch * p.n * per_token;
    if (i >= total) return;

    uint b = i / (p.n * per_token);
    uint rem = i % (p.n * per_token);
    uint tok = rem / per_token;
    uint r2 = rem % per_token;
    uint h = r2 / half_d;
    uint pair = r2 % half_d;

    uint base = b * p.n * p.row_stride + tok * p.row_stride + h * p.head_dim + pair * 2u;
    uint foff = (tok * half_d + pair) * 2u;
    float c = asfloat(p.freqs[foff + 0u]);
    float s = asfloat(p.freqs[foff + 1u]);
    float re = asfloat(p.x[base + 0u]);
    float im = asfloat(p.x[base + 1u]);
    p.x[base + 0u] = asuint(re * c - im * s);
    p.x[base + 1u] = asuint(re * s + im * c);
}

// ---------------------------------------------------------------------------
// Row gather (token embedding)
// ---------------------------------------------------------------------------

struct GatherParams {
    float* out;     // [N, C]
    uint*  table;   // [V, C]
    uint*  ids;     // [N] i32
    uint   n;
    uint   cols;
    uint   vocab;
    uint   groups_per_row;
    uint   _pad0;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void gather_rows(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                 uniform GatherParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.n * p.cols) return;
    uint row = i / p.cols;
    uint c = i % p.cols;
    uint id = p.ids[row];
    // Out-of-range ids read row 0 rather than faulting: a corrupt token stream
    // should degrade, not take the process down.
    if (id >= p.vocab) id = 0u;
    store_at(p.out, i, load_at(p.table, id * p.cols + c, kInputF16 != 0));
}

// ---------------------------------------------------------------------------
// Strided row copy (concat, slice, q/k/v extraction)
// ---------------------------------------------------------------------------

struct StridedCopyParams {
    float* out;
    uint*  x;
    uint   rows;
    uint   cols;
    uint   in_stride;
    uint   out_stride;
    uint   groups_per_row;
    uint   _pad0;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void strided_copy(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                  uniform StridedCopyParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.rows * p.cols) return;
    uint r = i / p.cols;
    uint c = i % p.cols;
    p.out[r * p.out_stride + c] = load_elem(p.x, r * p.in_stride + c, kInputF16 != 0);
}

// ---------------------------------------------------------------------------
// Window partition / un-partition for the ViT's local-attention blocks
// ---------------------------------------------------------------------------

struct WindowParams {
    float* out;
    uint*  x;
    uint   H;
    uint   W;
    uint   C;
    uint   ws;
    uint   nwh;   // windows down = ceil(H / ws)
    uint   nww;   // windows across
    uint   groups_per_row;
    uint   shift;       // Swin's cyclic shift, applied on the padded grid
    uint   accumulate;  // unpartition only: out += instead of out =
    uint   _pad0;
};

// [H, W, C] -> [nwh*nww, ws, ws, C]. Positions past the edge are zero-filled,
// so a feature map whose size is not a multiple of the window still works (SAM
// 3's native 72x72 with ws=24 divides exactly; a custom --img-size need not).
[shader("compute")]
[numthreads(WG, 1, 1)]
void window_partition(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                      uniform WindowParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint total = p.nwh * p.nww * p.ws * p.ws * p.C;
    if (i >= total) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint wx = rest % p.ws;
    rest /= p.ws;
    uint wy = rest % p.ws;
    rest /= p.ws;
    uint win = rest;
    uint wj = win % p.nww;
    uint wi = win / p.nww;
    uint y = wi * p.ws + wy;
    uint x = wj * p.ws + wx;
    if (p.shift != 0u) {
        // torch.roll(-shift) of the zero-padded map, as Swin's SW-MSA does.
        y = (y + p.shift) % (p.nwh * p.ws);
        x = (x + p.shift) % (p.nww * p.ws);
    }
    store_at(p.out, i, (y < p.H && x < p.W)
                           ? load_at(p.x, (y * p.W + x) * p.C + c, kInputF16 != 0)
                           : 0.0f);
}

// [nwh*nww, ws, ws, C] -> [H, W, C]; the padded tail is dropped.
[shader("compute")]
[numthreads(WG, 1, 1)]
void window_unpartition(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                        uniform WindowParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint total = p.H * p.W * p.C;
    if (i >= total) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint x = rest % p.W;
    uint y = rest / p.W;
    if (p.shift != 0u) {
        const uint hp = p.nwh * p.ws, wp = p.nww * p.ws;
        y = (y + hp - p.shift) % hp;
        x = (x + wp - p.shift) % wp;
    }
    uint wi = y / p.ws, wy = y % p.ws;
    uint wj = x / p.ws, wx = x % p.ws;
    uint win = wi * p.nww + wj;
    uint src = ((win * p.ws + wy) * p.ws + wx) * p.C + c;
    float v = load_at(p.x, src, kInputF16 != 0);
    if (p.accumulate != 0u) v += load_f_at(p.out, i);
    store_at(p.out, i, v);
}

// ---------------------------------------------------------------------------
// Transposed convolution, stride 2, kernel 2 -- the scatter half
// ---------------------------------------------------------------------------

struct ConvTScatterParams {
    float* out;     // [2*Hi, 2*Wi, Co]
    uint*  packed;  // [Hi, Wi, Co*4] -- the GEMM's output
    uint*  bias;    // [Co] f32 (never null)
    uint   Hi;
    uint   Wi;
    uint   Co;
    uint   groups_per_row;
    uint   _pad0;
};

// ConvTranspose2d(k=2, s=2) is a GEMM followed by a shuffle: each input pixel
// produces one 2x2 output block, and the four kernel taps are just four output
// channel groups. OpConv.cpp repacks the [Cin, Cout, 2, 2] weight into a
// [Cout*4, Cin] matrix at load time, runs the tuned GEMM, and this kernel
// scatters. That is worth more than a bespoke deconv: the neck's two deconvs
// are 22 GFLOP, and they get the same tiling as everything else.
[shader("compute")]
[numthreads(WG, 1, 1)]
void convt_scatter(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                   uniform ConvTScatterParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint Ho = p.Hi * 2u, Wo = p.Wi * 2u;
    if (i >= Ho * Wo * p.Co) return;
    uint co = i % p.Co;
    uint rest = i / p.Co;
    uint xo = rest % Wo;
    uint yo = rest / Wo;
    uint y = yo >> 1, sy = yo & 1u;
    uint x = xo >> 1, sx = xo & 1u;
    uint src = (y * p.Wi + x) * (p.Co * 4u) + co * 4u + sy * 2u + sx;
    float v = load_at(p.packed, src, kInputF16 != 0) + asfloat(p.bias[co]);
    store_at(p.out, i, apply_act(v, kAct));
}

// ---------------------------------------------------------------------------
// Tiled positional embedding
// ---------------------------------------------------------------------------

struct TiledAddParams {
    float* out;
    uint*  x;      // [H, W, C]
    uint*  tile;   // [th, tw, C]
    uint   H;
    uint   W;
    uint   C;
    uint   th;
    uint   tw;
    uint   groups_per_row;
};

// out[y][x][c] = x[y][x][c] + tile[y % th][x % tw][c].
//
// SAM 3's ViT ships a 24x24 absolute positional embedding from the 336-pixel
// pretraining and tiles it 3x3 over the 72x72 grid -- this is that tiling, and
// it is a repeat, NOT an interpolation.
[shader("compute")]
[numthreads(WG, 1, 1)]
void add_tiled(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
               uniform TiledAddParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.H * p.W * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint x = rest % p.W;
    uint y = rest / p.W;
    uint tsrc = ((y % p.th) * p.tw + (x % p.tw)) * p.C + c;
    store_at(p.out, i, load_at(p.x, i, kInputF16 != 0) + load_at(p.tile, tsrc, kInputF16 != 0));
}

// ---------------------------------------------------------------------------
// ROI-Align (exemplar boxes -> 7x7 pooled features)
// ---------------------------------------------------------------------------

struct RoiAlignParams {
    float* out;    // [nboxes, S, S, C]
    uint*  feat;   // [H, W, C]
    uint*  boxes;  // [nboxes, 4] f32, xyxy in feature-grid coordinates
    uint   nboxes;
    uint   H;
    uint   W;
    uint   C;
    uint   S;      // output bin count per side
    uint   groups_per_row;
    uint   _pad0;
};

// torchvision.ops.roi_align with sampling_ratio = 0 (adaptive): each bin
// averages ceil(bin_h) x ceil(bin_w) bilinear samples. Reproduced exactly,
// including the -1..W out-of-range rejection, because the exemplar embedding is
// sensitive to it.
[shader("compute")]
[numthreads(WG, 1, 1)]
void roi_align(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
               uniform RoiAlignParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.nboxes * p.S * p.S * p.C) return;
    uint c = i % p.C;
    uint rest = i / p.C;
    uint pw = rest % p.S;
    rest /= p.S;
    uint ph = rest % p.S;
    uint b = rest / p.S;

    float x0 = asfloat(p.boxes[b * 4u + 0u]);
    float y0 = asfloat(p.boxes[b * 4u + 1u]);
    float x1 = asfloat(p.boxes[b * 4u + 2u]);
    float y1 = asfloat(p.boxes[b * 4u + 3u]);
    float roi_w = max(x1 - x0, 1e-6f);
    float roi_h = max(y1 - y0, 1e-6f);
    float bin_w = roi_w / float(p.S);
    float bin_h = roi_h / float(p.S);
    int ny = max(1, int(ceil(bin_h)));
    int nx = max(1, int(ceil(bin_w)));

    float sum = 0.0f;
    for (int iy = 0; iy < ny; ++iy) {
        float sy = y0 + bin_h * (float(ph) + (float(iy) + 0.5f) / float(ny));
        for (int ix = 0; ix < nx; ++ix) {
            float sx = x0 + bin_w * (float(pw) + (float(ix) + 0.5f) / float(nx));
            if (sx < -1.0f || sx > float(p.W) || sy < -1.0f || sy > float(p.H)) continue;
            sum += sample_bilinear(p.feat, kInputF16 != 0, int(p.H), int(p.W), int(p.C),
                                   int(c), max(sy, 0.0f), max(sx, 0.0f));
        }
    }
    store_at(p.out, i, sum / float(ny * nx));
}

// ---------------------------------------------------------------------------
// Point-wise grid sample
// ---------------------------------------------------------------------------

struct GridSampleParams {
    float* out;   // [N, C]
    uint*  x;     // [H, W, C]
    uint*  pos;   // [N, 2] f32, normalized (gx, gy) in [-1, 1]
    uint   H;
    uint   W;
    uint   C;
    uint   N;
    uint   groups_per_row;
};

// torch's `grid_sample(mode='bilinear', padding_mode='zeros')` at N arbitrary
// points, with align_corners taken from the module's spec constant.
//
// A whole-map resample (resample.slang) is the wrong shape for this: ALIKED's
// descriptor head samples M positions around each of a few thousand keypoints,
// i.e. a list, not a grid. roi_align is the closest existing op and is not a
// substitute -- it averages over a box, this reads one point.
//
// The two align_corners conventions differ in what -1 and +1 mean: the corner
// *pixel centres* when true, the outer *edges* of the corner pixels when
// false. ALIKED normalizes its keypoints as x / (W - 1) * 2 - 1, which is the
// true convention, and mixing them costs half a pixel at the border.
[shader("compute")]
[numthreads(WG, 1, 1)]
void grid_sample_points(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                        uniform GridSampleParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.N * p.C) return;
    uint c = i % p.C;
    uint n = i / p.C;

    float gx = asfloat(p.pos[2u * n]);
    float gy = asfloat(p.pos[2u * n + 1u]);
    float sx, sy;
    if (kAlignCorners != 0) {
        sx = (gx + 1.0f) * 0.5f * (float(p.W) - 1.0f);
        sy = (gy + 1.0f) * 0.5f * (float(p.H) - 1.0f);
    } else {
        sx = ((gx + 1.0f) * float(p.W) - 1.0f) * 0.5f;
        sy = ((gy + 1.0f) * float(p.H) - 1.0f) * 0.5f;
    }
    store_at(p.out, i, sample_bilinear_zero(p.x, kInputF16 != 0, int(p.H), int(p.W),
                                           int(p.C), int(c), sy, sx));
}

// ---------------------------------------------------------------------------
// Row-wise L2 normalization
// ---------------------------------------------------------------------------

struct NormalizeParams {
    float* out;   // [rows, cols]
    uint*  x;
    uint   rows;
    uint   cols;
    float  eps;
    uint   groups_per_row;
};

// out[r] = x[r] / max(||x[r]||, eps) -- torch's F.normalize(p=2, dim=-1).
//
// One workgroup per row with a groupshared tree reduction, deliberately not
// wave intrinsics: a descriptor that differs in its last bits between a wave32
// and a wave64 device would make match indices device-dependent, and the whole
// reconstruction with them.
groupshared float g_norm_partial[WG];

[shader("compute")]
[numthreads(WG, 1, 1)]
void l2_normalize_rows(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                       uniform NormalizeParams p) {
    uint row = gid.y * p.groups_per_row + gid.x;
    uint t = tid.x;

    // Out-of-range rows must still reach every barrier below.
    float acc = 0.0f;
    if (row < p.rows)
        for (uint c = t; c < p.cols; c += uint(WG)) {
            float v = load_elem(p.x, row * p.cols + c, kInputF16 != 0);
            acc += v * v;
        }
    g_norm_partial[t] = acc;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) g_norm_partial[t] += g_norm_partial[t + s];
        GroupMemoryBarrierWithGroupSync();
    }
    if (row >= p.rows) return;

    float inv = 1.0f / max(sqrt(g_norm_partial[0]), p.eps);
    for (uint c = t; c < p.cols; c += uint(WG)) {
        float v = load_elem(p.x, row * p.cols + c, kInputF16 != 0);
        p.out[row * p.cols + c] = v * inv;
    }
}

// out[r] = softmax(x[r]) over the last dimension -- and on a channel-last
// [H, W, C] map that IS torch's softmax(dim=1) over channels, with no permute.
// Metric3D's depth regressor takes it over 256 bins per pixel and its convex
// upsampler over 9 neighbours per output pixel.
//
// Two passes over the row (max, then exp-sum) rather than an online rescale:
// the reduction is a groupshared tree for the same reason l2_normalize_rows'
// is, and an online form would make the order wave-width dependent.
groupshared float g_soft_partial[WG];

[shader("compute")]
[numthreads(WG, 1, 1)]
void softmax_rows(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                  uniform NormalizeParams p) {
    uint row = gid.y * p.groups_per_row + gid.x;
    uint t = tid.x;

    float hi = -3.402823466e+38f;
    if (row < p.rows)
        for (uint c = t; c < p.cols; c += uint(WG))
            hi = max(hi, load_elem(p.x, row * p.cols + c, kInputF16 != 0));
    g_soft_partial[t] = hi;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) g_soft_partial[t] = max(g_soft_partial[t], g_soft_partial[t + s]);
        GroupMemoryBarrierWithGroupSync();
    }
    float m = g_soft_partial[0];
    GroupMemoryBarrierWithGroupSync();

    float acc = 0.0f;
    if (row < p.rows)
        for (uint c = t; c < p.cols; c += uint(WG))
            acc += exp(load_elem(p.x, row * p.cols + c, kInputF16 != 0) - m);
    g_soft_partial[t] = acc;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) g_soft_partial[t] += g_soft_partial[t + s];
        GroupMemoryBarrierWithGroupSync();
    }
    if (row >= p.rows) return;

    float inv = 1.0f / g_soft_partial[0];
    for (uint c = t; c < p.cols; c += uint(WG)) {
        float v = load_elem(p.x, row * p.cols + c, kInputF16 != 0);
        p.out[row * p.cols + c] = exp(v - m) * inv;
    }
}

// ---------------------------------------------------------------------------
// Swin patch merging
// ---------------------------------------------------------------------------

struct PatchMergeParams {
    float* out;   // [ceil(H/2) * ceil(W/2), 4*C]
    uint*  x;     // [H, W, C]
    uint   H;
    uint   W;
    uint   C;
    uint   groups_per_row;
};

// torch.cat([x[0::2, 0::2], x[1::2, 0::2], x[0::2, 1::2], x[1::2, 1::2]], -1)
// with an odd edge zero-padded -- the ROW offset varies fastest, which is the
// order Swin's reduction weight was trained against.
[shader("compute")]
[numthreads(WG, 1, 1)]
void patch_merge(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                 uniform PatchMergeParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    const uint h2 = (p.H + 1u) / 2u, w2 = (p.W + 1u) / 2u;
    if (i >= h2 * w2 * 4u * p.C) return;
    const uint c4 = i % (4u * p.C);
    const uint pos = i / (4u * p.C);
    const uint q = c4 / p.C, c = c4 % p.C;
    const uint y = (pos / w2) * 2u + (q & 1u);
    const uint x = (pos % w2) * 2u + (q >> 1);
    store_at(p.out, i, (y < p.H && x < p.W)
                           ? load_at(p.x, (y * p.W + x) * p.C + c, kInputF16 != 0)
                           : 0.0f);
}

// ---------------------------------------------------------------------------
// Blocks to channels
// ---------------------------------------------------------------------------

struct BlocksParams {
    float* out;   // [H/gh, W/gw, C*gh*gw]
    uint*  x;     // [H, W, C]
    uint   H;
    uint   W;
    uint   C;
    uint   gh;
    uint   gw;
    uint   groups_per_row;
    uint   column_major;   // channel = (gx*gh + gy)*C + c, else (c*gh + gy)*gw + gx
    uint   _pad0;
};

// The image cut into a gh x gw grid of blocks, each block's pixels stacked as
// channels; unlike pixel_unshuffle, neighbouring outputs read neighbouring
// inputs. The two channel orders are nn::BlockOrder's.
[shader("compute")]
[numthreads(WG, 1, 1)]
void blocks_to_channels(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                        uniform BlocksParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    const uint ph = p.H / p.gh, pw = p.W / p.gw;
    const uint K = p.C * p.gh * p.gw;
    if (i >= ph * pw * K) return;
    const uint k = i % K, pos = i / K;
    uint c, gy, gx;
    if (p.column_major != 0u) {
        c = k % p.C;
        gy = (k / p.C) % p.gh;
        gx = (k / p.C) / p.gh;
    } else {
        c = k / (p.gh * p.gw);
        gy = (k % (p.gh * p.gw)) / p.gw;
        gx = k % p.gw;
    }
    const uint y = gy * ph + pos / pw;
    const uint x = gx * pw + pos % pw;
    store_at(p.out, i, load_at(p.x, (y * p.W + x) * p.C + c, kInputF16 != 0));
}

// ---------------------------------------------------------------------------
// Multi-scale deformable attention
// ---------------------------------------------------------------------------

struct MsDeformParams {
    float* out;      // [nq, n_heads * head_dim]
    uint*  value;    // [sum(H_l * W_l), n_heads * head_dim] f32
    uint*  offsets;  // [nq, n_heads, n_levels, n_points, 2] (x, y)
    uint*  attn;     // [nq, n_heads, n_levels * n_points] logits, pre-softmax
    uint*  refs;     // [nq, ref_dims]: (x, y) or (cx, cy, w, h), normalized
    uint   nq;
    uint   n_heads;
    uint   head_dim;
    uint   n_levels;
    uint   n_points;
    uint   ref_dims;
    uint   hw0, hw1, hw2, hw3;   // (H << 16) | W per level
    uint   st0, st1, st2, st3;   // first value row of each level
    uint   groups_per_row;
    uint   _pad0;
};

float msda_tap(uint* value, uint row0, int H, int W, uint C, uint c, float y, float x) {
    if (y <= -1.0f || float(H) <= y || x <= -1.0f || float(W) <= x) return 0.0f;
    const int y0 = int(floor(y)), x0 = int(floor(x));
    const float ly = y - float(y0), lx = x - float(x0);
    float v = 0.0f;
    if (y0 >= 0 && x0 >= 0)
        v += (1.0f - ly) * (1.0f - lx) * asfloat(load_u32_at(value, (row0 + uint(y0 * W + x0)) * C + c));
    if (y0 >= 0 && x0 + 1 < W)
        v += (1.0f - ly) * lx * asfloat(load_u32_at(value, (row0 + uint(y0 * W + x0 + 1)) * C + c));
    if (y0 + 1 < H && x0 >= 0)
        v += ly * (1.0f - lx) * asfloat(load_u32_at(value, (row0 + uint((y0 + 1) * W + x0)) * C + c));
    if (y0 + 1 < H && x0 + 1 < W)
        v += ly * lx * asfloat(load_u32_at(value, (row0 + uint((y0 + 1) * W + x0 + 1)) * C + c));
    return v;
}

// Deformable DETR's MultiScaleDeformableAttention, softmax included. Sampling
// is grid_sample(align_corners=False, zeros): pixel = loc * size - 0.5; a 4-d
// reference scales the offset by off / n_points * wh * 0.5, a 2-d one by 1/(W, H).
[shader("compute")]
[numthreads(WG, 1, 1)]
void ms_deform_attn(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                    uniform MsDeformParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    const uint C = p.n_heads * p.head_dim;
    if (i >= p.nq * C) return;
    const uint d = i % p.head_dim;
    const uint qh = i / p.head_dim;   // q * n_heads + h
    const uint h = qh % p.n_heads;
    const uint q = qh / p.n_heads;
    const uint LP = p.n_levels * p.n_points;

    float m = -3.402823466e+38f;
    for (uint j = 0; j < LP; ++j) m = max(m, asfloat(p.attn[qh * LP + j]));
    float sum = 0.0f;
    for (uint j = 0; j < LP; ++j) sum += exp(asfloat(p.attn[qh * LP + j]) - m);
    const float inv = 1.0f / sum;

    const float rx = asfloat(p.refs[q * p.ref_dims + 0u]);
    const float ry = asfloat(p.refs[q * p.ref_dims + 1u]);
    const float rw = (p.ref_dims == 4u) ? asfloat(p.refs[q * p.ref_dims + 2u]) : 0.0f;
    const float rh = (p.ref_dims == 4u) ? asfloat(p.refs[q * p.ref_dims + 3u]) : 0.0f;

    float acc = 0.0f;
    for (uint l = 0; l < p.n_levels; ++l) {
        const uint hw = (l == 0u) ? p.hw0 : (l == 1u) ? p.hw1 : (l == 2u) ? p.hw2 : p.hw3;
        const uint st = (l == 0u) ? p.st0 : (l == 1u) ? p.st1 : (l == 2u) ? p.st2 : p.st3;
        const int H = int(hw >> 16), W = int(hw & 0xFFFFu);
        for (uint pt = 0; pt < p.n_points; ++pt) {
            const uint j = l * p.n_points + pt;
            const uint ob = (qh * LP + j) * 2u;
            const float ox = asfloat(p.offsets[ob + 0u]);
            const float oy = asfloat(p.offsets[ob + 1u]);
            float lx, ly;
            if (p.ref_dims == 4u) {
                lx = rx + ox / float(p.n_points) * rw * 0.5f;
                ly = ry + oy / float(p.n_points) * rh * 0.5f;
            } else {
                lx = rx + ox / float(W);
                ly = ry + oy / float(H);
            }
            const float w = exp(asfloat(p.attn[qh * LP + j]) - m) * inv;
            acc += w * msda_tap(p.value, st, H, W, C, h * p.head_dim + d,
                                ly * float(H) - 0.5f, lx * float(W) - 0.5f);
        }
    }
    store_at(p.out, i, acc);
}
