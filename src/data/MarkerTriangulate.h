// MarkerTriangulate.h -- 3D triangulation of marker observations from camera rays.
#pragma once

#include "data/DatasetParser.h"
#include "data/MarkerTypes.h"

#include <vector>

namespace spirula {

// Triangulate one marker across all its image observations.
bool triangulate_marker(int marker_id,
                        const std::vector<MarkerObservation>& obs,
                        const ParsedDataset& ds,
                        Marker3D& out, MarkerFamily family = MarkerFamily::Auto);

// Triangulate all distinct markers found in the observations.
std::vector<Marker3D> triangulate_markers(const std::vector<MarkerObservation>& obs,
                                          const ParsedDataset& ds);

// Direct 3D ray intersection for M camera rays (origin + unit direction).
bool intersect_rays(const double* origins, const double* dirs, int n_rays,
                    double out_point[3], double* out_residual = nullptr);

// Write/read triangulated markers to/from JSON.
bool save_markers_json(const std::string& path, const std::vector<Marker3D>& markers);
bool load_markers_json(const std::string& path, std::vector<Marker3D>& markers);

}  // namespace spirula

