#pragma once

// PNG encoding at 8 or 16 bits, 1-4 channels, which stb_image_write cannot do
// past 8. Header-only on miniz so every target that already links miniz can use
// it without a new library edge.

#include "external/miniz.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace png {

namespace detail {

inline void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)x);
}

// length, type, payload, CRC over (type + payload).
inline void chunk(std::vector<uint8_t>& out, const char tag[4], const uint8_t* data,
                  size_t len) {
    be32(out, (uint32_t)len);
    const size_t crc_at = out.size();
    out.insert(out.end(), tag, tag + 4);
    if (len) out.insert(out.end(), data, data + len);
    be32(out, (uint32_t)mz_crc32(MZ_CRC32_INIT, out.data() + crc_at, len + 4));
}

inline uint8_t paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return (uint8_t)(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

}  // namespace detail

// `data` is w*h*channels interleaved samples, uint8 or host-order uint16 as
// `bits` says. Empty on failure.
inline std::vector<uint8_t> encode(const void* data, int width, int height, int channels,
                                   int bits) {
    if (!data || width <= 0 || height <= 0 || channels < 1 || channels > 4 ||
        (bits != 8 && bits != 16))
        return {};
    const size_t bpp = (size_t)channels * (bits / 8);
    const size_t stride = (size_t)width * bpp;
    std::vector<uint8_t> prev(stride, 0), cur(stride);
    // Paeth on every row: a photo compresses ~30% smaller than unfiltered.
    std::vector<uint8_t> raw((size_t)height * (1 + stride));
    for (int y = 0; y < height; ++y) {
        if (bits == 8) {
            const uint8_t* src = (const uint8_t*)data + (size_t)y * stride;
            cur.assign(src, src + stride);
        } else {
            const uint16_t* src = (const uint16_t*)data + (size_t)y * width * channels;
            for (size_t i = 0; i < (size_t)width * channels; ++i) {
                cur[i * 2 + 0] = (uint8_t)(src[i] >> 8);
                cur[i * 2 + 1] = (uint8_t)(src[i] & 0xFF);
            }
        }
        uint8_t* row = raw.data() + (size_t)y * (1 + stride);
        *row++ = 4;
        for (size_t i = 0; i < stride; ++i) {
            const int a = i >= bpp ? cur[i - bpp] : 0;
            const int c = i >= bpp ? prev[i - bpp] : 0;
            row[i] = (uint8_t)(cur[i] - detail::paeth(a, prev[i], c));
        }
        prev.swap(cur);
    }

    mz_ulong bound = mz_compressBound((mz_ulong)raw.size());
    std::vector<uint8_t> z((size_t)bound);
    if (mz_compress2(z.data(), &bound, raw.data(), (mz_ulong)raw.size(),
                     MZ_DEFAULT_LEVEL) != MZ_OK)
        return {};
    z.resize((size_t)bound);

    static const uint8_t color_type[5] = {0, 0, 4, 2, 6};   // by channel count
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    detail::be32(ihdr, (uint32_t)width);
    detail::be32(ihdr, (uint32_t)height);
    ihdr.insert(ihdr.end(), {(uint8_t)bits, color_type[channels], 0, 0, 0});
    detail::chunk(out, "IHDR", ihdr.data(), ihdr.size());
    detail::chunk(out, "IDAT", z.data(), z.size());
    detail::chunk(out, "IEND", nullptr, 0);
    return out;
}

}  // namespace png
