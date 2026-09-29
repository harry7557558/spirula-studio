#pragma once

// The boundary of a Region as triangles, for drawing it: surface nets over the
// inside test on a cubic grid filling `box`, each crossing bisected onto the
// true boundary. The mesh stays open where the region leaves the box, so an
// unbounded part shows only the faces it shares with its neighbours.

#include "data/Region.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace spirula {

struct LabelField;

struct RegionMesh {
    std::vector<float> xyz;      // [V,3]
    std::vector<uint32_t> tri;   // [T,3], wound outward
    bool empty() const { return tri.empty(); }
};

RegionMesh region_boundary_mesh(const Region& r, const Aabb& box, int cells_long_axis = 96);
// One mesh per label of the field, from one pass of queries over the grid;
// {} once `cancel` is set.
std::vector<RegionMesh> label_boundary_meshes(const LabelField& f, const Aabb& box,
                                              int cells_long_axis = 96,
                                              const std::atomic<bool>* cancel = nullptr);

// 1st..99th percentile of the points per axis, padded by a tenth: a box one
// stray point far away cannot blow up.
Aabb robust_bounds(const float* xyz, int64_t n);

}  // namespace spirula
