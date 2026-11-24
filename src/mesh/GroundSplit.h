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

// SAM misses floor far more often than it paints something else as floor: on a
// 3.6M-face indoor 360 capture, 0.3 covered 89.8% of the floor against 80.7% at
// 0.5, with 0.66% of the ground off the floor plane (docs/notes/ground-split.md).
inline constexpr float kGroundShare = 0.3f;
inline constexpr int kGroundIsland = 200;

// Adds one camera's votes to `ground` and `seen` [V]: a vertex votes when its
// ray distance is within the band of the camera's depth at its pixel. `depth`
// and `labels` are [height * width], depth as moments_to_depth gives it.
void vote_ground(const TsdfCamera& cam, const float* depth, const uint8_t* labels,
                 const MeshData& mesh, float* ground, float* seen);

// Per-face labels, 1 = ground where the smoothed ground share reaches `min_share`;
// connected islands under `min_island` faces then join what surrounds them.
std::vector<uint8_t> ground_faces(const MeshData& mesh, const std::vector<float>& ground,
                                  const std::vector<float>& seen,
                                  float min_share = kGroundShare,
                                  int min_island = kGroundIsland);

// The faces labelled `want`, vertices compacted; the texture is copied whole.
MeshData mesh_subset(const MeshData& mesh, const std::vector<uint8_t>& face_label,
                     uint8_t want);

} // namespace meshing
