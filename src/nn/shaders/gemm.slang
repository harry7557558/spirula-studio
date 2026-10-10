// The workhorse: out[M, N] = act(alpha * (x[M, K] @ W[N, K]^T) + bias[N] + residual)
//
// Both operands are row-major with K contiguous, which is the natural layout
// for every Linear in the model (PyTorch stores weights [out, in]) and lets the
// inner loop stream both along K. `W` may be fp16 (read as packed uint pairs);
// `x` and the output are fp32.
//
// The epilogue is fused on purpose. SAM 3's forward pass is ~200 Linears, each
// followed by a bias add and often an activation and a residual; as separate
// passes those cost more memory traffic than the matmul itself at these shapes.
//
// Tiling: 64x64 output tile per 256-thread workgroup, K stepped 16 at a time,
// 4x4 accumulators per thread. Shared memory 2 * 16 * 65 * 4 = 8.3 KiB, inside
// the 16 KiB Vulkan floor. The +1 padding on the fast axis keeps the
// four-per-thread staging stores off the same bank.
//
// Every loop indexing `acc`/`a`/`b` is [ForceUnroll], never [unroll]: rolled,
// those arrays stay dynamically indexed, which on Metal means thread-local
// *memory*. See src/backend/vulkan/README.md, "slangc [unroll]".

#include "_common.slang"

// Specialization constants, module-wide and in declaration order (ids 0..4).
// Every launcher in OpGemm.cpp passes all five -- supplying only a later axis
// would silently leave the earlier ones at these defaults.
[SpecializationConstant] const int kWeightF16  = 0;
[SpecializationConstant] const int kInputF16   = 0;
[SpecializationConstant] const int kAct        = 0;
[SpecializationConstant] const int kHasBias    = 0;
[SpecializationConstant] const int kHasResidual = 0;
// Nonzero: x is a sparse convolution's implicit im2col over Ci = kGatherCi
// channels, x[nbr[m, t], c] or 0 where nbr is -1 -- never written, because at
// MoGe-3's finest level the column matrix is 3.2 GB a convolution.
[SpecializationConstant] const int kGatherCi   = 0;

struct GemmParams {
    float* out;       // [M, N] f32
    uint*  x;         // [M, K] f32 or packed f16
    uint*  w;         // [N, K] f32 or packed f16
    uint*  bias;      // [N]    f32 (never null: launchers substitute a zeroed block)
    float* residual;  // [M, N] f32 (never null)
    uint   M;
    uint   N;
    uint   K;
    float  alpha;     // scales the matmul result before bias/residual
    uint   x_row_stride;  // in elements; == K unless x is a slice of a wider buffer
    uint   groups_per_row; // gemm_nt_thin only: N folds across (gid.x, gid.y)
    uint*  nbr;           // [M, taps] i32 under kGatherCi (never null)
    uint   taps;
    uint   n_in;          // rows of x under kGatherCi
};

// x[m, k], directly or through the neighbour table.
float load_x(GemmParams p, uint m, uint k) {
    if (kGatherCi == 0) return load_elem(p.x, m * p.x_row_stride + k, kInputF16 != 0);
    const uint tap = k / uint(kGatherCi);
    const int id = int(p.nbr[m * p.taps + tap]);
    if (id < 0 || uint(id) >= p.n_in) return 0.0f;
    return load_elem(p.x, uint(id) * uint(kGatherCi) + (k - tap * uint(kGatherCi)), false);
}

static const uint BM = 64;
static const uint BN = 64;
static const uint BK = 16;

groupshared float sA[BK][BM + 1];
groupshared float sB[BK][BN + 1];

[shader("compute")]
[numthreads(256, 1, 1)]
void gemm_nt(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
             uniform GemmParams p) {
    const uint tile_m = gid.x * BM;
    const uint tile_n = gid.y * BN;
    const uint t = tid.x;

    // Thread (tx, ty) owns rows [ty*4, ty*4+4) and cols [tx*4, tx*4+4).
    const uint tx = t & 15u;
    const uint ty = t >> 4;

    float acc[4][4];
    [ForceUnroll] for (int i = 0; i < 4; ++i)
        [ForceUnroll] for (int j = 0; j < 4; ++j) acc[i][j] = 0.0f;

    // Staging assignment: 1024 elements per tile, 4 per thread, laid out so the
    // four are consecutive along K (coalesced global reads).
    const uint load_row = t >> 2;          // 0..63
    const uint load_k0 = (t & 3u) * 4u;    // 0,4,8,12

    for (uint k0 = 0; k0 < p.K; k0 += BK) {
        // ---- stage x ----
        {
            uint m = tile_m + load_row;
            bool in_m = m < p.M;
            [ForceUnroll] for (uint u = 0; u < 4; ++u) {
                uint k = k0 + load_k0 + u;
                float v = 0.0f;
                if (in_m && k < p.K)
                    v = load_x(p, m, k);
                sA[load_k0 + u][load_row] = v;
            }
        }
        // ---- stage W ----
        {
            uint n = tile_n + load_row;
            bool in_n = n < p.N;
            [ForceUnroll] for (uint u = 0; u < 4; ++u) {
                uint k = k0 + load_k0 + u;
                float v = 0.0f;
                if (in_n && k < p.K) v = load_elem(p.w, n * p.K + k, kWeightF16 != 0);
                sB[load_k0 + u][load_row] = v;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        [ForceUnroll]
        for (uint kk = 0; kk < BK; ++kk) {
            float a[4], b[4];
            [ForceUnroll] for (int i = 0; i < 4; ++i) a[i] = sA[kk][ty * 4 + i];
            [ForceUnroll] for (int j = 0; j < 4; ++j) b[j] = sB[kk][tx * 4 + j];
            [ForceUnroll] for (int i = 0; i < 4; ++i)
                [ForceUnroll] for (int j = 0; j < 4; ++j) acc[i][j] += a[i] * b[j];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // ---- epilogue ----
    [ForceUnroll] for (uint i = 0; i < 4; ++i) {
        uint m = tile_m + ty * 4 + i;
        if (m >= p.M) continue;
        [ForceUnroll] for (uint j = 0; j < 4; ++j) {
            uint n = tile_n + tx * 4 + j;
            if (n >= p.N) continue;
            float v = acc[i][j] * p.alpha;
            if (kHasBias != 0) v += asfloat(p.bias[n]);
            if (kHasResidual != 0) v += p.residual[m * p.N + n];
            p.out[m * p.N + n] = apply_act(v, kAct);
        }
    }
}

// ---------------------------------------------------------------------------
// Wide variant: 128x128 output tile per 256-thread workgroup, K stepped 16 at a
// time, 8x8 accumulators per thread.
//
// This is the kernel that runs SAM 3's real shapes -- the ViT's 5184x3072x1024
// qkv projection, its 5184x4736 MLP, the neck's 82944x256x2304 3x3 convolution.
//
// What sets the runtime here is shared-memory *bandwidth*, not the FMA pipe.
// An Ampere SM retires 128 fp32 FMAs and reads 128 bytes of shared per clock,
// so a kernel may spend at most one shared byte per FMA. The 64x64 tile above
// spends two (4 FMAs per 8-byte a/b pair) and runs at a quarter of the machine.
// This kernel spends 0.75:
//
//   per thread, per k              shared bytes   FMAs
//   8 rows of A, fp32                    32         -
//   8 cols of W, fp16 packed             16         -
//   8x8 outer product                     -        64
//
// Keeping W in shared in the fp16 it already is on the wire is where the last
// quarter comes from -- it is the same bytes the checkpoint holds, unpacked in
// registers instead of on the way in. That is also why this variant requires
// fp16 weights: with an fp32 operand there is nothing to pack, and OpGemm falls
// back to the 64x64 tile. K is stepped two at a time because a packed word
// holds k and k+1 of one column.
//
// Column ownership is INTERLEAVED (thread tx owns columns tx, tx+16, ... ,
// tx+112) rather than blocked. A blocked 8-wide span would put the sixteen
// lanes of a half-warp on banks 8t mod 32 -- a 4-way conflict on every read of
// the weight tile. Interleaved, the same sixteen lanes read sixteen consecutive
// banks.
// ---------------------------------------------------------------------------

static const uint WM = 128;
static const uint WN = 128;
static const uint WK = 16;

groupshared float wA[WK * (WM + 1)];            // [k][m]              8.1 KiB
groupshared uint  wB[(WK / 2) * (WN + 1)];      // [k/2][n], half2     4.0 KiB

[shader("compute")]
[numthreads(256, 1, 1)]
void gemm_nt_big(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                 uniform GemmParams p) {
    // The grid runs N on x so that consecutive workgroups share an A tile and
    // the whole weight matrix streams past it. The other order re-reads A once
    // per column tile, and A is fp32 while W is fp16 -- twice the bytes for the
    // same reuse.
    const uint tile_m = gid.y * WM;
    const uint tile_n = gid.x * WN;
    const uint t = tid.x;
    const uint tx = t & 15u;   // column lane
    const uint ty = t >> 4;    // row lane

    float acc[8][8];
    [ForceUnroll] for (int i = 0; i < 8; ++i)
        [ForceUnroll] for (int j = 0; j < 8; ++j) acc[i][j] = 0.0f;

    // Staging. A: WM*WK elements, 8 per thread, WK consecutive along K per row
    // so the global reads coalesce. W: WN*WK/2 packed words, 4 per thread, each
    // holding one column's k and k+1.
    const uint load_row = t >> 4;        // 0..15, then +16 per pass
    const uint load_k = t & 15u;
    const uint wcol = t >> 3;            // 0..31, then +32 per pass
    const uint wkp = t & 7u;

    // The next k-block's global loads sit in registers while this one computes.
    // At 8x8 accumulators only two or three workgroups fit on an SM, so there is
    // not much of a second block to cover the load latency -- the kernel has to
    // cover it itself.
    // Zero-initialized only to keep slangc from warning: the priming pass
    // (k0 == 0) fills them before the k0 > 0 pass reads them back.
    float ra[8] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    uint  rb[4] = {0u, 0u, 0u, 0u};
    for (uint k0 = 0; k0 < p.K + WK; k0 += WK) {
        // Stage what the previous pass fetched, except on the priming pass.
        if (k0 > 0) {
            [ForceUnroll] for (uint u = 0; u < 8; ++u)
                wA[load_k * (WM + 1) + load_row + u * 16u] = ra[u];
            [ForceUnroll] for (uint u = 0; u < 4; ++u)
                wB[wkp * (WN + 1) + wcol + u * 32u] = rb[u];
            GroupMemoryBarrierWithGroupSync();
        }

        // Fetch the next block. Independent of the tile just staged, so its
        // latency overlaps the outer product below.
        if (k0 < p.K) {
            const uint ka = k0 + load_k;
            const uint kb = k0 + wkp * 2u;
            [ForceUnroll] for (uint u = 0; u < 8; ++u) {
                const uint m = tile_m + load_row + u * 16u;
                ra[u] = (m < p.M && ka < p.K)
                            ? load_x(p, m, ka)
                            : 0.0f;
            }
            [ForceUnroll] for (uint u = 0; u < 4; ++u) {
                const uint n = tile_n + wcol + u * 32u;
                const float w0 =
                    (n < p.N && kb < p.K) ? load_elem(p.w, n * p.K + kb, true) : 0.0f;
                const float w1 = (n < p.N && kb + 1u < p.K)
                                     ? load_elem(p.w, n * p.K + kb + 1u, true)
                                     : 0.0f;
                rb[u] = pack_half2(float2(w0, w1));
            }
        }
        if (k0 == 0) continue;   // nothing staged yet

        [ForceUnroll] for (uint kp = 0; kp < WK / 2; ++kp) {
            float b0[8], b1[8];
            [ForceUnroll] for (int j = 0; j < 8; ++j) {
                const float2 h = unpack_half2(wB[kp * (WN + 1) + tx + uint(j) * 16u]);
                b0[j] = h.x;
                b1[j] = h.y;
            }
            float a[8];
            [ForceUnroll] for (int i = 0; i < 8; ++i)
                a[i] = wA[(kp * 2u) * (WM + 1) + ty + uint(i) * 16u];
            [ForceUnroll] for (int i = 0; i < 8; ++i)
                [ForceUnroll] for (int j = 0; j < 8; ++j) acc[i][j] += a[i] * b0[j];
            [ForceUnroll] for (int i = 0; i < 8; ++i)
                a[i] = wA[(kp * 2u + 1u) * (WM + 1) + ty + uint(i) * 16u];
            [ForceUnroll] for (int i = 0; i < 8; ++i)
                [ForceUnroll] for (int j = 0; j < 8; ++j) acc[i][j] += a[i] * b1[j];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    [ForceUnroll] for (uint i = 0; i < 8; ++i) {
        const uint m = tile_m + ty + i * 16u;
        if (m >= p.M) continue;
        [ForceUnroll] for (uint j = 0; j < 8; ++j) {
            const uint n = tile_n + tx + j * 16u;
            if (n >= p.N) continue;
            float v = acc[i][j] * p.alpha;
            if (kHasBias != 0) v += asfloat(p.bias[n]);
            if (kHasResidual != 0) v += p.residual[m * p.N + n];
            p.out[m * p.N + n] = apply_act(v, kAct);
        }
    }
}

// ---------------------------------------------------------------------------
// Narrow variant: gemm_nt's tile and staging, 2x8 accumulators, interleaved
// columns (gemm_nt_big says why interleaved).
//
// A quarter of gemm_nt_big's accumulator registers for 0.625 shared floats per
// FMA against gemm_nt's 0.5 -- the trade for a register file that cannot hold
// 64 of them. M2: 820 GFLOP/s against gemm_nt_big's 330; RTX 5070: 2.8 TFLOP/s
// against 8.6.
// ---------------------------------------------------------------------------

[shader("compute")]
[numthreads(256, 1, 1)]
void gemm_nt_narrow(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                    uniform GemmParams p) {
    const uint tile_m = gid.x * BM;
    const uint tile_n = gid.y * BN;
    const uint t = tid.x;
    const uint tx = t & 7u;    // 8 column lanes, 8 columns each
    const uint ty = t >> 3;    // 32 row lanes, 2 rows each

    float acc[2][8];
    [ForceUnroll] for (int i = 0; i < 2; ++i)
        [ForceUnroll] for (int j = 0; j < 8; ++j) acc[i][j] = 0.0f;

    const uint load_row = t >> 2;
    const uint load_k0 = (t & 3u) * 4u;

    for (uint k0 = 0; k0 < p.K; k0 += BK) {
        {
            uint m = tile_m + load_row;
            bool in_m = m < p.M;
            [ForceUnroll] for (uint u = 0; u < 4; ++u) {
                uint k = k0 + load_k0 + u;
                float v = 0.0f;
                if (in_m && k < p.K)
                    v = load_x(p, m, k);
                sA[load_k0 + u][load_row] = v;
            }
        }
        {
            uint n = tile_n + load_row;
            bool in_n = n < p.N;
            [ForceUnroll] for (uint u = 0; u < 4; ++u) {
                uint k = k0 + load_k0 + u;
                float v = 0.0f;
                if (in_n && k < p.K) v = load_elem(p.w, n * p.K + k, kWeightF16 != 0);
                sB[load_k0 + u][load_row] = v;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        [ForceUnroll]
        for (uint kk = 0; kk < BK; ++kk) {
            float a[2], b[8];
            [ForceUnroll] for (int i = 0; i < 2; ++i) a[i] = sA[kk][ty + uint(i) * 32u];
            [ForceUnroll] for (int j = 0; j < 8; ++j) b[j] = sB[kk][tx + uint(j) * 8u];
            [ForceUnroll] for (int i = 0; i < 2; ++i)
                [ForceUnroll] for (int j = 0; j < 8; ++j) acc[i][j] += a[i] * b[j];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    [ForceUnroll] for (uint i = 0; i < 2; ++i) {
        uint m = tile_m + ty + i * 32u;
        if (m >= p.M) continue;
        [ForceUnroll] for (uint j = 0; j < 8; ++j) {
            uint n = tile_n + tx + j * 8u;
            if (n >= p.N) continue;
            float v = acc[i][j] * p.alpha;
            if (kHasBias != 0) v += asfloat(p.bias[n]);
            if (kHasResidual != 0) v += p.residual[m * p.N + n];
            p.out[m * p.N + n] = apply_act(v, kAct);
        }
    }
}

// ---------------------------------------------------------------------------
// Thin-M variant: one workgroup per output row block, no shared staging.
//
// The tiled kernel wastes 63/64 of its work when M is small, and M *is* small
// all over SAM 3 -- the DETR decoder runs 201 queries, the SAM mask decoder 6
// to 10 tokens, every MLP head exactly 1. This variant assigns 64 threads per
// output element group and reduces over K in registers, which is both simpler
// and several times faster in that regime. OpGemm.cpp picks between them.
// ---------------------------------------------------------------------------

groupshared float sRed[256];

[shader("compute")]
[numthreads(64, 1, 1)]
void gemm_nt_thin(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                  uniform GemmParams p) {
    // N can be far past the 65535 grid cap -- the mask hypernetwork multiplies
    // one token against 288x288 = 82944 pixel embeddings -- so the column index
    // folds across two grid dimensions and the row takes the third.
    const uint n = gid.y * p.groups_per_row + gid.x;
    const uint m = gid.z;
    if (n >= p.N || m >= p.M) return;

    float sum = 0.0f;
    for (uint k = tid.x; k < p.K; k += 64) {
        float xv = load_x(p, m, k);
        float wv = load_elem(p.w, n * p.K + k, kWeightF16 != 0);
        sum += xv * wv;
    }
    sRed[tid.x] = sum;
    GroupMemoryBarrierWithGroupSync();
    // Deterministic tree reduction: subgroup-width agnostic, so wave32 and
    // wave64 devices produce bit-identical results.
    for (uint s = 32; s > 0; s >>= 1) {
        if (tid.x < s) sRed[tid.x] += sRed[tid.x + s];
        GroupMemoryBarrierWithGroupSync();
    }
    if (tid.x == 0) {
        float v = sRed[0] * p.alpha;
        if (kHasBias != 0) v += asfloat(p.bias[n]);
        if (kHasResidual != 0) v += p.residual[m * p.N + n];
        p.out[m * p.N + n] = apply_act(v, kAct);
    }
}
