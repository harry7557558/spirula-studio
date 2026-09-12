#pragma once
// Decode a video file to RGB frames, entirely on the GPU.
//
//   container  ->  CodecDecoder  ->  vkCmdDecodeVideoKHR  ->  YCbCr image
//                                                              |
//                                        sharpness metric  <---+---> RGB frame
//
// Two properties shape the API. First, decoding and *using* a frame are
// separated: a caller that keeps only the sharpest frame of every window should
// not pay to convert and download the others, so pixels are touched only when
// toImage() or queueSharpness() ask for it. Second, frames come back in
// presentation order, which means holding a reorder queue -- so a frame is a
// handle into a pool, explicitly released, rather than a value.
//
// Public entry points never throw; failures return false with a message.

#include "nn/io/Image.h"
#include "video/CodecDecoder.h"
#include "video/Demuxer.h"

#include <memory>
#include <string>
#include <vector>

namespace video {

struct FrameHandle {
    int64_t index = -1;      // presentation order, from zero
    double  pts = 0.0;       // seconds
    int     slot = -1;       // picture-pool slot; < 0 when invalid
    bool valid() const { return slot >= 0; }
};

struct ConvertOpts {
    float scale = 1.0f;   // < 1 downscales; 1.0 is a straight copy
    int   rotate = 0;     // 0, 90, 180 or 270, clockwise
};

class VideoPipeline {
public:
    VideoPipeline();
    ~VideoPipeline();
    VideoPipeline(const VideoPipeline&) = delete;
    VideoPipeline& operator=(const VideoPipeline&) = delete;

    // Why Vulkan video decoding is unavailable here, or "" when it works.
    static std::string availability();

    // `lookahead` is how many decoded frames the caller may hold at once (the
    // motion-blur window); the picture pool is sized from it.
    bool open(const std::string& path, int track, int lookahead, std::string& error);

    const std::vector<TrackInfo>& tracks() const;
    const TrackInfo&              track() const;
    const StreamFormat&           format() const;

    // Next frame in presentation order. Returns false at end of stream, with
    // `error` empty; a decode failure sets it.
    bool next(FrameHandle& out, std::string& error);
    void release(FrameHandle& h);

    // To the sync sample at or before `index`, reporting the frame the next
    // next() hands back; false where the container carries no index, leaving
    // the pipeline as it was. Release every FrameHandle first.
    bool seek(int64_t index, int64_t& landed, std::string& error);

    // Records the sharpness reduction for a live frame. Values become readable
    // after flushSharpness(), which costs one queue sync for the whole batch.
    void  queueSharpness(const FrameHandle& h);
    void  flushSharpness();
    float sharpness(const FrameHandle& h) const;

    // Converts to host RGB. Blocks until the frame is ready.
    bool toImage(const FrameHandle& h, const ConvertOpts& opts, nn::Image& out,
                 std::string& error);
    // Output geometry for `opts`, so a caller can size buffers up front.
    void outputSize(const ConvertOpts& opts, int& w, int& h) const;

    // Frames decoded so far, for the timing report.
    int64_t decodedCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace video
