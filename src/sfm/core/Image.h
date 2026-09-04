// Host-side image container and decode.
//
// Minimal by design: decode JPEG/PNG/etc. through the vendored stb_image (D5),
// convert to single-channel float in [0,1] using Rec.601 luma weights (what
// COLMAP's FreeImage grayscale path uses), and optionally downscale so the long
// edge is <= max_image_size (COLMAP's default 3200). Everything downstream of
// feature extraction works on this GrayImage.
//
// EXIF / focal-length priors are a separate concern (docs/notes/sfm-design.md D5); not
// handled here yet.
#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Exif.h"
#include "sfm/core/Mask.h"

namespace sfm {

// Single-channel image, row-major, values in [0,1].
//
// `rgb` is an *optional* companion buffer at the same (post-downscale) width and
// height, 3 interleaved uint8 per pixel. It is empty unless the image was
// decoded with color requested (loadGrayImage(..., want_color=true)); SIFT never
// touches it -- it exists so `spirula-sfm extract` can sample a color at each keypoint
// for the reconstruction's point cloud (src/sfm/README.md "Point colors").
struct GrayImage {
    int width = 0;
    int height = 0;
    // The source file's size, which differs from (width,height) when the loader
    // downscaled to max_image_size. Features are extracted at (width,height)
    // and scaled back to this before they are written (D46), so intrinsics
    // describe the images on disk.
    int orig_width = 0;
    int orig_height = 0;
    std::vector<float> data;  // width*height
    std::vector<uint8_t> rgb; // empty, or width*height*3 (interleaved RGB)
    // Optional keypoint mask, at *its own* resolution (sfm/core/Mask.h): it is
    // sampled in uv, so it neither has to match this image's decoded size nor
    // the source file's. Empty unless the loader was given a mask path.
    Mask mask;
    // What the file's EXIF said, if anything (sfm/core/Exif.h). Parsed here so
    // the batch decode pool absorbs the cost, and because this is the last
    // stage that touches the image file at all.
    ExifData exif;

    float at(int x, int y) const { return data[(size_t)y * width + x]; }
    size_t pixels() const { return (size_t)width * height; }
    bool hasColor() const { return rgb.size() == pixels() * 3; }
};

// Bilinearly sample the color at (x,y) in the same coordinate frame as the gray
// data. Out-of-range coordinates clamp to the border. No-op (leaves gray) if the
// image carries no color.
inline void sampleColor(const GrayImage& img, float x, float y, uint8_t out[3]) {
    if (!img.hasColor()) { out[0] = out[1] = out[2] = 128; return; }
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float wx = x - x0, wy = y - y0;
    int x0c = std::max(0, std::min(img.width - 1, x0));
    int x1c = std::max(0, std::min(img.width - 1, x0 + 1));
    int y0c = std::max(0, std::min(img.height - 1, y0));
    int y1c = std::max(0, std::min(img.height - 1, y0 + 1));
    const uint8_t* p = img.rgb.data();
    for (int c = 0; c < 3; c++) {
        float a = p[3 * ((size_t)y0c * img.width + x0c) + c];
        float b = p[3 * ((size_t)y0c * img.width + x1c) + c];
        float cc = p[3 * ((size_t)y1c * img.width + x0c) + c];
        float d = p[3 * ((size_t)y1c * img.width + x1c) + c];
        float top = a + (b - a) * wx;
        float bot = cc + (d - cc) * wx;
        out[c] = (uint8_t)std::lround(std::max(0.0f, std::min(255.0f, top + (bot - top) * wy)));
    }
}

// A mask that fails to decode comes back empty, never as an exception -- a bad
// mask must not lose the image. `gamut` / `is_linear` describe the file; pixels
// convert to sRGB on decode, which luma and the learned frontend assume.
GrayImage loadGrayImage(const std::string& path, int max_image_size = 3200,
                        bool want_color = false, const std::string& mask_path = "",
                        const std::string& gamut = "",
                        std::optional<bool> is_linear = std::nullopt,
                        bool flip_mask = false);

// Read just the pixel dimensions from an image header (no full decode).
// Returns false if the file is not a decodable image.
bool imageSize(const std::string& path, int& width, int& height);

// Bilinear resample to an exact target size. Used by the loader for
// max_image_size clamping and available to callers (e.g. synthetic tests).
inline GrayImage resizeGray(const GrayImage& src, int dw, int dh) {
    GrayImage out;
    out.width = dw;
    out.height = dh;
    out.data.resize((size_t)dw * dh);
    // Map destination pixel centers back into source, clamp at the border.
    const float sx = src.width / (float)dw;
    const float sy = src.height / (float)dh;
    for (int y = 0; y < dh; y++) {
        float fy = (y + 0.5f) * sy - 0.5f;
        int y0 = (int)std::floor(fy);
        float wy = fy - y0;
        int y0c = std::max(0, std::min(src.height - 1, y0));
        int y1c = std::max(0, std::min(src.height - 1, y0 + 1));
        for (int x = 0; x < dw; x++) {
            float fx = (x + 0.5f) * sx - 0.5f;
            int x0 = (int)std::floor(fx);
            float wx = fx - x0;
            int x0c = std::max(0, std::min(src.width - 1, x0));
            int x1c = std::max(0, std::min(src.width - 1, x0 + 1));
            float a = src.at(x0c, y0c), b = src.at(x1c, y0c);
            float c = src.at(x0c, y1c), d = src.at(x1c, y1c);
            float top = a + (b - a) * wx;
            float bot = c + (d - c) * wx;
            out.data[(size_t)y * dw + x] = top + (bot - top) * wy;
        }
    }
    return out;
}

}  // namespace sfm
