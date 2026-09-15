#include "app/gui/SourceProbe.h"

#ifdef SS_HAVE_VIDEO
#include "app/FrameExtract.h"
#endif
#include "app/gui/DatasetPrep.h"

namespace gui {

namespace {

std::string probe_key(const std::string& path, const std::string& ffmpeg_exe) {
    return path + '\n' + ffmpeg_exe;
}

SourceProbeInfo probe(const std::string& path, const std::string& ffmpeg_exe,
                      const std::atomic<bool>& cancel) {
    SourceProbeInfo out;
    bool have_native_metadata = false;
#ifdef SS_HAVE_VIDEO
    std::string native_error;
    const std::vector<std::pair<int, int>> native_sizes =
        app::video_track_sizes(path, native_error);
    if (!native_sizes.empty() && !cancel.load()) {
        have_native_metadata = true;
        out.video_tracks = (int)native_sizes.size();
        out.width = native_sizes[0].first;
        out.height = native_sizes[0].second;
        if (is_pano360_path(path) && native_sizes.size() == 2 &&
            native_sizes[0] == native_sizes[1])
            app::eac360_detect(2, native_sizes[0].first, native_sizes[0].second,
                               out.eac360);
    }
#endif
    if (!cancel.load() && !have_native_metadata) {
        VideoFacts facts;
        if (ffmpeg_probe_video(ffmpeg_exe, path, facts, cancel)) {
            out.width = facts.width;
            out.height = facts.height;
            out.video_tracks = (int)facts.tracks.size();
            if (is_pano360_path(path) && facts.tracks.size() == 2 &&
                facts.tracks[0] == facts.tracks[1])
                app::eac360_detect(2, facts.tracks[0].first,
                                   facts.tracks[0].second, out.eac360);
        }
    }
    out.done = true;
    return out;
}

}  // namespace

SourceProbe::~SourceProbe() { stop(); }

SourceProbeInfo SourceProbe::get(const std::string& path,
                                 const std::string& ffmpeg_exe) {
    if (path.empty()) return {};
    const std::string key = probe_key(path, ffmpeg_exe);
    std::unique_lock<std::mutex> lk(_mu);
    auto it = _known.find(key);
    if (it != _known.end()) return it->second;
    SourceProbeInfo pending;
    _known[key] = pending;
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    _queue.push_back({_generation, key, path, ffmpeg_exe, cancel});
    if (!_worker.joinable()) _worker = std::thread([this] { run(); });
    lk.unlock();
    _cv.notify_one();
    return pending;
}

void SourceProbe::invalidate() {
    std::lock_guard<std::mutex> lk(_mu);
    ++_generation;
    _known.clear();
    _queue.clear();
    if (_active_cancel) *_active_cancel = true;
}

void SourceProbe::stop() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _quit = true;
        if (_active_cancel) *_active_cancel = true;
    }
    _cv.notify_all();
    if (_worker.joinable()) _worker.join();
}

void SourceProbe::run() {
    for (;;) {
        Request request;
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait(lk, [this] { return _quit || !_queue.empty(); });
            if (_quit) return;
            request = std::move(_queue.front());
            _queue.pop_front();
            _active_cancel = request.cancel;
        }
        SourceProbeInfo info =
            probe(request.path, request.ffmpeg_exe, *request.cancel);
        std::lock_guard<std::mutex> lk(_mu);
        if (_active_cancel == request.cancel) _active_cancel.reset();
        if (request.generation == _generation) _known[request.key] = info;
    }
}

}  // namespace gui
