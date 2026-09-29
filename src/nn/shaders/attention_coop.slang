// flash_attn with the Q @ K^T half on tensor cores.
//
// Same algorithm, same shared layout and same softmax as attention.slang --
// read that first; only phase (a) differs, and the comments here cover only the
// difference. Like gemm_coop.slang this is a separate module because it
// declares the CooperativeMatrixKHR capability, which must never reach a device
// that does not advertise the extension.
//
// Why only half. Flash attention's second matmul is `O = O * corr + P @ V`,
// and `corr` is per query row. A cooperative matrix does not say which element
// of the tile a lane holds, so a per-row rescale of an accumulator fragment is
// not expressible in the KHR extension -- and O is rescaled on every key tile.
// The score matrix has no such problem: it already goes through shared memory,
// because the softmax has to read it by (row, column) anyway. So S is computed
// as fragments and stored straight into sP, which is where the scalar kernel
// wrote it too, and everything downstream is unchanged.
//
// Requires head_dim to be a multiple of 16 (the fragment's K). Hiera's 96
// qualifies, its 72 does not, and the launcher sends that one to the scalar
// kernel.

#include "_common.slang"

using namespace linalg;

// Spec ids 0..5, module-wide declaration order. The first five match
// attention.slang; kSgCount is new and is the number of subgroups in a
// 256-thread workgroup at the width the context pinned.
[SpecializationConstant] const int kHeadDim  = 64;
[SpecializationConstant] const int kBiasMode = 0;  // 0 none, 1 per-key, 2 full, 3 causal, 4 window
[SpecializationConstant] const int kQF16     = 0;
[SpecializationConstant] const int kKVF16    = 0;
[SpecializationConstant] const int kSplitK   = 0;
[SpecializationConstant] const int kSgCount  = 8;

static const int BIAS_NONE   = 0;
static const int BIAS_PERKEY = 1;
static const int BIAS_FULL   = 2;
static const int BIAS_CAUSAL = 3;
static const int BIAS_WINDOW = 4;

static const int WG = 256;
static const int TX = 16;
static const int BR = 64;         // queries per workgroup
static const int BC = 32;         // keys per tile
static const int CH = 16;         // head-dim chunk staged for Q and K
static const int RS = BR / (WG / TX);  // 4 query rows per thread, phase (d)
static const int CG = BC / 4;     // softmax columns per thread

static const int  DPT       = (kHeadDim + TX - 1) / TX;
static const int  MAXDPT    = 256 / TX;
static const bool DIM_EXACT = (kHeadDim % TX) == 0;

typealias MatA = CoopMat<float16_t, MemoryScope::Subgroup, 16, 16, CoopMatMatrixUse::MatrixA>;
typealias MatB = CoopMat<float16_t, MemoryScope::Subgroup, 16, 16, CoopMatMatrixUse::MatrixB>;
typealias MatC = CoopMat<float, MemoryScope::Subgroup, 16, 16, CoopMatMatrixUse::MatrixAccumulator>;

// S is BR x BC = 4 x 2 fragments. Eight subgroups take one each; four take two.
static const int NFRAG   = (BR / 16) * (BC / 16);
static const int MAX_FPS = NFRAG / 4;   // fragments per subgroup at the widest

// Q and K staged as fp16, [token][chunk dim]. The 24-element row is a bank
// spread, and 48 B is still the multiple of 16 the fragment load requires.
static const int TS = CH + 8;
groupshared float16_t sQh[BR * TS];
groupshared float16_t sKh[BC * TS];
// The score tile. 36 floats per row rather than the scalar kernel's 33: a
// cooperative-matrix store needs its rows 16-byte aligned, and 33 is not.
static const int PS = BC + 4;
groupshared float sP[BR * PS];
groupshared float sM[BR];      // running row max
groupshared float sL[BR];      // running row sum
groupshared float sCorr[BR];   // exp(m_old - m_new); < 0 marks a fully masked tile
groupshared float sRed[WG];    // row-reduction scratch (sKh is fp16 and too small)

struct AttnParams {
    float* out;
    uint*  q;
    uint*  k;
    uint*  v;
    uint*  bias;
    float* part_ml;
    uint*  labels;
    uint   nq;
    uint   nk;
    uint   n_heads;
    float  scale;
    uint   q_stride;
    uint   k_stride;
    uint   v_stride;
    uint   out_stride;
    uint   bias_stride_h;
    uint   bias_stride_q;
    uint   keys_per_split;
    uint   n_splits;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void flash_attn_coop(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                     uniform AttnParams p) {
    const uint t = tid.x;
    const uint tx = t & uint(TX - 1);
    const uint ty = t >> 4;
    const uint head = gid.y;
    const uint batch = (kSplitK != 0) ? 0u : gid.z;
    const uint split = (kSplitK != 0) ? gid.z : 0u;

    const uint SG_SIZE = uint(WG / kSgCount);
    const uint sg = t / SG_SIZE;

    const uint q0 = gid.x * uint(BR);
    if (q0 >= p.nq) return;   // uniform: no barrier has been reached yet

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

    const uint qrow = ty * uint(RS);   // this thread's first query, phase (d)
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
        // ---- (a) S = Q @ K^T on tensor cores -------------------------------
        MatC s[MAX_FPS];
        [ForceUnroll] for (int f = 0; f < MAX_FPS; ++f) s[f] = MatC(0.0f);

        for (uint c0 = 0; c0 < HD; c0 += uint(CH)) {
            // 16 threads per token cover one chunk, 16 tokens at a time. A dim
            // past the head would be staged as zero, but the launcher only
            // selects this kernel when head_dim is a multiple of the chunk.
            const uint stage_tok = t >> 4;
            const uint stage_c = t & 15u;
            [unroll] for (uint u = 0; u < uint(BR / 16); ++u) {
                const uint qi = u * 16u + stage_tok;
                const uint gq = q0 + qi;
                sQh[qi * uint(TS) + stage_c] = float16_t(
                    gq < p.nq
                        ? load_elem(p.q, qbase + gq * p.q_stride + c0 + stage_c, kQF16 != 0)
                        : 0.0f);
            }
            [unroll] for (uint u = 0; u < uint(BC / 16); ++u) {
                const uint kj = u * 16u + stage_tok;
                const uint gk = j0 + kj;
                sKh[kj * uint(TS) + stage_c] = float16_t(
                    gk < j_end
                        ? load_elem(p.k, kbase + gk * p.k_stride + c0 + stage_c, kKVF16 != 0)
                        : 0.0f);
            }
            GroupMemoryBarrierWithGroupSync();

            [ForceUnroll] for (int f = 0; f < MAX_FPS; ++f) {
                const uint frag = sg + uint(f) * uint(kSgCount);
                if (frag >= uint(NFRAG)) continue;
                const uint qb = frag >> 1;
                const uint kb = frag & 1u;
                // K is [token][dim] like Q, so the KxN operand is that same
                // block read column-major -- no transpose anywhere.
                let a = MatA.Load<CoopMatMatrixLayout::RowMajor>(
                    sQh, qb * 16u * uint(TS), uint(TS));
                let b = MatB.Load<CoopMatMatrixLayout::ColumnMajor>(
                    sKh, kb * 16u * uint(TS), uint(TS));
                s[f] = coopMatMulAdd<float, false>(a, b, s[f]);
            }
            GroupMemoryBarrierWithGroupSync();
        }

        [ForceUnroll] for (int f = 0; f < MAX_FPS; ++f) {
            const uint frag = sg + uint(f) * uint(kSgCount);
            if (frag >= uint(NFRAG)) continue;
            s[f].Store<CoopMatMatrixLayout::RowMajor>(
                sP, (frag >> 1) * 16u * uint(PS) + (frag & 1u) * 16u, uint(PS));
        }
        GroupMemoryBarrierWithGroupSync();

        // ---- (b) scale, bias / mask, and this thread's row max --------------
        // Merged with the max pass: unlike the scalar kernel, the thread that
        // writes a cell here is the one that reads it, so no barrier separates
        // them.
        {
            const uint gq = q0 + sr;
            float m = -1e30f;
            for (uint u = 0; u < uint(CG); ++u) {
                const uint gk = j0 + sc0 + u;
                const uint o = sr * uint(PS) + sc0 + u;
                float v = sP[o] * p.scale;
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
                sP[o] = v;
                m = max(m, v);
            }
            sRed[t] = m;
        }
        GroupMemoryBarrierWithGroupSync();

        if (t < uint(BR)) {
            const uint b = t * 4u;
            const float rm = max(max(sRed[b], sRed[b + 1]), max(sRed[b + 2], sRed[b + 3]));
            if (rm <= -1e29f) {
                sCorr[t] = -1.0f;
            } else {
                const float m_new = max(sM[t], rm);
                sCorr[t] = (sM[t] <= -1e29f) ? 0.0f : exp(sM[t] - m_new);
                sM[t] = m_new;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        {
            float s2 = 0.0f;
            if (sCorr[sr] < 0.0f) {
                for (uint u = 0; u < uint(CG); ++u) sP[sr * uint(PS) + sc0 + u] = 0.0f;
            } else {
                const float m = sM[sr];
                for (uint u = 0; u < uint(CG); ++u) {
                    const uint o = sr * uint(PS) + sc0 + u;
                    const float e = exp(sP[o] - m);
                    sP[o] = e;
                    s2 += e;
                }
            }
            sRed[t] = s2;
        }
        GroupMemoryBarrierWithGroupSync();

        if (t < uint(BR)) {
            const uint b = t * 4u;
            const float rs = sRed[b] + sRed[b + 1] + sRed[b + 2] + sRed[b + 3];
            sL[t] = sL[t] * ((sCorr[t] < 0.0f) ? 1.0f : sCorr[t]) + rs;
        }
        GroupMemoryBarrierWithGroupSync();

        // ---- (d) O = O * corr + P @ V (scalar; see the note at the top) -----
        [unroll] for (int i = 0; i < RS; ++i) {
            const float c = sCorr[qrow + uint(i)];
            const float cs_ = (c < 0.0f) ? 1.0f : c;
            for (int j = 0; j < DPT; ++j) acc[i][j] *= cs_;
        }
        for (uint jj = 0; jj < uint(BC); ++jj) {
            const uint gk = j0 + jj;
            if (gk >= j_end) break;   // uniform across the workgroup
            float vv[MAXDPT];
            for (int j = 0; j < DPT; ++j)
                vv[j] = (DIM_EXACT || dcol + uint(j) < HD)
                            ? load_elem(p.v, vbase + gk * p.v_stride + dcol + uint(j),
                                        kKVF16 != 0)
                            : 0.0f;
            [unroll] for (int i = 0; i < RS; ++i) {
                const float pp = sP[(qrow + uint(i)) * uint(PS) + jj];
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
        const float sc_out = (kSplitK != 0) ? 1.0f : ((l > 0.0f) ? (1.0f / l) : 0.0f);
        for (int j = 0; j < DPT; ++j)
            if (DIM_EXACT || dcol + uint(j) < HD)
                p.out[obase + gq * p.out_stride + dcol + uint(j)] = acc[i][j] * sc_out;
    }
    if (kSplitK != 0 && t < uint(BR)) {
        const uint gq = q0 + t;
        if (gq < p.nq) {
            const uint o = ((split * p.n_heads + head) * p.nq + gq) * 2u;
            p.part_ml[o] = sM[t];
            p.part_ml[o + 1u] = sL[t];
        }
    }
}
