#pragma once

// ConfigUI -- the "Advanced options" editor. All widgets are expanded from
// the SS_CONFIG_FIELDS X-macro (config/TrainConfig.h), so every one of
// the training config fields is editable, with:
//   - grouping under the field table's `section` headings, collapsed by
//     default so novices are not overwhelmed
//   - a detail filter over the field table's `tier`, so the default view is
//     the ~20 flags a first run needs rather than all 178
//   - a search box filtering by flag name, label and help text, which ignores
//     the detail filter: searching finds a flag wherever it is buried
//   - a translated label per row (i18n/catalog/TrainFields.h), with the
//     `--flag` it stands for and its help text as a hover tooltip (+ the
//     preset default)
//   - modified-from-preset highlighting and right-click "Reset to default"
//   - fields with `choices` as dropdowns, std::optional as auto/override
//
// A row added to the field table appears here automatically -- no GUI change
// needed.

#include "config/TrainConfig.h"

#include <set>
#include <string>
#include <vector>

namespace gui {

// A path field's "..." click. GuiApp owns the one file dialog, so the editor
// leaves the request here and the caller opens it.
struct PathPick {
    std::string field;                    // the flag; empty when none asked
    std::string title;
    bool folder = false;
    std::vector<std::string> extensions;
    std::string start_dir;                // "" = where the last pick landed
};

struct ConfigUIState {
    char search[128] = "";
    bool modified_only = false;
    int  tier = 0;   // rank into kTrainTiers: show this specialist and below

    // Flags the user edited here, by name. Macro options (--quality and
    // friends) leave these alone, so touching a flag by hand takes it out of
    // a macro's reach for good -- see train_resolve_macros(). Reset when the
    // whole config is, i.e. on a preset change.
    std::set<std::string> touched;

    // Section fold state, ours rather than ImGui's: filtering force-opens the
    // sections it hits, and clearing the filter restores what was open before
    // it, keeping what the user opened or edited while it was on.
    bool open[kTrainNumSections] = {};
    bool saved_open[kTrainNumSections] = {};
    signed char sticky[kTrainNumSections] = {};   // -1 closed, +1 open, 0 unset
    std::string filter_key;
    bool filtering = false;

    // What `search` matches, per field; recomputed when the box changes.
    std::string match_query;
    std::vector<char> match;

    PathPick pick;
};

// How one value of a `choices` field is written in a dropdown: the value
// itself, with the translated word for it in front where there is one --
// "按间隔 (interval)". The value stays visible because it is what the flag
// takes, what config.json holds and what the help text names; an English
// reader sees it alone, the two being the same word. Shared with the Basic
// Options panel, which draws the macro options by hand.
std::string choice_display(const char* flag, const std::string& value);

// Draw the full generated editor. `defaults` is the preset-applied baseline
// used for modified-highlighting and reset. Returns true when any field
// changed this frame; the caller re-resolves the macro options when it does.
bool draw_config_editor(TrainConfig& cfg, const TrainConfig& defaults,
                        ConfigUIState& st);

// What a PathPick's dialog returned, stored into its field and marked edited.
// A "<data>/" field stores a pick inside the dataset folder relative to it.
void apply_path_pick(TrainConfig& cfg, ConfigUIState& st,
                     const std::string& field, const std::string& path);

}  // namespace gui
