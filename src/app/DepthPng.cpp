#include "app/DepthPng.h"

#include "external/miniz.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace app {
namespace {

void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)x);
}

// length, type, payload, CRC over (type + payload).
void chunk(std::vector<uint8_t>& out, const char tag[4], const uint8_t* data,
           size_t len) {
    be32(out, (uint32_t)len);
    const size_t crc_at = out.size();
    out.insert(out.end(), tag, tag + 4);
    out.insert(out.end(), data, data + len);
    const uint32_t crc =
        (uint32_t)mz_crc32(MZ_CRC32_INIT, out.data() + crc_at, len + 4);
    be32(out, crc);
}

}  // namespace

bool save_depth_png16(const std::string& path, const uint16_t* data, int width,
                      int height) {
    if (!data || width <= 0 || height <= 0) return false;

    // One filter byte per row, then the row big-endian. Filter 0 (None) all
    // through: a depth map is smooth, so Paeth would compress better, but the
    // whole file is one of these per frame against seconds of network.
    std::vector<uint8_t> raw((size_t)height * (1 + (size_t)width * 2));
    for (int y = 0; y < height; ++y) {
        uint8_t* row = raw.data() + (size_t)y * (1 + (size_t)width * 2);
        *row++ = 0;
        const uint16_t* src = data + (size_t)y * width;
        for (int x = 0; x < width; ++x) {
            row[x * 2 + 0] = (uint8_t)(src[x] >> 8);
            row[x * 2 + 1] = (uint8_t)(src[x] & 0xFF);
        }
    }

    mz_ulong bound = mz_compressBound((mz_ulong)raw.size());
    std::vector<uint8_t> z((size_t)bound);
    if (mz_compress2(z.data(), &bound, raw.data(), (mz_ulong)raw.size(),
                     MZ_DEFAULT_LEVEL) != MZ_OK)
        return false;
    z.resize((size_t)bound);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)((uint32_t)width >> 24);
    ihdr[1] = (uint8_t)((uint32_t)width >> 16);
    ihdr[2] = (uint8_t)((uint32_t)width >> 8);
    ihdr[3] = (uint8_t)width;
    ihdr[4] = (uint8_t)((uint32_t)height >> 24);
    ihdr[5] = (uint8_t)((uint32_t)height >> 16);
    ihdr[6] = (uint8_t)((uint32_t)height >> 8);
    ihdr[7] = (uint8_t)height;
    ihdr[8] = 16;   // bit depth
    ihdr[9] = 0;    // grayscale
    ihdr[10] = 0;   // deflate
    ihdr[11] = 0;   // adaptive filtering
    ihdr[12] = 0;   // no interlace
    chunk(png, "IHDR", ihdr, sizeof ihdr);
    chunk(png, "IDAT", z.data(), z.size());
    chunk(png, "IEND", nullptr, 0);

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    if (std::fclose(f) != 0) ok = false;
    if (!ok) std::remove(path.c_str());
    return ok;
}

}  // namespace app
