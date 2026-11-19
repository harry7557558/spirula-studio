// mesh_stages -- the finishing chain's patch-wise culling and slit filling
// (mesh/MeshStages.h) on grids whose answer is known.
//
//   ./build_vulkan/mesh_stages

#include "core/SourcePath.h"
#include "mesh/MeshCheck.h"
#include "mesh/MeshStages.h"

#include <cstdio>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what, int line) {
    if (ok) return;
    std::printf("FAIL %s:%d  %s\n", SS_FILE, line, what);
    g_fail++;
}
#define CHECK(cond) check((cond), #cond, __LINE__)

// An n x n vertex grid at x0, two triangles per cell; cells for which
// `hole(i, j)` holds are left out.
template <class Hole>
void add_grid(meshing::MeshData& m, int n, float x0, Hole hole) {
    const int base = (int)m.V.size();
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) m.V.push_back({x0 + (float)i, (float)j, 0.0f});
    for (int j = 0; j + 1 < n; ++j)
        for (int i = 0; i + 1 < n; ++i) {
            if (hole(i, j)) continue;
            const int a = base + j * n + i, b = a + 1, c = a + n, d = c + 1;
            m.F.push_back({a, b, d});
            m.F.push_back({a, d, c});
        }
}

void culling() {
    const int n = 24;
    meshing::MeshData m;
    add_grid(m, n, 0.0f, [](int, int) { return false; });
    add_grid(m, n, 100.0f, [](int, int) { return false; });   // a part nobody sees
    std::vector<unsigned char> vis(m.V.size(), 1);
    for (size_t v = (size_t)n * n; v < vis.size(); ++v) vis[v] = 0;
    auto at = [&](int i, int j) -> unsigned char& { return vis[(size_t)j * n + i]; };
    // a pit: four unseen vertices inside seen surface
    at(5, 5) = at(6, 5) = at(5, 6) = at(6, 6) = 0;
    // an unseen region with a speck of seen vertices in it
    for (int j = 10; j < 22; ++j)
        for (int i = 10; i < 22; ++i) at(i, j) = 0;
    at(15, 15) = at(16, 15) = 1;

    const long kept = meshing::despeckle_visibility(m, vis, 64);
    CHECK(kept == 4);
    CHECK(at(5, 5) == 1 && at(6, 6) == 1);
    CHECK(at(15, 15) == 0 && at(16, 15) == 0);
    CHECK(at(12, 12) == 0);
    CHECK(at(2, 2) == 1);
    bool far_seen = false;
    for (size_t v = (size_t)n * n; v < vis.size(); ++v) far_seen = far_seen || vis[v];
    CHECK(!far_seen);
}

void slit() {
    // A 42-edge slit one cell wide: over the edge limit, closed all the same.
    meshing::MeshData m;
    add_grid(m, 48, 0.0f, [](int i, int j) { return j == 23 && i >= 14 && i < 34; });
    meshing::MeshCheckLimits lim;
    lim.intersections = false;
    CHECK(meshing::check_mesh(m, lim).boundary_loops == 2);
    meshing::fill_holes(m, 0.0f, 4, false);
    const meshing::MeshCheckReport r = meshing::check_mesh(m, lim);
    CHECK(r.boundary_loops == 1);
    CHECK(r.nonmanifold_edges == 0 && r.misoriented_edges == 0 && r.folds == 0);

    // A square hole of the same length is not a slit and stays.
    meshing::MeshData q;
    add_grid(q, 24, 0.0f, [](int i, int j) { return i >= 8 && i < 15 && j >= 8 && j < 15; });
    meshing::fill_holes(q, 0.0f, 4, false);
    CHECK(meshing::check_mesh(q, lim).boundary_loops == 2);
}

}  // namespace

int main() {
    culling();
    slit();
    if (g_fail == 0) std::printf("mesh_stages: all passed\n");
    return g_fail == 0 ? 0 : 1;
}
