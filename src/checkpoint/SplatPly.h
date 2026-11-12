#pragma once

// Reading a splat.ply back in, in the layout the original 3D Gaussian
// Splatting release established: x y z / nx ny nz / f_dc_0..2 / f_rest_* /
// sb_params_* / opacity / scale_0..2 / rot_0..3, raw -- log scales, logit
// opacity, (w,x,y,z) quaternion, DC as the SH coefficient (colour - 0.5) / C0.
// sb_params_{f * L + l} is field f of spherical-beta lobe l, as Deformable
// Beta Splatting's code and gsplat write it. The file stores f_rest
// channel-major and the lobes field-major; the engine wants both
// coefficient-major. Used by `spirula mesh` and the GUI's viewer screen.

#include "core/ColorBasis.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace spirula {

// Raw Gaussians, as stored. Nothing here is activated: scales are logs,
// opacities logits, and the caller hands them to the engine unchanged.
struct SplatCloud {
    int64_t num = 0;
    int sh_degree = 0;               // 0 when the file carries no f_rest
    int sb_lobes = 0;                // 0 when it carries no sb_params
    std::string primitive;
    std::vector<float> means;        // [num, 3]
    std::vector<float> quats;        // [num, 4]  (w, x, y, z)
    std::vector<float> scales;       // [num, 3]  log
    std::vector<float> opacities;    // [num, 1]  logit
    std::vector<float> features_dc;  // [num, 3]
    // [num, coeffs(), 3], coefficient-major, SH bands then lobes
    // (core/ColorBasis.h) -- what set_data_3dgs wants. Empty when coeffs() is 0.
    std::vector<float> features_sh;

    int64_t dim_sh() const { return (int64_t)(sh_degree + 1) * (sh_degree + 1); }
    ColorBasis basis() const { return {sh_degree, sb_lobes}; }
    int64_t coeffs() const { return basis().coeffs(); }
};

// True when `path` is a PLY whose vertex element carries the 3DGS properties.
// False for a plain point cloud, a mesh, or a file that is not a PLY at all --
// it answers a question about the file, so it never throws.
bool is_splat_ply(const std::string& path);

// Throws std::runtime_error naming the file and what was wrong with it.
// `want_sh` false skips the f_rest and sb_params columns, which are most of
// the file: the mesher only needs geometry and DC colour.
SplatCloud read_splat_ply(const std::string& path, bool want_sh = true);
// The same file in runs of up to `chunk_rows` splats, front to back: `fn` gets
// each run (num = its length, arrays reused between calls), its first splat's
// index and the file's total. Host memory stays at one run.
void read_splat_ply_chunks(
    const std::string& path, int64_t chunk_rows,
    const std::function<void(const SplatCloud& rows, int64_t first, int64_t total)>& fn,
    bool want_sh = true);

// The layout read_splat_ply expects. `keep` is a flag per splat; `moved` is
// applied row by row, so baking a placement needs no second copy.
void write_splat_ply(const SplatCloud& c, const std::string& path,
                     const uint8_t* keep = nullptr,
                     const class SplatTransform* moved = nullptr);

// The same layout for a writer that fills rows itself (EngineCheckpoint.cpp);
// splat_ply_put_rest() turns a [basis.coeffs(), 3] colour row into the file
// order that follows the DC.
std::string splat_ply_header(int64_t vertices, const ColorBasis& basis,
                             const std::string& primitive = "");
int splat_ply_row_floats(const ColorBasis& basis);
void splat_ply_put_rest(const float* rest, const ColorBasis& basis, float* out);

// Resolve what a user pointed at into (splat.ply, run directory): a .ply
// directly, a step-*.ckpt / *.ckpt directory holding one, or a run directory
// whose newest checkpoint has one. The run directory is where config.json is
// looked for; it may not exist, and for a loose .ply it is a guess.
// Throws when nothing under `path` is a splat.ply.
std::pair<std::string, std::string> find_splat_ply(const std::string& path);

}  // namespace spirula
