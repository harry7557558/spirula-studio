#include "core/AvifImage.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

#ifdef SS_HAVE_AVIF
#include <avif/avif.h>
#endif

namespace avif {

namespace {

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    out.resize((size_t)f.tellg());
    f.seekg(0);
    return (bool)f.read((char*)out.data(), (std::streamsize)out.size());
}

#ifdef SS_HAVE_AVIF
// libavif applies the transformative properties (irot, imir, clap) only when
// asked through avifImageApply...; the photos seen carry none, and a turned one
// would come out as stored -- the same pixels its EXIF-less JPEG would give.
template <typename T>
std::string decode(const std::string& path, int channels, int threads, int depth, Info& info,
                   std::vector<T>& out) {
    std::vector<uint8_t> in;
    if (!read_file(path, in)) return "cannot read " + path;
    avifDecoder* d = avifDecoderCreate();
    d->maxThreads = threads > 0 ? threads : (int)std::max(1u, std::thread::hardware_concurrency());
    avifResult r = avifDecoderSetIOMemory(d, in.data(), in.size());
    if (r == AVIF_RESULT_OK) r = avifDecoderParse(d);
    if (r == AVIF_RESULT_OK) r = avifDecoderNextImage(d);
    if (r == AVIF_RESULT_OK) {
        avifRGBImage rgb;
        avifRGBImageSetDefaults(&rgb, d->image);
        rgb.format = channels == 4 ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
        rgb.depth = (uint32_t)depth;
        info.width = (int)rgb.width;
        info.height = (int)rgb.height;
        info.channels = 3 + (d->alphaPresent ? 1 : 0);
        info.bits = (int)d->image->depth;
        out.resize((size_t)rgb.width * rgb.height * (size_t)channels);
        rgb.pixels = (uint8_t*)out.data();
        rgb.rowBytes = rgb.width * (uint32_t)channels * (uint32_t)sizeof(T);
        r = avifImageYUVToRGB(d->image, &rgb);
    }
    avifDecoderDestroy(d);
    return r == AVIF_RESULT_OK ? "" : std::string(avifResultToString(r)) + ": " + path;
}
#endif

}  // namespace

bool is_avif(const std::string& path) {
    uint8_t h[32] = {};
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    const size_t n = std::fread(h, 1, sizeof h, f);
    std::fclose(f);
    if (n < 16 || std::memcmp(h + 4, "ftyp", 4) != 0) return false;
    const size_t box = std::min<size_t>(n, (size_t)h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3]);
    for (size_t i = 8; i + 4 <= box; i += 4)
        if (i != 12 && (std::memcmp(h + i, "avif", 4) == 0 || std::memcmp(h + i, "avis", 4) == 0))
            return true;
    return false;
}

#ifdef SS_HAVE_AVIF
bool available() { return true; }

bool can_encode() { return avifCodecName(AVIF_CODEC_CHOICE_AUTO, AVIF_CODEC_FLAG_CAN_ENCODE); }

std::string encode_srgb8(const uint8_t* px, int width, int height, int channels, int quality,
                         int speed, const std::vector<uint8_t>& exif, int threads,
                         std::vector<uint8_t>& out) {
    const bool alpha = channels == 2 || channels == 4;
    // libavif takes RGB(A) only: grey is spread over the three.
    std::vector<uint8_t> wide;
    if (channels < 3) {
        const size_t n = (size_t)width * height;
        wide.resize(n * (alpha ? 4 : 3));
        for (size_t i = 0; i < n; i++) {
            uint8_t* o = wide.data() + i * (alpha ? 4 : 3);
            o[0] = o[1] = o[2] = px[i * channels];
            if (alpha) o[3] = px[i * channels + 1];
        }
        px = wide.data();
    }
    // 10-bit: at 8, the RGB-YUV rounding alone left a 4K frame worse than JPEG 95
    // to Butteraugli at any quality.
    avifImage* im = avifImageCreate((uint32_t)width, (uint32_t)height, 10, AVIF_PIXEL_FORMAT_YUV444);
    im->colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
    im->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    im->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    im->yuvRange = AVIF_RANGE_FULL;
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, im);
    rgb.format = alpha ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
    rgb.depth = 8;
    rgb.pixels = const_cast<uint8_t*>(px);
    rgb.rowBytes = (uint32_t)width * (alpha ? 4u : 3u);
    avifResult r = avifImageRGBToYUV(im, &rgb);
    static const uint8_t kSig[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (r == AVIF_RESULT_OK && exif.size() > 6 && std::memcmp(exif.data(), kSig, 6) == 0)
        r = avifImageSetMetadataExif(im, exif.data() + 6, exif.size() - 6);
    avifEncoder* e = avifEncoderCreate();
    e->maxThreads = std::max(1, threads);
    e->speed = speed;
    e->quality = quality;
    e->qualityAlpha = AVIF_QUALITY_LOSSLESS;   // alpha is a mask: no soft edges made up
    avifRWData o = AVIF_DATA_EMPTY;
    if (r == AVIF_RESULT_OK) r = avifEncoderWrite(e, im, &o);
    if (r == AVIF_RESULT_OK) out.assign(o.data, o.data + o.size);
    avifRWDataFree(&o);
    avifEncoderDestroy(e);
    avifImageDestroy(im);
    return r == AVIF_RESULT_OK ? "" : std::string("libavif could not encode: ") + avifResultToString(r);
}

std::string probe(const std::string& path, Info& info) {
    std::vector<uint8_t> in;
    if (!read_file(path, in)) return "cannot read " + path;
    avifDecoder* d = avifDecoderCreate();
    avifResult r = avifDecoderSetIOMemory(d, in.data(), in.size());
    if (r == AVIF_RESULT_OK) r = avifDecoderParse(d);
    if (r == AVIF_RESULT_OK) {
        info.width = (int)d->image->width;
        info.height = (int)d->image->height;
        info.channels = 3 + (d->alphaPresent ? 1 : 0);
        info.bits = (int)d->image->depth;
    }
    avifDecoderDestroy(d);
    return r == AVIF_RESULT_OK ? "" : std::string(avifResultToString(r)) + ": " + path;
}

std::string decode_srgb8(const std::string& path, int channels, int threads, Info& info,
                         std::vector<uint8_t>& out) {
    return decode(path, channels, threads, 8, info, out);
}

std::string decode_srgb16(const std::string& path, int channels, int threads, Info& info,
                          std::vector<uint16_t>& out) {
    return decode(path, channels, threads, 16, info, out);
}
std::vector<uint8_t> read_exif(const std::string& path) {
    std::vector<uint8_t> in, out;
    if (!read_file(path, in)) return out;
    avifDecoder* d = avifDecoderCreate();
    size_t offset = 0;
    if (avifDecoderSetIOMemory(d, in.data(), in.size()) == AVIF_RESULT_OK &&
        avifDecoderParse(d) == AVIF_RESULT_OK && d->image->exif.size > 8 &&
        avifGetExifTiffHeaderOffset(d->image->exif.data, d->image->exif.size, &offset) ==
            AVIF_RESULT_OK) {
        static const uint8_t kApp1[6] = {'E', 'x', 'i', 'f', 0, 0};
        out.assign(kApp1, kApp1 + 6);
        out.insert(out.end(), d->image->exif.data + offset,
                   d->image->exif.data + d->image->exif.size);
    }
    avifDecoderDestroy(d);
    return out;
}
#else
static const char kOff[] = "AVIF images need a build with -DSS_WITH_AVIF=ON: ";
bool available() { return false; }
bool can_encode() { return false; }
std::string encode_srgb8(const uint8_t*, int, int, int, int, int, const std::vector<uint8_t>&,
                         int, std::vector<uint8_t>&) {
    return kOff + std::string("an image");
}
std::string probe(const std::string& path, Info&) { return kOff + path; }
std::string decode_srgb8(const std::string& path, int, int, Info&, std::vector<uint8_t>&) {
    return kOff + path;
}
std::string decode_srgb16(const std::string& path, int, int, Info&, std::vector<uint16_t>&) {
    return kOff + path;
}
std::vector<uint8_t> read_exif(const std::string&) { return {}; }
#endif

}  // namespace avif
