#pragma once

// AVIF dataset images, decoded by libavif with dav1d (SS_HAVE_AVIF,
// cmake/SsJxl.cmake). Without it is_avif() still answers and the decode
// returns an error that says how to build it.

#include <cstdint>
#include <string>
#include <vector>

namespace avif {

struct Info {
    int width = 0;      // upright: irot / imir are applied
    int height = 0;
    int channels = 0;   // colour plus alpha
    int bits = 8;
};

// By the 'ftyp' box: an 'avif' or 'avis' brand.
bool is_avif(const std::string& path);

// True in a build with libavif.
bool available();

// True when that libavif has an AV1 encoder linked (libaom, rav1e or SVT-AV1).
bool can_encode();

// Interleaved 8-bit sRGB (`channels` 1 to 4) as a 10-bit 4:4:4 AVIF at
// libavif's `quality` (0..100) and `speed` (0..10), alpha lossless, with
// `exif` (an APP1 payload, may be empty). "" on success.
std::string encode_srgb8(const uint8_t* px, int width, int height, int channels, int quality,
                         int speed, const std::vector<uint8_t>& exif, int threads,
                         std::vector<uint8_t>& out);

std::string probe(const std::string& path, Info& info);

// Interleaved 8- or 16-bit RGB(A) in the file's own transfer (sRGB for every
// photo seen), `channels` 3 or 4; `threads` 0 = all cores.
std::string decode_srgb8(const std::string& path, int channels, int threads, Info& info,
                         std::vector<uint8_t>& out);
std::string decode_srgb16(const std::string& path, int channels, int threads, Info& info,
                          std::vector<uint16_t>& out);

// The Exif item as a JPEG APP1 payload ("Exif\0\0" + TIFF); empty when there
// is none or without libavif.
std::vector<uint8_t> read_exif(const std::string& path);

}  // namespace avif
