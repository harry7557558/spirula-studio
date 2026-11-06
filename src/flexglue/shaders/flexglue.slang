// FlexGlue's model-specific kernels: the 1.5-entmax window readout, the
// offset head's binned expectation, and the 4-D mutual nearest neighbour.
// Everything else is a general op from nn/.

#include "_common.slang"

// Spec ids 0..1.
[SpecializationConstant] const int kInputF16 = 0;
[SpecializationConstant] const int kAct      = 0;

static const int WG = 256;
static const int TOPK = 512;   // ops.entmax15_window_readout's k
static const float NEG_BIG = -3.402823466e+38f;

// ---------------------------------------------------------------------------
// entmax15_window_readout(radius=1, k=512), one row per workgroup. Only y > -1
// can be in the support (tau >= -1 since y_max = 0), so only those are sorted;
// past 512 of them the top 512 are found by bisection, as topk would.
// ---------------------------------------------------------------------------

struct ReadoutParams {
    uint*  logits;   // [R, stride] f32, C = h * w used per row
    float* pos;      // [R, 2] normalized (x, y)
    float* mass;     // [R]
    float* cell;     // [R, 2]: the arg-max cell and tau, for parity dumps
    uint   R;
    uint   stride;
    uint   h;
    uint   w;
    uint   groups_per_row;
};

groupshared float g_val[WG];
groupshared uint  g_idx[WG];
groupshared uint  g_count;
groupshared float g_sorted[TOPK];
groupshared float g_tau[TOPK];

float row_logit(ReadoutParams p, uint row, uint c) {
    return asfloat(p.logits[row * p.stride + c]);
}

// #{c : y_c >= t}, by a deterministic tree.
uint count_at_least(ReadoutParams p, uint row, uint t, float vmax, float thr, uint C) {
    uint n = 0u;
    for (uint c = t; c < C; c += uint(WG))
        if ((row_logit(p, row, c) - vmax) / 2.0f >= thr) n++;
    g_idx[t] = n;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) g_idx[t] += g_idx[t + s];
        GroupMemoryBarrierWithGroupSync();
    }
    uint total = g_idx[0];
    GroupMemoryBarrierWithGroupSync();
    return total;
}

[shader("compute")]
[numthreads(WG, 1, 1)]
void readout(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID, uniform ReadoutParams p) {
    uint row = gid.y * p.groups_per_row + gid.x;
    uint t = tid.x;
    bool live = row < p.R;
    uint rr = live ? row : 0u;
    uint C = p.h * p.w;

    // Arg-max, the lowest index on a tie.
    float best = NEG_BIG;
    uint bi = 0u;
    for (uint c = t; c < C; c += uint(WG)) {
        float v = row_logit(p, rr, c);
        if (v > best) { best = v; bi = c; }
    }
    g_val[t] = best;
    g_idx[t] = bi;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1) {
        if (t < s) {
            bool take = g_val[t + s] > g_val[t] ||
                        (g_val[t + s] == g_val[t] && g_idx[t + s] < g_idx[t]);
            if (take) { g_val[t] = g_val[t + s]; g_idx[t] = g_idx[t + s]; }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    float vmax = g_val[0];
    uint idx = g_idx[0];
    GroupMemoryBarrierWithGroupSync();

    // The candidates, y > -1; past 512 of them, the 512 largest.
    float lo = -1.0f;
    float hi = -1.0f;   // collect y > hi unconditionally, then y in (lo, hi]
    uint n_cand = count_at_least(p, rr, t, vmax, -1.0f, C);
    // count_at_least counts y >= thr; y == -1 is never in the support.
    if (n_cand > uint(TOPK)) {
        lo = -1.0f;
        hi = 0.0f;
        for (int it = 0; it < 40; ++it) {
            float mid = 0.5f * (lo + hi);
            if (count_at_least(p, rr, t, vmax, mid, C) >= uint(TOPK)) lo = mid;
            else hi = mid;
        }
    }
    if (t == 0u) g_count = 0u;
    for (uint i = t; i < uint(TOPK); i += uint(WG)) g_sorted[i] = NEG_BIG;
    GroupMemoryBarrierWithGroupSync();
    for (uint c = t; c < C; c += uint(WG)) {
        float y = (row_logit(p, rr, c) - vmax) / 2.0f;
        if (y > -1.0f && (n_cand <= uint(TOPK) || y >= hi)) {
            uint slot;
            InterlockedAdd(g_count, 1u, slot);
            if (slot < uint(TOPK)) g_sorted[slot] = y;
        }
    }
    GroupMemoryBarrierWithGroupSync();
    if (n_cand > uint(TOPK)) {
        for (uint c = t; c < C; c += uint(WG)) {
            float y = (row_logit(p, rr, c) - vmax) / 2.0f;
            if (y >= lo && y < hi) {
                uint slot;
                InterlockedAdd(g_count, 1u, slot);
                if (slot < uint(TOPK)) g_sorted[slot] = y;
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Bitonic sort, descending.
    for (uint k = 2u; k <= uint(TOPK); k <<= 1) {
        for (uint j = k >> 1; j > 0u; j >>= 1) {
            for (uint i = t; i < uint(TOPK); i += uint(WG)) {
                uint l = i ^ j;
                if (l > i) {
                    float a = g_sorted[i], b = g_sorted[l];
                    bool desc = (i & k) == 0u;
                    if (desc ? (a < b) : (a > b)) { g_sorted[i] = b; g_sorted[l] = a; }
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (t != 0u || !live) return;
    uint n = min(g_count, uint(TOPK));
    // sorted_entmax15_threshold, sequentially over rho.
    float s1 = 0.0f, s2 = 0.0f;
    uint support = 0u;
    for (uint r = 0u; r < n; ++r) {
        float ys = g_sorted[r];
        float rho = float(r + 1u);
        s1 += ys;
        s2 += ys * ys;
        float mean = s1 / rho;
        float mean_sq = s2 / rho;
        float delta = (1.0f - rho * (mean_sq - mean * mean)) / rho;
        float tau = mean - sqrt(max(delta, 0.0f));
        g_tau[r] = tau;
        if (tau <= ys) support++;
    }
    float tau = g_tau[max(support, 1u) - 1u];

    int cy = int(idx / p.w), cx = int(idx % p.w);
    int H = int(p.h), W = int(p.w);
    float sp = 0.0f, sx = 0.0f, sy = 0.0f, m = 0.0f;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            int ny = cy + dy, nx = cx + dx;
            bool inside = ny >= 0 && ny < H && nx >= 0 && nx < W;
            ny = clamp(ny, 0, H - 1);
            nx = clamp(nx, 0, W - 1);
            float y = (row_logit(p, rr, uint(ny * W + nx)) - vmax) / 2.0f;
            float q = max(y - tau, 0.0f);
            q = q * q;
            sp += q;
            sx += q * ((float(nx) + 0.5f) / float(W) * 2.0f - 1.0f);
            sy += q * ((float(ny) + 0.5f) / float(H) * 2.0f - 1.0f);
            if (inside) m += q;
        }
    p.pos[2u * row] = sx / sp;
    p.pos[2u * row + 1u] = sy / sp;
    p.mass[row] = m;
    p.cell[2u * row] = float(idx);
    p.cell[2u * row + 1u] = tau;
}

// ---------------------------------------------------------------------------
// OffsetHead's tail and the confidence: per point, the first 17 of each
// axis's 18 bins are softmaxed, their expectation is a sub-cell offset in
// [-1, 1] cells, and conf = sigmoid(matchability) * window mass.
// ---------------------------------------------------------------------------

struct OffsetParams {
    uint*  head;     // [R, 36] f32 logits, (axis, bin)
    uint*  z;        // [R] matchability logit
    uint*  mass;     // [R]
    float* pos;      // [R, 2], read and updated in place
    float* conf;     // [R]
    uint   R;
    uint   h;
    uint   w;
    uint   groups_per_row;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void offset(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID, uniform OffsetParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.R) return;
    float delta[2];
    for (uint a = 0u; a < 2u; ++a) {
        uint base = i * 36u + a * 18u;
        float mx = NEG_BIG;
        for (uint b = 0u; b < 17u; ++b) mx = max(mx, asfloat(p.head[base + b]));
        float se = 0.0f, ex = 0.0f;
        for (uint b = 0u; b < 17u; ++b) {
            float e = exp(asfloat(p.head[base + b]) - mx);
            se += e;
            ex += e * float(b);
        }
        delta[a] = ((ex / se) / 16.0f - 0.5f) * 2.0f;
    }
    p.pos[2u * i] = p.pos[2u * i] + delta[0] * 2.0f / float(p.w);
    p.pos[2u * i + 1u] = p.pos[2u * i + 1u] + delta[1] * 2.0f / float(p.h);
    float z = asfloat(p.z[i]);
    p.conf[i] = (1.0f / (1.0f + exp(-z))) * asfloat(p.mass[i]);
}

// ---------------------------------------------------------------------------
// argmin over the columns of sq[m, n] = sum_d (X[m, d] - Y[n, d])^2, d < 4,
// lowest index on a tie: one thread per row, Y staged through shared memory.
// Run with X and Y swapped for the column argmin; sq is symmetric in them.
// ---------------------------------------------------------------------------

struct NearestParams {
    uint* x;     // [M, 4]
    uint* y;     // [N, 4]
    int*  out;   // [M]
    uint  M;
    uint  N;
    uint  groups_per_row;
};

groupshared float4 g_y[WG];

[shader("compute")]
[numthreads(WG, 1, 1)]
void nearest(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID, uniform NearestParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint t = tid.x;
    bool live = i < p.M;
    float4 a = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (live)
        a = float4(asfloat(p.x[4u * i]), asfloat(p.x[4u * i + 1u]), asfloat(p.x[4u * i + 2u]),
                   asfloat(p.x[4u * i + 3u]));
    float best = 3.402823466e+38f;
    int bi = -1;
    for (uint n0 = 0u; n0 < p.N; n0 += uint(WG)) {
        uint n = n0 + t;
        if (n < p.N)
            g_y[t] = float4(asfloat(p.y[4u * n]), asfloat(p.y[4u * n + 1u]),
                            asfloat(p.y[4u * n + 2u]), asfloat(p.y[4u * n + 3u]));
        GroupMemoryBarrierWithGroupSync();
        uint cnt = min(uint(WG), p.N - n0);
        for (uint k = 0u; k < cnt; ++k) {
            float4 d = a - g_y[k];
            float sq = d.x * d.x;
            sq = sq + d.y * d.y;
            sq = sq + d.z * d.z;
            sq = sq + d.w * d.w;
            if (sq < best) { best = sq; bi = int(n0 + k); }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (live) p.out[i] = bi;
}

