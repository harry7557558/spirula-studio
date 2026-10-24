#pragma once

// Out-of-core hierarchical streaming and cache manager:
// Manages a VRAM/RAM budget for Gaussian chunks, prioritizes asynchronous
// loading of visible and prefetch nodes, provides fallback to coarse
// summary representations when fine nodes are pending, and implements
// LRU eviction with thrashing protection.

#include "lod/HierarchyFile.h"
#include "lod/HierarchyNode.h"
#include "lod/HierarchyTraversal.h"
#include "lod/LodConfig.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace spirula {

enum class ChunkState : uint8_t {
    Unloaded = 0,
    Loading,
    Resident,
    Failed
};

// Cached payload for a single leaf chunk.
struct CachedChunk {
    int32_t node_index = -1;
    int32_t chunk_index = -1;
    int64_t splat_count = 0;
    uint64_t last_used_frame = 0;

    // Attributes
    std::vector<float> means;        // [N, 3]
    std::vector<float> quats;        // [N, 4]
    std::vector<float> scales;       // [N, 3]
    std::vector<float> opacities;    // [N, 1]
    std::vector<float> dc;           // [N, 3]
    std::vector<float> sh;           // [N, coeffs*3]

    int64_t byte_size() const;
};

// Priority request for chunk loading.
struct ChunkLoadRequest {
    int32_t chunk_index;
    int32_t node_index;
    float   priority;               // Higher value = higher priority
    uint64_t requested_frame;

    bool operator<(const ChunkLoadRequest& o) const {
        return priority < o.priority;
    }
};

class HierarchyCache {
public:
    explicit HierarchyCache(std::string slod_path, int64_t vram_budget_mb = 0);
    ~HierarchyCache();

    HierarchyCache(const HierarchyCache&) = delete;
    HierarchyCache& operator=(const HierarchyCache&) = delete;

    // Get the loaded hierarchy metadata (nodes and summary Gaussians).
    const GaussianHierarchy& hierarchy() const { return _hier; }
    GaussianHierarchy& hierarchy() { return _hier; }

    // Start background streaming worker thread.
    void start_worker();

    // Stop background worker thread and wait for completion.
    void stop_worker();

    // Check if a chunk is resident.
    bool is_chunk_resident(int32_t chunk_index) const;

    // Check if a node's leaf data is resident.
    bool is_node_resident(int32_t node_index) const;

    // Query state of a chunk.
    ChunkState chunk_state(int32_t chunk_index) const;

    // Resolve cut: for any non-resident leaf node in the cut, fallback to its
    // nearest loaded ancestor, and enqueue priority load requests for the missing
    // leaves and prefetch candidates.
    LodCutResult resolve_cut_with_fallback(
        const LodCutResult& desired_cut,
        uint64_t current_frame,
        int prefetch_depth = 1);

    // Flatten resolved cut into render buffers using resident chunks.
    void extract_render_splats(
        const LodCutResult& cut,
        std::vector<float>& out_means,
        std::vector<float>& out_quats,
        std::vector<float>& out_scales,
        std::vector<float>& out_opacities,
        std::vector<float>& out_dc,
        std::vector<float>& out_sh);

    // Evict least-recently-used resident chunks if over budget.
    // Safe to call from render thread.
    void evict_if_over_budget(uint64_t current_frame);

    // Statistics
    int64_t resident_splat_count() const { return _resident_splat_count.load(); }
    int64_t resident_bytes() const { return _resident_bytes.load(); }
    int64_t budget_bytes() const { return _budget_bytes; }
    int64_t num_resident_chunks() const;
    int64_t num_pending_requests() const;

    void set_budget_mb(int64_t mb);

private:
    std::string _path;
    GaussianHierarchy _hier;
    int64_t _budget_bytes = 0;

    std::vector<SlodChunkEntry> _chunk_table;
    std::vector<int32_t> _node_to_chunk;

    // Residency tracking
    mutable std::mutex _cache_mutex;
    std::unordered_map<int32_t, std::shared_ptr<CachedChunk>> _resident_chunks;
    std::vector<ChunkState> _chunk_states;

    std::atomic<int64_t> _resident_splat_count{0};
    std::atomic<int64_t> _resident_bytes{0};

    // Background worker
    std::atomic<bool> _stop_worker{false};
    std::thread _worker_thread;
    std::mutex _queue_mutex;
    std::condition_variable _cv_queue;
    std::vector<ChunkLoadRequest> _pending_requests;

    void worker_loop();
    void load_chunk_payload(int32_t chunk_index, CachedChunk& out);
};

}  // namespace spirula

