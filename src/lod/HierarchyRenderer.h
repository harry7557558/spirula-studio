#pragma once

// Connects runtime LOD selection and streaming directly to the Spirula Studio
// rendering engine (set_data_3dgs / engine_scene_set_data_3dgs).
// Provides per-frame traversal, memory budgeting, and debug visualization.

#include "lod/HierarchyCache.h"
#include "lod/HierarchyNode.h"
#include "lod/HierarchyTraversal.h"
#include "lod/LodConfig.h"

#include <chrono>
#include <memory>
#include <vector>

namespace spirula {

struct LodFrameStats {
    int64_t total_rendered_splats = 0;
    int64_t num_leaf_nodes_selected = 0;
    int64_t num_summary_nodes_selected = 0;
    int64_t num_nodes_culled = 0;
    int64_t num_nodes_traversed = 0;
    float   max_screen_error = 0.0f;
    int64_t resident_splats = 0;
    float   resident_mb = 0.0f;
    double  traversal_time_ms = 0.0;
    double  upload_time_ms = 0.0;
    int64_t upload_bytes = 0;          // actual bytes uploaded to GPU this frame
    bool    gpu_buffers_updated = false; // true if new buffers were uploaded to GPU
};

class HierarchyRenderer {
public:
    // In-memory mode (using pre-built or loaded GaussianHierarchy)
    explicit HierarchyRenderer(std::shared_ptr<GaussianHierarchy> hier);

    // Streaming mode (using HierarchyCache reading from .slod file)
    explicit HierarchyRenderer(std::shared_ptr<HierarchyCache> cache);

    ~HierarchyRenderer() = default;

    // Evaluate LOD cut for current camera, upload to engine, and return stats.
    // If scene_slot >= 0, uses engine_scene_set_data_3dgs(scene_slot) + activate;
    // if scene_slot < 0, uses global set_data_3dgs.
    LodFrameStats update_and_upload(
        const LodViewParams& view,
        const LodRenderConfig& cfg,
        int scene_slot = -1);

    // Reset temporal state (e.g. on teleport or camera cut)
    void reset_history();

    // Query underlying hierarchy
    const GaussianHierarchy* hierarchy() const;

    // Query cache (nullptr in pure in-memory mode)
    HierarchyCache* cache() { return _cache.get(); }
    const HierarchyCache* cache() const { return _cache.get(); }

private:
    std::shared_ptr<GaussianHierarchy> _hier;
    std::shared_ptr<HierarchyCache> _cache;
    HierarchyTraversal _traversal;
    uint64_t _frame_counter = 0;

    // Active GPU upload cache tracking
    std::vector<SelectedLodItem> _last_uploaded_items;
    bool _last_debug_colorized = false;
    bool _has_uploaded = false;

    // Reusable upload buffers to prevent per-frame reallocations
    std::vector<float> _upload_means;
    std::vector<float> _upload_quats;
    std::vector<float> _upload_scales;
    std::vector<float> _upload_opacities;
    std::vector<float> _upload_dc;
    std::vector<float> _upload_sh;
};

// Generate distinct color per hierarchy depth (for debug visualization)
void lod_level_color(int depth, float rgb[3]);

}  // namespace spirula

