#pragma once

// The built-in reconstruction's options editor: what the quality level sets,
// then every other matching and mapping flag, all drawn from sfm/SfmConfig.h's
// field table -- a flag added there appears here with no GUI change. An edit
// lands in SfmJob::options (max-image-size and the feature budget in their own
// SfmJob fields) and is passed to the run over the quality level's value.

#include <string>

namespace gui {

struct SfmJob;

struct SfmOptionsState {
    std::string search;
    bool modified_only = false;
};

// Flags with a control of their own elsewhere on the panel, kept off this one.
bool sfm_option_panel_owned(const std::string& flag);

// False in a build without the built-in engine, which has no table to draw.
bool draw_sfm_options_editor(SfmJob& job, SfmOptionsState& st);

}  // namespace gui
