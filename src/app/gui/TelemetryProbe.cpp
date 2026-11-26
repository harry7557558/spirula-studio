// TelemetryProbe.cpp -- see TelemetryProbe.h.

#include "app/gui/TelemetryProbe.h"

#ifdef SS_TOOL_SFM
#include "sfm/core/Attitude.h"
#include "sfm/core/Exif.h"
#include "sfm/core/Telemetry.h"
#endif

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

namespace {

#ifdef SS_TOOL_SFM
bool is_photo(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".tif" || e == ".tiff";
}
#endif

TelemetryInfo probe(const std::string& path, bool is_video) {
    TelemetryInfo out;
    out.video = is_video;
    out.done = true;
#ifdef SS_TOOL_SFM
    if (is_video) {
        sfm::Telemetry t;
        std::string err;
        if (!sfm::telemetry_read(path, t, err)) return out;
        out.gyro = !t.gyro.empty();
        out.accel = !t.accel.empty();
        out.attitude = !t.orientation.empty() || !t.gravity.empty();
        out.gps = std::any_of(t.gps.begin(), t.gps.end(), sfm::gps_valid);
        if (t.carrier != sfm::TelemetryCarrier::None)
            out.carrier = sfm::telemetry_carrier_name(t.carrier);
        return out;
    }
    // Only the formats that can carry an EXIF position are counted, so a PNG
    // folder reads as "no GPS" rather than as a folder of files without one.
    std::error_code ec;
    for (fs::directory_iterator it(path, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec) || !is_photo(it->path())) continue;
        out.photos++;
        if (sfm::readExif(it->path().string()).has_gps) out.with_gps++;
        if (sfm::readCameraAttitude(it->path().string()).valid) out.with_attitude++;
    }
#else
    (void)path;
#endif
    return out;
}

}  // namespace

TelemetryProbe::~TelemetryProbe() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _quit = true;
    }
    _cv.notify_all();
    if (_worker.joinable()) _worker.join();
}

TelemetryInfo TelemetryProbe::get(const std::string& path, bool is_video) {
    if (path.empty()) return {};
    std::unique_lock<std::mutex> lk(_mu);
    auto it = _known.find(path);
    if (it != _known.end()) return it->second;
    TelemetryInfo pending;
    pending.video = is_video;
    _known[path] = pending;
    _queue.push_back({path, is_video});
    if (!_worker.joinable()) _worker = std::thread([this] { run(); });
    lk.unlock();
    _cv.notify_one();
    return pending;
}

void TelemetryProbe::run() {
    for (;;) {
        std::string path;
        bool is_video = false;
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait(lk, [this] { return _quit || !_queue.empty(); });
            if (_quit) return;
            path = _queue.front().first;
            is_video = _queue.front().second;
            _queue.pop_front();
        }
        // Outside the lock: reading a multi-gigabyte video's sample table is
        // seconds, and the panel asks about every other row while it happens.
        TelemetryInfo info = probe(path, is_video);
        std::lock_guard<std::mutex> lk(_mu);
        _known[path] = info;
    }
}

}  // namespace gui
