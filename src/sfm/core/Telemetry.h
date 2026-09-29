#pragma once
// The IMU and GPS a video file carries alongside its pictures, recognised by
// content rather than by extension: a GoPro `gpmd` track (GPMF), the Insta360
// trailer after the MP4, a DJI `djmd` track (protobuf), or a CAMM track.
//
// map/SensorGauge.h consumes it through core/SensorTimeline.h;
// `sfm_telemetry_test FILE` prints what a file carries. Only the sample
// tables and the telemetry bytes are read, never a picture.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sfm {

// What a camera says its pictures ARE, rather than what their shape suggests:
// a GoPro's PRJT ("EACO" on a MAX, "FSFB" on a MAX 2) and the PMOD beside it,
// which for FSFB is the rows cut off a panorama and the padding at its sides.
struct VideoProjection {
    std::string name;
    std::vector<uint32_t> mode;
};

// Every `t` is seconds from the first video frame, on the container's clock.
struct TelemetryVec {
    double t = 0;
    double x = 0, y = 0, z = 0;
};

struct TelemetryQuat {
    double t = 0;
    double w = 1, x = 0, y = 0, z = 0;
};

struct TelemetryGps {
    double t = 0;
    double unix_time = 0;      // 0 when the carrier stamps nothing absolute
    double lat = 0, lon = 0;   // degrees, WGS-84
    double alt = 0;            // metres; read only with has_alt
    bool has_alt = false;
    bool fix = false;          // the receiver itself called this a fix
    double speed = -1;         // m/s over ground; < 0 unknown
    double track = -1;         // degrees clockwise from north; < 0 unknown
    double dop = 0;            // dilution of precision; 0 unknown
};

enum class TelemetryCarrier { None, Gpmf, Insta360, DjiDvtm, Camm };
const char* telemetry_carrier_name(TelemetryCarrier c);

struct Telemetry {
    TelemetryCarrier carrier = TelemetryCarrier::None;
    std::string camera, firmware, serial;
    VideoProjection projection;
    double video_duration = 0;   // seconds, from the movie header
    double video_fps = 0;        // first video track; 0 unknown
    double video_unix_start = 0; // 0 unknown
    double frame_readout = 0;    // rolling-shutter readout, seconds; 0 unknown

    // Sensor frames are the camera's own and differ per maker; `notes` says
    // what is known. GoPro axes are already permuted by ORIN into the frame
    // its GPMF spec documents.
    std::vector<TelemetryVec> gyro;         // rad/s
    std::vector<TelemetryVec> accel;        // m/s^2, gravity included
    std::vector<TelemetryVec> magnet;       // microtesla
    std::vector<TelemetryVec> gravity;      // unit vector in the sensor frame
    std::vector<TelemetryQuat> orientation; // fused attitude, as the camera wrote it
    // The order of accel-frame components the attitude acts on. GoPro's CORI
    // takes (X, Z, Y) of the ORIN frame: measured, see the note it carries.
    std::string orientation_axes = "XYZ";
    std::vector<TelemetryGps> gps;
    std::vector<std::string> notes;

    bool empty() const {
        return gyro.empty() && accel.empty() && gravity.empty() &&
               orientation.empty() && gps.empty();
    }
};

// False with `error` set when the file cannot be opened or is not a container
// this reads. A container carrying no telemetry is not an error: `carrier`
// stays None and `out.empty()`.
bool telemetry_read(const std::string& path, Telemetry& out, std::string& error);

bool telemetry_read(const uint8_t* data, size_t size, Telemetry& out, std::string& error);

// The same over random access the caller supplies: `read` fills `n` bytes at
// `off` and returns false past the end. For a file too large to hold at once
// -- the browser tool reads it back through the File API.
using TelemetryRead = std::function<bool(uint64_t off, void* dst, size_t n)>;
bool telemetry_read(uint64_t size, const TelemetryRead& read, Telemetry& out, std::string& error);

// `Telemetry::projection` alone, without reading a sensor sample: a 360 layout
// is settled per file before anything else about it is wanted, and the
// container's own boxes carry it. Empty when the file names none.
VideoProjection video_projection(const std::string& path);

// A DJI clip's picture profile: DJI's ColorModeType, 19 = D-Log M, 0 = Normal.
// Read on the Osmo 360 (dvtm_oq101, field 2.4.1) and the Avata 360
// (dvtm_AVATA360, field 2.2.4.1); other DJI layouts are Unknown.
enum class VideoColorMode { NotRecorded, Normal, DlogM, OtherLog, Unknown };
// Why a DJI clip is Unknown.
enum class VideoColorIssue {
    None, UnknownLayout, NoColorField, MalformedColorField, UnverifiedColorCode, NoClipHeader
};
struct VideoColor {
    VideoColorMode mode = VideoColorMode::NotRecorded;
    VideoColorIssue issue = VideoColorIssue::None;
    int code = -1;        // ColorModeType as read; -1 when none was
    std::string proto;    // the clip header's proto name
};

// One djmd sample. NotRecorded when it carries no clip header.
VideoColor djmd_color(const uint8_t* sample, size_t n);
// The first djmd sample that carries a clip header; Unknown when a djmd track
// has none that reads, NotRecorded when there is no djmd track.
VideoColor video_color(const std::string& path);
VideoColor video_color(const uint8_t* data, size_t size);

// Whether the readings look like a working sensor, not whether they are
// precise: units, coverage of the video, sample-rate regularity, a gravity
// norm, a GPS that moves rather than repeating one stale fix.
struct TelemetryStreamCheck {
    size_t count = 0;
    double t_first = 0, t_last = 0;
    double rate_hz = 0;        // 1 / median interval
    double max_gap = 0;        // seconds
    size_t non_monotonic = 0;
    size_t non_finite = 0;
};

struct TelemetryCheck {
    TelemetryStreamCheck gyro, accel, gravity, orientation, gps;
    double accel_norm_median = 0;  // m/s^2 after a 0.5 s average; ~9.81 when the units are right
    double accel_norm_spread = 0;  // median |raw - averaged|: vibration
    double gyro_norm_median = 0;   // rad/s
    double orientation_norm_err = 0;  // max | |q| - 1 |
    double orientation_max_step_deg = 0;
    // Gravity consistency across streams, degrees; < 0 when not computable.
    double grav_vs_accel_deg = -1;        // GRAV against the accelerometer (both sensor frame)
    double accel_in_world_spread_deg = -1;  // accel rotated by the attitude, over the capture
    bool attitude_is_sensor_to_world = false;  // the sense of the quaternion that made it constant
    double gps_fix_fraction = 0;
    double gps_frozen_fraction = 0;  // consecutive samples repeating a position bit for bit
    size_t gps_distinct = 0;         // positions that differ from the previous sample
    double gps_longest_hold = 0;     // seconds one position was repeated
    size_t gps_outliers = 0;         // fixes dropped: DOP over 10, or over 50 m/s from the last kept
    double gps_spread_m = 0;         // RMS radius of the kept fixes about their mean
    double gps_path_m = 0;           // summed distance between consecutive kept fixes
    double gps_speed_max = 0;        // reported, m/s
    bool imu_usable = false;
    bool gps_usable = false;
    std::vector<std::string> warnings;
};

TelemetryCheck telemetry_check(const Telemetry& t);

// The fixes worth positioning from: valid, with a DOP of 10 or better, and no
// jump over 50 m/s from the previous distinct position. Returns the count
// dropped by the last two gates -- one wild MAX fix passed its own DOP.
size_t telemetry_gps_filter(const Telemetry& t, std::vector<TelemetryGps>& kept);
bool gps_valid(const TelemetryGps& g);
double telemetry_haversine_m(double lat1, double lon1, double lat2, double lon2);

// The table `sfm_telemetry_test FILE` prints: a diagnostic, English only.
std::string telemetry_report(const Telemetry& t, const TelemetryCheck& c);

}  // namespace sfm
