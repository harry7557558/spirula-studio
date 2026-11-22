// MeshRepair.cpp -- the last cut-and-refill repair (MeshStages.h).

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
// Last repair: faces crossing others are cut out, and so are the faces around
// slivers, folds and flat valence-3 vertices; the holes are refilled. Margins
// keep a refill from landing just past MeshCheck's limits.
// ---------------------------------------------------------------------------

// Marks the faces to cut; returns how many.
long mark_defects(const Mesh& mesh, std::vector<char>& cut) {
    const auto& V = mesh.V;
    const auto& F = mesh.F;
    const int nv = (int)V.size();
    const long nf = (long)F.size();
    cut.assign(nf, 0);
    std::vector<char> bad_v(nv, 0);
    auto normal = [&](const std::array<int,3>& f) {
        const auto &a = V[f[0]], &b = V[f[1]], &c = V[f[2]];
        const double e1[3] = {(double)b[0]-a[0], (double)b[1]-a[1], (double)b[2]-a[2]};
        const double e2[3] = {(double)c[0]-a[0], (double)c[1]-a[1], (double)c[2]-a[2]};
        return std::array<double,3>{e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2],
                                    e1[0]*e2[1]-e1[1]*e2[0]};
    };
    std::vector<int> ids;
    ids.reserve(nf);
    const VertexFaces vf(mesh);
    auto edge_faces = [&](int a, int b) {
        int n = 0;
        for (int j = vf.off[a]; j < vf.off[a + 1]; ++j) {
            const auto& g = F[vf.inc[j]];
            n += g[0] == b || g[1] == b || g[2] == b;
        }
        return n;
    };
    // A sliver on a boundary goes alone; inside, a needle takes its short
    // edge's ends and a cap its apex, so the refill drops them.
    #pragma omp parallel for schedule(static)
    for (long t = 0; t < nf; ++t) {
        const auto& f = F[t];
        double l[3];   // l[k]: the edge opposite corner k
        for (int k = 0; k < 3; ++k) {
            const auto &p = V[f[(k + 1) % 3]], &q = V[f[(k + 2) % 3]];
            l[k] = std::sqrt(((double)p[0]-q[0])*(p[0]-q[0]) + ((double)p[1]-q[1])*(p[1]-q[1]) +
                             ((double)p[2]-q[2])*(p[2]-q[2]));
        }
        const int ks = l[0] <= l[1] && l[0] <= l[2] ? 0 : l[1] <= l[2] ? 1 : 2;
        const int kl = l[0] >= l[1] && l[0] >= l[2] ? 0 : l[1] >= l[2] ? 1 : 2;
        const double lmid = l[0] + l[1] + l[2] - l[ks] - l[kl];
        const auto n = normal(f);
        const double a2 = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (a2 > 1e-12 * l[kl] * l[kl] && a2 >= kRepairMinSin * lmid * l[kl] &&
            l[kl] * (l[0] + l[1] + l[2]) <= kRepairMaxAspect * 2.0 * std::sqrt(3.0) * a2)
            continue;
        bool on_boundary = false;
        for (int k = 0; k < 3; ++k) on_boundary = on_boundary || edge_faces(f[k], f[(k + 1) % 3]) == 1;
        if (on_boundary) { cut[t] = 1; continue; }
        const double h = a2 / std::max(l[kl], 1e-30);   // apex height over the long edge
        if (h * h < 0.25 * lmid * lmid && l[ks] > 0.25 * l[kl]) bad_v[f[kl]] = 1;   // cap
        else { bad_v[f[(ks + 1) % 3]] = 1; bad_v[f[(ks + 2) % 3]] = 1; }
    }
    for (long t = 0; t < nf; ++t) {
        const auto n = normal(F[t]);
        if (n[0]*n[0] + n[1]*n[1] + n[2]*n[2] > 0.0) ids.push_back((int)t);
    }
    // folds, and flat valence-3 vertices
    #pragma omp parallel for schedule(dynamic, 4096)
    for (int v = 0; v < nv; ++v) {
        const int b0 = vf.off[v], m = vf.off[v + 1] - b0;
        if (m == 0) continue;
        // either winding: orient_mesh settles it only after this
        for (int j = 0; j < m; ++j) {
            const auto& f = F[vf.inc[b0 + j]];
            const int k = f[0] == v ? 0 : f[1] == v ? 1 : 2;
            const int w = f[(k + 1) % 3];
            for (int i = 0; i < m; ++i) {
                const auto& g = F[vf.inc[b0 + i]];
                const int kg = g[0] == v ? 0 : g[1] == v ? 1 : 2;
                const bool against = g[(kg + 2) % 3] == w, along = g[(kg + 1) % 3] == w;
                if (i == j || (!against && !along)) continue;
                const auto n0 = normal(f), n1 = normal(g);
                const double l = std::sqrt((n0[0]*n0[0] + n0[1]*n0[1] + n0[2]*n0[2]) *
                                           (n1[0]*n1[0] + n1[1]*n1[1] + n1[2]*n1[2]));
                const double d = n0[0]*n1[0] + n0[1]*n1[1] + n0[2]*n1[2];
                if ((against ? d : -d) < -kFoldCos * l) bad_v[v] = 1;
            }
        }
        if (m > kRepairMaxValence) {   // m counts faces: one fewer than edges on a boundary
            bad_v[v] = 1;
            continue;
        }
        if (m != 3) continue;
        int nb[6], cnt[6], q = 0;
        for (int j = 0; j < 3; ++j)
            for (int w : F[vf.inc[b0 + j]]) {
                if (w == v) continue;
                int i = 0;
                while (i < q && nb[i] != w) ++i;
                if (i == q) { nb[q] = w; cnt[q++] = 0; }
                ++cnt[i];
            }
        if (q != 3 || cnt[0] != 2 || cnt[1] != 2 || cnt[2] != 2) continue;   // not interior
        std::array<double,3> n[3];
        for (int j = 0; j < 3; ++j) {
            n[j] = normal(F[vf.inc[b0 + j]]);
            const double l = std::sqrt(n[j][0]*n[j][0] + n[j][1]*n[j][1] + n[j][2]*n[j][2]);
            for (double& x : n[j]) x = l > 0 ? x / l : 0.0;
        }
        // Orientation comes after this: each face's normal as face 0 winds the
        // fan, which is opposite ways along the edge the two share.
        auto runs = [&](const std::array<int,3>& f, int w) {   // v -> w
            for (int k = 0; k < 3; ++k) if (f[k] == v && f[(k + 1) % 3] == w) return true;
            return false;
        };
        const auto& f0 = F[vf.inc[b0]];
        std::array<double,3> mean = n[0];
        for (int j = 1; j < 3; ++j) {
            const auto& fj = F[vf.inc[b0 + j]];
            for (int w : fj) {
                if (w == v || (f0[0] != w && f0[1] != w && f0[2] != w)) continue;
                if (runs(f0, w) == runs(fj, w)) for (double& x : n[j]) x = -x;
                break;
            }
            for (int a = 0; a < 3; ++a) mean[a] += n[j][a];
        }
        const double lm = std::sqrt(mean[0]*mean[0] + mean[1]*mean[1] + mean[2]*mean[2]);
        bool flat = true;
        for (int j = 0; j < 3; ++j)
            flat = flat && n[j][0]*mean[0] + n[j][1]*mean[1] + n[j][2]*mean[2] > kRepairFlatCos * lm;
        if (flat) bad_v[v] = 1;
    }
    // two vertices at one position: no edit moves them apart, so their faces go
    {
        size_t cap = 64;
        while (cap < (size_t)nv * 2) cap <<= 1;
        std::vector<int> slot(cap, -1);
        for (int v = 0; v < nv; ++v) {
            if (vf.off[v] == vf.off[v + 1]) continue;
            uint32_t k[3];
            std::memcpy(k, V[v].data(), 12);
            uint64_t h = (k[0] * 0x9E3779B97F4A7C15ull) ^ (k[1] * 0xC2B2AE3D27D4EB4Full) ^
                         (k[2] * 0x165667B19E3779F9ull);
            size_t i = (size_t)(h ^ (h >> 29)) & (cap - 1);
            while (slot[i] >= 0 && V[slot[i]] != V[v]) i = (i + 1) & (cap - 1);
            if (slot[i] < 0) slot[i] = v;
            else bad_v[v] = bad_v[slot[i]] = 1;
        }
    }
    for (long t = 0; t < nf; ++t) {
        const auto& f = F[t];
        if (bad_v[f[0]] || bad_v[f[1]] || bad_v[f[2]]) cut[t] = 1;
    }
    // crossings: the faces themselves
    FaceBVH bvh;
    bvh.build(mesh, ids);
    const int n = (int)ids.size();
    #pragma omp parallel for schedule(dynamic, 1024)
    for (int i = 0; i < n; ++i) {
        const int t = bvh.face[i];
        const std::array<float,3>* a[3] = {&V[F[t][0]], &V[F[t][1]], &V[F[t][2]]};
        bvh.visit(bvh.box[i], i + 1, [&](int j) {
            const int s = bvh.face[j];
            const std::array<float,3>* b[3] = {&V[F[s][0]], &V[F[s][1]], &V[F[s][2]]};
            if (triangles_cross(a, b) != 0) { cut[t] = 1; cut[s] = 1; }
            return false;
        });
    }
    long marked = 0;
    for (long t = 0; t < nf; ++t) marked += cut[t];
    return marked;
}

// Drops the marked faces and the vertices left without one; returns the old
// index of every vertex kept.
std::vector<int> cut_faces(Mesh& mesh, const std::vector<char>& cut) {
    const bool has_color = mesh.C.size() == mesh.V.size();
    std::vector<int> remap(mesh.V.size(), -1), kept;
    std::vector<std::array<float,3>> nV;
    std::vector<std::array<unsigned char,3>> nC;
    std::vector<std::array<int,3>> nF;
    nF.reserve(mesh.F.size());
    for (size_t t = 0; t < mesh.F.size(); ++t) {
        if (cut[t]) continue;
        std::array<int,3> f = mesh.F[t];
        for (int& v : f) {
            if (remap[v] < 0) {
                remap[v] = (int)nV.size();
                kept.push_back(v);
                nV.push_back(mesh.V[v]);
                if (has_color) nC.push_back(mesh.C[v]);
            }
            v = remap[v];
        }
        nF.push_back(f);
    }
    mesh.V.swap(nV);
    mesh.F.swap(nF);
    if (has_color) mesh.C.swap(nC);
    return kept;
}

// At each bowtie vertex, cuts every fan but the one with the most faces,
// until none is left.
void drop_minor_fans(Mesh& mesh) {
    for (int pass = 0; pass < 8; ++pass) {
        const VertexFaces vf(mesh);
        const int nv = (int)mesh.V.size();
        std::vector<char> cut(mesh.F.size(), 0);
        long n = 0;
        #pragma omp parallel for schedule(dynamic, 4096) reduction(+:n)
        for (int v = 0; v < nv; ++v) {
            const int b0 = vf.off[v], m = vf.off[v + 1] - b0;
            if (m < 2) continue;
            std::vector<int> uf(m);
            for (int j = 0; j < m; ++j) uf[j] = j;
            auto find = [&](int x) { while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; };
            for (int i = 0; i < m; ++i)
                for (int j = i + 1; j < m; ++j) {
                    int shared = 0;
                    for (int a : mesh.F[vf.inc[b0 + i]])
                        for (int b : mesh.F[vf.inc[b0 + j]]) shared += a == b;
                    if (shared >= 2) { const int x = find(i), y = find(j); if (x != y) uf[x] = y; }
                }
            std::vector<int> size(m, 0);
            for (int j = 0; j < m; ++j) ++size[find(j)];
            int keep = find(0);
            for (int j = 0; j < m; ++j) if (size[j] > size[keep]) keep = j;
            if (size[keep] == m) continue;
            for (int j = 0; j < m; ++j)
                if (find(j) != keep) { cut[vf.inc[b0 + j]] = 1; ++n; }
        }
        if (n == 0) return;
        cut_faces(mesh, cut);
    }
}

// Vertices on an edge with one face.
std::vector<char> boundary_vertices(const Mesh& mesh) {
    const int nv = (int)mesh.V.size();
    const VertexFaces vf(mesh);
    std::vector<char> out(nv, 0);
    #pragma omp parallel for schedule(dynamic, 4096)
    for (int v = 0; v < nv; ++v)
        for (int j = vf.off[v]; j < vf.off[v + 1] && !out[v]; ++j)
            for (int w : mesh.F[vf.inc[j]]) {
                if (w == v) continue;
                int n = 0;
                for (int i = vf.off[v]; i < vf.off[v + 1]; ++i) {
                    const auto& g = mesh.F[vf.inc[i]];
                    n += g[0] == w || g[1] == w || g[2] == w;
                }
                if (n == 1) { out[v] = 1; break; }
            }
    return out;
}

} // namespace meshing
