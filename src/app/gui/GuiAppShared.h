#pragma once

// Helpers shared by the files that implement GuiApp. Each comment names the
// file that defines it.

#include "app/gui/GuiApp.h"
#include "i18n/Message.h"

#include <string>
#include <vector>

namespace gui {

// GuiApp.cpp

// Hand a value to the reconstruction engines through the process environment
// (the SS_* namespace core/Env.h owns); the SfM subprocess inherits it too.
// Empty clears.
void set_ss_env(const char* suffix, const std::string& value);
// A tool location is typed as the folder that holds the program as often as
// the program itself (issue #201). A bare name stays a PATH lookup.
std::string tool_in_folder(const std::string& path, const std::string& name);

// TrainScreen.cpp

std::string format_count(double n);
// True when two configs parse to the same dataset -- every field
// load_dataset() consumes, listed as SS_DATASET_PARSE_FIELDS.
bool parse_settings_equal(const TrainConfig& a, const TrainConfig& b);

// DropAndPick.cpp

// What "Open a Model or Reconstruction" shows: models, and how a reconstruction
// is named when the desktop's picker cannot return a folder (transforms.json,
// cameras.bin, points3D.txt, a Metashape .xml / .psx).
extern const std::vector<std::string> kOpenableExtensions;
// What the viewer will find in a recent model, asked of the file the way the
// viewer asks it. Runs on the recent list's probe thread.
ModelContent classify_recent_model(const std::string& path);

// HomeScreen.cpp

// What an entry is called. A file keeps its folder in front, as the viewer's
// panes do: "splat.ply" alone would name every checkpoint there is.
std::string recent_name(const RecentItem& it);
const spirula::i18n::Msg& recent_badge(const RecentItem& it);
// What the viewer's "add a model" menu offers: models and finished runs.
std::vector<std::string> recent_models(const RecentList& r);

// MeshScreen.cpp

// Can this format carry this color? The child's own answer, asked through the
// same function it refuses the run with.
bool mesh_format_carries(int format, int color);

// PresetPickers.cpp

// "General purpose (3dgs)": the translated label plus the name the command
// line uses, which is what ties a row to `spirula train <preset>`.
std::string preset_label(const std::string& name);
std::string preset_help(const std::string& name);
// A preset row's hover help: what it is for and, dimmed, the file it came
// from -- the path is what tells two presets with one name apart.
void preset_hover(const std::string& description, const std::string& path);
void preset_hover(const TrainPreset& p);

// One built-in row of a preset combo: the name the file and the command line
// spell, the translated label beside it, and the sentence it hovers to.
struct BuiltinRow {
    std::string name, label, help;
};
std::vector<BuiltinRow> dataset_builtin_rows();

// What a preset combo came back with. Nothing moved unless `moved`; a
// built-in is `builtin`, a saved file is `index`, and `from_file` is the row
// that opens a file dialog.
struct PresetChoice {
    bool moved = false;
    bool from_file = false;
    std::string builtin;
    int index = -1;
};

// The combo every preset picker draws: the built-ins of that kind (or one
// stock row for a kind with none), the saved files, then a file elsewhere.
// Instantiated for DatasetPreset and MeshPreset.
template <class T>
PresetChoice preset_combo(const char* id, const PresetPicker<T>& pick,
                          const std::string& file, const std::string& builtin,
                          const std::vector<BuiltinRow>& builtins,
                          bool offer_file = false);

}  // namespace gui
