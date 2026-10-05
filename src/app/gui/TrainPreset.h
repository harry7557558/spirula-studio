#pragma once

// A saved training preset: the whole training config, under a name the user
// chose. PresetFile.h owns the file's header and its folder; this kind adds
// "base_preset" (the built-in it started from), "touched", and "config" --
// the same flat flag table a run's config.json carries.
//
// A run's own config.json loads as one too: the same table, naming its
// built-in under "preset" instead. Reusing the settings of a run that came
// out well is the most likely reason anybody wants this at all.
//
// `touched` is what stops the macro options undoing hand tuning
// (train_resolve_macros); a file without one derives it by diffing the base.

#include "app/gui/PresetFile.h"
#include "config/TrainConfig.h"

#include <set>
#include <string>
#include <vector>

namespace gui {

// What a preset must NOT carry: where the data is, which part of it trains,
// where the run goes. A preset is "how", the trainer screen and a batch row
// "where" -- so any preset can be paired with any dataset.
#define SS_PRESET_CONTEXT_FIELDS(X) \
    X(data) X(resume) X(output_dir_prefix) X(output_dir_name) X(roi_region) \
    /* end */

struct TrainPreset {
    std::string name;           // what the user called it
    std::string description;    // free text, may be empty
    std::string base = "3dgs";  // the built-in preset it started from
    std::string path;           // the file it was read from / written to
    TrainConfig cfg;
    std::set<std::string> touched;
};

// Write. Throws std::runtime_error if the file cannot be created. The context
// fields above are dropped here rather than at the call site, so no caller can
// forget and bake a dataset path into a shared preset.
void save_preset(const TrainPreset& p, const std::string& path);

// Read a preset file, or a run's config.json. Throws std::runtime_error when
// the file is unreadable, is not JSON, or names no training flag at all.
TrainPreset load_preset(const std::string& path);

// Cheap probe for drag-and-drop: would load_preset() accept this file? Never
// throws -- a JSON file that is something else entirely just answers false.
bool is_preset_file(const std::string& path);

// Delete a preset file. Throws std::runtime_error when the file is not a
// preset (nothing else may be deleted through here, whatever path a caller
// hands over) or when the filesystem refuses.
void delete_preset(const std::string& path);

// Every readable *.json in preset_dir(PresetKind::Train), sorted by name.
std::vector<TrainPreset> list_presets();

}  // namespace gui
