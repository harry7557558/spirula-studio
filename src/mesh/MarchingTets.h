#pragma once

// Marching tetrahedra, shared by the opacity-field and TSDF extractors: the
// case table and the cut-edge -> vertex table both build their meshes with.

#include <climits>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace meshing {

// The 6 tet edges as corner-index pairs.
inline constexpr int kTetEdge[6][2] = {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};

// Marching-tetrahedra triangle table. Case bit i is set when occ[i] < iso
// (the "empty" side). Each triangle is 3 edges; an edge is a corner pair.
// Winding follows Paul Bourke's PolygoniseTri so normals are consistent.
struct MTCase { int ntri; int e[2][3][2]; };
inline constexpr MTCase kMT[16] = {
    /*0000*/ {0, {}},
    /*0001*/ {1, {{{0,1},{0,2},{0,3}}}},
    /*0010*/ {1, {{{1,0},{1,3},{1,2}}}},
    /*0011*/ {2, {{{0,3},{0,2},{1,3}}, {{1,3},{1,2},{0,2}}}},
    /*0100*/ {1, {{{2,0},{2,1},{2,3}}}},
    /*0101*/ {2, {{{0,1},{2,3},{0,3}}, {{0,1},{1,2},{2,3}}}},
    /*0110*/ {2, {{{0,1},{1,3},{2,3}}, {{0,1},{0,2},{2,3}}}},
    /*0111*/ {1, {{{3,0},{3,2},{3,1}}}},
    /*1000*/ {1, {{{3,0},{3,2},{3,1}}}},
    /*1001*/ {2, {{{0,1},{1,3},{2,3}}, {{0,1},{0,2},{2,3}}}},
    /*1010*/ {2, {{{0,1},{2,3},{0,3}}, {{0,1},{1,2},{2,3}}}},
    /*1011*/ {1, {{{2,0},{2,1},{2,3}}}},
    /*1100*/ {2, {{{0,3},{0,2},{1,3}}, {{1,3},{1,2},{0,2}}}},
    /*1101*/ {1, {{{1,0},{1,3},{1,2}}}},
    /*1110*/ {1, {{{0,1},{0,2},{0,3}}}},
    /*1111*/ {0, {}},
};

inline int64_t edge_key(int a, int b, int64_t P) {
    int64_t lo = a < b ? a : b, hi = a < b ? b : a;
    return lo * P + hi;
}

// Cut-edge key -> mesh-vertex id, open-addressed. std::unordered_map spends
// ~120 bytes per entry here plus a bucket array sized from the tet count, which
// was 1.0 GB of the peak on a 1M-splat model.
class EdgeTable {
public:
    explicit EdgeTable(size_t expect) {
        size_t cap = 1024;
        while (cap < expect) cap <<= 1;
        keys_.assign(cap, kEmpty);
        vals_.assign(cap, -1);
        mask_ = cap - 1;
    }
    int find(int64_t k) const {
        size_t i = (size_t)mix(k) & mask_;
        while (keys_[i] != kEmpty) {
            if (keys_[i] == k) return vals_[i];
            i = (i + 1) & mask_;
        }
        return -1;
    }
    // The id already stored for `k`, or -1 after storing `id` for it.
    int insert(int64_t k, int id) {
        if ((load_ + 1) * 4 > (mask_ + 1) * 3) grow();
        size_t i = (size_t)mix(k) & mask_;
        while (keys_[i] != kEmpty) {
            if (keys_[i] == k) return vals_[i];
            i = (i + 1) & mask_;
        }
        keys_[i] = k; vals_[i] = id; ++load_;
        return -1;
    }
    void release() {
        std::vector<int64_t>().swap(keys_);
        std::vector<int32_t>().swap(vals_);
        mask_ = 0; load_ = 0;
    }

private:
    static constexpr int64_t kEmpty = INT64_MIN;   // keys are lo*P+hi >= 0
    static uint64_t mix(int64_t k) {
        uint64_t x = (uint64_t)k;
        x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
        return x ^ (x >> 33);
    }
    void grow() {
        const size_t cap = (mask_ + 1) * 2;
        std::vector<int64_t> nk(cap, kEmpty);
        std::vector<int32_t> nv(cap, -1);
        const size_t nmask = cap - 1;
        for (size_t j = 0; j <= mask_; ++j) {
            if (keys_[j] == kEmpty) continue;
            size_t i = (size_t)mix(keys_[j]) & nmask;
            while (nk[i] != kEmpty) i = (i + 1) & nmask;
            nk[i] = keys_[j]; nv[i] = vals_[j];
        }
        keys_.swap(nk); vals_.swap(nv); mask_ = nmask;
    }
    std::vector<int64_t> keys_;
    std::vector<int32_t> vals_;
    size_t mask_ = 0, load_ = 0;
};

} // namespace meshing
