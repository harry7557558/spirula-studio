// MeshQuality.cpp -- guarded local edits, the sliver fix and the quality pass (MeshStages.h).

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

// ---------------------------------------------------------------------------
// Local edits for the cleanup passes: the faces around each vertex, and an
// edge collapse and an edge flip that each keep the mesh manifold, fold-free
// and crossing-free, and do not make the worst nearby angle worse.
// ---------------------------------------------------------------------------
constexpr float kEditFloorSin = 0.05233596f;   // sin 3 deg: what an unforced edit may leave
constexpr float kEditTurnCos = 0.5f;           // a sound face turns at most 60 deg

namespace {

class LocalEdit {
public:
    using P = std::array<float,3>;
    using Tri = std::array<int,3>;
    Mesh& mesh;
    std::vector<VertexSet> vt;
    std::vector<char> alive;

    // One thread's edits: the block of vertices it may change (by Morton rank,
    // as in the merge), its scratch, the corners of every face it made.
    struct Ctx {
        const std::vector<int>* block_of = nullptr;   // null: one thread, every vertex
        int block = 0;
        std::vector<int> touched;
        long collapses = 0, flips = 0;
        std::vector<Tri> next, around;
        std::vector<int> old, gone, ring, ids;
        bool inside(int v) const { return !block_of || (*block_of)[v] == block; }
    };

    explicit LocalEdit(Mesh& m) : mesh(m), vt(m.V.size()), alive(m.F.size(), 1) {
        for (size_t t = 0; t < m.F.size(); ++t) {
            const auto& f = m.F[t];
            if (f[0] == f[1] || f[1] == f[2] || f[0] == f[2]) { alive[t] = 0; continue; }
            for (int v : f) vt[v].insert((int)t);
        }
    }

    // sin of a triangle's smallest angle; 0 when degenerate
    template <class Pt> static float min_sin(Pt&& pt, const Tri& f) {
        const P &a = pt(f[0]), &b = pt(f[1]), &c = pt(f[2]);
        float l[3];
        for (int k = 0; k < 3; ++k) {
            const P &p = pt(f[k]), &q = pt(f[(k + 1) % 3]);
            l[k] = std::sqrt((p[0]-q[0])*(p[0]-q[0]) + (p[1]-q[1])*(p[1]-q[1]) + (p[2]-q[2])*(p[2]-q[2]));
        }
        std::sort(l, l + 3);
        const P n = normal_of(a, b, c);
        const float a2 = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        return l[1] * l[2] > 0.0f ? std::min(1.0f, a2 / (l[1] * l[2])) : 0.0f;
    }
    static P normal_of(const P& a, const P& b, const P& c) {
        const float e1[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]}, e2[3] = {c[0]-a[0], c[1]-a[1], c[2]-a[2]};
        return {e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0]};
    }
    static bool has(const Tri& f, int v) { return f[0] == v || f[1] == v || f[2] == v; }
    static bool runs(const Tri& f, int a, int b) {
        for (int k = 0; k < 3; ++k) if (f[k] == a && f[(k + 1) % 3] == b) return true;
        return false;
    }

    int faces_on(int a, int b, int* out) const {
        int n = 0;
        for (int t : vt[a])
            if (alive[t] && has(mesh.F[t], b)) { if (n < 2) out[n] = t; ++n; }
        return n;
    }
    bool on_boundary(int x) const {
        for (int t : vt[x]) {
            if (!alive[t]) continue;
            for (int w : mesh.F[t]) {
                int tt[2];
                if (w != x && faces_on(x, w, tt) == 1) return true;
            }
        }
        return false;
    }
    bool adjacent(int a, int b) const {
        for (int t : vt[a]) if (alive[t] && has(mesh.F[t], b)) return true;
        return false;
    }

    // The link condition, refusing a closed tetrahedron and a boundary pinch.
    bool link_ok(int u, int v) const {
        int tt[2];
        const int ne = faces_on(u, v, tt);
        if (ne < 1 || ne > 2) return false;
        int opp[2] = {-1, -1};
        for (int i = 0; i < ne; ++i)
            for (int w : mesh.F[tt[i]]) if (w != u && w != v) opp[i] = w;
        for (int t : vt[u]) {
            if (!alive[t]) continue;
            for (int w : mesh.F[t])
                if (w != u && w != v && w != opp[0] && w != opp[1] && adjacent(v, w)) return false;
        }
        if (ne == 2) {
            auto face_cd = [&](int x) {
                for (int t : vt[x])
                    if (alive[t] && has(mesh.F[t], opp[0]) && has(mesh.F[t], opp[1])) return true;
                return false;
            };
            if (face_cd(u) && face_cd(v)) return false;
            if (on_boundary(u) && on_boundary(v)) return false;
        }
        return true;
    }

    // Checks the faces `next` would replace `old` (both lists of faces, `gone`
    // the ones that vanish): no fold, no crossing, no worse angle than allowed.
    template <class Pt>
    bool sound(Ctx& cx, Pt&& pt, bool strict, bool turn_guard) {
        const std::vector<Tri>& next = cx.next;
        const std::vector<int>& old = cx.old;
        const std::vector<int>& gone = cx.gone;
        auto cur = [&](int w) -> const P& { return mesh.V[w]; };
        float old_worst = 1.0f;
        for (int t : old) old_worst = std::min(old_worst, min_sin(cur, mesh.F[t]));
        for (int t : gone) old_worst = std::min(old_worst, min_sin(cur, mesh.F[t]));
        float new_worst = 1.0f;
        for (size_t i = 0; i < next.size(); ++i) {
            const float s = min_sin(pt, next[i]);
            if (!(s > 0.0f)) return false;
            new_worst = std::min(new_worst, s);
            if (turn_guard && i < old.size() && min_sin(cur, mesh.F[old[i]]) >= kEditFloorSin) {
                const P n0 = normal_of(cur(mesh.F[old[i]][0]), cur(mesh.F[old[i]][1]), cur(mesh.F[old[i]][2]));
                const P n1 = normal_of(pt(next[i][0]), pt(next[i][1]), pt(next[i][2]));
                const float d = n0[0]*n1[0] + n0[1]*n1[1] + n0[2]*n1[2];
                const float l = std::sqrt((n0[0]*n0[0] + n0[1]*n0[1] + n0[2]*n0[2]) *
                                          (n1[0]*n1[0] + n1[1]*n1[1] + n1[2]*n1[2]));
                if (d < kEditTurnCos * l) return false;
            }
        }
        if (strict ? !(new_worst > old_worst) : new_worst < std::min(old_worst, kEditFloorSin))
            return false;
        auto replaced = [&](int t) {
            return std::find(old.begin(), old.end(), t) != old.end() ||
                   std::find(gone.begin(), gone.end(), t) != gone.end();
        };
        // folds across every edge of a new face
        for (size_t i = 0; i < next.size(); ++i) {
            const P ni = normal_of(pt(next[i][0]), pt(next[i][1]), pt(next[i][2]));
            for (int k = 0; k < 3; ++k) {
                const int a = next[i][k], b = next[i][(k + 1) % 3];
                auto folded = [&](const Tri& g, auto&& gpt) {
                    const P ng = normal_of(gpt(g[0]), gpt(g[1]), gpt(g[2]));
                    float d = ni[0]*ng[0] + ni[1]*ng[1] + ni[2]*ng[2];
                    if (runs(g, a, b)) d = -d;
                    const float l = std::sqrt((ni[0]*ni[0] + ni[1]*ni[1] + ni[2]*ni[2]) *
                                              (ng[0]*ng[0] + ng[1]*ng[1] + ng[2]*ng[2]));
                    return d < -kFoldCos * l;
                };
                for (size_t j = 0; j < next.size(); ++j)
                    if (j != i && has(next[j], a) && has(next[j], b) && folded(next[j], pt))
                        return false;
                if (a < 0 || b < 0) continue;
                for (int t : vt[a])
                    if (alive[t] && !replaced(t) && has(mesh.F[t], b) && folded(mesh.F[t], pt))
                        return false;
            }
        }
        // crossings within one edge of the new faces' corners
        // Another block may be editing an outside vertex's faces: they are
        // skipped. A vertex next to this block cannot move during the phase.
        cx.ring.clear();
        for (const Tri& f : next)
            for (int w : f) {
                if (w < 0) continue;
                cx.ring.push_back(w);
                for (int t : vt[w])
                    if (alive[t]) for (int x : mesh.F[t]) cx.ring.push_back(x);
            }
        std::sort(cx.ring.begin(), cx.ring.end());
        cx.ring.erase(std::unique(cx.ring.begin(), cx.ring.end()), cx.ring.end());
        cx.ids.clear();
        for (int x : cx.ring)
            if (cx.inside(x))
                for (int t : vt[x]) if (alive[t] && !replaced(t)) cx.ids.push_back(t);
        std::sort(cx.ids.begin(), cx.ids.end());
        cx.ids.erase(std::unique(cx.ids.begin(), cx.ids.end()), cx.ids.end());
        cx.around.clear();
        for (int t : cx.ids) cx.around.push_back(mesh.F[t]);
        return nearly_flat(pt, next, cx.around) || !faces_cross(pt, next, cx.around);
    }

    // Collapses v into u, placed at p.
    bool collapse(Ctx& cx, int u, int v, const P& p, bool strict, bool turn_guard) {
        if (u == v || !cx.inside(u) || !cx.inside(v)) return false;
        for (int c : {u, v})
            for (int t : vt[c])
                if (alive[t])
                    for (int w : mesh.F[t]) if (!cx.inside(w)) return false;
        if (!link_ok(u, v)) return false;
        cx.next.clear(); cx.old.clear(); cx.gone.clear();
        for (int c : {u, v})
            for (int t : vt[c]) {
                if (!alive[t]) continue;
                const Tri& f = mesh.F[t];
                if (has(f, u) && has(f, v)) {
                    if (c == u) cx.gone.push_back(t);
                    continue;
                }
                Tri g = f;
                for (int& w : g) if (w == u || w == v) w = -1;
                cx.old.push_back(t);
                cx.next.push_back(g);
            }
        auto pt = [&](int w) -> const P& { return w < 0 ? p : mesh.V[w]; };
        if (!sound(cx, pt, strict, turn_guard)) return false;
        mesh.V[u] = p;
        for (const Tri& f : cx.next) for (int w : f) cx.touched.push_back(w < 0 ? u : w);
        for (int t : cx.gone) {
            alive[t] = 0;
            for (int w : mesh.F[t]) vt[w].erase(t);
        }
        for (int t : vt[v]) {
            if (!alive[t]) continue;
            for (int& w : mesh.F[t]) if (w == v) w = u;
            vt[u].insert(t);
        }
        vt[v].clear();
        ++cx.collapses;
        return true;
    }

    // Flips edge (a,b) to the far corners of its two faces.
    bool flip(Ctx& cx, int a, int b, bool strict) {
        int tt[2];
        if (!cx.inside(a) || !cx.inside(b) || faces_on(a, b, tt) != 2) return false;
        int t0 = tt[0], t1 = tt[1];
        if (!runs(mesh.F[t0], a, b)) std::swap(t0, t1);
        if (!runs(mesh.F[t0], a, b) || !runs(mesh.F[t1], b, a)) return false;
        int c = -1, d = -1;
        for (int w : mesh.F[t0]) if (w != a && w != b) c = w;
        for (int w : mesh.F[t1]) if (w != a && w != b) d = w;
        if (c < 0 || d < 0 || c == d || !cx.inside(c) || !cx.inside(d) || adjacent(c, d))
            return false;
        cx.next = {{c, d, b}, {d, c, a}};
        cx.old = {t0, t1};
        cx.gone.clear();
        auto pt = [&](int w) -> const P& { return mesh.V[w]; };
        if (!sound(cx, pt, strict, false)) return false;
        mesh.F[t0] = {c, d, b};
        mesh.F[t1] = {d, c, a};
        cx.touched.insert(cx.touched.end(), {a, b, c, d});
        vt[a].erase(t0);
        vt[b].erase(t1);
        vt[c].insert(t1);
        vt[d].insert(t0);
        ++cx.flips;
        return true;
    }

    // Drops dead faces and the vertices left without one.
    void compact() {
        std::vector<char> cut(mesh.F.size());
        for (size_t t = 0; t < mesh.F.size(); ++t) cut[t] = !alive[t];
        cut_faces(mesh, cut);
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Bad faces by local edits, worst first: a needle collapses its short edge, a
// cap flips its long edge or collapses its apex, a fold flips or collapses
// away, a flat valence-3 vertex collapses. angle_deg <= 0 disables the pass.
// ---------------------------------------------------------------------------
constexpr int kEditBlock = 1 << 14;   // vertices per block of the parallel edits

void fix_degenerate_faces(Mesh& mesh, float angle_deg, bool verbose, int num_threads) {
    if (angle_deg <= 0.0f || mesh.V.empty() || mesh.F.empty()) return;
#ifdef _OPENMP
    if (num_threads > 0) omp_set_num_threads(num_threads);
#else
    (void)num_threads;
#endif
    LocalEdit ed(mesh);
    using Ctx = LocalEdit::Ctx;
    auto& V = mesh.V;
    auto& F = mesh.F;
    const int nv = (int)V.size();
    const float sin_bad = std::sin(angle_deg * 0.017453292519943295f);
    auto cur = [&](int w) -> const LocalEdit::P& { return V[w]; };
    auto mid = [&](int a, int b) {
        return LocalEdit::P{0.5f*(V[a][0]+V[b][0]), 0.5f*(V[a][1]+V[b][1]), 0.5f*(V[a][2]+V[b][2])};
    };
    // Collapses edge (a,b) somewhere sound: onto a boundary end first, else mid.
    auto collapse_edge = [&](Ctx& c, int a, int b, bool strict, bool turn) {
        if (!c.inside(a) || !c.inside(b)) return false;
        const bool ba = ed.on_boundary(a), bb = ed.on_boundary(b);
        if (ba != bb) return ba ? ed.collapse(c, a, b, V[a], strict, turn)
                                : ed.collapse(c, b, a, V[b], strict, turn);
        return ed.collapse(c, a, b, mid(a, b), strict, turn) ||
               ed.collapse(c, a, b, V[a], strict, turn) || ed.collapse(c, b, a, V[b], strict, turn);
    };
    auto folded = [&](int t, int s) {
        const auto n0 = LocalEdit::normal_of(V[F[t][0]], V[F[t][1]], V[F[t][2]]);
        const auto n1 = LocalEdit::normal_of(V[F[s][0]], V[F[s][1]], V[F[s][2]]);
        const float l = std::sqrt((n0[0]*n0[0] + n0[1]*n0[1] + n0[2]*n0[2]) *
                                  (n1[0]*n1[0] + n1[1]*n1[1] + n1[2]*n1[2]));
        return n0[0]*n1[0] + n0[1]*n1[1] + n0[2]*n1[2] < -kFoldCos * l;
    };

    // One block's pass: degenerate faces worst first, then folds, then flat
    // valence-3 vertices.
    auto run_block = [&](Ctx& c, const std::vector<int>& faces, const std::vector<int>& verts) {
        std::vector<std::pair<float,int>> work;
        for (int t : faces) {
            if (!ed.alive[t]) continue;
            const float sn = LocalEdit::min_sin(cur, F[t]);
            if (sn < sin_bad) work.push_back({sn, t});
        }
        std::sort(work.begin(), work.end());
        for (const auto& w : work) {
            const int t = w.second;
            if (!ed.alive[t] || !(LocalEdit::min_sin(cur, F[t]) < sin_bad)) continue;
            const auto f = F[t];
            float l[3];
            for (int k = 0; k < 3; ++k) {
                const auto &p = V[f[(k + 1) % 3]], &q = V[f[(k + 2) % 3]];   // edge opposite corner k
                l[k] = (p[0]-q[0])*(p[0]-q[0]) + (p[1]-q[1])*(p[1]-q[1]) + (p[2]-q[2])*(p[2]-q[2]);
            }
            const int ks = l[0] <= l[1] && l[0] <= l[2] ? 0 : l[1] <= l[2] ? 1 : 2;
            const int kl = l[0] >= l[1] && l[0] >= l[2] ? 0 : l[1] >= l[2] ? 1 : 2;
            const int pa = f[kl], p0 = f[(kl + 1) % 3], p1 = f[(kl + 2) % 3];
            const auto e = LocalEdit::P{V[p1][0]-V[p0][0], V[p1][1]-V[p0][1], V[p1][2]-V[p0][2]};
            const float tp = l[kl] > 0.0f ? ((V[pa][0]-V[p0][0])*e[0] + (V[pa][1]-V[p0][1])*e[1] +
                                              (V[pa][2]-V[p0][2])*e[2]) / l[kl] : -1.0f;
            if (tp > 0.05f && tp < 0.95f) {   // cap
                if (ed.flip(c, p0, p1, true)) continue;
                const int near_end = tp < 0.5f ? p0 : p1, far_end = tp < 0.5f ? p1 : p0;
                if (ed.collapse(c, near_end, pa, V[near_end], true, true) ||
                    ed.collapse(c, far_end, pa, V[far_end], true, true)) continue;
            }
            if (collapse_edge(c, f[(ks + 1) % 3], f[(ks + 2) % 3], true, true)) continue;
            // a pinch blocks the collapse: flip a long edge instead
            for (int k = 0; k < 3; ++k)
                if (k != ks && ed.flip(c, f[(k + 1) % 3], f[(k + 2) % 3], true)) break;
        }
        for (int t : faces) {
            for (int k = 0; k < 3; ++k) {
                if (!ed.alive[t]) break;
                const int a = F[t][k], b = F[t][(k + 1) % 3];
                int tt[2];
                if (a > b || !c.inside(a) || ed.faces_on(a, b, tt) != 2) continue;
                const int s = tt[0] == t ? tt[1] : tt[0];
                if (!LocalEdit::runs(F[s], b, a) || !folded(t, s)) continue;
                int fc = -1, fd = -1;
                for (int w : F[t]) if (w != a && w != b) fc = w;
                for (int w : F[s]) if (w != a && w != b) fd = w;
                if (ed.flip(c, a, b, false)) continue;
                bool done = false;
                for (int x : {fc, fd})
                    for (int y : {a, b})
                        if (!done && ed.collapse(c, y, x, V[y], false, false)) done = true;
                if (!done) collapse_edge(c, a, b, false, false);
            }
        }
        for (int v : verts) {
            int live = 0, nb[3], q = 0;
            for (int t : ed.vt[v]) if (ed.alive[t]) ++live;
            if (live != 3 || ed.on_boundary(v)) continue;
            for (int t : ed.vt[v])
                if (ed.alive[t])
                    for (int w : F[t])
                        if (w != v && q < 3 && std::find(nb, nb + q, w) == nb + q) nb[q++] = w;
            if (q != 3) continue;
            float mean[3] = {0, 0, 0};
            std::array<float,3> n[3];
            int j = 0;
            for (int t : ed.vt[v]) {
                if (!ed.alive[t]) continue;
                n[j] = LocalEdit::normal_of(V[F[t][0]], V[F[t][1]], V[F[t][2]]);
                const float l = std::sqrt(n[j][0]*n[j][0] + n[j][1]*n[j][1] + n[j][2]*n[j][2]);
                for (int a = 0; a < 3; ++a) mean[a] += l > 0.0f ? n[j][a] / l : 0.0f;
                ++j;
            }
            const float lm = std::sqrt(mean[0]*mean[0] + mean[1]*mean[1] + mean[2]*mean[2]);
            bool flat = true;
            for (const auto& x : n)
                flat = flat && x[0]*mean[0] + x[1]*mean[1] + x[2]*mean[2] >
                       kRepairFlatCos * lm * std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
            if (!flat) continue;   // a tip, not a spare vertex
            for (int i = 0; i < 3; ++i)
                if (ed.collapse(c, nb[i], v, V[nb[i]], true, false)) break;
        }
    };

    // After the first round only faces near an edit can change their answer.
    const std::vector<int> rank = morton_rank(V);
    std::vector<int> block_of(nv);
    std::vector<char> near_edit(nv, 1);
    std::vector<int> touched;
    long collapses = 0, flips = 0;
    for (int round = 0; round < 6; ++round) {
        const long before = collapses + flips;
        for (int phase = 0; phase < 2; ++phase) {
            const int shift = phase ? kEditBlock / 2 : 0;
            for (int v = 0; v < nv; ++v) block_of[v] = (rank[v] + shift) / kEditBlock;
            const int nblocks = (nv + shift) / kEditBlock + 1;
            std::vector<std::vector<int>> bfaces(nblocks), bverts(nblocks);
            for (size_t t = 0; t < F.size(); ++t) {
                const auto& f = F[t];
                if (ed.alive[t] && (near_edit[f[0]] || near_edit[f[1]] || near_edit[f[2]]))
                    bfaces[block_of[f[0]]].push_back((int)t);
            }
            for (int v = 0; v < nv; ++v) if (near_edit[v]) bverts[block_of[v]].push_back(v);
            #pragma omp parallel for schedule(dynamic, 1)
            for (int bi = 0; bi < nblocks; ++bi) {
                Ctx c;
                c.block_of = &block_of;
                c.block = bi;
                run_block(c, bfaces[bi], bverts[bi]);
                #pragma omp critical
                {
                    touched.insert(touched.end(), c.touched.begin(), c.touched.end());
                    collapses += c.collapses;
                    flips += c.flips;
                }
            }
        }
        if (collapses + flips == before) break;
        std::fill(near_edit.begin(), near_edit.end(), 0);
        for (int v : touched) {
            near_edit[v] = 1;
            for (int t : ed.vt[v])
                if (ed.alive[t]) for (int w : F[t]) near_edit[w] = 1;
        }
        touched.clear();
    }
    if (collapses == 0 && flips == 0) {
        if (verbose) mlog::out(mlog::Stage::Cleanup, mmsg::degenerate_none);
        return;
    }
    ed.compact();
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::degenerate_fixed,
                  {collapses, flips, (long long)mesh.V.size(), (long long)mesh.F.size()});
}

// ---------------------------------------------------------------------------
// Quality: valence-improving edge flips, then tangential smoothing toward the
// 1-ring centroid (Botsch-Kobbelt). Both act only where the surface is nearly
// flat and are undone where they would make faces cross.
// ---------------------------------------------------------------------------
constexpr float kFlipFlatCos = 0.93969262f;     // quads within 20 deg
constexpr float kSmoothFlatCos = 0.86602540f;   // 1-rings within 30 deg

void improve_mesh_quality(Mesh& mesh, int iters, bool verbose,
                                 int num_threads) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (iters <= 0 || nv == 0 || nf == 0) return;
#ifdef _OPENMP
    if (num_threads > 0) omp_set_num_threads(num_threads);
#else
    (void)num_threads;
#endif
    auto& V = mesh.V;
    auto& F = mesh.F;
    using A3 = std::array<float,3>;
    auto sub = [](const A3& a, const A3& b) { return A3{a[0]-b[0], a[1]-b[1], a[2]-b[2]}; };
    auto cross = [](const A3& a, const A3& b) {
        return A3{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
    };
    auto dot = [](const A3& a, const A3& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
    auto nrm = [&](const A3& a) { return std::sqrt(dot(a, a)); };
    auto fnormal = [&](const std::array<int,3>& f) {
        return cross(sub(V[f[1]], V[f[0]]), sub(V[f[2]], V[f[0]]));
    };
    // cosine of a triangle's smallest angle
    auto min_angle_cos = [&](int a, int b, int c) -> float {
        auto d2 = [&](int p, int q) { auto e = sub(V[p], V[q]); return dot(e, e); };
        const float la = d2(b,c), lb = d2(c,a), lc = d2(a,b);
        auto co = [](float opp, float s1, float s2) {
            const float dn = 2.0f * std::sqrt(s1 * s2);
            return dn > 1e-30f ? (s1 + s2 - opp) / dn : 1.0f;
        };
        return std::max(co(la,lb,lc), std::max(co(lb,lc,la), co(lc,la,lb)));
    };
    const float kAngleFloorCos = std::cos(30.0f * 0.017453292519943295f);

    long total_flips = 0, total_moved = 0;
    std::vector<VertexSet> vt(nv);
    std::vector<int> val(nv);
    std::vector<char> is_bnd(nv);
    std::vector<std::array<int,3>> next, around;
    std::vector<int> ids;
    for (int pass = 0; pass < iters; ++pass) {
        for (auto& s : vt) s.clear();
        for (long t = 0; t < nf; ++t)
            for (int v : F[t]) vt[v].insert((int)t);
        auto faces_with = [&](int a, int b, int* out) {
            int n = 0;
            for (int t : vt[a]) {
                const auto& f = F[t];
                if (f[0] == b || f[1] == b || f[2] == b) { if (n < 2) out[n] = t; ++n; }
            }
            return n;
        };
        #pragma omp parallel for schedule(dynamic, 4096)
        for (int v = 0; v < nv; ++v) {
            int nb[64], cnt[64], m = 0;
            bool bnd = false, over = false;
            for (int t : vt[v])
                for (int w : F[t]) {
                    if (w == v) continue;
                    int j = 0;
                    while (j < m && nb[j] != w) ++j;
                    if (j == m) { if (m == 64) { over = true; continue; } nb[m] = w; cnt[m++] = 0; }
                    ++cnt[j];
                }
            for (int j = 0; j < m; ++j) bnd = bnd || cnt[j] == 1;
            val[v] = m;
            is_bnd[v] = bnd || over;
        }

        // ---- flips (serial; each edit is local) ----
        long flips = 0;
        auto ideal = [&](int v) { return is_bnd[v] ? 4 : 6; };
        for (long t0 = 0; t0 < nf; ++t0)
            for (int k = 0; k < 3; ++k) {
                const auto f0 = F[t0];
                const int aa = f0[k], bb = f0[(k + 1) % 3], c = f0[(k + 2) % 3];
                if (aa > bb) continue;   // each edge once, from the face running it upward
                int tt[2];
                if (faces_with(aa, bb, tt) != 2) continue;
                const int t1 = tt[0] == (int)t0 ? tt[1] : tt[0];
                const auto& f1 = F[t1];
                int d = -1, dir = 0;
                for (int j = 0; j < 3; ++j) {
                    if (f1[j] != aa && f1[j] != bb) d = f1[j];
                    if (f1[j] == bb && f1[(j + 1) % 3] == aa) dir = 1;
                }
                if (d < 0 || d == c || dir != 1) continue;   // wound against t0: leave it
                int dummy[2];
                if (faces_with(c, d, dummy) > 0) continue;        // (c,d) already an edge
                auto sq = [](int x) { return x * x; };
                const int before = sq(val[aa]-ideal(aa)) + sq(val[bb]-ideal(bb))
                                 + sq(val[c]-ideal(c)) + sq(val[d]-ideal(d));
                const int after  = sq(val[aa]-1-ideal(aa)) + sq(val[bb]-1-ideal(bb))
                                 + sq(val[c]+1-ideal(c)) + sq(val[d]+1-ideal(d));
                if (after >= before || val[aa] <= 3 || val[bb] <= 3) continue;
                // old (aa,bb,c), (bb,aa,d); new (c,d,bb), (d,c,aa)
                const A3 n0 = fnormal({aa, bb, c}), n1 = fnormal({bb, aa, d});
                const A3 m0 = fnormal({c, d, bb}), m1 = fnormal({d, c, aa});
                const float l0 = nrm(n0), l1 = nrm(n1), k0 = nrm(m0), k1 = nrm(m1);
                if (k0 < 1e-20f || k1 < 1e-20f) continue;
                if (dot(n0, n1) < kFlipFlatCos * l0 * l1 || dot(m0, m1) < kFlipFlatCos * k0 * k1 ||
                    dot(m0, n0) <= 0.0f || dot(m1, n0) <= 0.0f) continue;
                const float old_worst = std::max(min_angle_cos(aa,bb,c), min_angle_cos(bb,aa,d));
                const float new_worst = std::max(min_angle_cos(c,d,bb), min_angle_cos(d,c,aa));
                if (new_worst > kAngleFloorCos && new_worst >= old_worst) continue;
                next = {{c, d, bb}, {d, c, aa}};
                ids.clear();
                for (int x : {aa, bb, c, d})
                    for (int t : vt[x]) if (t != (int)t0 && t != t1) ids.push_back(t);
                std::sort(ids.begin(), ids.end());
                ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
                around.clear();
                for (int t : ids) around.push_back(F[t]);
                if (faces_cross([&](int w) -> const A3& { return V[w]; }, next, around)) continue;
                F[t0] = {c, d, bb};
                F[t1] = {d, c, aa};
                vt[aa].erase((int)t0);
                vt[bb].erase(t1);
                vt[c].insert(t1);
                vt[d].insert((int)t0);
                --val[aa]; --val[bb]; ++val[c]; ++val[d];
                ++flips;
            }

        // ---- tangential smoothing (Jacobi step) on nearly flat 1-rings ----
        std::vector<A3> cent(nv, {0, 0, 0}), vnorm(nv, {0, 0, 0});
        std::vector<float> wsum(nv, 0.0f);
        #pragma omp parallel for schedule(dynamic, 4096)
        for (int v = 0; v < nv; ++v)
            for (int t : vt[v]) {
                const auto& f = F[t];
                const A3 n = fnormal(f);
                const float area = 0.5f * nrm(n);
                for (int x = 0; x < 3; ++x) {
                    cent[v][x] += area * (V[f[0]][x] + V[f[1]][x] + V[f[2]][x]) / 3.0f;
                    vnorm[v][x] += n[x];
                }
                wsum[v] += area;
            }
        std::vector<A3> newV = V;
        #pragma omp parallel for schedule(dynamic, 4096)
        for (int v = 0; v < nv; ++v) {
            if (is_bnd[v] || wsum[v] <= 1e-30f) continue;
            const float ln = nrm(vnorm[v]);
            if (ln < 1e-20f) continue;
            const A3 n{vnorm[v][0]/ln, vnorm[v][1]/ln, vnorm[v][2]/ln};
            bool flat = true;
            for (int t : vt[v]) {
                const A3 fn = fnormal(F[t]);
                flat = flat && dot(fn, n) > kSmoothFlatCos * nrm(fn);
            }
            if (!flat) continue;
            const A3 d{cent[v][0]/wsum[v] - V[v][0], cent[v][1]/wsum[v] - V[v][1],
                       cent[v][2]/wsum[v] - V[v][2]};
            const float dn = dot(d, n);
            for (int x = 0; x < 3; ++x) newV[v][x] = V[v][x] + 0.5f * (d[x] - dn * n[x]);
        }
        // a vertex goes back when its move flips or flattens an incident face
        std::vector<char> back(nv, 0);
        #pragma omp parallel for schedule(static)
        for (long t = 0; t < nf; ++t) {
            const auto& f = F[t];
            const A3 no = fnormal(f);
            const A3 nn = cross(sub(newV[f[1]], newV[f[0]]), sub(newV[f[2]], newV[f[0]]));
            const float lo = nrm(no), lnn = nrm(nn);
            if (lnn < 1e-20f || (lo > 1e-20f && dot(no, nn) <= 0.0f))
                for (int v : f) back[v] = 1;
        }
        std::vector<int> moved;
        for (int v = 0; v < nv; ++v)
            if (!back[v] && newV[v] != V[v]) moved.push_back(v);
        std::vector<A3> oldV(moved.size());
        for (size_t i = 0; i < moved.size(); ++i) { oldV[i] = V[moved[i]]; V[moved[i]] = newV[moved[i]]; }
        // and when a face it touches now crosses another one
        std::vector<char> is_moved(nv, 0);
        for (int v : moved) is_moved[v] = 1;
        for (int round = 0; round < 4 && !moved.empty(); ++round) {
            std::vector<int> all(nf);
            for (long t = 0; t < nf; ++t) all[t] = (int)t;
            FaceBVH bvh;
            bvh.build(mesh, all);
            std::vector<char> undo(nv, 0);
            long hits = 0;
            #pragma omp parallel for schedule(dynamic, 1024) reduction(+:hits)
            for (long i = 0; i < nf; ++i) {
                const int ti = bvh.face[i];
                const auto& f = F[ti];
                if (!is_moved[f[0]] && !is_moved[f[1]] && !is_moved[f[2]]) continue;
                const A3* a[3] = {&V[f[0]], &V[f[1]], &V[f[2]]};
                bvh.visit(bvh.box[i], 0, [&](int j) {
                    if (bvh.face[j] == ti) return false;
                    const auto& g = F[bvh.face[j]];
                    const A3* b[3] = {&V[g[0]], &V[g[1]], &V[g[2]]};
                    if (triangles_cross(a, b) == 0) return false;
                    for (int v : f) if (is_moved[v]) undo[v] = 1;
                    for (int v : g) if (is_moved[v]) undo[v] = 1;
                    ++hits;
                    return false;
                });
            }
            if (hits == 0) break;
            for (size_t i = 0; i < moved.size(); ++i)
                if (undo[moved[i]]) { V[moved[i]] = oldV[i]; is_moved[moved[i]] = 0; }
        }
        long n_moved = 0;
        for (int v : moved) n_moved += is_moved[v];
        total_flips += flips;
        total_moved += n_moved;
        if (flips == 0 && n_moved == 0) break;
    }
    if (verbose)
        mlog::out(mlog::Stage::Quality, mmsg::quality_result,
                  {total_flips, total_moved, iters});
}

} // namespace meshing
