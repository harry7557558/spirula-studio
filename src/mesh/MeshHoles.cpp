// MeshHoles.cpp -- hole filling (MeshStages.h).

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
// Hole filling: small boundary loops get a minimum-weight (Liepa) patch wound
// like the faces around them; a patch crossing the mesh or itself is dropped.
// ---------------------------------------------------------------------------
// A loop this share of its part's size is its rim: filling would double the part.
constexpr float kFillMaxShare = 0.5f;
// A loop enclosing under this share of the area a circle of its length would
// is a slit (a 1:18 rectangle is at 0.16), and is closed whatever its length.
constexpr float kSlitArea = 0.15f;

// With `was_boundary` (per vertex, before faces were cut), a loop touching no
// such vertex is a cut's hole and is filled whatever its size.
void fill_holes(Mesh& mesh, float ratio, int max_edges, bool verbose,
                const std::vector<char>* was_boundary) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if ((ratio <= 0.0f && max_edges <= 0 && !was_boundary) || nv == 0 || nf == 0) return;
    auto& V = mesh.V;
    auto& F = mesh.F;
    const VertexFaces vf(mesh);

    // parts and their boxes
    std::vector<int> parent(nv);
    for (int i = 0; i < nv; ++i) parent[i] = i;
    auto find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    for (const auto& f : F) {
        int a = find(f[0]), b = find(f[1]), c = find(f[2]);
        if (a != b) parent[a] = b;
        b = find(b);
        if (b != c) parent[b] = c;
    }
    std::vector<std::array<float,6>> pbox(nv, {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f});
    for (int v = 0; v < nv; ++v) {
        if (vf.off[v] == vf.off[v + 1]) continue;
        auto& b = pbox[find(v)];
        for (int a = 0; a < 3; ++a) { b[a] = std::min(b[a], V[v][a]); b[3+a] = std::max(b[3+a], V[v][a]); }
    }
    auto diag = [](const std::array<float,6>& b) {
        const float dx = b[3]-b[0], dy = b[4]-b[1], dz = b[5]-b[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };

    // Boundary edges per vertex: up to two neighbours, and whether the face on
    // the edge runs v -> w. More than two marks the vertex unusable.
    std::vector<std::array<int,2>> bnb(nv, {-1, -1});
    std::vector<std::array<char,2>> bfwd(nv, {0, 0});
    std::vector<char> bbad(nv, 0);
    long n_bnd = 0;
    #pragma omp parallel for schedule(dynamic, 4096) reduction(+:n_bnd)
    for (int v = 0; v < nv; ++v) {
        int cnt = 0;
        for (int j = vf.off[v]; j < vf.off[v + 1]; ++j) {
            const auto& f = F[vf.inc[j]];
            const int k = f[0] == v ? 0 : f[1] == v ? 1 : 2;
            for (int s = 1; s <= 2; ++s) {
                const int w = f[(k + s) % 3];
                int m = 0;
                for (int i = vf.off[v]; i < vf.off[v + 1] && m < 2; ++i) {
                    const auto& g = F[vf.inc[i]];
                    m += g[0] == w || g[1] == w || g[2] == w;
                }
                if (m != 1) continue;
                if (cnt < 2) { bnb[v][cnt] = w; bfwd[v][cnt] = s == 1; }
                ++cnt;
            }
        }
        if (cnt > 2) bbad[v] = 1;
        n_bnd += cnt;
    }
    if (n_bnd == 0) return;

    FaceBVH bvh;
    {
        std::vector<int> all(nf);
        for (long t = 0; t < nf; ++t) all[t] = (int)t;
        bvh.build(mesh, all);
    }
    auto tri_ptrs = [&](const std::array<int,3>& f, const std::array<float,3>* p[3]) {
        for (int k = 0; k < 3; ++k) p[k] = &V[f[k]];
    };
    auto tri_box = [&](const std::array<int,3>& f) {
        Box3 b = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
        for (int k = 0; k < 3; ++k)
            for (int a = 0; a < 3; ++a) { b[a] = std::min(b[a], V[f[k]][a]); b[3+a] = std::max(b[3+a], V[f[k]][a]); }
        return b;
    };
    auto unit_normal = [&](const std::array<int,3>& f) {
        const auto &a = V[f[0]], &b = V[f[1]], &c = V[f[2]];
        const float e1[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]}, e2[3] = {c[0]-a[0], c[1]-a[1], c[2]-a[2]};
        std::array<float,3> n = {e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0]};
        const float l = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (l > 0.0f) for (float& x : n) x /= l;
        return n;
    };
    auto runs = [](const std::array<int,3>& f, int a, int b) {
        for (int k = 0; k < 3; ++k) if (f[k] == a && f[(k + 1) % 3] == b) return true;
        return false;
    };
    // A patch is kept only when no triangle of it crosses the mesh or another,
    // or folds onto a neighbour.
    auto patch_ok = [&](const std::vector<std::array<int,3>>& tris) {
        for (size_t i = 0; i < tris.size(); ++i) {
            const auto ni = unit_normal(tris[i]);
            for (int k = 0; k < 3; ++k) {
                const int a = tris[i][k], b = tris[i][(k + 1) % 3];
                auto folded = [&](const std::array<int,3>& g) {
                    const auto ng = unit_normal(g);
                    const float d = ni[0]*ng[0] + ni[1]*ng[1] + ni[2]*ng[2];
                    return (runs(g, a, b) ? -d : d) < -kFoldCos;
                };
                for (size_t j = 0; j < tris.size(); ++j)
                    if (j != i && runs(tris[j], b, a) && folded(tris[j])) return false;
                if (a < nv && b < nv)
                    for (int j = vf.off[a]; j < vf.off[a + 1]; ++j) {
                        const auto& g = F[vf.inc[j]];
                        if ((g[0] == b || g[1] == b || g[2] == b) && folded(g)) return false;
                    }
            }
        }
        std::vector<Box3> boxes(tris.size());
        for (size_t i = 0; i < tris.size(); ++i) {
            const std::array<float,3>* a[3];
            tri_ptrs(tris[i], a);
            boxes[i] = tri_box(tris[i]);
            if (bvh.visit(boxes[i], 0, [&](int j) {
                    const std::array<float,3>* b[3];
                    tri_ptrs(F[bvh.face[j]], b);
                    return triangles_cross(a, b) != 0;
                })) return false;
            for (size_t j = 0; j < i; ++j) {
                if (!boxes_overlap(boxes[i], boxes[j])) continue;
                const std::array<float,3>* b[3];
                tri_ptrs(tris[j], b);
                if (triangles_cross(a, b) != 0) return false;
            }
        }
        return true;
    };

    // triangle "thinness": (sum of squared edges) / (2 area); 3.46 when
    // equilateral, +inf when degenerate
    auto tri_weight = [&](int a, int b, int c) -> float {
        const auto& A = V[a]; const auto& B = V[b]; const auto& C = V[c];
        float ux=B[0]-A[0], uy=B[1]-A[1], uz=B[2]-A[2];
        float vx=C[0]-A[0], vy=C[1]-A[1], vz=C[2]-A[2];
        float wx=C[0]-B[0], wy=C[1]-B[1], wz=C[2]-B[2];
        float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        float area2 = std::sqrt(nx*nx+ny*ny+nz*nz);
        if (area2 < 1e-12f) return 1e30f;
        return ((ux*ux+uy*uy+uz*uz) + (vx*vx+vy*vy+vz*vz) + (wx*wx+wy*wy+wz*wz)) / area2;
    };

    // `pos[v]`: v's index in the sub-loop being triangulated, else -1.
    std::vector<int> pos(nv, -1);
    std::vector<std::pair<int,int>> chords;   // edges the current patch added
    std::vector<std::array<int,3>> patch;
    std::vector<char> blk;
    std::vector<float> W;
    std::vector<int> O, st;
    std::vector<std::array<float,3>> NT, ext;
    const int kDpMax = 64;   // the table is cubic: longer loops are split at a chord first
    auto mark_blocked = [&](const std::vector<int>& L) {
        const int n = (int)L.size();
        blk.assign((size_t)n * n, 0);
        for (int i = 0; i < n; ++i) pos[L[i]] = i;
        for (int i = 0; i < n; ++i) {
            const int v = L[i];
            if (v >= nv) continue;
            for (int j = vf.off[v]; j < vf.off[v + 1]; ++j)
                for (int w : F[vf.inc[j]])
                    if (w < nv && pos[w] >= 0 && w != v) blk[(size_t)i * n + pos[w]] = 1;
        }
        for (const auto& c : chords)
            if (c.first < nv && c.second < nv && pos[c.first] >= 0 && pos[c.second] >= 0) {
                blk[(size_t)pos[c.first] * n + pos[c.second]] = 1;
                blk[(size_t)pos[c.second] * n + pos[c.first]] = 1;
            }
        for (int i = 0; i < n; ++i) pos[L[i]] = -1;
    };
    auto dp_fill = [&](const std::vector<int>& L) -> bool {
        const int n = (int)L.size();
        mark_blocked(L);
        auto blocked = [&](int p, int q) {
            if (q == p + 1 || (p == 0 && q == n - 1)) return false;   // loop edge
            return blk[(size_t)p * n + q] != 0;
        };
        // Folds are ruled out as the table fills (Liepa weighs dihedrals): each
        // triangle against its sub-solutions' and the faces outside loop edges.
        ext.assign(n, {0.0f, 0.0f, 0.0f});
        for (int k = 0; k < n; ++k) {
            const int a = L[k], b = L[(k + 1) % n];
            if (a >= nv || b >= nv) continue;
            for (int j = vf.off[a]; j < vf.off[a + 1]; ++j) {
                const auto& g = F[vf.inc[j]];
                if (g[0] != b && g[1] != b && g[2] != b) continue;
                ext[k] = unit_normal(g);
                if (runs(g, a, b)) for (float& x : ext[k]) x = -x;
                break;
            }
        }
        auto fold = [](const std::array<float,3>& p, const std::array<float,3>& q) {
            return p[0]*q[0] + p[1]*q[1] + p[2]*q[2] < -kFoldCos;
        };
        W.assign((size_t)n * n, 0.0f);
        O.assign((size_t)n * n, 0);
        NT.assign((size_t)n * n, {0.0f, 0.0f, 0.0f});
        for (int gap = 2; gap < n; ++gap)
            for (int i = 0; i + gap < n; ++i) {
                const int j = i + gap;
                float best = 1e30f; int bo = i + 1;
                if (!blocked(i, j))
                    for (int m = i + 1; m < j; ++m) {
                        if (blocked(i, m) || blocked(m, j)) continue;
                        const float c = W[(size_t)i*n+m] + W[(size_t)m*n+j] + tri_weight(L[i], L[m], L[j]);
                        if (!(c < best)) continue;
                        const auto tn = unit_normal({L[i], L[m], L[j]});
                        if (fold(tn, m == i + 1 ? ext[i] : NT[(size_t)i*n+m]) ||
                            fold(tn, m + 1 == j ? ext[m] : NT[(size_t)m*n+j]) ||
                            (i == 0 && j == n - 1 && fold(tn, ext[n - 1]))) continue;
                        best = c; bo = m;
                    }
                W[(size_t)i*n+j] = best; O[(size_t)i*n+j] = bo;
                if (best < 1e29f) NT[(size_t)i*n+j] = unit_normal({L[i], L[bo], L[j]});
            }
        if (W[(size_t)n - 1] >= 1e29f) return false;
        st.clear(); st.push_back(0); st.push_back(n - 1);
        while (!st.empty()) {
            const int j = st.back(); st.pop_back();
            const int i = st.back(); st.pop_back();
            if (j <= i + 1) continue;
            const int m = O[(size_t)i*n+j];
            patch.push_back({L[i], L[m], L[j]});
            chords.push_back({L[i], L[j]});
            st.push_back(i); st.push_back(m);
            st.push_back(m); st.push_back(j);
        }
        return true;
    };

    // When the whole patch is refused: ears, best-shaped first, each kept only
    // if it neither folds nor crosses anything. What is left stays open.
    std::vector<std::array<float,3>> en;
    std::vector<Box3> ear_box;
    std::vector<int> prv, nxt, stamp;
    auto clip_ears = [&](const std::vector<int>& L) {
        const int n0 = (int)L.size();
        std::vector<std::array<int,3>> ears;
        ear_box.clear();
        // en[k]: the normal across the edge from k to nxt[k], as that face winds it
        en.assign(n0, {0.0f, 0.0f, 0.0f});
        prv.resize(n0); nxt.resize(n0); stamp.assign(n0, 0);
        for (int k = 0; k < n0; ++k) {
            prv[k] = (k + n0 - 1) % n0; nxt[k] = (k + 1) % n0;
            const int a = L[k], b = L[nxt[k]];
            for (int j = vf.off[a]; j < vf.off[a + 1]; ++j) {
                const auto& g = F[vf.inc[j]];
                if (g[0] != b && g[1] != b && g[2] != b) continue;
                en[k] = unit_normal(g);
                if (runs(g, a, b)) for (float& x : en[k]) x = -x;
                break;
            }
        }
        auto dot = [](const std::array<float,3>& p, const std::array<float,3>& q) {
            return p[0]*q[0] + p[1]*q[1] + p[2]*q[2];
        };
        int left = n0;
        // true when the ear at i (its neighbours' closing edge) may be added now
        auto valid = [&](int i) {
            const int a = L[prv[i]], b = L[i], c = L[nxt[i]];
            if (left > 3) {   // the closing edge may not exist already
                for (int j = vf.off[a]; j < vf.off[a + 1]; ++j)
                    for (int x : F[vf.inc[j]]) if (x == c) return false;
                for (const auto& e : ears)
                    if ((e[0] == a || e[1] == a || e[2] == a) && (e[0] == c || e[1] == c || e[2] == c))
                        return false;
            }
            const std::array<int,3> t = {a, b, c};
            const auto tn = unit_normal(t);
            if (dot(tn, en[prv[i]]) < -kFoldCos || dot(tn, en[i]) < -kFoldCos) return false;
            const std::array<float,3>* pa[3];
            tri_ptrs(t, pa);
            const Box3 tb = tri_box(t);
            if (bvh.visit(tb, 0, [&](int j) {
                    const std::array<float,3>* pb[3];
                    tri_ptrs(F[bvh.face[j]], pb);
                    return triangles_cross(pa, pb) != 0;
                })) return false;
            for (size_t j = 0; j < ears.size(); ++j) {
                if (!boxes_overlap(tb, ear_box[j])) continue;
                const std::array<float,3>* pb[3];
                tri_ptrs(ears[j], pb);
                if (triangles_cross(pa, pb) != 0) return false;
            }
            return true;
        };
        // (weight, vertex, its stamp when queued): an ear is tested when it comes up
        using Cand = std::pair<float, std::pair<int,int>>;
        std::priority_queue<Cand, std::vector<Cand>, std::greater<Cand>> queue;
        auto push = [&](int i) {
            const float w = tri_weight(L[prv[i]], L[i], L[nxt[i]]);
            if (w < 1e29f) queue.push({w, {i, stamp[i]}});
        };
        for (int i = 0; i < n0; ++i) push(i);
        while (left >= 3 && !queue.empty()) {
            const int i = queue.top().second.first, st0 = queue.top().second.second;
            queue.pop();
            if (stamp[i] != st0 || !valid(i)) continue;
            const std::array<int,3> t = {L[prv[i]], L[i], L[nxt[i]]};
            ears.push_back(t);
            ear_box.push_back(tri_box(t));
            const int p = prv[i], q = nxt[i];
            en[p] = unit_normal(t);
            nxt[p] = q; prv[q] = p;
            stamp[i] = -1;
            --left;
            ++stamp[p]; ++stamp[q];
            if (left >= 3) { push(p); push(q); }
        }
        for (const auto& t : ears) F.push_back(t);
        return (long)ears.size();
    };

    long n_filled = 0, n_tris = 0;
    std::vector<std::vector<int>> work;
    // Fills loop L0, wound so each patch triangle runs its loop edge forward.
    auto fill_loop = [&](const std::vector<int>& L0) {
        const int n0 = (int)L0.size();
        float bb[6] = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
        for (int v : L0)
            for (int a = 0; a < 3; ++a) { bb[a] = std::min(bb[a], V[v][a]); bb[3+a] = std::max(bb[3+a], V[v][a]); }
        const float d = std::sqrt((bb[3]-bb[0])*(bb[3]-bb[0]) + (bb[4]-bb[1])*(bb[4]-bb[1]) +
                                  (bb[5]-bb[2])*(bb[5]-bb[2]));
        const float pd = diag(pbox[find(L0[0])]);
        const bool ok_ratio = ratio > 0.0f && d < ratio * pd;
        const bool ok_edges = max_edges > 0 && n0 <= max_edges && d < kFillMaxShare * pd;
        bool cut_hole = was_boundary != nullptr && d < kFillMaxShare * pd;
        for (int v : L0) cut_hole = cut_hole && !(*was_boundary)[v];
        double area[3] = {0, 0, 0}, length = 0;
        for (int i = 0; i < n0; ++i) {
            const auto &p = V[L0[i]], &q = V[L0[(i + 1) % n0]];
            area[0] += 0.5 * ((double)p[1]*q[2] - (double)p[2]*q[1]);
            area[1] += 0.5 * ((double)p[2]*q[0] - (double)p[0]*q[2]);
            area[2] += 0.5 * ((double)p[0]*q[1] - (double)p[1]*q[0]);
            length += std::sqrt((double)(p[0]-q[0])*(p[0]-q[0]) + (double)(p[1]-q[1])*(p[1]-q[1]) +
                                (double)(p[2]-q[2])*(p[2]-q[2]));
        }
        const double a = std::sqrt(area[0]*area[0] + area[1]*area[1] + area[2]*area[2]);
        const bool slit = d < kFillMaxShare * pd &&
                          4.0 * 3.14159265358979 * a < kSlitArea * length * length;
        if (!ok_ratio && !ok_edges && !cut_hole && !slit) return;

        patch.clear();
        chords.clear();
        const size_t nv_before = V.size();
        work.clear();
        work.push_back(L0);
        while (!work.empty()) {
            std::vector<int> cur = std::move(work.back());
            work.pop_back();
            const int n = (int)cur.size();
            if (n < 3) continue;
            if (n == 3) { patch.push_back({cur[0], cur[1], cur[2]}); continue; }
            if (n <= kDpMax && dp_fill(cur)) continue;
            // split at the shortest chord leaving each side >= 1/5 of the loop
            mark_blocked(cur);
            const int lo = std::max(2, n / 5), hi = std::min(n - 2, (4 * n) / 5);
            int bi = -1, bj = -1; float blen = 1e30f;
            for (int i = 0; i < n; ++i)
                for (int g = lo; g <= hi; ++g) {
                    const int j = i + g;
                    if (j >= n) break;
                    if (blk[(size_t)i * n + j]) continue;
                    float l = 0;
                    for (int a = 0; a < 3; ++a) l += (V[cur[i]][a]-V[cur[j]][a]) * (V[cur[i]][a]-V[cur[j]][a]);
                    if (l < blen) { blen = l; bi = i; bj = j; }
                }
            if (bi < 0) {   // no chord: a fan around the centroid
                std::array<float,3> c = {0, 0, 0};
                for (int v : cur) for (int a = 0; a < 3; ++a) c[a] += V[v][a] / (float)n;
                const int ci = (int)V.size();
                V.push_back(c);
                for (int i = 0; i < n; ++i) patch.push_back({ci, cur[i], cur[(i + 1) % n]});
                continue;
            }
            chords.push_back({cur[bi], cur[bj]});
            std::vector<int> A(cur.begin() + bi, cur.begin() + bj + 1);
            std::vector<int> B(cur.begin() + bj, cur.end());
            B.insert(B.end(), cur.begin(), cur.begin() + bi + 1);
            work.push_back(std::move(A));
            work.push_back(std::move(B));
        }
        if (patch.empty() || !patch_ok(patch)) {
            V.resize(nv_before);
            n_tris += clip_ears(L0);
            return;
        }
        for (const auto& t : patch) F.push_back(t);
        n_tris += (long)patch.size();
        ++n_filled;
    };

    std::vector<char> seen(nv, 0);
    std::vector<int> loop;
    for (int s = 0; s < nv; ++s) {
        if (seen[s] || bbad[s] || bnb[s][1] < 0) continue;
        loop.clear();
        loop.push_back(s);
        seen[s] = 1;
        int prev = s, cur = bnb[s][0];
        bool ok = true;
        while (cur != s) {
            if (bbad[cur] || bnb[cur][1] < 0 || seen[cur] || (int)loop.size() > nv) { ok = false; break; }
            seen[cur] = 1;
            loop.push_back(cur);
            const int nxt = bnb[cur][0] == prev ? bnb[cur][1] : bnb[cur][0];
            prev = cur;
            cur = nxt;
        }
        if (!ok || loop.size() < 3) continue;
        // wind against the faces on the loop: they must run each edge backward
        int fwd = 0;
        for (size_t i = 0; i < loop.size(); ++i) {
            const int a = loop[i], b = loop[(i + 1) % loop.size()];
            fwd += (bnb[a][0] == b ? bfwd[a][0] : bfwd[a][1]) ? 1 : -1;
        }
        if (fwd > 0) std::reverse(loop.begin(), loop.end());
        fill_loop(loop);
    }
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::holes_filled,
                  {n_filled, (double)ratio, max_edges, n_tris,
                   (long long)mesh.V.size(), (long long)mesh.F.size()});
}

} // namespace meshing
