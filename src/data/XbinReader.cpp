#include "data/XbinReader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace spirula::xbin {
namespace {

namespace fs = std::filesystem;

constexpr size_t kBatch = 1 << 18;
constexpr size_t kChunkHeader = 48;
constexpr size_t kIndexHeader = 24;
constexpr uint64_t kMaxChunk = 1ull << 30;

constexpr uint32_t kChannelFiles = 0;
constexpr uint32_t kChannelImu = 2;
constexpr uint32_t kChannelLidar = 6;
constexpr uint32_t kChannelTrajectory = 42;
// 0.99e-4 and 1.01e-4 fit the turns between poses worse.
constexpr double kGyroRadPerCount = 1e-4;
// The gap carries the gyro's bias, which the spread takes out as well: the
// worst gap measured, bias included, was 0.47 deg.
constexpr double kMaxGyroGap = 2.0 * 3.14159265358979323846 / 180.0;
// Which channel holds which calibrated camera, by the device code and model of
// the device message. Nothing the reader parses says which; measured by projecting
// the sweeps into the photos, the two pinholes told apart by their stereo shift.
struct CameraChannel {
    const char* code;
    const char* model;
    uint32_t channel;
    const char* name;
};
constexpr CameraChannel kCameraChannels[] = {{"AA", "PortalCam", 28, "camera_0"},
                                             {"AA", "PortalCam", 36, "camera_1"},
                                             {"AA", "PortalCam", 32, "camera_2"},
                                             {"AA", "PortalCam", 40, "camera_3"}};
// The PortalCam's device message is 172 bytes.
constexpr uint32_t kMaxDeviceMessage = 1 << 16;

// Returns this close are the device itself and the hand holding it.
constexpr double kMinRange = 0.5;
// Bytes of channel 6 per point, headers included: 18.6 on a 110 s indoor scan.
constexpr int64_t kBytesPerPoint = 18;

[[noreturn]] void fail(const std::string& path, const std::string& why) {
    throw std::runtime_error(path + ": " + why);
}

template <class T> T rd(const uint8_t* p) { T v; std::memcpy(&v, p, sizeof v); return v; }

int64_t zigzag(uint64_t v) { return (int64_t)(v >> 1) ^ -(int64_t)(v & 1); }

// A protobuf message read without its schema, one field at a time.
struct Fields {
    const uint8_t* p;
    const uint8_t* end;
    uint32_t field = 0;
    int wire = 0;
    uint64_t value = 0;
    const uint8_t* data = nullptr;
    size_t size = 0;

    Fields(const uint8_t* b, size_t n) : p(b), end(b + n) {}

    uint64_t varint() {
        uint64_t r = 0;
        for (int s = 0; s < 64; s += 7) {
            if (p >= end) throw std::runtime_error("truncated message");
            const uint8_t b = *p++;
            r |= (uint64_t)(b & 0x7f) << s;
            if (!(b & 0x80)) return r;
        }
        throw std::runtime_error("malformed varint");
    }
    bool next() {
        if (p >= end) return false;
        const uint64_t key = varint();
        field = (uint32_t)(key >> 3);
        wire = (int)(key & 7);
        if (wire == 0) {
            value = varint();
        } else if (wire == 2) {
            size = (size_t)varint();
            if (size > (size_t)(end - p)) throw std::runtime_error("truncated message");
            data = p;
            p += size;
        } else if (wire == 1 || wire == 5) {
            const size_t n = wire == 1 ? 8 : 4;
            if (n > (size_t)(end - p)) throw std::runtime_error("truncated message");
            p += n;
        } else {
            throw std::runtime_error("unknown wire type");
        }
        return true;
    }
};

std::vector<uint8_t> lz4_block(const uint8_t* src, size_t n, size_t out_size) {
    std::vector<uint8_t> out;
    size_t i = 0;
    auto length = [&](size_t base) {
        if (base != 15) return base;
        for (uint8_t b = 255; b == 255;) {
            if (i >= n) throw std::runtime_error("truncated LZ4 block");
            b = src[i++];
            base += b;
        }
        return base;
    };
    while (i < n) {
        const uint8_t token = src[i++];
        const size_t lit = length(token >> 4);
        if (lit > n - i) throw std::runtime_error("truncated LZ4 block");
        if (lit > out_size - out.size()) throw std::runtime_error("LZ4 block has the wrong size");
        out.insert(out.end(), src + i, src + i + lit);
        i += lit;
        if (i >= n) break;
        if (n - i < 2) throw std::runtime_error("truncated LZ4 block");
        const size_t offset = src[i] | (size_t)src[i + 1] << 8;
        i += 2;
        const size_t match = length(token & 15) + 4;
        if (offset == 0 || offset > out.size()) throw std::runtime_error("bad LZ4 offset");
        if (match > out_size - out.size()) throw std::runtime_error("LZ4 block has the wrong size");
        const size_t from = out.size() - offset;
        for (size_t k = 0; k < match; k++) out.push_back(out[from + k]);
    }
    if (out.size() != out_size) throw std::runtime_error("LZ4 block has the wrong size");
    return out;
}

void mat4_mul(const double a[16], const double b[16], double out[16]) {
    double r[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a[i * 4 + k] * b[k * 4 + j];
            r[i * 4 + j] = s;
        }
    std::memcpy(out, r, sizeof r);
}

// A rigid transform's inverse.
void mat4_inverse(const double a[16], double out[16]) {
    double r[16] = {};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r[i * 4 + j] = a[j * 4 + i];
    for (int i = 0; i < 3; i++)
        r[i * 4 + 3] = -(r[i * 4] * a[3] + r[i * 4 + 1] * a[7] + r[i * 4 + 2] * a[11]);
    r[15] = 1;
    std::memcpy(out, r, sizeof r);
}

// ---- rotations: quaternions x y z w, the order of poses.csv ----

using Quat = std::array<double, 4>;
using Vec3 = std::array<double, 3>;

Quat quat_mul(const Quat& a, const Quat& b) {
    return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}

Quat quat_conj(const Quat& q) { return {-q[0], -q[1], -q[2], q[3]}; }

Quat quat_exp(const Vec3& v) {
    const double a = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    const double s = a < 1e-12 ? 0.5 : std::sin(0.5 * a) / a;
    return {v[0] * s, v[1] * s, v[2] * s, std::cos(0.5 * a)};
}

Vec3 quat_log(Quat q) {
    if (q[3] < 0)
        for (double& v : q) v = -v;
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    const double k = n < 1e-12 ? 2.0 : 2.0 * std::atan2(n, q[3]) / n;
    return {q[0] * k, q[1] * k, q[2] * k};
}

// The index i, in [1, n), with t[i - 1] <= x <= t[i]; t ascending, n >= 2.
size_t bracket(const std::vector<int64_t>& t, int64_t x) {
    const size_t i = (size_t)(std::upper_bound(t.begin(), t.end(), x) - t.begin());
    return std::clamp<size_t>(i, 1, t.size() - 1);
}

// The poses' attitudes plus one per gyro sample between two poses: the gyro's
// turn, its gap to the next pose spread evenly. An interval the gyro does not
// cover, or whose gap is over kMaxGyroGap, keeps the poses alone. Body-frame rad/s.
void fill_attitude(const std::vector<int64_t>& pose_t, const std::vector<Quat>& pose_q,
                   const std::vector<int64_t>& gyro_t, const std::vector<Vec3>& gyro,
                   std::vector<int64_t>& att_t, std::vector<Quat>& att) {
    auto rate = [&](int64_t t) {
        const size_t j = bracket(gyro_t, t);
        const double s = (double)(t - gyro_t[j - 1]) / (double)(gyro_t[j] - gyro_t[j - 1]);
        Vec3 w;
        for (int k = 0; k < 3; k++) w[k] = (1 - s) * gyro[j - 1][k] + s * gyro[j][k];
        return w;
    };
    std::vector<int64_t> node_t;
    std::vector<Quat> node_q;
    for (size_t i = 0; i < pose_t.size(); i++) {
        att_t.push_back(pose_t[i]);
        att.push_back(pose_q[i]);
        if (i + 1 == pose_t.size() || gyro_t.size() < 2) continue;
        const int64_t ta = pose_t[i], tb = pose_t[i + 1];
        if (ta < gyro_t.front() || tb > gyro_t.back()) continue;
        node_t.assign(1, ta);
        for (auto it = std::upper_bound(gyro_t.begin(), gyro_t.end(), ta); it != gyro_t.end() && *it < tb; ++it)
            node_t.push_back(*it);
        node_t.push_back(tb);
        node_q.assign(1, Quat{0, 0, 0, 1});
        Vec3 w0 = rate(ta);
        for (size_t k = 1; k < node_t.size(); k++) {
            const Vec3 w1 = rate(node_t[k]);
            const double dt = (double)(node_t[k] - node_t[k - 1]) * 1e-6;
            node_q.push_back(quat_mul(node_q.back(), quat_exp({0.5 * (w0[0] + w1[0]) * dt,
                                                               0.5 * (w0[1] + w1[1]) * dt,
                                                               0.5 * (w0[2] + w1[2]) * dt})));
            w0 = w1;
        }
        const Vec3 gap = quat_log(quat_mul(quat_conj(node_q.back()),
                                           quat_mul(quat_conj(pose_q[i]), pose_q[i + 1])));
        if (std::sqrt(gap[0] * gap[0] + gap[1] * gap[1] + gap[2] * gap[2]) > kMaxGyroGap) continue;
        for (size_t k = 1; k + 1 < node_t.size(); k++) {
            const double s = (double)(node_t[k] - ta) / (double)(tb - ta);
            att_t.push_back(node_t[k]);
            att.push_back(quat_mul(pose_q[i], quat_mul(node_q[k], quat_exp({s * gap[0], s * gap[1], s * gap[2]}))));
        }
    }
}

// ---- the calibration files: YAML, but only scalars and number lists ----

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream ss(text);
    for (std::string l; std::getline(ss, l);) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        out.push_back(l);
    }
    return out;
}

size_t indent_of(const std::string& l) {
    size_t n = 0;
    while (n < l.size() && l[n] == ' ') n++;
    return n;
}

// `key:` among `lines` [from, to), as `[a, b]` on its line or `- a` lines below.
std::vector<double> number_list(const std::vector<std::string>& lines, size_t from, size_t to,
                                const std::string& key) {
    std::vector<double> out;
    for (size_t i = from; i < to; i++) {
        const std::string& l = lines[i];
        const size_t at = indent_of(l);
        if (l.compare(at, key.size() + 1, key + ":") != 0) continue;
        const size_t open = l.find('[', at);
        if (open != std::string::npos) {
            std::string body = l.substr(open + 1, l.find(']', open) - open - 1);
            std::replace(body.begin(), body.end(), ',', ' ');
            std::istringstream ss(body);
            for (double v; ss >> v;) out.push_back(v);
            return out;
        }
        for (size_t k = i + 1; k < to; k++) {
            const size_t d = indent_of(lines[k]);
            if (lines[k].compare(d, 2, "- ") != 0) break;
            std::istringstream ss(lines[k].substr(d + 2));
            double v;
            if (!(ss >> v)) return {};
            out.push_back(v);
        }
        return out;
    }
    return out;
}

std::string scalar(const std::vector<std::string>& lines, size_t from, size_t to,
                   const std::string& key) {
    for (size_t i = from; i < to; i++) {
        const std::string& l = lines[i];
        const size_t at = indent_of(l);
        if (l.compare(at, key.size() + 1, key + ":") != 0) continue;
        std::string v = l.substr(at + key.size() + 1);
        v.erase(0, v.find_first_not_of(' '));
        return v;
    }
    return "";
}

}  // namespace

struct Reader::Impl {
    std::string path;
    std::ifstream f;
    uint64_t file_size = 0;
    std::map<uint32_t, std::vector<uint64_t>> chunks;   // channel -> chunk offsets
    std::map<uint32_t, int64_t> messages, bytes;
    std::map<uint32_t, std::pair<int64_t, int64_t>> span;   // channel -> first, last time (us)
    std::map<std::string, std::string> files;           // what channels 0 and 42 carry
    std::vector<int64_t> pose_t;
    std::vector<Vec3> pose_c;                           // IMU centre in world
    // Its attitude, gyro-filled between the poses: 10 Hz misses a hand's turns.
    std::vector<int64_t> att_t;
    std::vector<Quat> att;
    double imu_from_lidar[16] = {};
    double lidar_from_cam0[16] = {};
    std::vector<std::array<double, 16>> cam0_from_cam;
    std::vector<uint32_t> camera_channel;
    std::string device_name;

    std::vector<uint8_t> chunk(uint64_t at) {
        uint8_t h[kChunkHeader];
        if (at > file_size || file_size - at < kChunkHeader) throw std::runtime_error("the file ends inside a chunk header");
        f.clear();
        f.seekg((std::streamoff)at);
        if (!f.read((char*)h, sizeof h)) throw std::runtime_error("the file ends inside a chunk header");
        if (rd<uint16_t>(h + 4) != 1) throw std::runtime_error("a compressed data chunk, which this does not read");
        const uint32_t size = rd<uint32_t>(h + 16);
        if (size > kMaxChunk) throw std::runtime_error("a chunk larger than 1 GB");
        if (size > file_size - at - kChunkHeader) throw std::runtime_error("the file ends inside a chunk");
        std::vector<uint8_t> out(size);
        if (!f.read((char*)out.data(), (std::streamsize)size)) throw std::runtime_error("the file ends inside a chunk");
        return out;
    }

    // Each message of `channel`; `fn` returns false to stop.
    template <class Fn> void each_message(uint32_t channel, Fn&& fn) {
        const auto it = chunks.find(channel);
        if (it == chunks.end()) return;
        for (uint64_t at : it->second) {
            const std::vector<uint8_t> c = chunk(at);
            Fields m(c.data(), c.size());
            while (m.next())
                if (m.field == 1 && m.wire == 2 && !fn(m.data, m.size)) return;
        }
    }

    bool imu_pose(int64_t t, double R[9], double c[3]) const {
        if (pose_t.size() < 2 || t < pose_t.front() || t > pose_t.back()) return false;
        size_t i = bracket(pose_t, t);
        double s = (double)(t - pose_t[i - 1]) / (double)(pose_t[i] - pose_t[i - 1]);
        for (int k = 0; k < 3; k++) c[k] = (1 - s) * pose_c[i - 1][k] + s * pose_c[i][k];
        i = bracket(att_t, t);
        s = (double)(t - att_t[i - 1]) / (double)(att_t[i] - att_t[i - 1]);
        const Quat& a = att[i - 1];
        const Quat& b = att[i];
        const double sign = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0 ? -1 : 1;
        double q[4], n = 0;
        for (int k = 0; k < 4; k++) {
            q[k] = (1 - s) * a[k] + s * sign * b[k];
            n += q[k] * q[k];
        }
        n = std::sqrt(n);
        const double x = q[0] / n, y = q[1] / n, z = q[2] / n, w = q[3] / n;
        R[0] = 1 - 2 * (y * y + z * z); R[1] = 2 * (x * y - z * w);     R[2] = 2 * (x * z + y * w);
        R[3] = 2 * (x * y + z * w);     R[4] = 1 - 2 * (x * x + z * z); R[5] = 2 * (y * z - x * w);
        R[6] = 2 * (x * z - y * w);     R[7] = 2 * (y * z + x * w);     R[8] = 1 - 2 * (x * x + y * y);
        return true;
    }
};

Reader::Reader(const std::string& path) : _impl(std::make_unique<Impl>()) {
    Impl& I = *_impl;
    I.path = path;
    I.f.open(path, std::ios::binary);
    std::error_code size_error;
    I.file_size = fs::file_size(path, size_error);
    if (!I.f || size_error) fail(path, "cannot open");
    uint8_t h[32];
    if (!I.f.read((char*)h, sizeof h) || std::memcmp(h, "XBAG", 4) != 0)
        fail(path, "not an XGRIDS capture");
    const uint64_t first_chunk = rd<uint64_t>(h + 16), index_at = rd<uint64_t>(h + 24);
    // Before the first chunk: u32 length, a message naming the device, then bytes
    // this does not read. Read before the index, so that another device is refused
    // by name even where the rest of its file is laid out differently.
    std::string firmware, model, code;
    if (first_chunk > sizeof h) {
        uint8_t n[4];
        if (!I.f.read((char*)n, sizeof n)) fail(path, "the file ends inside its device message");
        if (rd<uint32_t>(n) > kMaxDeviceMessage) fail(path, "a device message larger than 64 KB");
        if (first_chunk < sizeof h + sizeof n || rd<uint32_t>(n) > first_chunk - sizeof h - sizeof n)
            fail(path, "its device message runs past the first chunk");
        std::vector<uint8_t> info(rd<uint32_t>(n));
        if (!I.f.read((char*)info.data(), (std::streamsize)info.size()))
            fail(path, "the file ends inside its device message");
        try {
            Fields d(info.data(), info.size());
            while (d.next()) {
                if (d.wire != 2) continue;
                if (d.field == 2) firmware.assign((const char*)d.data, d.size);
                else if (d.field == 6) model.assign((const char*)d.data, d.size);
                else if (d.field == 7) code.assign((const char*)d.data, d.size);
            }
        } catch (const std::runtime_error& e) {
            fail(path, std::string("its device message: ") + e.what());
        }
    }
    if (code.empty())
        fail(path, model.empty() ? std::string("the device message names no XGRIDS device")
                                 : "the device message of an XGRIDS " + model + " names no device code");
    std::vector<const CameraChannel*> layout;
    for (const CameraChannel& cc : kCameraChannels)
        if (code == cc.code && model == cc.model) layout.push_back(&cc);
    I.device_name = "XGRIDS" + (model.empty() ? "" : " " + model) + " (device code " + code +
                    (firmware.empty() ? "" : ", firmware " + firmware) + ")";
    if (layout.empty()) fail(path, "the camera layout of an " + I.device_name + " is not known");

    uint8_t ih[kIndexHeader];
    I.f.seekg((std::streamoff)index_at);
    // Laid out as a chunk header, its compression 0xff for LZ4.
    if (!I.f.read((char*)ih, sizeof ih) || rd<uint16_t>(ih + 4) != 0xff)
        fail(path, "no chunk index where the header says (the capture did not finish?)");
    const uint32_t raw = rd<uint32_t>(ih + 16), packed = rd<uint32_t>(ih + 20);
    if (raw > kMaxChunk || packed > kMaxChunk) fail(path, "a chunk index larger than 1 GB");
    if (packed > I.file_size - index_at - kIndexHeader) fail(path, "the file ends inside its index");
    std::vector<uint8_t> z(packed);
    if (!I.f.read((char*)z.data(), (std::streamsize)packed)) fail(path, "the file ends inside its index");
    try {
        const std::vector<uint8_t> index = lz4_block(z.data(), z.size(), raw);
        Fields top(index.data(), index.size());
        while (top.next()) {
            if (top.field != 1 || top.wire != 2) continue;
            uint32_t channel = 0;
            int64_t count = 0, size = 0, first = 0, last = 0;
            std::vector<uint64_t> offsets;
            Fields e(top.data, top.size);
            while (e.next()) {
                if (e.field == 1 && e.wire == 0) channel = (uint32_t)e.value;
                else if (e.field == 2 && e.wire == 0) count = (int64_t)e.value;
                else if (e.field == 3 && e.wire == 0) size = (int64_t)e.value;
                else if (e.field == 4 && e.wire == 0) first = zigzag(e.value);
                else if (e.field == 5 && e.wire == 0) last = zigzag(e.value);
                else if (e.field == 6 && e.wire == 2) {
                    Fields packed_offsets(e.data, e.size);
                    while (packed_offsets.p < packed_offsets.end)
                        offsets.push_back(packed_offsets.varint());
                }
            }
            I.chunks[channel] = std::move(offsets);
            I.messages[channel] = count;
            I.bytes[channel] = size;
            I.span[channel] = {first, last};
        }
        for (uint32_t ch : {kChannelFiles, kChannelTrajectory})
            I.each_message(ch, [&](const uint8_t* d, size_t n) {
                std::string name, body;
                Fields m(d, n);
                while (m.next()) {
                    if (m.wire != 2) continue;
                    if (m.field == 2) name.assign((const char*)m.data, m.size);
                    else if (m.field == 3) body.assign((const char*)m.data, m.size);
                }
                if (!name.empty()) I.files[name] = body;
                return true;
            });
    } catch (const std::runtime_error& e) {
        fail(path, e.what());
    }

    for (const auto& entry : I.chunks) {
        const uint32_t ch = entry.first;
        bool known = ch == kChannelFiles || ch == kChannelImu || ch == kChannelLidar || ch == kChannelTrajectory;
        for (const CameraChannel* cc : layout) known = known || ch == cc->channel;
        if (!known) fail(path, "channel " + std::to_string(ch) + " is not known for an " + I.device_name);
    }

    auto file = [&](const char* name) -> const std::string& {
        const auto it = I.files.find(name);
        if (it == I.files.end()) fail(path, std::string("the capture carries no ") + name);
        return it->second;
    };
    {
        const std::vector<std::string> l = lines_of(file("extrinsic_imu_lidar.yaml"));
        const std::vector<double> m = number_list(l, 0, l.size(), "transform");
        if (m.size() != 16) fail(path, "extrinsic_imu_lidar.yaml has no 4x4 transform");
        std::copy(m.begin(), m.end(), I.imu_from_lidar);
    }
    {
        // Measured: this file maps LiDAR points into camera_0, not the reverse.
        const std::vector<std::string> l = lines_of(file("extrinsic_camera_lidar.yaml"));
        const std::vector<double> m = number_list(l, 0, l.size(), "transform");
        if (m.size() != 16) fail(path, "extrinsic_camera_lidar.yaml has no 4x4 transform");
        double cam0_from_lidar[16];
        std::copy(m.begin(), m.end(), cam0_from_lidar);
        mat4_inverse(cam0_from_lidar, I.lidar_from_cam0);
    }
    {
        const std::vector<std::string> l = lines_of(file("camera.yaml"));
        // A top-level key with nothing after it but a comment, or a flow map, is a section.
        for (const std::string& s : l) {
            const size_t colon = s.find(':');
            if (s.empty() || indent_of(s) != 0 || s[0] == '#' || colon == std::string::npos) continue;
            const size_t rest = s.find_first_not_of(' ', colon + 1);
            const std::string key = s.substr(0, colon);
            if ((rest == std::string::npos || s[rest] == '#' || s[rest] == '{') &&
                std::none_of(layout.begin(), layout.end(), [&](const CameraChannel* cc) { return key == cc->name; }))
                fail(path, "camera.yaml calibrates " + key + ", which an " + I.device_name + " does not have");
        }
        for (const CameraChannel* cc : layout) {
            size_t from = l.size(), to = l.size();
            for (size_t i = 0; i < l.size(); i++) {
                if (from == l.size() && l[i] == std::string(cc->name) + ":") from = i + 1;
                else if (from < l.size() && i >= from && !l[i].empty() && indent_of(l[i]) == 0) {
                    to = i;
                    break;
                }
            }
            if (from == l.size()) fail(path, std::string("camera.yaml has no ") + cc->name);
            if (!I.chunks.count(cc->channel))
                fail(path, std::string("no photographs of ") + cc->name + " in channel " +
                               std::to_string(cc->channel) + ", where an " + I.device_name + " has them");
            Camera c;
            c.name = cc->name;
            const std::string lens = scalar(l, from, to, "camera_model");
            if (lens != "kb4" && lens != "pinhole")
                fail(path, cc->name + std::string(": unknown camera model '") + lens + "'");
            c.fisheye = lens == "kb4";
            c.width = std::atoi(scalar(l, from, to, "image_width").c_str());
            c.height = std::atoi(scalar(l, from, to, "image_height").c_str());
            const std::vector<double> K = number_list(l, from, to, "intrinsic");
            const std::vector<double> D = number_list(l, from, to, "distortion");
            const std::vector<double> pose = number_list(l, from, to, "camera_pose");
            if (c.width <= 0 || c.height <= 0 || K.size() != 4 || D.size() != 4 || pose.size() != 16)
                fail(path, cc->name + std::string(": incomplete calibration"));
            std::copy(K.begin(), K.end(), c.params);
            std::copy(D.begin(), D.end(), c.params + 4);
            c.frames = I.messages[cc->channel];
            const auto [first, last] = I.span[cc->channel];
            if (first > 0 && last > first) c.seconds = (double)(last - first) * 1e-6;
            std::array<double, 16> T;
            std::copy(pose.begin(), pose.end(), T.begin());
            I.cam0_from_cam.push_back(T);
            I.camera_channel.push_back(cc->channel);
            _cameras.push_back(c);
        }
    }
    std::vector<Quat> pose_q;
    {
        std::istringstream ss(file("poses.csv"));
        for (std::string line; std::getline(ss, line);) {
            if (line.empty() || line[0] == '#') continue;
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ls(line);
            double t;
            std::array<double, 7> p;
            if (!(ls >> t)) continue;
            bool whole = true;
            for (double& v : p) whole = whole && (bool)(ls >> v);
            if (!whole) continue;
            const int64_t us = std::llround(t * 1e6);
            if (!I.pose_t.empty() && us <= I.pose_t.back()) continue;
            I.pose_t.push_back(us);
            I.pose_c.push_back({p[0], p[1], p[2]});
            pose_q.push_back({p[3], p[4], p[5], p[6]});
        }
        if (I.pose_t.size() < 2) fail(path, "poses.csv holds no trajectory");
    }
    // Channel 2: header field 1 (its field 2 the time), accelerometer 2-4,
    // gyro 5-7, in the IMU frame of poses.csv. A channel this cannot read
    // leaves the poses alone, as for no channel.
    std::vector<int64_t> gyro_t;
    std::vector<Vec3> gyro;
    try {
        I.each_message(kChannelImu, [&](const uint8_t* d, size_t n) {
            int64_t t = 0, v[8] = {};
            Fields m(d, n);
            while (m.next()) {
                if (m.field == 1 && m.wire == 2) {
                    Fields h(m.data, m.size);
                    while (h.next())
                        if (h.field == 2 && h.wire == 0) t = zigzag(h.value);
                } else if (m.wire == 0 && m.field < 8) {
                    v[m.field] = zigzag(m.value);
                }
            }
            if (t > 0 && (gyro_t.empty() || t > gyro_t.back())) {
                gyro_t.push_back(t);
                gyro.push_back({v[5] * kGyroRadPerCount, v[6] * kGyroRadPerCount, v[7] * kGyroRadPerCount});
            }
            return true;
        });
    } catch (const std::runtime_error&) {
        gyro_t.clear();
        gyro.clear();
    }
    fill_attitude(I.pose_t, pose_q, gyro_t, gyro, I.att_t, I.att);
}

Reader::~Reader() = default;

int64_t Reader::sweeps() const {
    const auto it = _impl->messages.find(kChannelLidar);
    return it == _impl->messages.end() ? 0 : it->second;
}

int64_t Reader::points_estimate() const {
    const auto it = _impl->bytes.find(kChannelLidar);
    return it == _impl->bytes.end() ? 0 : it->second / kBytesPerPoint;
}

bool Reader::read_points(const std::function<void(const e57::Point*, size_t)>& sink,
                         const std::atomic<bool>* cancel) {
    Impl& I = *_impl;
    const double* E = I.imu_from_lidar;
    std::vector<e57::Point> batch;
    batch.reserve(kBatch);
    bool stopped = false;
    try {
        I.each_message(kChannelLidar, [&](const uint8_t* d, size_t n) {
            if (cancel && cancel->load()) {
                stopped = true;
                return false;
            }
            int64_t sweep_us = 0;
            Fields m(d, n);
            while (m.next()) {
                if (m.wire != 2) continue;
                if (m.field == 1) {
                    Fields h(m.data, m.size);
                    while (h.next())
                        if (h.field == 2 && h.wire == 0) sweep_us = zigzag(h.value);
                    continue;
                }
                if (m.field != 2) continue;
                int64_t v[7] = {};
                Fields pt(m.data, m.size);
                while (pt.next())
                    if (pt.wire == 0 && pt.field < 7) v[pt.field] = (int64_t)pt.value;
                const double x = zigzag((uint64_t)v[1]) * 1e-3, y = zigzag((uint64_t)v[2]) * 1e-3,
                             z = zigzag((uint64_t)v[3]) * 1e-3;
                if (x * x + y * y + z * z < kMinRange * kMinRange) continue;
                double R[9], c[3];
                if (!I.imu_pose(sweep_us + zigzag((uint64_t)v[6]), R, c)) continue;
                const double p[3] = {E[0] * x + E[1] * y + E[2] * z + E[3],
                                     E[4] * x + E[5] * y + E[6] * z + E[7],
                                     E[8] * x + E[9] * y + E[10] * z + E[11]};
                e57::Point& o = batch.emplace_back();
                for (int r = 0; r < 3; r++)
                    o.xyz[r] = R[r * 3] * p[0] + R[r * 3 + 1] * p[1] + R[r * 3 + 2] * p[2] + c[r];
                o.rgb[0] = o.rgb[1] = o.rgb[2] = (uint8_t)std::min<int64_t>(255, v[4]);
                if (batch.size() == kBatch) {
                    sink(batch.data(), batch.size());
                    batch.clear();
                }
            }
            return true;
        });
    } catch (const std::runtime_error& e) {
        fail(I.path, e.what());
    }
    if (stopped) return false;
    if (!batch.empty()) sink(batch.data(), batch.size());
    return true;
}

bool Reader::camera_pose(size_t cam, int64_t time_us, double R[9], double centre[3]) const {
    if (cam >= _cameras.size()) return false;
    double Rw[9], cw[3];
    if (!_impl->imu_pose(time_us, Rw, cw)) return false;
    double world_from_imu[16] = {Rw[0], Rw[1], Rw[2], cw[0], Rw[3], Rw[4], Rw[5], cw[1],
                                 Rw[6], Rw[7], Rw[8], cw[2], 0, 0, 0, 1};
    double T[16];
    mat4_mul(world_from_imu, _impl->imu_from_lidar, T);
    mat4_mul(T, _impl->lidar_from_cam0, T);
    mat4_mul(T, _impl->cam0_from_cam[cam].data(), T);
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 3; k++) R[r * 3 + k] = T[r * 4 + k];
        centre[r] = T[r * 4 + 3];
    }
    return true;
}

void Reader::read_frames(size_t cam, const std::function<bool(const Frame&)>& sink) {
    if (cam >= _cameras.size()) return;
    const Camera& c = _cameras[cam];
    try {
        _impl->each_message(_impl->camera_channel[cam], [&](const uint8_t* d, size_t n) {
            Frame fr;
            int64_t width = 0, height = 0;
            Fields m(d, n);
            while (m.next()) {
                if (m.wire == 0 && m.field == 3) width = (int64_t)m.value;
                if (m.wire == 0 && m.field == 4) height = (int64_t)m.value;
                if (m.wire != 2) continue;
                if (m.field == 1) {
                    Fields h(m.data, m.size);
                    while (h.next()) {
                        if (h.field == 1 && h.wire == 0) fr.counter = (int64_t)h.value;
                        if (h.field == 2 && h.wire == 0) fr.time_us = zigzag(h.value);
                    }
                } else if (m.field == 5) {
                    fr.h264.assign(m.data, m.data + m.size);
                }
            }
            if (fr.h264.empty()) return true;
            if (width != c.width || height != c.height)
                throw std::runtime_error(c.name + " of an " + _impl->device_name + ": a frame of " +
                                         std::to_string(width) + "x" + std::to_string(height) +
                                         ", calibrated for " + std::to_string(c.width) + "x" +
                                         std::to_string(c.height));
            return sink(fr);
        });
    } catch (const std::runtime_error& e) {
        fail(_impl->path, e.what());
    }
}

bool is_xbin_path(const std::string& path) {
    std::string e = fs::path(path).extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".xbin";
}

std::string capture_in_folder(const std::string& dir) {
    std::error_code ec;
    std::string found;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || !is_xbin_path(it->path().string())) continue;
        if (!found.empty()) return "";
        found = it->path().string();
    }
    return found;
}

}  // namespace spirula::xbin
