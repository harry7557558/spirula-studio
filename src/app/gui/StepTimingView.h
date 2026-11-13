#pragma once

// The trainer's step-time breakdown on screen: the card the status strip's
// "details" link shows on hover and pins open on click.
// docs/notes/step-timing.md.

#include "app/TrainForecast.h"
#include "config/TrainConfig.h"

#include <array>
#include <string>

namespace gui {

// What the savings name: whether the run split its frames into views, whether
// that came from the 360-camera preset, and the presets as the picker shows them.
struct SavingsContext {
    bool frames_split = false;
    double images_kept = 1.0;   // without the split: frames over the views they became
    bool preset_360 = false;
    std::string preset_360_label, preset_general_label;
    int sh_degree = 0;
};

struct StepTimingViewState {
    bool whole_run = false;
    bool grid = false;
    // A row or column once shown stays for the run, in both modes, so the
    // card does not jump as shares cross the threshold.
    std::array<bool, kStepParts> part_shown{};
    std::array<bool, kStepScales> scale_shown{};
    bool unaccounted_shown = false;

    // A different run, or the same one started over, forgets what was shown.
    void follow(const void* run, int step) {
        if (run != _run || step < _step) {
            part_shown = {};
            scale_shown = {};
            unaccounted_shown = false;
        }
        _run = run;
        _step = step;
    }

private:
    const void* _run = nullptr;
    int _step = 0;
};

// Per step (`step`) or the whole run (`run`), as two lists or a grid of
// purpose by what the time grows with, then the largest savings. `pinned`
// adds the switches between them; call inside a tooltip or a popup.
void step_timing_card(const spirula::TimeBreakdown& step, const spirula::TimeBreakdown& run,
                      const TrainConfig& cfg, const SavingsContext& ctx,
                      StepTimingViewState& st, bool pinned);

}  // namespace gui
