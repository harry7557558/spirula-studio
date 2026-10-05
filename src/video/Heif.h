#pragma once
// HEIF stills (.heic): the ISO/IEC 23008-12 container read far enough to hand
// the primary image's H.265 tiles to VideoPipeline. Patent-gated with the rest
// of src/video/, container included. README.md, "HEIF stills".

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace video {

// One transformative property, in the order the item lists them.
struct HeifOp {
    enum Kind { Crop, Rotate, Mirror };
    Kind kind = Crop;
    int x = 0, y = 0, w = 0, h = 0;   // Crop, in pixels of the image it applies to
    int ccw = 0;                      // Rotate: quarter turns ANTIclockwise (irot)
    // Mirror: imir mode 1. HEIF Amd 2 inverted the first edition's `axis`;
    // libheif, libavif and ffmpeg read it the amended way, 0 = top-bottom.
    bool left_right = true;
};

struct HeifImage {
    // The coded image, or a grid's output size, before `ops`.
    int width = 0, height = 0;
    int cols = 1, rows = 1;
    int tile_width = 0, tile_height = 0;      // 0 when no ispe says
    std::vector<uint8_t> hvcc;                // every tile's decoder configuration
    std::vector<std::vector<uint8_t>> tiles;  // row-major, length-prefixed NAL units
    std::vector<HeifOp> ops;
    // An nclx colr box, which overrides the bitstream's VUI.
    bool has_nclx = false;
    int  matrix_coefficients = 2;
    bool full_range = false;
    std::vector<uint8_t> exif;                // "Exif\0\0" + TIFF, or empty
};

// Reads `size` bytes at `offset` of the file; false past its end.
using HeifRead = std::function<bool(uint64_t offset, size_t size, uint8_t* dst)>;

// `with_tiles` false reads no coded data: enough for the size and the EXIF.
bool parse_heif(const HeifRead& read, uint64_t file_size, bool with_tiles,
                HeifImage& out, std::string& error);
bool read_heif(const std::string& path, bool with_tiles, HeifImage& out,
               std::string& error);

// `width` x `height` carried through `ops`.
void heif_display_size(const HeifImage& im, int& w, int& h);

// `ops` applied, in order, to an interleaved RGB image.
void heif_apply_ops(const std::vector<HeifOp>& ops, std::vector<uint8_t>& rgb, int& w,
                    int& h);

}  // namespace video
