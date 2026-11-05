// MarkerAlign.h -- coordinate system alignment and metric scaling from markers.
#pragma once

#include "core/Similarity.h"
#include "data/MarkerTypes.h"

#include <array>
#include <string>
#include <vector>

namespace spirula {

// Fit a closed-form Sim(3) similarity taking points in `src` to `dst` via Umeyama SVD.
bool estimate_sim3_umeyama(const std::vector<std::array<double, 3>>& src,
                           const std::vector<std::array<double, 3>>& dst,
                           Sim3& out_sim);

// Derive the Sim(3) that places the scene into the frame defined by the markers.
bool solve_marker_system(const std::vector<Marker3D>& markers,
                         const MarkerSystemConfig& cfg,
                         Sim3& out_sim);

Marker3Def suggest_marker_system(const std::vector<Marker3D>& markers,
                                 const double up[3]);

// Apply Sim(3) to the dataset's sparse reconstruction on disk and update gauge.txt.
bool apply_marker_alignment(const std::string& dataset_dir,
                            const Sim3& T,
                            std::string& error);

// Apply Sim(3) to an existing trained Gaussian splat file.
bool apply_marker_alignment_splat(const std::string& in_splat_path,
                                  const std::string& out_splat_path,
                                  const Sim3& T,
                                  std::string& error);

}  // namespace spirula

