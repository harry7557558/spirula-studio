#pragma once

// Progressive alignment's settings per --quality level, read by the CLI's
// presets and by the GUI's quality control alike. Provisional: more attempts
// than the 260-image capture needed (docs/notes/sfm-progressive-alignment.md);
// still to be measured on larger and harder captures.

namespace sfm {

struct ProgressivePreset {
    float error_start;   // px; the end is the run's --max-error
    int error_steps;     // attempts
    bool features;       // feature passes on the images left out
    int feature_steps;
    int patience;
};

// low, medium, high, extreme: the GUI's quality index.
inline constexpr ProgressivePreset kProgressivePresets[4] = {
    {12.0f, 5, false, 1, 1},
    {16.0f, 8, true, 2, 2},
    {20.0f, 12, true, 3, 3},
    {24.0f, 15, true, 4, 4},
};

}  // namespace sfm
