// Device sort / scan / stream-compaction kernels behind
// backend/common/SortScan.h (Vulkan backend). See the design notes in
// src/backend/vulkan/README.md.
//
// Radix sort: classic 3-phase LSD — per-partition histogram, spine
// (per-digit exclusive scan across partitions + cross-digit base scan),
// rank-and-scatter — 8 bits per pass with begin_bit/end_bit masking, stable,
// 32-bit or 64-bit keys (keys are compared UNSIGNED within the masked bit
// range; every call site sorts non-negative keys). No device-scope atomics:
// per-digit totals fall out of the spine scans.
//
// Subgroup-size handling: ballot-based ranking uses uint4 masks (lanes up to
// 128) and never assumes a width; the host asserts subgroup size >= 8 so the
// per-subgroup scratch bound (MAX_NSG) holds.

#include "int64_compat.slang"
#include "int8_compat.slang"

// ---------------------------------------------------------------------------
// shared configuration (host mirrors: backend/vulkan/SortScanVulkan.cpp)
// ---------------------------------------------------------------------------

#define SORT_WG 128         // threads per sort workgroup
#define SORT_ELEMS 16       // keys per thread
#define SORT_PART 2048      // SORT_WG * SORT_ELEMS
#define RADIX 256
#define MAX_NSG 16          // SORT_WG / min supported subgroup size (8)

#define SCAN_WG 256         // threads per scan workgroup
#define SCAN_ELEMS 8        // elements per thread
#define SCAN_BLOCK 2048     // SCAN_WG * SCAN_ELEMS

// ---------------------------------------------------------------------------
// subgroup ballot helpers (uint4 masks, subgroup width <= 128)
// ---------------------------------------------------------------------------

uint popcount4(uint4 v) {
    return countbits(v.x) + countbits(v.y) + countbits(v.z) + countbits(v.w);
}

// Bits [0, lane) set.
uint4 lane_lt_mask(uint lane) {
    uint word = lane >> 5;
    uint bit = lane & 31;
    uint4 m = uint4(0);
    [ForceUnroll]
    for (uint w = 0; w < 4; w++) {
        if (w < word) m[w] = 0xffffffffu;
        else if (w == word) m[w] = (bit == 0) ? 0u : (0xffffffffu >> (32 - bit));
    }
    return m;
}

uint lsb4(uint4 m) {
    if (m.x != 0) return firstbitlow(m.x);
    if (m.y != 0) return 32 + firstbitlow(m.y);
    if (m.z != 0) return 64 + firstbitlow(m.z);
    if (m.w != 0) return 96 + firstbitlow(m.w);
    return 0xffffffffu;
}

// Mask of active lanes whose `digit` equals this lane's, restricted to
// `valid` lanes. All lanes must call this (uniform control flow).
uint4 match_digit(uint digit, bool valid) {
    uint4 mask = WaveActiveBallot(valid);
    [ForceUnroll]
    for (uint b = 0; b < 8; b++) {
        bool bit = ((digit >> b) & 1u) != 0;
        uint4 bal = WaveActiveBallot(bit);
        mask &= bit ? bal : ~bal;
    }
    return mask;
}

// ---------------------------------------------------------------------------
// radix sort: histogram
// ---------------------------------------------------------------------------

struct HistParams {
    uint32_t* keys;      // u32 or u64 elements depending on entry (cast)
    uint32_t* part_hist; // [RADIX][num_parts], digit-major
    uint32_t n;
    uint32_t shift;
    uint32_t digit_mask;
    uint32_t num_parts;
    uint32_t parts_per_row;  // grid fold: part = wg.y * parts_per_row + wg.x
};

groupshared uint _hist_local[RADIX];

// KEY_LOAD(params, idx) yields the uint digit source.
#define DEF_HIST(NAME, KTYPE)                                              \
[shader("compute")]                                                        \
[numthreads(SORT_WG, 1, 1)]                                                \
void NAME(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,                \
          uniform HistParams p) {                                          \
    uint part = wg.y * p.parts_per_row + wg.x;                             \
    if (part >= p.num_parts) return;                                       \
    for (uint i = tid; i < RADIX; i += SORT_WG) _hist_local[i] = 0;        \
    GroupMemoryBarrierWithGroupSync();                                     \
    KTYPE* keys = (KTYPE*)p.keys;                                          \
    gindex_t base = gindex_t(part) * SORT_PART;                            \
    for (uint e = 0; e < SORT_ELEMS; e++) {                                \
        gindex_t idx = base + e * SORT_WG + tid;                           \
        if (idx < gindex_t(p.n)) {                                         \
            uint d = u64key_digit(elem_at(keys, idx)[0], p.shift,          \
                                  p.digit_mask);                           \
            InterlockedAdd(_hist_local[d], 1u);                            \
        }                                                                  \
    }                                                                      \
    GroupMemoryBarrierWithGroupSync();                                     \
    for (uint d = tid; d < RADIX; d += SORT_WG)                            \
        p.part_hist[d * p.num_parts + part] = _hist_local[d];              \
}

DEF_HIST(sort_hist_u32, uint32_t)
DEF_HIST(sort_hist_u64, u64key_t)

// ---------------------------------------------------------------------------
// radix sort: spine
// ---------------------------------------------------------------------------

struct SpineLocalParams {
    uint32_t* part_hist;   // in: counts; out: per-digit exclusive prefixes
    uint32_t* digit_total; // [RADIX] out
    uint32_t num_parts;
};

groupshared uint _spinel_tmp[SORT_WG];

// One workgroup per digit: exclusive scan of part_hist[d][*] in place,
// total to digit_total[d].
[shader("compute")]
[numthreads(SORT_WG, 1, 1)]
void sort_spine_local(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform SpineLocalParams p) {
    uint d = wg.x;
    uint running = 0;
    for (uint chunk = 0; chunk < p.num_parts; chunk += SORT_WG) {
        uint idx = chunk + tid;
        uint v = idx < p.num_parts ? p.part_hist[d * p.num_parts + idx] : 0;
        // Hillis-Steele inclusive scan over the workgroup.
        _spinel_tmp[tid] = v;
        GroupMemoryBarrierWithGroupSync();
        [ForceUnroll]
        for (uint off = 1; off < SORT_WG; off <<= 1) {
            uint x = tid >= off ? _spinel_tmp[tid - off] : 0;
            GroupMemoryBarrierWithGroupSync();
            _spinel_tmp[tid] += x;
            GroupMemoryBarrierWithGroupSync();
        }
        uint incl = _spinel_tmp[tid];
        if (idx < p.num_parts)
            p.part_hist[d * p.num_parts + idx] = running + incl - v;
        uint chunk_total = _spinel_tmp[SORT_WG - 1];
        GroupMemoryBarrierWithGroupSync();
        running += chunk_total;
    }
    if (tid == 0) p.digit_total[d] = running;
}

struct SpineGlobalParams {
    uint32_t* digit_total;  // [RADIX] in
    uint32_t* global_base;  // [RADIX] out (exclusive scan)
};

groupshared uint _spineg_tmp[RADIX];

// Single workgroup: exclusive scan of the 256 digit totals.
[shader("compute")]
[numthreads(RADIX, 1, 1)]
void sort_spine_global(uint tid: SV_GroupThreadID,
                       uniform SpineGlobalParams p) {
    uint v = p.digit_total[tid];
    _spineg_tmp[tid] = v;
    GroupMemoryBarrierWithGroupSync();
    [ForceUnroll]
    for (uint off = 1; off < RADIX; off <<= 1) {
        uint x = tid >= off ? _spineg_tmp[tid - off] : 0;
        GroupMemoryBarrierWithGroupSync();
        _spineg_tmp[tid] += x;
        GroupMemoryBarrierWithGroupSync();
    }
    p.global_base[tid] = _spineg_tmp[tid] - v;
}

// ---------------------------------------------------------------------------
// radix sort: rank and scatter (stable)
// ---------------------------------------------------------------------------

struct DownsweepParams {
    uint32_t* keys_in;   // cast to the entry's key type
    uint32_t* keys_out;
    uint32_t* vals_in;
    uint32_t* vals_out;
    uint32_t* part_offsets;  // [RADIX][num_parts] per-digit exclusive prefix
    uint32_t* global_base;   // [RADIX]
    uint32_t n;
    uint32_t shift;
    uint32_t digit_mask;
    uint32_t num_parts;
    uint32_t parts_per_row;
};

// Ranking scratch. Iterations process SORT_WG contiguous keys; stability
// needs rank = (prior iterations) + (prior subgroups this iteration) +
// (rank within subgroup), all in memory order.
groupshared uint _ds_sg_hist[RADIX * MAX_NSG];  // [digit][subgroup]
// Write cursor per digit: the partition's global base, advanced by each
// iteration's counts. Holding the two global bases here keeps them out of the
// inner loop, which read part_offsets once per key on a num_parts stride.
groupshared uint _ds_cursor[RADIX];

#define DEF_DOWNSWEEP(NAME, KTYPE)                                         \
[shader("compute")]                                                        \
[numthreads(SORT_WG, 1, 1)]                                                \
void NAME(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,                \
          uniform DownsweepParams p) {                                     \
    uint part = wg.y * p.parts_per_row + wg.x;                             \
    if (part >= p.num_parts) return;                                       \
    KTYPE* keys_in = (KTYPE*)p.keys_in;                                    \
    KTYPE* keys_out = (KTYPE*)p.keys_out;                                  \
    uint lane = WaveGetLaneIndex();                                        \
    uint sg = tid / WaveGetLaneCount();                                    \
    uint nsg = (SORT_WG + WaveGetLaneCount() - 1) / WaveGetLaneCount();    \
    uint4 lt = lane_lt_mask(lane);                                         \
    for (uint i = tid; i < RADIX; i += SORT_WG)                            \
        _ds_cursor[i] = p.global_base[i] +                                 \
                        p.part_offsets[i * p.num_parts + part];            \
    for (uint i = tid; i < RADIX * MAX_NSG; i += SORT_WG)                  \
        _ds_sg_hist[i] = 0;                                                \
    GroupMemoryBarrierWithGroupSync();                                     \
    gindex_t base = gindex_t(part) * SORT_PART;                            \
    for (uint e = 0; e < SORT_ELEMS; e++) {                                \
        gindex_t idx = base + e * SORT_WG + tid;                           \
        bool valid = idx < gindex_t(p.n);                                  \
        KTYPE key = valid ? elem_at(keys_in, idx)[0] : KTYPE(0);           \
        uint d = u64key_digit(key, p.shift, p.digit_mask);                     \
        uint4 match = match_digit(d, valid);                               \
        uint rank_sg = popcount4(match & lt);                              \
        bool leader = valid && (lsb4(match) == lane);                      \
        if (leader) _ds_sg_hist[d * MAX_NSG + sg] = popcount4(match);      \
        GroupMemoryBarrierWithGroupSync();                                 \
        if (valid) {                                                       \
            uint prior_sg = 0;                                             \
            for (uint s = 0; s < sg; s++)                                  \
                prior_sg += _ds_sg_hist[d * MAX_NSG + s];                  \
            gindex_t pos = gindex_t(_ds_cursor[d] + prior_sg + rank_sg);   \
            elem_at(keys_out, pos)[0] = key;                               \
            elem_at(p.vals_out, pos)[0] = elem_at(p.vals_in, idx)[0];      \
        }                                                                  \
        GroupMemoryBarrierWithGroupSync();                                 \
        /* fold this iteration's counts into the running totals and */     \
        /* clear the per-subgroup scratch for the next iteration */        \
        for (uint dd = tid; dd < RADIX; dd += SORT_WG) {                   \
            uint total = 0;                                                \
            for (uint s = 0; s < nsg; s++) {                               \
                total += _ds_sg_hist[dd * MAX_NSG + s];                    \
                _ds_sg_hist[dd * MAX_NSG + s] = 0;                         \
            }                                                              \
            _ds_cursor[dd] += total;                                       \
        }                                                                  \
        GroupMemoryBarrierWithGroupSync();                                 \
    }                                                                      \
}

DEF_DOWNSWEEP(sort_downsweep_u32, uint32_t)
DEF_DOWNSWEEP(sort_downsweep_u64, u64key_t)

// ---------------------------------------------------------------------------
// two-level scans (inclusive_sum / exclusive_sum)
// ---------------------------------------------------------------------------

// Spec constant 0: 1 = inclusive, 0 = exclusive.
[SpecializationConstant]
const int kInclusive = 0;

// int64 scan element. The emulated form is a (lo, hi) = (x, y) uint2 with
// explicit carry; the memory image matches the native int64 buffer layout,
// so the same entry names serve both blob variants. The scan macros add and
// subtract through the acc_add/acc_sub overloads below rather than +/- so
// the pair form can carry. A uint2 rather than a two-field struct: the
// Intel Windows driver (igvk64.dll, observed on Gen9.5 UHD) segfaults in
// vkCreateComputePipelines on this kernel's struct form (groupshared struct
// array + struct loads through buffer pointers), while vector types are
// exercised by every backend kernel and compile everywhere.
#ifndef SS_EMULATE_INT64
typedef int64_t i64acc_t;
int64_t acc_add(int64_t a, int64_t b) { return a + b; }
int64_t acc_sub(int64_t a, int64_t b) { return a - b; }
#else
typedef uint2 i64acc_t;
uint2 acc_add(uint2 a, uint2 b) {
    uint lo = a.x + b.x;
    return uint2(lo, a.y + b.y + (lo < a.x ? 1u : 0u));
}
uint2 acc_sub(uint2 a, uint2 b) {
    return uint2(a.x - b.x, a.y - b.y - (a.x < b.x ? 1u : 0u));
}
#endif
int acc_add(int a, int b) { return a + b; }
int acc_sub(int a, int b) { return a - b; }
float acc_add(float a, float b) { return a + b; }
float acc_sub(float a, float b) { return a - b; }

struct ScanParams {
    uint32_t* input;       // typed per entry (cast)
    uint32_t* output;
    uint32_t* block_sums;  // [num_blocks]
    uint32_t n;
    uint32_t blocks_per_row;
};

groupshared i64acc_t _scan_tmp_i64[SCAN_WG];
groupshared int _scan_tmp_i32[SCAN_WG];
groupshared float _scan_tmp_f32[SCAN_WG];

// uint8 flag arrays go through int8_compat.slang: packed-word extraction on
// the baseline (allocations are 16-byte rounded, so the word covering byte
// n-1 stays in bounds), native byte loads in the ".int8" variant.
int load_flag_byte(uint32_t* words, uint idx) {
    return int(u8_load(words, idx));
}
// gindex_t overload, per the int8_compat convention: without it the scan
// macros' 64-bit index would narrow to uint at the call.
int load_flag_byte(uint32_t* words, gindex_t idx) {
    return int(u8_load(words, idx));
}

// LOAD(idx) is an expression over `p` yielding T; blocked layout (each
// thread owns SCAN_ELEMS consecutive elements) keeps intra-thread order
// trivial.
#define DEF_SCAN_BLOCKS(NAME, T, TMP, LOAD)                                \
[shader("compute")]                                                        \
[numthreads(SCAN_WG, 1, 1)]                                                \
void NAME(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,                \
          uniform ScanParams p) {                                          \
    uint block = wg.y * p.blocks_per_row + wg.x;                           \
    if (block * SCAN_BLOCK >= p.n) return;  /* 65535 fold overshoot */     \
    gindex_t base = gindex_t(block) * SCAN_BLOCK + tid * SCAN_ELEMS;       \
    T* output = (T*)p.output;                                              \
    T* block_sums = (T*)p.block_sums;                                      \
    T loaded[SCAN_ELEMS];                                                  \
    T v[SCAN_ELEMS];                                                       \
    [ForceUnroll]                                                          \
    for (uint e = 0; e < SCAN_ELEMS; e++) {                                \
        gindex_t idx = base + e;                                           \
        loaded[e] = idx < gindex_t(p.n) ? (LOAD(idx)) : T(0);              \
        v[e] = loaded[e];                                                  \
    }                                                                      \
    T thread_total = T(0);                                                 \
    [ForceUnroll]                                                          \
    for (uint e = 0; e < SCAN_ELEMS; e++) {                                \
        T x = v[e];                                                        \
        v[e] = thread_total; /* exclusive within thread */                 \
        thread_total = acc_add(thread_total, x);                           \
    }                                                                      \
    TMP[tid] = thread_total;                                               \
    GroupMemoryBarrierWithGroupSync();                                     \
    [ForceUnroll]                                                          \
    for (uint off = 1; off < SCAN_WG; off <<= 1) {                         \
        T x = tid >= off ? TMP[tid - off] : T(0);                          \
        GroupMemoryBarrierWithGroupSync();                                 \
        TMP[tid] = acc_add(TMP[tid], x);                                   \
        GroupMemoryBarrierWithGroupSync();                                 \
    }                                                                      \
    T thread_base = acc_sub(TMP[tid], thread_total); /* excl across thr */ \
    [ForceUnroll]                                                          \
    for (uint e = 0; e < SCAN_ELEMS; e++) {                                \
        gindex_t idx = base + e;                                           \
        if (idx < gindex_t(p.n)) {                                         \
            T excl = acc_add(thread_base, v[e]);                           \
            elem_at(output, idx)[0] = kInclusive != 0                      \
                ? acc_add(excl, loaded[e]) : excl;                         \
        }                                                                  \
    }                                                                      \
    if (tid == SCAN_WG - 1)                                                \
        block_sums[block] = acc_add(thread_base, thread_total);            \
}

#define LOAD_I32(idx) (elem_at((int*)p.input, idx)[0])
#define LOAD_I64(idx) (elem_at((i64acc_t*)p.input, idx)[0])
#define LOAD_F32(idx) (elem_at((float*)p.input, idx)[0])
#define LOAD_FLAG(idx) (load_flag_byte((uint32_t*)p.input, idx) != 0 ? 1 : 0)

DEF_SCAN_BLOCKS(scan_blocks_i32, int, _scan_tmp_i32, LOAD_I32)
DEF_SCAN_BLOCKS(scan_blocks_i64, i64acc_t, _scan_tmp_i64, LOAD_I64)
// float sums (densify MCMC sampling cumsum). The blocked layout makes the
// summation order deterministic per n, but it is NOT the same association
// as CUB's device scan — cross-backend float cumsums agree only to rounding.
DEF_SCAN_BLOCKS(scan_blocks_f32, float, _scan_tmp_f32, LOAD_F32)
// bool/uint8 flags scanned into an int32 output (select_flagged).
DEF_SCAN_BLOCKS(scan_blocks_flags, int, _scan_tmp_i32, LOAD_FLAG)

struct ScanSpineParams {
    uint32_t* block_sums;  // scanned exclusively in place (cast)
    uint32_t num_blocks;
};

#define DEF_SCAN_SPINE(NAME, T, TMP)                                       \
[shader("compute")]                                                        \
[numthreads(SCAN_WG, 1, 1)]                                                \
void NAME(uint tid: SV_GroupThreadID, uniform ScanSpineParams p) {         \
    T* sums = (T*)p.block_sums;                                            \
    T running = T(0);                                                      \
    for (uint chunk = 0; chunk < p.num_blocks; chunk += SCAN_WG) {         \
        uint idx = chunk + tid;                                            \
        T v = idx < p.num_blocks ? sums[idx] : T(0);                       \
        TMP[tid] = v;                                                      \
        GroupMemoryBarrierWithGroupSync();                                 \
        [ForceUnroll]                                                      \
        for (uint off = 1; off < SCAN_WG; off <<= 1) {                     \
            T x = tid >= off ? TMP[tid - off] : T(0);                      \
            GroupMemoryBarrierWithGroupSync();                             \
            TMP[tid] = acc_add(TMP[tid], x);                               \
            GroupMemoryBarrierWithGroupSync();                             \
        }                                                                  \
        if (idx < p.num_blocks)                                            \
            sums[idx] = acc_add(running, acc_sub(TMP[tid], v));            \
        T chunk_total = TMP[SCAN_WG - 1];                                  \
        GroupMemoryBarrierWithGroupSync();                                 \
        running = acc_add(running, chunk_total);                           \
    }                                                                      \
}

DEF_SCAN_SPINE(scan_spine_i32, int, _scan_tmp_i32)
DEF_SCAN_SPINE(scan_spine_i64, i64acc_t, _scan_tmp_i64)
DEF_SCAN_SPINE(scan_spine_f32, float, _scan_tmp_f32)

struct ScanAddParams {
    uint32_t* output;
    uint32_t* block_sums;  // exclusive block bases (cast)
    uint32_t n;
    uint32_t blocks_per_row;
};

#define DEF_SCAN_ADD(NAME, T)                                              \
[shader("compute")]                                                        \
[numthreads(SCAN_WG, 1, 1)]                                                \
void NAME(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,                \
          uniform ScanAddParams p) {                                       \
    uint block = wg.y * p.blocks_per_row + wg.x;                           \
    if (block * SCAN_BLOCK >= p.n) return;  /* 65535 fold overshoot */     \
    T* output = (T*)p.output;                                              \
    T addend = ((T*)p.block_sums)[block];                                  \
    gindex_t base = gindex_t(block) * SCAN_BLOCK + tid * SCAN_ELEMS;       \
    [ForceUnroll]                                                          \
    for (uint e = 0; e < SCAN_ELEMS; e++) {                                \
        gindex_t idx = base + e;                                           \
        if (idx < gindex_t(p.n)) elem_at(output, idx)[0] =                 \
            acc_add(elem_at(output, idx)[0], addend);                      \
    }                                                                      \
}

DEF_SCAN_ADD(scan_add_i32, int)
DEF_SCAN_ADD(scan_add_i64, i64acc_t)
DEF_SCAN_ADD(scan_add_f32, float)

// ---------------------------------------------------------------------------
// stream compaction (select_flagged)
// ---------------------------------------------------------------------------

struct ScatterFlaggedParams {
    uint32_t* input;
    uint32_t* flags;  // uint8 array read as packed words (see load_flag_byte)
    int* scan;        // exclusive scan of flags
    uint32_t* output;
    uint32_t n;
    uint32_t blocks_per_row;  // SCAN_BLOCK-element blocks, matching the scan
};

[shader("compute")]
[numthreads(SCAN_WG, 1, 1)]
void scatter_flagged_u32(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform ScatterFlaggedParams p) {
    uint block = wg.y * p.blocks_per_row + wg.x;
    gindex_t base = gindex_t(block) * SCAN_BLOCK + tid * SCAN_ELEMS;
    [ForceUnroll]
    for (uint e = 0; e < SCAN_ELEMS; e++) {
        gindex_t idx = base + e;
        if (idx < gindex_t(p.n) && load_flag_byte(p.flags, idx) != 0)
            elem_at(p.output, gindex_t(elem_at(p.scan, idx)[0]))[0] =
                elem_at(p.input, idx)[0];
    }
}
