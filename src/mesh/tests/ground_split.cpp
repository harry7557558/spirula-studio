// ground_split -- a floor with a table on it, seen from above by pinhole
// cameras whose ground masks are 5% wrong: the vertex votes put the floor and
// the table top on their own sides, and the two parts add up to the mesh.
// Host only, so both builds run it.
//
//   ./build_vulkan/ground_split

#include "core/CameraModel.h"
#include "core/SourcePath.h"
#include "mesh/GroundSplit.h"

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

using meshing::MeshData;
using meshing::TsdfCamera;

constexpr double kFloor = 4.0, kTable = 1.0, kTableZ = 1.0;

// A camera at `eye` looking at `at`, CV axes (+Z forward, +Y down).
TsdfCamera look_at(const double eye[3], const double at[3], int w, int h, double f) {
    double z[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
    const double zn = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    for (double& v : z) v /= zn;
    const double up[3] = {0.0, 1.0, 0.0};
    double x[3] = {up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]};
    const double xn = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    for (double& v : x) v /= xn;
    const double y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    TsdfCamera c{};
    const double* rows[3] = {x, y, z};
    for (int r = 0; r < 3; ++r) {
        for (int k = 0; k < 3; ++k) c.viewmat[4 * r + k] = (float)rows[r][k];
        c.viewmat[4 * r + 3] = (float)-(rows[r][0] * eye[0] + rows[r][1] * eye[1] + rows[r][2] * eye[2]);
    }
    c.viewmat[15] = 1.0f;
    c.fx = c.fy = (float)f;
    c.cx = 0.5f * w;
    c.cy = 0.5f * h;
    c.width = w;
    c.height = h;
    c.model = (int)CameraModelType::PINHOLE;
    c.distortion = (int)CameraDistortionType::None;
    return c;
}

// A square [-half, half]^2 at height z, n x n vertices, wound to face +z.
void add_grid(MeshData& m, double half, double z, int n) {
    const int base = (int)m.V.size();
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            m.V.push_back({(float)(-half + 2.0 * half * i / (n - 1)),
                           (float)(-half + 2.0 * half * j / (n - 1)), (float)z});
            m.N.push_back({0.0f, 0.0f, 1.0f});
        }
    for (int j = 0; j + 1 < n; ++j)
        for (int i = 0; i + 1 < n; ++i) {
            const int a = base + j * n + i, b = a + 1, c = a + n, d = c + 1;
            m.F.push_back({a, b, d});
            m.F.push_back({a, d, c});
        }
}

// Ray distance and label per pixel: the table top first, then the floor.
void render(const TsdfCamera& c, const double eye[3], std::vector<float>& depth,
            std::vector<uint8_t>& label) {
    const long n = (long)c.width * c.height;
    depth.assign((size_t)n, 0.0f);
    label.assign((size_t)n, meshing::kGroundUnknown);
    for (long p = 0; p < n; ++p) {
        const double u = ((double)(p % c.width) + 0.5 - c.cx) / c.fx;
        const double v = ((double)(p / c.width) + 0.5 - c.cy) / c.fy;
        const double rn = std::sqrt(u * u + v * v + 1.0);
        const double rc[3] = {u / rn, v / rn, 1.0 / rn};
        double dir[3];
        for (int k = 0; k < 3; ++k)
            dir[k] = c.viewmat[k] * rc[0] + c.viewmat[4 + k] * rc[1] + c.viewmat[8 + k] * rc[2];
        if (dir[2] >= 0.0) continue;
        const double planes[2] = {kTableZ, 0.0}, halves[2] = {kTable, kFloor};
        for (int s = 0; s < 2; ++s) {
            const double t = (planes[s] - eye[2]) / dir[2];
            const double x = eye[0] + t * dir[0], y = eye[1] + t * dir[1];
            if (std::fabs(x) > halves[s] || std::fabs(y) > halves[s]) continue;
            depth[(size_t)p] = (float)t;
            const bool floor = s == 1;
            const bool wrong = (((uint32_t)p * 2654435761u) >> 16) % 100 < 5;
            label[(size_t)p] = (floor != wrong) ? meshing::kGroundYes : meshing::kGroundNo;
            break;
        }
    }
}

}  // namespace

int main() {
    MeshData mesh;
    add_grid(mesh, kFloor, 0.0, 81);
    const size_t floor_faces = mesh.F.size();
    add_grid(mesh, kTable, kTableZ, 21);

    std::vector<float> ground(mesh.V.size(), 0.0f), seen(mesh.V.size(), 0.0f);
    std::vector<float> depth;
    std::vector<uint8_t> label;
    const double at[3] = {0.0, 0.0, 0.0};
    for (int k = 0; k < 8; ++k) {
        const double a = 2.0 * 3.14159265358979 * k / 8;
        const double eye[3] = {3.0 * std::cos(a), 3.0 * std::sin(a), 6.0};
        const TsdfCamera c = look_at(eye, at, 320, 240, 200.0);
        render(c, eye, depth, label);
        meshing::vote_ground(c, depth.data(), label.data(), mesh, ground.data(), seen.data());
    }
    const std::vector<uint8_t> faces = meshing::ground_faces(mesh, ground, seen, 20);
    CHECK(faces.size() == mesh.F.size());

    size_t floor_ok = 0, floor_seen = 0, table_ok = 0;
    for (size_t t = 0; t < faces.size(); ++t) {
        const auto& f = mesh.F[t];
        if (t < floor_faces) {
            if (seen[(size_t)f[0]] + seen[(size_t)f[1]] + seen[(size_t)f[2]] == 0.0f) continue;
            floor_seen++;
            floor_ok += faces[t] == 1;
        } else {
            table_ok += faces[t] == 0;
        }
    }
    const size_t table_faces = mesh.F.size() - floor_faces;
    std::printf("floor %zu / %zu seen faces ground, table %zu / %zu not\n", floor_ok, floor_seen,
                table_ok, table_faces);
    CHECK(floor_seen > floor_faces / 2);
    CHECK(floor_ok >= floor_seen * 98 / 100);
    CHECK(table_ok >= table_faces * 98 / 100);

    // The table occludes the floor beneath it: no vote may reach through.
    for (size_t v = 0; v < (size_t)81 * 81; ++v) {
        const auto& p = mesh.V[v];
        if (std::fabs(p[0]) < 0.3f && std::fabs(p[1]) < 0.3f) CHECK(seen[v] == 0.0f);
    }

    const MeshData g = meshing::mesh_subset(mesh, faces, 1), r = meshing::mesh_subset(mesh, faces, 0);
    CHECK(g.F.size() + r.F.size() == mesh.F.size());
    CHECK(g.N.size() == g.V.size() && r.N.size() == r.V.size());
    for (const MeshData* part : {&g, &r})
        for (const auto& f : part->F)
            for (int k = 0; k < 3; ++k) CHECK(f[k] >= 0 && (size_t)f[k] < part->V.size());

    if (g_fail) {
        std::printf("ground_split: %d FAILED\n", g_fail);
        return 1;
    }
    std::printf("ground_split: OK\n");
    return 0;
}
