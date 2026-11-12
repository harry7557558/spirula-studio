// `spirula encode` -- raw RGB frames on stdin to an MP4 (or a raw H.264,
// H.265 or AV1 stream), encoded on the GPU. What the GUI's render mode pipes its
// frames into; a separate process because it needs a Vulkan device of its
// own beside the engine's (AGENTS.md, "Three Vulkan devices"). PATENT-GATED.

#include "core/BufferedFile.h"
#include "app/Tools.h"
#include "i18n/catalog/Render.h"
#include "nn/vk/Context.h"
#include "video/Mp4Writer.h"
#include "video/VideoEncoder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace msg = spirula::i18n::msg::render;
using spirula::i18n::format;

namespace {

bool ends_with(const std::string& s, const char* tail) {
    const size_t n = std::strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

const char* const kCodecNames[3] = {"h264", "h265", "av1"};

// Encode two small frames with each codec: a device that lists an encode
// queue can still refuse a session, and the GUI should learn that here --
// with the largest frame each takes, so it knows when to use ffmpeg instead.
int probe() {
    int found = 0;
    for (int c = 0; c < 3; c++) {
        video::EncodeOptions o;
        o.codec = (video::Codec)c;
        o.width = 256;
        o.height = 144;
        video::VideoEncoder e;
        std::string err;
        std::vector<uint8_t> rgb((size_t)o.width * o.height * 3, 128), au;
        bool sync = false, ok = e.open(o, err);
        for (int i = 0; i < 2 && ok; i++) ok = e.encode(rgb.data(), au, sync, err) && !au.empty();
        if (!ok) {
            // Why not goes to stderr; stdout is the list a caller reads.
            std::fprintf(stderr, "%s: %s\n", kCodecNames[c],
                         format(msg::encode_failed, {err}).c_str());
            continue;
        }
        int w = 0, h = 0;
        e.max_size(w, h);
        std::printf("%s %d %d\n", kCodecNames[c], w, h);
        found++;
    }
    return found ? 0 : 1;
}

}  // namespace

int spirula_encode_main(int argc, char** argv) {
    app::set_program_name(argv[0], "spirula encode");
    video::EncodeOptions o;
    std::string out;
    bool spherical = false, want_probe = false;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--probe") want_probe = true;
        else if (a == "--size") std::sscanf(next(), "%dx%d", &o.width, &o.height);
        else if (a == "--fps") o.fps = std::atof(next());
        else if (a == "--codec") {
            const std::string c = next();
            o.codec = c == "h265" ? video::Codec::H265 : c == "av1" ? video::Codec::Av1
                                                                    : video::Codec::H264;
        }
        else if (a == "--quality") o.quality = std::atoi(next());
        else if (a == "--spherical") spherical = true;
        else if (a == "-o" || a == "--output") out = next();
        else if (a == "-h" || a == "--help") {
            std::printf("%s\n", format(msg::encode_usage, {app::program_name()}).c_str());
            return 0;
        }
    }
    nn::vk::ContextOptions co;
    co.want_encode = true;
    try {
        nn::vk::Context::get(co);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", format(msg::encode_failed, {e.what()}).c_str());
        return 1;
    }
    if (want_probe) return probe();
    if (out.empty() || o.width <= 0 || o.height <= 0) {
        std::fprintf(stderr, "%s\n", format(msg::encode_usage, {app::program_name()}).c_str());
        return 2;
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    video::VideoEncoder enc;
    std::string err;
    if (!enc.open(o, err)) {
        std::fprintf(stderr, "%s\n", format(msg::encode_failed, {err}).c_str());
        return 1;
    }
    // A raw stream carries its parameter sets before every key frame (AV1's
    // after a temporal delimiter opening every frame); an MP4 keeps them in
    // the sample entry.
    const bool av1 = o.codec == video::Codec::Av1;
    const bool raw = ends_with(out, ".h264") || ends_with(out, ".264") ||
                     ends_with(out, ".h265") || ends_with(out, ".265") ||
                     ends_with(out, ".hevc") || ends_with(out, ".obu");
    video::Mp4Writer mp4;
    std::unique_ptr<char[]> raw_buffer;
    std::FILE* raw_file = nullptr;
    if (raw) {
        raw_buffer.reset(new char[spirula::kFileBufferBytes]);
        raw_file = std::fopen(out.c_str(), "wb");
        if (!raw_file) err = "cannot create " + out;
        else std::setvbuf(raw_file, raw_buffer.get(), _IOFBF, spirula::kFileBufferBytes);
    } else if (mp4.open(out, o.codec, o.width + (o.width & 1), o.height + (o.height & 1), o.fps,
                        spherical, err)) {
        // 4:2:0 crops in pairs of pixels, so an odd size decodes one larger.
        mp4.set_parameter_sets(enc.headers());
    }
    if (!err.empty()) {
        std::fprintf(stderr, "%s\n", format(msg::encode_failed, {err}).c_str());
        return 1;
    }

    const size_t frame_bytes = (size_t)o.width * o.height * 3;
    std::vector<uint8_t> rgb(frame_bytes), au;
    long long frames = 0;
    for (;;) {
        size_t got = 0;
        while (got < frame_bytes) {
            const size_t n = std::fread(rgb.data() + got, 1, frame_bytes - got, stdin);
            if (n == 0) break;
            got += n;
        }
        if (got < frame_bytes) break;
        bool sync = false;
        if (!enc.encode(rgb.data(), au, sync, err)) break;
        bool ok = true;
        if (raw_file) {
            static const uint8_t kDelimiter[2] = {0x12, 0x00};
            if (av1) ok = std::fwrite(kDelimiter, 1, 2, raw_file) == 2;
            if (sync) ok = ok && std::fwrite(enc.headers().data(), 1, enc.headers().size(), raw_file) ==
                                     enc.headers().size();
            ok = ok && std::fwrite(au.data(), 1, au.size(), raw_file) == au.size();
        } else {
            ok = mp4.write_sample(au.data(), au.size(), sync);
        }
        if (!ok) {
            err = "cannot write " + out;
            break;
        }
        frames++;
    }
    if (raw_file && std::fclose(raw_file) != 0 && err.empty()) err = "cannot write " + out;
    if (!raw && err.empty()) mp4.close(err);
    if (!err.empty()) {
        std::fprintf(stderr, "%s\n", format(msg::encode_failed, {err}).c_str());
        return 1;
    }
    std::printf("%s\n", format(msg::encode_done, {frames, out}).c_str());
    return 0;
}
