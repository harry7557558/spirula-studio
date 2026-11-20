// mesh_check -- the triangle-pair test and the whole-mesh checks behind
// `spirula mesh --check` on shapes whose answer is known.
//
//   ./build_vulkan/mesh_check

#include "core/SourcePath.h"
#include "mesh/MeshCheck.h"

#include <array>
#include <cmath>
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

using P = std::array<float, 3>;

int cross(const P& a0, const P& a1, const P& a2, const P& b0, const P& b1, const P& b2) {
    const P* a[3] = {&a0, &a1, &a2};
    const P* b[3] = {&b0, &b1, &b2};
    return meshing::triangles_cross(a, b);
}

void pairs() {
    const P o = {0, 0, 0}, x = {1, 0, 0}, y = {0, 1, 0}, z = {0, 0, 1};
    // apart, crossing, identical
    CHECK(cross(o, x, y, {0, 0, 2}, {1, 0, 2}, {0, 1, 2}) == 0);
    CHECK(cross(o, x, y, {0.2f, 0.2f, -1}, {0.2f, 0.2f, 1}, {0.2f, 1.5f, 0}) == 1);
    CHECK(cross(o, x, y, y, o, x) == 2);
    // shared edge: a hinge is fine, folded flat onto itself is not
    CHECK(cross(o, x, y, o, x, z) == 0);
    CHECK(cross(o, x, y, o, x, {0, -1, 0}) == 0);
    CHECK(cross(o, x, y, o, x, {0.5f, 0.5f, 0}) == 1);
    // shared corner: touching there is fine, passing through is not
    CHECK(cross(o, x, y, o, {-1, 0, 1}, {0, -1, 1}) == 0);
    CHECK(cross(o, x, y, o, {0.3f, 0.3f, 1}, {0.3f, 0.3f, -1}) == 1);
    CHECK(cross(o, x, y, o, {-1, 0, 0}, {0, -1, 0}) == 0);
    CHECK(cross(o, x, y, o, {1, 1, 0}, {-1, 2, 0}) == 1);
    // coplanar, no shared corner
    CHECK(cross(o, x, y, {0.1f, 0.1f, 0}, {0.6f, 0.1f, 0}, {0.1f, 0.6f, 0}) == 1);
    CHECK(cross(o, x, y, {1, 1, 0}, {2, 1, 0}, {1, 2, 0}) == 0);
    // a copy of a corner at the same position counts as shared
    const P o2 = o;
    CHECK(cross(o, x, y, o2, {-1, 0, 1}, {0, -1, 1}) == 0);
}

// An octahedron, wound outward.
meshing::MeshData octahedron() {
    meshing::MeshData m;
    m.V = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    m.F = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4},
           {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
    return m;
}

meshing::MeshCheckReport run(const meshing::MeshData& m) {
    meshing::MeshCheckLimits lim;
    return meshing::check_mesh(m, lim);
}

void meshes() {
    {
        const auto r = run(octahedron());
        CHECK(r.defects() == 0);
        CHECK(r.parts == 1 && r.boundary_edges == 0);
    }
    {   // inside out
        auto m = octahedron();
        for (auto& f : m.F) std::swap(f[1], f[2]);
        CHECK(run(m).inside_out_parts == 1);
    }
    {   // one face wound the other way
        auto m = octahedron();
        std::swap(m.F[0][1], m.F[0][2]);
        CHECK(run(m).misoriented_edges == 3);
    }
    {   // a copy of the first face, and a fourth face on one edge
        auto m = octahedron();
        m.F.push_back(m.F[0]);
        const auto r = run(m);
        CHECK(r.duplicate_faces == 1);
        CHECK(r.nonmanifold_edges == 3);
    }
    {   // two octahedra touching at one shared vertex: a bowtie
        auto m = octahedron();
        const int n = (int)m.V.size();
        for (int i = 0; i < 6; ++i) m.V.push_back({m.V[i][0] + 2, m.V[i][1], m.V[i][2]});
        for (int i = 0; i < 8; ++i) {
            std::array<int, 3> f = m.F[i];
            for (int& v : f) v = v == 1 ? 0 : v + n;
            m.F.push_back(f);
        }
        const auto r = run(m);
        CHECK(r.nonmanifold_vertices == 1);
        CHECK(r.intersecting_pairs == 0);
    }
    {   // two octahedra sunk into each other
        auto m = octahedron();
        const int n = (int)m.V.size();
        for (int i = 0; i < 6; ++i) m.V.push_back({m.V[i][0] + 1.2f, m.V[i][1] + 0.1f, m.V[i][2] + 0.05f});
        for (int i = 0; i < 8; ++i) m.F.push_back({m.F[i][0] + n, m.F[i][1] + n, m.F[i][2] + n});
        const auto r = run(m);
        CHECK(r.intersecting_pairs > 0);
        CHECK(r.parts == 2);
    }
    {   // a flat valence-3 vertex in a triangle split into three
        meshing::MeshData m;
        m.V = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0.3f, 0.3f, 0}, {0, 0, -1}};
        m.F = {{0, 1, 3}, {1, 2, 3}, {2, 0, 3}, {1, 0, 4}, {2, 1, 4}, {0, 2, 4}};
        const auto r = run(m);
        CHECK(r.low_valence == 1);
        CHECK(r.intersecting_pairs == 0);
    }
    {   // a needle, a zero-area face, a NaN, an unused vertex, a bad index
        meshing::MeshData m;
        m.V = {{0, 0, 0}, {1, 0, 0}, {0.5f, 0.001f, 0}, {2, 0, 0}, {NAN, 0, 0}, {5, 5, 5}};
        m.F = {{0, 1, 2}, {0, 1, 3}, {0, 1, 4}, {0, 1, 9}};
        const auto r = run(m);
        CHECK(r.small_angles == 1 && r.large_aspect == 1);
        CHECK(r.zero_area == 1);
        CHECK(r.nonfinite == 1 && r.bad_index == 1);
        CHECK(r.unused_vertices == 1);
    }
}

}  // namespace

int main() {
    pairs();
    meshes();
    if (g_fail == 0) std::printf("mesh_check: all passed\n");
    return g_fail == 0 ? 0 : 1;
}
