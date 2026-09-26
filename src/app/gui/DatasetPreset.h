#pragma once

// A saved dataset-creation preset: everything the New Dataset screen decides
// about HOW a dataset is built, under a name the user chose.
//
// What it never carries is WHERE. The videos and photo folders, the output
// folder, the clicks drawn on one capture's frames and the border fitted to
// one lens all describe a particular capture; a preset that moved them would
// be unusable on the next one. The field table in the .cpp is what says which
// side of that line each setting is on.

#include "app/gui/ColmapRunner.h"
#include "app/gui/MaskSettings.h"
#include "app/gui/PresetFile.h"
#include "app/gui/SfmRunner.h"

#include <string>
#include <vector>

namespace gui {

// The screen's settings, in the structs that run them. Only the fields the
// preset table names are read or written; the rest are the run's own context.
struct DatasetSettings {
    bool colmap_engine = false;
    SfmJob sfm;
    ColmapJob colmap;
    MaskSettings mask;               // clicks excluded -- see the header note
    std::string mask_model_id = "sam2.1-base-plus";
    std::string mask_detector_id = "gdino-tiny";   // a TextDetector, ModelCache.h
    bool use_found_masks = true;
    bool border_enable = false;
    // A saved stencil's name (StencilPreset.h), drawn on every input.
    std::string frame_shapes;
};

struct DatasetPreset {
    std::string name;
    std::string description;
    std::string path;
    DatasetSettings s;
};

// The built-in presets, by the name the picker and a batch row spell.
// "general" applies nothing, which is what makes it the row a capture starts
// on. Their labels are in i18n/catalog/Dataset.h (see config/TrainConfig.h).
struct DatasetPresetInfo { const char* name; };
inline constexpr DatasetPresetInfo kDatasetPresets[] = {
    {"general"},
    {"360-camera"},
    {"internet-photos"},
};
inline constexpr int kNumDatasetPresets =
    (int)(sizeof(kDatasetPresets) / sizeof(kDatasetPresets[0]));

bool is_dataset_preset_name(const std::string& name);

// Apply one over whatever `s` already holds. False for an unknown name, which
// leaves `s` alone.
bool dataset_apply_preset(DatasetSettings& s, const std::string& name);

// Throws std::runtime_error when the file cannot be written / read.
void save_dataset_preset(const DatasetPreset& p, const std::string& path);
DatasetPreset load_dataset_preset(const std::string& path);
void delete_dataset_preset(const std::string& path);
std::vector<DatasetPreset> list_dataset_presets();

// Every bounded field back inside its range. A preset file is text a user can
// edit and a batch can run unattended for a weekend, so a value nothing
// checked must not reach a runner.
void sanitize_dataset_settings(DatasetSettings& s);

}  // namespace gui
