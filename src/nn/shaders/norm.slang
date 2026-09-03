// Normalization layers.
//
// Feature maps are channel-last ([H, W, C]), so LayerNorm2d over channels is
// literally the same kernel as LayerNorm over a token's features -- one
// reduction over the contiguous last dimension. That is the main reason for the
// layout choice; the ggml original spends a permute on every LayerNorm2d.
//
// The residual add is fused into LayerNorm because the post-norm blocks (DETR
// decoder, SAM two-way decoder) always do `x = LN(x + sublayer(x))`, and as a
// separate pass that add is pure memory traffic.

#include "_common.slang"

// Spec ids 0..1.
[SpecializationConstant] const int kHasResidual = 0;
[SpecializationConstant] const int kInputF16    = 0;

static const int WG = 256;

groupshared float sSum[WG];
groupshared float sSqs[WG];
groupshared float sMean;
groupshared float sInvStd;

struct LayerNormParams {
    float* out;       // [R, C]
    uint*  x;         // [R, C]
    uint*  weight;    // [C] f32 (never null)
    uint*  bias;      // [C] f32 (never null)
    float* residual;  // [R, C] f32 (never null)
    uint   rows;
    uint   cols;
    float  eps;
    uint   x_row_stride;
    uint   out_row_stride;
    uint   has_affine;      // 0 -> skip weight/bias
    uint   groups_per_row;  // grid fold: workgroups on the x axis
    uint   _pad0;
};

// One workgroup per row. `rows` can exceed the 65535 per-dimension dispatch cap
// (a 288x288 map has 82944 rows), so the grid folds across y.
[shader("compute")]
[numthreads(WG, 1, 1)]
void layer_norm(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                uniform LayerNormParams p) {
    const uint row = gid.y * p.groups_per_row + gid.x;
    if (row >= p.rows) return;
    const uint t = tid.x;
    const uint C = p.cols;
    const uint xoff = row * p.x_row_stride;
    const uint ooff = row * p.out_row_stride;

    float s = 0.0f, q = 0.0f;
    for (uint c = t; c < C; c += uint(WG)) {
        float v = load_elem(p.x, xoff + c, kInputF16 != 0);
        if (kHasResidual != 0) v += p.residual[ooff + c];
        s += v;
        q += v * v;
    }
    sSum[t] = s;
    sSqs[t] = q;
    GroupMemoryBarrierWithGroupSync();
    // Deterministic tree reduction (no wave intrinsics): identical results on
    // wave32 and wave64 devices, which keeps cross-device parity meaningful.
    for (uint stride = WG / 2; stride > 0; stride >>= 1) {
        if (t < stride) {
            sSum[t] += sSum[t + stride];
            sSqs[t] += sSqs[t + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (t == 0) {
        float inv_n = 1.0f / float(C);
        float mean = sSum[0] * inv_n;
        float var = sSqs[0] * inv_n - mean * mean;
        sMean = mean;
        sInvStd = 1.0f / sqrt(max(var, 0.0f) + p.eps);
    }
    GroupMemoryBarrierWithGroupSync();

    const float mean = sMean;
    const float inv_std = sInvStd;
    for (uint c = t; c < C; c += uint(WG)) {
        float v = load_elem(p.x, xoff + c, kInputF16 != 0);
        if (kHasResidual != 0) v += p.residual[ooff + c];
        float n = (v - mean) * inv_std;
        if (p.has_affine != 0)
            n = n * asfloat(p.weight[c]) + asfloat(p.bias[c]);
        p.out[ooff + c] = n;
    }
}

// ---------------------------------------------------------------------------
// GroupNorm over a channel-last [N, C] map (N = H*W), C split into `groups`.
//
// Two passes: one workgroup per group reduces the whole N*C/groups slice, then
// an elementwise pass applies it. The seg head's PixelDecoder is the only user
// (GroupNorm(8, 256) at 144x144 and 288x288), so a grid-stride reduce is
// plenty -- fusing it into the conv would save one 85 MB read at the cost of a
// much less reusable kernel.
// ---------------------------------------------------------------------------

struct GroupNormStatsParams {
    float* stats;   // [groups][2] -> (mean, inv_std)
    uint*  x;       // [N, C]
    uint   n;
    uint   channels;
    uint   groups;
    float  eps;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void group_norm_stats(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                      uniform GroupNormStatsParams p) {
    const uint g = gid.x;
    const uint t = tid.x;
    const uint per_group = p.channels / p.groups;
    const uint c0 = g * per_group;

    float s = 0.0f, q = 0.0f;
    const uint total = p.n * per_group;
    for (uint i = t; i < total; i += uint(WG)) {
        uint row = i / per_group;
        uint c = c0 + (i % per_group);
        float v = load_at(p.x, row * p.channels + c, kInputF16 != 0);
        s += v;
        q += v * v;
    }
    sSum[t] = s;
    sSqs[t] = q;
    GroupMemoryBarrierWithGroupSync();
    for (uint stride = WG / 2; stride > 0; stride >>= 1) {
        if (t < stride) {
            sSum[t] += sSum[t + stride];
            sSqs[t] += sSqs[t + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (t == 0) {
        float inv_n = 1.0f / float(total);
        float mean = sSum[0] * inv_n;
        float var = sSqs[0] * inv_n - mean * mean;
        p.stats[g * 2 + 0] = mean;
        p.stats[g * 2 + 1] = 1.0f / sqrt(max(var, 0.0f) + p.eps);
    }
}

struct GroupNormApplyParams {
    float* out;
    uint*  x;
    float* stats;
    uint*  weight;   // [C] f32
    uint*  bias;     // [C] f32
    uint   n;
    uint   channels;
    uint   groups;
    uint   act;
    uint   groups_per_row;
    uint   _pad0;
    uint   _pad1;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void group_norm_apply(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                      uniform GroupNormApplyParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint total = p.n * p.channels;
    if (i >= total) return;
    uint c = i % p.channels;
    uint g = c / (p.channels / p.groups);
    float mean = p.stats[g * 2 + 0];
    float inv = p.stats[g * 2 + 1];
    float v = (load_at(p.x, i, kInputF16 != 0) - mean) * inv;
    v = v * asfloat(p.weight[c]) + asfloat(p.bias[c]);
    store_at(p.out, i, apply_act(v, int(p.act)));
}
