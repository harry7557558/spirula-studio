#pragma once

// IEEE binary16 -> float, for the image readers (core/ExrImage.cpp,
// core/TiffImage.cpp).

#include <cstdint>
#include <cstring>
#include <vector>

namespace spirula {

// 256 KB, built once. Real scene-linear captures are full of subnormals, and
// the branchy bit-twiddle conversion measures slower on them than the table.
inline const float* half_to_float_table() {
    static const std::vector<float> table = [] {
        std::vector<float> t(65536);
        for (uint32_t h = 0; h < 65536; h++) {
            const uint32_t sign = (h & 0x8000u) << 16;
            uint32_t e = (h >> 10) & 0x1fu, m = h & 0x3ffu, bits;
            if (e == 0 && m == 0) {
                bits = sign;
            } else if (e == 0) {
                e = 1;
                while (!(m & 0x400u)) { m <<= 1; e--; }
                bits = sign | ((e + 112u) << 23) | ((m & 0x3ffu) << 13);
            } else if (e == 31) {
                bits = sign | 0x7f800000u | (m << 13);
            } else {
                bits = sign | ((e + 112u) << 23) | (m << 13);
            }
            std::memcpy(&t[h], &bits, 4);
        }
        return t;
    }();
    return table.data();
}

}  // namespace spirula
