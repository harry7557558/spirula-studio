// TelemetryProbe.cpp -- see TelemetryProbe.h.

#include "app/gui/TelemetryProbe.h"
#include "app/gui/DatasetPrep.h"

#ifdef SS_TOOL_SFM
#include "sfm/core/Attitude.h"
#include "sfm/core/Exif.h"
#include "sfm/core/Telemetry.h"
#endif

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

enum class WalkResult { Complete, Stopped, Failed };

template <typename Read>
WalkResult walk_photos(const fs::path& images, const fs::path& skip,
                       const std::atomic<bool>* cancel, Read read) {
    std::error_code ec;
    const fs::path root = fs::canonical(images, ec);
    if (ec) return WalkResult::Failed;
    std::vector<fs::path> ancestors{root};
    const auto options = fs::directory_options::follow_directory_symlink;
    fs::recursive_directory_iterator it(images, options, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (cancel && cancel->load()) return WalkResult::Stopped;
        const fs::path file = it->path();
        if (it->is_directory(ec)) {
            const fs::path target = fs::canonical(file, ec);
            if (ec) break;
            ancestors.resize((size_t)it.depth() + 1);
            if (!skip.empty() && target == skip) {
                it.disable_recursion_pending();
                continue;
            }
            if (std::find(ancestors.begin(), ancestors.end(), target) != ancestors.end())
                return WalkResult::Failed;
            ancestors.push_back(target);
        } else if (!ec && it->is_regular_file(ec) && is_image_file(file)) {
            if (!read(file)) return WalkResult::Stopped;
        }
    }
    return ec ? WalkResult::Failed : WalkResult::Complete;
}

TelemetryInfo probe(const std::string& path, bool is_video,
                    const std::atomic<bool>* cancel) {
    if (!is_video) return probe_photo_telemetry(path, cancel);
    TelemetryInfo out;
    out.video = is_video;
    if (cancel && cancel->load()) return out;
#ifdef SS_TOOL_SFM
    sfm::Telemetry t;
    std::string err;
    if (!sfm::telemetry_read(path, t, err)) {
        out.failed = true;
        return out;
    }
    out.gyro = !t.gyro.empty();
    out.accel = !t.accel.empty();
    out.attitude = !t.orientation.empty() || !t.gravity.empty();
    out.gps = !t.gps.empty();
    if (t.carrier != sfm::TelemetryCarrier::None)
        out.carrier = sfm::telemetry_carrier_name(t.carrier);
#else
    (void)path;
#endif
    out.done = !(cancel && cancel->load());
    return out;
}

}  // namespace

TelemetryInfo probe_photo_telemetry(const std::string& path,
                                   const std::atomic<bool>* cancel) {
    TelemetryInfo out;
    auto cancelled = [&] { return cancel && cancel->load(); };
    if (cancelled()) return out;
    try {
        std::error_code ec;
        if (!fs::is_directory(fs::path(path), ec) || ec) {
            out.failed = true;
            return out;
        }
        fs::path images(path), skip;
        const fs::path child = images / "images";
        const bool have_child = fs::exists(child, ec);
        if (ec) { out.failed = true; return out; }
        if (have_child && fs::is_directory(child, ec)) {
            bool found = false;
            const auto result = walk_photos(child, {}, cancel, [&](const fs::path&) {
                found = true;
                return false;
            });
            if (result == WalkResult::Failed) { out.failed = true; return out; }
            if (cancelled()) return out;
            if (found) images = child;
        }
        if (ec) { out.failed = true; return out; }
        const fs::path masks = images / "masks";
        const bool have_masks = fs::exists(masks, ec);
        if (ec) { out.failed = true; return out; }
        if (have_masks && fs::is_directory(masks, ec)) skip = fs::canonical(masks, ec);
        if (ec) { out.failed = true; return out; }
        const auto result = walk_photos(images, skip, cancel, [&](const fs::path& file) {
            out.photos++;
#ifdef SS_TOOL_SFM
            const sfm::ExifData exif = sfm::readExif(file.string());
            if (exif.has_gps && std::isfinite(exif.lat_deg) &&
                std::isfinite(exif.lon_deg) && std::abs(exif.lat_deg) <= 90 &&
                std::abs(exif.lon_deg) <= 180 &&
                (!exif.has_alt || std::isfinite(exif.alt_m)))
                out.with_gps++;
            if (cancelled()) return false;
            if (sfm::readCameraAttitude(file.string()).valid) out.with_attitude++;
#endif
            return true;
        });
        out.failed = result == WalkResult::Failed;
        out.done = result == WalkResult::Complete && !cancelled();
    } catch (const std::exception&) {
        out.failed = true;
    }
    return out;
}

TelemetryProbe::~TelemetryProbe() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _quit.store(true);
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
            _cv.wait(lk, [this] { return _quit.load() || !_queue.empty(); });
            if (_quit.load()) return;
            path = _queue.front().first;
            is_video = _queue.front().second;
            _queue.pop_front();
        }
        // Outside the lock: reading a multi-gigabyte video's sample table is
        // seconds, and the panel asks about every other row while it happens.
        TelemetryInfo info;
        try {
            info = probe(path, is_video, &_quit);
        } catch (const std::exception&) {
            info.failed = true;
        }
        std::lock_guard<std::mutex> lk(_mu);
        if (_quit.load()) return;
        _known[path] = info;
    }
}

}  // namespace gui
