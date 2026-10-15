#pragma once

#include <cstdint>
#include <string>

namespace spirula {

struct LodBuildConfig {
    int max_leaf_size = 32;          // max Gaussians per leaf node
    int max_parent_gaussians = 8;    // summary Gaussians per internal node
    float sh_band_decay = 0.5f;      // attenuation per SH band in parent
    int target_depth = 0;            // 0 = automatic depth from splat count
    bool verbose = false;
};

struct LodRenderConfig {
    bool enabled = false;
    float quality_threshold = 1.0f;   // screen-space error threshold in pixels
    float hysteresis_band = 0.2f;     // ±band around threshold for hysteresis
    int64_t vram_budget_mb = 0;       // 0 = no budget (load all), else MB
    int prefetch_depth = 1;           // hierarchy levels to prefetch beyond cut
    bool debug_colorize_levels = false;
    bool freeze_lod = false;          // lock current LOD selection
    float transition_band = 0.1f;     // fraction of threshold for opacity fade
};

}  // namespace spirula
