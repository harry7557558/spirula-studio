#include "data/DownscaleCache.h"

#include "core/PngWrite.h"
#include "data/DatasetParser.h"

#include "external/stb_image.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <random>
#include <thread>

namespace fs = std::filesystem;

namespace dscache {
namespace {

// No chroma subsampling above 90 in stb's encoder.
constexpr int kJpegQuality = 95;

// A camera's maker trailer can follow the end marker, so look a little way back.
constexpr long kTailBytes = 64 * 1024;

bool tail_has(const std::string& path, const uint8_t* marker, size_t n) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> tail;
    if (std::fseek(f, 0, SEEK_END) == 0) {
        const long size = std::ftell(f);
        const long from = std::max(0L, size - kTailBytes);
        if (size > 0 && std::fseek(f, from, SEEK_SET) == 0) {
            tail.resize((size_t)(size - from));
            tail.resize(std::fread(tail.data(), 1, tail.size(), f));
        }
    }
    std::fclose(f);
    return std::search(tail.begin(), tail.end(), marker, marker + n) != tail.end();
}

}  // namespace

Format format_of(const std::string& path) {
    uint8_t head[8] = {};
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return Format::None;
    const size_t n = std::fread(head, 1, sizeof head, f);
    std::fclose(f);
    static const uint8_t png_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n == 8 && std::memcmp(head, png_sig, 8) == 0) return Format::Png;
    if (n >= 3 && head[0] == 0xFF && head[1] == 0xD8 && head[2] == 0xFF) return Format::Jpeg;
    return Format::None;
}

std::string path_for(const std::string& image, const std::string& image_dir, int d) {
    if (d < 2 || format_of(image) == Format::None) return "";
    std::error_code ec;
    const fs::path img = fs::absolute(image, ec).lexically_normal();
    fs::path dir;
    std::string rel;
    if (!image_dir.empty()) {
        dir = fs::absolute(image_dir, ec).lexically_normal();
        rel = dsparse::relative_under(img.string(), dir.string());
    }
    if (rel.empty()) {
        dir = img.parent_path();
        rel = img.filename().string();
    }
    if (dir.filename().empty()) dir = dir.parent_path();   // a trailing separator
    const std::string name = dir.filename().string();
    if (name.empty() || name == "." || name == "..") return "";
    return (dir.parent_path() / (name + "_" + std::to_string(d)) / rel).string();
}

int scaled_side(int full, int d, const std::string& mode) {
    const double v = (double)full / d;
    const double r = mode == "ceil" ? std::ceil(v) : mode == "round" ? std::round(v)
                                                                     : std::floor(v);
    return std::max(1, (int)r);
}

bool usable(const std::string& cached, const std::string& image, int d) {
    int cw, chh, cn, iw, ih, in;
    if (!stbi_info(cached.c_str(), &cw, &chh, &cn) ||
        !stbi_info(image.c_str(), &iw, &ih, &in))
        return false;
    auto fits = [d](int side, int full) {
        for (const char* mode : {"floor", "ceil", "round"})
            if (side == scaled_side(full, d, mode)) return true;
        return false;
    };
    if (!fits(cw, iw) || !fits(chh, ih)) return false;
    static const uint8_t eoi[2] = {0xFF, 0xD9};
    static const uint8_t iend[4] = {'I', 'E', 'N', 'D'};
    switch (format_of(cached)) {
        case Format::Jpeg: return tail_has(cached, eoi, 2);
        case Format::Png:  return tail_has(cached, iend, 4);
        default:           return false;
    }
}

std::vector<uint8_t> encode(Format fmt, const void* px, int w, int h, int channels,
                            int bits) {
    if (fmt == Format::Png) return png::encode(px, w, h, channels, bits);
    if (fmt != Format::Jpeg || bits != 8 || (channels != 1 && channels != 3)) return {};
    std::vector<uint8_t> out;
    auto append = [](void* ctx, void* data, int size) {
        auto* v = (std::vector<uint8_t>*)ctx;
        v->insert(v->end(), (const uint8_t*)data, (const uint8_t*)data + size);
    };
    if (!stbi_write_jpg_to_func(append, &out, w, h, channels, px, kJpegQuality)) return {};
    return out;
}

bool store(const std::string& cached, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    const fs::path dst(cached);
    if (fs::exists(dst, ec)) return true;
    fs::create_directories(dst.parent_path(), ec);

    static std::atomic<uint64_t> serial{std::random_device{}()};
    fs::path tmp = dst;
    tmp += ".tmp" + std::to_string(serial.fetch_add(1) ^
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::FILE* f = std::fopen(tmp.string().c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0) ok = false;
    if (ok) {
        // A hard link fails rather than replace a file that appeared meanwhile;
        // rename is the fallback for filesystems without links.
        fs::create_hard_link(tmp, dst, ec);
        if (ec && !fs::exists(dst, ec)) {
            ec.clear();
            fs::rename(tmp, dst, ec);
        }
    }
    fs::remove(tmp, ec);
    return fs::exists(dst, ec);
}

}  // namespace dscache
