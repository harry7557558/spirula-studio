// MeshCheck.cpp -- see MeshCheck.h. Topology is one pass over a vertex ->
// face CSR; intersections test face pairs found through a BVH over the faces
// in Morton order, with vertices compared by position, not index.

#include "mesh/MeshCheck.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>
#include <utility>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

long MeshCheckReport::defects() const {
    return bad_index + repeated_index + nonfinite + unused_vertices + duplicate_vertices +
           duplicate_edges + duplicate_faces + nonmanifold_edges + nonmanifold_vertices +
           misoriented_edges + inside_out_parts + small_parts + zero_area + folds +
           intersecting_pairs + small_angles + large_aspect + low_valence + high_valence;
}

namespace {

using P3 = std::array<double, 3>;

inline P3 sub(const P3& a, const P3& b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
inline P3 cross(const P3& a, const P3& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
inline double dot(const P3& a, const P3& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
inline double orient(const P3& a, const P3& b, const P3& c, const P3& d) {
    return dot(sub(b, a), cross(sub(c, a), sub(d, a)));
}
inline int sgn(double x, double tol) { return x > tol ? 1 : x < -tol ? -1 : 0; }

struct UnionFind {
    std::vector<int> p;
    explicit UnionFind(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) {
        while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; }
        return x;
    }
    void unite(int a, int b) { a = find(a); b = find(b); if (a != b) p[a] = b; }
};

// Position classes: vertices at bit-identical positions (with -0 == 0) share one id.
std::vector<int> position_ids(const std::vector<std::array<float,3>>& V,
                              const std::vector<char>& used, long& duplicates) {
    const int nv = (int)V.size();
    std::vector<int> pid(nv);
    std::iota(pid.begin(), pid.end(), 0);
    size_t cap = 16;
    while (cap < 2 * (size_t)nv) cap <<= 1;
    std::vector<int> table(cap, -1);
    auto bits = [](float f) { uint32_t u; std::memcpy(&u, &f, 4); return f == 0.0f ? 0u : u; };
    duplicates = 0;
    for (int v = 0; v < nv; ++v) {
        if (!used[v]) continue;
        uint64_t h = bits(V[v][0]) * 0x9E3779B97F4A7C15ull;
        h ^= bits(V[v][1]) * 0xC2B2AE3D27D4EB4Full;
        h ^= bits(V[v][2]) * 0x165667B19E3779F9ull;
        h ^= h >> 29;
        for (size_t i = h & (cap - 1);; i = (i + 1) & (cap - 1)) {
            const int u = table[i];
            if (u < 0) { table[i] = v; break; }
            if (V[u] == V[v]) { pid[v] = u; ++duplicates; break; }
        }
    }
    return pid;
}

double cross2(const double* o, const double* a, const double* b) {
    return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0]);
}
bool wedge_has(const double* u1, const double* u2, const double* x, double tol) {
    const double o[2] = {0.0, 0.0};
    const double sg = cross2(o, u1, u2) > 0 ? 1.0 : -1.0;
    return cross2(o, u1, x) * sg > tol && cross2(o, x, u2) * sg > tol;
}
bool same_ray(const double* u, const double* w, double tol) {
    return std::fabs(u[0]*w[1] - u[1]*w[0]) <= tol && u[0]*w[0] + u[1]*w[1] > 0;
}

// Coplanar faces, projected along the normal's dominant axis; with one
// shared corner (a[0] == b[0]) they overlap when their wedges there do.
bool coplanar_overlap(const P3* a, const P3* b, int shared, const P3& n, double tol2) {
    int ax = 0;
    if (std::fabs(n[1]) > std::fabs(n[ax])) ax = 1;
    if (std::fabs(n[2]) > std::fabs(n[ax])) ax = 2;
    const int i0 = ax == 0 ? 1 : 0, i1 = ax == 2 ? 1 : 2;
    double p[3][2], q[3][2];
    for (int k = 0; k < 3; ++k) {
        p[k][0] = a[k][i0]; p[k][1] = a[k][i1];
        q[k][0] = b[k][i0]; q[k][1] = b[k][i1];
    }
    if (shared == 1) {
        const double u1[2] = {p[1][0]-p[0][0], p[1][1]-p[0][1]}, u2[2] = {p[2][0]-p[0][0], p[2][1]-p[0][1]};
        const double w1[2] = {q[1][0]-q[0][0], q[1][1]-q[0][1]}, w2[2] = {q[2][0]-q[0][0], q[2][1]-q[0][1]};
        if (wedge_has(u1, u2, w1, tol2) || wedge_has(u1, u2, w2, tol2) ||
            wedge_has(w1, w2, u1, tol2) || wedge_has(w1, w2, u2, tol2)) return true;
        return (same_ray(u1, w1, tol2) && same_ray(u2, w2, tol2)) ||
               (same_ray(u1, w2, tol2) && same_ray(u2, w1, tol2));
    }
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const double* a0 = p[i]; const double* a1 = p[(i+1)%3];
            const double* b0 = q[j]; const double* b1 = q[(j+1)%3];
            const int s0 = sgn(cross2(a0, a1, b0), tol2), s1 = sgn(cross2(a0, a1, b1), tol2);
            const int s2 = sgn(cross2(b0, b1, a0), tol2), s3 = sgn(cross2(b0, b1, a1), tol2);
            if (s0 * s1 < 0 && s2 * s3 < 0) return true;
        }
    auto inside = [&](double (*t)[2], const double* x) {
        const int s0 = sgn(cross2(t[0], t[1], x), tol2), s1 = sgn(cross2(t[1], t[2], x), tol2),
                  s2 = sgn(cross2(t[2], t[0], x), tol2);
        return s0 != 0 && s0 == s1 && s1 == s2;
    };
    for (int k = 0; k < 3; ++k)
        if (inside(q, p[k]) || inside(p, q[k])) return true;
    return false;
}

// Segment pq strictly through the interior of triangle t.
bool segment_crosses(const P3& p, const P3& q, const P3* t, double tol3) {
    const int s0 = sgn(orient(p, q, t[0], t[1]), tol3);
    const int s1 = sgn(orient(p, q, t[1], t[2]), tol3);
    const int s2 = sgn(orient(p, q, t[2], t[0]), tol3);
    return s0 != 0 && s0 == s1 && s1 == s2;
}

}  // namespace

// Orientations are exact enough in double for float input that the relative
// tolerance only absorbs touching.
int triangles_cross(const std::array<float,3>* const ta[3],
                    const std::array<float,3>* const tb[3]) {
    const std::array<float,3>* A[3] = {ta[0], ta[1], ta[2]};
    const std::array<float,3>* B[3] = {tb[0], tb[1], tb[2]};
    int shared = 0;   // rotate the shared corners to the front, in matching order
    for (int i = 0; i < 3 && shared < 3; ++i)
        for (int j = shared; j < 3; ++j)
            if (*B[j] == *A[i]) {
                std::swap(A[i], A[shared]);
                std::swap(B[j], B[shared]);
                ++shared;
                break;
            }
    if (shared == 3) return 2;
    P3 a[3], b[3];
    for (int k = 0; k < 3; ++k) {
        a[k] = {(*A[k])[0], (*A[k])[1], (*A[k])[2]};
        b[k] = {(*B[k])[0], (*B[k])[1], (*B[k])[2]};
    }
    double L = 0.0;
    for (int k = 0; k < 3; ++k)
        for (int m = 0; m < 3; ++m) {
            const P3 d = sub(a[k], b[m]);
            L = std::max(L, dot(d, d));
        }
    L = std::sqrt(L);
    const double tol3 = 1e-10 * L * L * L, tol2 = 1e-10 * L * L;
    if (shared == 2) {   // a fold: far corners on one side of the edge, in one plane
        const P3 e = sub(a[1], a[0]);
        if (sgn(orient(a[0], a[1], a[2], b[2]), tol3) != 0) return 0;
        return dot(cross(e, sub(a[2], a[0])), cross(e, sub(b[2], a[0]))) > 0 ? 1 : 0;
    }
    int db[3], da[3];
    for (int k = 0; k < 3; ++k) {
        db[k] = k < shared ? 0 : sgn(orient(a[0], a[1], a[2], b[k]), tol3);
        da[k] = k < shared ? 0 : sgn(orient(b[0], b[1], b[2], a[k]), tol3);
    }
    if ((db[0] == db[1] && db[1] == db[2] && db[0] != 0) ||
        (da[0] == da[1] && da[1] == da[2] && da[0] != 0)) return 0;
    if (db[0] == 0 && db[1] == 0 && db[2] == 0 && da[0] == 0 && da[1] == 0 && da[2] == 0) {
        const P3 na = cross(sub(a[1], a[0]), sub(a[2], a[0]));
        return coplanar_overlap(a, b, shared, na, tol2) ? 1 : 0;
    }
    for (int k = 0; k < 3; ++k) {
        const int k1 = (k + 1) % 3;
        if (db[k] * db[k1] < 0 && segment_crosses(b[k], b[k1], a, tol3)) return 1;
        if (da[k] * da[k1] < 0 && segment_crosses(a[k], a[k1], b, tol3)) return 1;
    }
    return 0;
}

Box3 face_box(const MeshData& mesh, int t) {
    Box3 b = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
    for (int k = 0; k < 3; ++k)
        for (int a = 0; a < 3; ++a) {
            const float x = mesh.V[mesh.F[t][k]][a];
            b[a] = std::min(b[a], x);
            b[3 + a] = std::max(b[3 + a], x);
        }
    return b;
}

void FaceBVH::build(const MeshData& mesh, const std::vector<int>& faces) {
    const auto& V = mesh.V;
    const auto& F = mesh.F;
    const int n = (int)faces.size();
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (int t : faces)
        for (int k = 0; k < 3; ++k)
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(lo[a], V[F[t][k]][a]);
                hi[a] = std::max(hi[a], V[F[t][k]][a]);
            }
    std::vector<std::pair<uint64_t,int>> code(n);
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        const auto& f = F[faces[i]];
        uint64_t q[3];
        for (int a = 0; a < 3; ++a) {
            const float c = (V[f[0]][a] + V[f[1]][a] + V[f[2]][a]) / 3.0f;
            const double x = (c - lo[a]) / std::max(hi[a] - lo[a], 1e-30f);
            q[a] = (uint64_t)std::min(2097151.0, std::max(0.0, x * 2097151.0));
        }
        uint64_t m = 0;
        for (int bit = 20; bit >= 0; --bit)
            for (int a = 0; a < 3; ++a) m = (m << 1) | ((q[a] >> bit) & 1u);
        code[i] = {m, faces[i]};
    }
    std::sort(code.begin(), code.end());
    face.resize(n);
    box.resize(n);
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        face[i] = code[i].second;
        box[i] = face_box(mesh, face[i]);
    }
    leaves = 1;
    while (leaves * kLeaf < n) leaves <<= 1;
    node.assign(2 * (size_t)leaves, {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f});
    auto grow = [](Box3& b, const Box3& c) {
        for (int a = 0; a < 3; ++a) { b[a] = std::min(b[a], c[a]); b[3+a] = std::max(b[3+a], c[3+a]); }
    };
    for (int i = 0; i < n; ++i) grow(node[leaves + i / kLeaf], box[i]);
    for (int i = leaves - 1; i >= 1; --i) { node[i] = node[2*i]; grow(node[i], node[2*i+1]); }
}

// A valence-3 vertex whose faces turn less than this is flat enough to drop.
constexpr double kFlatCos = 0.8660254037844387;   // cos 30 deg

MeshCheckReport check_mesh(const MeshData& mesh, const MeshCheckLimits& lim) {
    const auto t0 = std::chrono::steady_clock::now();
#ifdef _OPENMP
    if (lim.num_threads > 0) omp_set_num_threads(lim.num_threads);
#endif
    MeshCheckReport r;
    const auto& V = mesh.V;
    const auto& F = mesh.F;
    const int nv = (int)V.size();
    const long nf = (long)F.size();
    r.vertices = nv;
    r.faces = nf;

    // ---- indexing ----
    std::vector<char> fok(nf, 1), used(nv, 0), finite(nv, 1);
    for (int v = 0; v < nv; ++v)
        if (!std::isfinite(V[v][0]) || !std::isfinite(V[v][1]) || !std::isfinite(V[v][2])) {
            finite[v] = 0;
            ++r.nonfinite;
        }
    for (long t = 0; t < nf; ++t) {
        const auto& f = F[t];
        bool in = true;
        for (int k = 0; k < 3; ++k) in = in && f[k] >= 0 && f[k] < nv;
        if (!in) { fok[t] = 0; ++r.bad_index; continue; }
        if (f[0] == f[1] || f[1] == f[2] || f[0] == f[2]) { fok[t] = 0; ++r.repeated_index; }
        for (int k = 0; k < 3; ++k) used[f[k]] = 1;
        if (fok[t] && !(finite[f[0]] && finite[f[1]] && finite[f[2]])) fok[t] = 0;
    }
    for (int v = 0; v < nv; ++v) if (!used[v]) ++r.unused_vertices;
    std::vector<int> pid = position_ids(V, used, r.duplicate_vertices);
    std::vector<char> has_dup(nv, 0);
    for (int v = 0; v < nv; ++v) if (pid[v] != v) has_dup[v] = has_dup[pid[v]] = 1;
    for (int v = 0; v < nv; ++v) has_dup[v] = has_dup[pid[v]];

    // ---- vertex -> faces ----
    std::vector<int> off(nv + 1, 0);
    for (long t = 0; t < nf; ++t)
        if (fok[t]) for (int k = 0; k < 3; ++k) ++off[F[t][k] + 1];
    for (int v = 0; v < nv; ++v) off[v + 1] += off[v];
    std::vector<int> inc(off[nv]);
    {
        std::vector<int> fill(off.begin(), off.end() - 1);
        for (long t = 0; t < nf; ++t)
            if (fok[t]) for (int k = 0; k < 3; ++k) inc[fill[F[t][k]]++] = (int)t;
    }

    // ---- per vertex: edges, fans, valence ----
    const double cos_fold = std::cos(lim.fold_deg * 0.017453292519943295);
    std::vector<char> vflag(nv, 0);   // 1: on a boundary edge, 2: on a bad edge
    std::vector<std::pair<int,int>> boundary_pairs, dup_edge_keys;
    long n_bnd = 0, n_nm = 0, n_mis = 0, n_fold = 0, n_nmv = 0, n_low = 0, n_high = 0;
    int max_val = 0;
    #pragma omp parallel reduction(+:n_bnd,n_nm,n_mis,n_fold,n_nmv,n_low,n_high)
    {
        int my_max_val = 0;
        struct Nb { int w, cnt, fw, bw, f0, f1; };
        std::vector<Nb> nb;
        std::vector<int> uf;
        std::vector<std::pair<int,int>> my_bnd, my_dup;
        auto ufind = [&](int x) { while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; };
        auto face_normal = [&](int t) {
            const auto& f = F[t];
            P3 a = {V[f[0]][0], V[f[0]][1], V[f[0]][2]};
            P3 b = {V[f[1]][0], V[f[1]][1], V[f[1]][2]};
            P3 c = {V[f[2]][0], V[f[2]][1], V[f[2]][2]};
            return cross(sub(b, a), sub(c, a));
        };
        #pragma omp for schedule(dynamic, 4096)
        for (int v = 0; v < nv; ++v) {
            const int b0 = off[v], m = off[v + 1] - b0;
            if (m == 0) continue;
            nb.clear();
            uf.resize(m);
            std::iota(uf.begin(), uf.end(), 0);
            auto add = [&](int w, bool fwd, int lf) {
                for (auto& e : nb)
                    if (e.w == w) {
                        ++e.cnt;
                        (fwd ? e.fw : e.bw) += 1;
                        if (e.cnt == 2) e.f1 = lf;
                        const int x = ufind(lf), y = ufind(e.f0);
                        if (x != y) uf[x] = y;
                        return;
                    }
                nb.push_back({w, 1, fwd ? 1 : 0, fwd ? 0 : 1, lf, -1});
            };
            for (int j = 0; j < m; ++j) {
                const auto& f = F[inc[b0 + j]];
                const int k = f[0] == v ? 0 : f[1] == v ? 1 : 2;
                add(f[(k + 1) % 3], true, j);
                add(f[(k + 2) % 3], false, j);
            }
            int fans = 0;
            for (int j = 0; j < m; ++j) fans += ufind(j) == j;
            bool boundary = false, bad = false;
            for (const auto& e : nb) {
                boundary = boundary || e.cnt == 1;
                bad = bad || e.cnt > 2 || (e.cnt == 2 && (e.fw == 2 || e.bw == 2));
            }
            if (fans > 1) ++n_nmv;
            const int val = (int)nb.size();
            my_max_val = std::max(my_max_val, val);
            if (!boundary && !bad && fans == 1 && val == 3) {   // flat: a removable vertex
                P3 n[3], mean = {0, 0, 0};
                bool flat = true;
                for (int j = 0; j < 3; ++j) {
                    n[j] = face_normal(inc[b0 + j]);
                    const double l = std::sqrt(dot(n[j], n[j]));
                    for (int a = 0; a < 3; ++a) mean[a] += l > 0 ? n[j][a] / l : 0.0;
                }
                const double lm = std::sqrt(dot(mean, mean));
                for (int j = 0; j < 3; ++j)
                    flat = flat && dot(n[j], mean) > kFlatCos * lm * std::sqrt(dot(n[j], n[j]));
                if (flat) ++n_low;
            }
            if (val > lim.max_valence) ++n_high;
            vflag[v] = (char)((boundary ? 1 : 0) | (bad ? 2 : 0));
            for (const auto& e : nb) {
                if (e.w < v) continue;
                if (has_dup[v] || has_dup[e.w])
                    my_dup.push_back({std::min(pid[v], pid[e.w]), std::max(pid[v], pid[e.w])});
                if (e.cnt == 1) { ++n_bnd; my_bnd.push_back({v, e.w}); continue; }
                if (e.cnt > 2) { ++n_nm; continue; }
                const bool mis = e.fw == 2 || e.bw == 2;
                if (mis) ++n_mis;
                P3 n0 = face_normal(inc[b0 + e.f0]), n1 = face_normal(inc[b0 + e.f1]);
                const double l = std::sqrt(dot(n0, n0) * dot(n1, n1));
                if (l > 0.0 && (mis ? -1.0 : 1.0) * dot(n0, n1) < -cos_fold * l) ++n_fold;
            }
        }
        #pragma omp critical
        {
            max_val = std::max(max_val, my_max_val);
            boundary_pairs.insert(boundary_pairs.end(), my_bnd.begin(), my_bnd.end());
            dup_edge_keys.insert(dup_edge_keys.end(), my_dup.begin(), my_dup.end());
        }
    }
    r.boundary_edges = n_bnd; r.nonmanifold_edges = n_nm; r.misoriented_edges = n_mis;
    r.folds = n_fold; r.nonmanifold_vertices = n_nmv; r.low_valence = n_low;
    r.high_valence = n_high; r.max_valence = max_val;
    std::sort(dup_edge_keys.begin(), dup_edge_keys.end());
    for (size_t i = 1; i < dup_edge_keys.size(); ++i)
        r.duplicate_edges += dup_edge_keys[i] == dup_edge_keys[i - 1];

    // ---- duplicate faces (same indices); same positions are found below ----
    long n_dupf = 0;
    #pragma omp parallel for schedule(dynamic, 4096) reduction(+:n_dupf)
    for (long t = 0; t < nf; ++t) {
        if (!fok[t]) continue;
        std::array<int,3> a = F[t];
        std::sort(a.begin(), a.end());
        for (int j = off[a[0]]; j < off[a[0] + 1]; ++j) {
            const int s = inc[j];
            if (s >= t) continue;
            std::array<int,3> b = F[s];
            std::sort(b.begin(), b.end());
            if (a == b) { ++n_dupf; break; }
        }
    }
    r.duplicate_faces = n_dupf;

    // ---- per face: area, angles, aspect ----
    std::vector<char> nonzero(nf, 0);
    long n_zero = 0, n_angle = 0, n_aspect = 0, n5 = 0, n15 = 0;
    double min_ang = 180.0, max_asp = 0.0;
    const double sin_min = std::sin(lim.min_angle_deg * 0.017453292519943295);
    const double sin5 = std::sin(5.0 * 0.017453292519943295), sin15 = std::sin(15.0 * 0.017453292519943295);
    #pragma omp parallel reduction(+:n_zero,n_angle,n_aspect,n5,n15)
    {
    double my_min = 180.0, my_max = 0.0;
    #pragma omp for schedule(static)
    for (long t = 0; t < nf; ++t) {
        if (!fok[t]) continue;
        const auto& f = F[t];
        P3 p[3];
        for (int k = 0; k < 3; ++k) p[k] = {V[f[k]][0], V[f[k]][1], V[f[k]][2]};
        double l[3];
        for (int k = 0; k < 3; ++k) { const P3 e = sub(p[(k+1)%3], p[k]); l[k] = std::sqrt(dot(e, e)); }
        std::sort(l, l + 3);
        const P3 n = cross(sub(p[1], p[0]), sub(p[2], p[0]));
        const double a2 = std::sqrt(dot(n, n));
        if (!(a2 > 1e-12 * l[2] * l[2])) { ++n_zero; ++n5; ++n15; my_min = 0.0; continue; }
        nonzero[t] = 1;
        const double s = std::min(1.0, a2 / (l[1] * l[2]));
        n5 += s < sin5;
        n15 += s < sin15;
        my_min = std::min(my_min, std::asin(s) * 57.29577951308232);
        if (s < sin_min) ++n_angle;
        const double asp = l[2] * (l[0] + l[1] + l[2]) / (2.0 * std::sqrt(3.0) * a2);
        my_max = std::max(my_max, asp);
        if (asp > lim.max_aspect) ++n_aspect;
    }
    #pragma omp critical
    {
        min_ang = std::min(min_ang, my_min);
        max_asp = std::max(max_asp, my_max);
    }
    }
    r.zero_area = n_zero; r.small_angles = n_angle; r.large_aspect = n_aspect;
    r.under_5_deg = n5; r.under_15_deg = n15;
    r.min_angle_deg = min_ang; r.max_aspect = max_asp;

    // ---- parts: closed ones must enclose positive volume ----
    {
        UnionFind uf(nv);
        for (long t = 0; t < nf; ++t)
            if (fok[t]) { uf.unite(F[t][0], F[t][1]); uf.unite(F[t][1], F[t][2]); }
        std::vector<int> faces_of(nv, 0);
        std::vector<char> open(nv, 0);
        std::vector<double> vol(nv, 0.0);
        for (int v = 0; v < nv; ++v) if (vflag[v]) open[uf.find(v)] = 1;
        for (long t = 0; t < nf; ++t) {
            if (!fok[t]) continue;
            const auto& f = F[t];
            const int root = uf.find(f[0]);
            ++faces_of[root];
            const P3 o = {V[root][0], V[root][1], V[root][2]};
            P3 p[3];
            for (int k = 0; k < 3; ++k) p[k] = sub({V[f[k]][0], V[f[k]][1], V[f[k]][2]}, o);
            vol[root] += dot(p[0], cross(p[1], p[2]));
        }
        for (int v = 0; v < nv; ++v) {
            if (faces_of[v] == 0) continue;
            ++r.parts;
            if (lim.min_part_faces > 1 && faces_of[v] < lim.min_part_faces) ++r.small_parts;
            if (!open[v] && vol[v] < 0.0) ++r.inside_out_parts;
        }
        UnionFind bl(nv);
        for (const auto& e : boundary_pairs) bl.unite(e.first, e.second);
        std::vector<char> seen(nv, 0);
        for (const auto& e : boundary_pairs) {
            const int root = bl.find(e.first);
            if (!seen[root]) { seen[root] = 1; ++r.boundary_loops; }
        }
    }

    // ---- self-intersections ----
    if (lim.intersections) {
        std::vector<int> ids;
        ids.reserve(nf);
        for (long t = 0; t < nf; ++t) if (nonzero[t]) ids.push_back((int)t);
        FaceBVH bvh;
        bvh.build(mesh, ids);
        const int n = (int)ids.size();
        std::vector<char> hit(nf, 0), dup(nf, 0);
        long pairs = 0;
        #pragma omp parallel for schedule(dynamic, 1024) reduction(+:pairs)
        for (int i = 0; i < n; ++i) {
            const int t = bvh.face[i];
            const std::array<float,3>* a[3] = {&V[F[t][0]], &V[F[t][1]], &V[F[t][2]]};
            bvh.visit(bvh.box[i], i + 1, [&](int j) {
                const int s = bvh.face[j];
                const std::array<float,3>* b[3] = {&V[F[s][0]], &V[F[s][1]], &V[F[s][2]]};
                const int res = triangles_cross(a, b);
                if (res == 1) { ++pairs; hit[t] = 1; hit[s] = 1; }
                else if (res == 2) dup[s] = 1;
                return false;
            });
        }
        r.intersecting_pairs = pairs;
        long nd = 0;
        for (long t = 0; t < nf; ++t) { r.intersecting_faces += hit[t]; nd += dup[t]; }
        r.duplicate_faces = std::max(r.duplicate_faces, nd);
    }
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

std::vector<std::string> format_mesh_check(const MeshCheckReport& r,
                                           const MeshCheckLimits& lim,
                                           const std::string& tag) {
    std::vector<std::string> out;
    char buf[512];
    std::snprintf(buf, sizeof buf,
                  "check %s: %ld vertices, %ld faces, %ld parts, %ld boundary edges in "
                  "%ld loops (%.2fs)",
                  tag.c_str(), r.vertices, r.faces, r.parts, r.boundary_edges,
                  r.boundary_loops, r.seconds);
    out.push_back(buf);
    std::snprintf(buf, sizeof buf,
                  "  indexing: bad index %ld, repeated index %ld, non-finite %ld, unused "
                  "vertices %ld, duplicate vertices %ld, edges %ld, faces %ld",
                  r.bad_index, r.repeated_index, r.nonfinite, r.unused_vertices,
                  r.duplicate_vertices, r.duplicate_edges, r.duplicate_faces);
    out.push_back(buf);
    std::snprintf(buf, sizeof buf,
                  "  topology: non-manifold edges %ld, non-manifold vertices %ld, "
                  "mis-oriented edges %ld, inside-out parts %ld, parts under %d faces %ld",
                  r.nonmanifold_edges, r.nonmanifold_vertices, r.misoriented_edges,
                  r.inside_out_parts, lim.min_part_faces, r.small_parts);
    out.push_back(buf);
    if (lim.intersections)
        std::snprintf(buf, sizeof buf,
                      "  geometry: self-intersecting face pairs %ld (faces %ld), folds under "
                      "%g deg %ld, zero-area faces %ld",
                      r.intersecting_pairs, r.intersecting_faces, (double)lim.fold_deg,
                      r.folds, r.zero_area);
    else
        std::snprintf(buf, sizeof buf,
                      "  geometry: self-intersections not tested, folds under %g deg %ld, "
                      "zero-area faces %ld",
                      (double)lim.fold_deg, r.folds, r.zero_area);
    out.push_back(buf);
    std::snprintf(buf, sizeof buf,
                  "  quality: angles under %g deg %ld (smallest %.3f), aspect over %g %ld "
                  "(largest %.1f), flat valence 3 %ld, valence over %d %ld (largest %d)",
                  (double)lim.min_angle_deg, r.small_angles, r.min_angle_deg,
                  (double)lim.max_aspect, r.large_aspect, r.max_aspect, r.low_valence,
                  lim.max_valence, r.high_valence, r.max_valence);
    out.push_back(buf);
    const long d = r.defects();
    if (d == 0) std::snprintf(buf, sizeof buf, "  result: PASS");
    else std::snprintf(buf, sizeof buf, "  result: FAIL, %ld defects", d);
    out.push_back(buf);
    return out;
}

}  // namespace meshing
