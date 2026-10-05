#pragma once

// TIFF reader: classic and BigTIFF, strips and tiles, chunky and planar,
// 8/16-bit unsigned and 16/32/64-bit float samples, NONE / LZW / Deflate /
// PackBits with either predictor. The first image in the file is the one
// read. What it cannot read it names in the returned string -- docs/notes/tiff.md
// lists the gaps. No libtiff dependency; inflate is the vendored miniz.

#include "core/ColorSpace.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tiff {

// What decode() hands back: integer samples at their stored width, every float
// width widened to float32.
enum class Sample { U8, U16, F32 };

struct Info {
    int width = 0;
    int height = 0;
    int channels = 0;               // 1 grey, 2 grey + alpha, 3 RGB, 4 RGBA
    Sample sample = Sample::U8;
    std::string compression;
    // From the embedded ICC profile, when it is one core/IccProfile.h reads.
    // Without one a TIFF is display-encoded Rec.709, like a PNG.
    bool icc = false;
    std::string gamut;              // "" = Rec.709
    bool is_linear = false;
    bool gamut_known = true;
    // Set by decode_srgb8.
    float peak = 0.0f;              // largest colour sample; 1.0 is white
    float gain = 1.0f;              // the exposure applied, in linear light
};

// True when the first four bytes are a TIFF or BigTIFF header. A file this
// returns false for is left to the other readers.
bool is_tiff(const std::string& path);

struct Options {
    int channels = 3;      // 1, 3 or 4 wanted, interleaved in that order
    int threads = 0;       // 0 = all cores; 1 = decode on the calling thread
    colorspace::Exposure exposure;   // decode_srgb8 only
};

// Header only. Returns "" on success, else one sentence naming the problem.
std::string probe(const std::string& path, Info& info);

// What `path`'s ICC profile declares, for a caller filling in what its user
// left unset. False for a TIFF without one this can read.
bool declared_color_space(const std::string& path, Info& info);

// Interleaved samples of `info.sample`'s type, `opt.channels` per pixel, as
// stored: no transfer, float values above 1 kept, alpha straight.
std::string decode(const std::string& path, const Options& opt, Info& info,
                   std::vector<uint8_t>& out);

// Interleaved 8-bit sRGB with `opt.exposure` applied; an unset half of the
// colour space is the ICC profile's, else Rec.709 / display-encoded. Given
// `unexposed` also gets the pixels without it, or stays empty if it was 1x.
std::string decode_srgb8(const std::string& path, const Options& opt, Info& info,
                         std::vector<uint8_t>& out, const std::string& gamut = "",
                         std::optional<bool> is_linear = std::nullopt,
                         std::vector<uint8_t>* unexposed = nullptr);

}  // namespace tiff
