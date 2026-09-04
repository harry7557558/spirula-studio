/*
 * MeshingHost.cpp
 *
 * Host-side orchestration of the 3DGS meshing pipeline (see Meshing.h):
 *   point cloud -> Delaunay -> marching tetrahedra (bisection on cut edges) ->
 *   manifold-preserving short-edge merge -> binary PLY.
 *
 * The GPU work (occupancy field, bisection) lives in Meshing.cu behind
 * OccupancyEvaluator; everything here is plain multithreaded host C++.
 */

#include "mesh/Meshing.h"
#include "mesh/MeshExport.h"
#include "mesh/MeshUV.h"
#include "mesh/Delaunay3D.h"
#include "mesh/MeshLog.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <stdexcept>

#include "core/Env.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

namespace mmsg = spirula::i18n::msg::mesh;

namespace {

using Clock = std::chrono::steady_clock;
static double secs_since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

// The 6 tet edges as corner-index pairs.
static const int kTetEdge[6][2] = {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};

// Marching-tetrahedra triangle table. Case bit i is set when occ[i] < iso
// (the "empty" side). Each triangle is 3 edges; an edge is a corner pair.
// Winding follows Paul Bourke's PolygoniseTri so normals are consistent.
struct MTCase { int ntri; int e[2][3][2]; };
static const MTCase kMT[16] = {
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

static inline int64_t edge_key(int a, int b, int64_t P) {
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

// ---------------------------------------------------------------------------
// Manifold-preserving short-edge merge.
//
// Collapses mesh edges shorter than a local threshold (merge_factor times the
// average incident edge length at the shorter end). A collapse is only applied
// when it satisfies the edge-collapse link condition, so anything locally
// manifold stays manifold.
// ---------------------------------------------------------------------------
// The pipeline operates on the shared export container (V/F/C, plus N/UV/
// texture filled by the export stage). Post-processing only touches V/F/C.
using Mesh = MeshData;

// Lock-free atomic min on a 64-bit slot (CAS loop).
static inline void atomic_min_u64(uint64_t* p, uint64_t val) {
    uint64_t old = *p;
    while (val < old) {
#ifdef _MSC_VER
        uint64_t prev = (uint64_t)_InterlockedCompareExchange64(
            reinterpret_cast<volatile long long*>(p), (long long)val, (long long)old);
#else
        uint64_t prev = __sync_val_compare_and_swap(p, old, val);
#endif
        if (prev == old) break;
        old = prev;
    }
}
// Order-preserving bit key for a non-negative float (priority by edge length).
static inline uint32_t float_key(float f) {
    uint32_t u; std::memcpy(&u, &f, 4); return u;
}

// A vertex's neighbours (or incident triangles) during the merge. Valences are
// ~6, so a linear scan over inline storage beats hashing, and it costs 40 bytes
// per vertex where std::unordered_set cost ~330 (5.8 GB of an 8.3 GB peak).
class VertexSet {
public:
    VertexSet() {}
    ~VertexSet() { if (cap_ > kInline) std::free(heap_); }
    VertexSet(const VertexSet&) = delete;
    VertexSet& operator=(const VertexSet&) = delete;
    VertexSet(VertexSet&& o) noexcept : n_(o.n_), cap_(o.cap_) {
        if (cap_ > kInline) heap_ = o.heap_;
        else std::memcpy(inl_, o.inl_, sizeof inl_);
        o.n_ = 0; o.cap_ = kInline;
    }

    const int* begin() const { return data(); }
    const int* end()   const { return data() + n_; }
    uint32_t size() const { return n_; }
    bool empty() const { return n_ == 0; }

    const int* find(int x) const {
        const int* d = data();
        for (uint32_t i = 0; i < n_; ++i) if (d[i] == x) return d + i;
        return d + n_;
    }
    void insert(int x) {
        int* d = data();
        for (uint32_t i = 0; i < n_; ++i) if (d[i] == x) return;
        if (n_ == cap_) { grow(); d = data(); }
        d[n_++] = x;
    }
    void erase(int x) {
        int* d = data();
        for (uint32_t i = 0; i < n_; ++i)
            if (d[i] == x) { d[i] = d[--n_]; return; }
    }
    void clear() {
        if (cap_ > kInline) { std::free(heap_); cap_ = kInline; }
        n_ = 0;
    }
    // Drop entries flagged dead. Every reader already skips them, so this only
    // stops a collapse chain from accumulating them without bound.
    void retain(const char* alive) {
        int* d = data();
        uint32_t k = 0;
        for (uint32_t i = 0; i < n_; ++i) if (alive[d[i]]) d[k++] = d[i];
        n_ = k;
    }

private:
    static constexpr uint32_t kInline = 8;
    int* data() { return cap_ > kInline ? heap_ : inl_; }
    const int* data() const { return cap_ > kInline ? heap_ : inl_; }
    void grow() {
        uint32_t nc = cap_ * 2;
        int* np = (int*)std::malloc((size_t)nc * sizeof(int));
        std::memcpy(np, data(), (size_t)n_ * sizeof(int));
        if (cap_ > kInline) std::free(heap_);
        heap_ = np; cap_ = nc;
    }
    uint32_t n_ = 0, cap_ = kInline;
    union { int inl_[kInline]; int* heap_; };
};

// Parallel manifold-preserving short-edge merge.
//
// Collapses mesh edges shorter than a local threshold (merge_factor times the
// average incident edge length at the shorter end), only when the edge-collapse
// link condition holds (so anything locally manifold stays manifold).
//
// Parallelism is round-based: each round selects a set of collapsible edges
// whose closed 1-rings are pairwise disjoint, then collapses them concurrently.
// Independence is decided by a "claim" pass -- every candidate writes a
// (length, id) priority into the claim slot of each vertex in its closed
// neighborhood via atomic-min; an edge wins iff it owns every slot it touched.
// The global-shortest candidate always wins, so each round makes progress, and
// the winners' disjoint neighborhoods make the concurrent collapses race-free.
// The outcome is deterministic regardless of thread count.
static void merge_vertices(Mesh& mesh, float merge_factor, float max_flip_deg,
                           bool verbose, int num_threads) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (nv == 0 || merge_factor <= 0.0f) return;
#ifdef _OPENMP
    if (num_threads > 0) omp_set_num_threads(num_threads);
    int nthreads = omp_get_max_threads();
#else
    int nthreads = 1;
#endif

    std::vector<std::array<float,3>>& V = mesh.V;
    std::vector<std::array<int,3>>& F = mesh.F;
    std::vector<VertexSet> adj(nv), vt(nv);
    std::vector<char> valive(nv, 1);
    std::vector<char> talive(nf, 1);

    #pragma omp parallel for schedule(static)
    for (long t = 0; t < nf; ++t) {
        const auto& f = F[t];
        if (f[0]==f[1] || f[1]==f[2] || f[0]==f[2]) talive[t] = 0;
    }

    // adjacency + incident triangles, built lock-free: each thread owns a
    // vertex range and only writes the sets it owns (scans all faces).
    #pragma omp parallel
    {
    #ifdef _OPENMP
        int nt = omp_get_num_threads(), tid = omp_get_thread_num();
    #else
        int nt = 1, tid = 0;
    #endif
        long lo = (long)nv * tid / nt, hi = (long)nv * (tid + 1) / nt;
        for (long t = 0; t < nf; ++t) {
            if (!talive[t]) continue;
            const auto& f = F[t];
            for (int k = 0; k < 3; ++k) {
                int w = f[k];
                if (w >= lo && w < hi) {
                    adj[w].insert(f[(k+1)%3]);
                    adj[w].insert(f[(k+2)%3]);
                    vt[w].insert((int)t);
                }
            }
        }
    }

    auto dist = [&](int a, int b) {
        float dx = V[a][0]-V[b][0], dy = V[a][1]-V[b][1], dz = V[a][2]-V[b][2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    std::vector<float> Lavg(nv, 0.0f);
    #pragma omp parallel for schedule(dynamic, 4096)
    for (int v = 0; v < nv; ++v) {
        if (adj[v].empty()) continue;
        float s = 0; for (int w : adj[v]) s += dist(v, w);
        Lavg[v] = s / (float)adj[v].size();
    }

    // link condition for edge (u,v): common neighbors of u and v must be
    // exactly the third vertices of triangles on that edge (1 boundary, 2
    // interior). Read-only on the current topology.
    auto link_ok = [&](int u, int v) -> bool {
        int opp0 = -1, opp1 = -1, opp_cnt = 0;
        for (int t : vt[u]) {
            if (!talive[t]) continue;
            const auto& f = F[t];
            bool hu=false, hv=false; int other=-1;
            for (int k=0;k<3;k++){ if(f[k]==u)hu=true; else if(f[k]==v)hv=true; else other=f[k]; }
            if (hu && hv) {
                if (opp_cnt == 0) opp0 = other;
                else if (opp_cnt == 1) opp1 = other;
                else return false;   // >2 faces on edge -> already non-manifold
                ++opp_cnt;
            }
        }
        const auto& au = adj[u]; const auto& av = adj[v];
        const auto& small = (au.size() < av.size()) ? au : av;
        const auto& big   = (au.size() < av.size()) ? av : au;
        int common = 0;
        for (int w : small) {
            if (w == u || w == v) continue;
            if (big.find(w) != big.end()) {
                ++common;
                if (w != opp0 && w != opp1) return false;
            }
        }
        return common == opp_cnt;
    };

    // Geometric guard: reject a collapse of edge (u,v) -> midpoint `np` if it
    // would fold the surface or create a degenerate face. For every alive face
    // incident to u or v (other than the two faces on the collapsing edge), the
    // reshaped face's normal must not rotate past max_flip_deg, and must not go
    // (near-)degenerate. Read-only on the current topology (winners' 1-rings are
    // disjoint within a round, so this is race-free in the parallel collapse).
    const float cos_flip = (max_flip_deg > 0.0f && max_flip_deg < 180.0f)
                               ? std::cos(max_flip_deg * 0.017453292519943295f)
                               : -2.0f;  // <= -1 sentinel: guard disabled
    auto tri_normal = [&](const float* a, const float* b, const float* c,
                          float& nx, float& ny, float& nz) {
        float e1x=b[0]-a[0], e1y=b[1]-a[1], e1z=b[2]-a[2];
        float e2x=c[0]-a[0], e2y=c[1]-a[1], e2z=c[2]-a[2];
        nx = e1y*e2z - e1z*e2y; ny = e1z*e2x - e1x*e2z; nz = e1x*e2y - e1y*e2x;
    };
    auto geom_ok = [&](int u, int v, const float* np) -> bool {
        if (cos_flip <= -1.0f) return true;                 // guard disabled
        for (int pass = 0; pass < 2; ++pass) {
            int center = pass == 0 ? u : v;                 // the vertex moving to np
            for (int t : vt[center]) {
                if (!talive[t]) continue;
                const auto& f = F[t];
                bool hu=false, hv=false;
                for (int k=0;k<3;k++){ if(f[k]==u)hu=true; else if(f[k]==v)hv=true; }
                if (hu && hv) continue;                     // face on the collapsed edge
                const float* q0 = (f[0]==center) ? np : V[f[0]].data();
                const float* q1 = (f[1]==center) ? np : V[f[1]].data();
                const float* q2 = (f[2]==center) ? np : V[f[2]].data();
                float ox,oy,oz, nx,ny,nz;
                tri_normal(V[f[0]].data(), V[f[1]].data(), V[f[2]].data(), ox,oy,oz);
                tri_normal(q0, q1, q2, nx, ny, nz);
                float lo = std::sqrt(ox*ox+oy*oy+oz*oz);
                float ln = std::sqrt(nx*nx+ny*ny+nz*nz);
                if (ln < 1e-12f) return false;              // collapse -> degenerate
                if (lo > 1e-12f &&
                    (ox*nx + oy*ny + oz*nz) / (lo*ln) < cos_flip) return false;  // fold
            }
        }
        return true;
    };

    std::vector<uint64_t> claim(nv);
    const uint64_t CLAIM_MAX = ~0ull;
    struct Cand { int u, v; uint32_t lk; };  // lk = length priority key

    // ---- gather the short-edge candidates ONCE (parallel) ----
    // The link condition is (re)checked when a candidate is actually selected,
    // not here -- like the original single pass, this avoids re-scanning the
    // whole mesh every round.
    std::vector<std::vector<Cand>> loc(nthreads);
    #pragma omp parallel
    {
    #ifdef _OPENMP
        int tid = omp_get_thread_num();
    #else
        int tid = 0;
    #endif
        auto& L = loc[tid];
        #pragma omp for schedule(dynamic, 4096)
        for (int u = 0; u < nv; ++u) {
            for (int w : adj[u]) {
                if (w <= u) continue;
                float len = dist(u, w);
                float thr = merge_factor * std::min(Lavg[u], Lavg[w]);
                if (len < thr) L.push_back({u, w, float_key(len)});
            }
        }
    }
    std::vector<Cand> cands;
    { size_t n=0; for (auto& l : loc) n += l.size(); cands.reserve(n); }
    for (auto& l : loc) cands.insert(cands.end(), l.begin(), l.end());

    // ---- process in rounds over a shrinking candidate list ----
    // Each round selects winners whose closed 1-rings are pairwise disjoint
    // (claim/atomic-min by length priority) and collapses them concurrently.
    // A winner is then removed whether it collapsed or was found invalid (dead
    // endpoint / edge gone / no longer short / link condition) -- the global
    // shortest candidate always wins, so every round removes >=1 candidate and
    // the loop terminates.
    long total_collapses = 0;
    std::vector<char> done;
    while (!cands.empty()) {
        const int NC = (int)cands.size();
        done.assign(NC, 0);

        #pragma omp parallel for schedule(static)
        for (int v = 0; v < nv; ++v) claim[v] = CLAIM_MAX;

        #pragma omp parallel for schedule(dynamic, 1024)
        for (int ci = 0; ci < NC; ++ci) {
            int u = cands[ci].u, v = cands[ci].v;
            if (!valive[u] || !valive[v]) continue;
            uint64_t key = ((uint64_t)cands[ci].lk << 32) | (uint32_t)ci;
            atomic_min_u64(&claim[u], key);
            atomic_min_u64(&claim[v], key);
            for (int w : adj[u]) atomic_min_u64(&claim[w], key);
            for (int w : adj[v]) atomic_min_u64(&claim[w], key);
        }

        long round_collapses = 0;
        #pragma omp parallel for schedule(dynamic, 1024) reduction(+:round_collapses)
        for (int ci = 0; ci < NC; ++ci) {
            int u = cands[ci].u, v = cands[ci].v;
            if (!valive[u] || !valive[v]) { done[ci] = 1; continue; }
            uint64_t key = ((uint64_t)cands[ci].lk << 32) | (uint32_t)ci;
            if (claim[u] != key || claim[v] != key) continue;  // not a winner; retry
            bool win = true;
            for (int w : adj[u]) if (claim[w] != key) { win = false; break; }
            if (win) for (int w : adj[v]) if (claim[w] != key) { win = false; break; }
            if (!win) continue;

            // winner: consumed this round regardless of the outcome below
            done[ci] = 1;
            if (adj[u].find(v) == adj[u].end()) continue;           // edge gone
            float len = dist(u, v);
            if (len >= merge_factor * std::min(Lavg[u], Lavg[v])) continue;  // not short now
            if (!link_ok(u, v)) continue;                           // would break manifold
            float np[3] = {0.5f*(V[u][0]+V[v][0]), 0.5f*(V[u][1]+V[v][1]),
                           0.5f*(V[u][2]+V[v][2])};
            if (!geom_ok(u, v, np)) continue;                       // would fold / degenerate

            // collapse v -> u (move u to midpoint); disjoint 1-rings => race-free
            V[u][0] = np[0]; V[u][1] = np[1]; V[u][2] = np[2];
            vt[u].retain(talive.data());
            for (int t : vt[v]) {
                if (!talive[t]) continue;
                auto& f = F[t];
                for (int k=0;k<3;k++) if (f[k]==v) f[k]=u;
                if (f[0]==f[1] || f[1]==f[2] || f[0]==f[2]) talive[t] = 0;
                else vt[u].insert(t);
            }
            for (int w : adj[v]) {
                adj[w].erase(v);
                if (w != u) { adj[w].insert(u); adj[u].insert(w); }
            }
            adj[u].erase(v);
            adj[v].clear(); vt[v].clear(); valive[v] = 0;
            ++round_collapses;
        }
        total_collapses += round_collapses;

        // ---- compact: keep candidates that are neither consumed nor dead ----
        for (auto& l : loc) l.clear();
        #pragma omp parallel
        {
        #ifdef _OPENMP
            int tid = omp_get_thread_num();
        #else
            int tid = 0;
        #endif
            auto& L = loc[tid];
            #pragma omp for schedule(static)
            for (int ci = 0; ci < NC; ++ci) {
                if (done[ci]) continue;
                int u = cands[ci].u, v = cands[ci].v;
                if (valive[u] && valive[v]) L.push_back(cands[ci]);
            }
        }
        cands.clear();
        for (auto& l : loc) cands.insert(cands.end(), l.begin(), l.end());
    }

    // ---- compact ----
    std::vector<int> remap(nv, -1);
    std::vector<std::array<float,3>> newV; newV.reserve(nv);
    for (int v = 0; v < nv; ++v)
        if (valive[v]) { remap[v] = (int)newV.size(); newV.push_back(V[v]); }
    std::vector<std::array<int,3>> newF; newF.reserve(mesh.F.size());
    for (long t = 0; t < nf; ++t) {
        if (!talive[t]) continue;
        auto& f = F[t];
        int a=remap[f[0]], b=remap[f[1]], c2=remap[f[2]];
        if (a<0||b<0||c2<0||a==b||b==c2||a==c2) continue;
        newF.push_back({a,b,c2});
    }
    mesh.V.swap(newV);
    mesh.F.swap(newF);
    if (verbose)
        mlog::out(mlog::Stage::Merge, mmsg::merge_result,
                  {total_collapses, (long long)mesh.V.size(),
                   (long long)mesh.F.size()});
}

// ---------------------------------------------------------------------------
// Global orientation: make the winding consistent across the whole (closed,
// manifold => orientable) surface. Flood-fill orientation across edge
// adjacency so neighbours traverse their shared edge in opposite directions,
// then flip each connected component as a whole if the consistent orientation
// disagrees with the per-triangle outward guess from emission (majority vote),
// so normals end up pointing outward.
// ---------------------------------------------------------------------------
static void orient_mesh(Mesh& mesh) {
    const int nf = (int)mesh.F.size();
    if (nf == 0) return;
    auto& F = mesh.F;
    const int64_t P = (int64_t)mesh.V.size();
    auto ekey = [P](int a, int b) -> int64_t {
        int64_t lo = a<b?a:b, hi = a<b?b:a; return lo*P + hi;
    };

    // undirected edge -> the (<=2) triangles on it
    std::unordered_map<int64_t, std::array<int,2>> etri;
    etri.reserve((size_t)nf * 3);
    for (int t = 0; t < nf; ++t) {
        const auto& f = F[t];
        for (int k = 0; k < 3; ++k) {
            auto& slot = etri.emplace(ekey(f[k], f[(k+1)%3]),
                                      std::array<int,2>{-1,-1}).first->second;
            if (slot[0] < 0) slot[0] = t; else if (slot[1] < 0) slot[1] = t;
        }
    }

    // Flood-fill consistent winding. Neighbours are looked up by undirected
    // edge each step (not a precomputed per-corner index) so they stay valid
    // even after a triangle's winding is flipped.
    //
    // BFS (a queue, via an index into `comp`), not DFS. For a clean orientable
    // surface either gives a fully consistent result, but the extracted mesh can
    // still contain small non-orientable defects (e.g. thin twisted sliver
    // strips). Around such a defect SOME edge stays inconsistent no matter what,
    // and the count of those leftover edges is the number of spanning-tree "back
    // edges" closing an odd cycle -- far smaller for BFS's shallow tree than for
    // DFS's long stringy one. BFS thus confines the residual front/back mixing
    // to the immediate neighbourhood of each defect instead of smearing it along
    // a deep traversal path across the surface.
    std::vector<char> vis(nf, 0), flipped(nf, 0);
    std::vector<int> comp;
    for (int seed = 0; seed < nf; ++seed) {
        if (vis[seed]) continue;
        vis[seed] = 1; comp.clear(); comp.push_back(seed);
        for (size_t h = 0; h < comp.size(); ++h) {
            int t = comp[h];
            const auto& f = F[t];
            for (int k = 0; k < 3; ++k) {
                int a = f[k], b = f[(k+1)%3];
                auto it = etri.find(ekey(a, b));
                int nb = (it->second[0] == t) ? it->second[1] : it->second[0];
                if (nb < 0 || vis[nb]) continue;
                auto& g = F[nb];
                bool same = false;  // neighbour traverses (a->b) the same way?
                for (int m = 0; m < 3; ++m)
                    if (g[m] == a && g[(m+1)%3] == b) { same = true; break; }
                if (same) { std::swap(g[1], g[2]); flipped[nb] = 1; }
                vis[nb] = 1; comp.push_back(nb);
            }
        }
        // align the component's global sign with the emission (outward) guess
        long fl = 0; for (int t : comp) fl += flipped[t];
        if (fl * 2 > (long)comp.size())
            for (int t : comp) std::swap(F[t][1], F[t][2]);
    }
}

// ---------------------------------------------------------------------------
// Duplicate-face removal.
//
// Marching tetrahedra over a dense point cloud occasionally emits two coincident
// triangles on the same vertex triple ("doublet" balloons of zero volume). They
// are degenerate junk and, being isolated 2-triangle shells, survive the merge.
// Drop every face whose (sorted) vertex triple was already seen, keeping one;
// the leftover singleton then falls to the floater pass.
// ---------------------------------------------------------------------------
static void dedup_faces(Mesh& mesh, bool verbose) {
    const long nf = (long)mesh.F.size();
    if (nf == 0) return;
    std::vector<std::array<int,4>> key(nf);
    for (long t = 0; t < nf; ++t) {
        int a = mesh.F[t][0], b = mesh.F[t][1], c = mesh.F[t][2];
        if (a > b) std::swap(a, b);
        if (b > c) std::swap(b, c);
        if (a > b) std::swap(a, b);
        key[t] = {a, b, c, (int)t};
    }
    std::sort(key.begin(), key.end());
    std::vector<char> drop(nf, 0);
    long ndup = 0;
    for (long i = 1; i < nf; ++i)
        if (key[i][0]==key[i-1][0] && key[i][1]==key[i-1][1] && key[i][2]==key[i-1][2]) {
            drop[key[i][3]] = 1; ++ndup;
        }
    if (ndup == 0) return;
    std::vector<std::array<int,3>> newF; newF.reserve(nf - ndup);
    for (long t = 0; t < nf; ++t) if (!drop[t]) newF.push_back(mesh.F[t]);
    mesh.F.swap(newF);
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::dedup_faces,
                  {ndup, (long long)mesh.F.size()});
}

// ---------------------------------------------------------------------------
// Floater removal: drop small disconnected components.
//
// Components are the connected components of the face graph (faces sharing a
// vertex are connected, via a union-find over vertices). A component is a
// "floater" when its face count is below min_faces; its faces are deleted and
// the mesh compacted (orphan vertices, i.e. vertices no longer used by any face,
// are dropped too). min_faces <= 1 disables the pass.
// ---------------------------------------------------------------------------
static void remove_floaters(Mesh& mesh, int min_faces, bool verbose) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (min_faces <= 1 || nv == 0 || nf == 0) return;

    std::vector<int> parent(nv);
    for (int i = 0; i < nv; ++i) parent[i] = i;
    std::function<int(int)> find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto unite = [&](int a, int b) { a=find(a); b=find(b); if (a!=b) parent[a]=b; };
    for (long t = 0; t < nf; ++t) {
        const auto& f = mesh.F[t];
        unite(f[0], f[1]); unite(f[1], f[2]);
    }

    std::unordered_map<int, long> comp_faces;        // root -> face count
    for (long t = 0; t < nf; ++t) comp_faces[find(mesh.F[t][0])] += 1;
    const long thresh = min_faces;

    std::vector<int> remap(nv, -1);
    std::vector<std::array<float,3>> newV; newV.reserve(nv);
    std::vector<std::array<int,3>> newF; newF.reserve(mesh.F.size());
    long dropped_f = 0;
    for (long t = 0; t < nf; ++t) {
        const auto& f = mesh.F[t];
        if (comp_faces[find(f[0])] < thresh) { ++dropped_f; continue; }
        int idx[3];
        for (int k = 0; k < 3; ++k) {
            int v = f[k];
            if (remap[v] < 0) { remap[v] = (int)newV.size(); newV.push_back(mesh.V[v]); }
            idx[k] = remap[v];
        }
        newF.push_back({idx[0], idx[1], idx[2]});
    }
    size_t removed_v = (size_t)nv - newV.size();
    mesh.V.swap(newV);
    mesh.F.swap(newF);
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::floaters_removed,
                  {dropped_f, (long long)removed_v, (long long)mesh.V.size(),
                   (long long)mesh.F.size()});
}

// ---------------------------------------------------------------------------
// Hole filling: triangulate small boundary loops.
//
// A boundary edge is used by a single triangle. Boundary edges are chained into
// loops (after split_nonmanifold_vertices every boundary vertex has exactly two
// boundary edges, so the boundary is a set of disjoint cyclic loops). A loop is
// filled when it is small relative to its connected component -- its bounding-box
// diagonal is less than `ratio` times the component's bbox diagonal; larger loops
// (open scene / sky / large unseen regions) stay open.
//
// Each fillable loop is triangulated with a minimum-weight polygon triangulation
// (Liepa-style dynamic program over the cyclic boundary). This uses ONLY the
// loop's own vertices -- no centroid -- so it adds no high-valence hub, and the
// weight is each triangle's "thinness" (sum of squared edge lengths / area), so
// the DP minimises sliver/thin triangles. Because every loop edge is covered by
// exactly one fill triangle, fills are watertight (no spurious boundary edges).
// Degenerate triangles get an infinite weight, so when a loop visits one position
// via two distinct split-copy vertices ("overlapping" verts) the DP never joins
// them with a (zero-area) triangle -- i.e. no edge between overlapping vertices --
// it bridges the two sides through non-degenerate triangles instead. Winding is
// left to the subsequent orient pass. Very large loops fall back to a (watertight)
// centroid fan to keep the cubic DP bounded.
// ---------------------------------------------------------------------------
static void fill_holes(Mesh& mesh, float ratio, int max_edges, bool verbose) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if ((ratio <= 0.0f && max_edges <= 0) || nv == 0 || nf == 0) return;
    const int64_t P = (int64_t)nv;
    auto ekey = [P](int a, int b) -> int64_t {
        int64_t lo = a<b?a:b, hi = a<b?b:a; return lo*P + hi;
    };

    // ---- connected components (union-find over verts via faces) + per-comp bbox ----
    std::vector<int> parent(nv);
    for (int i = 0; i < nv; ++i) parent[i] = i;
    std::function<int(int)> find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    for (long t = 0; t < nf; ++t) {
        const auto& f = mesh.F[t];
        int a=find(f[0]), b=find(f[1]), c=find(f[2]);
        if (a!=b) parent[a]=b; b=find(b); if (b!=c) parent[b]=c;
    }
    std::unordered_map<int,std::array<float,6>> caabb;  // root -> (min xyz, max xyz)
    for (int v = 0; v < nv; ++v) {
        int r = find(v); const auto& p = mesh.V[v];
        auto it = caabb.find(r);
        if (it == caabb.end()) caabb[r] = {p[0],p[1],p[2],p[0],p[1],p[2]};
        else { auto& b = it->second;
            for (int a=0;a<3;a++){ b[a]=std::min(b[a],p[a]); b[3+a]=std::max(b[3+a],p[a]); } }
    }
    auto comp_diag = [&](int r) {
        const auto& b = caabb[r];
        float dx=b[3]-b[0], dy=b[4]-b[1], dz=b[5]-b[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    // ---- boundary edges + adjacency ----
    std::unordered_map<int64_t,int> ecount;
    ecount.reserve((size_t)nf * 3);
    for (long t = 0; t < nf; ++t) {
        const auto& f = mesh.F[t];
        for (int k = 0; k < 3; ++k) ecount[ekey(f[k], f[(k+1)%3])] += 1;
    }
    struct BE { int w; int64_t key; };
    std::unordered_map<int, std::vector<BE>> badj;
    std::unordered_map<int64_t,char> used;
    int n_bnd = 0;
    for (long t = 0; t < nf; ++t) {
        const auto& f = mesh.F[t];
        for (int k = 0; k < 3; ++k) {
            int a = f[k], b = f[(k+1)%3];
            int64_t key = ekey(a, b);
            if (ecount[key] != 1) continue;
            if (used.emplace(key, 0).second) {
                badj[a].push_back({b, key});
                badj[b].push_back({a, key});
                ++n_bnd;
            }
        }
    }
    if (n_bnd == 0) return;

    long n_filled = 0, n_tris = 0;

    // Emit a fill triangle. We do NOT drop (near-)degenerate triangles: skipping
    // one would leave its boundary edge uncovered, which (next to a coincident
    // split-copy vertex) shows up as a spurious boundary in tools like MeshLab.
    // The DP never picks degenerate triangles (they have infinite weight, so it
    // splits instead); only the rare fan fallback emits the odd zero-area face,
    // which is invisible but keeps the fill watertight.
    auto push_tri = [&](int a, int b, int c) {
        mesh.F.push_back({a, b, c});
        n_tris += 1;
    };
    // triangle "thinness": (sum of squared edge lengths)/area; minimal (~3.46)
    // for an equilateral triangle, large for slivers, +inf for (near-)degenerate.
    auto tri_weight = [&](int a, int b, int c) -> float {
        const auto& A = mesh.V[a]; const auto& B = mesh.V[b]; const auto& C = mesh.V[c];
        float ux=B[0]-A[0], uy=B[1]-A[1], uz=B[2]-A[2];
        float vx=C[0]-A[0], vy=C[1]-A[1], vz=C[2]-A[2];
        float wx=C[0]-B[0], wy=C[1]-B[1], wz=C[2]-B[2];
        float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        float area2 = std::sqrt(nx*nx+ny*ny+nz*nz);   // = 2*area
        if (area2 < 1e-12f) return 1e30f;
        float s = (ux*ux+uy*uy+uz*uz) + (vx*vx+vy*vy+vz*vz) + (wx*wx+wy*wy+wz*wz);
        return s / area2;
    };

    // Minimum-weight triangulation of a cyclic sub-polygon L (Liepa DP). A
    // diagonal that already exists as a mesh edge is forbidden (would put a 3rd
    // triangle on it -> non-manifold). Returns false if no collision-free
    // triangulation exists (caller then uses the collision-proof fan).
    const int kDpMax = 250;   // cubic DP cap; larger loops are split first
    std::vector<float> W;     // reused across calls
    std::vector<int> O, st;
    auto dp_fill = [&](const std::vector<int>& L) -> bool {
        const int n = (int)L.size();
        auto blocked = [&](int p, int q) -> bool {
            int lo = p<q?p:q, hi = p<q?q:p;
            if (hi == lo+1 || (lo == 0 && hi == n-1)) return false;  // boundary edge
            return ecount.find(ekey(L[lo], L[hi])) != ecount.end();
        };
        W.assign((size_t)n*n, 0.0f);
        O.assign((size_t)n*n, 0);
        for (int gap = 2; gap < n; ++gap)
            for (int i = 0; i + gap < n; ++i) {
                int j = i + gap; float best = 1e30f; int bo = i+1;
                for (int m = i+1; m < j; ++m) {
                    float c = W[(size_t)i*n+m] + W[(size_t)m*n+j]
                            + tri_weight(L[i], L[m], L[j]);
                    if (blocked(i,m) || blocked(m,j) || blocked(i,j)) c = 1e30f;
                    if (c < best) { best = c; bo = m; }
                }
                W[(size_t)i*n+j] = best; O[(size_t)i*n+j] = bo;
            }
        if (W[(size_t)0*n+(n-1)] >= 1e29f) return false;
        st.clear(); st.push_back(0); st.push_back(n-1);
        while (!st.empty()) {
            int j = st.back(); st.pop_back();
            int i = st.back(); st.pop_back();
            if (j <= i+1) continue;
            int m = O[(size_t)i*n+j];
            push_tri(L[i], L[m], L[j]);
            st.push_back(i); st.push_back(m);
            st.push_back(m); st.push_back(j);
        }
        return true;
    };
    auto fan_fill = [&](const std::vector<int>& L) {     // collision-proof last resort
        const int n = (int)L.size(); if (n < 3) return;
        const long before = (long)mesh.F.size();
        std::array<float,3> c = {0,0,0};
        for (int v : L) for (int a=0;a<3;a++) c[a] += mesh.V[v][a];
        for (int a=0;a<3;a++) c[a] /= (float)n;
        int ci = (int)mesh.V.size(); mesh.V.push_back(c);
        for (int i=0;i<n;++i) push_tri(ci, L[i], L[(i+1)%n]);
        if ((long)mesh.F.size() == before) mesh.V.pop_back();
    };

    // Fill one boundary loop if it is small relative to its component. A large
    // loop is recursively split by a non-colliding chord near its midpoint into
    // two sub-loops (sharing only that chord) until each is small enough for the
    // DP -- this avoids any high-valence hub vertex while keeping good triangle
    // shape. The chord is recorded in `ecount` so no later diagonal reuses it.
    std::vector<std::vector<int>> work;
    auto fill_loop = [&](const std::vector<int>& L0) {
        const int n0 = (int)L0.size();
        if (n0 < 3) return;
        float bb[6] = {1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
        for (int v : L0) { const auto& p = mesh.V[v];
            for (int a=0;a<3;a++){ bb[a]=std::min(bb[a],p[a]); bb[3+a]=std::max(bb[3+a],p[a]); } }
        float dx=bb[3]-bb[0], dy=bb[4]-bb[1], dz=bb[5]-bb[2];
        // fill if EITHER criterion is met: small relative to its component, OR
        // few enough edges (so tiny holes always fill, even in a small component).
        bool ok_ratio = ratio > 0.0f &&
                        std::sqrt(dx*dx+dy*dy+dz*dz) < ratio * comp_diag(find(L0[0]));
        bool ok_edges = max_edges > 0 && n0 <= max_edges;
        if (!ok_ratio && !ok_edges) return;
        const long before = (long)mesh.F.size();

        work.clear(); work.push_back(L0);
        while (!work.empty()) {
            std::vector<int> cur = std::move(work.back()); work.pop_back();
            const int n = (int)cur.size();
            if (n < 3) continue;
            if (n == 3) { push_tri(cur[0], cur[1], cur[2]); continue; }
            if (n <= kDpMax && dp_fill(cur)) continue;   // DP triangulated it cleanly
            // Otherwise split (loop too big for the DP, or the DP found no
            // collision-free triangulation -- smaller sub-loops usually resolve it,
            // so splitting beats falling straight to the high-valence fan).
            // Split at the SHORTEST non-colliding chord that divides the loop into
            // two balanced arcs (each >= ~1/5 of the loop). Shortest-balanced finds
            // a narrow neck instead of an arbitrary long diameter, so the split
            // edge is short and the resulting triangles stay well-shaped; the
            // balance bound also keeps the recursion shallow (O(n^2) overall).
            const int lo = std::max(2, n/5), hi = std::min(n-2, (4*n)/5);
            int bi = -1, bj = -1; float blen = 1e30f;
            for (int i = 0; i < n; ++i) {
                const auto& Pi = mesh.V[cur[i]];
                for (int g = lo; g <= hi; ++g) {
                    int j = i + g; if (j >= n) break;
                    if (ecount.find(ekey(cur[i], cur[j])) != ecount.end()) continue;
                    const auto& Pj = mesh.V[cur[j]];
                    float ex=Pi[0]-Pj[0], ey=Pi[1]-Pj[1], ez=Pi[2]-Pj[2];
                    float l = ex*ex + ey*ey + ez*ez;
                    if (l < blen) { blen = l; bi = i; bj = j; }
                }
            }
            if (bi < 0) { fan_fill(cur); continue; }     // no clean chord -> last resort
            ecount[ekey(cur[bi], cur[bj])] += 1;          // reserve the chord
            std::vector<int> A(cur.begin()+bi, cur.begin()+bj+1);
            std::vector<int> B(cur.begin()+bj, cur.end());
            B.insert(B.end(), cur.begin(), cur.begin()+bi+1);
            work.push_back(std::move(A));
            work.push_back(std::move(B));
        }
        if ((long)mesh.F.size() > before) ++n_filled;
    };

    // Walk each boundary loop and triangulate it.
    std::vector<int> loop;
    for (auto& kv : badj) {
        for (BE& e0 : kv.second) {
            if (used[e0.key]) continue;
            int start = kv.first;
            loop.clear(); loop.push_back(start);
            used[e0.key] = 1;
            int prev = start, cur = e0.w;
            bool closed = false, ok = true;
            while ((int)loop.size() <= n_bnd) {
                if (cur == start) { closed = true; break; }
                loop.push_back(cur);
                int nxt = -1; int64_t nkey = -1;
                auto it = badj.find(cur);
                if (it != badj.end())
                    for (BE& e : it->second)
                        if (!used[e.key] && (nxt < 0 || e.w != prev)) { nxt = e.w; nkey = e.key; }
                if (nxt < 0) { ok = false; break; }
                used[nkey] = 1; prev = cur; cur = nxt;
            }
            if (!ok || !closed || (int)loop.size() < 3) continue;
            fill_loop(loop);
        }
    }
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::holes_filled,
                  {n_filled, (double)ratio, max_edges, n_tris,
                   (long long)mesh.V.size(), (long long)mesh.F.size()});
}

// ---------------------------------------------------------------------------
// Split non-manifold vertices.
//
// A vertex is non-manifold ("bowtie") when its incident triangles form more
// than one edge-connected fan -- two surface sheets touching at a single point.
// Edges may all be manifold (<= 2 triangles) yet such a vertex still breaks the
// surface: the link is several cycles, not one, so orientation cannot be
// propagated consistently across it (the flood-fill in orient_mesh leaves mixed
// front/back faces). We duplicate the vertex into one copy per fan and reassign
// each fan's triangles to its own copy. Geometry is unchanged (copies share the
// position); the surface becomes a clean manifold-with-boundary, so the
// subsequent orient pass yields a single consistent winding per component.
// ---------------------------------------------------------------------------
static void split_nonmanifold_vertices(Mesh& mesh, bool verbose) {
    const int nv0 = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (nv0 == 0 || nf == 0) return;

    std::vector<std::vector<int>> vt(nv0);
    for (long t = 0; t < nf; ++t)
        for (int k = 0; k < 3; ++k) vt[mesh.F[t][k]].push_back((int)t);

    std::unordered_map<int,int> parent;   // tri -> parent (local UF over incident tris)
    std::unordered_map<int,int> w2rep;    // other endpoint w -> a tri carrying edge (v,w)
    std::unordered_map<int,int> root2vid; // fan root -> assigned vertex id
    auto find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    long n_split = 0, n_new = 0;
    for (int v = 0; v < nv0; ++v) {
        auto& tris = vt[v];
        if (tris.size() < 2) continue;
        // union incident triangles that share an edge (v,w)
        parent.clear(); w2rep.clear();
        for (int t : tris) parent[t] = t;
        for (int t : tris) {
            const auto& f = mesh.F[t];
            for (int k = 0; k < 3; ++k) {
                int w = f[k];
                if (w == v) continue;
                auto it = w2rep.find(w);
                if (it == w2rep.end()) w2rep[w] = t;
                else { int a = find(it->second), b = find(t); if (a != b) parent[a] = b; }
            }
        }
        // first fan keeps v; each other fan gets a fresh copy
        root2vid.clear();
        int first_root = find(tris[0]);
        for (int t : tris) {
            int r = find(t);
            if (r == first_root) continue;
            int vid;
            auto it = root2vid.find(r);
            if (it == root2vid.end()) {
                vid = (int)mesh.V.size();
                mesh.V.push_back(mesh.V[v]);
                root2vid[r] = vid;
                ++n_new;
            } else vid = it->second;
            auto& f = mesh.F[t];
            for (int k = 0; k < 3; ++k) if (f[k] == v) f[k] = vid;
        }
        if (!root2vid.empty()) ++n_split;
    }
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::nonmanifold_split,
                  {n_split, n_new, (long long)mesh.V.size()});
}

// ---------------------------------------------------------------------------
// Narrow / degenerate face repair.
//
// Marching tets + merge + hole-fill can leave thin "sliver" triangles and the
// odd zero-area face (e.g. the watertight fill placed a triangle across a
// coincident split-copy "pinch"). Such faces have a smallest angle near 0 and
// are harmful downstream (degenerate normals, bad simplification / physics). We
// remove them with two fidelity-preserving local operations (Botsch & Kobbelt,
// "A robust procedure to eliminate degenerate faces"):
//
//   * NEEDLE  (apex projects OUTSIDE the longest edge => one very short edge):
//       collapse the shortest edge to its midpoint. Its two endpoints are nearly
//       coincident, so the surface barely moves. Applied only when the
//       edge-collapse link condition holds, so the mesh stays manifold -- in
//       particular it REFUSES to re-merge the two copies of a split pinch vertex
//       when that would re-create a bowtie (when the surrounding fill already
//       bridged the two fans, the link condition passes and the merge is a clean
//       win that removes the zero-area face).
//
//   * CAP     (apex projects INSIDE the longest edge => one angle near 180):
//       flip the longest edge with its neighbour. The apex is ~on that edge, so
//       the flip barely moves the surface but replaces the cap with two
//       well-shaped triangles.
//
// Both operations are gated so the pass can never make things worse: a flip must
// strictly improve the worst angle and keep both new triangles non-degenerate
// and un-folded; a collapse must not rotate any reshaped face normal past 60
// degrees or create a degenerate face. Winding is left to the later orient pass,
// so the fold tests use a locally-consistent winding rather than the stored one.
//
// It iterates in "locked" rounds: within a round, once an operation touches a
// vertex, any later candidate sharing that vertex is deferred to the next round
// (so the stale per-round adjacency is never read after a local edit). It stops
// when a round fixes nothing or a small round budget is hit. angle_deg <= 0
// disables the pass. The collapse/flip both preserve manifoldness, so no
// re-split is needed before orient.
// ---------------------------------------------------------------------------
static void fix_degenerate_faces(Mesh& mesh, float angle_deg, bool verbose) {
    const int nv = (int)mesh.V.size();
    if (angle_deg <= 0.0f || nv == 0 || mesh.F.empty()) return;
    auto& V = mesh.V;
    auto& F = mesh.F;
    // A triangle is degenerate when its smallest angle < angle_deg, i.e. the
    // cosine of that angle exceeds cos(angle_deg).
    const float cos_small = std::cos(angle_deg * 0.017453292519943295f);
    const float cos_fold = 0.5f;  // reshaped face normal may rotate <= 60 deg

    auto sub = [](const std::array<float,3>& a, const std::array<float,3>& b) {
        return std::array<float,3>{a[0]-b[0], a[1]-b[1], a[2]-b[2]};
    };
    auto cross = [](const std::array<float,3>& a, const std::array<float,3>& b) {
        return std::array<float,3>{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2],
                                   a[0]*b[1]-a[1]*b[0]};
    };
    auto dot = [](const std::array<float,3>& a, const std::array<float,3>& b) {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    };
    auto nrm = [&](const std::array<float,3>& a) { return std::sqrt(dot(a,a)); };
    auto d2  = [&](int a, int b) { auto e = sub(V[a], V[b]); return dot(e, e); };
    // max cosine over the three angles = cosine of the SMALLEST angle (worst).
    auto worst_cos = [&](int a, int b, int c) -> float {
        float la = d2(b,c), lb = d2(c,a), lc = d2(a,b);
        auto ang = [](float opp, float s1, float s2) {
            float dn = 2.0f * std::sqrt(s1 * s2);
            return dn > 1e-30f ? (s1 + s2 - opp) / dn : 1.0f;
        };
        return std::max(ang(la,lb,lc), std::max(ang(lb,lc,la), ang(lc,la,lb)));
    };

    std::vector<char> alive(F.size(), 1);
    long n_collapse = 0, n_flip = 0;

    const int kMaxRounds = 8;
    for (int round = 0; round < kMaxRounds; ++round) {
        const long nf = (long)F.size();
        // vertex -> incident alive triangles (rebuilt each round)
        std::vector<std::vector<int>> vt(nv);
        for (long t = 0; t < nf; ++t) {
            if (!alive[t]) continue;
            const auto& f = F[t];
            vt[f[0]].push_back((int)t);
            vt[f[1]].push_back((int)t);
            vt[f[2]].push_back((int)t);
        }
        std::vector<char> locked(nv, 0);
        long applied = 0;

        auto apex_of = [&](int t, int a, int b) {
            const auto& f = F[t];
            for (int k = 0; k < 3; ++k) if (f[k] != a && f[k] != b) return f[k];
            return -1;
        };
        // the (<=2) alive faces on edge (a,b)
        auto faces_on_edge = [&](int a, int b, int& t0, int& t1) -> int {
            t0 = t1 = -1; int cnt = 0;
            for (int t : vt[a]) {
                if (!alive[t]) continue;
                const auto& f = F[t];
                if (f[0]==b || f[1]==b || f[2]==b) {
                    if (cnt == 0) t0 = t; else if (cnt == 1) t1 = t;
                    ++cnt;
                }
            }
            return cnt;
        };
        auto edge_exists = [&](int a, int b) -> bool {
            for (int t : vt[a]) {
                if (!alive[t]) continue;
                const auto& f = F[t];
                if (f[0]==b || f[1]==b || f[2]==b) return true;
            }
            return false;
        };
        // a vertex is on the boundary if any incident edge has a single face
        auto is_bnd = [&](int x) -> bool {
            std::unordered_map<int,int> ec;
            for (int t : vt[x]) {
                if (!alive[t]) continue;
                const auto& g = F[t];
                for (int k=0;k<3;k++) if (g[k]!=x) ec[g[k]] += 1;
            }
            for (auto& kv : ec) if (kv.second == 1) return true;
            return false;
        };

        for (long t = 0; t < nf; ++t) {
            if (!alive[t]) continue;
            const auto& f = F[t];
            int i0 = f[0], i1 = f[1], i2 = f[2];
            if (i0==i1 || i1==i2 || i0==i2) { alive[t] = 0; continue; }
            if (locked[i0] || locked[i1] || locked[i2]) continue;
            if (worst_cos(i0, i1, i2) <= cos_small) continue;  // not degenerate

            float l0 = d2(i1,i2), l1 = d2(i2,i0), l2 = d2(i0,i1);  // opp i0,i1,i2
            // shortest edge (sa,sb); longest edge (p0,p1) with apex pa
            int sa, sb; float ls;
            if (l0<=l1 && l0<=l2)      { sa=i1; sb=i2; ls=l0; }
            else if (l1<=l2)          { sa=i2; sb=i0; ls=l1; }
            else                      { sa=i0; sb=i1; ls=l2; }
            int p0, p1, pa; float ll;
            if (l0>=l1 && l0>=l2)      { p0=i1; p1=i2; pa=i0; ll=l0; }
            else if (l1>=l2)          { p0=i2; p1=i0; pa=i1; ll=l1; }
            else                      { p0=i0; p1=i1; pa=i2; ll=l2; }
            (void)ls;

            // classify by where the apex of the longest edge projects onto it
            auto ev = sub(V[p1], V[p0]);
            float tproj = ll > 1e-30f ? dot(sub(V[pa], V[p0]), ev) / ll : -1.0f;
            bool is_cap = (tproj > 0.05f && tproj < 0.95f);

            if (is_cap) {
                // ---- CAP: flip the longest edge (p0,p1) ----
                int t0, t1;
                if (faces_on_edge(p0, p1, t0, t1) != 2) continue;  // boundary / nonmanifold
                int o0 = apex_of(t0, p0, p1), o1 = apex_of(t1, p0, p1);
                int d = (o0 == pa) ? o1 : o0;       // far apex (other triangle)
                if (d < 0 || d == pa) continue;
                if (locked[p0]||locked[p1]||locked[pa]||locked[d]) continue;
                if (edge_exists(pa, d)) continue;   // flip would create a 3rd face on (pa,d)
                // locally-consistent quad winding for the fold / improvement test
                auto n0 = cross(sub(V[p1],V[p0]), sub(V[pa],V[p0]));
                auto n1 = cross(sub(V[p0],V[p1]), sub(V[d ],V[p1]));
                std::array<float,3> nav{n0[0]+n1[0], n0[1]+n1[1], n0[2]+n1[2]};
                float lav = nrm(nav); if (lav < 1e-20f) continue;
                nav[0]/=lav; nav[1]/=lav; nav[2]/=lav;
                auto m0 = cross(sub(V[p1],V[pa]), sub(V[d ],V[pa]));  // (pa,p1,d)
                auto m1 = cross(sub(V[d ],V[pa]), sub(V[p0],V[pa]));  // (pa,d,p0)
                if (nrm(m0) < 1e-20f || nrm(m1) < 1e-20f) continue;  // new tri degenerate
                if (dot(m0,nav) <= 0.0f || dot(m1,nav) <= 0.0f) continue;  // fold
                float oldw = std::max(worst_cos(p0,p1,pa), worst_cos(p1,p0,d));
                float neww = std::max(worst_cos(pa,p1,d), worst_cos(pa,d,p0));
                if (neww >= oldw) continue;          // not an improvement
                F[t0] = {pa, p1, d};
                F[t1] = {pa, d, p0};
                locked[p0]=locked[p1]=locked[pa]=locked[d]=1;
                ++n_flip; ++applied;
            } else {
                // ---- NEEDLE: collapse the shortest edge (u,v) -> midpoint ----
                int u = sa, v = sb;
                if (locked[u] || locked[v]) continue;
                // edge-collapse link condition (local). opposite verts of the
                // (<=2) faces on (u,v); >2 => already non-manifold edge.
                int opp[2]; int nopp = 0; bool nonmanif = false;
                for (int t2 : vt[u]) {
                    if (!alive[t2]) continue;
                    const auto& g = F[t2];
                    bool hu=false, hv=false; int other=-1;
                    for (int k=0;k<3;k++){ if(g[k]==u)hu=true; else if(g[k]==v)hv=true; else other=g[k]; }
                    if (hu && hv) {
                        if (nopp < 2) opp[nopp] = other;
                        if (++nopp > 2) { nonmanif = true; break; }
                    }
                }
                if (nonmanif || nopp == 0) continue;
                // boundary rule: collapsing an INTERIOR edge (2 faces) whose two
                // endpoints are both on the boundary pinches the surface into a
                // non-manifold vertex even though the link condition holds. Skip.
                if (nopp == 2 && is_bnd(u) && is_bnd(v)) continue;
                std::unordered_set<int> nbu;
                for (int t2 : vt[u]) {
                    if (!alive[t2]) continue; const auto& g = F[t2];
                    for (int k=0;k<3;k++) if (g[k]!=u) nbu.insert(g[k]);
                }
                int common = 0; bool bad = false;
                std::unordered_set<int> seen;
                for (int t2 : vt[v]) {
                    if (!alive[t2]) continue; const auto& g = F[t2];
                    for (int k=0;k<3;k++) {
                        int w = g[k];
                        if (w==u || w==v) continue;
                        if (!seen.insert(w).second) continue;
                        if (nbu.count(w)) {
                            ++common;
                            if (w != opp[0] && !(nopp>1 && w == opp[1])) bad = true;
                        }
                    }
                }
                if (bad || common != nopp) continue;   // link condition fails
                // geometric guard: midpoint must not fold / degenerate any face
                std::array<float,3> np{0.5f*(V[u][0]+V[v][0]),
                                       0.5f*(V[u][1]+V[v][1]),
                                       0.5f*(V[u][2]+V[v][2])};
                bool ok = true;
                for (int pass = 0; pass < 2 && ok; ++pass) {
                    int center = pass == 0 ? u : v;
                    for (int t2 : vt[center]) {
                        if (!alive[t2]) continue;
                        const auto& g = F[t2];
                        bool hu=false, hv=false;
                        for (int k=0;k<3;k++){ if(g[k]==u)hu=true; else if(g[k]==v)hv=true; }
                        if (hu && hv) continue;        // face on collapsed edge -> dies
                        std::array<float,3> q0 = g[0]==center?np:V[g[0]];
                        std::array<float,3> q1 = g[1]==center?np:V[g[1]];
                        std::array<float,3> q2 = g[2]==center?np:V[g[2]];
                        auto no = cross(sub(V[g[1]],V[g[0]]), sub(V[g[2]],V[g[0]]));
                        auto nn = cross(sub(q1,q0), sub(q2,q0));
                        float lo = nrm(no), ln = nrm(nn);
                        if (ln < 1e-20f) { ok = false; break; }
                        if (lo > 1e-20f && dot(no,nn)/(lo*ln) < cos_fold) { ok = false; break; }
                    }
                }
                if (!ok) continue;
                // apply: move u to midpoint, retarget v's faces, kill faces on (u,v)
                V[u] = np;
                for (int t2 : vt[u]) { if(!alive[t2])continue; const auto&g=F[t2]; locked[g[0]]=locked[g[1]]=locked[g[2]]=1; }
                for (int t2 : vt[v]) { if(!alive[t2])continue; const auto&g=F[t2]; locked[g[0]]=locked[g[1]]=locked[g[2]]=1; }
                for (int t2 : vt[v]) {
                    if (!alive[t2]) continue;
                    auto& g = F[t2];
                    for (int k=0;k<3;k++) if (g[k]==v) g[k]=u;
                    if (g[0]==g[1] || g[1]==g[2] || g[0]==g[2]) alive[t2] = 0;
                }
                ++n_collapse; ++applied;
            }
        }
        if (applied == 0) break;
    }

    if (n_collapse == 0 && n_flip == 0) {
        if (verbose) mlog::out(mlog::Stage::Cleanup, mmsg::degenerate_none);
        return;
    }

    // compact: drop dead / index-degenerate faces and orphan vertices
    const bool has_color = mesh.C.size() == mesh.V.size();
    std::vector<std::array<int,3>> nF; nF.reserve(F.size());
    for (size_t t = 0; t < F.size(); ++t) {
        if (!alive[t]) continue;
        const auto& f = F[t];
        if (f[0]!=f[1] && f[1]!=f[2] && f[0]!=f[2]) nF.push_back(f);
    }
    std::vector<int> remap(nv, -1);
    std::vector<std::array<float,3>> nV; nV.reserve(nv);
    std::vector<std::array<unsigned char,3>> nC; if (has_color) nC.reserve(nv);
    for (auto& f : nF)
        for (int k = 0; k < 3; ++k) {
            int v = f[k];
            if (remap[v] < 0) {
                remap[v] = (int)nV.size();
                nV.push_back(V[v]);
                if (has_color) nC.push_back(mesh.C[v]);
            }
            f[k] = remap[v];
        }
    mesh.V.swap(nV);
    mesh.F.swap(nF);
    if (has_color) mesh.C.swap(nC);
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::degenerate_fixed,
                  {n_collapse, n_flip, (long long)mesh.V.size(),
                   (long long)mesh.F.size()});
}

// ---------------------------------------------------------------------------
// Mesh quality improvement: valence-optimizing edge flips + guarded
// tangential smoothing (the flip/relax half of Botsch-Kobbelt isotropic
// remeshing; no splits/collapses, so vertex count and features are kept).
//
//   * FLIP an interior edge when it strictly reduces the squared deviation of
//     the four touched vertices' valences from ideal (6 interior, 4 boundary),
//     guarded so the two new triangles are non-degenerate, do not fold
//     against the old quad normal, and do not push the quad's minimum angle
//     below 30 degrees (or below its previous minimum when already worse).
//     This is what fixes bad valences and, with it, most awkward angles.
//
//   * TANGENTIAL SMOOTHING moves each interior vertex toward the area-
//     weighted centroid of its 1-ring, projected onto the tangent plane of
//     the vertex normal -- to first order the surface does not move, only the
//     triangulation slides along it. A vertex whose move would fold or
//     degenerate an incident face is reverted. Boundary vertices stay fixed.
// ---------------------------------------------------------------------------
static void improve_mesh_quality(Mesh& mesh, int iters, bool verbose,
                                 int num_threads) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (iters <= 0 || nv == 0 || nf == 0) return;
#ifdef _OPENMP
    if (num_threads > 0) omp_set_num_threads(num_threads);
#endif
    auto& V = mesh.V;
    auto& F = mesh.F;
    const int64_t P = (int64_t)nv;
    auto ekey = [P](int a, int b) -> int64_t {
        int64_t lo = a<b?a:b, hi = a<b?b:a; return lo*P + hi;
    };
    auto sub = [](const std::array<float,3>& a, const std::array<float,3>& b) {
        return std::array<float,3>{a[0]-b[0], a[1]-b[1], a[2]-b[2]};
    };
    auto cross = [](const std::array<float,3>& a, const std::array<float,3>& b) {
        return std::array<float,3>{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2],
                                   a[0]*b[1]-a[1]*b[0]};
    };
    auto dot = [](const std::array<float,3>& a, const std::array<float,3>& b) {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    };
    auto nrm = [&](const std::array<float,3>& a) { return std::sqrt(dot(a,a)); };
    // minimum angle (cos of max angle... we need min angle) of a triangle via
    // squared edge lengths; returns the cosine of the SMALLEST angle (max cos)
    auto min_angle_cos = [&](int a, int b, int c) -> float {
        auto d2 = [&](int p, int q) { auto e = sub(V[p], V[q]); return dot(e, e); };
        float la = d2(b,c), lb = d2(c,a), lc = d2(a,b);
        auto co = [](float opp, float s1, float s2) {
            float dn = 2.0f * std::sqrt(s1 * s2);
            return dn > 1e-30f ? (s1 + s2 - opp) / dn : 1.0f;
        };
        return std::max(co(la,lb,lc), std::max(co(lb,lc,la), co(lc,la,lb)));
    };
    const float kAngleFloorCos = std::cos(30.0f * 0.017453292519943295f);

    long total_flips = 0, total_moved = 0;
    for (int pass = 0; pass < iters; ++pass) {
        // ---- rebuild connectivity for this pass ----
        // edge -> the (<=2) faces on it
        std::unordered_map<int64_t, std::array<int,2>> etri;
        etri.reserve((size_t)nf * 3 / 2);
        bool nonmanifold = false;
        for (long t = 0; t < nf; ++t) {
            const auto& f = F[t];
            for (int k = 0; k < 3; ++k) {
                auto& slot = etri.emplace(ekey(f[k], f[(k+1)%3]),
                                          std::array<int,2>{-1,-1}).first->second;
                if (slot[0] < 0) slot[0] = (int)t;
                else if (slot[1] < 0) slot[1] = (int)t;
                else nonmanifold = true;   // shouldn't happen; be safe below
            }
        }
        std::vector<char> is_bnd(nv, 0);
        std::vector<int> val(nv, 0);   // valence = incident (unique) edges
        for (const auto& kv : etri) {
            int a = (int)(kv.first / P), b = (int)(kv.first % P);
            ++val[a]; ++val[b];
            if (kv.second[1] < 0) { is_bnd[a] = 1; is_bnd[b] = 1; }
        }

        // ---- valence-optimizing flips (serial; updates are local) ----
        long flips = 0;
        auto ideal = [&](int v) { return is_bnd[v] ? 4 : 6; };
        std::vector<int64_t> keys;
        keys.reserve(etri.size());
        for (const auto& kv : etri) keys.push_back(kv.first);
        for (int64_t key : keys) {
            auto it = etri.find(key);
            if (it == etri.end()) continue;
            int t0 = it->second[0], t1 = it->second[1];
            if (t0 < 0 || t1 < 0) continue;              // boundary edge
            int a = (int)(key / P), b = (int)(key % P);
            const auto& f0 = F[t0];
            const auto& f1 = F[t1];
            // verify faces still carry the edge (map may be stale after flips)
            auto has_edge = [&](const std::array<int,3>& f) {
                int cnt = 0;
                for (int k = 0; k < 3; ++k) if (f[k]==a || f[k]==b) ++cnt;
                return cnt == 2;
            };
            if (!has_edge(f0) || !has_edge(f1)) continue;
            int c = -1, d = -1;
            for (int k = 0; k < 3; ++k) { if (f0[k]!=a && f0[k]!=b) c = f0[k];
                                          if (f1[k]!=a && f1[k]!=b) d = f1[k]; }
            if (c < 0 || d < 0 || c == d) continue;
            if (etri.count(ekey(c, d))) continue;        // (c,d) already exists
            // valence gain: a,b lose one; c,d gain one
            auto sq = [](int x) { return x * x; };
            int before = sq(val[a]-ideal(a)) + sq(val[b]-ideal(b))
                       + sq(val[c]-ideal(c)) + sq(val[d]-ideal(d));
            int after  = sq(val[a]-1-ideal(a)) + sq(val[b]-1-ideal(b))
                       + sq(val[c]+1-ideal(c)) + sq(val[d]+1-ideal(d));
            if (after >= before) continue;
            if (val[a] <= 3 || val[b] <= 3) continue;    // avoid valence-2 spikes
            // geometric guards: locally consistent winding around the quad.
            // old: (a,b,c) + (b,a,d); new: (c,d,a)... derive winding from f0:
            // find orientation of (a,b) in f0 to keep consistency.
            bool ab_in_f0 = false;
            for (int k = 0; k < 3; ++k)
                if (f0[k]==a && f0[(k+1)%3]==b) ab_in_f0 = true;
            int aa = ab_in_f0 ? a : b, bb = ab_in_f0 ? b : a;
            // old tris: (aa,bb,c), (bb,aa,d); new tris: (c,d,bb), (d,c,aa)
            auto n0 = cross(sub(V[bb],V[aa]), sub(V[c],V[aa]));
            auto n1 = cross(sub(V[aa],V[bb]), sub(V[d],V[bb]));
            std::array<float,3> nav{n0[0]+n1[0], n0[1]+n1[1], n0[2]+n1[2]};
            float lav = nrm(nav);
            if (lav < 1e-20f) continue;
            auto m0 = cross(sub(V[d],V[c]), sub(V[bb],V[c]));   // (c,d,bb)
            auto m1 = cross(sub(V[c],V[d]), sub(V[aa],V[d]));   // (d,c,aa)
            if (nrm(m0) < 1e-20f || nrm(m1) < 1e-20f) continue; // degenerate
            if (dot(m0,nav) <= 0.0f || dot(m1,nav) <= 0.0f) continue;  // fold
            // angle floor: don't create angles below 30 deg unless the quad
            // was already worse, in which case require strict improvement
            float old_worst = std::max(min_angle_cos(aa,bb,c), min_angle_cos(bb,aa,d));
            float new_worst = std::max(min_angle_cos(c,d,bb), min_angle_cos(d,c,aa));
            if (new_worst > kAngleFloorCos && new_worst >= old_worst) continue;
            // apply: rewrite the two faces, update valences + edge map
            F[t0] = {c, d, bb};
            F[t1] = {d, c, aa};
            --val[a]; --val[b]; ++val[c]; ++val[d];
            etri.erase(it);
            etri.emplace(ekey(c, d), std::array<int,2>{t0, t1});
            // reassign the four quad-boundary edges to the new faces
            auto retag = [&](int u, int v, int oldt, int newt) {
                auto jt = etri.find(ekey(u, v));
                if (jt == etri.end()) return;
                if (jt->second[0] == oldt) jt->second[0] = newt;
                else if (jt->second[1] == oldt) jt->second[1] = newt;
            };
            retag(aa, c, t0, t1);  // old f0 had (aa,c); new owner of (aa,c) is t1? see below
            // new tris: t0 = (c,d,bb) owns edges (c,d),(d,bb),(bb,c)
            //           t1 = (d,c,aa) owns edges (d,c),(c,aa),(aa,d)
            // old tris: t0 = (aa,bb,c) owned (aa,bb),(bb,c),(c,aa)
            //           t1 = (bb,aa,d) owned (bb,aa),(aa,d),(d,bb)
            // (bb,c): stays with t0. (c,aa): t0 -> t1. (aa,d): stays with t1.
            // (d,bb): t1 -> t0. retag(aa,c,...) above already moved (c,aa).
            retag(d, bb, t1, t0);
            ++flips;
        }
        (void)nonmanifold;

        // ---- tangential smoothing (Jacobi step with fold-revert) ----
        // area-weighted 1-ring centroid + vertex normal
        std::vector<std::array<float,3>> cent(nv, {0,0,0});
        std::vector<float> wsum(nv, 0.0f);
        std::vector<std::array<float,3>> vnorm(nv, {0,0,0});
        for (long t = 0; t < nf; ++t) {
            const auto& f = F[t];
            auto n = cross(sub(V[f[1]],V[f[0]]), sub(V[f[2]],V[f[0]]));
            float area = 0.5f * nrm(n);
            std::array<float,3> fc{(V[f[0]][0]+V[f[1]][0]+V[f[2]][0])/3.0f,
                                   (V[f[0]][1]+V[f[1]][1]+V[f[2]][1])/3.0f,
                                   (V[f[0]][2]+V[f[1]][2]+V[f[2]][2])/3.0f};
            for (int k = 0; k < 3; ++k) {
                int v = f[k];
                for (int x = 0; x < 3; ++x) {
                    cent[v][x] += area * fc[x];
                    vnorm[v][x] += n[x];
                }
                wsum[v] += area;
            }
        }
        std::vector<std::array<float,3>> newV = V;
        #pragma omp parallel for schedule(static)
        for (int v = 0; v < nv; ++v) {
            if (is_bnd[v] || wsum[v] <= 1e-30f) continue;
            float ln = nrm(vnorm[v]);
            if (ln < 1e-20f) continue;
            std::array<float,3> n{vnorm[v][0]/ln, vnorm[v][1]/ln, vnorm[v][2]/ln};
            std::array<float,3> d{cent[v][0]/wsum[v] - V[v][0],
                                  cent[v][1]/wsum[v] - V[v][1],
                                  cent[v][2]/wsum[v] - V[v][2]};
            float dn = dot(d, n);
            const float lambda = 0.5f;
            for (int x = 0; x < 3; ++x)
                newV[v][x] = V[v][x] + lambda * (d[x] - dn * n[x]);
        }
        // fold-revert: a vertex whose move flips/degenerates an incident face
        // goes back to its old position (checked against the OLD face normal)
        std::vector<char> revert(nv, 0);
        #pragma omp parallel for schedule(static)
        for (long t = 0; t < nf; ++t) {
            const auto& f = F[t];
            auto no = cross(sub(V[f[1]],V[f[0]]), sub(V[f[2]],V[f[0]]));
            auto nn = cross(sub(newV[f[1]],newV[f[0]]), sub(newV[f[2]],newV[f[0]]));
            float lo = nrm(no), lnn = nrm(nn);
            if (lnn < 1e-20f || (lo > 1e-20f && dot(no,nn) <= 0.0f))
                for (int k = 0; k < 3; ++k) revert[f[k]] = 1;
        }
        long moved = 0;
        for (int v = 0; v < nv; ++v) {
            if (revert[v] || is_bnd[v]) continue;
            if (newV[v][0]!=V[v][0] || newV[v][1]!=V[v][1] || newV[v][2]!=V[v][2]) {
                V[v] = newV[v];
                ++moved;
            }
        }
        total_flips += flips;
        total_moved += moved;
        if (flips == 0 && moved == 0) break;
    }
    if (verbose)
        mlog::out(mlog::Stage::Quality, mmsg::quality_result,
                  {total_flips, total_moved, iters});
}

} // namespace

bool generate_mesh(
    const float* means, const float* quats,
    const float* log_scales, const float* logit_opac, const float* features_dc,
    int num_splats,
    const float* cam_pos, int num_cameras,
    const CameraParams& cams,
    const MeshingConfig& cfg,
    const std::string& output_path
) {
    const float iso = cfg.iso;
    auto t0 = Clock::now();

    // ---- validate the export request BEFORE any heavy work ----
    std::string formats_csv;
    for (const auto& f : cfg.formats)
        formats_csv += (formats_csv.empty() ? "" : ",") + f;
    if (formats_csv.empty()) formats_csv = "ply";
    std::vector<MeshFormatSpec> formats = parse_mesh_formats(formats_csv);
    std::string out_base = output_path;
    for (const char* ext : {".ply", ".obj", ".gltf", ".glb"}) {
        size_t n = std::strlen(ext);
        if (out_base.size() > n &&
            out_base.compare(out_base.size() - n, n, ext) == 0) {
            out_base.resize(out_base.size() - n);
            break;
        }
    }
    for (const auto& spec : formats) {
        std::string err = check_export_support(spec, cfg.color_mode);
        if (!err.empty()) {
            mlog::err_raw(mlog::Stage::Wrote, err);
            return false;
        }
    }

    OccupancyEvaluator ev(means, quats, log_scales, logit_opac, features_dc, num_splats,
                          cam_pos, num_cameras, cams, cfg);

    // ---- debug: dump one camera's occupancy moments and bail out ----
    // SS_MESH_DEBUG_RENDER=<cam_idx> -> /tmp/ss_moments.f32 (float32 [H,W,3]).
    if (const char* dbg = spirula::env("MESH_DEBUG_RENDER")) {
        int cam = std::atoi(dbg);
        std::vector<float> mom; int W = 0, H = 0;
        if (!ev.debug_render_moments(cam, mom, W, H)) {
            mlog::out(mlog::Stage::Debug, mmsg::debug_render_unavailable);
            return false;
        }
        double m0sum = 0; float m0max = 0, mn = 1e30f, mx = -1e30f;
        size_t seen = 0;
        for (size_t p = 0; p < (size_t)W * H; ++p) {
            float m0 = mom[3*p], me = mom[3*p+1];
            m0sum += m0; m0max = std::max(m0max, m0);
            if (m0 > 0.5f) { mn = std::min(mn, me); mx = std::max(mx, me); ++seen; }
        }
        // A dump of numbers behind SS_MESH_DEBUG_RENDER, read by whoever is
        // debugging the occupancy field rather than by whoever is waiting for
        // a mesh. It stays as it is.
        {
            char buf[256];
            std::snprintf(buf, sizeof buf,
                          "cam %d: %dx%d, mean m0=%.4f max m0=%.4f; "
                          "depth(m0>0.5) in [%.3f, %.3f] over %zu px",
                          cam, W, H, m0sum / ((double)W * H), m0max, mn, mx, seen);
            mlog::out_raw(mlog::Stage::Debug, buf);
        }
        FILE* f = std::fopen("/tmp/ss_moments.f32", "wb");
        if (f) {
            int hdr[3] = {H, W, 3};
            std::fwrite(hdr, sizeof(int), 3, f);
            std::fwrite(mom.data(), sizeof(float), mom.size(), f);
            std::fclose(f);
            mlog::out_raw(mlog::Stage::Debug,
                          "/tmp/ss_moments.f32 (header: H W 3, then floats)");
        }
        return true;
    }

    // Refused here rather than inside the solver, which can only report the
    // empty result as "no tetrahedra".
    if ((long long)ev.num_kept() * 7 > delaunay3d::kMaxPoints) {
        mlog::fail(mlog::Stage::PointCloud, mmsg::too_many_gaussians,
                   {(long long)ev.num_kept(), (long long)delaunay3d::kMaxPoints,
                    (long long)(delaunay3d::kMaxPoints / 7)});
        return false;
    }

    // Each long phase announces itself BEFORE it runs, not only when it
    // finishes. A phase that takes minutes and prints only on completion is
    // indistinguishable from a hang, and it leaves any progress display with
    // nothing to show.
    // ---- 1. point cloud (float everywhere except the Delaunay call) ----
    if (cfg.verbose)
        mlog::out(mlog::Stage::PointCloud, mmsg::point_cloud_sampling,
                  {ev.num_kept()});
    std::vector<float> pts;
    ev.generate_point_cloud(pts);
    const int P = ev.num_points();
    if (cfg.verbose)
        mlog::out(mlog::Stage::PointCloud, mmsg::point_cloud_done,
                  {P, mlog::num(secs_since(t0), 2)});

    // ---- 2. Delaunay tetrahedralization (needs double precision) ----
    auto t1 = Clock::now();
    if (cfg.verbose)
        mlog::out(mlog::Stage::Delaunay, mmsg::delaunay_start, {P});
    std::vector<double> pts_d(pts.begin(), pts.end());
    // The float copy is dead until step 3 and float(double(f)) == f, so hand its
    // pages back before the solver claims ~260 bytes per point.
    pts.clear(); pts.shrink_to_fit();
    auto tri = delaunay3d::compute_delaunay_3d(pts_d.data(), P, cfg.num_threads, false);
    pts.assign(pts_d.begin(), pts_d.end());
    pts_d.clear(); pts_d.shrink_to_fit();
    const int M = tri.nb_cells;
    if (cfg.verbose)
        mlog::out(mlog::Stage::Delaunay, mmsg::delaunay_done,
                  {M, tri.num_threads, mlog::num(secs_since(t1), 2)});
    if (M == 0) {
        mlog::fail(mlog::Stage::Delaunay, mmsg::no_tetrahedra);
        return false;
    }

    // ---- 3. occupancy at all vertices ----
    auto t2 = Clock::now();
    if (cfg.verbose)
        mlog::out(mlog::Stage::Occupancy, mmsg::occupancy_start, {P});
    std::vector<float> occ(P);
    ev.evaluate(pts.data(), P, occ.data());
    if (cfg.verbose)
        mlog::out(mlog::Stage::Occupancy, mmsg::seconds,
                  {mlog::num(secs_since(t2), 2)});

    // ---- 4. collect cut edges ----
    auto t3 = Clock::now();
    if (cfg.verbose)
        mlog::out(mlog::Stage::CutEdges, mmsg::cut_edges_start, {M});
    const int* cv = (const int*)tri.cell_vertices.data();
    // One marching-tetrahedra case index per tet, so the two serial passes below
    // read a byte instead of gathering four occupancies each.
    std::vector<uint8_t> code((size_t)M);
    int64_t n_mt_tri = 0;
    #pragma omp parallel for schedule(static) reduction(+:n_mt_tri)
    for (long t = 0; t < (long)M; ++t) {
        const int* c = cv + 4 * t;
        int k = (occ[c[0]] < iso ? 1 : 0) | (occ[c[1]] < iso ? 2 : 0) |
                (occ[c[2]] < iso ? 4 : 0) | (occ[c[3]] < iso ? 8 : 0);
        code[t] = (uint8_t)k;
        n_mt_tri += kMT[k].ntri;
    }
    // Every mesh vertex is one cut edge, and the isosurface is a triangle mesh,
    // so V ~ F/2; 1.4x of that keeps the table's load factor near half.
    const size_t e_est = (size_t)(n_mt_tri / 2) + 1;
    EdgeTable edge_id(e_est + e_est / 2 + e_est / 10);
    std::vector<int32_t> ea, eb;
    std::vector<float> oa, ob;
    ea.reserve(e_est); eb.reserve(e_est);
    oa.reserve(e_est); ob.reserve(e_est);
    auto get_edge = [&](int va, int vb) {
        const int id = (int)ea.size();
        if (edge_id.insert(edge_key(va, vb, P), id) >= 0) return;
        ea.push_back(va); eb.push_back(vb);
        oa.push_back(occ[va]); ob.push_back(occ[vb]);
    };
    for (long t = 0; t < (long)M; ++t) {
        const int k = code[t];
        if (k == 0 || k == 15) continue;
        const int* c = cv + 4 * t;
        for (int e = 0; e < 6; ++e) {
            const int ia = kTetEdge[e][0], ib = kTetEdge[e][1];
            if (((k >> ia) & 1) != ((k >> ib) & 1)) get_edge(c[ia], c[ib]);
        }
    }
    const int E = (int)ea.size();
    if (cfg.verbose)
        mlog::out(mlog::Stage::CutEdges, mmsg::cut_edges_done,
                  {E, mlog::num(secs_since(t3), 2)});
    if (E == 0) {
        mlog::fail(mlog::Stage::CutEdges, mmsg::isosurface_empty,
                   {mlog::num(iso, 3)});
        return false;
    }

    // ---- 5. bisection: one mesh vertex per cut edge ----
    auto t4 = Clock::now();
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bisection, mmsg::bisection_start,
                  {E, cfg.bisection_iters});
    Mesh mesh;
    mesh.V.resize(E);
    {
        std::vector<float> xyz((size_t)3 * E);
        ev.bisect_edges(pts.data(), ea.data(), eb.data(), oa.data(), ob.data(), E, xyz.data());
        for (size_t i = 0; i < (size_t)E; ++i)
            mesh.V[i] = {xyz[3*i], xyz[3*i+1], xyz[3*i+2]};
    }
    std::vector<int32_t>().swap(ea); std::vector<int32_t>().swap(eb);
    std::vector<float>().swap(oa);   std::vector<float>().swap(ob);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bisection, mmsg::seconds,
                  {mlog::num(secs_since(t4), 2)});

    // ---- 6. emit triangles ----
    // Delaunay tets have no consistent signed-volume orientation, so the MT
    // table's winding alone is not globally coherent. Orient every triangle by
    // the field instead: its normal must point toward the EMPTY side (occ<iso),
    // i.e. away from the occupied tet corners. This yields a consistently
    // outward-facing, watertight mesh (good for backface culling / normals).
    auto t5 = Clock::now();
    mesh.F.reserve((size_t)n_mt_tri);
    for (long t = 0; t < (long)M; ++t) {
        const MTCase& mc = kMT[code[t]];
        if (mc.ntri == 0) continue;
        const int* c = cv + 4 * t;

        // outward direction = (centroid of empty corners) - (centroid of solid)
        float cin[3] = {0,0,0}, cout[3] = {0,0,0};
        int nin = 0, nout = 0;
        for (int k = 0; k < 4; ++k) {
            const float* pc = &pts[3 * c[k]];
            if (occ[c[k]] < iso) { cout[0]+=pc[0]; cout[1]+=pc[1]; cout[2]+=pc[2]; ++nout; }
            else                 { cin[0]+=pc[0];  cin[1]+=pc[1];  cin[2]+=pc[2];  ++nin; }
        }
        float dout[3];
        for (int a = 0; a < 3; ++a)
            dout[a] = (nout ? cout[a]/nout : 0.f) - (nin ? cin[a]/nin : 0.f);

        for (int ti = 0; ti < mc.ntri; ++ti) {
            int vid[3];
            bool ok = true;
            for (int k = 0; k < 3; ++k) {
                int ca = c[mc.e[ti][k][0]], cb = c[mc.e[ti][k][1]];
                vid[k] = edge_id.find(edge_key(ca, cb, P));
                if (vid[k] < 0) { ok = false; break; }
            }
            if (!ok) continue;
            // flip winding if the triangle normal faces the solid side
            const auto& P0 = mesh.V[vid[0]];
            const auto& P1 = mesh.V[vid[1]];
            const auto& P2 = mesh.V[vid[2]];
            float e1[3] = {P1[0]-P0[0], P1[1]-P0[1], P1[2]-P0[2]};
            float e2[3] = {P2[0]-P0[0], P2[1]-P0[1], P2[2]-P0[2]};
            float nx = e1[1]*e2[2] - e1[2]*e2[1];
            float ny = e1[2]*e2[0] - e1[0]*e2[2];
            float nz = e1[0]*e2[1] - e1[1]*e2[0];
            if (nx*dout[0] + ny*dout[1] + nz*dout[2] < 0.0f)
                std::swap(vid[1], vid[2]);
            mesh.F.push_back({vid[0], vid[1], vid[2]});
        }
    }
    if (cfg.verbose)
        mlog::out(mlog::Stage::Marching, mmsg::marching_done,
                  {(long long)mesh.F.size(), mlog::num(secs_since(t5), 2)});

    edge_id.release();
    std::vector<uint8_t>().swap(code);
    std::vector<float>().swap(occ);
    pts.clear(); pts.shrink_to_fit();
    tri.cell_adjacents.clear(); tri.cell_adjacents.shrink_to_fit();
    tri.cell_vertices.clear(); tri.cell_vertices.shrink_to_fit();

    // ---- 7. merge (do this once to reduce chance of GPU OOM during 6b, but risk CPU OOM) ----
    auto t6 = Clock::now();
    merge_vertices(mesh, cfg.merge_factor, cfg.merge_max_flip_deg, cfg.verbose, cfg.num_threads);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Merge, mmsg::seconds,
                  {mlog::num(secs_since(t6), 2)});

  for (int iter = 0; iter < 2; ++iter) {

    // ---- 6b. cull vertices seen by no training camera (dataset path only) ----
    // A vertex is kept iff some camera both sees it in-frame and has an
    // unobstructed line of sight to it (occlusion tested against the mesh's own
    // triangles via a GPU LBVH; see OccupancyEvaluator::cull_unseen_vertices).
    // Removing a vertex drops every face incident to it. Since this only deletes
    // faces, each surviving edge keeps <= its original triangle count, so the
    // mesh stays manifold-with-boundary: the only new topology is boundary edges
    // (holes). The downstream merge (link condition handles 1-triangle boundary
    // edges) and orient (edge->triangle map allows the -1 "no neighbour" slot)
    // are already boundary-safe, so no further repair is needed.
    if (cfg.cull_unseen && ev.has_render_cameras() && !mesh.V.empty()) {
        auto tc = Clock::now();
        const int nv = (int)mesh.V.size();
        const int nf = (int)mesh.F.size();
        std::vector<unsigned char> vis(nv, 1);
        ev.cull_unseen_vertices(
            &mesh.V[0][0], nv, nf > 0 ? &mesh.F[0][0] : nullptr, nf, vis.data());

        std::vector<int> remap(nv, -1);
        std::vector<std::array<float,3>> newV; newV.reserve(nv);
        for (int v = 0; v < nv; ++v)
            if (vis[v]) { remap[v] = (int)newV.size(); newV.push_back(mesh.V[v]); }
        std::vector<std::array<int,3>> newF; newF.reserve(mesh.F.size());
        long dropped_f = 0;
        for (int t = 0; t < nf; ++t) {
            const auto& f = mesh.F[t];
            if (!vis[f[0]] || !vis[f[1]] || !vis[f[2]]) { ++dropped_f; continue; }
            newF.push_back({remap[f[0]], remap[f[1]], remap[f[2]]});
        }
        size_t removed_v = (size_t)nv - newV.size();
        mesh.V.swap(newV);
        mesh.F.swap(newF);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Cull, mmsg::cull_result,
                      {(long long)removed_v, nv, dropped_f,
                       (long long)mesh.V.size(), (long long)mesh.F.size(),
                       mlog::num(secs_since(tc), 2)});
    }

    // ---- 7. manifold-preserving merge (with fold/sliver guard) ----
    auto t6 = Clock::now();
    merge_vertices(mesh, cfg.merge_factor, cfg.merge_max_flip_deg, cfg.verbose, cfg.num_threads);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Merge, mmsg::seconds,
                  {mlog::num(secs_since(t6), 2)});

    // ---- 7a. post-merge cleanup: drop floaters, then fill small holes ----
    // Run after merge (final topology) and before orient so the orient pass
    // gives the new fill faces a consistent, outward winding. Floaters first so
    // we do not waste work filling holes in components about to be deleted.
    // Order matters: split_nonmanifold_vertices runs BEFORE fill_holes.
    //   - Culling deletes faces, which can split a vertex's fan and leave
    //     non-manifold (bowtie) vertices; at those the boundary has >2 edges, so
    //     a "butterfly" hole pinches through one point. Splitting first makes
    //     every boundary vertex have exactly two boundary edges, so the boundary
    //     becomes a set of disjoint simple loops -- a butterfly becomes two
    //     separate simple loops that fill_holes can then fill (instead of being
    //     skipped), and every filled loop is a true boundary circle. Filling a
    //     disk onto a true boundary circle preserves orientability; filling the
    //     bogus loops produced by ambiguous chaining at a bowtie did NOT (it was
    //     the sole source of non-orientable defects -- MT+merge+cull alone are
    //     orientable).
    //   - The split also isolates vertex-only-attached doublets into their own
    //     tiny components, which the final remove_floaters deletes.
    // A second split runs AFTER fill: fill can occasionally close a loop in a way
    // that leaves a single bowtie vertex, so re-splitting guarantees a fully
    // vertex-manifold result (split preserves orientability). The final floater
    // then removes whole isolated components (manifold-safe) left by either split.
    auto tcl = Clock::now();
    dedup_faces(mesh, cfg.verbose);
    remove_floaters(mesh, cfg.floater_min_faces, cfg.verbose);
    split_nonmanifold_vertices(mesh, cfg.verbose);
    fill_holes(mesh, cfg.fill_hole_ratio, cfg.fill_hole_max_edges, cfg.verbose);
    split_nonmanifold_vertices(mesh, cfg.verbose);
    remove_floaters(mesh, cfg.floater_min_faces, cfg.verbose);
    fix_degenerate_faces(mesh, cfg.degenerate_angle_deg, cfg.verbose);
    dedup_faces(mesh, cfg.verbose);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::seconds,
                  {mlog::num(secs_since(tcl), 2)});
  }

    // ---- 7a2. valence flips + tangential relaxation ----
    if (cfg.quality_iters > 0) {
        auto tq = Clock::now();
        improve_mesh_quality(mesh, cfg.quality_iters, cfg.verbose, cfg.num_threads);
        // the relaxation moved vertices; one more degenerate sweep catches
        // slivers that were locked before
        fix_degenerate_faces(mesh, cfg.degenerate_angle_deg, cfg.verbose);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Quality, mmsg::seconds,
                      {mlog::num(secs_since(tq), 2)});
    }

    // ---- 7b. globally consistent, outward-facing winding ----
    auto t7 = Clock::now();
    orient_mesh(mesh);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Orient, mmsg::seconds,
                  {mlog::num(secs_since(t7), 2)});

    // ---- 7c. smooth vertex normals (before any seam split, so normals stay
    //      continuous across UV seams) ----
    compute_vertex_normals(mesh);

    // ---- 7d. color: per-vertex, or a baked texture atlas ----
    auto t8 = Clock::now();
    if (cfg.color_mode == MeshColorMode::Vertex && !mesh.V.empty()) {
        const int nv = (int)mesh.V.size();
        std::vector<float> rgb(3 * nv);
        ev.colorize(&mesh.V[0][0], nv, rgb.data());
        mesh.C.resize(nv);
        for (int i = 0; i < nv; ++i)
            for (int a = 0; a < 3; ++a)
                mesh.C[i][a] = (unsigned char)std::min(std::max(
                    (int)std::lround(rgb[3*i+a] * 255.0f), 0), 255);
        if (cfg.verbose)
            mlog::out(mlog::Stage::Color, mmsg::seconds,
                      {mlog::num(secs_since(t8), 2)});
    } else if (cfg.color_mode == MeshColorMode::Texture && !mesh.V.empty()) {
        UVAtlasConfig uvcfg;
        uvcfg.texture_size = cfg.texture_size;
        uvcfg.gutter_px = cfg.tex_gutter_px;
        uvcfg.chart_angle_deg = cfg.chart_angle_deg;
        uvcfg.num_threads = cfg.num_threads;
        uvcfg.verbose = cfg.verbose;
        // Texel budget per face = its projected pixel area in the best
        // unoccluded training view (3D area x (focal/z)^2): texels go where
        // the views actually captured detail. Face count alone starves
        // smooth-but-prominent surfaces -- big splats mean big faces, yet the
        // cameras may have seen them close-up -- and world area wastes the
        // atlas on huge distant background faces.
        if (ev.has_render_cameras()) {
            const size_t nvv = mesh.V.size(), nff = mesh.F.size();
            std::vector<float> nu(nvv, 0.0f);
            ev.view_texel_density(&mesh.V[0][0], (int)nvv, nu.data());
            // cap at the 99th percentile: a camera nearly touching a surface
            // must not hog the whole atlas
            std::vector<float> pos;
            pos.reserve(nvv);
            for (float v : nu) if (v > 0.0f) pos.push_back(v);
            if (!pos.empty()) {
                size_t q = (size_t)(0.99 * (double)(pos.size() - 1));
                std::nth_element(pos.begin(), pos.begin() + q, pos.end());
                const float nu_cap = std::max(pos[q], 1e-12f);
                std::vector<float> w(nff, 0.0f);
                std::vector<float> wpos;
                wpos.reserve(nff);
                for (size_t t = 0; t < nff; ++t) {
                    const auto& f = mesh.F[t];
                    const auto &A = mesh.V[f[0]], &B = mesh.V[f[1]], &C = mesh.V[f[2]];
                    float e1[3] = {B[0]-A[0], B[1]-A[1], B[2]-A[2]};
                    float e2[3] = {C[0]-A[0], C[1]-A[1], C[2]-A[2]};
                    float cx = e1[1]*e2[2] - e1[2]*e2[1];
                    float cy = e1[2]*e2[0] - e1[0]*e2[2];
                    float cz = e1[0]*e2[1] - e1[1]*e2[0];
                    float a3d = 0.5f * std::sqrt(cx*cx + cy*cy + cz*cz);
                    float nf_ = std::min(std::max({nu[f[0]], nu[f[1]], nu[f[2]]}),
                                         nu_cap);
                    w[t] = a3d * nf_ * nf_;
                    if (w[t] > 0.0f) wpos.push_back(w[t]);
                }
                // unseen faces (interior, undercarriage) still get a small
                // share so they don't collapse to a single texel
                if (!wpos.empty()) {
                    size_t m = wpos.size() / 2;
                    std::nth_element(wpos.begin(), wpos.begin() + m, wpos.end());
                    const float floor_w = 0.05f * std::max(wpos[m], 1e-12f);
                    for (auto& v : w) v = std::max(v, floor_w);
                    uvcfg.face_weight = std::move(w);
                }
            }
        }
        std::vector<int> face_chart = build_uv_atlas(mesh, uvcfg);
        bake_texture(mesh, face_chart, uvcfg,
                     [&](const float* xyz, int n, float* rgb) {
                         ev.colorize(xyz, n, rgb);
                     });
        if (cfg.verbose)
            mlog::out(mlog::Stage::Texture, mmsg::seconds,
                      {mlog::num(secs_since(t8), 2)});
    }

    // ---- 8. write ----
    if (cfg.verbose) print_mesh_stats(mesh);
    for (const auto& spec : formats)
        write_mesh(mesh, cfg.color_mode, spec, out_base, /*verbose=*/true);
    mlog::out(mlog::Stage::Done, mmsg::done_summary,
              {(long long)mesh.V.size(), (long long)mesh.F.size(),
               mlog::num(secs_since(t0), 2)});
    return true;
}

} // namespace meshing
