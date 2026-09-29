// RegionMesh.cpp -- see RegionMesh.h.

#include "data/RegionMesh.h"

#include "data/LabelField.h"

#include <algorithm>
#include <cmath>

namespace spirula {

namespace {

struct Grid {
    double lo[3] = {0, 0, 0}, h = 0;
    int64_t cells[3] = {0, 0, 0}, dim[3] = {0, 0, 0};
    std::vector<double> at;   // sample positions [dim0*dim1*dim2, 3]
    bool ok() const { return h > 0; }
    int64_t sid(int64_t i, int64_t j, int64_t k) const { return (i * dim[1] + j) * dim[2] + k; }
    int64_t cid(int64_t i, int64_t j, int64_t k) const { return (i * cells[1] + j) * cells[2] + k; }
    int64_t num_samples() const { return dim[0] * dim[1] * dim[2]; }
    int64_t num_cells() const { return cells[0] * cells[1] * cells[2]; }
};

Grid make_grid(const Aabb& box, int cells_long_axis) {
    Grid g;
    if (box.empty() || box.unbounded()) return g;
    double ext = 0;
    for (int a = 0; a < 3; a++) ext = std::max(ext, box.hi[a] - box.lo[a]);
    if (!(ext > 0)) return g;
    g.h = ext / std::max(2, cells_long_axis);
    for (int a = 0; a < 3; a++) {
        g.lo[a] = box.lo[a];
        g.cells[a] = std::max<int64_t>(1, (int64_t)std::ceil((box.hi[a] - box.lo[a]) / g.h));
        g.dim[a] = g.cells[a] + 1;
    }
    g.at.resize((size_t)g.num_samples() * 3);
    for (int64_t i = 0; i < g.dim[0]; i++)
        for (int64_t j = 0; j < g.dim[1]; j++)
            for (int64_t k = 0; k < g.dim[2]; k++) {
                double* p = &g.at[(size_t)g.sid(i, j, k) * 3];
                p[0] = g.lo[0] + g.h * i;
                p[1] = g.lo[1] + g.h * j;
                p[2] = g.lo[2] + g.h * k;
            }
    return g;
}

// Surface nets around the samples labelled `k`; `is_k` answers for any
// point, which the crossings bisect with.
template <class IsK>
RegionMesh extract(const Grid& g, const std::vector<int32_t>& lab, int32_t k, const IsK& is_k) {
    RegionMesh m;
    static const int corner[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                                     {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
    static const int edge[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                    {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    auto in = [&](int64_t s) { return lab[(size_t)s] == k; };
    const int64_t n_cells = g.num_cells();
    std::vector<float> vert((size_t)n_cells * 3);
    std::vector<uint8_t> has((size_t)n_cells, 0);
#pragma omp parallel for schedule(dynamic, 256)
    for (int64_t c = 0; c < n_cells; c++) {
        const int64_t i = c / (g.cells[1] * g.cells[2]), j = (c / g.cells[2]) % g.cells[1],
                      kk = c % g.cells[2];
        int64_t s[8];
        bool any_in = false, any_out = false;
        for (int q = 0; q < 8; q++) {
            s[q] = g.sid(i + corner[q][0], j + corner[q][1], kk + corner[q][2]);
            (in(s[q]) ? any_in : any_out) = true;
        }
        if (!any_in || !any_out) continue;
        double sum[3] = {0, 0, 0};
        int count = 0;
        for (const auto& e : edge) {
            const int64_t a = s[e[0]], b = s[e[1]];
            if (in(a) == in(b)) continue;
            double lo[3], hi[3];
            for (int d = 0; d < 3; d++) {
                lo[d] = g.at[(size_t)(in(a) ? a : b) * 3 + d];
                hi[d] = g.at[(size_t)(in(a) ? b : a) * 3 + d];
            }
            for (int step = 0; step < 6; step++) {
                double mid[3];
                for (int d = 0; d < 3; d++) mid[d] = 0.5 * (lo[d] + hi[d]);
                double* keep = is_k(mid) ? lo : hi;
                for (int d = 0; d < 3; d++) keep[d] = mid[d];
            }
            for (int d = 0; d < 3; d++) sum[d] += 0.5 * (lo[d] + hi[d]);
            count++;
        }
        for (int d = 0; d < 3; d++) vert[(size_t)c * 3 + d] = (float)(sum[d] / count);
        has[(size_t)c] = 1;
    }
    std::vector<uint32_t> index((size_t)n_cells, UINT32_MAX);
    for (int64_t c = 0; c < n_cells; c++)
        if (has[(size_t)c]) {
            index[(size_t)c] = (uint32_t)(m.xyz.size() / 3);
            m.xyz.insert(m.xyz.end(), &vert[(size_t)c * 3], &vert[(size_t)c * 3] + 3);
        }

    // A quad of the four cells around every grid edge the boundary crosses;
    // with (a, b, c) cyclic, that order is counter-clockwise seen from +a.
    for (int a = 0; a < 3; a++) {
        const int b = (a + 1) % 3, cc = (a + 2) % 3;
        int64_t p[3];
        for (p[0] = 0; p[0] < g.dim[0]; p[0]++)
            for (p[1] = 0; p[1] < g.dim[1]; p[1]++)
                for (p[2] = 0; p[2] < g.dim[2]; p[2]++) {
                    if (p[a] + 1 >= g.dim[a] || p[b] < 1 || p[b] >= g.cells[b] || p[cc] < 1 ||
                        p[cc] >= g.cells[cc])
                        continue;
                    int64_t p1[3] = {p[0], p[1], p[2]};
                    p1[a]++;
                    const bool i0 = in(g.sid(p[0], p[1], p[2]));
                    if (i0 == in(g.sid(p1[0], p1[1], p1[2]))) continue;
                    uint32_t v[4];
                    const int off[4][2] = {{-1, -1}, {0, -1}, {0, 0}, {-1, 0}};
                    bool ok = true;
                    for (int q = 0; q < 4 && ok; q++) {
                        int64_t c3[3] = {p[0], p[1], p[2]};
                        c3[b] += off[q][0];
                        c3[cc] += off[q][1];
                        v[q] = index[(size_t)g.cid(c3[0], c3[1], c3[2])];
                        ok = v[q] != UINT32_MAX;
                    }
                    if (!ok) continue;
                    if (!i0) std::swap(v[1], v[3]);
                    m.tri.insert(m.tri.end(), {v[0], v[1], v[2], v[0], v[2], v[3]});
                }
    }
    return m;
}

}  // namespace

RegionMesh region_boundary_mesh(const Region& r, const Aabb& box, int cells_long_axis) {
    const Grid g = make_grid(box, cells_long_axis);
    if (!g.ok()) return {};
    std::vector<uint8_t> in((size_t)g.num_samples());
    r.contains_many(g.at.data(), g.num_samples(), in.data());
    const std::vector<int32_t> lab(in.begin(), in.end());
    return extract(g, lab, 1, [&](const double* p) { return r.contains(p); });
}

std::vector<RegionMesh> label_boundary_meshes(const LabelField& f, const Aabb& box, int cells_long_axis,
                                              const std::atomic<bool>* cancel) {
    auto stop = [&] { return cancel && cancel->load(std::memory_order_relaxed); };
    const Grid g = make_grid(box, cells_long_axis);
    const int n = f.num_labels();
    std::vector<RegionMesh> out((size_t)std::max(0, n));
    if (!g.ok() || n <= 0) return out;
    std::vector<int32_t> lab((size_t)g.num_samples());
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < g.num_samples(); i++) lab[(size_t)i] = f.label(&g.at[(size_t)i * 3]);
    for (int k = 0; k < n; k++) {
        if (stop()) return {};
        out[(size_t)k] = extract(g, lab, k, [&](const double* p) { return f.label(p) == k; });
    }
    return out;
}

Aabb robust_bounds(const float* xyz, int64_t n) {
    Aabb box;
    if (n <= 0) return box;
    std::vector<float> v((size_t)n);
    for (int a = 0; a < 3; a++) {
        for (int64_t i = 0; i < n; i++) v[(size_t)i] = xyz[i * 3 + a];
        const size_t lo = (size_t)(0.01 * (double)(n - 1)), hi = (size_t)(0.99 * (double)(n - 1));
        std::nth_element(v.begin(), v.begin() + lo, v.end());
        const double l = v[lo];
        std::nth_element(v.begin(), v.begin() + hi, v.end());
        const double u = v[hi];
        const double pad = 0.1 * std::max(u - l, 1e-6);
        box.lo[a] = l - pad;
        box.hi[a] = u + pad;
    }
    return box;
}

}  // namespace spirula
