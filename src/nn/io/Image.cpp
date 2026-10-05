// Host image I/O -- see nn/io/Image.h.
//
// stb_image / stb_image_write are the repository's vendored copies under
// src/external (public domain, header-only), instantiated once for the whole
// tree by external/stb_image{,_write}_impl.cpp; this file takes only their
// declarations. They pull in nothing: no libpng, no libjpeg, no ffmpeg.

#include "core/ColorSpace.h"
#include "core/ImageFile.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"

#include "external/stb_image.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace nn {

Image load_image(const std::string& path, const std::string& gamut,
                 std::optional<bool> is_linear, const colorspace::Exposure& exposure) {
    Image img;
    int w = 0, h = 0, c = 0;
    if (imagefile::handles(path)) {
        imagefile::Info info;
        imagefile::Options opt;
        opt.exposure = exposure;
        const std::string err =
            imagefile::decode_srgb8(path, opt, info, img.data, gamut, is_linear);
        if (!err.empty()) {
            NN_LOG_ERROR("load_image: cannot read '%s': %s\n", path.c_str(), err.c_str());
            return Image();
        }
        img.width = info.width;
        img.height = info.height;
        img.channels = 3;
        return img;
    }
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &c, 3);
    if (!data) {
        NN_LOG_ERROR("load_image: cannot read '%s': %s\n", path.c_str(),
                       stbi_failure_reason() ? stbi_failure_reason() : "unknown format");
        return img;
    }
    img.width = w;
    img.height = h;
    img.channels = 3;
    img.data.assign(data, data + (size_t)w * h * 3);
    stbi_image_free(data);
    colorspace::to_srgb_inplace(img.data.data(), (size_t)w * h, gamut,
                                is_linear.value_or(false));
    colorspace::expose_srgb8_inplace(
        img.data.data(), img.data.size(),
        colorspace::exposure_gain_srgb8(exposure, img.data.data(), (size_t)w, (size_t)h));
    return img;
}

bool save_image(const Image& image, const std::string& path, int quality) {
    if (image.empty() || image.width <= 0 || image.height <= 0) {
        NN_LOG_ERROR("save_image: empty image\n");
        return false;
    }
    if (quality >= 0 && quality <= 100)
        return stbi_write_jpg(path.c_str(), image.width, image.height, image.channels,
                              image.data.data(), quality) != 0;
    return stbi_write_png(path.c_str(), image.width, image.height, image.channels,
                          image.data.data(), image.width * image.channels) != 0;
}

bool save_gray_png(const uint8_t* data, int width, int height,
                   const std::string& path) {
    if (!data || width <= 0 || height <= 0) {
        NN_LOG_ERROR("save_gray_png: empty image\n");
        return false;
    }
    return stbi_write_png(path.c_str(), width, height, 1, data, width) != 0;
}

}  // namespace nn
