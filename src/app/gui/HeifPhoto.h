#pragma once

// HEIC / HEIF photos, which nothing past dataset preparation reads, so they
// always become JPEGs on the way in. The in-process decoder (src/video/,
// SS_HAVE_VIDEO) hands back the pixels and the EXIF; without it, or where it
// fails, an external ffmpeg converts the file and the EXIF stays behind. Both
// apply the container's crop, turn and mirror, so the pixels are upright.

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// .heic, .heif or .hif, by the name alone.
bool is_heif_path(const std::string& path);

// In-process: interleaved RGB and, when `exif` is given, the JPEG APP1 payload
// ("Exif\0\0" + TIFF) or nothing. The device must already be frozen. False
// with `error` in a build without the decoder.
bool decode_heif_builtin(const std::string& path, int& w, int& h,
                         std::vector<uint8_t>& rgb, std::vector<uint8_t>* exif,
                         std::string& error);

// ffmpeg writes `jpeg` itself, one call at a time process-wide. False with
// `error` a sentence for the log.
bool ffmpeg_heif_to_jpeg(const std::string& ffmpeg_exe, const std::string& heif,
                         const std::string& jpeg, const std::atomic<bool>& cancel,
                         std::string& error);

// The upright size, from the container, where this build reads one.
bool heif_size(const std::string& path, int& w, int& h);

// Pixels for a preview: the built-in decoder unless `builtin` is false or it
// fails, then ffmpeg through a temporary JPEG. `error` is the last one's.
bool load_heif(const std::string& path, bool builtin, const std::string& ffmpeg_exe,
               int& w, int& h, std::vector<uint8_t>& rgb,
               const std::atomic<bool>& cancel, std::string& error);

}  // namespace gui
