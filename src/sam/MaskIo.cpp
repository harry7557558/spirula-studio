// Writers for what the segmentation subsystem produces: a binary mask PNG and
// a human-readable overlay. Plain image reading and writing is nn's
// (nn/io/Image.h); these two know what a Mask and a Detection are.

#include "sam/Sam.h"

#include "nn/core/Log.h"

namespace sam {

bool save_mask_png(const Mask& mask, const std::string& path) {
    if (mask.data.empty() || mask.width <= 0 || mask.height <= 0) {
        NN_LOG_ERROR("save_mask_png: empty mask\n");
        return false;
    }
    return nn::save_gray_png(mask.data.data(), mask.width, mask.height, path);
}

bool save_overlay_png(const Image& image, const Result& result, const std::string& path) {
    if (image.empty()) return false;
    Image overlay = image;
    std::vector<uint8_t>& rgb = overlay.data;

    // A fixed, reasonably distinguishable palette. Instance ids are arbitrary
    // integers, so the index into the palette is the detection order.
    static const uint8_t kPalette[][3] = {
        {230, 60, 60},  {60, 160, 230}, {90, 200, 90},  {235, 175, 50},
        {170, 100, 220}, {60, 210, 200}, {240, 120, 180}, {150, 150, 150},
    };
    const int n_colors = (int)(sizeof(kPalette) / sizeof(kPalette[0]));

    for (size_t d = 0; d < result.detections.size(); ++d) {
        const Detection& det = result.detections[d];
        if (det.mask.width != image.width || det.mask.height != image.height) continue;
        const uint8_t* col = kPalette[d % (size_t)n_colors];
        for (size_t i = 0; i < det.mask.data.size(); ++i) {
            if (det.mask.data[i] <= 127) continue;
            for (int ch = 0; ch < 3; ++ch) {
                const size_t o = i * 3 + (size_t)ch;
                rgb[o] = (uint8_t)((rgb[o] * 55 + col[ch] * 45) / 100);
            }
        }
        // Box outline, 2 px.
        auto plot = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
            const size_t o = ((size_t)y * image.width + x) * 3;
            rgb[o + 0] = col[0];
            rgb[o + 1] = col[1];
            rgb[o + 2] = col[2];
        };
        const int x0 = (int)det.box.x0, y0 = (int)det.box.y0;
        const int x1 = (int)det.box.x1, y1 = (int)det.box.y1;
        if (x1 > x0 && y1 > y0) {
            for (int t = 0; t < 2; ++t) {
                for (int x = x0; x <= x1; ++x) { plot(x, y0 + t); plot(x, y1 - t); }
                for (int y = y0; y <= y1; ++y) { plot(x0 + t, y); plot(x1 - t, y); }
            }
        }
    }
    return nn::save_image(overlay, path, -1);
}

}  // namespace sam
