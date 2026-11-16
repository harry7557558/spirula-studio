#pragma once

// A synthetic XGRIDS capture written byte for byte, for the tests that read
// one: the container, the calibration files, a trajectory, sweeps and frames.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace xbin_test {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

inline void varint(Bytes& b, uint64_t v) {
    while (v >= 0x80) {
        b.push_back((uint8_t)(v | 0x80));
        v >>= 7;
    }
    b.push_back((uint8_t)v);
}
inline uint64_t zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
inline void field(Bytes& b, uint32_t f, uint64_t v) { varint(b, (uint64_t)f << 3); varint(b, v); }
inline void field(Bytes& b, uint32_t f, const Bytes& m) {
    varint(b, (uint64_t)f << 3 | 2);
    varint(b, m.size());
    b.insert(b.end(), m.begin(), m.end());
}
inline Bytes text(const std::string& s) { return Bytes(s.begin(), s.end()); }
template <class T> inline void put(Bytes& b, T v) {
    uint8_t x[sizeof v];
    std::memcpy(x, &v, sizeof v);
    b.insert(b.end(), x, x + sizeof v);
}

// One sequence of literals only, which is a valid LZ4 block.
inline Bytes lz4_literals(const Bytes& raw) {
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

// CRC-32 bit by bit, apart from the reader's table.
inline uint32_t crc32(const Bytes& b, size_t from, size_t n) {
    uint32_t c = 0xffffffffu;
    for (size_t i = from; i < from + n; i++) {
        c ^= b[i];
        for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0xedb88320u : c >> 1;
    }
    return ~c;
}

// The topics of a PortalCam's table that the test writes, by id: half the channel.
const std::map<uint32_t, std::string> kTopics = {
    {0, "/config_file"},          {1, "/imu"},           {3, "/lidar"},
    {14, "/camera_left_main/h264"}, {16, "/camera_left_seco/h264"}, {18, "/camera_right_main/h264"},
    {20, "/camera_right_seco/h264"}, {21, "/project_data"}, {22, "/raw_ntrip_log"}};
constexpr uint64_t kTopicsAt = 0x1000;

inline Bytes file_message(const std::string& name, const std::string& body) {
    Bytes m;
    field(m, 2, text(name));
    field(m, 3, text(body));
    return m;
}

constexpr const char* kCameraYaml =
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
constexpr const char* kImuLidar = "transform: [1, 0, 0, 0.01, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n";
constexpr const char* kCameraLidar =
    "transform:\n  - 1\n  - 0\n  - 0\n  - 0\n  - 0\n  - 1\n  - 0\n  - 0\n"
    "  - 0\n  - 0\n  - 1\n  - -0.1\n  - 0\n  - 0\n  - 0\n  - 1\n";
// One metre along +x in one second, no turn.
constexpr const char* kPoses = "1.000000,0,0,0,0,0,0,1\n2.000000,1,0,0,0,0,0,1\n";
// Standing still, turned 90 degrees about x, then 91 more about its own z in one
// second: body z is world -y, so a turn applied in the wrong frame shows.
constexpr const char* kTurn = "1.000000,0,0,0,0.70710678118654752,0,0,0.70710678118654752\n"
                    "2.000000,0,0,0,0.49561769378289866,-0.50434422928127265,0.50434422928127276,0.49561769378289877\n";
// The same, its second quaternion negated.
constexpr const char* kTurnFlipped = "1.000000,0,0,0,0.70710678118654752,0,0,0.70710678118654752\n"
                           "2.000000,0,0,0,-0.49561769378289866,0.50434422928127265,-0.50434422928127276,-0.49561769378289877\n";

inline Bytes point(int64_t x_mm, int64_t y_mm, int64_t z_mm, int intensity, int64_t dt_us) {
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
inline std::vector<Bytes> gyro_messages(double fast, double slow) {
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
    std::string code = "AA";     // "" writes an empty device message
    std::string model = "PortalCam";
    uint32_t extra_channel = 0;                    // index field 1, twice the topic id
    uint32_t missing_channel = 0;                  // a camera channel left out
    bool seco_swapped = false;                     // the table names the two pinhole topics the other way
    std::string extra_yaml;
    std::string missing_section;                   // a camera.yaml section left out
    int64_t frame_width = 40, frame_height = 30;   // 0 leaves the field out
    uint32_t device_overrun = 0;                   // bytes the device length claims past the topic table
    bool device_truncated = false;                 // its message ends inside a field
    bool index_lost = false;                       // the header's index offset is 0
    bool code_written = true;                      // field 7 of the device message
    uint64_t first_chunk = 0;                      // written over the real offset when set
    size_t cut_at = 0;                             // the file ends after this many bytes
    bool frame_predicted = false;                  // camera_3's frame a P slice, no IDR
    std::vector<Bytes> sweeps;                     // /lidar messages; empty writes one of four points
};

inline Bytes capture_bytes(const char* poses, const std::vector<Bytes>& imu, const Layout& layout) {
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
    if (!layout.sweeps.empty()) {
        channels[6] = layout.sweeps;
    } else {
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
    {
        Bytes device;
        if (!layout.code.empty()) field(device, 1, text("X000000000"));
        if (!layout.code.empty()) {
            field(device, 2, text("V3.2.5"));
            field(device, 3, text("V3.2.5-20260402.131505"));
            field(device, 6, text(layout.model));
            if (layout.code_written) field(device, 7, text(layout.code));
            field(device, 8, text("rs_airy"));
        }
        if (layout.device_truncated) device.insert(device.end(), {0x0a, 0x05, 'A'});
        const uint32_t room = (uint32_t)kTopicsAt - 36;
        put<uint32_t>(file, layout.device_overrun ? room + layout.device_overrun : (uint32_t)device.size());
        file.insert(file.end(), device.begin(), device.end());
        // A real capture has bytes here that are no protobuf, up to the topic table.
        file.resize(kTopicsAt, 0xbb);
    }
    {
        Bytes table;
        for (const auto& [id, topic] : kTopics) {
            std::string name = topic;
            if (layout.seco_swapped && (id == 16 || id == 20)) name = kTopics.at(36 - id);
            put<uint32_t>(table, id);
            name.resize(60, '\0');
            table.insert(table.end(), name.begin(), name.end());
        }
        const Bytes packed = lz4_literals(table);
        put<uint32_t>(file, (uint32_t)(packed.size() + 28));
        put<uint16_t>(file, 0);
        put<uint16_t>(file, 12);
        put<uint64_t>(file, 1000000);
        put<uint32_t>(file, (uint32_t)table.size());
        put<uint32_t>(file, (uint32_t)packed.size());
        file.insert(file.end(), packed.begin(), packed.end());
        put<uint32_t>(file, crc32(file, file.size() - packed.size(), packed.size()));
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
        put<uint32_t>(file, ch / 2);
        put<uint32_t>(file, (uint32_t)payload.size());
        file.insert(file.end(), payload.begin(), payload.end());
        put<uint32_t>(file, crc32(payload, 0, payload.size()));
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
    put<uint32_t>(file, crc32(packed, 0, packed.size()));
    if (layout.cut_at) file.resize(layout.cut_at);
    return file;
}

inline fs::path write_capture(const fs::path& dir, const char* poses = kPoses,
                       const std::vector<Bytes>& imu = {}, const Layout& layout = {}) {
    const Bytes file = capture_bytes(poses, imu, layout);
    fs::create_directories(dir);
    const fs::path path = dir / "capture.xbin";
    std::ofstream(path, std::ios::binary).write((const char*)file.data(), (std::streamsize)file.size());
    return path;
}

}  // namespace xbin_test
