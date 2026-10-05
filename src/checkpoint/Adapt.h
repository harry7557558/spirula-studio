#pragma once

// Checkpoint layout adaptation: rewrite a checkpoint's saved buffers to a
// layout different from the one they were written at, so training can resume
// with fewer Gaussians, a different SH degree, or with/without bilagrid/PPISP.
// The result is an ordinary state.tar that the unchanged (exact-match)
// engine_load_checkpoint() then loads.
//
// Everything runs on the host, buffer at a time -- the motivating case is
// resuming a run that just ran out of VRAM, so the adaptation cannot need any.
// Quantized buffers are decoded and re-encoded through the engine's own codecs
// in core/Tensor.h, not a copy of them.
//
// Splat reduction policy (target count = min(cur_ckpt, max_new)):
//   1. drop the tail (newest splats) by up to the checkpoint's unsaturated
//      slack (max_ckpt - cur_ckpt), then
//   2. drop the rest by lowest opacity.

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
    int     num_sh         = 0;    // non-DC SH coefficients per colour channel
    int     num_images     = 0;    // POST-split camera count
    // Grid extents as (L, H, W); unset means the target has no such channel.
    std::optional<std::array<int, 3>> bilagrid_rgb;
    std::optional<std::array<int, 3>> bilagrid_depth;
    std::optional<std::array<int, 3>> bilagrid_normal;
    bool    ppisp = false;
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
