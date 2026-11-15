#pragma once

// SHA-256 (FIPS 180-4), header-only.
//
// Here because the repository has no crypto dependency and should not grow one
// for this. It is used wherever bytes come off the network and go straight
// into a parser -- the ALIKED / LightGlue checkpoints (src/aliked/model/) and
// the CJK font faces (src/app/gui/Fonts.cpp) -- so it has to be reachable from
// both, and neither of those may include the other.
//
// Header-only for the same reason src/core/Env.h is: the standalone tool
// binaries link neither the engine nor each other.

#include "core/BufferedFile.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace spirula {

struct Sha256 {
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint8_t  buf[64] = {};
    size_t   buf_len = 0;
    uint64_t total = 0;

    static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
            0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
            0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
            0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
            0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
                   ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const uint8_t* p, size_t n) {
        total += n;
        while (n) {
            const size_t take = std::min(n, sizeof(buf) - buf_len);
            std::memcpy(buf + buf_len, p, take);
            buf_len += take;
            p += take;
            n -= take;
            if (buf_len == sizeof(buf)) { block(buf); buf_len = 0; }
        }
    }

    // Finalizes; the object must not be updated again afterwards.
    std::string hex() {
        const uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        pad = 0x00;
        while (buf_len != 56) update(&pad, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = (uint8_t)(bits >> (56 - 8 * i));
        // update() would count these into `total`, but the length is already
        // frozen in `bits`, so the extra count is harmless.
        update(len, 8);

        static const char* kHex = "0123456789abcdef";
        std::string out(64, '0');
        for (int i = 0; i < 8; ++i)
            for (int b = 0; b < 4; ++b) {
                const uint8_t v = (uint8_t)(h[i] >> (24 - 8 * b));
                out[(size_t)(i * 8 + b * 2)] = kHex[v >> 4];
                out[(size_t)(i * 8 + b * 2 + 1)] = kHex[v & 0xF];
            }
        return out;
    }
};

// Lowercase hex SHA-256 of a file's contents. Empty when it cannot be read.
inline std::string sha256_file(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin) return {};
    Sha256 sha;
    thread_local std::vector<char> chunk(kFileBufferBytes);
    while (fin) {
        fin.read(chunk.data(), (std::streamsize)chunk.size());
        const std::streamsize got = fin.gcount();
        if (got > 0)
            sha.update(reinterpret_cast<const uint8_t*>(chunk.data()), (size_t)got);
    }
    return sha.hex();
}

}  // namespace spirula
