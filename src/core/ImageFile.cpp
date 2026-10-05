#include "core/ImageFile.h"

#include "core/ExrImage.h"
#include "core/TiffImage.h"

namespace imagefile {

bool handles(const std::string& path) {
    return exr::is_exr(path) || tiff::is_tiff(path);
}

std::string probe(const std::string& path, Info& info) {
    if (exr::is_exr(path)) {
        exr::Info e;
        if (const std::string err = exr::probe(path, e); !err.empty()) return err;
        info = {e.width, e.height, e.channels};
        return "";
    }
    tiff::Info t;
    if (const std::string err = tiff::probe(path, t); !err.empty()) return err;
    info = {t.width, t.height, t.channels};
    return "";
}

bool declared_color_space(const std::string& path, DeclaredColor& out) {
    if (exr::Info e; exr::declared_color_space(path, e)) {
        out = {"EXR", e.gamut, e.is_linear, e.gamut_known};
        return true;
    }
    if (tiff::Info t; tiff::declared_color_space(path, t)) {
        out = {"TIFF", t.gamut, t.is_linear, t.gamut_known};
        return true;
    }
    return false;
}

std::string decode_srgb8(const std::string& path, const Options& opt, Info& info,
                         std::vector<uint8_t>& out, const std::string& gamut,
                         std::optional<bool> is_linear, std::vector<uint8_t>* unexposed) {
    if (exr::is_exr(path)) {
        exr::Info e;
        exr::Options o;
        o.channels = opt.channels;
        o.threads = opt.threads;
        o.exposure = opt.exposure;
        const std::string err =
            exr::decode_srgb8(path, o, e, out, gamut, is_linear, unexposed);
        info = {e.width, e.height, e.channels, e.peak, e.gain};
        return err;
    }
    tiff::Info t;
    tiff::Options o;
    o.channels = opt.channels;
    o.threads = opt.threads;
    o.exposure = opt.exposure;
    const std::string err = tiff::decode_srgb8(path, o, t, out, gamut, is_linear, unexposed);
    info = {t.width, t.height, t.channels, t.peak, t.gain};
    return err;
}

}  // namespace imagefile
