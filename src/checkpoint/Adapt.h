#pragma once

// Checkpoint layout adaptation: rewrite a checkpoint's saved buffers so
// training can resume with fewer Gaussians, another SH degree or lobe count,
// or with/without bilagrid/PPISP; engine_load_checkpoint() then loads the
// result exactly. Host-only, buffer at a time -- the motivating case is a run
// that just ran out of VRAM -- and quantized buffers go through the engine's
// own codecs in core/Tensor.h.
//
// Splats past min(cur_ckpt, max_new) go newest first, by up to the
// checkpoint's unsaturated slack (max_ckpt - cur_ckpt), then faintest first.

#include "data/Json.h"

#include <array>
#include <filesystem>
#include <optional>

namespace ckpt {

// The layout the engine skeleton actually holds. Read off the live engine
// rather than re-derived from the config: whether a depth/normal grid exists
// also depends on the dataset carrying those maps, which only setup knows.
struct TargetLayout {
    int64_t max_num_splats = 0;
    int     num_sh         = 0;    // float3 per features_sh row, lobes included
    int     sb_lobes       = 0;
    int     num_images     = 0;    // POST-split camera count
    // Grid extents as (L, H, W); unset means the target has no such channel.
    std::optional<std::array<int, 3>> bilagrid_rgb;
    std::optional<std::array<int, 3>> bilagrid_depth;
    std::optional<std::array<int, 3>> bilagrid_normal;
    bool    ppisp = false;
    // (grid, floats per control point) of the calibration warp's table.
    std::optional<std::array<int, 2>> calib_warp;
};

// True when `state` (a checkpoint's state.json) disagrees with `t` in any way
// that changes a buffer's shape.
bool needs_adapt(const JsonValue& state, const TargetLayout& t);

// Write an adapted `out_dir/state.tar`. Returns false and writes nothing when
// needs_adapt() is false -- the caller then loads the original directly.
bool adapt_checkpoint(const std::filesystem::path& ckpt_dir,
                      const TargetLayout& t,
                      const std::filesystem::path& out_dir);

}  // namespace ckpt
