// FrameSink.cpp -- see FrameSink.h.

#include "app/gui/render/FrameSink.h"

#include "app/gui/Subprocess.h"
#include "app/gui/render/GifWriter.h"
#include "core/HardwareThreads.h"

#include "external/stb_image_write.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace gui::render {

namespace {

// A bounded queue drained by `threads` workers, each frame tagged with its
// number so image files can be written out of order.
class Queue {
public:
    struct Item { int index; std::vector<uint8_t> px; };

    void start(int threads, std::function<bool(Item&)> work, size_t limit) {
        _work = std::move(work);
        _limit = limit;
        for (int i = 0; i < threads; i++) _threads.emplace_back([this] { run(); });
    }
    bool push(Item&& it) {
        std::unique_lock<std::mutex> lk(_mu);
        _space.wait(lk, [&] { return _q.size() < _limit || _failed || _stop; });
        if (_failed || _stop) return false;
        _q.push_back(std::move(it));
        _ready.notify_one();
        return true;
    }
    // Drain (or drop, when `drop`) and join.
    void close(bool drop) {
        {
            std::lock_guard<std::mutex> lk(_mu);
            _closing = true;
            if (drop) { _q.clear(); _stop = true; }
        }
        _ready.notify_all();
        _space.notify_all();
        for (auto& t : _threads) t.join();
        _threads.clear();
    }
    bool failed() const { return _failed.load(); }
    int done() const { return _done.load(); }
    bool full() const {
        std::lock_guard<std::mutex> lk(_mu);
        return _q.size() >= _limit && !_failed && !_stop;
    }

private:
    void run() {
        for (;;) {
            Item it;
            {
                std::unique_lock<std::mutex> lk(_mu);
                _ready.wait(lk, [&] { return !_q.empty() || _closing; });
                if (_q.empty()) return;
                it = std::move(_q.front());
                _q.pop_front();
            }
            _space.notify_one();
            if (_failed) continue;
            if (!_work(it)) {
                _failed = true;
                _space.notify_all();
            } else {
                _done++;
            }
        }
    }

    std::function<bool(Item&)> _work;
    size_t _limit = 4;
    std::vector<std::thread> _threads;
    mutable std::mutex _mu;
    std::condition_variable _ready, _space;
    std::deque<Item> _q;
    bool _closing = false, _stop = false;
    std::atomic<bool> _failed{false};
    std::atomic<int> _done{0};
};

class ImageSink : public FrameSink {
public:
    ImageSink(std::string path, int w, int h, ImageFormat format, int quality)
        : _path(std::move(path)), _w(w), _h(h), _jpeg(format == ImageFormat::Jpeg),
          _quality(quality), _c(format == ImageFormat::PngAlpha ? 4 : 3) {
        const int threads = std::clamp((int)spirula::hardware_threads() - 2, 1, 6);
        _q.start(threads, [this](Queue::Item& it) { return write(it); },
                 (size_t)threads * 2);
    }
    ~ImageSink() override { _q.close(true); }
    bool push(std::vector<uint8_t>&& px) override {
        return _q.push({_next++, std::move(px)});
    }
    bool finish() override {
        _q.close(false);
        return !_q.failed();
    }
    void cancel() override { _q.close(true); }
    std::string error() const override {
        std::lock_guard<std::mutex> lk(_err_mu);
        return _error;
    }
    int channels() const override { return _c; }
    int written() const override { return _q.done(); }
    bool full() const override { return _q.full(); }

private:
    bool write(Queue::Item& it) {
        std::string file = _path;
        const size_t at = file.find("%05d");
        if (at != std::string::npos) {
            char num[16];
            std::snprintf(num, sizeof num, "%05d", it.index + 1);
            file.replace(at, 4, num);
        }
        const int ok = _jpeg
            ? stbi_write_jpg(file.c_str(), _w, _h, _c, it.px.data(), _quality)
            : stbi_write_png(file.c_str(), _w, _h, _c, it.px.data(), _w * _c);
        if (!ok) {
            std::lock_guard<std::mutex> lk(_err_mu);
            if (_error.empty()) _error = file;
        }
        return ok != 0;
    }

    std::string _path;
    int _w, _h;
    bool _jpeg;
    int _quality, _c;
    int _next = 0;
    Queue _q;
    mutable std::mutex _err_mu;
    std::string _error;
};

// Frames compressed on several threads, written in order by whichever
// finishes the next one due.
class GifSink : public FrameSink {
public:
    GifSink(const std::string& path, int w, int h, double fps, int frames, int quality)
        : _w(w), _h(h), _fps(fps), _frames(frames) {
        static const int kColours[3] = {256, 192, 96};
        _colours = kColours[std::clamp(quality, 0, 2)];
        _ok = _gif.open(path, w, h);
        if (!_ok) _error = path;
        const int threads = std::clamp((int)spirula::hardware_threads() / 2, 1, 4);
        _q.start(threads, [this](Queue::Item& it) { return encode(it); }, (size_t)threads * 2);
    }
    ~GifSink() override { _q.close(true); }
    bool push(std::vector<uint8_t>&& px) override {
        if (!_ok) return false;
        return _q.push({_next++, std::move(px)});
    }
    bool finish() override {
        _q.close(false);
        std::lock_guard<std::mutex> lk(_mu);
        return _ok && !_q.failed() && _gif.close();
    }
    void cancel() override { _q.close(true); }
    std::string error() const override {
        std::lock_guard<std::mutex> lk(_mu);
        return _error;
    }
    int channels() const override { return 3; }
    int written() const override { return _q.done(); }
    bool full() const override { return _q.full(); }

private:
    bool encode(Queue::Item& it) {
        const int delay = gif_delay_cs(it.index, _frames, _fps);
        std::vector<uint8_t> block;
        if (delay > 0) block = gif_encode_frame(it.px.data(), _w, _h, _colours, delay);
        std::lock_guard<std::mutex> lk(_mu);
        _ready[it.index] = std::move(block);
        while (!_ready.empty() && _ready.begin()->first == _written) {
            const std::vector<uint8_t>& b = _ready.begin()->second;
            if (!b.empty() && !_gif.write(b)) {
                _ok = false;
                if (_error.empty()) _error = "write failed";
            }
            _ready.erase(_ready.begin());
            _written++;
        }
        return _ok;
    }

    int _w, _h;
    double _fps;
    int _frames, _colours = 256;
    int _next = 0, _written = 0;
    bool _ok = false;
    GifWriter _gif;
    std::map<int, std::vector<uint8_t>> _ready;
    mutable std::mutex _mu;
    std::string _error;
    Queue _q;
};

class PipeSink : public FrameSink {
public:
    PipeSink(const std::vector<std::string>& argv, int w, int h) : _w(w), _h(h) {
        _ok = _proc.start(argv, [this](const std::string& line) {
            std::lock_guard<std::mutex> lk(_log_mu);
            _tail.push_back(line);
            if (_tail.size() > 12) _tail.pop_front();
        });
        if (!_ok) _error = argv.empty() ? std::string() : argv[0];
        // One writer: the encoder takes frames in order.
        _q.start(1, [this](Queue::Item& it) {
            return _proc.write(it.px.data(), it.px.size());
        }, 3);
    }
    ~PipeSink() override {
        _q.close(true);
        _proc.kill();
    }
    bool push(std::vector<uint8_t>&& px) override {
        if (!_ok) return false;
        return _q.push({0, std::move(px)});
    }
    bool finish() override {
        _q.close(false);
        if (!_ok) return false;
        const int code = _proc.finish();
        _ok = false;
        if (code != 0 || _q.failed()) {
            std::lock_guard<std::mutex> lk(_log_mu);
            for (const std::string& l : _tail) _error += (_error.empty() ? "" : "\n") + l;
            if (_error.empty()) _error = "exit " + std::to_string(code);
            return false;
        }
        return true;
    }
    void cancel() override {
        _q.close(true);
        _proc.kill();
        _proc.finish();
        _ok = false;
    }
    std::string error() const override { return _error; }
    int channels() const override { return 3; }
    int written() const override { return _q.done(); }
    bool full() const override { return _q.full(); }

private:
    int _w, _h;
    ProcessPipe _proc;
    bool _ok = false;
    Queue _q;
    std::mutex _log_mu;
    std::deque<std::string> _tail;
    std::string _error;
};

}  // namespace


std::unique_ptr<FrameSink> open_image_sink(const std::string& path, int width,
                                           int height, ImageFormat format, int quality) {
    std::error_code ec;
    const fs::path p = fs::u8path(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    return std::make_unique<ImageSink>(path, width, height, format, quality);
}

std::unique_ptr<FrameSink> open_gif_sink(const std::string& path, int width, int height,
                                         double fps, int frames, int quality) {
    std::error_code ec;
    const fs::path p = fs::u8path(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    return std::make_unique<GifSink>(path, width, height, fps, frames, quality);
}

const std::vector<std::string>& ffmpeg_encoders_for(Codec codec) {
    static const std::vector<std::string> k264 = {"libx264", "h264_nvenc", "h264_qsv", "h264_amf",
                                                  "h264_videotoolbox", "libopenh264"};
    static const std::vector<std::string> k265 = {"libx265", "hevc_nvenc", "hevc_qsv", "hevc_amf",
                                                  "hevc_videotoolbox"};
    static const std::vector<std::string> kAv1 = {"libsvtav1", "libaom-av1", "librav1e",
                                                  "av1_nvenc", "av1_qsv", "av1_amf"};
    static const std::vector<std::string> kNone;
    switch (codec) {
        case Codec::H264: return k264;
        case Codec::H265: return k265;
        case Codec::Av1:
        case Codec::Av1Webm: return kAv1;
        default: return kNone;
    }
}

std::vector<std::string> parse_ffmpeg_encoders(const std::vector<std::string>& lines) {
    // " V....D libx264   libx264 H.264 ...": flags, then the name.
    std::vector<std::string> out;
    for (const std::string& l : lines) {
        std::istringstream in(l);
        std::string flags, name;
        if (!(in >> flags >> name)) continue;
        if (flags.size() == 6 && flags[0] == 'V' && name != "=") out.push_back(name);
    }
    return out;
}

std::unique_ptr<FrameSink> open_pipe_sink(const std::vector<std::string>& argv,
                                          int width, int height) {
    return std::make_unique<PipeSink>(argv, width, height);
}

std::vector<std::string> encoder_argv(const Encoder& e, int width, int height,
                                      double fps, Codec codec, int quality,
                                      bool spherical, const std::string& path) {
    char size[32], rate[32];
    std::snprintf(size, sizeof size, "%dx%d", width, height);
    std::snprintf(rate, sizeof rate, "%.6g", fps);
    quality = std::clamp(quality, 0, 2);
    static const char* const kCodecArg[3] = {"h264", "h265", "av1"};
    if (e.kind == Encoder::BuiltIn) {
        std::vector<std::string> a = {e.exe, "encode", "--size", size,
                                      "--fps", rate, "--codec", kCodecArg[std::min((int)codec, 2)],
                                      "--quality", std::to_string(quality)};
        if (spherical) a.push_back("--spherical");
        a.push_back("-o");
        a.push_back(path);
        return a;
    }
    std::vector<std::string> a = {e.exe, "-hide_banner", "-loglevel", "error",
                                  "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                                  "-s", size, "-r", rate, "-i", "-", "-c:v", e.codec};
    auto add = [&](std::initializer_list<std::string> v) { a.insert(a.end(), v); };
    const std::string& c = e.codec;
    // Settings that land each encoder near the same look per quality step.
    if (c == "libx264") {
        static const int kCrf[3] = {16, 20, 26};
        add({"-preset", "medium", "-crf", std::to_string(kCrf[quality])});
    } else if (c == "libx265") {
        static const int kCrf[3] = {20, 24, 30};
        add({"-preset", "medium", "-crf", std::to_string(kCrf[quality])});
    } else if (c == "libsvtav1") {
        static const int kCrf[3] = {24, 32, 40};
        add({"-preset", "6", "-crf", std::to_string(kCrf[quality])});
    } else if (c == "libaom-av1") {
        static const int kCrf[3] = {24, 32, 40};
        add({"-cpu-used", "6", "-row-mt", "1", "-b:v", "0", "-crf", std::to_string(kCrf[quality])});
    } else if (c == "librav1e") {
        static const int kQp[3] = {60, 90, 120};
        add({"-speed", "6", "-qp", std::to_string(kQp[quality])});
    } else if (c.find("nvenc") != std::string::npos) {
        static const int kCq[3] = {19, 24, 30};
        add({"-preset", "p5", "-rc", "vbr", "-cq", std::to_string(kCq[quality]), "-b:v", "0"});
    } else if (c.find("qsv") != std::string::npos) {
        static const int kQ[3] = {20, 25, 30};
        add({"-global_quality", std::to_string(kQ[quality])});
    } else {
        static const double kBpp[3] = {0.25, 0.12, 0.06};
        add({"-b:v", std::to_string((long long)(kBpp[quality] * width * height * fps))});
    }
    add({"-pix_fmt", "yuv420p"});
    if (codec == Codec::H265) add({"-tag:v", "hvc1"});
    if (codec != Codec::Av1Webm) add({"-movflags", "+faststart"});
    a.push_back(path);
    return a;
}

}  // namespace gui::render
