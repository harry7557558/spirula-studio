// lidar_align_test -- app/LidarAlign.h and data/PointCloudFile.h against a
// synthetic room whose similarity to the "reconstruction" is known: the anchor
// fit recovers it exactly (also from anchors on one line, where centres alone
// cannot), ICP recovers it from a few degrees and percent off, and LAS / PLY
// files written here read back with their colours and scanner stations.

#include "app/LidarAlign.h"
#include "data/PointCloudFile.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using sfm::Mat3;
using sfm::Vec3;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

Mat3 axis_angle(Vec3 a, double deg) {
    a = a.normalized();
    const double t = deg * 3.14159265358979323846 / 180.0;
    const Mat3 K = sfm::crossMatrix(a), K2 = sfm::mul(K, K);
    Mat3 R = sfm::mat3Identity();
    for (int i = 0; i < 9; i++) R[i] += std::sin(t) * K[i] + (1 - std::cos(t)) * K2[i];
    return R;
}

double rot_deg(const Mat3& A, const Mat3& B) {
    const Mat3 D = sfm::mul(sfm::transpose(A), B);
    return std::acos(std::fmax(-1.0, std::fmin(1.0, (D[0] + D[4] + D[8] - 1) / 2))) * 180.0 /
           3.14159265358979323846;
}

// A 10 x 8 x 3 m room with two boxes on the floor, 4 cm between points.
std::vector<Vec3> room() {
    std::vector<Vec3> p;
    const double s = 0.04;
    for (double x = 0; x <= 10; x += s)
        for (double y = 0; y <= 8; y += s) { p.push_back({x, y, 0}); p.push_back({x, y, 3}); }
    for (double z = 0; z <= 3; z += s) {
        for (double x = 0; x <= 10; x += s) { p.push_back({x, 0, z}); p.push_back({x, 8, z}); }
        for (double y = 0; y <= 8; y += s) { p.push_back({0, y, z}); p.push_back({10, y, z}); }
    }
    for (double u = 0; u <= 1; u += s)
        for (double v = 0; v <= 1; v += s) {
            p.push_back({2 + u, 2 + v, 1}); p.push_back({2 + u, 2, v}); p.push_back({2, 2 + u, v});
            p.push_back({7 + u * 0.5, 5 + v, 1.5}); p.push_back({7 + u * 0.5, 5, v * 1.5});
        }
    return p;
}

template <class T> void put(std::ofstream& f, T v) { f.write((const char*)&v, sizeof v); }

}  // namespace

int main() {
    const std::vector<Vec3> scan = room();
    // model -> scan: x_s = s R x_m + t
    sfm::Sim3 T;
    T.scale = 3.7;
    T.R = axis_angle({0.3, -0.5, 0.8}, 40);
    T.t = {5, -2, 1};
    const sfm::Sim3 inv = sfm::invertSim3(T);

    // ---- the anchor fit ----
    auto views_on = [&](const std::vector<Vec3>& where, app::lidar::ModelGeometry& g,
                        std::vector<app::lidar::Anchor>& anchors) {
        for (size_t i = 0; i < where.size(); i++) {
            app::lidar::Anchor a;
            a.name = "scan/" + std::to_string(i) + ".jpg";
            a.R = axis_angle({0.1 * i, 1, 0.2}, 25.0 * i);
            a.centre = where[i];
            anchors.push_back(a);
            app::lidar::ModelGeometry::View v;
            v.name = "scan/" + std::to_string(i) + ".png";   // the extension may differ
            v.R = sfm::mul(inv.R, a.R);
            v.centre = sfm::transformPoint(inv, a.centre);
            g.views.push_back(v);
        }
    };
    {
        app::lidar::ModelGeometry g;
        std::vector<app::lidar::Anchor> anchors;
        views_on({{2, 3, 1.5}, {5, 6, 1.4}, {8, 2, 1.6}, {4, 1, 1.5}, {6, 4, 1.5}}, g, anchors);
        // One anchor whose recorded pose is wrong by 20 degrees.
        anchors[3].R = sfm::mul(axis_angle({0, 0, 1}, 20), anchors[3].R);
        const app::lidar::AnchorFit f = app::lidar::fit_anchors(g, anchors);
        check(f.ok && f.registered == 5 && f.inliers == 4, "anchor fit: the wrong anchor is left out");
        check(std::fabs(f.T.scale / T.scale - 1) < 1e-9 && rot_deg(f.T.R, T.R) < 1e-6 &&
                  (f.T.t - T.t).norm() < 1e-6,
              "anchor fit: the similarity recovered exactly");
    }
    {
        app::lidar::ModelGeometry g;
        std::vector<app::lidar::Anchor> anchors;
        views_on({{1, 4, 1.5}, {3, 4, 1.5}, {5, 4, 1.5}, {7, 4, 1.5}}, g, anchors);
        const app::lidar::AnchorFit f = app::lidar::fit_anchors(g, anchors);
        check(f.scale_known && rot_deg(f.T.R, T.R) < 1e-6 && (f.T.t - T.t).norm() < 1e-6,
              "anchor fit: anchors on one line still fix the rotation about it");
    }
    {
        app::lidar::ModelGeometry g;
        std::vector<app::lidar::Anchor> anchors;
        views_on({{4, 4, 1.5}, {4, 4, 1.5}}, g, anchors);
        const app::lidar::AnchorFit f = app::lidar::fit_anchors(g, anchors);
        check(f.ok && !f.scale_known && rot_deg(f.T.R, T.R) < 1e-6,
              "anchor fit: one station gives the turn, not the scale");
    }

    // ---- ICP ----
    {
        std::vector<double> xyz;
        std::vector<uint8_t> rgb;
        for (const Vec3& p : scan) {
            xyz.insert(xyz.end(), {p.x + 1000.0, p.y + 2000.0, p.z});
            rgb.insert(rgb.end(), {128, 128, 128});
        }
        app::lidar::AlignCloud cloud = app::lidar::make_align_cloud(xyz, rgb, 100000);
        sfm::Sim3 Tw = T;
        Tw.t = Tw.t + Vec3{1000, 2000, 0};
        const sfm::Sim3 invw = sfm::invertSim3(Tw);
        // Model points: a sample of the room, 5 mm of noise in metres.
        app::lidar::ModelGeometry g;
        std::mt19937 rng(7);
        std::normal_distribution<double> noise(0, 0.005);
        for (size_t i = 0; i < scan.size(); i += 7) {
            const Vec3 p = scan[i] + Vec3{1000 + noise(rng), 2000 + noise(rng), noise(rng)};
            g.points.push_back(sfm::transformPoint(invw, p));
            g.track.push_back(5);
        }
        sfm::Sim3 start = Tw;
        start.scale *= 1.03;
        start.R = sfm::mul(axis_angle({1, 1, 0}, 3), start.R);
        start.t = start.t + Vec3{0.1, -0.08, 0.05};
        app::lidar::IcpStats st;
        const sfm::Sim3 got = app::lidar::refine_icp(g, cloud, start, &st);
        // Where a model point at the room's far corner lands, against the truth.
        const Vec3 corner = sfm::transformPoint(invw, {1010, 2008, 3});
        const double err = (sfm::transformPoint(got, corner) - sfm::transformPoint(Tw, corner)).norm();
        std::printf("     icp: %d iterations, scale %.5f (true %.5f), %.4f deg, corner %.4f m\n",
                    st.iterations, got.scale, Tw.scale, rot_deg(got.R, Tw.R), err);
        check(err < 0.01 && rot_deg(got.R, Tw.R) < 0.05, "ICP: recovered from 3 deg, 3%, 13 cm off");
    }

    // ---- LAS and PLY ----
    const fs::path dir = fs::temp_directory_path() / "lidar_align_test";
    fs::create_directories(dir);
    {
        // Format 2 (RGB), colours written as 8-bit values in 16-bit fields.
        const fs::path p = dir / "a.las";
        std::ofstream f(p, std::ios::binary);
        char h[227] = {};
        std::memcpy(h, "LASF", 4);
        h[24] = 1; h[25] = 2;
        const uint16_t hs = 227; std::memcpy(h + 94, &hs, 2);
        const uint32_t off = 227; std::memcpy(h + 96, &off, 4);
        h[104] = 2;
        const uint16_t rl = 26; std::memcpy(h + 105, &rl, 2);
        const uint32_t n = 3; std::memcpy(h + 107, &n, 4);
        const double sc[3] = {0.001, 0.001, 0.001}, of[3] = {500000, 4000000, 0};
        std::memcpy(h + 131, sc, 24);
        std::memcpy(h + 155, of, 24);
        f.write(h, sizeof h);
        for (int i = 0; i < 3; i++) {
            put<int32_t>(f, 1000 * i); put<int32_t>(f, -2000 * i); put<int32_t>(f, 500);
            put<uint16_t>(f, 7);
            for (int k = 0; k < 6; k++) put<uint8_t>(f, 0);
            put<uint16_t>(f, (uint16_t)(10 * i)); put<uint16_t>(f, 200); put<uint16_t>(f, 255);
        }
        f.close();
        spirula::cloud::Reader r(p.string());
        std::vector<spirula::e57::Point> pts;
        r.read([&](const spirula::e57::Point* q, size_t k) { pts.insert(pts.end(), q, q + k); });
        check(r.info().has_color && pts.size() == 3 &&
                  std::fabs(pts[2].xyz[0] - 500002.0) < 1e-9 &&
                  std::fabs(pts[2].xyz[1] - 3999996.0) < 1e-9 && pts[2].rgb[0] == 20 &&
                  pts[2].rgb[2] == 255,
              "LAS: offsets, scales and 8-bit colours in 16-bit fields");
    }
    {
        const fs::path p = dir / "b.ply";
        std::ofstream f(p, std::ios::binary);
        f << "ply\nformat binary_little_endian 1.0\nelement vertex 2\nproperty double x\n"
             "property double y\nproperty double z\nproperty uchar red\nproperty uchar green\n"
             "property uchar blue\nelement camera 2\n";
        const char* names[12] = {"view_px", "view_py", "view_pz", "x_axisx", "x_axisy", "x_axisz",
                                 "y_axisx", "y_axisy", "y_axisz", "z_axisx", "z_axisy", "z_axisz"};
        for (const char* nm : names) f << "property float " << nm << "\n";
        f << "end_header\n";
        for (int i = 0; i < 2; i++) {
            put<double>(f, 1.5 + i); put<double>(f, 2.5); put<double>(f, 3.5);
            put<uint8_t>(f, 1); put<uint8_t>(f, 2); put<uint8_t>(f, 3);
        }
        for (int c = 0; c < 2; c++) {
            const float v[12] = {(float)(4 + c), 5, 6, 1, 0, 0, 0, 1, 0, 0, 0, 1};
            f.write((const char*)v, sizeof v);
        }
        f.close();
        spirula::cloud::Reader r(p.string());
        std::vector<spirula::e57::Point> pts;
        r.read([&](const spirula::e57::Point* q, size_t k) { pts.insert(pts.end(), q, q + k); });
        check(pts.size() == 2 && pts[1].xyz[0] == 2.5 && pts[1].rgb[2] == 3 &&
                  r.info().stations.size() == 2 && r.info().stations[1].origin[0] == 5,
              "PLY: points, colours and both scanner stations");
    }
    fs::remove_all(dir);
    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}
