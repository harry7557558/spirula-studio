#include "app/DepthPng.h"

#include "core/PngWrite.h"

#include <cstdio>
#include <vector>

namespace app {

bool save_depth_png16(const std::string& path, const uint16_t* data, int width,
                      int height) {
    const std::vector<uint8_t> bytes = png::encode(data, width, height, 1, 16);
    if (bytes.empty()) return false;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0) ok = false;
    if (!ok) std::remove(path.c_str());
    return ok;
}

}  // namespace app
