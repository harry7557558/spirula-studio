#pragma once
// Container parsing: the seam between a file on disk and a coded picture.
//
// Two implementations, both written here because the dependency rule (AGENTS.md)
// rules out ffmpeg: ISO-BMFF (mp4/mov/insv) and Matroska (mkv/webm). Neither
// decodes anything -- they hand out coded frames, and the codec parsers in
// H264Parser/H265Parser/Av1Parser turn those into picture-level parameters for
// VK_KHR_video_decode_*.
//
// A file may carry several video tracks (an Insta360 .insv is two fisheye
// streams side by side), so tracks are enumerated and selected explicitly.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace video {

enum class Codec { Unknown, H264, H265, AV1 };
const char* codec_name(Codec c);

struct TrackInfo {
    int      index = 0;              // position among the file's video tracks
    Codec    codec = Codec::Unknown;
    int      width = 0;
    int      height = 0;
    double   fps = 0.0;
    int64_t  frame_count = 0;        // 0 when the container does not state it
    double   duration_sec = 0.0;
    // avcC / hvcC / av1C record, or Matroska CodecPrivate (same bytes).
    std::vector<uint8_t> codec_config;
    // Length in bytes of the size prefix on each NAL unit; 0 means the frame
    // data is already Annex-B (start codes) or, for AV1, a stream of OBUs.
    int      nal_length_size = 4;
    // The container's display transform: turn the decoded picture this many
    // degrees clockwise, then mirror it. A phone records a portrait clip as
    // landscape pixels plus this matrix, and nothing else says so.
    int      rotate = 0;             // 0, 90, 180 or 270
    bool     mirror = false;
};

struct Packet {
    std::vector<uint8_t> data;
    int64_t index = 0;               // position in DECODE order
    // ... and in PRESENTATION order, which is what numbers a decoded frame:
    // counting the pictures that come out instead drifts on a stream the
    // decoder drops any of, and cannot survive a seek.
    int64_t display_index = 0;
    double  pts = 0.0;               // seconds, presentation order
    double  dts = 0.0;               // seconds, decode order
    bool    is_sync = false;
};

class Demuxer {
public:
    virtual ~Demuxer() = default;

    virtual const std::vector<TrackInfo>& tracks() const = 0;
    // Rewinds to the first packet of the chosen track.
    virtual bool selectTrack(int index, std::string& error) = 0;
    // Next coded frame in DECODE order. Returns false at end of stream (with
    // `error` empty) or on a parse failure (with `error` set).
    virtual bool next(Packet& out, std::string& error) = 0;

    // Rewinds to the sync sample at or before `index`, reporting which one,
    // so a caller can decode forward from there rather than from the start.
    // False where there is no index to do it with -- read from 0 instead.
    virtual bool seekSync(int64_t index, int64_t& landed, std::string& error) {
        (void)index; (void)landed; (void)error;
        return false;
    }
};

// Picks the implementation by content, not by extension: .insv is an MP4 and
// .webm is a Matroska, and neither says so in its name.
std::unique_ptr<Demuxer> open_demuxer(const std::string& path, std::string& error);

}  // namespace video
