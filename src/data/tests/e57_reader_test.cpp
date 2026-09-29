// e57_reader_test -- data/E57Reader.h against a synthetic file whose every
// value is known: bit-packed fields of odd widths cut across packets, a pose,
// invalid points, colour limits, a blob that spans pages, and files that are
// not E57 or are cut short.

#include "data/E57Reader.h"
#include "data/tests/E57TestFile.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace e57 = spirula::e57;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

std::string write_file(const std::string& name, const std::string& bytes) {
    const fs::path p = fs::temp_directory_path() / name;
    std::ofstream(p, std::ios::binary) << bytes;
    return p.string();
}

bool throws(const std::string& path) {
    try {
        e57::Reader r(path);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> coord(-150.0, 150.0);
    std::uniform_int_distribution<int> byte(0, 255), ten(0, 1023), state(0, 9);

    e57test::Spec spec;
    for (int i = 0; i < 3000; i++) {
        e57test::TestPoint p;
        p.x = std::round(coord(rng) * 1e4) / 1e4;
        p.y = (double)(float)coord(rng);
        p.z = coord(rng);
        p.r = byte(rng); p.g = ten(rng); p.b = byte(rng);
        p.invalid = state(rng) == 0 ? 2 : 0;
        spec.points.push_back(p);
    }
    // 90 degrees about +z, then a translation.
    spec.scan_q[0] = std::sqrt(0.5); spec.scan_q[3] = std::sqrt(0.5);
    spec.scan_t[0] = 1000.0; spec.scan_t[1] = -2.0; spec.scan_t[2] = 0.5;
    for (int i = 0; i < 3000; i++) spec.blob.push_back((char)(i * 7 + 3));
    const double q[4] = {1, 0, 0, 0}, t[3] = {1, 2, 3};
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[cam]]></name>"
        "<associatedData3DGuid type=\"String\"><![CDATA[scan-0]]></associatedData3DGuid>" +
        e57test::pose_xml(q, t) +
        "<pinholeRepresentation type=\"Structure\">{BLOB}"
        "<imageWidth type=\"Integer\">640</imageWidth><imageHeight type=\"Integer\">480</imageHeight>"
        "<focalLength type=\"Float\">0.004</focalLength>"
        "<pixelWidth type=\"Float\">5e-06</pixelWidth><pixelHeight type=\"Float\">4e-06</pixelHeight>"
        "<principalPointX type=\"Float\">319.5</principalPointX>"
        "<principalPointY type=\"Float\"/></pinholeRepresentation>");
    spec.images.push_back(
        "<name type=\"String\"><![CDATA[pano]]></name>"
        "<sphericalRepresentation type=\"Structure\">{BLOB}"
        "<imageWidth type=\"Integer\">200</imageWidth><imageHeight type=\"Integer\">100</imageHeight>"
        "<pixelWidth type=\"Float\">0.031415926535897934</pixelWidth>"
        "<pixelHeight type=\"Float\">0.031415926535897934</pixelHeight>"
        "</sphericalRepresentation>");

    const std::string bytes = e57test::build(spec);
    const std::string path = write_file("e57_reader_test.e57", bytes);
    e57::Reader reader(path);

    check(reader.scans().size() == 1 && reader.images().size() == 2, "one scan, two images");
    const e57::Scan& scan = reader.scans()[0];
    check(scan.count == 3000 && reader.total_points() == 3000, "record count");
    check(scan.fields.size() == 7 && scan.fields[0].bits == 22 && scan.fields[4].bits == 10 &&
          scan.fields[6].bits == 2 && scan.fields[1].bits == 32, "bit widths from the prototype");
    check(scan.has_color && scan.color_hi[1] == 1023, "colour limits");

    std::vector<e57::Point> got;
    const bool whole = reader.read_points(scan, [&](const e57::Point* p, size_t n) {
        got.insert(got.end(), p, p + n);
    });
    std::vector<e57test::TestPoint> valid;
    for (const auto& p : spec.points) if (!p.invalid) valid.push_back(p);
    check(whole && got.size() == valid.size(), "invalid points dropped, the rest all read");
    double worst = 0;
    int color_bad = 0;
    for (size_t i = 0; i < std::min(got.size(), valid.size()); i++) {
        const e57test::TestPoint& p = valid[i];
        // R(90 deg about z) (x, y, z) = (-y, x, z)
        const double want[3] = {-p.y + 1000.0, p.x - 2.0, p.z + 0.5};
        for (int k = 0; k < 3; k++) worst = std::max(worst, std::fabs(got[i].xyz[k] - want[k]));
        const int g = (int)std::lround(p.g / 1023.0 * 255.0);
        if (got[i].rgb[0] != p.r || std::abs(got[i].rgb[1] - g) > 1 || got[i].rgb[2] != p.b)
            color_bad++;
    }
    check(worst < 1e-9, "positions, posed into the file frame (worst " + std::to_string(worst) + ")");
    check(color_bad == 0, "colours, green scaled from 0..1023");

    const e57::Image& pin = reader.images()[0];
    check(pin.projection == e57::Projection::Pinhole && pin.width == 640 && pin.height == 480 &&
          pin.focal_length == 0.004 && pin.principal_x == 319.5 && pin.principal_y == 0.0 &&
          pin.has_pose && pin.pose.t[2] == 3.0 && pin.scan_guid == "scan-0",
          "pinhole calibration, an empty element read as zero");
    check(reader.images()[1].projection == e57::Projection::Spherical &&
          !reader.images()[1].has_pose, "spherical image without a pose");
    const std::vector<uint8_t> blob = reader.read_blob(pin.jpeg);
    check(std::string(blob.begin(), blob.end()) == spec.blob, "blob across page boundaries");

    std::atomic<bool> stop{true};
    check(!reader.read_points(scan, [](const e57::Point*, size_t) {}, &stop), "cancel");

    check(throws(write_file("e57_reader_bad.e57", "not an e57 file at all, not even close...")),
          "a file that is not E57 throws");
    check(throws(write_file("e57_reader_cut.e57", bytes.substr(0, bytes.size() - 1024))),
          "a truncated file throws");

    std::printf(g_failures ? "\n%d FAILED\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}
