#include "core/WebpWriter.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#ifdef SS_TEST_WEBP_DECODER
#include <webp/decode.h>
#endif

namespace {
int failures = 0;

void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void roundtrip(int w, int h, int channels, int pattern, const std::filesystem::path& directory) {
    std::vector<uint8_t> pixels(size_t(w) * h * channels);
    std::mt19937 rng(1234);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < channels; ++c) {
                uint8_t v = 0;
                switch (pattern) {
                case 0: v = uint8_t(37 + c * 63); break;
                case 1: v = uint8_t(x + y + c * 31); break;
                case 2: v = uint8_t(rng()); break;
                case 3: v = uint8_t(((x / 4 + y / 7) & 1) * 255); break;
                case 4: v = c == 3 ? 0 : uint8_t(rng()); break;
                case 5: v = uint8_t((x * 255 / w + y * 255 / h + c * 31) / 3); break;
                case 6: v = (x % 4097) == 0 ? uint8_t(c * 57) : 128; break;
                }
                pixels[(size_t(y) * w + x) * channels + c] = v;
            }
    std::vector<uint8_t> encoded;
    auto start = std::chrono::steady_clock::now();
    bool ok = spirula::encode_webp_lossless(pixels.data(), pixels.size(), w, h, channels, encoded);
    check(ok && encoded.size() >= 26, "encode succeeds");
    if (!ok || encoded.size() < 26) return;
    check(std::memcmp(encoded.data(), "RIFF", 4) == 0 &&
          std::memcmp(encoded.data() + 8, "WEBPVP8L", 8) == 0, "lossless RIFF signature");
    check(u32(encoded.data() + 4) + 8 == encoded.size(), "RIFF size includes padding");
    check(20 + u32(encoded.data() + 16) + (u32(encoded.data() + 16) & 1) == encoded.size(),
          "VP8L size excludes padding");
    check(encoded[20] == 0x2f, "VP8L signature");
    uint32_t dimensions = u32(encoded.data() + 21);
    check(int(dimensions & 16383) + 1 == w && int((dimensions >> 14) & 16383) + 1 == h,
          "dimensions roundtrip");
    if (pattern == 0 && pixels.size() > 10000)
        check(encoded.size() < pixels.size() / 20, "flat image compresses");
    if (pattern == 5 && size_t(w) * h >= 65536)
        check(encoded.size() < pixels.size() / 5, "smooth image compresses");
#ifdef SS_TEST_WEBP_DECODER
    int dw = 0, dh = 0;
    uint8_t* decoded = WebPDecodeRGBA(encoded.data(), encoded.size(), &dw, &dh);
    check(decoded && dw == w && dh == h, "independent decoder accepts file");
    if (decoded && dw == w && dh == h) {
        bool same = true;
        for (size_t i = 0; i < size_t(w) * h; ++i)
            for (int c = 0; c < 4; ++c)
                same &= decoded[4 * i + c] == (c < channels ? pixels[channels * i + c] : 255);
        check(same, "exact pixels, including RGB under zero alpha");
    }
    WebPFree(decoded);
#endif
    if (!directory.empty()) {
        std::string name = std::to_string(w) + "x" + std::to_string(h) + "-" +
                           std::to_string(channels) + "-" + std::to_string(pattern);
        std::ofstream webp(directory / (name + ".webp"), std::ios::binary);
        webp.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        std::ofstream raw(directory / (name + ".raw"), std::ios::binary);
        raw.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
        check(webp.good() && raw.good(), "write decoder fixtures");
    }
    if (w == 2048) {
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::printf("%dx%d pattern %d: %zu -> %zu bytes, %.0f ms\n", w, h, pattern,
                    pixels.size(), encoded.size(), ms);
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path directory;
    if (argc == 2) { directory = argv[1]; std::filesystem::create_directories(directory); }
#ifndef SS_TEST_WEBP_DECODER
    std::puts("Independent pixel checks skipped: libwebp decoder unavailable; pass a directory to emit fixtures.");
#endif
    std::vector<uint8_t> out{1, 2, 3}, pixel(4);
    check(!spirula::encode_webp_lossless(nullptr, 4, 1, 1, 4, out) && out.empty(), "null input rejected");
    for (int size : {-1, 0, 16385})
        check(!spirula::encode_webp_lossless(pixel.data(), 4, size, 1, 4, out), "invalid width rejected");
    check(!spirula::encode_webp_lossless(pixel.data(), 4, 1, 0, 4, out), "invalid height rejected");
    check(!spirula::encode_webp_lossless(pixel.data(), 4, 1, 16385, 4, out), "oversize height rejected");
    check(!spirula::encode_webp_lossless(pixel.data(), 4, 1, 1, 2, out), "invalid channels rejected");
    check(!spirula::encode_webp_lossless(pixel.data(), 3, 1, 1, 4, out), "short input rejected");
    for (int channels : {3, 4}) {
        for (auto size : {std::pair<int, int>{1, 1}, {1, 129}, {129, 1}, {3, 5},
                          {31, 33}, {64, 64}, {257, 65}, {16384, 1}, {1, 16384}})
            for (int pattern = 0; pattern < 7; ++pattern)
                roundtrip(size.first, size.second, channels, pattern, directory);
        std::mt19937 rng(4321);
        for (int i = 0; i < 100; ++i)
            roundtrip(1 + rng() % 150, 1 + rng() % 150, channels, i % 7, {});
    }
    for (int pattern : {0, 2, 5}) roundtrip(2048, 1024, 3, pattern, directory);
    if (!failures) std::puts("webp_writer_test: OK");
    return failures ? 1 : 0;
}
