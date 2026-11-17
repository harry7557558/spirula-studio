// tsdf_sphere -- TSDF fusion of analytic depth maps of a unit sphere, seen by
// pinhole, fisheye and orthographic cameras around it, meshes back to the sphere: vertices
// within a voxel of it, every edge shared by two faces, faces wound outward.
// Host only, so both builds run it.
//
//   ./build_vulkan/tsdf_sphere

#include "core/CameraModel.h"
#include "core/SourcePath.h"
#include "data/CameraMath.h"
#include "mesh/Tsdf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <utility>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what, int line) {
    if (ok) return;
    std::printf("FAIL %s:%d  %s\n", SS_FILE, line, what);
    g_fail++;
}
#define CHECK(cond) check((cond), #cond, __LINE__)

using meshing::TsdfCamera;

// A camera at `eye` looking at the origin, CV axes (+Z forward, +Y down).
TsdfCamera look_at(const double eye[3], int model, int w, int h, double f) {
    double z[3] = {-eye[0], -eye[1], -eye[2]};
    const double zn = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    for (double& v : z) v /= zn;
    const double up[3] = {std::fabs(z[1]) > 0.9 ? 1.0 : 0.0, std::fabs(z[1]) > 0.9 ? 0.0 : 1.0, 0.0};
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
    c.model = model;
    c.distortion = (int)CameraDistortionType::None;
    return c;
}

// Ray distance to the unit sphere per pixel, 0 where the ray misses it.
std::vector<float> sphere_depth(const TsdfCamera& c) {
    std::vector<float> d((size_t)c.width * c.height, 0.0f);
    double eye[3];
    for (int k = 0; k < 3; ++k)
        eye[k] = -(c.viewmat[k] * c.viewmat[3] + c.viewmat[4 + k] * c.viewmat[7] +
                   c.viewmat[8 + k] * c.viewmat[11]);
    const bool ortho = c.model == (int)CameraModelType::ORTHOGRAPHIC;
    for (int i = 0; i < c.height; ++i)
        for (int j = 0; j < c.width; ++j) {
            const double u = (j + 0.5 - c.cx) / c.fx, v = (i + 0.5 - c.cy) / c.fy;
            // Orthographic: a +Z ray from the pixel's offset on the image plane.
            double r[3] = {0.0, 0.0, 1.0}, o[3] = {eye[0], eye[1], eye[2]};
            if (ortho) {
                for (int k = 0; k < 3; ++k) o[k] += c.viewmat[k] * u + c.viewmat[4 + k] * v;
            } else if (!camhost::generate_ray(u, v, c.model, c.distortion, c.dist, r)) {
                continue;
            }
            double w[3];
            for (int k = 0; k < 3; ++k)
                w[k] = c.viewmat[k] * r[0] + c.viewmat[4 + k] * r[1] + c.viewmat[8 + k] * r[2];
            const double b = o[0] * w[0] + o[1] * w[1] + o[2] * w[2];
            const double cc = o[0] * o[0] + o[1] * o[1] + o[2] * o[2] - 1.0;
            const double disc = b * b - cc;
            if (disc <= 0.0) continue;
            d[(size_t)i * c.width + j] = (float)(-b - std::sqrt(disc));
        }
    return d;
}

void test_sphere() {
    const float voxel = 0.02f, trunc = 4 * voxel;
    meshing::TsdfVolume vol(voxel, trunc);
    // The six axis directions and the eight cube corners, at distance 3.
    std::vector<std::vector<double>> dirs = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
                                             {0, 0, 1}, {0, 0, -1}};
    for (int s = 0; s < 8; ++s) dirs.push_back({s & 1 ? 1.0 : -1.0, s & 2 ? 1.0 : -1.0, s & 4 ? 1.0 : -1.0});
    int n = 0;
    for (auto& d : dirs) {
        const double norm = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const double eye[3] = {3 * d[0] / norm, 3 * d[1] / norm, 3 * d[2] / norm};
        // Alternate pinhole and fisheye, so both projections fold into one volume.
        const bool fish = (n++ % 2) == 1;
        const TsdfCamera c = look_at(eye, (int)(fish ? CameraModelType::FISHEYE : CameraModelType::PINHOLE),
                                     320, 320, 400.0);
        const std::vector<float> depth = sphere_depth(c);
        vol.integrate(c, depth.data(), 0.0f);
    }
    for (double side : {1.0, -1.0}) {
        const double eye[3] = {0.0, 0.0, 3.0 * side};
        const TsdfCamera c = look_at(eye, (int)CameraModelType::ORTHOGRAPHIC, 640, 640, 200.0);
        const std::vector<float> depth = sphere_depth(c);
        vol.integrate(c, depth.data(), 0.0f);
    }
    meshing::MeshData mesh;
    vol.extract(mesh);
    CHECK(mesh.V.size() > 1000);
    CHECK(mesh.F.size() > 2000);

    // Depth is read at the nearest pixel, as Open3D does: that, not the
    // interpolation, bounds the error (median 0.05 voxel, worst 0.6 here).
    std::vector<double> err;
    for (const auto& v : mesh.V)
        err.push_back(std::fabs(std::sqrt((double)v[0] * v[0] + (double)v[1] * v[1] +
                                          (double)v[2] * v[2]) - 1.0));
    std::sort(err.begin(), err.end());
    const double median = err[err.size() / 2], worst = err.back();
    std::printf("  %zu vertices, %zu faces, radial error median %.4f worst %.4f (voxel %.3f)\n",
                mesh.V.size(), mesh.F.size(), median, worst, voxel);
    CHECK(median < 0.1 * voxel);
    CHECK(worst < voxel);

    std::map<std::pair<int, int>, int> uses;
    int inward = 0;
    for (const auto& f : mesh.F) {
        for (int k = 0; k < 3; ++k) {
            const int a = f[k], b = f[(k + 1) % 3];
            uses[{std::min(a, b), std::max(a, b)}]++;
        }
        const auto &A = mesh.V[f[0]], &B = mesh.V[f[1]], &C = mesh.V[f[2]];
        const double e1[3] = {B[0] - A[0], B[1] - A[1], B[2] - A[2]};
        const double e2[3] = {C[0] - A[0], C[1] - A[1], C[2] - A[2]};
        const double nrm[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                               e1[0] * e2[1] - e1[1] * e2[0]};
        if (nrm[0] * (A[0] + B[0] + C[0]) + nrm[1] * (A[1] + B[1] + C[1]) +
                nrm[2] * (A[2] + B[2] + C[2]) < 0.0)
            inward += 0.5 * std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]) >
                      0.02 * voxel * voxel;
    }
    int open = 0;
    for (const auto& u : uses) open += u.second != 2;
    // A sliver's corner-centroid winding can flip; the finishing chain re-orients.
    std::printf("  edges not shared by exactly two faces: %d, faces past 2%% of a voxel "
                "face wound inward: %d\n", open, inward);
    CHECK(open == 0);
    CHECK(inward == 0);
}

// Two orthographic views alone: the caps they see, on the sphere. Without other
// views to average it, the nearest-pixel error stays near half a pixel (0.005).
void test_orthographic_alone() {
    const float voxel = 0.02f;
    meshing::TsdfVolume vol(voxel, 4 * voxel);
    for (double side : {1.0, -1.0}) {
        const double eye[3] = {0.0, 0.0, 3.0 * side};
        const TsdfCamera c = look_at(eye, (int)CameraModelType::ORTHOGRAPHIC, 640, 640, 200.0);
        const std::vector<float> depth = sphere_depth(c);
        vol.integrate(c, depth.data(), 0.0f);
    }
    meshing::MeshData mesh;
    vol.extract(mesh);
    std::vector<double> err;
    for (const auto& v : mesh.V)
        if (std::fabs(v[2]) > 0.5)   // away from the silhouette ring, which no view faces
            err.push_back(std::fabs(std::sqrt((double)v[0] * v[0] + (double)v[1] * v[1] +
                                              (double)v[2] * v[2]) - 1.0));
    std::sort(err.begin(), err.end());
    CHECK(err.size() > 1000);
    if (!err.empty()) CHECK(err[err.size() / 2] < 0.25 * voxel);
}

void test_depth_from_moments() {
    // Occupancy 1 - exp(-(c0 + c1 z)^2) is 1/2 at z = (sqrt(ln 2) - c0) / c1.
    const float m[9] = {0.9f, -2.0f, 1.0f,     // crosses at sqrt(ln 2) + 2
                        0.4f, -2.0f, 1.0f,     // never reaches 1/2
                        0.9f, 1.0f, 0.0f};     // no slope: no surface
    float d[3];
    meshing::moments_to_depth(m, 3, d);
    CHECK(std::fabs(d[0] - (std::sqrt(std::log(2.0f)) + 2.0f)) < 1e-5f);
    CHECK(d[1] == 0.0f);
    CHECK(d[2] == 0.0f);
}

}  // namespace

int main() {
    test_depth_from_moments();
    test_orthographic_alone();
    test_sphere();
    if (g_fail) {
        std::printf("tsdf_sphere: %d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("tsdf_sphere: OK\n");
    return 0;
}
