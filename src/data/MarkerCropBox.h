// MarkerCropBox.h -- automatic bounding box (ROI) generation from markers.
#pragma once

#include "data/MarkerTypes.h"
#include "data/RoiDocument.h"

#include <string>
#include <vector>

namespace spirula {

struct MarkerCropOptions {
    double pad_xy = 0.10;   // metres of padding around markers in X and Y
    double z_min = -0.05;   // metres below the marker plane (Z=0)
    double z_max = 1.00;    // metres above the marker plane
    bool explicit_bounds = false;
    double bounds[6]{0.0};  // xmin, xmax, ymin, ymax, zmin, zmax
};

// Compute a BoxRegion enclosing the markers with the given height and padding.
RoiDocument compute_marker_crop_box(const std::vector<Marker3D>& aligned_markers,
                                    const MarkerCropOptions& opt);

// Write the crop box to <dataset_dir>/roi/<roi_name>.json.
bool save_marker_crop_roi(const std::string& dataset_dir,
                          const RoiDocument& doc,
                          const std::string& roi_name,
                          std::string& out_path,
                          std::string& error);

}  // namespace spirula

