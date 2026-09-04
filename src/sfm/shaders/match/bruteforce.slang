// Brute-force descriptor matching (src/sfm/README.md, matching stage).
//
// Descriptors are 128 uint8 = 32 uint words, packed little-endian exactly as
// the SIFT descriptor kernel wrote them, and descriptors for many images live
// in one buffer (`desc`) uploaded once per block; a dispatch addresses its two
// images by descriptor index, so an exhaustive pass re-reads them from VRAM
// instead of re-uploading per pair (D22).
//
// Distances come from the expansion
//     ||q - t||^2 = ||q||^2 + ||t||^2 - 2*(q.t)
// so the inner loop is a pure dot product and uses the hardware packed uint8x4
// dot product (`dot4add_u8packed` -> OpUDot/PackedVectorFormat4x8Bit, i.e.
// DP4A) -- 32 instructions per descriptor pair instead of 128 lanes of
// shift/mask/sub/mul/add. `descriptor_norms` precomputes ||d||^2 once per
// descriptor, which is why it is worth a buffer: recomputing ||t||^2 inside the
// loop would undo the saving (D21).
//
// This is exact, not an approximation. Every term is a non-negative integer
// below 128*255^2 = 8323200 < 2^23, the signed combination fits int32 with room
// to spare, and the result is bit-identical to the naive sum of squared
// differences -- so matches, and every reconstruction downstream, are unchanged.
//
// ---- one pass, both directions (D23) ----
//
// The matcher needs nearest neighbours in both directions: A->B for the ratio
// test, B->A for the cross-check. Those are row and column reductions of the
// *same* distance matrix, so computing them as two dispatches did the whole
// matrix twice. `match_pair` computes it once:
//
//   rows (A side)    -- best + second best + index, per thread, in registers.
//   columns (B side) -- best index only, via a groupshared InterlockedMin over
//                       the workgroup's queries, then `reduce_cols` across
//                       workgroups.
//
// Columns only need the argmin because the host reads exactly one field of the
// B-side result, `bestIdx`, for the mutual-nearest test; the ratio test is
// applied to the A side alone. THAT IS A REAL COUPLING: if a B-side ratio test
// is ever added, the column pass has to start carrying second-best too.
//
// The column packing is (d2 << 6) | localQuery: d2 < 2^23 and the workgroup
// holds 64 queries, so 29 bits, and a smaller packed value means a smaller d2
// or -- on a tie -- a lower query index, which is the tie-break the two-pass
// version had. Reduction order across workgroups is fixed, so the result does
// not depend on scheduling.
//
// Trains are staged through groupshared in tiles. Without that, every query
// thread streams the entire train descriptor set from L2 (8 KiB * nTrain per
// workgroup); with it, each tile is read once per workgroup and reused by all
// 64 queries. Measured: the kernel was L2-bandwidth-bound, not ALU-bound, once
// DP4A landed.

// dot4add_u8packed lowers to OpUDot with PackedVectorFormat4x8Bit. Where the
// device has no such instruction the shader is built again with -DNO_DOT4, the
// same arithmetic spelled out -- identical integer result, and `match_nodot`.
// The chooser is integerDotProduct4x8BitPackedUnsignedAccelerated, not whether
// the extension is present: MoltenVK advertises it on Apple silicon and expands
// it worse than the spelling below (114 vs 43 ms for one 8192x8192 pair on an
// M2). Everything below calls udot4add, never the intrinsic.
#ifdef NO_DOT4
uint udot4add(uint a, uint b, uint acc) {
    [unroll] for (uint s = 0; s < 32u; s += 8u)
        acc += ((a >> s) & 0xffu) * ((b >> s) & 0xffu);
    return acc;
}
#else
uint udot4add(uint a, uint b, uint acc) { return dot4add_u8packed(a, b, acc); }
#endif

// 32 uint words = 128 uint8 (SIFT, ALIKED). DESC_WORDS=64 is a second build for
// DeDoDe-G's 256; the tile halves so sTrain stays 8 KiB, and the header's
// d2 < 2^23 becomes 2^24, still inside what the column packing leaves it.
#ifndef DESC_WORDS
#define DESC_WORDS 32
#endif

static const int DW = DESC_WORDS;
static const uint TQ = 64;   // queries per workgroup for match_pair (see the
                             // column packing: 6 bits of local query index)
static const uint TQR = 256; // ... and for match_rows, which has no column side
                             // (BruteForceMatcher::kRowThreads must agree)
static const uint TT = (DESC_WORDS >= 64) ? 32 : 64;  // trains per shared tile

[[vk::push_constant]] cbuffer PC {
    uint dQ;       // first descriptor index of image A
    uint nQuery;   // features in A
    uint dT;       // first descriptor index of image B
    uint nTrain;   // features in B
    uint _f0, _f1, _f2, _f3;  // Push's float slots, unused here
    uint outA;     // uint4 index where A-side results start
    uint outB;     // uint4 index where B-side results start
    uint numWG;    // workgroups the match dispatch used (for reduce_cols)
};

[[vk::binding(0)]] StructuredBuffer<uint>    desc;   // all resident descriptors
[[vk::binding(1)]] RWStructuredBuffer<uint>  norm;   // ||d||^2 per descriptor
[[vk::binding(2)]] RWStructuredBuffer<uint4> result; // (bestIdx, bestD2, secondD2, 0)
[[vk::binding(3)]] RWStructuredBuffer<uint>  colPartial;  // numWG * nTrain packed candidates

// ||d||^2 for a run of descriptors, once per upload. u0 = first descriptor
// index, u1 = how many.
[shader("compute")] [numthreads(64, 1, 1)]
void descriptor_norms(uint3 tid : SV_DispatchThreadID) {
    uint d = tid.x;
    if (d >= nQuery) return;
    uint idx = dQ + d;
    uint base = idx * uint(DW);
    uint s = 0;
    [unroll] for (int k = 0; k < DW; k++) {
        uint w = desc[base + uint(k)];
        s = udot4add(w, w, s);
    }
    norm[idx] = s;
}

groupshared uint sTrain[TT * uint(DW)];  // 8 KiB at either width
groupshared uint sCol[TT];               // packed (d2 << 6) | localQuery

// Each train tile is read from VRAM once per *workgroup*, so a pair's train
// traffic is ceil(nQuery / TQ) * nTrain -- widening the workgroup divides it.
// match_pair cannot: its column packing gives the local query index 6 bits.
// match_rows has no column side, so it runs 256 wide and reads a scoring
// pair's train descriptors twice instead of eight times, which is what pair
// selection (half a million tiny pairs, every one streaming a full train set)
// is actually limited by. The row reduction is per query and scans trains in
// increasing index order either way, so the result is bit-identical.

// Row reduction shared by both match entry points; WITH_COLS also feeds the
// column candidates for the cross-check. match_rows exists because dispatches
// that skip the column side share no writable state (disjoint `result`
// regions, read-only desc/norm), so a whole batch of them can run with *no*
// inter-pair barriers -- that is what makes pair-selection scoring (thousands
// of tiny dispatches per submit) GPU-bound instead of pipeline-drain-bound.
void matchImpl<let WITH_COLS : bool, let NQ : uint>(uint3 gid, uint3 lid) {
    const uint lq = lid.x;
    const uint q = gid.x * NQ + lq;
    const bool active = q < nQuery;

    uint qd[DW];
    [unroll] for (int k = 0; k < DW; k++) qd[k] = 0;  // threads past nQuery never read these
    int nq = 0;
    if (active) {
        uint qbase = (dQ + q) * uint(DW);
        [unroll] for (int k = 0; k < DW; k++) qd[k] = desc[qbase + uint(k)];
        nq = int(norm[dQ + q]);
    }

    uint best = 0xffffffffu, second = 0xffffffffu, bestIdx = 0xffffffffu;

    for (uint t0 = 0; t0 < nTrain; t0 += TT) {
        const uint cnt = min(TT, nTrain - t0);

        GroupMemoryBarrierWithGroupSync();  // previous tile fully consumed
        for (uint e = lid.x; e < cnt * uint(DW); e += NQ)
            sTrain[e] = desc[(dT + t0 + e / uint(DW)) * uint(DW) + (e % uint(DW))];
        if (WITH_COLS)
            for (uint c = lid.x; c < cnt; c += NQ) sCol[c] = 0xffffffffu;
        GroupMemoryBarrierWithGroupSync();  // tile visible to every thread

        for (uint c = 0; c < cnt; c++) {
            uint packed = 0xffffffffu;  // inactive lanes contribute nothing
            if (active) {
                uint dot = 0;
                [unroll] for (int k = 0; k < DW; k++)
                    dot = udot4add(qd[k], sTrain[c * uint(DW) + uint(k)], dot);
                uint d2 = uint(nq + int(norm[dT + t0 + c]) - 2 * int(dot));
                uint j = t0 + c;
                if (d2 < best) { second = best; best = d2; bestIdx = j; }
                else if (d2 < second) { second = d2; }
                packed = (d2 << 6) | lq;
            }
            // Straight to the shared atomic. Pre-folding within the wave
            // (WaveActiveMin + WaveIsFirstLane) to cut same-address contention
            // measured *slower* here -- the hardware resolves the conflict
            // cheaper than the subgroup ops cost -- so keep it simple.
            if (WITH_COLS && packed != 0xffffffffu) InterlockedMin(sCol[c], packed);
        }

        if (WITH_COLS) {
            GroupMemoryBarrierWithGroupSync();  // sCol complete before it is flushed
            for (uint c = lid.x; c < cnt; c += NQ)
                colPartial[gid.x * nTrain + t0 + c] = sCol[c];
        }
    }

    if (active) result[outA + q] = uint4(bestIdx, best, second, 0);
}

[shader("compute")] [numthreads(64, 1, 1)]
void match_pair(uint3 gid : SV_GroupID, uint3 lid : SV_GroupThreadID) {
    matchImpl<true, TQ>(gid, lid);
}

// Row side only: for callers that skip the cross-check (pair-selection
// scoring). Writes nothing shared between pairs.
[shader("compute")] [numthreads(256, 1, 1)]
void match_rows(uint3 gid : SV_GroupID, uint3 lid : SV_GroupThreadID) {
    matchImpl<false, TQR>(gid, lid);
}

// Fold the per-workgroup column candidates into the B-side result. Only
// bestIdx is consumed downstream (see the header note); secondD2 is written as
// "no second" so a stray reader cannot silently pass a ratio test.
[shader("compute")] [numthreads(64, 1, 1)]
void reduce_cols(uint3 tid : SV_DispatchThreadID) {
    uint j = tid.x;
    if (j >= nTrain) return;

    uint bestD2 = 0xffffffffu, bestIdx = 0xffffffffu;
    for (uint w = 0; w < numWG; w++) {
        uint packed = colPartial[w * nTrain + j];
        if (packed == 0xffffffffu) continue;  // no query in that workgroup
        uint d2 = packed >> 6;
        uint gi = w * TQ + (packed & 63u);
        if (d2 < bestD2 || (d2 == bestD2 && gi < bestIdx)) { bestD2 = d2; bestIdx = gi; }
    }
    result[outB + j] = uint4(bestIdx, bestD2, 0xffffffffu, 0);
}
