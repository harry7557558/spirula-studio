#pragma once

// Training images downscaled by an integer train_resolution_divisor, kept on
// disk beside the originals (<image_dir>_<d>/, the Mip-NeRF 360 layout) so a
// later load decodes the small file instead of the full one. JPEG and PNG
// only; an existing file there is used when it fits and never replaced.

#include <cstdint>
#include <string>
#include <vector>

namespace dscache {

enum class Format { None, Jpeg, Png };

// By the file's first bytes, not its name.
Format format_of(const std::string& path);

// Where `image` caches at divisor `d`: its path under `image_dir` moved to
// `<image_dir>_<d>`, or `<parent>_<d>/<name>` when it is not under `image_dir`.
// "" when its format is not cached.
std::string path_for(const std::string& image, const std::string& image_dir, int d);

// `full` divided by `d` under `mode` (floor | ceil | round), never below 1.
int scaled_side(int full, int d, const std::string& mode);

// True when `cached` is the size `image` divided by `d` gives under any of the
// rounding modes, and ends the way a complete file of its format does.
bool usable(const std::string& cached, const std::string& image, int d);

// `px` (w x h x channels, uint8 or host-order uint16 by `bits`) as a file of
// `fmt`; empty when that format cannot hold it.
std::vector<uint8_t> encode(Format fmt, const void* px, int w, int h, int channels,
                            int bits);

// Writes `bytes` to `cached` unless a file is already there; true when one is
// there afterwards. A concurrent writer of the same file is harmless.
bool store(const std::string& cached, const std::vector<uint8_t>& bytes);

}  // namespace dscache
