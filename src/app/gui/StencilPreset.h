#pragma once

// Saved "drawn areas": a frame stencil's shapes as SVG (app/FrameMaskSvg.h)
// in <config>/presets/stencil, listed by <title>. A set drawn per camera is
// one file per camera sharing the title, each naming its camera. A dataset
// preset names one (DatasetSettings::frame_shapes), which is how a batch run
// gets them. The fitted fisheye border is never in one.

#include "app/FrameMask.h"

#include <string>
#include <utility>
#include <vector>

namespace gui {

struct StencilPreset {
    std::string name;
    std::string path;                   // its file, or one of a per-camera set's
    std::vector<std::string> cameras;   // a per-camera set's, sorted
};
// The name, and a per-camera set's cameras after it: "Rig [cam0, cam1]".
std::string stencil_preset_label(const StencilPreset& p);

std::string stencil_preset_dir();
std::vector<StencilPreset> list_stencil_presets();

// By the name the list shows, or by a path to any .svg, with its set.
bool load_stencil_preset(const std::string& name_or_path, app::MaskSet& out,
                         std::string& error);
// Replaces every file of a saved set of the same name. `path` is the first
// file written.
bool save_stencil_preset(const std::string& name, const app::MaskSet& set, std::string& path,
                         std::string& error);

// A run keeps each input's drawn shapes beside the dataset, so they outlive
// the session that drew them even if nobody saved them.
inline constexpr const char* kDatasetStencilDir = "frame_stencil";
// (input path, what it draws) per input: <input>.svg, or <input>-<camera>.svg
// for each camera of a per-camera set. The folder is rewritten, so it holds
// exactly what this run drew. Returns the files written; nothing drawn, none.
std::vector<std::string> save_dataset_stencils(
    const std::string& workspace, const std::vector<std::pair<std::string, app::MaskSet>>& inputs,
    std::string& error);
std::vector<StencilPreset> list_dataset_stencils(const std::string& workspace);

}  // namespace gui
