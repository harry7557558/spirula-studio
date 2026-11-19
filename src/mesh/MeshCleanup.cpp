// MeshCleanup.cpp -- orientation, duplicate faces, floaters and bowtie vertices (MeshStages.h).

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
// Orientation: a breadth-first flood (which keeps a non-orientable defect local)
// winds neighbours alike; then a closed part faces out by its volume's sign,
// unless it holds most cameras, and an open one by most extraction guesses.
// ---------------------------------------------------------------------------
void orient_mesh(Mesh& mesh, const float* cam_pos, int num_cameras) {
    const int nf = (int)mesh.F.size();
    if (nf == 0) return;
    auto& F = mesh.F;
    const auto& V = mesh.V;
    const VertexFaces vf(mesh);
    auto neighbour = [&](int t, int a, int b) {
        for (int j = vf.off[a]; j < vf.off[a + 1]; ++j) {
            const int s = vf.inc[j];
            if (s != t && (F[s][0] == b || F[s][1] == b || F[s][2] == b)) return s;
        }
        return -1;
    };
    std::vector<char> vis(nf, 0), flipped(nf, 0);
    std::vector<int> comp;
    for (int seed = 0; seed < nf; ++seed) {
        if (vis[seed]) continue;
        vis[seed] = 1;
        comp.clear();
        comp.push_back(seed);
        bool closed = true;
        for (size_t h = 0; h < comp.size(); ++h) {
            const int t = comp[h];
            for (int k = 0; k < 3; ++k) {
                const int a = F[t][k], b = F[t][(k + 1) % 3];
                const int nb = neighbour(t, a, b);
                if (nb < 0) { closed = false; continue; }
                if (vis[nb]) continue;
                auto& g = F[nb];
                for (int m = 0; m < 3; ++m)
                    if (g[m] == a && g[(m + 1) % 3] == b) { std::swap(g[1], g[2]); flipped[nb] = 1; break; }
                vis[nb] = 1;
                comp.push_back(nb);
            }
        }
        bool flip_all;
        const auto& o = V[F[seed][0]];
        double vol = 0.0;
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (int t : comp) {
            double p[3][3];
            for (int k = 0; k < 3; ++k)
                for (int a = 0; a < 3; ++a) {
                    p[k][a] = (double)V[F[t][k]][a] - o[a];
                    lo[a] = std::min(lo[a], V[F[t][k]][a]);
                    hi[a] = std::max(hi[a], V[F[t][k]][a]);
                }
            vol += p[0][0] * (p[1][1]*p[2][2] - p[1][2]*p[2][1]) -
                   p[0][1] * (p[1][0]*p[2][2] - p[1][2]*p[2][0]) +
                   p[0][2] * (p[1][0]*p[2][1] - p[1][1]*p[2][0]);
        }
        if (closed && vol != 0.0) {
            int inside = 0;
            for (int c = 0; c < num_cameras; ++c) {
                const float* q = cam_pos + 3 * c;
                inside += q[0] >= lo[0] && q[0] <= hi[0] && q[1] >= lo[1] && q[1] <= hi[1] &&
                          q[2] >= lo[2] && q[2] <= hi[2];
            }
            flip_all = (2 * inside > num_cameras) ? vol > 0.0 : vol < 0.0;
        } else {
            long fl = 0;
            for (int t : comp) fl += flipped[t];
            flip_all = fl * 2 > (long)comp.size();
        }
        if (flip_all)
            for (int t : comp) std::swap(F[t][1], F[t][2]);
    }
}

// ---------------------------------------------------------------------------
// Culling by patch: seen specks (sight through a gap) turn unseen, then unseen
// specks (pits, creases) seen, as dropping those opens holes. Dessert capture:
// 17.5k of 17.8k unseen patches had under 100 raw vertices, 9% of the unseen.
// ---------------------------------------------------------------------------
long despeckle_visibility(const Mesh& mesh, std::vector<unsigned char>& visible, int min_patch) {
    const int nv = (int)mesh.V.size();
    std::vector<int> parent(nv), size(nv);
    auto find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    // patches of vertices whose flag is `state`, joined by edges; their sizes at the roots
    auto patches = [&](unsigned char state) {
        for (int v = 0; v < nv; ++v) { parent[v] = v; size[v] = 0; }
        for (const auto& f : mesh.F)
            for (int k = 0; k < 3; ++k) {
                const int a = f[k], b = f[(k + 1) % 3];
                if (visible[a] != state || visible[b] != state) continue;
                const int ra = find(a), rb = find(b);
                if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
            }
        for (int v = 0; v < nv; ++v) if (visible[v] == state) ++size[find(v)];
    };
    patches(1);
    for (int v = 0; v < nv; ++v)
        if (visible[v] == 1 && size[find(v)] < min_patch) visible[v] = 0;
    patches(0);
    std::vector<char> speck(nv, 0);
    for (int v = 0; v < nv; ++v) speck[v] = visible[v] == 0 && size[find(v)] < min_patch;
    // a part no camera sees goes whole, whatever its size
    for (int v = 0; v < nv; ++v) { parent[v] = v; size[v] = 0; }
    for (const auto& f : mesh.F)
        for (int k = 1; k < 3; ++k) {
            const int ra = find(f[0]), rb = find(f[k]);
            if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
        }
    for (int v = 0; v < nv; ++v) if (visible[v] == 1) size[find(v)] = 1;
    long kept = 0;
    for (int v = 0; v < nv; ++v)
        if (speck[v] && size[find(v)]) { visible[v] = 1; ++kept; }
    return kept;
}

// ---------------------------------------------------------------------------
// Duplicate faces: marching tetrahedra now and then emits two triangles on one
// vertex triple (a zero-volume doublet), which survive the merge. One is kept;
// a doublet's leftover face falls to the floater pass.
// ---------------------------------------------------------------------------
void dedup_faces(Mesh& mesh, bool verbose) {
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
// Floaters: connected parts (faces sharing a vertex) under min_faces faces go,
// with the vertices they leave unused.
// ---------------------------------------------------------------------------
void remove_floaters(Mesh& mesh, int min_faces, bool verbose) {
    const int nv = (int)mesh.V.size();
    const long nf = (long)mesh.F.size();
    if (min_faces <= 1 || nv == 0 || nf == 0) return;

    std::vector<int> parent(nv);
    for (int i = 0; i < nv; ++i) parent[i] = i;
    auto find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto unite = [&](int a, int b) { a=find(a); b=find(b); if (a!=b) parent[a]=b; };
    for (long t = 0; t < nf; ++t) {
        const auto& f = mesh.F[t];
        unite(f[0], f[1]); unite(f[1], f[2]);
    }

    std::vector<long> comp_faces(nv, 0);   // by root
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
// Bowtie vertices: faces around a vertex in several edge-connected fans get
// one copy of the vertex per fan, each nudged into its own fan so the copies
// do not coincide and orientation can propagate around each fan.
// ---------------------------------------------------------------------------
constexpr float kSplitNudge = 0.05f;   // of the way to the fan's centroid

void split_nonmanifold_vertices(Mesh& mesh, bool verbose) {
    const int nv0 = (int)mesh.V.size();
    if (nv0 == 0 || mesh.F.empty()) return;
    const VertexFaces vf(mesh);
    long n_split = 0, n_new = 0;
    std::vector<int> uf, root_vid, fan_of;
    std::vector<std::array<int,2>> first_face;   // other vertex -> local face
    for (int v = 0; v < nv0; ++v) {
        const int b0 = vf.off[v], m = vf.off[v + 1] - b0;
        if (m < 2) continue;
        uf.resize(m);
        for (int j = 0; j < m; ++j) uf[j] = j;
        auto ufind = [&](int x) { while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; };
        first_face.clear();
        for (int j = 0; j < m; ++j)
            for (int w : mesh.F[vf.inc[b0 + j]]) {
                if (w == v) continue;
                bool found = false;
                for (auto& e : first_face)
                    if (e[0] == w) {
                        const int a = ufind(e[1]), b = ufind(j);
                        if (a != b) uf[a] = b;
                        found = true;
                        break;
                    }
                if (!found) first_face.push_back({w, j});
            }
        int fans = 0;
        for (int j = 0; j < m; ++j) fans += ufind(j) == j;
        if (fans < 2) continue;
        ++n_split;
        root_vid.assign(m, -1);
        const int first_root = ufind(0);
        const std::array<float,3> p = mesh.V[v];
        for (int j = 0; j < m; ++j) {
            const int r = ufind(j);
            if (root_vid[r] < 0) {
                root_vid[r] = r == first_root ? v : (int)mesh.V.size();
                if (r != first_root) { mesh.V.push_back(p); ++n_new; }
            }
        }
        for (int r = 0; r < m; ++r) {
            if (root_vid[r] < 0) continue;
            std::array<double,3> c = {0, 0, 0};
            int cnt = 0;
            for (int j = 0; j < m; ++j) {
                if (ufind(j) != r) continue;
                auto& f = mesh.F[vf.inc[b0 + j]];
                for (int& w : f) {
                    if (w == v) { w = root_vid[r]; continue; }
                    for (int a = 0; a < 3; ++a) c[a] += mesh.V[w][a];
                    ++cnt;
                }
            }
            auto& q = mesh.V[root_vid[r]];
            for (int a = 0; a < 3; ++a)
                q[a] = p[a] + kSplitNudge * (float)(c[a] / std::max(cnt, 1) - p[a]);
        }
    }
    if (verbose)
        mlog::out(mlog::Stage::Cleanup, mmsg::nonmanifold_split,
                  {n_split, n_new, (long long)mesh.V.size()});
}

} // namespace meshing
