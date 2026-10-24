#pragma once

// Hierarchy node for the LOD tree.

#include <cstdint>
#include <cmath>
#include <vector>

namespace spirula {

struct Aabb3 {
    float lo[3] = {1e30f, 1e30f, 1e30f};
    float hi[3] = {-1e30f, -1e30f, -1e30f};

    void expand(float x, float y, float z) {
        lo[0] = std::min(lo[0], x); lo[1] = std::min(lo[1], y); lo[2] = std::min(lo[2], z);
        hi[0] = std::max(hi[0], x); hi[1] = std::max(hi[1], y); hi[2] = std::max(hi[2], z);
    }
    void expand(const Aabb3& o) {
        for (int i = 0; i < 3; i++) { lo[i] = std::min(lo[i], o.lo[i]); hi[i] = std::max(hi[i], o.hi[i]); }
    }
    float center(int axis) const { return 0.5f * (lo[axis] + hi[axis]); }
    float extent(int axis) const { return hi[axis] - lo[axis]; }
    float diagonal() const {
        float d = 0;
        for (int i = 0; i < 3; i++) { float e = extent(i); d += e * e; }
        return std::sqrt(d);
    }
    bool valid() const { return lo[0] <= hi[0]; }
    int widest_axis() const {
        int a = 0;
        for (int i = 1; i < 3; i++) if (extent(i) > extent(a)) a = i;
        return a;
    }
};

// Summary Gaussian stored in parent nodes -- raw parameters matching SplatCloud.
struct SummaryGaussian {
    float mean[3];
    float quat[4];      // (w,x,y,z)
    float scale[3];      // log scale
    float opacity;       // logit
    float dc[3];         // DC color (SH C0 coefficient)
};

struct HierarchyNode {
    Aabb3 bounds;
    float error_radius = 0.0f;  // world-space radius for screen-space error

    // Tree structure: -1 means none.
    int32_t parent = -1;
    int32_t child_left = -1;
    int32_t child_right = -1;
    int32_t depth = 0;

    // Leaf: range [splat_offset, splat_offset + splat_count) into sorted array.
    int64_t splat_offset = 0;
    int64_t splat_count = 0;

    // Internal: summary Gaussians approximating all descendants.
    std::vector<SummaryGaussian> summary;

    bool is_leaf() const { return child_left < 0; }
    int64_t total_descendant_splats = 0;  // recursive count
};

// The full hierarchy: nodes + the reordered original Gaussians.
struct GaussianHierarchy {
    std::vector<HierarchyNode> nodes;
    int32_t root = 0;
    int32_t max_depth = 0;

    // Reordered original Gaussians (leaf data), matching SplatCloud layout.
    int64_t num_splats = 0;
    int sh_degree = 0;
    int sb_lobes = 0;
    std::vector<float> means;        // [N, 3]
    std::vector<float> quats;        // [N, 4]
    std::vector<float> scales;       // [N, 3]
    std::vector<float> opacities;    // [N, 1]
    std::vector<float> features_dc;  // [N, 3]
    std::vector<float> features_sh;  // [N, K, 3]

    int64_t coeffs() const;
    int64_t total_summary_gaussians() const;
};

}  // namespace spirula
