#pragma once

// OpenEXR reader: scanline and tiled parts, HALF / FLOAT / UINT channels, and
// NONE / RLE / ZIP / ZIPS / PIZ / PXR24 / B44 / B44A compression. What it
// cannot read it names in the returned string -- docs/notes/exr.md lists the
// gaps and what to re-save with. No OpenEXR/Imath dependency; inflate is the
// vendored miniz.

#include "core/ColorSpace.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace exr {

// What the header says. `gamut` is a core/ColorSpace.h name, resolved from the
// file's chromaticities; `gamut_known` is false when it declared primaries
// that match none of them, in which case `gamut` falls back to Rec.709.
struct Info {
    int width = 0;                  // display window, not data window
    int height = 0;
    int channels = 0;               // 1 (luminance), 3 (RGB) or 4 (RGBA)
    std::string compression;
    std::string gamut;              // "" = Rec.709
    bool is_linear = true;          // EXR pixels are scene-linear by spec
    bool chromaticities = false;    // the file declared its primaries
    bool gamut_known = true;
    bool tiled = false;
    int parts = 1;
    // Set by decode_srgb8.
    float peak = 0.0f;              // largest colour value
    float gain = 1.0f;              // the exposure applied, in linear light
};

// True when the first four bytes are the EXR magic. A file this returns false
// for is left to stb_image.
bool is_exr(const std::string& path);

struct Options {
    int channels = 3;      // 1, 3 or 4 wanted, interleaved in that order
    int threads = 0;       // 0 = all cores; 1 = decode on the calling thread
    colorspace::Exposure exposure;   // decode_srgb8 only
};

// Header only. Returns "" on success, else one sentence naming the problem.
std::string probe(const std::string& path, Info& info);

// What `path` declares about its own colour space, for a caller filling in
// what its user left unset. False when the file is not a readable EXR.
bool declared_color_space(const std::string& path, Info& info);

// Interleaved float, `opt.channels` per pixel, in the file's own colour space
// (scene-linear, so values above 1 are normal and are not clamped).
std::string decode(const std::string& path, const Options& opt, Info& info,
                   std::vector<float>& out);

// Interleaved 8-bit sRGB with `opt.exposure` applied. Each unset half of the
// colour space is the file's own; "Rec.709" and `false` override a header that
// lies. `unexposed` as in core/TiffImage.h.
std::string decode_srgb8(const std::string& path, const Options& opt, Info& info,
                         std::vector<uint8_t>& out, const std::string& gamut = "",
                         std::optional<bool> is_linear = std::nullopt,
                         std::vector<uint8_t>* unexposed = nullptr);

}  // namespace exr
