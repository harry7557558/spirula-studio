#pragma once

// What an input's sensors recorded, drawn so that a wrong one shows: the GPS
// as a metric map or a 3D track (where altitude drift is a slope), every IMU,
// GPS and exposure stream against the video's time, and the verdicts the
// reconstruction reaches on them (sfm/core/Telemetry.h). A folder of photos
// has one position per photo instead of a track, and its plots run by photo.

#include "i18n/Message.h"

#include "imgui.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace gui {

struct SensorFix {
    double t = 0.0;              // seconds into the video; the photo's index for photos
    double lat = 0.0, lon = 0.0, alt = 0.0;
    bool has_alt = false;
    float speed = -1.0f, dop = 0.0f;
    int photo = -1;
    bool outlier = false;
    bool no_fix = false;         // the receiver did not call it a fix: an outlier, drawn apart
    bool has_yaw = false;
    float yaw = 0.0f, pitch = 0.0f;   // degrees clockwise from north, up from the horizon
    double x = 0.0, y = 0.0;     // metres east and north of the data's origin
};

// What the camera exposed a frame (a photo) with; <= 0 where it did not say.
struct SensorExposure {
    float t = 0.0f;              // as SensorFix::t
    float shutter = -1.0f;       // seconds
    float iso = -1.0f, fnum = -1.0f;
};

// One quantity against the video's time (a photo's index for photos): up to
// four channels on one axis, a vector's x, y and z or several readings of it.
// Shutter, Iso and Aperture hold log2 of the value, so a stop is one height.
struct SensorSeries {
    enum Kind { Speed, Altitude, Dop, Rotation, Acceleration, Gravity, Attitude, Magnet, Step,
                Exposure, Shutter, Iso, Aperture };
    Kind kind = Speed;
    std::vector<float> t;
    std::vector<float> v[4];
    int channels = 1;
    const spirula::i18n::Msg* channel[4] = {};   // null: x, y, z
    ImU32 ink[4] = {};                           // 0: the place's own colour
    float reference = -1.0f;                     // a dashed line here, when >= 0
    double rate_hz = 0.0;
    float gap = 0.0f;                            // seconds of silence that are a gap; 0 never
    bool empty() const { return t.empty(); }
};

struct SensorData {
    bool done = false;
    bool video = false;
    std::string error;
    double duration = 0.0;
    std::string carrier, camera;
    std::vector<SensorFix> fixes;
    double lat0 = 0.0, lon0 = 0.0;
    size_t zero = 0;          // at 0, 0: no fix, ignored
    size_t no_fix = 0;        // the receiver said so; drawn, not used
    size_t outliers = 0;
    size_t distinct = 0;
    double path_m = 0.0, spread_m = 0.0, rate_hz = 0.0, imu_rate_hz = 0.0;
    std::vector<SensorSeries> series;
    std::vector<SensorExposure> exposure;   // in time order
    bool has_gps = false, has_imu = false, gps_usable = false, imu_usable = false;
    std::vector<std::string> warnings;   // English diagnostics, as the checks wrote them
    std::string report;
    int photos = 0, scanned = 0, with_gps = 0, with_attitude = 0, with_exposure = 0;
};

class SensorView {
public:
    ~SensorView();

    // Nothing reads anything until one of these; the same key again is free.
    void load_video(const std::string& key, const std::string& path);
    void load_photos(const std::string& key, std::vector<std::string> files);
    void clear();
    const std::string& key() const { return _key; }

    // What is known so far; a photo folder fills in as it is read.
    std::shared_ptr<const SensorData> data() const;
    // Whether this build can read a video's sensors at all.
    static bool reads_video();

    struct Result {
        double seek = -1.0;    // a point on the track or a plot was clicked
        int open_photo = -1;
    };
    // `playhead` and the clip are the video's, in seconds; < 0 for photos.
    Result draw(float height, double playhead, double clip_start, double clip_end);

private:
    void join();
    void draw_status(const SensorData& d);
    void draw_map(const SensorData& d, const ImVec2& size, double playhead, Result& r);
    void draw_track3d(const SensorData& d, const ImVec2& size, double playhead, Result& r);
    void draw_series(const SensorData& d, const SensorSeries& s, float height, double playhead,
                     double clip_start, double clip_end, Result& r);

    std::string _key;
    std::thread _worker;
    std::atomic<bool> _cancel{false};
    mutable std::mutex _mu;
    std::shared_ptr<const SensorData> _data;

    // The map's view: metres at its centre, pixels per metre.
    double _cx = 0.0, _cy = 0.0, _scale = 0.0;
    bool _fit = true;
    // The 3D track's orbit: degrees from north and above the horizon, metres.
    bool _track3d = false;
    double _yaw = 30.0, _pitch = 30.0, _dist = 0.0;
    double _target[3] = {0, 0, 0};
    bool _fit3d = true;
    int _height_scale = 1;           // 0 picks a stretch, else one of those offered
    // The plots' shared span of time (of photos), and which of them are hidden.
    double _win0 = 0.0, _win1 = 0.0, _full = 1.0;
    bool _zoomed = false;
    std::set<int> _hidden{SensorSeries::Dop, SensorSeries::Gravity, SensorSeries::Magnet,
                          SensorSeries::Shutter, SensorSeries::Iso, SensorSeries::Aperture};
};

}  // namespace gui
