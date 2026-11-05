#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace spirula {

// Packed RGB/RGBA, top row first. Alpha and RGB under transparent pixels are preserved.
bool encode_webp_lossless(const uint8_t* pixels, size_t size, int width, int height,
                          int channels, std::vector<uint8_t>& output);

}  // namespace spirula
