#pragma once

#include "app/Pano360.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <thread>

namespace gui {

struct SourceProbeInfo {
    bool done = false;
    int video_tracks = 0;
    int width = 0, height = 0;
    app::Eac360Layout eac360;
};

class SourceProbe {
public:
    ~SourceProbe();

    SourceProbeInfo get(const std::string& path, const std::string& ffmpeg_exe);
    void invalidate();
    void stop();

private:
    struct Request {
        uint64_t generation = 0;
        std::string key;
        std::string path;
        std::string ffmpeg_exe;
        std::shared_ptr<std::atomic<bool>> cancel;
    };

    void run();

    std::mutex _mu;
    std::condition_variable _cv;
    std::map<std::string, SourceProbeInfo> _known;
    std::deque<Request> _queue;
    std::thread _worker;
    uint64_t _generation = 0;
    std::shared_ptr<std::atomic<bool>> _active_cancel;
    bool _quit = false;
};

}  // namespace gui
