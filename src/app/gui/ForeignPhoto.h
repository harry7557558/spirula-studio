#pragma once

// Photo formats nothing past dataset preparation reads -- HEIC / HEIF, JPEG XL,
// AVIF -- so they always become JPEGs on the way in. HEIF has an in-process
// decoder (src/video/, SS_HAVE_VIDEO) that hands back the pixels and the EXIF.
// JPEG XL and AVIF, and HEIF without that decoder, go through an external
// ffmpeg, which applies the turn and mirror but writes no EXIF; a JPEG XL's
// EXIF box is read here and spliced back in.

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// .heic, .heif or .hif, by the name alone.
bool is_heif_path(const std::string& path);

// HEIF, .jxl or .avif, by the name alone.
bool is_foreign_photo_path(const std::string& path);

// A foreign photo this build cannot train on as it is: HEIC always, JPEG XL and
// AVIF without libjxl / libavif.
bool photo_needs_jpeg(const std::string& path);

// In-process: interleaved RGB and, when `exif` is given, the JPEG APP1 payload
// ("Exif\0\0" + TIFF) or nothing. The device must already be frozen. False
// with `error` in a build without the decoder.
bool decode_heif_builtin(const std::string& path, int& w, int& h,
                         std::vector<uint8_t>& rgb, std::vector<uint8_t>* exif,
                         std::string& error);

// ffmpeg writes `jpeg` itself; HEIF one call at a time process-wide. False
// with `error` a sentence for the log.
bool ffmpeg_photo_to_jpeg(const std::string& ffmpeg_exe, const std::string& photo,
                          const std::string& jpeg, const std::atomic<bool>& cancel,
                          std::string& error);
// The same, or with `rgba_png` an RGBA PNG that keeps the photo's alpha.
bool ffmpeg_photo_convert(const std::string& ffmpeg_exe, const std::string& photo,
                          const std::string& out, bool rgba_png,
                          const std::atomic<bool>& cancel, std::string& error);

// A JPEG XL or AVIF that carries alpha, from its header alone: a JPEG XL's
// extra channels, an AVIF's auxiliary alpha image. False for HEIF.
bool foreign_photo_has_alpha(const std::string& path);

// A JPEG XL's EXIF as a JPEG APP1 payload, empty when it has none. False when
// the photo may carry EXIF this does not read: HEIF, AVIF, or a JPEG XL whose
// Exif box is Brotli-compressed ('brob').
bool foreign_photo_exif(const std::string& path, std::vector<uint8_t>& exif);

// The upright size from the file's own header: HEIF where this build reads
// one, JPEG XL always, AVIF never.
bool foreign_photo_size(const std::string& path, int& w, int& h);

// Pixels for a preview: HEIF's built-in decoder unless `builtin` is false or
// it fails, then ffmpeg through a temporary JPEG. `error` is the last one's.
bool load_foreign_photo(const std::string& path, bool builtin,
                        const std::string& ffmpeg_exe, int& w, int& h,
                        std::vector<uint8_t>& rgb, const std::atomic<bool>& cancel,
                        std::string& error);

}  // namespace gui
