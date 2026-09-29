// e57_dataset_test -- app/E57Dataset.h end to end: a synthetic E57 file is
// written out as a dataset, read back by the trainer's own parser, and every
// camera must put a world point on the pixel the E57 file says it lands on.
// The E57 side below is the convention measured on real scanner files
// (docs/datasets.md "E57 laser scans"); the other side is the engine's.

#include "app/E57Dataset.h"
#include "app/ScanDepth.h"
#include "core/CameraModel.h"
#include "data/CameraMath.h"
#include "data/DatasetParser.h"
#include "data/SceneTransform.h"
#include "data/tests/E57TestFile.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr double kPi = 3.14159265358979323846;
int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

void rotation(const double q[4], double R[9]) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double n = std::sqrt(w*w + x*x + y*y + z*z);
    spirula::e57::Pose p;
    for (int i = 0; i < 4; i++) p.q[i] = q[i] / n;
    p.rotation(R);
}

// Local = R^T (p - t).
void to_local(const double R[9], const double t[3], const double p[3], double out[3]) {
    const double d[3] = {p[0] - t[0], p[1] - t[1], p[2] - t[2]};
    for (int c = 0; c < 3; c++) out[c] = R[c]*d[0] + R[3 + c]*d[1] + R[6 + c]*d[2];
}

struct Cam {
    double q[4], t[3];
};

uint8_t green8(int g10) { return (uint8_t)(g10 / 1023.0 * 255.0 + 0.5); }

// A room of coloured 1 m tiles: texture enough for the alignment check to tell
// a camera's true orientation from the other 23.
std::vector<e57test::TestPoint> tiled_room(std::mt19937& rng, int n) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    std::vector<e57test::TestPoint> pts;
    for (int i = 0; i < n; i++) {
        double p[3] = {-4 + 8 * u(rng), -4 + 8 * u(rng), -1.5 + 4 * u(rng)};
        const int face = (int)(u(rng) * 6);
        p[face / 2] = face / 2 == 2 ? (face % 2 ? 2.5 : -1.5) : (face % 2 ? 4.0 : -4.0);
        const int t = ((int)std::floor(p[0] + 8) * 7 + (int)std::floor(p[1] + 8) * 13 +
                       (int)std::floor(p[2] + 8) * 3 + face * 5) & 7;
        pts.push_back({std::round(p[0] * 1e4) / 1e4, (double)(float)p[1], p[2],
                       t & 1 ? 230 : 25, t & 2 ? 950 : 90, t & 4 ? 210 : 35, 0});
    }
    return pts;
}

// What the camera `R_gl` (OpenGL axes, columns) at `t` sees of the room, as PNG.
std::string render_png(const std::vector<e57test::TestPoint>& pts, const double R_gl[9],
                       const double t[3], int side, double f) {
    std::vector<double> xyz;
    std::vector<uint8_t> rgb;
    for (const auto& p : pts) {
        xyz.insert(xyz.end(), {p.x, p.y, p.z});
        rgb.insert(rgb.end(), {(uint8_t)p.r, green8(p.g), (uint8_t)p.b});
    }
    const app::ScanCloud cloud = app::build_scan_cloud(xyz, rgb, 2.0);
    app::MapCamera cam;
    cam.model = (int)CameraModelType::PINHOLE;
    cam.width = cam.height = side;
    cam.fx = cam.fy = f;
    cam.cx = cam.cy = side / 2.0;
    for (int r = 0; r < 3; r++) {
        cam.c2w[r * 4] = R_gl[r * 3];
        cam.c2w[r * 4 + 1] = -R_gl[r * 3 + 1];
        cam.c2w[r * 4 + 2] = -R_gl[r * 3 + 2];
        cam.c2w[r * 4 + 3] = t[r];
    }
    std::vector<uint32_t> index;
    std::vector<float> depth;
    app::render_front(cloud, cam, false, index, depth);
    std::vector<uint8_t> px((size_t)side * side * 3, 128);
    for (size_t k = 0; k < index.size(); k++)
        if (index[k] != UINT32_MAX)
            for (int c = 0; c < 3; c++) px[k * 3 + c] = cloud.rgb[(size_t)index[k] * 3 + c];
    std::string out;
    stbi_write_png_to_func(
        [](void* ctx, void* data, int size) {
            static_cast<std::string*>(ctx)->append(static_cast<const char*>(data), (size_t)size);
        },
        &out, side, side, 3, px.data(), side * 3);
    return out;
}

// One E57 of the room with one photo whose stored pose is `R_file`; what the
// converter made of it, and the camera the trainer then reads.
app::E57Check convert_room(const fs::path& dir, const std::vector<e57test::TestPoint>& room,
                           const std::string& png, const double R_file[9], const double t[3],
                           double R_out[9]) {
    e57test::Spec spec;
    spec.points = room;
    spec.blob = png;
    double q[4];
    spirula::rotation_to_quaternion(R_file, q);
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[room]]></name>" + e57test::pose_xml(q, t) +
        "<pinholeRepresentation type=\"Structure\">{PNGBLOB}"
        "<imageWidth type=\"Integer\">160</imageWidth><imageHeight type=\"Integer\">160</imageHeight>"
        "<focalLength type=\"Float\">0.01</focalLength>"
        "<pixelWidth type=\"Float\">0.0001</pixelWidth><pixelHeight type=\"Float\">0.0001</pixelHeight>"
        "<principalPointX type=\"Float\">79.5</principalPointX>"
        "<principalPointY type=\"Float\">79.5</principalPointY></pinholeRepresentation>");
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream((dir / "room.e57").string(), std::ios::binary) << e57test::build(spec);
    app::E57DatasetOptions opt;
    opt.input = (dir / "room.e57").string();
    opt.output = (dir / "dataset").string();
    opt.seed_points = 1000;
    opt.depth_maps = false;
    const app::E57DatasetResult res = app::write_e57_dataset(opt, [](const std::string&) {});
    DatasetParserConfig cfg;
    const ParsedDataset ds = parse_dataset(opt.output, cfg, "");
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) R_out[r * 3 + c] = ds.c2w[r * 4 + c];
    return res.check;
}

}  // namespace

int main() {
    const Cam pin{{0.9, 0.1, -0.3, 0.2}, {1.0, -0.5, 1.5}};
    const Cam pano{{0.7, 0.05, 0.1, -0.6}, {-0.8, 0.4, 1.2}};
    const double f = 0.004, pw = 5e-6, ph = 4e-6, ppx = 319.5, ppy = 239.5;
    const int pan_w = 200, pan_h = 100;

    e57test::Spec spec;
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> u(-5.0, 5.0);
    for (int i = 0; i < 4000; i++)
        spec.points.push_back({std::round(u(rng) * 1e4) / 1e4, (double)(float)u(rng), u(rng),
                               100, 400, 200, 0});
    spec.blob = "\xff\xd8 not really a JPEG \xff\xd9";
    auto num = [](double v) { return e57test::num(v); };
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[cam]]></name>" + e57test::pose_xml(pin.q, pin.t) +
        "<pinholeRepresentation type=\"Structure\">{BLOB}"
        "<imageWidth type=\"Integer\">640</imageWidth><imageHeight type=\"Integer\">480</imageHeight>"
        "<focalLength type=\"Float\">" + num(f) + "</focalLength>"
        "<pixelWidth type=\"Float\">" + num(pw) + "</pixelWidth>"
        "<pixelHeight type=\"Float\">" + num(ph) + "</pixelHeight>"
        "<principalPointX type=\"Float\">" + num(ppx) + "</principalPointX>"
        "<principalPointY type=\"Float\">" + num(ppy) + "</principalPointY>"
        "</pinholeRepresentation>");
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[pano]]></name>" + e57test::pose_xml(pano.q, pano.t) +
        "<sphericalRepresentation type=\"Structure\">{BLOB}"
        "<imageWidth type=\"Integer\">200</imageWidth><imageHeight type=\"Integer\">100</imageHeight>"
        "<pixelWidth type=\"Float\">" + num(2 * kPi / pan_w) + "</pixelWidth>"
        "<pixelHeight type=\"Float\">" + num(kPi / pan_h) + "</pixelHeight>"
        "</sphericalRepresentation>");
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[strip]]></name>" + e57test::pose_xml(pano.q, pano.t) +
        "<cylindricalRepresentation type=\"Structure\">{BLOB}"
        "<imageWidth type=\"Integer\">200</imageWidth><imageHeight type=\"Integer\">50</imageHeight>"
        "</cylindricalRepresentation>");
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[lost]]></name>"
        "<pinholeRepresentation type=\"Structure\">{BLOB}"
        "<imageWidth type=\"Integer\">640</imageWidth><imageHeight type=\"Integer\">480</imageHeight>"
        "</pinholeRepresentation>");

    const fs::path dir = fs::temp_directory_path() / "e57_dataset_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string e57_path = (dir / "scan.e57").string();
    std::ofstream(e57_path, std::ios::binary) << e57test::build(spec);

    app::E57DatasetOptions opt;
    opt.input = e57_path;
    opt.output = (dir / "dataset").string();
    opt.seed_points = 1000;
    std::vector<std::string> lines;
    const app::E57DatasetResult res =
        app::write_e57_dataset(opt, [&](const std::string& l) { lines.push_back(l); });
    check(res.images == 2, "the pinhole and the panorama become cameras, the rest are skipped");
    check(res.seed_points > 900 && res.seed_points <= 1000 && res.voxel > 0,
          "4000 points thinned to about 1000 (" + std::to_string(res.seed_points) + ")");

    bool refused = false;
    try {
        app::write_e57_dataset(opt, [](const std::string&) {});
    } catch (const std::runtime_error&) {
        refused = true;
    }
    check(refused, "a folder that is not empty needs --overwrite");
    opt.overwrite = true;
    check(app::write_e57_dataset(opt, [](const std::string&) {}).images == 2, "--overwrite");

    DatasetParserConfig cfg;
    const ParsedDataset ds = parse_dataset(opt.output, cfg, "");
    check(ds.num_cameras == 2, "the parser reads both cameras");
    check(ds.camera_models.size() == 2 &&
          ds.camera_models[0] == (int)CameraModelType::PINHOLE &&
          ds.camera_models[1] == (int)CameraModelType::EQUIRECTANGULAR, "camera models");
    check(ds.gauge_oriented && ds.gauge_metric, "gauge.txt: +Z up, metres");
    check(ds.points.num() == res.seed_points, "the seed cloud");

    // Every camera must send a world point to the pixel its E57 image has it at.
    const float zero[kCameraDistortionParams] = {};
    double worst[2] = {0, 0};
    int seen[2] = {0, 0};
    std::uniform_real_distribution<double> w(-4.0, 4.0);
    for (int j = 0; j < 2; j++) {
        const Cam& c = j == 0 ? pin : pano;
        double R[9];
        rotation(c.q, R);
        const float* m = &ds.c2w[j * 12];
        const double Rgl[9] = {m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]};
        const double tgl[3] = {m[3], m[7], m[11]};
        const float* k = &ds.intrins[j * 4];
        for (int i = 0; i < 2000; i++) {
            const double p[3] = {w(rng), w(rng), w(rng)};
            double e[3], g[3];
            to_local(R, c.t, p, e);
            to_local(Rgl, tgl, p, g);
            double want[2];
            if (j == 0) {
                if (e[2] > -0.5) continue;   // behind the camera: it looks down -z
                want[0] = ppx + 0.5 + f / pw * e[0] / -e[2];
                want[1] = ppy + 0.5 + f / ph * -e[1] / -e[2];
            } else {
                const double az = std::atan2(e[1], e[0]);
                if (std::fabs(az) > 3.0) continue;   // stay off the seam
                const double el = std::atan2(e[2], std::hypot(e[0], e[1]));
                want[0] = pan_w / 2.0 - az / (2 * kPi / pan_w);
                want[1] = pan_h / 2.0 - el / (kPi / pan_h);
            }
            const double cv[3] = {g[0], -g[1], -g[2]};
            double uv[2];
            if (!camhost::project_ray(cv, ds.camera_models[j], (int)CameraDistortionType::None,
                                      zero, uv))
                continue;
            const double got[2] = {k[0] * uv[0] + k[2], k[1] * uv[1] + k[3]};
            worst[j] = std::max({worst[j], std::fabs(got[0] - want[0]),
                                 std::fabs(got[1] - want[1])});
            seen[j]++;
        }
    }
    check(seen[0] > 100 && worst[0] < 1e-2,
          "pinhole: engine pixel == E57 pixel (worst " + std::to_string(worst[0]) + " px)");
    check(seen[1] > 1000 && worst[1] < 1e-3,
          "panorama: engine pixel == E57 pixel (worst " + std::to_string(worst[1]) + " px)");

    std::vector<double> xyz = {0, 0, 0, 1, 1, 1};
    std::vector<uint8_t> rgb = {10, 20, 30, 40, 50, 60};
    check(app::thin_to_voxels(xyz, rgb, 2) == 0.0 && xyz.size() == 6,
          "a cloud already under the target is left alone");
    xyz.assign(8 * 3, 2.5);
    rgb.assign(8 * 3, 0);
    for (int i = 0; i < 8; i++) rgb[i * 3] = (uint8_t)(i * 10);
    xyz.insert(xyz.end(), {9, 9, 9});
    rgb.insert(rgb.end(), {1, 2, 3});
    app::thin_to_voxels(xyz, rgb, 2);
    check(xyz.size() == 6 && (rgb[0] == 35 || rgb[3] == 35),
          "coincident points merge into their mean");

    // A camera looking across the room, 0.7 rad of yaw and a little down.
    {
        std::mt19937 room_rng(5);
        const std::vector<e57test::TestPoint> room = tiled_room(room_rng, 300000);
        const double yaw = 0.7, pitch = -0.2;
        const double fwd[3] = {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw),
                               std::sin(pitch)};
        double right[3] = {fwd[1], -fwd[0], 0.0};
        const double rl = std::hypot(right[0], right[1]);
        for (double& v : right) v /= rl;
        const double up[3] = {right[1] * fwd[2] - right[2] * fwd[1],
                              right[2] * fwd[0] - right[0] * fwd[2],
                              right[0] * fwd[1] - right[1] * fwd[0]};
        const double R_true[9] = {right[0], up[0], -fwd[0], right[1], up[1], -fwd[1],
                                  right[2], up[2], -fwd[2]};
        const double t_cam[3] = {0.3, -0.2, 0.4};
        const std::string png = render_png(room, R_true, t_cam, 160, 100.0);
        // The same camera stored with OpenCV's axes, as an exporter that got the
        // convention wrong would: y and z turned half way round.
        const double R_cv[9] = {R_true[0], -R_true[1], -R_true[2], R_true[3], -R_true[4],
                                -R_true[5], R_true[6], -R_true[7], -R_true[8]};
        double R_got[9];
        auto worst = [&](const double* a) {
            double w = 0;
            for (int k = 0; k < 9; k++) w = std::max(w, std::fabs(a[k] - R_true[k]));
            return w;
        };
        const app::E57Check right_way = convert_room(dir / "room_ok", room, png, R_true, t_cam, R_got);
        check(right_way == app::E57Check::Agree && worst(R_got) < 1e-5,
              "alignment check: a correctly posed photo is left alone");
        const app::E57Check wrong_way = convert_room(dir / "room_cv", room, png, R_cv, t_cam, R_got);
        check(wrong_way == app::E57Check::Fixed && worst(R_got) < 1e-5,
              "alignment check: a photo posed with the wrong axes is turned back");
    }

    fs::remove_all(dir);
    std::printf(g_failures ? "\n%d FAILED\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}
