#pragma once

// A region's boundary drawn over a view: a translucent fill that fades where
// the scene is nearer, and a dashed outline along the silhouette and the open
// edges. RenderWorker rasterizes it on the host against the render's depth;
// the OpenGL preview draws the same triangles and outline itself.

#include "data/RegionMesh.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace spirula {

struct RegionOverlay {
    struct Layer {
        std::vector<float> xyz;        // [V,3]
        std::vector<uint32_t> tri;     // [T,3]
        std::vector<uint32_t> edge;    // [E,2] vertex pairs
        std::vector<int32_t> face;     // [E,2] adjacent triangles, -1 for none
        float rgb[3] = {1.0f, 0.55f, 0.1f};
    };
    std::vector<Layer> layers;
    // When set, pixels whose surface lies outside it are greyed: the region's
    // own frame is the overlay's plus `shift`.
    std::shared_ptr<const Region> region;
    double shift[3] = {0, 0, 0};
    bool empty() const { return layers.empty() && !region; }

    // `to_frame` maps the mesh into the overlay's frame, row-major 3x4.
    void add(const RegionMesh& m, const float rgb[3], const float to_frame[12] = nullptr);
};

// Outline segments [S,2,3] of a layer seen from `eye`: edges between a face
// turned toward it and one turned away, and edges with one face.
void region_outline(const RegionOverlay::Layer& l, const float eye[3], std::vector<float>& out);

// Over an [H,W,3] image: `ray_depth` is the distance along each pixel's ray
// to the scene, <= 0 or infinite for none; `c2w` is row-major 3x4 in the
// OpenGL basis; a pinhole camera.
void draw_region_overlay(const RegionOverlay& ov, uint8_t* rgb, int W, int H, const float* ray_depth,
                         const float c2w[12], float fx, float fy, float cx, float cy);

}  // namespace spirula
