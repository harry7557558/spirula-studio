// MeshMerge.cpp -- see MeshStages.h and docs/notes/mesh-merge.md.

#include "mesh/MeshStages.h"
#include "mesh/MeshLog.h"

#include <algorithm>
#include <limits>
#include <queue>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

namespace mmsg = spirula::i18n::msg::mesh;

std::vector<int> morton_rank(const std::vector<std::array<float,3>>& V) {
    const int nv = (int)V.size();
    std::vector<int> rank(nv);
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (const auto& p : V)
        for (int a = 0; a < 3; ++a) { lo[a] = std::min(lo[a], p[a]); hi[a] = std::max(hi[a], p[a]); }
    std::vector<std::pair<uint64_t, int>> code(nv);
    #pragma omp parallel for schedule(static)
    for (int v = 0; v < nv; ++v) {
        uint64_t c = 0;
        uint32_t q[3];
        for (int a = 0; a < 3; ++a)
            q[a] = (uint32_t)std::min(1023.0f, std::max(0.0f,
                1023.0f * (V[v][a] - lo[a]) / std::max(hi[a] - lo[a], 1e-20f)));
        for (int bit = 9; bit >= 0; --bit)
            for (int a = 0; a < 3; ++a) c = (c << 1) | ((q[a] >> bit) & 1u);
        code[v] = {c, v};
    }
    std::sort(code.begin(), code.end());
    for (int i = 0; i < nv; ++i) rank[code[i].second] = i;
    return rank;
}

// Vertices per block of the merge's parallel greedy pass.
constexpr int kMergeBlock = 1 << 15;

// A collapse whose fan, before and after, stays within 30 degrees of flat skips
// the crossing test.
constexpr float kFanFlatCos = 0.86602540f;

// Collapses edges, lowest error first, while the surface moves less than both
// ends' limits. Blocks of kMergeBlock vertices run in parallel, so the outcome
// does not depend on thread count (docs/notes/mesh-merge.md).
void merge_vertices(Mesh& mesh, MergeLimits lim, float max_flip_deg,
                           bool quadrics, bool verbose, int num_threads) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (nv == 0 || lim.tol.size() != (size_t)nv) return;
#ifdef _OPENMP
    if (num_threads > 0) omp_set_num_threads(num_threads);
#else
    (void)num_threads;
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

    auto dist3 = [](const float* a, const float* b) {
        float dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    auto on_boundary = [&](int x) {
        for (int t : vt[x]) {
            if (!talive[t]) continue;
            for (int w : F[t]) {
                if (w == x) continue;
                int n = 0;
                for (int s : vt[x])
                    if (talive[s] && (F[s][0] == w || F[s][1] == w || F[s][2] == w)) ++n;
                if (n == 1) return true;
            }
        }
        return false;
    };

    // Link condition for edge (u,v): the common neighbours of u and v are
    // exactly the far corners of the faces on it. Read-only on the topology.
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
        const auto& smaller = (au.size() < av.size()) ? au : av;
        const auto& larger  = (au.size() < av.size()) ? av : au;
        int common = 0;
        for (int w : smaller) {
            if (w == u || w == v) continue;
            if (larger.find(w) != larger.end()) {
                ++common;
                if (w != opp0 && w != opp1) return false;
            }
        }
        if (common != opp_cnt) return false;
        if (opp_cnt == 2) {
            // a closed tetrahedron would fold into two faces on the same corners
            auto has_cd = [&](int x) {
                for (int t : vt[x]) {
                    if (!talive[t]) continue;
                    const auto& f = F[t];
                    const bool c = f[0] == opp0 || f[1] == opp0 || f[2] == opp0;
                    const bool d = f[0] == opp1 || f[1] == opp1 || f[2] == opp1;
                    if (c && d) return true;
                }
                return false;
            };
            if (has_cd(u) && has_cd(v)) return false;
            // joining two boundaries across the surface pinches it
            if (on_boundary(u) && on_boundary(v)) return false;
        }
        return true;
    };

    // Fold guard: every reshaped face around u and v keeps its normal within
    // max_flip_deg and does not go degenerate. Read-only, so race-free.
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
        if (cos_flip <= -1.0f) return true;
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

    auto is_boundary = [&](int a, int b) {
        int n = 0;
        for (int t : vt[a]) {
            if (!talive[t]) continue;
            const auto& f = F[t];
            if (f[0] == b || f[1] == b || f[2] == b) ++n;
        }
        return n == 1;
    };
    // Vertices that may touch a boundary edge (a survivor inherits the flag),
    // so the exact test runs only where it can succeed.
    std::vector<char> bflag(nv, 0);
    #pragma omp parallel for schedule(dynamic, 4096)
    for (int v = 0; v < nv; ++v)
        for (int w : adj[v])
            if (is_boundary(v, w)) { bflag[v] = 1; break; }
    auto boundary_edge = [&](int a, int b) { return bflag[a] && bflag[b] && is_boundary(a, b); };

    // With `quadrics`, each vertex keeps the area-weighted planes of every face
    // merged into it (Garland-Heckbert), so error is measured against the
    // surface the pass started from rather than the one it has simplified.
    std::vector<std::array<double,10>> Q(quadrics ? nv : 0);
    std::vector<double> QA(quadrics ? nv : 0, 0.0);
    auto add_plane = [&](int v, const double* n, double d, double w) {
        auto& q = Q[v];
        q[0] += w*n[0]*n[0]; q[1] += w*n[0]*n[1]; q[2] += w*n[0]*n[2]; q[3] += w*n[0]*d;
        q[4] += w*n[1]*n[1]; q[5] += w*n[1]*n[2]; q[6] += w*n[1]*d;
        q[7] += w*n[2]*n[2]; q[8] += w*n[2]*d;    q[9] += w*d*d;
        QA[v] += w;
    };
    if (quadrics) {
        #pragma omp parallel for schedule(dynamic, 4096)
        for (int v = 0; v < nv; ++v)
            for (int t : vt[v]) {
                if (!talive[t]) continue;
                const auto& f = F[t];
                float fn[3];
                tri_normal(V[f[0]].data(), V[f[1]].data(), V[f[2]].data(), fn[0], fn[1], fn[2]);
                const double l = std::sqrt((double)fn[0]*fn[0] + (double)fn[1]*fn[1] + (double)fn[2]*fn[2]);
                if (l < 1e-30) continue;
                const double n[3] = {fn[0] / l, fn[1] / l, fn[2] / l};
                const double area = 0.5 * l;
                add_plane(v, n, -(n[0]*V[f[0]][0] + n[1]*V[f[0]][1] + n[2]*V[f[0]][2]), area);
                for (int k = 0; k < 3; ++k) {
                    const int a = f[k], b = f[(k + 1) % 3];
                    if ((a != v && b != v) || !boundary_edge(a, b)) continue;
                    const double e[3] = {(double)V[b][0]-V[a][0], (double)V[b][1]-V[a][1],
                                         (double)V[b][2]-V[a][2]};
                    double m[3] = {n[1]*e[2]-n[2]*e[1], n[2]*e[0]-n[0]*e[2], n[0]*e[1]-n[1]*e[0]};
                    const double ml = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
                    if (ml < 1e-30) continue;
                    for (double& x : m) x /= ml;
                    add_plane(v, m, -(m[0]*V[a][0] + m[1]*V[a][1] + m[2]*V[a][2]), area);
                }
            }
    }
    auto quadric_error = [&](int u, int v, const float* p) {
        const auto& a = Q[u]; const auto& b = Q[v];
        double q[10];
        for (int i = 0; i < 10; ++i) q[i] = a[i] + b[i];
        const double x = p[0], y = p[1], z = p[2];
        const double e2 = q[0]*x*x + 2*q[1]*x*y + 2*q[2]*x*z + 2*q[3]*x
                        + q[4]*y*y + 2*q[5]*y*z + 2*q[6]*y
                        + q[7]*z*z + 2*q[8]*z + q[9];
        const double w = QA[u] + QA[v];
        return w > 0.0 ? (float)std::sqrt(std::max(e2 / w, 0.0)) : 0.0f;
    };

    // Without quadrics, error is the area-weighted RMS distance to the planes
    // of the faces around u and v and of planes standing on their boundary edges.
    struct Plane { float n[3], o[3], w; };
    auto gather_planes = [&](int u, int v, std::vector<Plane>& out) {
        out.clear();
        for (int c : {u, v})
            for (int t : vt[c]) {
                if (!talive[t]) continue;
                const auto& f = F[t];
                if (c == v && (f[0] == u || f[1] == u || f[2] == u)) continue;
                const float* a = V[f[0]].data();
                float n[3];
                tri_normal(a, V[f[1]].data(), V[f[2]].data(), n[0], n[1], n[2]);
                const float l = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
                if (l < 1e-30f) continue;
                for (float& x : n) x /= l;
                out.push_back({{n[0], n[1], n[2]}, {a[0], a[1], a[2]}, 0.5f * l});
                for (int k = 0; k < 3; ++k) {
                    const int ea = f[k], eb = f[(k + 1) % 3];
                    if ((ea != c && eb != c) || !boundary_edge(ea, eb)) continue;
                    const float* q = V[ea].data();
                    const float d[3] = {V[eb][0]-q[0], V[eb][1]-q[1], V[eb][2]-q[2]};
                    float m[3] = {n[1]*d[2]-n[2]*d[1], n[2]*d[0]-n[0]*d[2], n[0]*d[1]-n[1]*d[0]};
                    const float ml = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
                    if (ml < 1e-30f) continue;
                    for (float& x : m) x /= ml;
                    out.push_back({{m[0], m[1], m[2]}, {q[0], q[1], q[2]}, 0.5f * l});
                }
            }
    };
    auto plane_error = [](const std::vector<Plane>& planes, const float* p) {
        double e2 = 0.0, w = 0.0;
        for (const Plane& q : planes) {
            const double d = q.n[0]*(p[0]-q.o[0]) + q.n[1]*(p[1]-q.o[1]) + q.n[2]*(p[2]-q.o[2]);
            e2 += q.w * d * d;
            w += q.w;
        }
        return w > 0.0 ? (float)std::sqrt(e2 / w) : 0.0f;
    };

    // k = 0: u's position, 1: v's, 2: the midpoint
    auto position = [&](int u, int v, int k, float* p) {
        for (int a = 0; a < 3; ++a)
            p[a] = k == 0 ? V[u][a] : k == 1 ? V[v][a] : 0.5f * (V[u][a] + V[v][a]);
    };
    auto within_cap = [&](int u, int v, const float* p, float cap) {
        for (int c : {u, v})
            for (int w : adj[c])
                if (w != u && w != v && dist3(p, V[w].data()) > cap) return false;
        return true;
    };

    // Each vertex's colour box: the range of the colours merged into it.
    const bool colors = !lim.rgb.empty();
    std::vector<std::array<float,3>> clo, chi;
    if (colors) { clo = lim.rgb; chi = lim.rgb; }
    auto colors_ok = [&](int u, int v) {
        if (!colors) return true;
        for (int a = 0; a < 3; ++a)
            if (std::max(chi[u][a], chi[v][a]) - std::min(clo[u][a], clo[v][a]) > lim.color_tol)
                return false;
        return true;
    };

    // Lowest-error position whose error and new edges fit the limits: k and
    // its error, or k = -1.
    auto best_collapse = [&](int u, int v, float& err, std::vector<Plane>& planes) {
        if (!colors_ok(u, v)) return -1;
        const float tol = std::min(lim.tol[u], lim.tol[v]);
        const float cap = std::min(lim.max_edge[u], lim.max_edge[v]);
        int best = -1;
        err = 0.0f;
        if (!quadrics) gather_planes(u, v, planes);
        for (int k : {2, 0, 1}) {
            float p[3];
            position(u, v, k, p);
            if (!within_cap(u, v, p, cap)) continue;
            const float e = quadrics ? quadric_error(u, v, p) : plane_error(planes, p);
            if (e <= tol && (best < 0 || e < 0.99f * err)) { best = k; err = e; }
        }
        return best;
    };

    // Morton order of the vertices, cut into blocks of kMergeBlock: each block is
    // simplified greedily by one thread, touching only vertices whose closed
    // 1-rings lie inside it; a second phase shifts the cuts by half a block.
    const std::vector<int> rank = morton_rank(V);

    std::vector<uint32_t> stamp(nv, 0);
    struct Item { float pri; int u, v; uint32_t su, sv; int k; };
    struct Later {
        bool operator()(const Item& a, const Item& b) const { return a.pri > b.pri; }
    };
    long total_collapses = 0;
    for (int phase = 0; phase < 2; ++phase) {
        const int shift = phase ? kMergeBlock / 2 : 0;
        auto block_of = [&](int v) { return (rank[v] + shift) / kMergeBlock; };
        const int nblocks = (nv + shift) / kMergeBlock + 1;
        std::vector<std::vector<int>> members(nblocks);
        for (int v = 0; v < nv; ++v) if (valive[v]) members[block_of(v)].push_back(v);

        long phase_collapses = 0;
        #pragma omp parallel for schedule(dynamic, 1) reduction(+:phase_collapses)
        for (int bi = 0; bi < nblocks; ++bi) {
            auto inside = [&](int x) { return block_of(x) == bi; };
            auto interior = [&](int u, int v) {
                for (int c : {u, v}) {
                    if (!inside(c)) return false;
                    for (int w : adj[c]) if (!inside(w)) return false;
                }
                return true;
            };
            std::vector<Plane> planes;
            std::vector<std::array<int,3>> reshaped, around;
            std::vector<int> ring, ids;
            auto crosses_nearby = [&](int u, int v, const float* np) {
                reshaped.clear();
                around.clear();
                ring.clear();
                for (int c : {u, v})
                    for (int t : vt[c]) {
                        if (!talive[t]) continue;
                        std::array<int,3> f = F[t];
                        const bool hu = f[0] == u || f[1] == u || f[2] == u;
                        if (c == v && hu) continue;
                        if (hu && (f[0] == v || f[1] == v || f[2] == v)) continue;
                        around.push_back(f);   // the fan as it was
                        for (int& w : f) if (w == u || w == v) w = -1;
                        reshaped.push_back(f);
                    }
                const std::array<float,3> m = {np[0], np[1], np[2]};
                auto pt = [&](int w) -> const std::array<float,3>& { return w < 0 ? m : V[w]; };
                if (nearly_flat(pt, reshaped, around, kFanFlatCos)) return false;
                // Another block may be editing the faces of a vertex outside this
                // one; a vertex next to this block cannot move during the phase.
                for (int c : {u, v})
                    for (int w : adj[c]) {
                        if (w == u || w == v) continue;
                        for (int x : adj[w])
                            if (x != u && x != v && inside(x)) ring.push_back(x);
                    }
                std::sort(ring.begin(), ring.end());
                ring.erase(std::unique(ring.begin(), ring.end()), ring.end());
                ids.clear();
                for (int x : ring)
                    for (int t : vt[x]) if (talive[t]) ids.push_back(t);
                std::sort(ids.begin(), ids.end());
                ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
                around.clear();
                for (int t : ids) {
                    const auto& g = F[t];
                    if (g[0] == u || g[1] == u || g[2] == u || g[0] == v || g[1] == v || g[2] == v)
                        continue;
                    around.push_back(g);
                }
                return !nearly_flat(pt, reshaped, around) && faces_cross(pt, reshaped, around);
            };
            std::priority_queue<Item, std::vector<Item>, Later> heap;
            auto push = [&](int u, int v) {
                if (!inside(u) || !inside(v)) return;
                float err;
                const int k = best_collapse(u, v, err, planes);
                if (k < 0) return;
                const float len = std::sqrt(
                    (V[u][0]-V[v][0])*(V[u][0]-V[v][0]) + (V[u][1]-V[v][1])*(V[u][1]-V[v][1]) +
                    (V[u][2]-V[v][2])*(V[u][2]-V[v][2]));
                heap.push({err + 0.01f * len, u, v, stamp[u], stamp[v], k});
            };
            for (int u : members[bi])
                for (int w : adj[u]) if (w > u) push(u, w);
            while (!heap.empty()) {
                const Item it = heap.top();
                heap.pop();
                const int u = it.u, v = it.v;
                if (!valive[u] || !valive[v] || stamp[u] != it.su || stamp[v] != it.sv) continue;
                if (adj[u].find(v) == adj[u].end() || !interior(u, v)) continue;
                float np[3];
                position(u, v, it.k, np);
                if (!within_cap(u, v, np, std::min(lim.max_edge[u], lim.max_edge[v])) ||
                    !link_ok(u, v) || !geom_ok(u, v, np) || crosses_nearby(u, v, np)) continue;

                // collapse v -> u
                V[u][0] = np[0]; V[u][1] = np[1]; V[u][2] = np[2];
                if (quadrics) {
                    for (int i = 0; i < 10; ++i) Q[u][i] += Q[v][i];
                    QA[u] += QA[v];
                }
                bflag[u] |= bflag[v];
                lim.tol[u] = std::min(lim.tol[u], lim.tol[v]);
                lim.max_edge[u] = std::min(lim.max_edge[u], lim.max_edge[v]);
                if (colors)
                    for (int a = 0; a < 3; ++a) {
                        clo[u][a] = std::min(clo[u][a], clo[v][a]);
                        chi[u][a] = std::max(chi[u][a], chi[v][a]);
                    }
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
                ++stamp[u];
                ++phase_collapses;
                for (int w : adj[u]) push(u, w);
            }
        }
        total_collapses += phase_collapses;
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

} // namespace meshing
