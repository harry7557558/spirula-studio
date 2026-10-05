#pragma once

// What an embedded ICC profile says about the pixels, for the readers that
// find one (core/TiffImage.h). Only matrix/TRC profiles -- RGB colorants plus
// a curve per channel, or a grey curve -- are read; a LUT-based profile is
// reported as unreadable. docs/notes/tiff.md.

#include <cstddef>
#include <cstdint>
#include <string>

namespace icc {

struct ColorSpace {
    std::string gamut;          // core/ColorSpace.h name; "" = Rec.709
    bool gamut_known = true;    // false: colorants matching no gamut there
    bool is_linear = false;     // every curve is the identity
    bool grey = false;          // a grey profile, which has no primaries
};

// False when `p` is not a matrix/TRC (or grey TRC) profile with an XYZ PCS.
bool read(const uint8_t* p, size_t n, ColorSpace& out);

}  // namespace icc
