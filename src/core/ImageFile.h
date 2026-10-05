#pragma once

// The image formats decoded here rather than by stb_image -- OpenEXR
// (core/ExrImage.h) and TIFF (core/TiffImage.h) -- behind one probe and one
// sRGB decode, so a caller that hands everything else to stb branches once.

#include "core/ColorSpace.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace imagefile {

struct Info {
    int width = 0;
    int height = 0;
    int channels = 0;      // as stored, alpha included
    float peak = 0.0f;     // decode_srgb8: largest colour value; 1.0 is white
    float gain = 1.0f;     // decode_srgb8: the exposure applied, in linear light
};

struct Options {
    int channels = 3;      // 1, 3 or 4 wanted, interleaved in that order
    int threads = 0;       // 0 = all cores; 1 = decode on the calling thread
    colorspace::Exposure exposure;   // decode_srgb8 only
};

// What a file says about its own colour space: an EXR's header, a TIFF's ICC
// profile. `format` names which, for messages.
struct DeclaredColor {
    std::string format;
    std::string gamut;     // "" = Rec.709
    bool is_linear = false;
    bool gamut_known = true;
};

// True for an EXR or a TIFF, by the first bytes of the file.
bool handles(const std::string& path);

// Header only. Returns "" on success, else one sentence naming the problem.
std::string probe(const std::string& path, Info& info);

// False when the file declares nothing -- a TIFF without a profile this reads,
// or any format but these two.
bool declared_color_space(const std::string& path, DeclaredColor& out);

// Interleaved 8-bit sRGB with `opt.exposure` applied; an unset half of the
// colour space is the file's declaration, else Rec.709 / display-encoded.
// `unexposed` as in core/TiffImage.h.
std::string decode_srgb8(const std::string& path, const Options& opt, Info& info,
                         std::vector<uint8_t>& out, const std::string& gamut = "",
                         std::optional<bool> is_linear = std::nullopt,
                         std::vector<uint8_t>* unexposed = nullptr);

}  // namespace imagefile
