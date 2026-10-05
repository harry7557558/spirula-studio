// The decode path declared in sfm/core/Image.h.
//
// stb_image is instantiated once for the whole repository, in
// src/external/stb_image_impl.cpp, which cmake/SsSfm.cmake adds to this
// library. Do NOT define STB_IMAGE_IMPLEMENTATION here: spirula-gui links both
// this library and the engine, which carries that TU too.
#include "sfm/core/Image.h"

#include "core/ColorSpace.h"
#include "core/ImageFile.h"
#include "core/ImageOrient.h"

#include <algorithm>

#include "external/stb_image.h"

namespace sfm {

// Box-average an interleaved-RGB uint8 image to (dw,dh). A cheap area filter is
// enough: the color is only ever point-sampled at keypoints for the point cloud,
// not fed to SIFT. Downscaling avoids holding a full-res color buffer alongside
// the gray one, which matters for the batch decoder's memory budget.
static std::vector<uint8_t> downscaleRgb(const unsigned char* src, int w, int h, int dw, int dh) {
    std::vector<uint8_t> out((size_t)dw * dh * 3);
    std::vector<int> cx0(dw), cx1(dw);
    for (int x = 0; x < dw; x++) {
        cx0[x] = (int)((int64_t)x * w / dw);
        cx1[x] = std::max(cx0[x] + 1, (int)((int64_t)(x + 1) * w / dw));
    }
    for (int y = 0; y < dh; y++) {
        int sy0 = (int)((int64_t)y * h / dh), sy1 = std::max(sy0 + 1, (int)((int64_t)(y + 1) * h / dh));
        for (int x = 0; x < dw; x++) {
            const int sx0 = cx0[x], sx1 = cx1[x];
            uint32_t acc[3] = {0, 0, 0}, n = 0;
            for (int sy = sy0; sy < sy1; sy++)
                for (int sx = sx0; sx < sx1; sx++) {
                    const unsigned char* p = src + 3 * ((size_t)sy * w + sx);
                    acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; n++;
                }
            uint8_t* o = &out[3 * ((size_t)y * dw + x)];
            o[0] = (uint8_t)(acc[0] / n); o[1] = (uint8_t)(acc[1] / n); o[2] = (uint8_t)(acc[2] / n);
        }
    }
    return out;
}

// Rec.601 luma, matching COLMAP's FreeImage grayscale conversion.
static constexpr float kLumaR = 0.299f / 255.0f;
static constexpr float kLumaG = 0.587f / 255.0f;
static constexpr float kLumaB = 0.114f / 255.0f;

static inline float lumaAt(const unsigned char* rgb, int w, int x, int y) {
    const unsigned char* p = rgb + 3 * ((size_t)y * w + x);
    return kLumaR * p[0] + kLumaG * p[1] + kLumaB * p[2];
}

// Bit-identical to resizeGray() of the full-size luma image, which is never
// held (it capped the decode pool, sfm/core/ImageLoader.h). Luma once per source
// row, column taps tabulated (floor is a libm call pre-SSE4.1): 84 -> 30 ms, 20 MP.
static void resizeGrayFromRgb(const unsigned char* rgb, int w, int h, int dw, int dh,
                              std::vector<float>& out) {
    out.resize((size_t)dw * dh);
    const float sx = w / (float)dw;
    const float sy = h / (float)dh;
    std::vector<int> cx0(dw), cx1(dw);
    std::vector<float> cwx(dw);
    for (int x = 0; x < dw; x++) {
        float fx = (x + 0.5f) * sx - 0.5f;
        int x0 = (int)std::floor(fx);
        cwx[x] = fx - x0;
        cx0[x] = std::max(0, std::min(w - 1, x0));
        cx1[x] = std::max(0, std::min(w - 1, x0 + 1));
    }
    std::vector<float> lumaRows[2] = {std::vector<float>(w), std::vector<float>(w)};
    int rowOf[2] = {-1, -1};
    auto lumaRow = [&](int sy) -> const float* {
        for (int i = 0; i < 2; i++)
            if (rowOf[i] == sy) return lumaRows[i].data();
        // Evict the slot the other tap of this row is not using.
        const int i = rowOf[0] < rowOf[1] ? 0 : 1;
        for (int x = 0; x < w; x++) lumaRows[i][x] = lumaAt(rgb, w, x, sy);
        rowOf[i] = sy;
        return lumaRows[i].data();
    };
    for (int y = 0; y < dh; y++) {
        float fy = (y + 0.5f) * sy - 0.5f;
        int y0 = (int)std::floor(fy);
        float wy = fy - y0;
        int y0c = std::max(0, std::min(h - 1, y0));
        int y1c = std::max(0, std::min(h - 1, y0 + 1));
        const float* r0 = lumaRow(y0c);
        const float* r1 = lumaRow(y1c);
        for (int x = 0; x < dw; x++) {
            const float wx = cwx[x];
            float a = r0[cx0[x]], b = r0[cx1[x]];
            float c = r1[cx0[x]], d = r1[cx1[x]];
            float top = a + (b - a) * wx;
            float bot = c + (d - c) * wx;
            out[(size_t)y * dw + x] = top + (bot - top) * wy;
        }
    }
}

namespace {

// Turn a decoded image, its colour and its mask by the EXIF Orientation. Done
// on the DOWNSCALED buffers: a quarter turn commutes with the resample, and
// turning the full-resolution RGB would double what a concurrent decode holds.
void applyExifOrientation(GrayImage& img) {
    const ExifTransform xf = exifTransform(img.exif.orientation);
    if (xf.turns_cw != 0) {
        std::vector<float> gray((size_t)img.width * img.height);
        spirula::orient_pixels(img.data.data(), img.width, img.height, 1,
                               xf.turns_cw, false, gray.data());
        img.data.swap(gray);
        for (std::vector<uint8_t>* px : {&img.rgb, &img.color}) {
            if (px->empty()) continue;
            std::vector<uint8_t> turned(px->size());
            spirula::orient_pixels(px->data(), img.width, img.height, 3,
                                   xf.turns_cw, false, turned.data());
            px->swap(turned);
        }
        if (!img.mask.empty()) {
            std::vector<uint8_t> bits(img.mask.bits.size());
            spirula::orient_pixels(img.mask.bits.data(), img.mask.width,
                                   img.mask.height, 1, xf.turns_cw, false,
                                   bits.data());
            img.mask.bits.swap(bits);
            std::swap(img.mask.width, img.mask.height);
        }
        std::swap(img.width, img.height);
        std::swap(img.orig_width, img.orig_height);
        std::swap(img.exif.pixel_width, img.exif.pixel_height);
    }
    img.exif_mirror_dropped = xf.mirror;
    img.exif.orientation = 1;
}

}  // namespace

GrayImage loadGrayImage(const std::string& path, int max_image_size, bool want_color,
                        const std::string& mask_path,
                        const std::string& gamut, std::optional<bool> is_linear,
                        bool flip_mask, bool apply_exif_orientation,
                        const std::string& feature_mask_path,
                        const colorspace::Exposure& exposure) {
    int w = 0, h = 0, chan = 0;
    // Force 3 channels; we do our own luma so behavior is decoder-independent.
    // An EXR or TIFF decodes on this thread: the pool above already owns every core.
    std::vector<uint8_t> own_rgb, plain;
    unsigned char* rgb = nullptr;
    GrayImage img;
    if (imagefile::handles(path)) {
        imagefile::Info info;
        imagefile::Options opt;
        opt.threads = 1;
        opt.exposure = exposure;
        const std::string err = imagefile::decode_srgb8(path, opt, info, own_rgb, gamut,
                                                        is_linear, want_color ? &plain : nullptr);
        if (!err.empty())
            throw std::runtime_error("cannot decode image " + path + ": " + err);
        w = info.width;
        h = info.height;
        rgb = own_rgb.data();
        img.gain = info.gain;
        img.peak = info.peak;
    } else {
        rgb = stbi_load(path.c_str(), &w, &h, &chan, 3);
        if (!rgb)
            throw std::runtime_error("cannot decode image " + path + ": " + stbi_failure_reason());
        const size_t n = (size_t)w * h * 3;
        if (is_linear.value_or(false)) img.peak = *std::max_element(rgb, rgb + n) / 255.0f;
        colorspace::to_srgb_inplace(rgb, (size_t)w * h, gamut,
                                    is_linear.value_or(false));
        img.gain = colorspace::exposure_gain_srgb8(exposure, rgb, (size_t)w, (size_t)h);
        if (img.gain != 1.0f) {
            if (want_color) plain.assign(rgb, rgb + n);
            colorspace::expose_srgb8_inplace(rgb, n, img.gain);
        }
    }

    img.orig_width = w;
    img.orig_height = h;

    // Downscale so the long edge is at most max_image_size (COLMAP default 3200).
    int dw = w, dh = h;
    int longEdge = std::max(w, h);
    if (max_image_size > 0 && longEdge > max_image_size) {
        double scale = (double)max_image_size / longEdge;
        dw = std::max(1, (int)std::lround(w * scale));
        dh = std::max(1, (int)std::lround(h * scale));
    }
    img.width = dw;
    img.height = dh;
    // Keep the color companion at the *gray* (post-downscale) resolution, so a
    // keypoint's coordinates index it directly.
    if (want_color) {
        img.rgb = (dw == w && dh == h) ? std::vector<uint8_t>(rgb, rgb + (size_t)w * h * 3)
                                       : downscaleRgb(rgb, w, h, dw, dh);
        if (!plain.empty())
            img.color = (dw == w && dh == h) ? std::move(plain)
                                             : downscaleRgb(plain.data(), w, h, dw, dh);
    }
    if (dw == w && dh == h) {
        img.data.resize((size_t)w * h);
        for (size_t i = 0; i < img.data.size(); i++)
            img.data[i] = kLumaR * rgb[3 * i] + kLumaG * rgb[3 * i + 1] + kLumaB * rgb[3 * i + 2];
    } else {
        resizeGrayFromRgb(rgb, w, h, dw, dh, img.data);
    }
    if (own_rgb.empty()) stbi_image_free(rgb);
    // Kept at the mask file's own resolution: applyMask() samples it in uv, so
    // resampling it to match `img` would only lose detail (D39).
    if (!mask_path.empty()) {
        img.mask = loadMask(mask_path);
        if (flip_mask) img.mask.invert();
    }
    // Not over a first mask that failed to decode: the caller reports that by
    // finding img.mask empty.
    if (!feature_mask_path.empty() && (mask_path.empty() || !img.mask.empty()))
        intersectMask(img.mask, loadMask(feature_mask_path));
    img.exif = readExif(path);  // header bytes only; see sfm/core/Exif.h
    if (apply_exif_orientation) applyExifOrientation(img);
    return img;
}

Mask loadMask(const std::string& path) {
    int w = 0, h = 0, chan = 0;
    // One channel: a mask is categorical, and every convention in the wild
    // (1-bit PNG, 8-bit gray, RGB white-on-black, RGBA alpha) reduces to the
    // same thing under stb's gray conversion -- except an alpha-only mask,
    // which stb would flatten to white. Masks that carry their signal in alpha
    // are handled below.
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &chan, 1);
    if (!px || w <= 0 || h <= 0) {
        if (px) stbi_image_free(px);
        return Mask();
    }
    Mask m;
    m.width = w;
    m.height = h;
    m.bits.resize((size_t)w * h);
    for (size_t i = 0; i < m.bits.size(); i++) m.bits[i] = px[i] != 0 ? 1 : 0;
    stbi_image_free(px);

    // An RGBA mask that is uniformly white in RGB carries its shape in alpha
    // (what "cut out the subject" exporters produce). stb's gray conversion
    // drops alpha, so that mask decodes as all-ones; re-read the alpha channel
    // and use it instead. Only done when the gray read was fully saturated, so
    // an ordinary opaque RGBA mask is untouched.
    if (chan == 4) {
        bool all_keep = true;
        for (uint8_t b : m.bits)
            if (!b) { all_keep = false; break; }
        if (all_keep) {
            int w2 = 0, h2 = 0, c2 = 0;
            unsigned char* rgba = stbi_load(path.c_str(), &w2, &h2, &c2, 4);
            if (rgba) {
                if (w2 == w && h2 == h)
                    for (size_t i = 0; i < m.bits.size(); i++)
                        m.bits[i] = rgba[4 * i + 3] != 0 ? 1 : 0;
                stbi_image_free(rgba);
            }
        }
    }
    return m;
}

bool imageSize(const std::string& path, int& width, int& height) {
    if (imagefile::handles(path)) {
        imagefile::Info info;
        if (!imagefile::probe(path, info).empty()) return false;
        width = info.width;
        height = info.height;
        return true;
    }
    int comp = 0;
    return stbi_info(path.c_str(), &width, &height, &comp) != 0;
}

}  // namespace sfm
