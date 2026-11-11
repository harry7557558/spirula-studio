// A synthetic XGRIDS capture written byte for byte, then read back through
// xbin::Reader and cloud::Reader: the container, the calibration chain and
// the per-point deskew against values worked out by hand, and each device,
// channel, calibration and frame the reader must refuse.

#include "data/PointCloudFile.h"
#include "data/XbinReader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

namespace {
int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "ok" : "FAIL", name);
    if (!ok) ++failures;
}

bool close_to(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }

void varint(Bytes& b, uint64_t v) {
    while (v >= 0x80) {
        b.push_back((uint8_t)(v | 0x80));
        v >>= 7;
    }
    b.push_back((uint8_t)v);
}
uint64_t zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
void field(Bytes& b, uint32_t f, uint64_t v) { varint(b, (uint64_t)f << 3); varint(b, v); }
void field(Bytes& b, uint32_t f, const Bytes& m) {
    varint(b, (uint64_t)f << 3 | 2);
    varint(b, m.size());
    b.insert(b.end(), m.begin(), m.end());
}
Bytes text(const std::string& s) { return Bytes(s.begin(), s.end()); }
template <class T> void put(Bytes& b, T v) {
    uint8_t x[sizeof v];
    std::memcpy(x, &v, sizeof v);
    b.insert(b.end(), x, x + sizeof v);
}

// One sequence of literals only, which is a valid LZ4 block.
Bytes lz4_literals(const Bytes& raw) {
    Bytes out;
    size_t n = raw.size();
    out.push_back((uint8_t)(std::min<size_t>(n, 15) << 4));
    if (n >= 15) {
        for (n -= 15; n >= 255; n -= 255) out.push_back(255);
        out.push_back((uint8_t)n);
    }
    out.insert(out.end(), raw.begin(), raw.end());
    return out;
}

Bytes file_message(const std::string& name, const std::string& body) {
    Bytes m;
    field(m, 2, text(name));
    field(m, 3, text(body));
    return m;
}

const char* kCameraYaml =
    "camera_0:\n  camera_pose:\n"
    "    - 1\n    - 0\n    - 0\n    - 0\n    - 0\n    - 1\n    - 0\n    - 0\n"
    "    - 0\n    - 0\n    - 1\n    - 0\n    - 0\n    - 0\n    - 0\n    - 1\n"
    "  image_height: 30\n  image_width: 40\n  camera_model: kb4\n"
    "  intrinsic:\n    - 10\n    - 11\n    - 20\n    - 15\n"
    "  distortion:\n    - 0.1\n    - 0.2\n    - 0.3\n    - 0.4\n"
    "camera_1:\n  calibrated: true\n  camera_pose: [1, 0, 0, 0.5, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n"
    "  image_height: 30\n  image_width: 40\n  camera_model: kb4\n"
    "  intrinsic: [10, 11, 20, 15]\n  distortion: [0, 0, 0, 0]\n"
    "camera_2:\n  camera_pose: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n"
    "  image_height: 30\n  image_width: 40\n  camera_model: pinhole\n"
    "  intrinsic: [30, 31, 20, 15]\n  distortion: [0.01, 0.02, 0.03, 0.04]\n"
    "camera_3:\n  camera_pose: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n"
    "  image_height: 30\n  image_width: 40\n  camera_model: pinhole\n"
    "  intrinsic: [30, 31, 20, 15]\n  distortion: [0, 0, 0, 0]\n"
    "calibrated: true\nversion: V3.1.1\n";

// The IMU 1 cm ahead of the LiDAR; camera_0 10 cm above it, written as the
// device writes it: LiDAR -> camera.
const char* kImuLidar = "transform: [1, 0, 0, 0.01, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n";
const char* kCameraLidar =
    "transform:\n  - 1\n  - 0\n  - 0\n  - 0\n  - 0\n  - 1\n  - 0\n  - 0\n"
    "  - 0\n  - 0\n  - 1\n  - -0.1\n  - 0\n  - 0\n  - 0\n  - 1\n";
// One metre along +x in one second, no turn.
const char* kPoses = "1.000000,0,0,0,0,0,0,1\n2.000000,1,0,0,0,0,0,1\n";
// Standing still, turned 90 degrees about x, then 91 more about its own z in one
// second: body z is world -y, so a turn applied in the wrong frame shows.
const char* kTurn = "1.000000,0,0,0,0.70710678118654752,0,0,0.70710678118654752\n"
                    "2.000000,0,0,0,0.49561769378289866,-0.50434422928127265,0.50434422928127276,0.49561769378289877\n";
// The same, its second quaternion negated.
const char* kTurnFlipped = "1.000000,0,0,0,0.70710678118654752,0,0,0.70710678118654752\n"
                           "2.000000,0,0,0,-0.49561769378289866,0.50434422928127265,-0.50434422928127276,-0.49561769378289877\n";

Bytes point(int64_t x_mm, int64_t y_mm, int64_t z_mm, int intensity, int64_t dt_us) {
    Bytes p;
    field(p, 1, zz(x_mm));
    field(p, 2, zz(y_mm));
    field(p, 3, zz(z_mm));
    field(p, 4, (uint64_t)intensity);
    field(p, 6, zz(dt_us));
    return p;
}

// 200 Hz from 0.9 s to 2.1 s, about +z: `fast` rad/s before 1.5 s, `slow` after,
// in counts of 1e-4 rad/s with a bias of 0.01 rad/s, which the gap's spread removes.
std::vector<Bytes> gyro_messages(double fast, double slow) {
    std::vector<Bytes> out;
    for (int64_t t = 900000; t <= 2100000; t += 5000) {
        Bytes m, head;
        field(head, 1, (uint64_t)out.size());
        field(head, 2, zz(t));
        field(m, 1, head);
        field(m, 4, zz(-98100));
        field(m, 7, zz(std::llround(((t < 1500000 ? fast : slow) + 0.01) / 1e-4)));
        out.push_back(m);
    }
    return out;
}

// A capture's device and the faults written into it; the defaults are a PortalCam's.
struct Layout {
    std::string code = "AA";     // "" writes no device message
    std::string model = "PortalCam";
    uint32_t extra_channel = 0;
    uint32_t missing_channel = 0;                  // a camera channel left out
    std::string extra_yaml;
    std::string missing_section;                   // a camera.yaml section left out
    int64_t frame_width = 40, frame_height = 30;   // 0 leaves the field out
    uint32_t device_overrun = 0;                   // bytes the device length claims past its message
    bool device_truncated = false;                 // its message ends inside a field
    bool index_lost = false;                       // the header's index offset is 0
    bool code_written = true;                      // field 7 of the device message
    uint64_t first_chunk = 0;                      // written over the real offset when set
    size_t cut_at = 0;                             // the file ends after this many bytes
    bool frame_predicted = false;                  // camera_3's frame a P slice, no IDR
};

Bytes capture_bytes(const char* poses, const std::vector<Bytes>& imu, const Layout& layout) {
    std::map<uint32_t, std::vector<Bytes>> channels;
    std::string yaml = kCameraYaml + layout.extra_yaml;
    if (!layout.missing_section.empty()) {
        const size_t from = yaml.find(layout.missing_section + ":");
        yaml.erase(from, yaml.find("calibrated: true\nversion") - from);
    }
    channels[0] = {file_message("camera.yaml", yaml),
                   file_message("extrinsic_imu_lidar.yaml", kImuLidar),
                   file_message("extrinsic_camera_lidar.yaml", kCameraLidar)};
    if (!imu.empty()) channels[2] = imu;
    channels[42] = {file_message("gnss.csv", "#timestamp\n"), file_message("poses.csv", poses)};
    {
        Bytes sweep, head;
        field(head, 1, 1);
        field(head, 2, zz(1500000));
        field(sweep, 1, head);
        field(sweep, 2, point(2000, 0, 0, 90, 0));          // kept
        field(sweep, 2, point(100, 0, 0, 90, 0));           // device's own return
        field(sweep, 2, point(0, 3000, 0, 300, 100000));    // kept, 0.1 s later
        field(sweep, 2, point(3000, 0, 0, 90, 600000));     // past the trajectory
        channels[6] = {sweep};
    }
    for (uint32_t ch : {28u, 32u, 36u, 40u}) {
        if (ch == layout.missing_channel) continue;
        Bytes frame, head;
        field(head, 1, 1);
        field(head, 2, zz(1250000 + ch));
        field(frame, 1, head);
        field(frame, 2, 3);
        if (layout.frame_width) field(frame, 3, (uint64_t)layout.frame_width);
        if (layout.frame_height) field(frame, 4, (uint64_t)layout.frame_height);
        // SPS, PPS and an IDR slice; the last byte tells the channels apart.
        const uint8_t slice = layout.frame_predicted && ch == 40 ? 0x41 : 0x65;
        field(frame, 5, Bytes{0, 0, 0, 1, 0x67, 0, 0, 0, 1, 0x68, 0, 0, 0, 1, slice, (uint8_t)ch});
        channels[ch] = {frame};
    }
    if (layout.extra_channel) channels[layout.extra_channel] = {Bytes{0x08, 0x01}};

    Bytes file;
    file.insert(file.end(), {'X', 'B', 'A', 'G'});
    put<uint16_t>(file, 512);
    put<uint16_t>(file, 1);
    put<uint64_t>(file, 1000000);
    put<uint64_t>(file, 0);   // the first chunk's offset, patched below
    put<uint64_t>(file, 0);   // the index offset, patched below
    if (!layout.code.empty()) {
        Bytes device;
        field(device, 1, text("X000000000"));
        field(device, 2, text("V3.2.5"));
        field(device, 3, text("V3.2.5-20260402.131505"));
        field(device, 6, text(layout.model));
        if (layout.code_written) field(device, 7, text(layout.code));
        field(device, 8, text("rs_airy"));
        if (layout.device_truncated) device.insert(device.end(), {0x0a, 0x05, 'A'});
        put<uint32_t>(file, (uint32_t)device.size() + layout.device_overrun);
        file.insert(file.end(), device.begin(), device.end());
        // A real capture has 4311 bytes here that are no protobuf.
        file.insert(file.end(), 16, 0xbb);
    }
    const uint64_t first_chunk = file.size();
    std::memcpy(&file[16], layout.first_chunk ? &layout.first_chunk : &first_chunk, 8);
    Bytes index;
    for (const auto& [ch, messages] : channels) {
        Bytes payload;
        for (const Bytes& m : messages) field(payload, 1, m);
        const uint64_t at = file.size();
        put<uint32_t>(file, (uint32_t)(payload.size() + 52));
        put<uint16_t>(file, 1);
        put<uint16_t>(file, 36);
        put<uint64_t>(file, 1000000);
        put<uint32_t>(file, (uint32_t)payload.size());
        put<uint32_t>(file, (uint32_t)messages.size());
        put<uint64_t>(file, 1000000);
        put<uint64_t>(file, 2000000);
        put<uint32_t>(file, 0);
        put<uint32_t>(file, (uint32_t)payload.size());
        file.insert(file.end(), payload.begin(), payload.end());
        Bytes entry, offsets;
        if (ch) field(entry, 1, ch);
        field(entry, 2, messages.size());
        field(entry, 3, payload.size());
        // The time range of the channel, wider than its one frame.
        if (ch >= 28 && ch <= 40) {
            field(entry, 4, zz(1250000 + ch));
            field(entry, 5, zz(3250000 + ch));
        }
        varint(offsets, at);
        field(entry, 6, offsets);
        field(index, 1, entry);
    }
    const uint64_t index_at = layout.index_lost ? 0 : file.size();
    std::memcpy(&file[24], &index_at, 8);
    const Bytes packed = lz4_literals(index);
    put<uint32_t>(file, (uint32_t)(packed.size() + 28));
    put<uint16_t>(file, 255);
    put<uint16_t>(file, 12);
    put<uint64_t>(file, 2000000);
    put<uint32_t>(file, (uint32_t)index.size());
    put<uint32_t>(file, (uint32_t)packed.size());
    file.insert(file.end(), packed.begin(), packed.end());
    if (layout.cut_at) file.resize(layout.cut_at);
    return file;
}

fs::path write_capture(const fs::path& dir, const char* poses = kPoses,
                       const std::vector<Bytes>& imu = {}, const Layout& layout = {}) {
    const Bytes file = capture_bytes(poses, imu, layout);
    fs::create_directories(dir);
    const fs::path path = dir / "capture.xbin";
    std::ofstream(path, std::ios::binary).write((const char*)file.data(), (std::streamsize)file.size());
    return path;
}

}  // namespace

// Its own function so that every reader has closed the file before it is
// deleted: Windows refuses to remove an open file.
void check_capture(const fs::path& dir, const fs::path& path) {
    spirula::xbin::Reader r(path.string());
    const auto& cams = r.cameras();
    check(cams.size() == 4, "four cameras");
    check(cams.size() == 4 && cams[0].name == "camera_0" && cams[1].name == "camera_1" &&
              cams[2].name == "camera_2" && cams[3].name == "camera_3",
          "cameras in calibration order");
    check(cams.size() == 4 && cams[0].fisheye && cams[1].fisheye && !cams[2].fisheye,
          "kb4 is the fisheye");
    check(cams.size() == 4 && close_to(cams[0].params[1], 11) && close_to(cams[0].params[7], 0.4) &&
              cams[0].width == 40 && cams[0].height == 30 && cams[0].frames == 1,
          "block-list calibration");
    check(cams.size() == 4 && close_to(cams[2].params[0], 30) && close_to(cams[2].params[5], 0.02),
          "flow-list calibration");
    check(cams.size() == 4 && close_to(cams[0].seconds, 2.0) && close_to(cams[3].seconds, 2.0),
          "camera time range from the index");
    check(r.sweeps() == 1, "one sweep");

    std::vector<spirula::e57::Point> pts;
    check(r.read_points([&](const spirula::e57::Point* p, size_t n) { pts.insert(pts.end(), p, p + n); }),
          "points read");
    check(pts.size() == 2, "the near return and the point past the trajectory are dropped");
    if (pts.size() == 2) {
        // x = 2 m + 1 cm IMU offset + 0.5 m travelled by t = 1.5 s.
        check(close_to(pts[0].xyz[0], 2.51) && close_to(pts[0].xyz[1], 0) && close_to(pts[0].xyz[2], 0),
              "point placed at the sweep's time");
        check(close_to(pts[1].xyz[0], 0.61) && close_to(pts[1].xyz[1], 3.0), "point placed at its own time");
        check(pts[0].rgb[0] == 90 && pts[1].rgb[0] == 255, "intensity grey, clamped");
    }

    double R[9], c[3];
    check(r.camera_pose(0, 1250000, R, c), "camera_0 posed inside the trajectory");
    // centre = 0.25 m travelled + 1 cm IMU offset, 10 cm up.
    check(close_to(c[0], 0.26) && close_to(c[1], 0) && close_to(c[2], 0.1) && close_to(R[0], 1) && close_to(R[4], 1),
          "LiDAR -> camera file read as its inverse");
    check(r.camera_pose(1, 1250000, R, c) && close_to(c[0], 0.76), "camera_pose is camera -> camera_0");
    check(!r.camera_pose(0, 500000, R, c), "no pose before the trajectory");

    std::vector<spirula::xbin::Frame> frames;
    r.read_frames(2, [&](const spirula::xbin::Frame& f) { frames.push_back(f); return true; });
    check(frames.size() == 1 && frames[0].time_us == 1250032 && frames[0].h264.size() == 16 &&
              frames[0].h264[15] == 32,
          "camera_2 is channel 32");

    spirula::cloud::Reader cr(path.string());
    check(cr.info().format == spirula::cloud::Format::Xbin && cr.xbin() && !cr.info().has_color &&
              cr.info().stations.empty(),
          "cloud::Reader opens the capture");
    size_t n = 0;
    check(cr.read([&](const spirula::e57::Point*, size_t k) { n += k; }) && n == 2,
          "cloud::Reader streams the same points");
    check(spirula::cloud::is_cloud_path("a/B.XBIN"), "is_cloud_path knows .xbin");
    check(spirula::xbin::capture_in_folder(dir.string()) == path.string(), "capture found in its folder");

    {
        std::ofstream(dir / "bad.xbin", std::ios::binary) << "XBAG";
        bool threw = false;
        try {
            spirula::xbin::Reader bad((dir / "bad.xbin").string());
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "a truncated capture is refused");
        check(spirula::xbin::capture_in_folder(dir.string()).empty(), "two captures in one folder: none");
    }
}

// Degrees between camera_0 at `t_us`, whose rotation is the IMU's here, and
// 90 degrees about x followed by `about_z` degrees about the body's z.
double off_at(const fs::path& path, int64_t t_us, double about_z) {
    spirula::xbin::Reader r(path.string());
    double R[9], c[3];
    if (!r.camera_pose(0, t_us, R, c)) return 180;
    const double z = about_z * 3.14159265358979323846 / 180.0;
    // Rx(90) * Rz(z), row-major.
    const double E[9] = {std::cos(z), -std::sin(z), 0, 0, 0, -1, std::sin(z), std::cos(z), 0};
    double tr = 0;
    for (int k = 0; k < 9; k++) tr += E[k] * R[k];
    return std::acos(std::clamp((tr - 1) / 2, -1.0, 1.0)) * 180.0 / 3.14159265358979323846;
}

void check_gyro(const fs::path& dir) {
    const double deg = 3.14159265358979323846 / 180.0;
    // 135 deg/s for half a second, then 45, the trapezoid across the step losing
    // 0.225, plus the 0.573 deg/s bias: 67.56 by 1.5 s, 90.35 by 2 s. Half the 0.65
    // gap to the pose comes on top at 1.5 s; the poses interpolated give 45.5.
    const std::vector<Bytes> imu = gyro_messages(135 * deg, 45 * deg);
    const fs::path turn = write_capture(dir / "turn", kTurn, imu);
    check(off_at(turn, 1500000, 67.8875) < 0.02, "gyro fills the turn in the body frame, its gap spread by time");
    // Half way from the interval's last sample (1.995 s, 90.769 deg) to the pose.
    const fs::path flip = write_capture(dir / "flip", kTurnFlipped, imu);
    check(off_at(flip, 1500000, 67.8875) < 0.02 && off_at(flip, 1997500, 90.8844) < 0.01,
          "a pose quaternion of the other sign turns the same way");
    // Three times the poses' turn: this gyro does not belong to the trajectory.
    check(off_at(write_capture(dir / "wrong", kTurn, gyro_messages(405 * deg, 135 * deg)), 1500000, 45.5) < 1e-4,
          "a gyro far off the poses is not used");
    check(off_at(write_capture(dir / "none", kTurn), 1500000, 45.5) < 1e-4, "no IMU: the poses interpolated");
    // From 1.005 s, or up to 1.995 s: the interval is not covered.
    const std::vector<Bytes> late(imu.begin() + 21, imu.end()), early(imu.begin(), imu.end() - 21);
    check(off_at(write_capture(dir / "late", kTurn, late), 1500000, 45.5) < 1e-4 &&
              off_at(write_capture(dir / "early", kTurn, early), 1500000, 45.5) < 1e-4,
          "a gyro that starts or ends inside the interval is not used");
    std::vector<Bytes> broken = imu;
    broken.push_back(Bytes{0x08});   // a key without its value
    check(off_at(write_capture(dir / "broken", kTurn, broken), 1500000, 45.5) < 1e-4,
          "an IMU channel that does not read: the poses interpolated");
}

// The reader's error for `path`, read through to its frames; "" when it reads.
std::string refusal(const fs::path& path) {
    try {
        spirula::xbin::Reader r(path.string());
        for (size_t k = 0; k < r.cameras().size(); k++) r.read_frames(k, [](const spirula::xbin::Frame&) { return true; });
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    return "";
}

void check_refusals(const fs::path& dir) {
    Layout k1;
    k1.code = "XX";
    k1.model = "Lixel K1";
    const std::string why = refusal(write_capture(dir / "k1", kPoses, {}, k1));
    check(why.find("camera layout of an XGRIDS Lixel K1 (device code XX, firmware V3.2.5) is not known") !=
              std::string::npos,
          "a device whose camera layout was not measured is refused, by name");
    Layout none;
    none.code = "";
    check(refusal(write_capture(dir / "anonymous", kPoses, {}, none)).find("names no XGRIDS device") != std::string::npos,
          "a header naming no device is refused");
    Layout overrun;
    overrun.device_overrun = 17;   // one byte more than the padding holds
    check(refusal(write_capture(dir / "overrun", kPoses, {}, overrun)).find("runs past the first chunk") !=
              std::string::npos,
          "a device length that runs into the first chunk is refused");
    Layout truncated;
    truncated.device_truncated = true;
    check(refusal(write_capture(dir / "truncated", kPoses, {}, truncated)).find("its device message: ") !=
              std::string::npos,
          "a device message that does not parse is refused as such");
    k1.index_lost = true;
    check(refusal(write_capture(dir / "k1_lost", kPoses, {}, k1)).find("Lixel K1 (device code XX") != std::string::npos,
          "another device is refused by name before its index is read");
    Layout codeless;
    codeless.code_written = false;
    check(refusal(write_capture(dir / "codeless", kPoses, {}, codeless)).find("PortalCam names no device code") !=
              std::string::npos,
          "a header with a model and no code names the model");
    Layout extra;
    extra.extra_channel = 44;
    check(refusal(write_capture(dir / "extra", kPoses, {}, extra)).find("channel 44") != std::string::npos,
          "a channel the device does not have is refused");
    Layout fifth;
    bool all_refused = true;
    for (const char* section : {"camera_4:\n  camera_model: kb4\n", "camera_4: # spare\n", "camera_4: {}\n"}) {
        fifth.extra_yaml = section;
        all_refused = all_refused && refusal(write_capture(dir / "fifth", kPoses, {}, fifth))
                                             .find("calibrates camera_4") != std::string::npos;
    }
    check(all_refused, "a calibrated camera the device does not have is refused, however its section is written");
    fifth.extra_yaml = "# spare cameras:\nspare: 1\n";
    check(refusal(write_capture(dir / "comment", kPoses, {}, fifth)).empty(), "a comment and a scalar key are no camera");
    Layout other_model;
    other_model.model = "Lixel K1";
    check(refusal(write_capture(dir / "same_code", kPoses, {}, other_model))
                  .find("camera layout of an XGRIDS Lixel K1 (device code AA") != std::string::npos,
          "the PortalCam's code with another model is refused");
    Layout other_code;
    other_code.code = "XX";
    check(refusal(write_capture(dir / "same_model", kPoses, {}, other_code))
                  .find("camera layout of an XGRIDS PortalCam (device code XX") != std::string::npos,
          "the PortalCam's model with another code is refused");
    Layout short_file;
    bool cut_refused = true;
    for (size_t at : {34, 40}) {
        short_file.cut_at = at;
        cut_refused = cut_refused && refusal(write_capture(dir / "short", kPoses, {}, short_file))
                                             .find("the file ends inside its device message") != std::string::npos;
    }
    check(cut_refused, "a file that ends inside its device message is refused");
    Layout tiny;
    tiny.first_chunk = 34;
    check(refusal(write_capture(dir / "tiny", kPoses, {}, tiny)).find("runs past the first chunk") != std::string::npos,
          "a first chunk too close to the header for a device length is refused");
    Layout huge;
    huge.first_chunk = 1 << 20;
    huge.device_overrun = 1 << 16;
    check(refusal(write_capture(dir / "huge", kPoses, {}, huge)).find("larger than 64 KB") != std::string::npos,
          "a device length over 64 KB is refused before anything is allocated");
    Layout wide;
    wide.frame_width = 41;
    check(refusal(write_capture(dir / "wide", kPoses, {}, wide)).find("41x30") != std::string::npos,
          "a frame of another width than its calibration is refused");
    Layout tall;
    tall.frame_height = 31;
    check(refusal(write_capture(dir / "tall", kPoses, {}, tall)).find("40x31") != std::string::npos,
          "a frame of another height than its calibration is refused");
    Layout unsized;
    unsized.frame_width = unsized.frame_height = 0;
    check(refusal(write_capture(dir / "unsized", kPoses, {}, unsized)).find("0x0") != std::string::npos,
          "a frame that states no size is refused");
    Layout no_photos;
    no_photos.missing_channel = 40;
    check(refusal(write_capture(dir / "no_photos", kPoses, {}, no_photos))
                  .find("no photographs of camera_3 in channel 40") != std::string::npos,
          "a camera without its photographs is refused");
    Layout no_section;
    no_section.missing_section = "camera_3";
    check(refusal(write_capture(dir / "no_section", kPoses, {}, no_section)).find("camera.yaml has no camera_3") !=
              std::string::npos,
          "a camera without its calibration is refused");
    const Bytes portalcam = capture_bytes(kPoses, {}, {});
    auto patched = [&](const char* name, uint64_t at, uint16_t v) {
        Bytes b = portalcam;
        std::memcpy(&b[at], &v, 2);
        const fs::path p = dir / name / "capture.xbin";
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary).write((const char*)b.data(), (std::streamsize)b.size());
        return refusal(p);
    };
    uint64_t first_chunk = 0, index_at = 0;
    std::memcpy(&first_chunk, &portalcam[16], 8);
    std::memcpy(&index_at, &portalcam[24], 8);
    check(patched("version", 4, 513).find("file version 513.1 of an XGRIDS PortalCam (device code AA") !=
              std::string::npos,
          "another file version is refused, by name");
    check(patched("chunk_tag", first_chunk + 6, 37).find("chunk header laid out") != std::string::npos,
          "a chunk header laid out otherwise is refused");
    check(patched("index_tag", index_at + 6, 13).find("chunk index laid out") != std::string::npos,
          "an index header laid out otherwise is refused");
    Layout predicted;
    predicted.frame_predicted = true;
    check(refusal(write_capture(dir / "predicted", kPoses, {}, predicted)).find("camera_3 of an XGRIDS PortalCam") !=
              std::string::npos,
          "a frame that does not decode alone is refused");
    check(refusal(write_capture(dir / "portalcam")).empty(), "a PortalCam reads through to its frames");
}

// Every prefix of a capture, and each of its bytes set four ways: the reader
// reads it through or refuses it naming the file once, and throws nothing else.
void check_corruption(const fs::path& dir) {
    const double deg = 3.14159265358979323846 / 180.0;
    // Every 8th sample of a gyro the poses accept: the rest only repeat the same bytes.
    std::vector<Bytes> imu;
    const std::vector<Bytes> gyro = gyro_messages(91 * deg, 91 * deg);
    for (size_t k = 0; k < gyro.size(); k += 8) imu.push_back(gyro[k]);
    const Bytes good = capture_bytes(kTurn, imu, {});
    fs::create_directories(dir);
    const fs::path path = dir / "corrupt.xbin";
    const std::string named = path.string() + ": ";
    int cases = 0, unnamed = 0;
    std::string example;
    auto attempt = [&](const Bytes& b) {
        std::ofstream(path, std::ios::binary | std::ios::trunc).write((const char*)b.data(), (std::streamsize)b.size());
        cases++;
        try {
            spirula::xbin::Reader r(path.string());
            r.read_points([](const spirula::e57::Point*, size_t) {});
            double R[9], c[3];
            for (size_t k = 0; k < r.cameras().size(); k++) {
                r.camera_pose(k, 1500000, R, c);
                r.read_frames(k, [](const spirula::xbin::Frame&) { return true; });
            }
            return true;
        } catch (const std::exception& e) {
            const std::string why = e.what();
            if (why.rfind(named, 0) != 0 || why.find(named, named.size()) != std::string::npos) {
                unnamed++;
                if (example.empty()) example = why;
            }
        } catch (...) {
            unnamed++;
        }
        return false;
    };
    check(attempt(good), "the capture the corruption starts from reads");
    for (size_t n = 0; n < good.size(); n++) attempt(Bytes(good.begin(), good.begin() + (std::ptrdiff_t)n));
    for (size_t i = 0; i < good.size(); i++)
        for (uint8_t v : {(uint8_t)0, (uint8_t)0xff, (uint8_t)(good[i] + 1), (uint8_t)(good[i] - 1)}) {
            Bytes b = good;
            b[i] = v;
            attempt(b);
        }
    if (unnamed) std::printf("  %d of %d, e.g. %s\n", unnamed, cases, example.c_str());
    check(unnamed == 0, "a cut or corrupted capture reads or is refused by its path, every byte");
}

int run() {
    const fs::path dir = fs::temp_directory_path() / "spirula_xbin_reader_test";
    fs::remove_all(dir);
    check_capture(dir, write_capture(dir));
    check_gyro(dir);
    check_refusals(dir);
    check_corruption(dir / "corrupt");
    fs::remove_all(dir);
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

int main() {
    try {
        return run();
    } catch (const std::exception& e) {
        std::printf("FAIL threw: %s\n", e.what());
        return 1;
    }
}
