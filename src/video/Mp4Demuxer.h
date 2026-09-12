#pragma once
// ISO base media file format (mp4 / mov / m4v / insv).
//
// Scope: every video track's sample table, in decode order, with composition
// offsets applied so presentation timestamps are right for a stream with
// B-frames. No audio, no edit lists, no fragmented MP4 -- `moof` is reported by
// name rather than producing silent garbage.

#include "video/Demuxer.h"

#include <fstream>

namespace video {

class Mp4Demuxer : public Demuxer {
public:
    bool open(const std::string& path, std::string& error);

    const std::vector<TrackInfo>& tracks() const override { return infos_; }
    bool selectTrack(int index, std::string& error) override;
    bool next(Packet& out, std::string& error) override;
    bool seekSync(int64_t index, int64_t& landed, std::string& error) override;

private:
    struct Sample {
        uint64_t offset = 0;
        uint32_t size = 0;
        int64_t  dts = 0;      // timescale units
        int64_t  pts = 0;
        bool     is_sync = false;
    };
    struct Track {
        TrackInfo info;
        uint32_t  timescale = 0;
        uint64_t  duration = 0;
        std::vector<Sample> samples;
        // Each sample's rank in composition-time order; a stream with
        // B-frames decodes them out of that order.
        std::vector<uint32_t> by_pts, pts_rank;
    };

    void buildPresentationOrder(Track& tk);
    bool parseMoov(const uint8_t* data, size_t size, std::string& error);
    bool parseTrak(const uint8_t* data, size_t size);

    std::string   path_;
    std::ifstream file_;
    std::vector<Track>     tracks_;
    std::vector<TrackInfo> infos_;
    int    selected_ = -1;
    size_t next_sample_ = 0;
};

}  // namespace video
