#pragma once

#include "data/LabelField.h"
#include "data/SceneTransform.h"

#include <string>

namespace spirula {

struct RegionCameras {
    LabelField field;
    SceneTransform train_from_dataset;
};

bool valid_region_camera_hash(const std::string& hash);
std::string save_region_cameras(const std::string& directory, const RegionCameras& cameras);
RegionCameras read_region_cameras(const std::string& ply, const std::string& hash);

}  // namespace spirula
