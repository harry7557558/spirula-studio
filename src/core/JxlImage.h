#pragma once

// JPEG XL dataset images, decoded by libjxl (SS_HAVE_JXL, cmake/SsJxl.cmake).
// Without it is_jxl() still answers, so a .jxl is refused by name rather than
// handed to stb_image; the decode returns an error that says how to build it.

#include <cstdint>
#include <string>
#include <vector>

namespace jxl {

struct Info {
    int width = 0;      // upright: libjxl applies the codestream's orientation
    int height = 0;
    int channels = 0;   // colour plus alpha, as stored
    int bits = 8;       // per sample, as stored
};

// By the first bytes: a bare codestream (FF 0A) or the container's signature box.
bool is_jxl(const std::string& path);

// True in a build with libjxl.
bool available();

// `jpeg` stored losslessly as a JPEG XL at `out` (~20% smaller), kept only once
// it gives back the same JPEG byte for byte, encoded on `threads`. "" on success.
std::string transcode_jpeg(const std::string& jpeg, const std::string& out, int threads = 1);

// Interleaved 8-bit sRGB (`channels` 1 to 4) at `quality` on JPEG's 0..100
// scale and libjxl's `effort` (1..10, default 7), alpha lossless, with `exif`
// (an APP1 payload, may be empty) as its Exif box. "" on success.
std::string encode_srgb8(const uint8_t* px, int width, int height, int channels, int quality,
                         int effort, const std::vector<uint8_t>& exif, int threads,
                         std::vector<uint8_t>& out);

// Header only. "" on success, else one sentence naming the problem.
std::string probe(const std::string& path, Info& info);

// Interleaved 8-bit sRGB, `channels` of 1, 3 or 4; `threads` 0 = all cores.
std::string decode_srgb8(const std::string& path, int channels, int threads, Info& info,
                         std::vector<uint8_t>& out);

// The same as 16-bit samples, for a dataset read at 16 bits.
std::string decode_srgb16(const std::string& path, int channels, int threads, Info& info,
                          std::vector<uint16_t>& out);

// The Exif box as a JPEG APP1 payload ("Exif\0\0" + TIFF), Brotli-compressed
// ('brob') or not; empty when there is none or without libjxl.
std::vector<uint8_t> read_exif(const std::string& path);

}  // namespace jxl
