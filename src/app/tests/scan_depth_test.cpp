// scan_depth_test -- app/ScanDepth.h against surfaces whose depth is known: a
// plane in front of a pinhole camera (planar and ray depth, a normal facing
// it), a sparse near patch that the far plane must not show through, and a
// sphere around a panorama camera (ray depth, normals towards its centre).

#include "app/ScanDepth.h"
#include "core/CameraModel.h"
#include "data/CameraMath.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

void add(std::vector<double>& xyz, std::vector<uint8_t>& rgb, double x, double y, double z) {
    xyz.insert(xyz.end(), {x, y, z});
    rgb.insert(rgb.end(), {128, 128, 128});
}

}  // namespace

int main() {
    // Far from the origin, as a geo-referenced scan is.
    const double t[3] = {500000.0, 4000000.0, 30.0};
    std::vector<double> xyz;
    std::vector<uint8_t> rgb;
    for (int i = -600; i <= 600; i++)
        for (int j = -600; j <= 600; j++) add(xyz, rgb, t[0] + i * 0.01, t[1] + j * 0.01, t[2] + 5.0);
    // A 0.6 m patch at 2 m, 5 cm between points: sparse enough for the plane
    // behind to show through the gaps before filtering.
    for (int i = -6; i <= 6; i++)
        for (int j = -6; j <= 6; j++) add(xyz, rgb, t[0] + 1.0 + i * 0.05, t[1] + j * 0.05, t[2] + 2.0);
    const app::ScanCloud cloud = app::build_scan_cloud(xyz, rgb, 2.0);

    app::MapCamera cam;
    cam.model = (int)CameraModelType::PINHOLE;
    cam.width = cam.height = 200;
    cam.fx = cam.fy = 200;
    cam.cx = cam.cy = 100;
    const double c2w[12] = {1, 0, 0, t[0], 0, 1, 0, t[1], 0, 0, 1, t[2]};   // looks down +z
    std::copy(c2w, c2w + 12, cam.c2w);

    std::vector<float> depth, normal;
    app::render_depth_normal(cloud, cam, false, depth, normal);
    // The patch covers x in [0.7, 1.3] m and y in [-0.3, 0.3] m at z = 2:
    // columns 170..230 (the map ends at 200), rows 70..130.
    int plane_bad = 0, plane_n = 0, patch_leak = 0, normal_bad = 0;
    for (int y = 5; y < 195; y++)
        for (int x = 5; x < 195; x++) {
            const float d = depth[(size_t)y * 200 + x];
            const bool in_patch = x >= 174 && y >= 74 && y <= 126;
            if (in_patch) {
                patch_leak += d > 3.0f;
                continue;
            }
            if (x >= 163 && y >= 63 && y <= 137) continue;   // the patch's edge
            plane_n++;
            plane_bad += std::fabs(d - 5.0f) > 1e-3f;
            const float* n = &normal[((size_t)y * 200 + x) * 3];
            normal_bad += std::fabs(n[2] + 1.0f) > 1e-3f;
        }
    check(plane_bad == 0, "planar depth of the plane is 5 m everywhere (" +
                              std::to_string(plane_bad) + " of " + std::to_string(plane_n) + " off)");
    check(normal_bad == 0, "its normal faces the camera, (0, 0, -1)");
    check(patch_leak == 0, "the plane does not show through the sparse patch (" +
                               std::to_string(patch_leak) + " pixels leak)");

    app::render_depth_normal(cloud, cam, true, depth, normal);
    const double u = (10 + 0.5 - 100) / 200.0, v = (20 + 0.5 - 100) / 200.0;
    const double want = 5.0 * std::sqrt(1 + u * u + v * v);
    // Within the 2.5 cm a pixel spans there: the front-most point is the one
    // nearest the axis, a few millimetres short of the pixel centre's ray.
    check(std::fabs(depth[20 * 200 + 10] - want) < 1e-2,
          "ray depth off axis is 5 m / cos(angle)");

    // A sphere of 3 m around a panorama camera.
    xyz.clear();
    rgb.clear();
    const int n_sphere = 2000000;
    for (int i = 0; i < n_sphere; i++) {
        const double z = 1.0 - 2.0 * (i + 0.5) / n_sphere;
        const double r = std::sqrt(1.0 - z * z), a = i * 2.399963229728653;
        add(xyz, rgb, t[0] + 3 * r * std::cos(a), t[1] + 3 * r * std::sin(a), t[2] + 3 * z);
    }
    const app::ScanCloud sphere = app::build_scan_cloud(xyz, rgb, 2.0);
    app::MapCamera pano;
    pano.model = (int)CameraModelType::EQUIRECTANGULAR;
    pano.width = 400;
    pano.height = 200;
    pano.fx = pano.width / (2 * 3.14159265358979323846);
    pano.fy = pano.height / 3.14159265358979323846;
    pano.cx = 200;
    pano.cy = 100;
    std::copy(c2w, c2w + 12, pano.c2w);
    app::render_depth_normal(sphere, pano, true, depth, normal);
    int covered = 0, off = 0, facing = 0;
    for (int y = 10; y < 190; y++)
        for (int x = 0; x < 400; x++) {
            const size_t k = (size_t)y * 400 + x;
            if (!(depth[k] > 0)) continue;
            covered++;
            off += std::fabs(depth[k] - 3.0f) > 2e-3f;
            double ray[3];
            camhost::generate_ray((x + 0.5 - pano.cx) / pano.fx, (y + 0.5 - pano.cy) / pano.fy,
                                  pano.model, 0, nullptr, ray);
            const float* nn = &normal[k * 3];
            facing += nn[0] * ray[0] + nn[1] * ray[1] + nn[2] * ray[2] < -0.99;
        }
    check(covered == 180 * 400, "the panorama sees the sphere in every pixel");
    check(off == 0, "its ray depth is 3 m everywhere");
    check(facing > covered * 99 / 100, "its normals point back at the camera");

    std::printf(g_failures ? "\n%d FAILED\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}
