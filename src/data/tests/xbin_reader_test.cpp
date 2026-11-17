// A synthetic XGRIDS capture written byte for byte, then read back through
// xbin::Reader and cloud::Reader: the container, the calibration chain and
// the per-point deskew against values worked out by hand, and each device,
// channel, calibration and frame the reader must refuse.

#include "data/PointCloudFile.h"
#include "data/XbinReader.h"
#include "data/tests/XbinTestCapture.h"

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
using namespace xbin_test;

namespace {
int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "ok" : "FAIL", name);
    if (!ok) ++failures;
}

bool close_to(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }

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

    // A correction: 20 cm along y at 1 s, a quarter turn about z at 2 s.
    spirula::xbin::Correction corr;
    corr.time_us = {1000000, 2000000};
    corr.move = {{0, 0, 0, 0, 0.2, 0}, {0, 0, 1.5707963267948966, 0, 0, 0}};
    r.set_correction(corr);
    check(r.camera_pose(0, 1000000, R, c) && close_to(c[0], 0.01) && close_to(c[1], 0.2) && close_to(R[0], 1),
          "a correction key moves the camera");
    check(r.camera_pose(0, 1250000, R, c) && close_to(R[3], std::sin(0.25 * 1.5707963267948966)) &&
              close_to(c[1], 0.15 + std::sin(0.25 * 1.5707963267948966) * 0.26, 1e-3),
          "between two keys the move is blended");
    std::vector<spirula::e57::Point> moved;
    r.read_points([&](const spirula::e57::Point* p, size_t n) { moved.insert(moved.end(), p, p + n); });
    // The sweep at 1.5 s: half way, an eighth turn and 10 cm.
    check(moved.size() == 2 && close_to(moved[0].xyz[0], 2.51 * std::cos(0.7853981633974483), 1e-3) &&
              close_to(moved[0].xyz[1], 2.51 * std::sin(0.7853981633974483) + 0.1, 1e-3),
          "points move by the blend at their own time");
    r.set_correction({});
    check(r.camera_pose(0, 1250000, R, c) && close_to(c[0], 0.26) && close_to(c[1], 0), "an empty correction is the device's trajectory");

    // Outer: a quarter turn about z and 30 cm up, at 1.5 s.
    // Composed, the shift of `corr` turns with it.
    spirula::xbin::Correction turn;
    turn.time_us = {1500000};
    turn.move = {{0, 0, 1.5707963267948966, 0, 0, 0.3}};
    const spirula::xbin::Correction both = spirula::xbin::compose(turn, corr);
    check(both.time_us == std::vector<int64_t>{1000000, 1500000, 2000000}, "a composed correction keys at both");
    r.set_correction(both);
    check(r.camera_pose(0, 1000000, R, c) && close_to(c[0], -0.2) && close_to(c[1], 0.01) && close_to(c[2], 0.4) &&
              close_to(R[3], 1),
          "a composed correction applies the inner move first");
    check(spirula::xbin::compose({}, corr).time_us == corr.time_us &&
              spirula::xbin::compose(corr, {}).move == corr.move,
          "composing with nothing keeps the other");
    r.set_correction({});

    std::vector<spirula::xbin::Frame> frames;
    r.read_frames(2, [&](const spirula::xbin::Frame& f) { frames.push_back(f); return true; });
    check(frames.size() == 1 && frames[0].time_us == 1250032 && frames[0].h264.size() == 16 &&
              frames[0].h264[15] == 32,
          "camera_2 is channel 32");
    {
        Layout swapped;
        swapped.seco_swapped = true;
        spirula::xbin::Reader s(write_capture(dir / "swapped", kPoses, {}, swapped).string());
        frames.clear();
        s.read_frames(2, [&](const spirula::xbin::Frame& f) { frames.push_back(f); return true; });
        check(frames.size() == 1 && frames[0].h264.size() == 16 && frames[0].h264[15] == 40,
              "a camera is found by its topic's name, not by its channel");
    }

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
    overrun.device_overrun = 1;
    check(refusal(write_capture(dir / "overrun", kPoses, {}, overrun)).find("runs past byte 4096") !=
              std::string::npos,
          "a device length that runs into the topic table is refused");
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
    check(refusal(write_capture(dir / "extra", kPoses, {}, extra)).find("topic /raw_ntrip_log is not known") !=
              std::string::npos,
          "a topic the reader does not read is refused, by name");
    extra.extra_channel = 2 * 29;
    check(refusal(write_capture(dir / "unnamed", kPoses, {}, extra)).find("index entry for topic 29") !=
              std::string::npos,
          "an index entry for a topic the table does not name is refused");
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
    check(refusal(write_capture(dir / "tiny", kPoses, {}, tiny)).find("runs past byte 34") != std::string::npos,
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
                  .find("no photographs of camera_3 in /camera_right_seco/h264") != std::string::npos,
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
    check(patched("chunk_type", first_chunk + 4, 0).find("a block of type 0 where the index has a data chunk") !=
              std::string::npos,
          "a block of another type where a chunk should be is refused");
    check(patched("chunk_topic", first_chunk + 40, 1).find("a chunk of another topic") != std::string::npos,
          "a chunk of another topic than its index entry is refused");
    check(patched("chunk_crc", first_chunk + 48, 0xffff).find("a damaged chunk") != std::string::npos,
          "a chunk whose CRC-32 does not match is refused");
    check(patched("topics_type", kTopicsAt + 4, 1).find("no topic table before the first chunk") !=
              std::string::npos,
          "a capture without its topic table is refused");
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
    // The bytes between the device message and the topic table, which nothing
    // reads: one cut and one changed byte there stand for all of them.
    uint32_t device = 0;
    std::memcpy(&device, &good[32], 4);
    const size_t gap_from = 36 + device + 1;
    auto in_gap = [&](size_t i) { return i >= gap_from && i < kTopicsAt; };
    for (size_t n = 0; n < good.size(); n++)
        if (!in_gap(n)) attempt(Bytes(good.begin(), good.begin() + (std::ptrdiff_t)n));
    for (size_t i = 0; i < good.size(); i++)
        for (uint8_t v : {(uint8_t)0, (uint8_t)0xff, (uint8_t)(good[i] + 1), (uint8_t)(good[i] - 1)}) {
            if (in_gap(i)) continue;
            Bytes b = good;
            b[i] = v;
            attempt(b);
        }
    std::printf("  %d cut or corrupted copies\n", cases);
    if (unnamed) std::printf("  %d of %d, e.g. %s\n", unnamed, cases, example.c_str());
    check(unnamed == 0, "a cut or corrupted capture reads or is refused by its path, every byte it reads");
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
