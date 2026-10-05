#pragma once

// The two decoders behind app/FrameExtract.h: Vulkan Video in-process
// (FrameDecodeVulkan.cpp, SS_HAVE_VIDEO builds only) and an ffmpeg child
// (FrameDecodeFfmpeg.cpp). Both walk one SelectClock and hand the chosen
// instant to one sink, so which frames are kept, and their names, do not
// depend on which decoder ran.

#include "app/FfmpegVideo.h"
#include "app/FrameExtract.h"
#include "app/FrameMotion.h"
#include "nn/io/Image.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app {

// Which decoded frames are candidates and when a window is written.
struct SelectClock {
    // A fixed rate is arithmetic on the index; `ends`, from the motion pass,
    // is the frame each window closes at. Both are walked forward only.
    int keep = 0, skip = 1;
    std::vector<int64_t> ends;
    size_t next = 0;

    bool candidate(int64_t i) {
        if (!ends.empty()) {
            while (next < ends.size() && ends[next] < i) ++next;
            return next < ends.size() && i > ends[next] - std::max(keep, 1) &&
                   i <= ends[next];
        }
        return keep == 0 ? (i % skip == 0) : (((i + keep) % skip) < keep);
    }
    bool step(int64_t i) {
        if (!ends.empty()) return next < ends.size() && i == ends[next];
        return keep == 0 || ((i + 1) % skip) == 0;
    }
};

// The chosen instant's pictures, one per track in the order they were asked
// for, and the source frame it is.
using InstantSink = std::function<bool(std::vector<nn::Image>& imgs, int64_t index,
                                       std::string& err)>;

void log_line(const FrameExtractSinks& sinks, const std::string& s);

// `convert` false hands pictures over as decoded, unscaled and unturned: a 360
// layout is measured in source pixels.
bool decode_lockstep_vulkan(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                            const std::vector<int>& tracks, bool convert,
                            const std::vector<int64_t>& plan, FrameExtractStats& t,
                            std::string& error, const InstantSink& on_frame);
bool measure_motion_vulkan(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                           int track, int tracks, MotionPlanInput& out,
                           FrameExtractStats& t, std::string& error);

// The same two over ffmpeg. `facts` is the probe the track numbers index.
bool decode_lockstep_ffmpeg(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                            const VideoFacts& facts, const std::vector<int>& tracks,
                            bool convert, const std::vector<int64_t>& plan,
                            FrameExtractStats& t, std::string& error,
                            const InstantSink& on_frame);
bool measure_motion_ffmpeg(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                           const VideoFacts& facts, int track, int tracks,
                           MotionPlanInput& out, FrameExtractStats& t,
                           std::string& error);

}  // namespace app
