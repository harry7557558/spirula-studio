#pragma once

// Truncated signed distance fusion of per-camera depth maps, and its zero
// level as a triangle mesh: the extraction 2D Gaussian Splatting reports its
// meshes with (docs/notes/tsdf.md). Host only. Depth is RAY distance along
// unit rays, as the meshing render measures it, so every camera model fuses
// the same way.

#include "mesh/MeshExport.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace meshing {

struct TsdfCamera {
    float viewmat[16];         // world -> camera, row-major, CV axes
    float fx, fy, cx, cy;
    int width, height;
    int model, distortion;     // CameraModelType, CameraDistortionType
    float dist[8];
};

// Median depth per pixel from the meshing render's moments (opacity, c0, c1):
// where its occupancy min(opacity, 1 - exp(-(c0 + c1 z)^2)) reaches 1/2, the
// level the opacity-field extractor meshes at. 0 where it never does.
void moments_to_depth(const float* moments, size_t num_pixels, float* depth);

class TsdfVolume {
public:
    // `voxel` and `trunc` in world units.
    TsdfVolume(float voxel, float trunc);
    ~TsdfVolume();
    TsdfVolume(const TsdfVolume&) = delete;
    TsdfVolume& operator=(const TsdfVolume&) = delete;

    // Folds in one depth map [height * width]. A pixel at depth <= 0, or
    // beyond `max_depth` when that is > 0, says nothing.
    void integrate(const TsdfCamera& cam, const float* depth, float max_depth);

    // The zero level, by marching tetrahedra over the voxel grid (six per
    // cube, split along its main diagonal), faces wound toward the free side.
    // Only cells whose corners all have observations contribute.
    void extract(MeshData& mesh) const;

    size_t num_blocks() const;
    size_t bytes() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace meshing
