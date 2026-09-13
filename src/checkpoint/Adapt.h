#pragma once

// Host-only checkpoint retargeting for recovery without spare VRAM.
// Output uses the engine loader's exact-size state.tar format.

#include "data/Json.h"

#include <array>
#include <filesystem>
#include <optional>

namespace ckpt {

// Resolve optional channels during setup and the fused layout before the first step.
struct TargetLayout {
    int64_t max_num_splats = 0;
    int     num_sh         = 0;    // SH REST coefficients per channel (15 = SH3)
    int     num_images     = 0;    // POST-split camera count
    // Grid extents as (L, H, W); unset means the target has no such channel.
    std::optional<std::array<int, 3>> bilagrid_rgb;
    std::optional<std::array<int, 3>> bilagrid_depth;
    std::optional<std::array<int, 3>> bilagrid_normal;
    bool    ppisp = false;
    bool    fused_proj_bwd_optim = false;
};

// Includes buffer shapes and quantized SH layout.
bool needs_adapt(const JsonValue& state, const TargetLayout& t);

// Write an adapted `out_dir/state.tar`. Returns false and writes nothing when
// needs_adapt() is false -- the caller then loads the original directly.
bool adapt_checkpoint(const std::filesystem::path& ckpt_dir,
                      const TargetLayout& t,
                      const std::filesystem::path& out_dir);

}  // namespace ckpt
