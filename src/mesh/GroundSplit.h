#pragma once

// A finished mesh split into ground and the rest by per-camera ground masks,
// voted on at its vertices (docs/notes/ground-split.md). Host only.

#include "mesh/MeshExport.h"
#include "mesh/Tsdf.h"

#include <cstdint>
#include <vector>

namespace meshing {

// One pixel of a camera's ground labels.
inline constexpr uint8_t kGroundNo = 0, kGroundYes = 1, kGroundUnknown = 255;

// Adds one camera's votes to `ground` and `seen` [V]: a vertex votes when its
// ray distance is within the band of the camera's depth at its pixel. `depth`
// and `labels` are [height * width], depth as moments_to_depth gives it.
void vote_ground(const TsdfCamera& cam, const float* depth, const uint8_t* labels,
                 const MeshData& mesh, float* ground, float* seen);

// Per-face labels, 1 = ground: vertex ground shares smoothed over the mesh,
// then connected islands under `min_island` faces join what surrounds them.
std::vector<uint8_t> ground_faces(const MeshData& mesh, const std::vector<float>& ground,
                                  const std::vector<float>& seen, int min_island);

// The faces labelled `want`, vertices compacted; the texture is copied whole.
MeshData mesh_subset(const MeshData& mesh, const std::vector<uint8_t>& face_label,
                     uint8_t want);

} // namespace meshing
