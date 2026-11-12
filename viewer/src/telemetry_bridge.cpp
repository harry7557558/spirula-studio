// Telemetry bridge for the standalone IMU/GPS tool (telemetry.html).
//
// Reuses src/sfm/core/Telemetry.cpp and Exif.h, referenced in place by the
// viewer's CMakeLists, so the browser sees exactly what the trainer sees.
// The file itself never leaves the browser and is never buffered whole: the
// parser's reads come back through Module.sstRead, which the scan worker
// answers with FileReaderSync over a File slice.

#include "sfm/core/Exif.h"
#include "sfm/core/Telemetry.h"

#include <emscripten.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define KEEP EMSCRIPTEN_KEEPALIVE extern "C"

// Filled by the worker before any sst_scan call.
EM_JS(int, sst_host_read, (double off, void* dst, double n), {
    return Module.sstRead(off, dst, n) ? 1 : 0;
});

namespace {

sfm::Telemetry      g_t;
sfm::TelemetryCheck g_c;
std::string         g_json;
std::string         g_report;
std::string         g_error;

// Interleaved copies handed to JS as typed-array views: (t,x,y,z) per vector
// reading, (t,w,x,y,z) per attitude, (t,shutter,iso,fnum) per exposure, and
// nine f64 per fix -- lat/lon need the double.
std::vector<float>  g_vec[4];
std::vector<float>  g_quat;
std::vector<float>  g_expo;
std::vector<double> g_gps;

void jstr(std::string& o, const std::string& s) {
    o += '"';
    for (const char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += c;
    }
    o += '"';
}

void jnum(std::string& o, double v) {
    char b[40];
    std::snprintf(b, sizeof b, "%.10g", std::isfinite(v) ? v : 0.0);
    o += b;
}

void jkv(std::string& o, const char* k, double v) {
    o += '"'; o += k; o += "\":";
    jnum(o, v);
    o += ',';
}

void jlist(std::string& o, const char* k, const std::vector<std::string>& v) {
    o += '"'; o += k; o += "\":[";
    for (size_t i = 0; i < v.size(); i++) { if (i) o += ','; jstr(o, v[i]); }
    o += "],";
}

void jstream(std::string& o, const char* k, const sfm::TelemetryStreamCheck& s) {
    o += '"'; o += k; o += "\":{";
    jkv(o, "count", (double)s.count);
    jkv(o, "tFirst", s.t_first);
    jkv(o, "tLast", s.t_last);
    jkv(o, "rate", s.rate_hz);
    jkv(o, "maxGap", s.max_gap);
    jkv(o, "nonMonotonic", (double)s.non_monotonic);
    jkv(o, "nonFinite", (double)s.non_finite);
    o.back() = '}';
    o += ',';
}

void pack(const std::vector<sfm::TelemetryVec>& in, std::vector<float>& out) {
    out.clear();
    out.reserve(in.size() * 4);
    for (const sfm::TelemetryVec& v : in) {
        out.push_back((float)v.t); out.push_back((float)v.x);
        out.push_back((float)v.y); out.push_back((float)v.z);
    }
}

void build_result() {
    g_c = sfm::telemetry_check(g_t);
    g_report = sfm::telemetry_report(g_t, g_c);
    pack(g_t.gyro, g_vec[0]);
    pack(g_t.accel, g_vec[1]);
    pack(g_t.gravity, g_vec[2]);
    pack(g_t.magnet, g_vec[3]);
    g_quat.clear();
    for (const sfm::TelemetryQuat& q : g_t.orientation) {
        g_quat.push_back((float)q.t); g_quat.push_back((float)q.w);
        g_quat.push_back((float)q.x); g_quat.push_back((float)q.y); g_quat.push_back((float)q.z);
    }
    g_expo.clear();
    for (const sfm::TelemetryExposure& e : g_t.exposure) {
        g_expo.push_back((float)e.t); g_expo.push_back((float)e.shutter);
        g_expo.push_back((float)e.iso); g_expo.push_back((float)e.fnum);
    }
    g_gps.clear();
    for (const sfm::TelemetryGps& g : g_t.gps) {
        g_gps.push_back(g.t); g_gps.push_back(g.unix_time);
        g_gps.push_back(g.lat); g_gps.push_back(g.lon);
        g_gps.push_back(g.has_alt ? g.alt : 0.0);
        g_gps.push_back((g.has_alt ? 1.0 : 0.0) + (g.fix ? 2.0 : 0.0));
        g_gps.push_back(g.speed); g_gps.push_back(g.track); g_gps.push_back(g.dop);
    }

    std::string& o = g_json;
    o.clear();
    o += '{';
    o += "\"carrier\":"; jstr(o, sfm::telemetry_carrier_name(g_t.carrier)); o += ',';
    o += "\"camera\":"; jstr(o, g_t.camera); o += ',';
    o += "\"firmware\":"; jstr(o, g_t.firmware); o += ',';
    o += "\"serial\":"; jstr(o, g_t.serial); o += ',';
    o += "\"orientationAxes\":"; jstr(o, g_t.orientation_axes); o += ',';
    jkv(o, "duration", g_t.video_duration);
    jkv(o, "fps", g_t.video_fps);
    jkv(o, "unixStart", g_t.video_unix_start);
    jkv(o, "readout", g_t.frame_readout);
    jlist(o, "notes", g_t.notes);
    o += "\"check\":{";
    jstream(o, "gyro", g_c.gyro);
    jstream(o, "accel", g_c.accel);
    jstream(o, "gravity", g_c.gravity);
    jstream(o, "orientation", g_c.orientation);
    jstream(o, "gps", g_c.gps);
    jkv(o, "accelNormMedian", g_c.accel_norm_median);
    jkv(o, "accelNormSpread", g_c.accel_norm_spread);
    jkv(o, "gyroNormMedian", g_c.gyro_norm_median);
    jkv(o, "orientationNormErr", g_c.orientation_norm_err);
    jkv(o, "orientationMaxStepDeg", g_c.orientation_max_step_deg);
    jkv(o, "gravVsAccelDeg", g_c.grav_vs_accel_deg);
    jkv(o, "accelInWorldSpreadDeg", g_c.accel_in_world_spread_deg);
    jkv(o, "attitudeSensorToWorld", g_c.attitude_is_sensor_to_world ? 1 : 0);
    jkv(o, "gpsFixFraction", g_c.gps_fix_fraction);
    jkv(o, "gpsFrozenFraction", g_c.gps_frozen_fraction);
    jkv(o, "gpsDistinct", (double)g_c.gps_distinct);
    jkv(o, "gpsLongestHold", g_c.gps_longest_hold);
    jkv(o, "gpsOutliers", (double)g_c.gps_outliers);
    jkv(o, "gpsSpreadM", g_c.gps_spread_m);
    jkv(o, "gpsPathM", g_c.gps_path_m);
    jkv(o, "gpsSpeedMax", g_c.gps_speed_max);
    jkv(o, "imuUsable", g_c.imu_usable ? 1 : 0);
    jkv(o, "gpsUsable", g_c.gps_usable ? 1 : 0);
    jlist(o, "warnings", g_c.warnings);
    o.back() = '}';
    o += '}';
}

}  // namespace

KEEP void* sst_alloc(int n) { return std::malloc((size_t)n); }
KEEP void sst_free(void* p) { std::free(p); }

// 1 when the file carries telemetry, 0 when it is a container without any,
// -1 when it is not a container this reads (sst_error says why).
KEEP int sst_scan(double size) {
    g_t = sfm::Telemetry();
    g_error.clear();
    const bool ok = sfm::telemetry_read(
        (uint64_t)size,
        [](uint64_t off, void* dst, size_t n) { return sst_host_read((double)off, dst, (double)n) != 0; },
        g_t, g_error);
    if (!ok) { g_json.clear(); g_report.clear(); return -1; }
    build_result();
    return g_t.empty() && g_t.exposure.empty() ? 0 : 1;
}

KEEP const char* sst_json() { return g_json.c_str(); }
KEEP const char* sst_report() { return g_report.c_str(); }
KEEP const char* sst_error() { return g_error.c_str(); }

// which: 0 gyro, 1 accel, 2 gravity, 3 magnet, 4 orientation, 5 gps, 6 exposure.
KEEP void* sst_stream(int which) {
    if (which >= 0 && which < 4) return g_vec[which].data();
    if (which == 4) return g_quat.data();
    if (which == 5) return g_gps.data();
    if (which == 6) return g_expo.data();
    return nullptr;
}

KEEP int sst_stream_count(int which) {
    if (which >= 0 && which < 4) return (int)(g_vec[which].size() / 4);
    if (which == 4) return (int)(g_quat.size() / 5);
    if (which == 5) return (int)(g_gps.size() / 9);
    if (which == 6) return (int)(g_expo.size() / 4);
    return 0;
}

// A still's EXIF, from the head of a JPEG. One GPS fix, one exposure and the
// lens identity is all a photo carries, so it comes back as JSON rather than a
// stream. Exposure falls back to the APEX tags, as the trainer reads them.
KEEP const char* sst_exif(const uint8_t* data, int len) {
    g_json.clear();
    sfm::ExifData e;
    const size_t n = (size_t)std::max(0, len);
    if (n > 4 && data[0] == 0xFF && data[1] == 0xD8) {
        size_t o = 2;
        while (o + 4 <= n && data[o] == 0xFF) {
            const uint8_t marker = data[o + 1];
            if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) { o += 2; continue; }
            if (marker == 0xDA || marker == 0xD9) break;
            const size_t seg = (size_t)(data[o + 2] << 8 | data[o + 3]);
            if (seg < 2 || o + 2 + seg > n) break;
            if (marker == 0xE1 && seg >= 8 && std::memcmp(&data[o + 4], "Exif\0\0", 6) == 0) {
                e = sfm::parseExifTiff(&data[o + 10], seg - 8);
                break;
            }
            o += 2 + seg;
        }
    }
    std::string& j = g_json;
    j += '{';
    jkv(j, "valid", e.valid ? 1 : 0);
    jkv(j, "hasGps", e.has_gps ? 1 : 0);
    jkv(j, "lat", e.lat_deg);
    jkv(j, "lon", e.lon_deg);
    jkv(j, "alt", e.has_alt ? e.alt_m : 0.0);
    jkv(j, "hasAlt", e.has_alt ? 1 : 0);
    jkv(j, "focalMm", e.focal_mm);
    jkv(j, "exposureTime", e.exposure_time > 0 ? e.exposure_time
                           : std::isfinite(e.shutter_apex) ? std::exp2(-e.shutter_apex) : 0.0);
    jkv(j, "fNumber", e.f_number > 0 ? e.f_number
                      : std::isfinite(e.aperture_apex) ? std::exp2(e.aperture_apex / 2) : 0.0);
    jkv(j, "iso", e.iso);
    jkv(j, "width", e.pixel_width);
    jkv(j, "height", e.pixel_height);
    j += "\"make\":"; jstr(j, e.make); j += ',';
    j += "\"model\":"; jstr(j, e.model);
    j += '}';
    return j.c_str();
}
