#pragma once

// Where a LiDAR scan's own outputs sit in a dataset. Only the scan writers
// put files there, so a path under lidar/ is proof that data came from the
// scan and not from photographs or a depth model.

namespace lidar_layout {

inline constexpr const char* kPoints  = "lidar/points.ply";
inline constexpr const char* kDepths  = "lidar/depths";
inline constexpr const char* kNormals = "lidar/normals";
// One word, `ray` or `axial`: the distance the maps in kDepths hold.
inline constexpr const char* kDepthKind = "lidar/depths/depth_kind.txt";

}  // namespace lidar_layout
