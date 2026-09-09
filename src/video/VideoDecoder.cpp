// The public VideoReader: a sequential RGB frame source over VideoPipeline.
//
// Everything interesting -- the demuxers, the bitstream parsers, the Vulkan
// decode loop -- lives below this file. What is here is the simplification the
// public API promises: open a file, pull frames in presentation order, get
// host-side RGB. Callers that care about throughput (spirula-sam extract) drive
// VideoPipeline directly, so they can skip converting frames they will discard.

#include "nn/core/Log.h"
#include "video/Common.h"
#include "video/Demuxer.h"
#include "video/Video.h"
#include "video/VideoPipeline.h"

namespace video {

bool probe_video(const std::string& path, VideoProbe& out, std::string& error) {
    // Both demuxers refuse a file with no supported video track, so a
    // successful open always names at least one track.
    std::unique_ptr<Demuxer> demux = open_demuxer(path, error);
    if (!demux) return false;
    const std::vector<TrackInfo>& tracks = demux->tracks();
    const TrackInfo& t = tracks[0];
    out.tracks = (int)tracks.size();
    out.width = t.width;
    out.height = t.height;
    out.frame_count = t.frame_count;
    out.fps = t.fps;
    out.codec = codec_name(t.codec);
    return true;
}

struct VideoReader::Impl {
    video::VideoPipeline pipe;
    VideoInfo   info;
    std::string error;
    bool        open = false;
};

VideoReader::VideoReader() : impl_(new Impl()) {}
VideoReader::~VideoReader() = default;

bool VideoReader::isOpen() const { return impl_->open; }
const VideoInfo& VideoReader::info() const { return impl_->info; }
const std::string& VideoReader::lastError() const { return impl_->error; }

std::string VideoReader::availability() { return video::VideoPipeline::availability(); }

bool VideoReader::open(const std::string& path) {
    impl_->open = false;
    if (!impl_->pipe.open(path, /*track=*/0, /*lookahead=*/1, impl_->error)) return false;

    const video::StreamFormat& f = impl_->pipe.format();
    impl_->info.width = f.width;
    impl_->info.height = f.height;
    impl_->info.frame_count = (int)impl_->pipe.track().frame_count;
    impl_->info.fps = impl_->pipe.track().fps;
    impl_->info.codec = video::codec_name(impl_->pipe.track().codec);
    impl_->open = true;
    return true;
}

nn::Image VideoReader::readFrame() {
    if (!impl_->open) {
        impl_->error = "readFrame: no file open";
        return {};
    }
    video::FrameHandle h;
    if (!impl_->pipe.next(h, impl_->error)) return {};  // end of stream, or error

    nn::Image out;
    if (!impl_->pipe.toImage(h, {}, out, impl_->error)) out = nn::Image{};
    impl_->pipe.release(h);
    return out;
}

}  // namespace video
