#pragma once

// A saved set of the built-in reconstruction's matching and mapping settings:
// the quality level, the frontend, and what the advanced editor changed. What
// a capture is -- its inputs, lenses, capture type -- stays out, so one preset
// reconstructs any dataset the same way.

#include "app/gui/PresetFile.h"

#include <map>
#include <string>
#include <vector>

namespace gui {

struct SfmJob;

struct SfmPreset {
    std::string name;
    std::string description;
    std::string path;
    int quality = 2;
    int features = 0;
    int matcher = 0;
    int max_features = 0;
    int max_image_size = 0;
    std::map<std::string, std::string> options;
};

SfmPreset capture_sfm_preset(const SfmJob& job);
// Onto `job`, leaving every other setting of it as it was.
void apply_sfm_preset(const SfmPreset& p, SfmJob& job);

// Throws std::runtime_error when the file cannot be written / read.
void save_sfm_preset(const SfmPreset& p, const std::string& path);
SfmPreset load_sfm_preset(const std::string& path);
std::vector<SfmPreset> list_sfm_presets();

}  // namespace gui
