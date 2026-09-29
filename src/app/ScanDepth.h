#pragma once

// Depth and normal maps of a camera, rendered from a laser scan's points: the
// front-most point per pixel, then the far points that showed through gaps in
// a nearer surface removed and pinholes filled. Normals are taken from that
// depth with the engine's own stencil (shaders/pixel_wise.slang
// points_to_normal), so they mean what the trainer's depth normals mean.

#include <cstdint>
#include <vector>

namespace app {

// Points as float offsets from `origin`, sorted into cubic cells so a pinhole
// camera projects only the cells in front of it.
struct ScanCloud {
    struct Cell {
        uint32_t begin = 0, end = 0;
        float center[3] = {0, 0, 0};
        float radius = 0;
        float spacing = 0;   // between neighbouring points, as if on one surface
    };
    double origin[3] = {0, 0, 0};
    std::vector<float> xyz;      // [N*3]
    std::vector<uint8_t> rgb;    // [N*3]
    std::vector<Cell> cells;
    size_t size() const { return xyz.size() / 3; }
};

ScanCloud build_scan_cloud(const std::vector<double>& xyz, const std::vector<uint8_t>& rgb,
                           double cell_edge);

// A camera at the map's own resolution: OpenCV-axis camera-to-world, row-major
// 3x4, pixel centres at +0.5, `model` a CameraModelType without distortion.
struct MapCamera {
    int model = 0;
    double c2w[12] = {};
    double fx = 0, fy = 0, cx = 0, cy = 0;
    int width = 0, height = 0;
};

// The front-most point per pixel: its index (UINT32_MAX where none) and its
// distance (along the ray when `ray_depth`, else the optical axis). Points
// cover their share of the surface, so a sparse one still hides what is behind.
void render_front(const ScanCloud& cloud, const MapCamera& cam, bool ray_depth,
                  std::vector<uint32_t>& index, std::vector<float>& depth);

// Metres, 0 where the scan says nothing; unit normals in the camera's frame,
// facing it, all zero where none.
void render_depth_normal(const ScanCloud& cloud, const MapCamera& cam, bool ray_depth,
                         std::vector<float>& depth, std::vector<float>& normal);

}  // namespace app
