#pragma once

// Where rendered frames go: image files written by stb on a few threads, an
// animated GIF, or raw RGB piped into an encoder process -- `spirula encode`
// where this build has the GPU encoder (patent-gated, src/video/), ffmpeg
// otherwise.
//
// push() is called from the GUI thread and blocks only while the bounded
// queue is full, which is what keeps a slow disk or encoder from growing it;
// full() says so first, so the GUI can draw instead of waiting.

#include "app/gui/render/RenderProject.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gui::render {

class FrameSink {
public:
    virtual ~FrameSink() = default;
    // One frame, rows top-down, channels() bytes a pixel. False once the
    // sink has failed; error() then says why.
    virtual bool push(std::vector<uint8_t>&& pixels) = 0;
    // Everything written and closed.
    virtual bool finish() = 0;
    virtual void cancel() = 0;
    virtual std::string error() const = 0;
    virtual int channels() const = 0;
    // What has been written so far.
    virtual int written() const = 0;
    // A push now would wait.
    virtual bool full() const = 0;
};

// PNG or JPEG files: `path` itself for a single photo, else `path` with the
// frame number filled into its one `%05d`.
std::unique_ptr<FrameSink> open_image_sink(const std::string& path, int width,
                                           int height, ImageFormat format, int quality);

// An animated GIF of `frames` frames at `fps`; `quality` 0..2 as for video.
std::unique_ptr<FrameSink> open_gif_sink(const std::string& path, int width, int height,
                                         double fps, int frames, int quality);

// Frames into `argv`'s stdin as rgb24. Its output is kept for the error.
std::unique_ptr<FrameSink> open_pipe_sink(const std::vector<std::string>& argv,
                                          int width, int height);

// Which program turns frames into a video file, and for ffmpeg which of
// its encoders.
struct Encoder {
    enum Kind { None = 0, BuiltIn, Ffmpeg };
    Kind kind = None;
    std::string exe;
    std::string codec;                  // ffmpeg's name for it, "libx264"...
};
// ffmpeg's encoders for `codec`, best first: the software ones make the
// better file, a GPU one is next.
const std::vector<std::string>& ffmpeg_encoders_for(Codec codec);
// The names in `ffmpeg -encoders` output.
std::vector<std::string> parse_ffmpeg_encoders(const std::vector<std::string>& lines);
// The command for an encode of `width` x `height` at `fps` into `path`.
// `quality` 0 best .. 2 smallest; `spherical` tags an equirectangular video
// as 360 where the encoder can.
std::vector<std::string> encoder_argv(const Encoder& e, int width, int height,
                                      double fps, Codec codec, int quality,
                                      bool spherical, const std::string& path);

}  // namespace gui::render
