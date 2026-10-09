#pragma once

// CubeLut -- a 3D colour look-up table from a .cube file (the Adobe/Resolve
// text format camera makers ship their log-to-Rec.709 LUTs in), applied to
// 8-bit RGB frames as they are extracted from a video.
//
// Tetrahedral interpolation, as grading software uses: trilinear bends the
// grey axis of a log LUT visibly, tetrahedral keeps it on the diagonal.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace app {

struct CubeLut {
    int size = 0;                       // samples per axis
    float domain_min[3] = {0, 0, 0};
    float domain_max[3] = {1, 1, 1};
    std::vector<float> rgb;             // size^3 * 3, red varying fastest
    std::string title;
    bool empty() const { return size < 2; }
};

// Parses a 3D .cube file. A 1D-only file, a missing or wrong number of
// samples, or anything unreadable fails with `error` set.
bool load_cube_lut(const std::string& path, CubeLut& out, std::string& error);

// The same file loaded once per process; null with `error` set on failure.
std::shared_ptr<const CubeLut> cube_lut_cached(const std::string& path,
                                               std::string& error);

// Maps `pixels` interleaved RGB8 pixels in place. `threads` 0 = hardware.
void apply_cube_lut(const CubeLut& lut, uint8_t* rgb, size_t pixels, int threads = 0);

// `path` as the value of ffmpeg's lut3d `file` option inside a filter graph,
// escaped for both levels ffmpeg parses it at.
std::string ffmpeg_lut3d_filter(const std::string& path);

}  // namespace app
