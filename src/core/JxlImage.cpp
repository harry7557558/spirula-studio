#include "core/JxlImage.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#ifdef SS_HAVE_JXL
#include <jxl/cms.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/encode.h>   // JxlColorEncodingSetToSRGB
#include <jxl/resizable_parallel_runner.h>
#endif

namespace jxl {

namespace {

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    out.resize((size_t)f.tellg());
    f.seekg(0);
    return (bool)f.read((char*)out.data(), (std::streamsize)out.size());
}

#ifdef SS_HAVE_JXL
// The JPEG a losslessly transcoded file was made from, byte for byte, rebuilt
// on `runner`'s threads. False for anything not made from a JPEG.
bool reconstruct_jpeg(const std::vector<uint8_t>& in, std::vector<uint8_t>& jpeg,
                      void* runner) {
    JxlDecoder* d = JxlDecoderCreate(nullptr);
    JxlDecoderSetParallelRunner(d, JxlResizableParallelRunner, runner);
    JxlDecoderSubscribeEvents(d, JXL_DEC_JPEG_RECONSTRUCTION | JXL_DEC_FULL_IMAGE);
    JxlDecoderSetInput(d, in.data(), in.size());
    JxlDecoderCloseInput(d);
    bool have = false, ok = false;
    for (;;) {
        const JxlDecoderStatus s = JxlDecoderProcessInput(d);
        if (s == JXL_DEC_JPEG_RECONSTRUCTION) {
            have = true;
            jpeg.resize(in.size() * 2);
            JxlDecoderSetJPEGBuffer(d, jpeg.data(), jpeg.size());
        } else if (s == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
            const size_t used = jpeg.size() - JxlDecoderReleaseJPEGBuffer(d);
            jpeg.resize(jpeg.size() * 2);
            JxlDecoderSetJPEGBuffer(d, jpeg.data() + used, jpeg.size() - used);
        } else if (s == JXL_DEC_FULL_IMAGE && have) {
            jpeg.resize(jpeg.size() - JxlDecoderReleaseJPEGBuffer(d));
            ok = true;
            break;
        } else {
            break;   // pixels wanted first: not a transcoded JPEG
        }
    }
    JxlDecoderDestroy(d);
    return ok;
}

// Everything `e` has left to give, into `bytes` (grown as needed).
bool take_output(JxlEncoder* e, std::vector<uint8_t>& bytes) {
    bytes.resize(std::max<size_t>(bytes.size(), 4096));
    size_t used = 0;
    for (;;) {
        uint8_t* next = bytes.data() + used;
        size_t avail = bytes.size() - used;
        const JxlEncoderStatus s = JxlEncoderProcessOutput(e, &next, &avail);
        used = (size_t)(next - bytes.data());
        if (s == JXL_ENC_SUCCESS) {
            bytes.resize(used);
            return true;
        }
        if (s != JXL_ENC_NEED_MORE_OUTPUT) return false;
        bytes.resize(bytes.size() * 2);
    }
}

// One decode of the first frame into `out` as `type`. Asking for sRGB with the
// CMS attached is what turns an XYB image, or one with an ICC profile, into
// the display-encoded sRGB every other reader of a dataset hands on.
std::string decode(const std::string& path, int channels, int threads, JxlDataType type,
                   size_t sample, Info& info, void* (*grow)(void*, size_t), void* ctx) {
    std::vector<uint8_t> in;
    if (!read_file(path, in)) return "cannot read " + path;
    JxlDecoder* d = JxlDecoderCreate(nullptr);
    void* runner = JxlResizableParallelRunnerCreate(nullptr);
    const size_t cap = threads > 0 ? (size_t)threads
                                   : std::max<size_t>(1, std::thread::hardware_concurrency());
    JxlDecoderSetParallelRunner(d, JxlResizableParallelRunner, runner);
    JxlDecoderSetCms(d, *JxlGetDefaultCms());
    JxlDecoderSubscribeEvents(d, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                                     JXL_DEC_FULL_IMAGE);
    JxlDecoderSetInput(d, in.data(), in.size());
    JxlDecoderCloseInput(d);
    const JxlPixelFormat fmt = {(uint32_t)channels, type, JXL_NATIVE_ENDIAN, 0};
    std::string err;
    for (;;) {
        const JxlDecoderStatus s = JxlDecoderProcessInput(d);
        if (s == JXL_DEC_BASIC_INFO) {
            JxlBasicInfo bi;
            JxlDecoderGetBasicInfo(d, &bi);
            info.width = (int)bi.xsize;
            info.height = (int)bi.ysize;
            info.channels = (int)bi.num_color_channels + (bi.alpha_bits ? 1 : 0);
            info.bits = (int)bi.bits_per_sample;
            // As libjxl's docs do it: no more threads than the image has work for.
            JxlResizableParallelRunnerSetThreads(
                runner, std::min<size_t>(cap, JxlResizableParallelRunnerSuggestThreads(
                                                  bi.xsize, bi.ysize)));
        } else if (s == JXL_DEC_COLOR_ENCODING) {
            JxlColorEncoding srgb;
            JxlColorEncodingSetToSRGB(&srgb, channels < 3);
            JxlDecoderSetPreferredColorProfile(d, &srgb);
        } else if (s == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t n = 0;
            JxlDecoderImageOutBufferSize(d, &fmt, &n);
            void* buf = grow(ctx, n / sample);
            JxlDecoderSetImageOutBuffer(d, &fmt, buf, n);
        } else if (s == JXL_DEC_FULL_IMAGE) {
            break;   // the first frame of an animation is the photo
        } else {
            err = s == JXL_DEC_NEED_MORE_INPUT ? "truncated JPEG XL: " + path
                                               : "not a decodable JPEG XL: " + path;
            break;
        }
    }
    JxlDecoderDestroy(d);
    JxlResizableParallelRunnerDestroy(runner);
    return err;
}
#endif

}  // namespace

bool is_jxl(const std::string& path) {
    uint8_t h[12] = {};
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    const size_t n = std::fread(h, 1, sizeof h, f);
    std::fclose(f);
    static const uint8_t kBox[12] = {0, 0, 0, 12, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A};
    return (n >= 2 && h[0] == 0xFF && h[1] == 0x0A) ||
           (n == 12 && std::memcmp(h, kBox, 12) == 0);
}

#ifdef SS_HAVE_JXL
bool available() { return true; }

std::string encode_srgb8(const uint8_t* px, int width, int height, int channels, int quality,
                         int effort, const std::vector<uint8_t>& exif, int threads,
                         std::vector<uint8_t>& out) {
    JxlEncoder* e = JxlEncoderCreate(nullptr);
    void* runner = JxlResizableParallelRunnerCreate(nullptr);
    JxlResizableParallelRunnerSetThreads(
        runner, std::min<size_t>(std::max(1, threads), JxlResizableParallelRunnerSuggestThreads(
                                                           (uint64_t)width, (uint64_t)height)));
    JxlEncoderSetParallelRunner(e, JxlResizableParallelRunner, runner);
    const bool alpha = channels == 2 || channels == 4;
    JxlBasicInfo bi;
    JxlEncoderInitBasicInfo(&bi);
    bi.xsize = (uint32_t)width;
    bi.ysize = (uint32_t)height;
    bi.bits_per_sample = 8;
    bi.num_color_channels = channels < 3 ? 1 : 3;
    bi.num_extra_channels = alpha ? 1 : 0;
    bi.alpha_bits = alpha ? 8 : 0;
    bi.uses_original_profile = JXL_FALSE;   // lossy is coded in XYB
    JxlColorEncoding srgb;
    JxlColorEncodingSetToSRGB(&srgb, channels < 3);
    bool ok = JxlEncoderSetBasicInfo(e, &bi) == JXL_ENC_SUCCESS &&
              JxlEncoderSetColorEncoding(e, &srgb) == JXL_ENC_SUCCESS;
    // The box holds the TIFF block behind a 4-byte offset to its header.
    static const uint8_t kSig[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (ok && exif.size() > 6 && std::memcmp(exif.data(), kSig, 6) == 0) {
        std::vector<uint8_t> box(4, 0);
        box.insert(box.end(), exif.begin() + 6, exif.end());
        ok = JxlEncoderUseBoxes(e) == JXL_ENC_SUCCESS &&
             JxlEncoderAddBox(e, "Exif", box.data(), box.size(), JXL_FALSE) == JXL_ENC_SUCCESS;
    }
    JxlEncoderFrameSettings* fs = JxlEncoderFrameSettingsCreate(e, nullptr);
    ok = ok && JxlEncoderSetFrameDistance(fs, JxlEncoderDistanceFromQuality((float)quality)) ==
                   JXL_ENC_SUCCESS &&
         JxlEncoderFrameSettingsSetOption(fs, JXL_ENC_FRAME_SETTING_EFFORT, effort) ==
             JXL_ENC_SUCCESS;
    // Alpha is a mask to the trainer: kept exact, where libjxl would use the
    // colour's distance, and so is the colour under it, which lossy drops. No
    // MA tree ("fast decode", libjxl says): a 33 MP RGBA decodes 14% faster.
    if (alpha)
        ok = ok && JxlEncoderSetExtraChannelDistance(fs, 0, 0.0f) == JXL_ENC_SUCCESS &&
             JxlEncoderFrameSettingsSetOption(fs, JXL_ENC_FRAME_SETTING_KEEP_INVISIBLE, 1) ==
                 JXL_ENC_SUCCESS &&
             JxlEncoderFrameSettingsSetFloatOption(
                 fs, JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT, 0.0f) ==
                 JXL_ENC_SUCCESS;
    const JxlPixelFormat fmt = {(uint32_t)channels, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    ok = ok && JxlEncoderAddImageFrame(fs, &fmt, px, (size_t)width * height * channels) ==
                   JXL_ENC_SUCCESS;
    JxlEncoderCloseInput(e);
    out.resize((size_t)width * height / 4 + 4096);
    ok = ok && take_output(e, out);
    JxlEncoderDestroy(e);
    JxlResizableParallelRunnerDestroy(runner);
    return ok ? "" : "libjxl could not encode the image";
}

std::string transcode_jpeg(const std::string& jpeg, const std::string& out, int threads) {
    std::vector<uint8_t> in;
    if (!read_file(jpeg, in)) return "cannot read " + jpeg;
    JxlEncoder* e = JxlEncoderCreate(nullptr);
    void* runner = JxlResizableParallelRunnerCreate(nullptr);
    JxlResizableParallelRunnerSetThreads(runner, (size_t)std::max(1, threads));
    JxlEncoderSetParallelRunner(e, JxlResizableParallelRunner, runner);
    JxlEncoderUseContainer(e, JXL_TRUE);
    JxlEncoderStoreJPEGMetadata(e, JXL_TRUE);
    JxlEncoderFrameSettings* fs = JxlEncoderFrameSettingsCreate(e, nullptr);
    bool ok = JxlEncoderAddJPEGFrame(fs, in.data(), in.size()) == JXL_ENC_SUCCESS;
    JxlEncoderCloseInput(e);
    std::vector<uint8_t> bytes(in.size());
    ok = ok && take_output(e, bytes);
    JxlEncoderDestroy(e);
    std::vector<uint8_t> back;
    const bool same = ok && reconstruct_jpeg(bytes, back, runner) && back == in;
    JxlResizableParallelRunnerDestroy(runner);
    if (!ok) return "libjxl could not transcode " + jpeg;
    if (!same) return "the JPEG XL of " + jpeg + " does not give the same JPEG back";
    const std::string tmp = out + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary);
        if (!f.write((const char*)bytes.data(), (std::streamsize)bytes.size()) || !f.flush())
            return "cannot write " + tmp;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, out, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return "cannot write " + out;
    }
    return "";
}

std::string probe(const std::string& path, Info& info) {
    std::vector<uint8_t> in;
    if (!read_file(path, in)) return "cannot read " + path;
    JxlDecoder* d = JxlDecoderCreate(nullptr);
    JxlDecoderSubscribeEvents(d, JXL_DEC_BASIC_INFO);
    JxlDecoderSetInput(d, in.data(), in.size());
    JxlDecoderCloseInput(d);
    JxlBasicInfo bi;
    const bool ok = JxlDecoderProcessInput(d) == JXL_DEC_BASIC_INFO &&
                    JxlDecoderGetBasicInfo(d, &bi) == JXL_DEC_SUCCESS;
    JxlDecoderDestroy(d);
    if (!ok) return "not a decodable JPEG XL: " + path;
    info.width = (int)bi.xsize;
    info.height = (int)bi.ysize;
    info.channels = (int)bi.num_color_channels + (bi.alpha_bits ? 1 : 0);
    info.bits = (int)bi.bits_per_sample;
    return "";
}

std::string decode_srgb8(const std::string& path, int channels, int threads, Info& info,
                         std::vector<uint8_t>& out) {
    return decode(path, channels, threads, JXL_TYPE_UINT8, 1, info,
                  [](void* v, size_t n) -> void* {
                      auto* o = (std::vector<uint8_t>*)v;
                      o->resize(n);
                      return o->data();
                  },
                  &out);
}

std::string decode_srgb16(const std::string& path, int channels, int threads, Info& info,
                          std::vector<uint16_t>& out) {
    return decode(path, channels, threads, JXL_TYPE_UINT16, 2, info,
                  [](void* v, size_t n) -> void* {
                      auto* o = (std::vector<uint16_t>*)v;
                      o->resize(n);
                      return o->data();
                  },
                  &out);
}
std::vector<uint8_t> read_exif(const std::string& path) {
    std::vector<uint8_t> in, box, out;
    if (!read_file(path, in)) return out;
    JxlDecoder* d = JxlDecoderCreate(nullptr);
    JxlDecoderSubscribeEvents(d, JXL_DEC_BOX);
    JxlDecoderSetDecompressBoxes(d, JXL_TRUE);
    JxlDecoderSetInput(d, in.data(), in.size());
    JxlDecoderCloseInput(d);
    bool in_exif = false;
    size_t used = 0;
    for (;;) {
        const JxlDecoderStatus s = JxlDecoderProcessInput(d);
        if (s == JXL_DEC_BOX || s == JXL_DEC_SUCCESS) {
            if (in_exif) {
                used = box.size() - JxlDecoderReleaseBoxBuffer(d);
                break;
            }
            if (s == JXL_DEC_SUCCESS) break;
            JxlBoxType type;
            JxlDecoderGetBoxType(d, type, JXL_TRUE);
            if (std::memcmp(type, "Exif", 4) == 0) {
                in_exif = true;
                box.resize(1 << 16);
                JxlDecoderSetBoxBuffer(d, box.data(), box.size());
            }
        } else if (s == JXL_DEC_BOX_NEED_MORE_OUTPUT && in_exif) {
            const size_t left = JxlDecoderReleaseBoxBuffer(d);
            const size_t have = box.size() - left;
            box.resize(box.size() * 2);
            JxlDecoderSetBoxBuffer(d, box.data() + have, box.size() - have);
        } else {
            break;
        }
    }
    JxlDecoderDestroy(d);
    // The box opens with a big-endian offset from its payload to the TIFF header.
    if (used > 8) {
        const size_t tiff = 4 + ((size_t)box[0] << 24 | box[1] << 16 | box[2] << 8 | box[3]);
        if (tiff + 8 <= used) {
            static const uint8_t kApp1[6] = {'E', 'x', 'i', 'f', 0, 0};
            out.assign(kApp1, kApp1 + 6);
            out.insert(out.end(), box.begin() + (ptrdiff_t)tiff, box.begin() + (ptrdiff_t)used);
        }
    }
    return out;
}
#else
static const char kOff[] = "JPEG XL images need a build with -DSS_WITH_JXL=ON: ";
bool available() { return false; }
std::string transcode_jpeg(const std::string& jpeg, const std::string&, int) { return kOff + jpeg; }
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

}  // namespace jxl
