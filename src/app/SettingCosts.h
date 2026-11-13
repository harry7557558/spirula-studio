#pragma once

// What a basic training setting does to the kernels it changes: factors on a
// step's cells, relative to the defaults (3dgs, no floater suppression, basic
// calibration correction, SH degree 3). Paired runs on garden and bonsai, RTX
// 5070, docs/notes/step-timing.md. Settings are taken as independent, so a
// switch scales the cells by the ratio of its two values' factors.

#include "engine/StepTiming.h"

#include <cstring>
#include <string>

namespace spirula {

struct SettingCost {
    const char* key;       // the TrainConfig field
    const char* value;
    double raster_fwd;     // render x splats x pixels
    double raster_bwd;     // backward x splats x pixels
    double projection;     // render x splats
    double optimizer;      // optimizer x splats
    double own;            // the setting's own row (`own_part`)
};

inline constexpr SettingCost kSettingCosts[] = {
    // Distortion terms weigh on the backward; the forward gets cheaper as
    // max_screen_size caps splat size. Strong adds the local-PCA regularizer.
    {"floater_suppression", "off",      1.00, 1.00, 1.00, 1.00, 0.0},
    {"floater_suppression", "mild",     0.99, 1.44, 1.00, 1.00, 0.0},
    {"floater_suppression", "moderate", 0.96, 1.40, 1.00, 1.00, 0.0},
    {"floater_suppression", "strong",   0.90, 1.30, 1.00, 1.00, 1.0},
    // The depth the warp reads is rendered anyway by the default normal term.
    {"calib_correction", "off",      1.00, 1.00, 1.00, 1.00, 0.0},
    {"calib_correction", "basic",    1.00, 1.00, 1.00, 1.00, 1.0},
    {"calib_correction", "accurate", 1.00, 1.00, 1.00, 1.00, 1.46},
    {"primitive", "3dgs",  1.00, 1.00, 1.00, 1.00, 1.0},
    {"primitive", "mip",   1.05, 1.03, 1.00, 1.00, 1.0},
    {"primitive", "3dgut", 1.61, 1.87, 1.00, 1.00, 1.0},
    // Colour coefficients, 15 / 8 / 3 / 0: projection evaluates them and the
    // optimizer steps them; at degree 0 it keeps no state for them at all.
    {"sh_degree", "3", 1.00, 1.00, 1.00, 1.00, 1.0},
    {"sh_degree", "2", 1.00, 1.00, 0.84, 0.74, 1.0},
    {"sh_degree", "1", 1.00, 1.00, 0.65, 0.56, 1.0},
    {"sh_degree", "0", 1.00, 1.00, 0.55, 0.28, 1.0},
};

inline const SettingCost* setting_cost(const char* key, const std::string& value) {
    for (const SettingCost& c : kSettingCosts)
        if (std::strcmp(c.key, key) == 0 && value == c.value) return &c;
    return nullptr;
}

// The row a setting's `own` factor scales.
inline StepPart own_part(const char* key) {
    if (std::strcmp(key, "floater_suppression") == 0) return StepPart::Regularizer;
    if (std::strcmp(key, "calib_correction") == 0) return StepPart::Calibration;
    return StepPart::Count;
}

// What switching `key` from `from` to `to` takes off cells `c`, in seconds;
// 0 when the table knows neither value.
inline double setting_saving(const StepCells& c, const char* key, const std::string& from,
                             const std::string& to) {
    const SettingCost* a = setting_cost(key, from);
    const SettingCost* b = setting_cost(key, to);
    if (!a || !b) return 0.0;
    const auto cut = [](double t, double fa, double fb) { return fa > 0.0 ? t * (1.0 - fb / fa) : 0.0; };
    double saved = cut(c.at(StepPart::Render, StepScale::SplatsPixels), a->raster_fwd, b->raster_fwd) +
                   cut(c.at(StepPart::Backward, StepScale::SplatsPixels), a->raster_bwd, b->raster_bwd) +
                   cut(c.at(StepPart::Render, StepScale::Splats), a->projection, b->projection) +
                   cut(c.at(StepPart::Optimizer, StepScale::Splats), a->optimizer, b->optimizer);
    const StepPart own = own_part(key);
    if (own != StepPart::Count) saved += cut(c.part(own), a->own, b->own);
    return saved;
}

}  // namespace spirula
