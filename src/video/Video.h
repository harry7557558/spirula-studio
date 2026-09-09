#pragma once
// Video decoding -- the subsystem's public header.
//
// Decoding goes through VK_KHR_video_decode_* on the same device the rest of
// the pipeline uses. There is no software fallback and no ffmpeg here: on a
// device or driver without video decode, open() fails with a message naming
// what is missing, and the caller falls back to an external ffmpeg (that is
// what src/app/gui/DatasetPrep.cpp does).
//
// The whole subsystem is compiled only when SS_ENABLE_PATENTED is ON --
// H.264 / H.265 / AV1 bitstream parsing is the patent-encumbered part of this
// repository, and a default build leaves it out. See cmake/SsOptions.cmake.

#include "nn/io/Image.h"

#include <cstdint>
#include <memory>
#include <string>

namespace video {

struct VideoInfo {
    int    width = 0;
    int    height = 0;
    int    frame_count = 0;   // 0 when the container does not state it
    double fps = 0.0;
    std::string codec;        // "h264" | "h265" | "av1"
};

// Container metadata without a decode session: the header tables the
// demuxer reads, nothing on the GPU. `tracks` counts the file's video
// tracks; width/height/fps/frame_count/codec are the first track's.
struct VideoProbe {
    int         tracks = 0;
    int         width = 0;
    int         height = 0;
    int64_t     frame_count = 0;   // 0 when the container does not state it
    double      fps = 0.0;
    std::string codec;             // "h264" | "h265" | "av1"
};

// What a file contains, without the decode session open() builds. False
// with `error` set when the file has no readable video track.
bool probe_video(const std::string& path, VideoProbe& out, std::string& error);

class VideoReader {
public:
    VideoReader();
    ~VideoReader();
    VideoReader(const VideoReader&) = delete;
    VideoReader& operator=(const VideoReader&) = delete;

    bool open(const std::string& path);
    bool isOpen() const;
    const VideoInfo& info() const;

    // Next frame in decode order, RGB. Empty image at end of stream.
    nn::Image readFrame();

    const std::string& lastError() const;

    // Why video decoding is unavailable on this device, or "" if it is
    // available. Cheap: probes without opening a file.
    static std::string availability();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace video
