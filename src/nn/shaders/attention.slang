// Fused ("flash") scaled dot-product attention. The score matrix is never
// materialized -- it cannot be: SAM 3's ViT runs 5184 queries against 5184 keys
// over 16 heads, which is 1.7 GiB of scores per block, and the memory attention
// runs 5184 queries against 36 288 memory tokens. Streaming K/V with an online
// softmax keeps the whole thing in registers and shared memory.
//
// Layout (see nn/Tensor.h): q/k/v are token-major [.., N, n_heads * head_dim],
// which is exactly what a fused qkv projection produces -- so nothing has to be
// permuted between the Linear and the attention, unlike the ggml original.
// A row stride is passed separately so q/k/v can be *slices* of one fused
// [N, 3*E] buffer with no copy.
//
// Tiling. One workgroup of 256 threads, viewed as a 16 x 16 grid, owns BR = 64
// queries of one (head, batch) and streams the keys in BC = 32 wide tiles.
// Both matmuls are register-blocked, which is the point of this shape:
//
//   phase        thread owns              shared reads : FMAs
//   Q @ K^T      4 queries x 2 keys            6 : 8
//   P @ V        4 queries x ceil(HD/16) dims  4 : 4*ceil(HD/16)
//
// head_dim need not divide 16: SAM 2's Hiera runs 96 (tiny/small) and 72
// (large/b+). The tail is handled by staging zeros and skipping the loads,
// which costs the ragged case a few idle lanes and everyone else nothing.
//
// The first version of this kernel gave each thread four *scalar* score slots
// and read both operands from shared for every FMA (2:1), and ran the softmax
// on 32 of the 256 threads with the rest parked at a barrier. Fixing those two
// things halved the kernel's time.
//
// Going further did not help, and the negative result is worth recording: a
// 64x64 key tile with 4x4 score registers and fp16 probabilities cuts the
// shared traffic per FMA by a further 1.7x and measured *exactly the same* on
// an RTX 3070 (2.99 vs 2.93 TFLOP/s on the ViT window shape, 1.61 vs 1.63 on
// the memory-attention shape). So this kernel is not bound by shared bandwidth
// the way gemm_nt_big is -- it is bound by latency and occupancy, and the lever
// that would move it is more workgroups, not fewer shared reads. The memory
// attention is the case that shows it: one head over 5184 queries is 81
// workgroups on 46 SMs, and the identical kernel with 16 heads to spread over
// runs 1.6x faster per FLOP. Splitting the key range across workgroups
// (flash-decoding) is the fix if that path ever needs one; it costs a partial-
// output buffer and a combine pass.
//
// Shared budget: 1040 + 528 + 2112 + 192 floats = 15.1 KiB, inside the 16 KiB
// Vulkan floor. sK doubles as the row-reduction scratch, which is dead space
// during the softmax and exactly the size the reduction needs.

#include "_common.slang"

// Spec ids 0..4, module-wide declaration order. OpAttention.cpp always passes
// all five.
[SpecializationConstant] const int kHeadDim  = 64;
[SpecializationConstant] const int kBiasMode = 0;  // 0 none, 1 per-key, 2 full, 3 causal, 4 window
[SpecializationConstant] const int kQF16     = 0;
[SpecializationConstant] const int kKVF16    = 0;
// Flash-decoding: each workgroup takes a slice of the key range and writes an
// unnormalized accumulator plus its running (max, sum), which flash_attn_combine
// merges. See the note above about occupancy for why this exists.
[SpecializationConstant] const int kSplitK   = 0;

static const int BIAS_NONE   = 0;
static const int BIAS_PERKEY = 1;  // bias[nk], broadcast over queries and heads
static const int BIAS_FULL   = 2;  // bias[n_heads][nq][nk]
static const int BIAS_CAUSAL = 3;  // key j attends only if j <= i
// Swin's shifted windows: bias[n_heads][nq][nk] (the relative position table,
// shared by every window) plus -100 where labels[batch][q] != labels[batch][k].
static const int BIAS_WINDOW = 4;

static const int WG = 256;
static const int TX = 16;         // threads along the key / dim axis
static const int TY = WG / TX;    // threads along the query axis
static const int BR = 64;         // queries per workgroup
static const int BC = 32;         // keys per tile
static const int CH = 16;         // head-dim chunk staged for Q and K
static const int RS = BR / TY;    // 4 query rows per thread
static const int CS = BC / TX;    // 2 keys per thread
static const int CG = BC / 4;     // softmax columns per thread

// Dims per thread in the P @ V accumulation, rounded up: Hiera's head dim is
// 96 (tiny/small) or 72 (large/b+), neither a multiple of TX. kHeadDim is a
// specialization constant, hence the fixed-size arrays at the 256-dim maximum;
// the driver resolves the bound at pipeline creation and unrolls.
//
// The two EXACT flags below are what keep the ragged case free for everyone
// else: they are compile-time-constant after specialization, so on an aligned
// head dim the driver folds every bounds test away and this is byte for byte
// the kernel that ran before.
static const int  DPT       = (kHeadDim + TX - 1) / TX;
static const int  MAXDPT    = 256 / TX;
static const bool DIM_EXACT = (kHeadDim % TX) == 0;   // dcol + j always in range
static const bool CHK_EXACT = (kHeadDim % CH) == 0;   // every staged chunk is full

// +1 on the fast axis of every tile keeps the staging stores and the strided
// reads off a single bank.
groupshared float sQ[CH * (BR + 1)];
groupshared float sK[CH * (BC + 1)];   // >= WG floats, reused as reduction scratch
groupshared float sP[BR * (BC + 1)];
groupshared float sM[BR];      // running row max
groupshared float sL[BR];      // running row sum
groupshared float sCorr[BR];   // exp(m_old - m_new); < 0 marks a fully masked tile

struct AttnParams {
    // When kSplitK, `out` is the [n_splits, nq, n_heads*head_dim] partial
    // accumulator instead (unnormalized) and out_stride is n_heads*head_dim.
    float* out;    // [B, nq, out_stride]
    uint*  q;      // [B, nq, q_stride]
    uint*  k;      // [B, nk, k_stride]
    uint*  v;      // [B, nk, v_stride]
    uint*  bias;   // f32; never null (launchers substitute a zeroed block)
    float* part_ml;  // kSplitK only: [n_splits, n_heads, nq, 2] = (max, sum)
    uint*  labels;   // BIAS_WINDOW only: [batch, nq] region ids; never null
    uint   nq;
    uint   nk;
    uint   n_heads;
    float  scale;
    uint   q_stride;
    uint   k_stride;
    uint   v_stride;
    uint   out_stride;
    uint   bias_stride_h;  // elements between heads   (BIAS_FULL)
    uint   bias_stride_q;  // elements between queries (BIAS_FULL)
    uint   keys_per_split;  // kSplitK only; a multiple of BC
    uint   n_splits;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void flash_attn(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                uniform AttnParams p) {
    const uint t = tid.x;
    const uint tx = t & uint(TX - 1);   // key / dim lane
    const uint ty = t >> 4;             // query lane
    const uint head = gid.y;
    // Vulkan has three grid dimensions and this kernel wants four, so the split
    // index rides on z. The launcher only ever splits when batch == 1.
    const uint batch = (kSplitK != 0) ? 0u : gid.z;
    const uint split = (kSplitK != 0) ? gid.z : 0u;

    const uint q0 = gid.x * uint(BR);
    if (q0 >= p.nq) return;   // uniform: no barrier has been reached yet

    // Key range this workgroup owns. Without kSplitK it is the whole thing.
    const uint j_begin = (kSplitK != 0) ? split * p.keys_per_split : 0u;
    const uint j_end = (kSplitK != 0) ? min(p.nk, j_begin + p.keys_per_split) : p.nk;

    const uint HD = uint(kHeadDim);
    const uint head_off = head * HD;
    const uint qbase = batch * p.nq * p.q_stride + head_off;
    const uint kbase = batch * p.nk * p.k_stride + head_off;
    const uint vbase = batch * p.nk * p.v_stride + head_off;
    const uint obase = (kSplitK != 0)
                           ? (split * p.nq * p.out_stride + head_off)
                           : (batch * p.nq * p.out_stride + head_off);

    const uint qrow = ty * uint(RS);   // this thread's first query, block-local
    const uint kcol = tx * uint(CS);   // this thread's first key,   tile-local
    const uint dcol = tx * uint(DPT);  // this thread's first output dim

    if (t < uint(BR)) {
        sM[t] = -1e30f;
        sL[t] = 0.0f;
    }

    float acc[RS][MAXDPT];
    [unroll] for (int i = 0; i < RS; ++i)
        for (int j = 0; j < DPT; ++j) acc[i][j] = 0.0f;

    // Softmax lane assignment: 64 rows x 4 column groups of CG keys each.
    const uint sr = t >> 2;
    const uint sc0 = (t & 3u) * uint(CG);

    GroupMemoryBarrierWithGroupSync();

    for (uint j0 = j_begin; j0 < j_end; j0 += uint(BC)) {
        // ---- (a) S = Q @ K^T, accumulated over head-dim chunks -------------
        float sc[RS][CS];
        [unroll] for (int i = 0; i < RS; ++i)
            [unroll] for (int j = 0; j < CS; ++j) sc[i][j] = 0.0f;

        for (uint c0 = 0; c0 < HD; c0 += uint(CH)) {
            // Stage Q: BR*CH elements, CH consecutive along the head dim per
            // thread group, so the global reads coalesce into 64-byte runs.
            [unroll] for (uint u = 0; u < uint((BR * CH) / WG); ++u) {
                const uint idx = u * uint(WG) + t;
                const uint qi = idx / uint(CH);
                const uint cc = idx % uint(CH);
                const uint gq = q0 + qi;
                // A dim past the head is staged as zero: it contributes nothing
                // to the dot product, so a ragged head dim needs no masking
                // beyond not reading out of bounds.
                sQ[cc * uint(BR + 1) + qi] =
                    (gq < p.nq && (CHK_EXACT || c0 + cc < HD))
                        ? load_elem(p.q, qbase + gq * p.q_stride + c0 + cc, kQF16 != 0)
                        : 0.0f;
            }
            [unroll] for (uint u = 0; u < uint((BC * CH) / WG); ++u) {
                const uint idx = u * uint(WG) + t;
                const uint kj = idx / uint(CH);
                const uint cc = idx % uint(CH);
                const uint gk = j0 + kj;
                sK[cc * uint(BC + 1) + kj] =
                    (gk < j_end && (CHK_EXACT || c0 + cc < HD))
                        ? load_elem(p.k, kbase + gk * p.k_stride + c0 + cc, kKVF16 != 0)
                        : 0.0f;
            }
            GroupMemoryBarrierWithGroupSync();

            [unroll] for (uint cc = 0; cc < uint(CH); ++cc) {
                // sQ is a 2-way broadcast inside a warp (ty takes two values);
                // sK is stride-CS over 16 lanes, which is conflict-free.
                float a[RS], b[CS];
                [unroll] for (int i = 0; i < RS; ++i)
                    a[i] = sQ[cc * uint(BR + 1) + qrow + uint(i)];
                [unroll] for (int j = 0; j < CS; ++j)
                    b[j] = sK[cc * uint(BC + 1) + kcol + uint(j)];
                [unroll] for (int i = 0; i < RS; ++i)
                    [unroll] for (int j = 0; j < CS; ++j) sc[i][j] += a[i] * b[j];
            }
            GroupMemoryBarrierWithGroupSync();
        }

        // ---- (b) scale, bias / mask, publish to sP -------------------------
        [unroll] for (int i = 0; i < RS; ++i) {
            const uint gq = q0 + qrow + uint(i);
            [unroll] for (int j = 0; j < CS; ++j) {
                const uint gk = j0 + kcol + uint(j);
                float v = sc[i][j] * p.scale;
                if (gk >= j_end || gq >= p.nq) {
                    v = -1e30f;
                } else if (kBiasMode == BIAS_PERKEY) {
                    v += asfloat(p.bias[gk]);
                } else if (kBiasMode == BIAS_FULL) {
                    v += asfloat(
                        p.bias[head * p.bias_stride_h + gq * p.bias_stride_q + gk]);
                } else if (kBiasMode == BIAS_CAUSAL) {
                    if (gk > gq) v = -1e30f;
                } else if (kBiasMode == BIAS_WINDOW) {
                    v += asfloat(
                        p.bias[head * p.bias_stride_h + gq * p.bias_stride_q + gk]);
                    if (p.labels[batch * p.nq + gq] != p.labels[batch * p.nq + gk])
                        v -= 100.0f;
                }
                sP[(qrow + uint(i)) * uint(BC + 1) + kcol + uint(j)] = v;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        // ---- (c) online softmax, all 256 threads ---------------------------
        {
            float m = -1e30f;
            for (uint u = 0; u < uint(CG); ++u)
                m = max(m, sP[sr * uint(BC + 1) + sc0 + u]);
            sK[t] = m;
        }
        GroupMemoryBarrierWithGroupSync();

        if (t < uint(BR)) {
            const uint b = t * 4u;
            const float rm = max(max(sK[b], sK[b + 1]), max(sK[b + 2], sK[b + 3]));
            if (rm <= -1e29f) {
                // Every key in this tile is masked (padding tail, or a causal
                // row that starts later). The sentinel keeps the running max and
                // sum untouched -- taking the normal path would turn
                // exp(-1e30 - -1e30) into exp(0) = 1 and inject BC phantom keys.
                sCorr[t] = -1.0f;
            } else {
                const float m_new = max(sM[t], rm);
                sCorr[t] = (sM[t] <= -1e29f) ? 0.0f : exp(sM[t] - m_new);
                sM[t] = m_new;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        {
            float s = 0.0f;
            if (sCorr[sr] < 0.0f) {
                for (uint u = 0; u < uint(CG); ++u)
                    sP[sr * uint(BC + 1) + sc0 + u] = 0.0f;
            } else {
                const float m = sM[sr];
                for (uint u = 0; u < uint(CG); ++u) {
                    const uint o = sr * uint(BC + 1) + sc0 + u;
                    const float e = exp(sP[o] - m);
                    sP[o] = e;
                    s += e;
                }
            }
            sK[t] = s;
        }
        GroupMemoryBarrierWithGroupSync();

        if (t < uint(BR)) {
            const uint b = t * 4u;
            const float rs = sK[b] + sK[b + 1] + sK[b + 2] + sK[b + 3];
            sL[t] = sL[t] * ((sCorr[t] < 0.0f) ? 1.0f : sCorr[t]) + rs;
        }
        GroupMemoryBarrierWithGroupSync();

        // ---- (d) O = O * corr + P @ V --------------------------------------
        [unroll] for (int i = 0; i < RS; ++i) {
            const float c = sCorr[qrow + uint(i)];
            const float cs_ = (c < 0.0f) ? 1.0f : c;
            for (int j = 0; j < DPT; ++j) acc[i][j] *= cs_;
        }
        for (uint jj = 0; jj < uint(BC); ++jj) {
            const uint gk = j0 + jj;
            if (gk >= j_end) break;   // uniform across the workgroup
            // dcol..dcol+DPT-1 over 16 lanes spans head_dim contiguously, so
            // one warp's V reads are a single coalesced run.
            float vv[MAXDPT];
            for (int j = 0; j < DPT; ++j)
                vv[j] = (DIM_EXACT || dcol + uint(j) < HD)
                            ? load_elem(p.v, vbase + gk * p.v_stride + dcol + uint(j),
                                        kKVF16 != 0)
                            : 0.0f;
            [unroll] for (int i = 0; i < RS; ++i) {
                const float pp = sP[(qrow + uint(i)) * uint(BC + 1) + jj];
                for (int j = 0; j < DPT; ++j) acc[i][j] += pp * vv[j];
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // ---- normalize and write ---------------------------------------------
    [unroll] for (int i = 0; i < RS; ++i) {
        const uint gq = q0 + qrow + uint(i);
        if (gq >= p.nq) continue;
        const float l = sL[qrow + uint(i)];
        // Under kSplitK the accumulator stays unnormalized: the combine pass
        // rescales each slice against the global row maximum, which it cannot
        // do once the division has happened.
        const float sc_out = (kSplitK != 0) ? 1.0f : ((l > 0.0f) ? (1.0f / l) : 0.0f);
        for (int j = 0; j < DPT; ++j)
            if (DIM_EXACT || dcol + uint(j) < HD)
                p.out[obase + gq * p.out_stride + dcol + uint(j)] = acc[i][j] * sc_out;
    }
    if (kSplitK != 0 && t < uint(BR)) {
        const uint gq = q0 + t;
        if (gq < p.nq) {
            // A slice whose keys were all masked reports its sentinel maximum,
            // and the combine weights it by exp(-inf) = 0.
            const uint o = ((split * p.n_heads + head) * p.nq + gq) * 2u;
            p.part_ml[o] = sM[t];
            p.part_ml[o + 1u] = sL[t];
        }
    }
}

// ---------------------------------------------------------------------------
// Flash-decoding combine
// ---------------------------------------------------------------------------

struct CombineParams {
    float* out;      // [nq, out_stride]
    uint*  part;     // [n_splits, nq, n_heads*head_dim] f32, unnormalized
    uint*  part_ml;  // [n_splits, n_heads, nq, 2] f32
    uint   nq;
    uint   n_heads;
    uint   n_splits;
    uint   out_stride;
    uint   groups_per_row;
    uint   _pad0;
    uint   _pad1;
};

// One thread per output element. Re-reading each slice's (max, sum) per element
// costs 2*n_splits scalar loads that land in L1, which is cheaper than a
// separate pass to precompute the weights and a barrier to wait on it.
[shader("compute")]
[numthreads(256, 1, 1)]
void flash_attn_combine(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                        uniform CombineParams p) {
    const uint i = fold_index(gid, tid, p.groups_per_row, 256u);
    const uint HD = uint(kHeadDim);
    const uint dim = p.n_heads * HD;
    if (i >= p.nq * dim) return;
    const uint d = i % dim;
    const uint gq = i / dim;
    const uint head = d / HD;

    float m = -1e30f;
    for (uint s = 0; s < p.n_splits; ++s)
        m = max(m, asfloat(p.part_ml[((s * p.n_heads + head) * p.nq + gq) * 2u]));

    float num = 0.0f, den = 0.0f;
    for (uint s = 0; s < p.n_splits; ++s) {
        const uint o = ((s * p.n_heads + head) * p.nq + gq) * 2u;
        const float ls = asfloat(p.part_ml[o + 1u]);
        if (ls <= 0.0f) continue;
        const float w = exp(asfloat(p.part_ml[o]) - m);
        den += w * ls;
        num += w * asfloat(p.part[(s * p.nq + gq) * dim + d]);
    }
    p.out[gq * p.out_stride + d] = (den > 0.0f) ? (num / den) : 0.0f;
}
