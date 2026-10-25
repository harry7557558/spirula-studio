#pragma once

// Runtime hierarchy traversal and LOD cut selection:
// Evaluates per-node screen-space error against a quality threshold with
// hysteresis, frustum-culls against pinhole or equirectangular cameras,
// computes smooth parent-child transition weights, and extracts the flat
// Gaussian buffer to render for the current frame.

#include "lod/HierarchyNode.h"
#include "lod/LodConfig.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace spirula {

enum class LodCameraModel : int {
    Pinhole = 0,
    Fisheye = 1,
    Equisolid = 2,
    Equirectangular = 3
};

// View parameters for traversal.
struct LodViewParams {
    float cam_pos[3] = {0, 0, 0};
    float cam_dir[3] = {0, 0, 1};       // forward vector
    float cam_up[3]  = {0, 1, 0};
    float cam_right[3] = {1, 0, 0};

    // Camera projection
    LodCameraModel camera_model = LodCameraModel::Pinhole;
    int   width  = 1920;
    int   height = 1080;
    float fx = 1000.0f;
    float fy = 1000.0f;
    float cx = 960.0f;
    float cy = 540.0f;

    // View-projection matrix (4x4 row-major) for pinhole frustum culling
    float view_proj[16] = {};

    // Near / far plane distances
    float z_near = 0.05f;
    float z_far  = 10000.0f;
};

// A selected item in the LOD cut: either a leaf range or an internal node's summary.
struct SelectedLodItem {
    int32_t node_index = -1;
    bool is_leaf = false;

    // Leaf: range into hierarchy's reordered splats
    int64_t splat_offset = 0;
    int64_t splat_count  = 0;

    // Internal: summary Gaussian index within node.summary (or -1 for all summaries)
    int32_t summary_index = -1;

    // Transition weight: 1.0 = fully visible, < 1.0 = fading in/out
    float opacity_multiplier = 1.0f;
};

// Result of a cut selection.
struct LodCutResult {
    std::vector<SelectedLodItem> items;
    int64_t total_rendered_splats = 0;
    int64_t num_leaf_nodes_selected = 0;
    int64_t num_summary_nodes_selected = 0;
    int64_t num_nodes_culled = 0;
    int64_t num_nodes_traversed = 0;
    float   max_screen_error = 0.0f;
};

// Persistent traversal state that maintains previous frame's cut for hysteresis
// and temporal anti-popping.
class HierarchyTraversal {
public:
    explicit HierarchyTraversal() = default;

    // Select the LOD cut for the given view parameters and configuration.
    LodCutResult select_cut(
        const GaussianHierarchy& hier,
        const LodViewParams& view,
        const LodRenderConfig& cfg);

    // Flatten the selected cut into a unified SplatCloud ready for rendering.
    static void extract_render_splats(
        const GaussianHierarchy& hier,
        const LodCutResult& cut,
        std::vector<float>& out_means,
        std::vector<float>& out_quats,
        std::vector<float>& out_scales,
        std::vector<float>& out_opacities,
        std::vector<float>& out_dc,
        std::vector<float>& out_sh);

    // Reset hysteresis history (e.g. on camera cut/teleport)
    void reset_history() {
        _prev_selected_flags.clear();
        _has_prev_selection = false;
    }

    bool is_node_previously_selected(int32_t nid) const {
        if (!_has_prev_selection || nid < 0 || (size_t)nid >= _prev_selected_flags.size()) return false;
        return _prev_selected_flags[(size_t)nid] != 0;
    }

private:
    std::vector<uint8_t> _prev_selected_flags;
    bool _has_prev_selection = false;
};

// Helper: tests if AABB is inside or intersects frustum with precomputed planes.
bool test_aabb_frustum_pinhole_planes(const Aabb3& box, const float planes[6][4], int num_planes);

// Helper: tests if AABB is inside or intersects frustum.
bool test_aabb_frustum_pinhole(const Aabb3& box, const float view_proj[16], bool check_far = false);

// Helper: tests if AABB intersects equirectangular view (conservative spherical check).
bool test_aabb_equirectangular(const Aabb3& box, const float cam_pos[3], float z_near, float z_far);

// Compute screen-space error in pixels for a node given camera parameters.
float compute_screen_error(const HierarchyNode& node, const LodViewParams& view);

}  // namespace spirula
