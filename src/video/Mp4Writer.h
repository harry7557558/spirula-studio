#pragma once
// Writes an ISO-BMFF (.mp4) file around an H.264, H.265 or AV1 stream: ftyp,
// one mdat the samples are appended to as they come, and the moov at the
// end, once every sample's size is known.
//
// H.264 and H.265 samples arrive as Annex-B access units; the parameter sets
// go into the sample entry (avcC / hvcC) and the samples are rewritten with
// 4-byte length prefixes. AV1 samples are OBUs, kept as they are with the
// sequence header in av1C and at every key frame. A 360 video is tagged the
// two ways players look for: Spherical Video V1 (an XMP uuid box) and V2.

#include "core/BufferedFile.h"
#include "video/VideoEncoder.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace video {

class Mp4Writer {
public:
    ~Mp4Writer();
    // False with `error` set when the file cannot be created.
    bool open(const std::string& path, Codec codec, int width, int height,
              double fps, bool spherical, std::string& error);
    // The stream's parameter sets: Annex-B SPS and PPS (and VPS for H.265),
    // or AV1's sequence header OBU.
    void set_parameter_sets(const std::vector<uint8_t>& headers);
    // One frame in presentation order. `sync` marks a random-access point.
    bool write_sample(const uint8_t* annexb, size_t bytes, bool sync);
    bool close(std::string& error);

private:
    std::unique_ptr<char[]> _io_buffer;
    std::FILE* _f = nullptr;
    Codec _codec = Codec::H264;
    bool _h265 = false, _av1 = false, _spherical = false;
    std::vector<uint8_t> _seq_obu;
    int _width = 0, _height = 0;
    uint32_t _timescale = 90000, _delta = 3000;
    uint64_t _mdat_start = 0, _written = 0;
    std::vector<uint32_t> _sizes;
    std::vector<uint64_t> _offsets;
    std::vector<uint32_t> _syncs;
    std::vector<std::vector<uint8_t>> _vps, _sps, _pps;
    bool _failed = false;
};

// Split an Annex-B stream into NAL units (without their start codes).
std::vector<std::pair<const uint8_t*, size_t>> split_annexb(const uint8_t* p, size_t n);

// Split AV1 OBUs (each with its size field) into (type, whole OBU) pairs.
std::vector<std::pair<int, std::pair<const uint8_t*, size_t>>> split_obus(const uint8_t* p,
                                                                          size_t n);

}  // namespace video
