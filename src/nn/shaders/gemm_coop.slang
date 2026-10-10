// The same GEMM as gemm.slang, on tensor cores.
//
//   out[M, N] = act(alpha * (x[M, K] @ W[N, K]^T) + bias[N] + residual)
//
// This module is compiled unconditionally but must only ever be *instantiated*
// on a device that advertises VK_KHR_cooperative_matrix: it declares the
// CooperativeMatrixKHR, Float16 and VulkanMemoryModel capabilities, and
// vkCreateShaderModule may reject the whole blob elsewhere. That is why it is a
// separate file rather than two more entry points in gemm.slang -- Pipelines
// creates one VkShaderModule per module, lazily, the first time an entry from
// it is acquired, so a device without the feature never sees these words.
// `OpGemm.cpp` gates the dispatch on `nn::coop_matrix_enabled()`.
//
// Arithmetic. Operands are fp16, accumulation is fp32. The weights are already
// fp16 -- it is what the checkpoint holds -- so the only new rounding is on the
// activations, which is what every reference implementation of these models
// does (PyTorch runs them under autocast). The accumulator stays fp32 because
// that is where a 4736-long dot product would actually lose digits.
//
// Tiling. One 256-thread workgroup owns a TM x 128 output tile; each subgroup
// owns a 32 x 64 quarter of it as 2 x 4 accumulator fragments, 8 multiplies per
// K step of 16.
//
// **W is not staged through shared memory.** The B operand of A(MxK) @ B(KxN)
// is a 16x16 block of W read column-major with stride K -- which is exactly
// what W already is on the wire, in exactly the fp16 the fragment wants. Going
// through shared instead cost a global load, a 16-bit shared store, a shared
// read and a transpose per element, and that staging, not the multiply, is what
// this kernel is short of: with both operands staged it measured no faster than
// the fp32 kernel it replaces, while the same multiplies in isolation ran five
// times quicker. Only `x` needs the round trip, because it is fp32 and the
// fragment is not.
//
// Reading W directly is also why the launcher requires N and K to be multiples
// of 16: a cooperative-matrix load has no bounds checking, so a fragment must
// lie either wholly inside the matrix or wholly outside it (those are skipped).
//
// Subgroup width is not assumed: the host passes it as `kSgRows` (the number of
// subgroup rows in the workgroup), from the width it pinned at device creation.
// 32-wide -> 8 subgroups -> 4 x 2 -> a 128 x 128 tile; 64-wide -> 4 -> 2 x 2 ->
// 64 x 128. Both give every thread the same 64 accumulator registers.

#include "_common.slang"

using namespace linalg;

// Specialization constants, module-wide, in declaration order (ids 0..4).
// NOTE this is NOT the same list as gemm.slang's -- there is no kWeightF16,
// because a cooperative-matrix dispatch is only chosen for an fp16 weight.
[SpecializationConstant] const int kInputF16    = 0;
[SpecializationConstant] const int kAct         = 0;
[SpecializationConstant] const int kHasBias     = 0;
[SpecializationConstant] const int kHasResidual = 0;
[SpecializationConstant] const int kSgRows      = 4;
[SpecializationConstant] const int kGatherCi    = 0;   // as gemm.slang's

struct GemmParams {
    float* out;       // [M, N] f32
    uint*  x;         // [M, K] f32 or packed f16
    uint*  w;         // [N, K] packed f16
    uint*  bias;      // [N]    f32 (never null: launchers substitute a zeroed block)
    float* residual;  // [M, N] f32 (never null)
    uint   M;
    uint   N;
    uint   K;
    float  alpha;
    uint   x_row_stride;   // in elements; == K unless x is a slice of a wider buffer
    uint   groups_per_row; // unused here; keeps the struct byte-identical to gemm.slang
    uint*  nbr;
    uint   taps;
    uint   n_in;
};

float load_x(GemmParams p, uint m, uint k) {
    if (kGatherCi == 0) return load_elem(p.x, m * p.x_row_stride + k, kInputF16 != 0);
    const uint tap = k / uint(kGatherCi);
    const int id = int(p.nbr[m * p.taps + tap]);
    if (id < 0 || uint(id) >= p.n_in) return 0.0f;
    return load_elem(p.x, uint(id) * uint(kGatherCi) + (k - tap * uint(kGatherCi)), false);
}

typealias MatA = CoopMat<float16_t, MemoryScope::Subgroup, 16, 16, CoopMatMatrixUse::MatrixA>;
typealias MatB = CoopMat<float16_t, MemoryScope::Subgroup, 16, 16, CoopMatMatrixUse::MatrixB>;
typealias MatC = CoopMat<float, MemoryScope::Subgroup, 16, 16, CoopMatMatrixUse::MatrixAccumulator>;

static const uint CK = 16;      // K per stage == the fragment's K
static const uint TN = 128;     // output columns per workgroup, all widths
static const uint MAX_TM = 128; // output rows at the narrowest subgroup width
static const uint RF = 2;       // accumulator fragments down, per subgroup
static const uint CF = 4;       // accumulator fragments across, per subgroup
static const uint MAX_SG = 8;   // 256 threads / a 32-wide subgroup

// x's tile, [m][k]. The pad spreads the fragment loads across banks: at a bare
// 16-element row the sixteen rows of a fragment start on only four of them.
// 24 elements is 48 B -- still a multiple of 16, which the load requires.
static const uint SA_STRIDE = CK + 8;
groupshared float16_t sA[MAX_TM * SA_STRIDE];
// One 16x16 fp32 landing pad per subgroup. Cooperative matrix does not say
// which element of the tile a given lane holds, so the epilogue -- which needs
// bias[n] and residual[m, n] -- goes through memory to recover the mapping.
groupshared float sEpi[MAX_SG * 256];

[shader("compute")]
[numthreads(256, 1, 1)]
void gemm_nt_coop(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                  uniform GemmParams p) {
    const uint TM = uint(kSgRows) * 32u;
    const uint SG_SIZE = 256u / (uint(kSgRows) * 2u);
    const uint sg = tid.x / SG_SIZE;
    const uint lane = tid.x - sg * SG_SIZE;
    // Subgroup (sr, sc) owns rows [sr*32, +32) and columns [sc*64, +64).
    const uint sr = sg >> 1;
    const uint sc = sg & 1u;

    const uint tile_m = gid.y * TM;
    const uint tile_n = gid.x * TN;
    // W as halves. The launcher guarantees K is a multiple of 16 and the buffer
    // is 256-byte aligned, so every fragment base below is 32-byte aligned.
    float16_t* wh = (float16_t*)p.w;

    MatC acc[RF][CF];
    [ForceUnroll] for (uint i = 0; i < RF; ++i)
        [ForceUnroll] for (uint j = 0; j < CF; ++j) acc[i][j] = MatC(0.0f);

    // 16 threads cover one row's K step, 16 rows at a time.
    const uint load_k = tid.x & 15u;
    const uint load_row = tid.x >> 4;
    const uint MAX_RA = MAX_TM / 16u;   // rows of x staged per thread

    // Literal trip counts with a predicate rather than `u * 16 < TM`: TM comes
    // from a specialization constant, so a loop bounded by it is not a
    // compile-time count in Slang and `ra` would land in scratch memory instead
    // of registers. The predicate folds away when the driver specializes.
    float ra[MAX_RA];
    [ForceUnroll] for (uint u = 0; u < MAX_RA; ++u) {
        const uint m = tile_m + u * 16u + load_row;
        ra[u] = (u * 16u < TM && m < p.M)
                    ? load_x(p, m, load_k)
                    : 0.0f;
    }

    for (uint k0 = 0; k0 < p.K; k0 += CK) {
        // Issued first so the load is in flight across the staging barrier.
        MatB b[CF];
        [ForceUnroll] for (uint j = 0; j < CF; ++j) {
            const uint n0 = tile_n + sc * 64u + j * 16u;
            b[j] = n0 < p.N ? MatB.Load<CoopMatMatrixLayout::ColumnMajor>(
                                  wh, n0 * p.K + k0, p.K)
                            : MatB(float16_t(0.0f));
        }

        [ForceUnroll] for (uint u = 0; u < MAX_RA; ++u)
            if (u * 16u < TM)
                sA[(u * 16u + load_row) * SA_STRIDE + load_k] = float16_t(ra[u]);
        GroupMemoryBarrierWithGroupSync();

        // The next K block's x tile sits in registers while this one multiplies.
        if (k0 + CK < p.K) {
            const uint k = k0 + CK + load_k;
            [ForceUnroll] for (uint u = 0; u < MAX_RA; ++u) {
                const uint m = tile_m + u * 16u + load_row;
                ra[u] = (u * 16u < TM && m < p.M)
                            ? load_x(p, m, k)
                            : 0.0f;
            }
        }

        MatA a[RF];
        [ForceUnroll] for (uint i = 0; i < RF; ++i)
            a[i] = MatA.Load<CoopMatMatrixLayout::RowMajor>(
                sA, (sr * RF + i) * 16u * SA_STRIDE, SA_STRIDE);
        [ForceUnroll] for (uint i = 0; i < RF; ++i)
            [ForceUnroll] for (uint j = 0; j < CF; ++j)
                acc[i][j] = coopMatMulAdd<float, false>(a[i], b[j], acc[i][j]);

        GroupMemoryBarrierWithGroupSync();
    }

    // ---- epilogue -------------------------------------------------------
    // One fragment at a time through sEpi, where element e of the pad is row
    // e/16, column e%16 of that fragment. Every subgroup runs the same RF*CF
    // iterations, so the workgroup-wide barriers stay uniform.
    [ForceUnroll] for (uint i = 0; i < RF; ++i) {
        [ForceUnroll] for (uint j = 0; j < CF; ++j) {
            GroupMemoryBarrierWithGroupSync();
            acc[i][j].Store<CoopMatMatrixLayout::RowMajor>(sEpi, sg * 256u, 16u);
            GroupMemoryBarrierWithGroupSync();
            const uint m0 = tile_m + sr * 32u + i * 16u;
            const uint n0 = tile_n + sc * 64u + j * 16u;
            for (uint e = lane; e < 256u; e += SG_SIZE) {
                const uint m = m0 + (e >> 4);
                const uint n = n0 + (e & 15u);
                if (m >= p.M || n >= p.N) continue;
                float v = sEpi[sg * 256u + e] * p.alpha;
                if (kHasBias != 0) v += asfloat(p.bias[n]);
                if (kHasResidual != 0) v += p.residual[m * p.N + n];
                p.out[m * p.N + n] = apply_act(v, kAct);
            }
        }
    }
}
