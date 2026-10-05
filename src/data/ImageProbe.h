#pragma once

// An image file's pixel size without decoding it -- stb_image's header reader,
// or core/ImageFile.h's. Signature matches DatasetParserConfig::probe_image_size,
// which the WebAssembly viewer leaves null because it has neither decoder.

#include <cstdint>
#include <string>
#include <vector>

bool probe_image_size(const char* path, int* w, int* h);

// [N] flags for the images whose alpha channel is a cut-out, or empty when none
// is. The headers say which files have alpha; a few of them are decoded, since
// an RGBA export that is opaque everywhere carries no mask.
std::vector<uint8_t> probe_alpha_masks(const std::vector<std::string>& paths);
