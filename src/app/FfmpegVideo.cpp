// FfmpegVideo.cpp -- see FfmpegVideo.h.

#include "app/FfmpegVideo.h"

#include "app/gui/Subprocess.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace app {

using gui::command_exists;
using gui::run_process;

bool ffmpeg_probe_video(const std::string& ffmpeg_exe, const std::string& path,
                        VideoFacts& out, const std::atomic<bool>& cancel) {
    out = VideoFacts{};
    if (!command_exists(ffmpeg_exe)) return false;
    // No output file, so ffmpeg prints the stream table and exits non-zero
    // for want of one: the lines are the answer, not the return code.
    run_process({ffmpeg_exe, "-nostdin", "-hide_banner", "-i", path}, "",
                [&](const std::string& line) {
                    const size_t d = line.find("Duration:");
                    if (d != std::string::npos) {
                        int hh = 0, mm = 0;
                        double ss = 0.0;
                        if (std::sscanf(line.c_str() + d, "Duration: %d:%d:%lf",
                                        &hh, &mm, &ss) == 3)
                            out.duration = hh * 3600.0 + mm * 60.0 + ss;
                    }
                    // "... 1920x1080, 19938 kb/s, 30.01 fps, 30 tbr, ..."
                    // A cover picture is a video stream to ffmpeg and is not a
                    // lens; counting it gives a DJI .osv three of them.
                    const size_t f = line.find(" fps");
                    if (f == std::string::npos ||
                        line.find("Video:") == std::string::npos ||
                        line.find("(attached pic)") != std::string::npos)
                        return;
                    // The frame size off the same line. The 16-pixel floor is
                    // what rejects the fourcc ("0x31637661"), which is also
                    // digits on both sides of an x.
                    int lw = 0, lh = 0;
                    for (size_t i = 1; i + 1 < line.size() && !lw; i++) {
                        if (line[i] != 'x') continue;
                        size_t b = i, e = i + 1;
                        while (b > 0 && std::isdigit((unsigned char)line[b - 1])) b--;
                        while (e < line.size() && std::isdigit((unsigned char)line[e])) e++;
                        if (b == i || e == i + 1) continue;
                        const int w = std::atoi(line.c_str() + b);
                        const int h = std::atoi(line.c_str() + i + 1);
                        if (w >= 16 && h >= 16) { lw = w; lh = h; }
                    }
                    if (lw > 0) {
                        // "Stream #0:1[0x2](und): Video: ..."
                        int file = 0, stream = -1;
                        const size_t st = line.find("Stream #");
                        if (st != std::string::npos)
                            std::sscanf(line.c_str() + st, "Stream #%d:%d", &file,
                                        &stream);
                        out.streams.push_back(stream);
                        out.tracks.emplace_back(lw, lh);
                        if (out.width == 0) { out.width = lw; out.height = lh; }
                    }
                    size_t b = f;
                    while (b > 0 && (std::isdigit((unsigned char)line[b - 1]) ||
                                     line[b - 1] == '.'))
                        b--;
                    if (b < f) {
                        try {
                            out.fps = std::stod(line.substr(b, f - b));
                        } catch (...) {}
                    }
                },
                cancel);
    if (out.duration > 0.0 && out.fps > 0.0)
        out.frames = (long long)(out.duration * out.fps);
    return out.duration > 0.0;
}

// ---------------------------------------------------------------------------
// Frames over a pipe
// ---------------------------------------------------------------------------

FfmpegFrames::FfmpegFrames() = default;
FfmpegFrames::~FfmpegFrames() { close(); }

bool FfmpegFrames::open(std::vector<std::string> argv, bool gray,
                        std::string& error) {
    close();
    _gray = gray;
    _at = _end = 0;
    _buf.resize(1 << 16);
    _last_line.clear();
    argv.insert(argv.end(), {"-f", "image2pipe", "-c:v", gray ? "pgm" : "ppm", "-"});
    _proc = std::make_unique<gui::ProcessReader>();
    if (!_proc->start(argv, [this](const std::string& l) { _last_line = l; })) {
        _proc.reset();
        error = "could not run " + argv[0];
        return false;
    }
    return true;
}

int FfmpegFrames::get() {
    if (_at == _end) {
        _at = 0;
        _end = _proc->read(_buf.data(), _buf.size());
        if (_end == 0) return -1;
    }
    return _buf[_at++];
}

bool FfmpegFrames::read_exact(uint8_t* dst, size_t n) {
    const size_t have = std::min(n, _end - _at);
    std::memcpy(dst, _buf.data() + _at, have);
    _at += have;
    for (size_t done = have; done < n;) {
        const size_t got = _proc->read(dst + done, n - done);
        if (got == 0) return false;
        done += got;
    }
    return true;
}

// A PNM header field: whitespace, then decimal digits.
bool FfmpegFrames::read_int(int& v) {
    int c = get();
    while (c >= 0 && std::isspace(c)) c = get();
    if (c < 0 || !std::isdigit(c)) return false;
    v = 0;
    while (c >= 0 && std::isdigit(c)) {
        v = v * 10 + (c - '0');
        c = get();
    }
    // The one whitespace byte after maxval is part of the header.
    return c >= 0 && std::isspace(c);
}

bool FfmpegFrames::finish(std::string& error) {
    const int rc = _proc->finish();
    _proc.reset();
    if (rc == 0) return true;
    error = rc == gui::kSpawnFailed ? "ffmpeg could not be started"
                                    : "ffmpeg failed: " + _last_line;
    return false;
}

bool FfmpegFrames::next(std::vector<uint8_t>& px, int& width, int& height,
                        std::string& error) {
    error.clear();
    if (!_proc) return false;
    const int p = get();
    if (p < 0) {
        finish(error);
        return false;
    }
    int maxval = 0;
    if (p != 'P' || get() != (_gray ? '5' : '6') || !read_int(width) ||
        !read_int(height) || !read_int(maxval) || maxval != 255 || width <= 0 ||
        height <= 0) {
        close();
        error = "ffmpeg wrote a picture this could not read";
        return false;
    }
    px.resize((size_t)width * height * (_gray ? 1 : 3));
    if (!read_exact(px.data(), px.size())) {
        if (finish(error)) error = "ffmpeg stopped mid-picture";
        return false;
    }
    return true;
}

void FfmpegFrames::close() {
    if (!_proc) return;
    _proc->kill();
    _proc->finish();
    _proc.reset();
}

}  // namespace app
