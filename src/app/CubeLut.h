#pragma once

// CubeLut -- a 3D colour look-up table, from a .cube file (the Adobe/Resolve
// text format camera makers ship their log-to-Rec.709 LUTs in) or baked from
// one of the built-in fitted ones (app/LogLuts.h), and how a frame goes
// through it.
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
    uint64_t serial = 0;                // distinct per table cube_lut_cached made
    bool empty() const { return size < 2; }
};

// A LUT as an input row, a dataset record and --lut name it: a .cube path, or
// this prefix and a built-in's id.
inline constexpr const char* kBuiltinLutPrefix = "builtin:";
bool is_builtin_lut(const std::string& spec);
std::string builtin_lut_spec(const std::string& id);
// The built-in's name, or the file's.
std::string lut_display_name(const std::string& spec);

// Parses a 3D .cube file. A 1D-only file, a missing or wrong number of
// samples, or anything unreadable fails with `error` set.
bool load_cube_lut(const std::string& path, CubeLut& out, std::string& error);
bool write_cube_lut(const CubeLut& lut, const std::string& path, std::string& error);

// `spec` read or baked once per process, and again when the file changes;
// null with `error` set on failure.
std::shared_ptr<const CubeLut> cube_lut_cached(const std::string& spec,
                                               std::string& error);

// A .cube on disk holding `spec`: the file itself, or a built-in written to
// the temp directory, named by its contents.
bool cube_lut_file(const std::string& spec, std::string& path, std::string& error);

// Maps `pixels` interleaved RGB8 pixels in place. `threads` 0 = hardware.
void apply_cube_lut(const CubeLut& lut, uint8_t* rgb, size_t pixels, int threads = 0);
// Maps big-endian RGB16 samples normalized by 65535, quantizing only the result.
void apply_cube_lut_rgb48be(const CubeLut& lut, const uint8_t* rgb48, uint8_t* rgb8,
                           size_t pixels, int threads = 0);

// `path` as the value of ffmpeg's lut3d `file` option inside a filter graph,
// escaped for both levels ffmpeg parses it at.
std::string ffmpeg_lut3d_filter(const std::string& path);

}  // namespace app
