// LoMa's model-specific kernels. Everything else in the three forward passes
// is a general op from nn/ -- convolution, pooling, resize, grid sample, GEMM,
// softmax, patch gather -- including DaD's sub-pixel refinement, which is a
// patch_gather plus a softmax plus a 2x9 linear and needed no kernel at all.
//
// What is left is one suppression rule and one assignment reduction, and both
// are here for the same reason ALIKED's are: they are reductions over
// something that should never be materialized.

#include "_common.slang"

// Spec ids 0..1.
[SpecializationConstant] const int kInputF16 = 0;
[SpecializationConstant] const int kAct      = 0;

static const int WG = 256;

struct PeakParams {
    uint*  out;       // [cap, 3]: x, y (u32) and the probability's bits
    uint*  counter;   // [1]
    uint*  probs;     // [H, W] f32, the softmax over the whole map
    uint   H;
    uint   W;
    float  min_score;
    uint   cap;
    uint   groups_per_row;
};

// DaD's NMS is the whole of `probs * (probs == max_pool(probs, 3))` -- no
// suppression rounds, unlike ALIKED's -- so the mask never has to exist and
// the compaction can share the pass that computes it.
[shader("compute")]
[numthreads(WG, 1, 1)]
void peak_collect(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                  uniform PeakParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.H * p.W) return;
    float s = asfloat(p.probs[i]);
    if (s < p.min_score) return;

    int x = int(i % p.W), y = int(i / p.W);
    float best = -3.402823466e+38f;
    for (int dy = -1; dy <= 1; ++dy) {
        int yy = y + dy;
        if (yy < 0 || yy >= int(p.H)) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            int xx = x + dx;
            if (xx < 0 || xx >= int(p.W)) continue;
            best = max(best, asfloat(p.probs[uint(yy) * p.W + uint(xx)]));
        }
    }
    if (s != best) return;

    uint slot;
    InterlockedAdd(p.counter[0], 1u, slot);
    if (slot >= p.cap) return;   // the host reads the counter and warns
    p.out[slot * 3u + 0u] = uint(x);
    p.out[slot * 3u + 1u] = uint(y);
    p.out[slot * 3u + 2u] = p.probs[i];
}

struct AssignParams {
    uint*  out_idx;     // [n0] i32, the column each row matched, -1 for none
    float* out_score;   // [n0]
    uint*  sim;         // [n0, n1] f32
    uint*  lse1;        // [n1] log-sum-exp down the column
    uint*  argmax1;     // [n1] i32, each column's best row
    uint   n0;
    uint   n1;
    float  min_score;
    uint   groups_per_row;
};

groupshared float g_part[WG];
groupshared float g_idx[WG];

// One THREAD per column, not one workgroup: consecutive threads then read
// consecutive elements of a row, where a workgroup per column would have each
// lane 8 KB from its neighbour. Costs the whole matrix once, coalesced.
[shader("compute")]
[numthreads(WG, 1, 1)]
void lse_cols(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
              uniform AssignParams p) {
    uint col = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (col >= p.n1) return;

    // Online: one pass keeping (max, sum of exp(x - max)) and rescaling the
    // sum whenever the max moves. The arg-max comes free with it.
    float m = -3.402823466e+38f;
    float acc = 0.0f;
    uint  best = 0u;
    for (uint r = 0; r < p.n0; ++r) {
        float v = asfloat(p.sim[r * p.n1 + col]);
        if (v > m) {
            acc = acc * exp(m - v) + 1.0f;
            m = v;
            best = r;
        } else {
            acc += exp(v - m);
        }
    }
    p.lse1[col] = asuint(m + log(acc));
    p.argmax1[col] = best;
}

// softmax(sim, 2) * softmax(sim, 1) is exp(2*sim - lse_row - lse_col), so both
// softmaxes are vectors and the score matrix never exists. lse_row cannot
// change which column wins, so it and the arg-max come out of one pass.
[shader("compute")]
[numthreads(WG, 1, 1)]
void assign_rows(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                 uniform AssignParams p) {
    uint row = gid.y * p.groups_per_row + gid.x;
    uint t = tid.x;
    bool live = row < p.n0;

    float m = -3.402823466e+38f, acc = 0.0f;
    float bv = -3.402823466e+38f;
    uint  bi = 0u;
    if (live)
        for (uint c = t; c < p.n1; c += uint(WG)) {
            float v = asfloat(p.sim[row * p.n1 + c]);
            if (v > m) { acc = acc * exp(m - v) + 1.0f; m = v; }
            else       { acc += exp(v - m); }
            float sc = 2.0f * v - asfloat(p.lse1[c]);
            if (sc > bv) { bv = sc; bi = c; }
        }

    // Merge the per-thread (max, sum) pairs, then the per-thread arg-max.
    g_part[t] = m;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) g_part[t] = max(g_part[t], g_part[t + s]);
        GroupMemoryBarrierWithGroupSync();
    }
    float M = g_part[0];
    GroupMemoryBarrierWithGroupSync();
    g_part[t] = (m == -3.402823466e+38f) ? 0.0f : acc * exp(m - M);
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) g_part[t] += g_part[t + s];
        GroupMemoryBarrierWithGroupSync();
    }
    float lse0 = M + log(g_part[0]);
    GroupMemoryBarrierWithGroupSync();

    g_part[t] = bv;
    g_idx[t] = float(bi);
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s && (g_part[t + s] > g_part[t] ||
                      (g_part[t + s] == g_part[t] && g_idx[t + s] < g_idx[t]))) {
            g_part[t] = g_part[t + s];
            g_idx[t] = g_idx[t + s];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (!live || t != 0u) return;

    int  best = int(g_idx[0]);
    // exp() of the log score: LoMa thresholds the probability, not its log.
    float score = exp(g_part[0] - lse0);
    bool ok = p.argmax1[uint(best)] == row && score > p.min_score;
    p.out_idx[row] = asuint(ok ? best : -1);
    p.out_score[row] = ok ? score : 0.0f;
}
