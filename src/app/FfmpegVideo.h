#pragma once

// ffmpeg as a video decoder, for every tool whose build or device has no
// in-process one: what `ffmpeg -i` says a file holds, and its frames as
// pictures over a pipe. Only `ffmpeg_exe` is needed -- ffprobe is not assumed
// to be installed beside it.

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gui { class ProcessReader; }

namespace app {

// What an external ffmpeg says about a video. A zero means it did not say.
struct VideoFacts {
    double duration = 0.0;    // seconds
    double fps = 0.0;
    long long frames = 0;     // duration * fps; the container's own count is
                              // not printed by `ffmpeg -i`
    int width = 0, height = 0;   // one frame, before any scaling
    // Video streams but attached pictures, in ffmpeg's order (the built-in
    // demuxer's too). `streams` is each one's index in the file, for
    // `-map 0:N`: `0:v:N` would count a cover picture.
    std::vector<std::pair<int, int>> tracks;
    std::vector<int> streams;
};
bool ffmpeg_probe_video(const std::string& ffmpeg_exe, const std::string& path,
                        VideoFacts& out, const std::atomic<bool>& cancel);

// One ffmpeg decoding to a pipe as PPM (RGB) or PGM (grey) pictures. Each
// carries its own size, so an autorotated or scaled stream needs no guess at
// what ffmpeg made of it.
class FfmpegFrames {
public:
    FfmpegFrames();
    ~FfmpegFrames();
    FfmpegFrames(const FfmpegFrames&) = delete;
    FfmpegFrames& operator=(const FfmpegFrames&) = delete;
    // `argv` is the command up to the output, which this appends.
    bool open(std::vector<std::string> argv, bool gray, std::string& error);
    // The next picture; false at the end of the stream with `error` empty, or
    // on a failure with it set.
    bool next(std::vector<uint8_t>& px, int& width, int& height,
              std::string& error);
    // Stops the child if it is still running.
    void close();

private:
    int get();
    bool read_exact(uint8_t* dst, size_t n);
    bool read_int(int& v);
    bool finish(std::string& error);

    std::unique_ptr<gui::ProcessReader> _proc;
    std::vector<uint8_t> _buf;
    size_t _at = 0, _end = 0;
    bool _gray = false;
    std::string _last_line;   // of its stderr: what a failure says
};

}  // namespace app
