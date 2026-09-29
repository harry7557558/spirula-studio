#pragma once
// The host-side image the inference layer takes in and hands back, plus the
// stb-backed readers and writers for it.
//
// It lives here rather than in a model's public header because everything
// above nn/ speaks it: sam/ encodes it, video/ decodes into it, and the GUI
// blits it. Pixels are 8-bit and interleaved; device tensors are nn::Tensor.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nn {

struct Image {
    int width = 0;
    int height = 0;
    int channels = 3;              // always 3 (RGB) once loaded
    std::vector<uint8_t> data;     // row-major, `channels` interleaved
    bool empty() const { return data.empty(); }
};

// Decodes to RGB; an unreadable file logs and returns empty(). `gamut` /
// `is_linear` describe the file (core/ColorSpace.h names, an unset half read
// from an EXR's header); pixels convert to sRGB, what the models expect.
Image load_image(const std::string& path, const std::string& gamut = "",
                 std::optional<bool> is_linear = std::nullopt);

// Writes RGB. `quality` in 0..100 selects JPEG, anything else lossless PNG --
// the same convention as reference/scripts/extract_frames.py.
bool save_image(const Image& image, const std::string& path, int quality);

// PIL's Image.resize(..., BILINEAR): the triangle filter widens with the
// downscale factor, so shrinking averages instead of aliasing. torchvision's
// Resize on a PIL image, i.e. what most reference pipelines feed a network.
Image resize_image(const Image& src, int width, int height);

// Writes a single-channel 8-bit PNG (masks, sharpness maps).
bool save_gray_png(const uint8_t* data, int width, int height,
                   const std::string& path);

}  // namespace nn
