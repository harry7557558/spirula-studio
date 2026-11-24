#include "mesh/GroundSplit.h"

#include "data/CameraMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace meshing {

namespace {

constexpr int kOrthographic = 4;   // CameraModelType::ORTHOGRAPHIC
// A vertex counts as seen within this many pixel footprints plus this share of
// its distance: the merge and relaxation move it off the rendered median depth.
constexpr double kBandPx = 3.0, kBandRel = 0.02;
constexpr int kSmoothIters = 2;

int find_root(std::vector<int>& parent, int x) {
    while (parent[(size_t)x] != x) {
        parent[(size_t)x] = parent[(size_t)parent[(size_t)x]];
        x = parent[(size_t)x];
    }
    return x;
}

}  // namespace

void vote_ground(const TsdfCamera& c, const float* depth, const uint8_t* labels,
                 const MeshData& mesh, float* ground, float* seen) {
    const bool ortho = c.model == kOrthographic;
    const bool normals = mesh.N.size() == mesh.V.size();
    double R[9], t[3], o[3];
    for (int r = 0; r < 3; ++r) {
        for (int k = 0; k < 3; ++k) R[3 * r + k] = c.viewmat[4 * r + k];
        t[r] = c.viewmat[4 * r + 3];
    }
    for (int k = 0; k < 3; ++k) o[k] = -(R[k] * t[0] + R[3 + k] * t[1] + R[6 + k] * t[2]);
    const double fwd[3] = {R[6], R[7], R[8]};

    const long nv = (long)mesh.V.size();
    #pragma omp parallel for schedule(static)
    for (long v = 0; v < nv; ++v) {
        const auto& g = mesh.V[(size_t)v];
        if (normals) {
            const auto& n = mesh.N[(size_t)v];
            const double to[3] = {ortho ? -fwd[0] : o[0] - g[0], ortho ? -fwd[1] : o[1] - g[1],
                                  ortho ? -fwd[2] : o[2] - g[2]};
            if (n[0] * to[0] + n[1] * to[1] + n[2] * to[2] <= 0.0) continue;
        }
        double pc[3];
        for (int r = 0; r < 3; ++r)
            pc[r] = R[3 * r] * g[0] + R[3 * r + 1] * g[1] + R[3 * r + 2] * g[2] + t[r];
        double uv[2], dist;
        if (ortho) {
            if (pc[2] <= 0.0) continue;
            uv[0] = pc[0]; uv[1] = pc[1];
            dist = pc[2];
        } else {
            if (!camhost::project_ray(pc, c.model, c.distortion, c.dist, uv)) continue;
            dist = std::sqrt(pc[0] * pc[0] + pc[1] * pc[1] + pc[2] * pc[2]);
        }
        const double px = uv[0] * c.fx + c.cx, py = uv[1] * c.fy + c.cy;
        if (!(px >= 0.0 && py >= 0.0 && px < c.width && py < c.height)) continue;
        const long p = (long)py * c.width + (long)px;
        const float d = depth[p];
        const uint8_t l = labels[p];
        if (!(d > 0.0f) || l == kGroundUnknown) continue;
        const double footprint = (ortho ? 1.0 : dist) / c.fx;
        if (std::fabs(d - dist) > kBandPx * footprint + kBandRel * dist) continue;
        seen[v] += 1.0f;
        if (l == kGroundYes) ground[v] += 1.0f;
    }
}

std::vector<uint8_t> ground_faces(const MeshData& mesh, const std::vector<float>& ground,
                                  const std::vector<float>& seen, int min_island) {
    const size_t nv = mesh.V.size(), nf = mesh.F.size();
    std::vector<float> share(nv, -1.0f), next(nv);
    for (size_t v = 0; v < nv; ++v)
        if (seen[v] > 0.0f) share[v] = ground[v] / seen[v];

    // Each face averages its known corners; each vertex then averages its faces.
    std::vector<float> sum(nv), cnt(nv);
    for (int it = 0; it < kSmoothIters; ++it) {
        std::fill(sum.begin(), sum.end(), 0.0f);
        std::fill(cnt.begin(), cnt.end(), 0.0f);
        for (const auto& f : mesh.F) {
            float s = 0.0f, n = 0.0f;
            for (int k = 0; k < 3; ++k)
                if (share[(size_t)f[k]] >= 0.0f) { s += share[(size_t)f[k]]; n += 1.0f; }
            if (n == 0.0f) continue;
            for (int k = 0; k < 3; ++k) { sum[(size_t)f[k]] += s / n; cnt[(size_t)f[k]] += 1.0f; }
        }
        for (size_t v = 0; v < nv; ++v) next[v] = cnt[v] > 0.0f ? sum[v] / cnt[v] : share[v];
        share.swap(next);
    }

    std::vector<uint8_t> label(nf, 0);
    for (size_t t = 0; t < nf; ++t) {
        float s = 0.0f, n = 0.0f;
        for (int k = 0; k < 3; ++k)
            if (share[(size_t)mesh.F[t][k]] >= 0.0f) { s += share[(size_t)mesh.F[t][k]]; n += 1.0f; }
        label[t] = n > 0.0f && s >= 0.5f * n ? 1 : 0;
    }
    if (min_island <= 1 || nf == 0) return label;

    // Faces of one label that share a vertex are one island.
    std::vector<int> parent(nf);
    std::iota(parent.begin(), parent.end(), 0);
    std::vector<int> first(2 * nv, -1);   // per vertex and label, the first face met
    for (size_t t = 0; t < nf; ++t)
        for (int k = 0; k < 3; ++k) {
            int& f0 = first[2 * (size_t)mesh.F[t][k] + label[t]];
            if (f0 < 0) { f0 = (int)t; continue; }
            const int a = find_root(parent, f0), b = find_root(parent, (int)t);
            if (a != b) parent[(size_t)std::max(a, b)] = std::min(a, b);
        }
    std::vector<int> size(nf, 0);
    for (size_t t = 0; t < nf; ++t) size[(size_t)find_root(parent, (int)t)]++;
    for (size_t t = 0; t < nf; ++t)
        if (size[(size_t)find_root(parent, (int)t)] < min_island) label[t] ^= 1;
    return label;
}

void grow_ground(const MeshData& mesh, std::vector<uint8_t>& label, double max_deg) {
    const size_t nf = mesh.F.size();
    std::vector<std::array<double, 3>> n(nf);
    for (size_t t = 0; t < nf; ++t) {
        const auto &a = mesh.V[(size_t)mesh.F[t][0]], &b = mesh.V[(size_t)mesh.F[t][1]],
                   &c = mesh.V[(size_t)mesh.F[t][2]];
        const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const double v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        n[t] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const double l = std::sqrt(n[t][0] * n[t][0] + n[t][1] * n[t][1] + n[t][2] * n[t][2]);
        for (double& x : n[t]) x = l > 0.0 ? x / l : 0.0;
    }
    // Faces sharing an edge: sort the edges, equal neighbours are a pair.
    struct Edge { int a, b, face; };
    std::vector<Edge> edges;
    edges.reserve(nf * 3);
    for (size_t t = 0; t < nf; ++t)
        for (int k = 0; k < 3; ++k) {
            const int a = mesh.F[t][k], b = mesh.F[t][(k + 1) % 3];
            edges.push_back({std::min(a, b), std::max(a, b), (int)t});
        }
    std::sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y) {
        return x.a != y.a ? x.a < y.a : x.b < y.b;
    });
    std::vector<int> first(nf + 1, 0), next_face;   // adjacency as CSR
    std::vector<std::pair<int, int>> pairs;
    for (size_t i = 0; i + 1 < edges.size(); ++i)
        if (edges[i].a == edges[i + 1].a && edges[i].b == edges[i + 1].b)
            pairs.push_back({edges[i].face, edges[i + 1].face});
    std::vector<Edge>().swap(edges);
    for (const auto& p : pairs) { first[(size_t)p.first + 1]++; first[(size_t)p.second + 1]++; }
    for (size_t t = 0; t < nf; ++t) first[t + 1] += first[t];
    next_face.resize((size_t)first[nf]);
    std::vector<int> fill(first.begin(), first.end() - 1);
    for (const auto& p : pairs) {
        next_face[(size_t)fill[(size_t)p.first]++] = p.second;
        next_face[(size_t)fill[(size_t)p.second]++] = p.first;
    }

    // Each grown face keeps the normal of the labelled face it came from, so
    // the growth cannot bend round a rounded corner a few degrees at a time.
    const double cos_max = std::cos(max_deg * 3.14159265358979323846 / 180.0);
    std::vector<int> seed(nf, -1), front, grown;
    for (size_t t = 0; t < nf; ++t)
        if (label[t]) { seed[t] = (int)t; front.push_back((int)t); }
    while (!front.empty()) {
        grown.clear();
        for (int f : front)
            for (int i = first[(size_t)f]; i < first[(size_t)f + 1]; ++i) {
                const int h = next_face[(size_t)i];
                if (seed[(size_t)h] >= 0) continue;
                const auto& sn = n[(size_t)seed[(size_t)f]];
                const auto& hn = n[(size_t)h];
                if (sn[0] * hn[0] + sn[1] * hn[1] + sn[2] * hn[2] < cos_max) continue;
                seed[(size_t)h] = seed[(size_t)f];
                label[(size_t)h] = 1;
                grown.push_back(h);
            }
        front.swap(grown);
    }
}

MeshData mesh_subset(const MeshData& mesh, const std::vector<uint8_t>& face_label,
                     uint8_t want) {
    MeshData out;
    std::vector<int> remap(mesh.V.size(), -1);
    const bool has_n = mesh.N.size() == mesh.V.size(), has_c = mesh.C.size() == mesh.V.size(),
               has_uv = mesh.UV.size() == mesh.V.size();
    for (size_t t = 0; t < mesh.F.size(); ++t) {
        if (face_label[t] != want) continue;
        std::array<int, 3> f;
        for (int k = 0; k < 3; ++k) {
            const int v = mesh.F[t][k];
            if (remap[(size_t)v] < 0) {
                remap[(size_t)v] = (int)out.V.size();
                out.V.push_back(mesh.V[(size_t)v]);
                if (has_n) out.N.push_back(mesh.N[(size_t)v]);
                if (has_c) out.C.push_back(mesh.C[(size_t)v]);
                if (has_uv) out.UV.push_back(mesh.UV[(size_t)v]);
            }
            f[k] = remap[(size_t)v];
        }
        out.F.push_back(f);
    }
    out.tex_width = mesh.tex_width;
    out.tex_height = mesh.tex_height;
    out.texture = mesh.texture;
    return out;
}

} // namespace meshing
