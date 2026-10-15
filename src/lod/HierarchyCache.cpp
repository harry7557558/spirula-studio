// HierarchyCache.cpp -- out-of-core streaming and residency manager.

#include "lod/HierarchyCache.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace spirula {

namespace {

inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

inline float logit(float p) {
    p = std::clamp(p, 1e-6f, 1.0f - 1e-6f);
    return std::log(p / (1.0f - p));
}

int64_t splat_payload_floats(int sh_degree, int sb_lobes) {
    int64_t coeffs = 0;
    if (sh_degree > 0 || sb_lobes > 0) {
        int64_t sh = (int64_t)(sh_degree + 1) * (sh_degree + 1) - 1;
        coeffs = sh + sb_lobes * 3;
    }
    return 3 + 4 + 3 + 1 + 3 + coeffs * 3;
}

}  // namespace

int64_t CachedChunk::byte_size() const {
    int64_t bytes = sizeof(CachedChunk);
    bytes += (int64_t)means.capacity() * sizeof(float);
    bytes += (int64_t)quats.capacity() * sizeof(float);
    bytes += (int64_t)scales.capacity() * sizeof(float);
    bytes += (int64_t)opacities.capacity() * sizeof(float);
    bytes += (int64_t)dc.capacity() * sizeof(float);
    bytes += (int64_t)sh.capacity() * sizeof(float);
    return bytes;
}

HierarchyCache::HierarchyCache(std::string slod_path, int64_t vram_budget_mb)
    : _path(std::move(slod_path))
{
    set_budget_mb(vram_budget_mb);

    // Load hierarchy topology and summaries (metadata only, no leaves yet)
    _hier = read_slod(_path, /*metadata_only=*/true);

    // Read chunk table from the file
    std::ifstream in(_path, std::ios::binary);
    if (!in) throw std::runtime_error("HierarchyCache: cannot open " + _path);

    SlodFileHeader hdr;
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in || hdr.magic != kSlodMagic)
        throw std::runtime_error("HierarchyCache: invalid file header in " + _path);

    _chunk_table.resize((size_t)hdr.num_chunks);
    in.seekg(hdr.chunk_table_offset);
    in.read(reinterpret_cast<char*>(_chunk_table.data()),
            (std::streamsize)(_chunk_table.size() * sizeof(SlodChunkEntry)));

    _chunk_states.assign((size_t)hdr.num_chunks, ChunkState::Unloaded);

    // Build node to chunk mapping
    _node_to_chunk.assign(_hier.nodes.size(), -1);
    for (size_t i = 0; i < _chunk_table.size(); i++) {
        int32_t nid = _chunk_table[i].node_index;
        if (nid >= 0 && nid < (int32_t)_node_to_chunk.size()) {
            _node_to_chunk[(size_t)nid] = (int32_t)i;
        }
    }
}

HierarchyCache::~HierarchyCache() {
    stop_worker();
}

void HierarchyCache::set_budget_mb(int64_t mb) {
    if (mb <= 0) {
        _budget_bytes = 0; // unlimited
    } else {
        _budget_bytes = mb * 1024 * 1024;
    }
}

void HierarchyCache::start_worker() {
    if (_worker_thread.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        _stop_worker = false;
    }
    _worker_thread = std::thread(&HierarchyCache::worker_loop, this);
}

void HierarchyCache::stop_worker() {
    {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        _stop_worker = true;
    }
    _cv_queue.notify_all();
    if (_worker_thread.joinable()) {
        _worker_thread.join();
    }
}

bool HierarchyCache::is_chunk_resident(int32_t chunk_index) const {
    std::lock_guard<std::mutex> lock(_cache_mutex);
    if (chunk_index < 0 || chunk_index >= (int32_t)_chunk_states.size()) return false;
    return _chunk_states[(size_t)chunk_index] == ChunkState::Resident;
}

bool HierarchyCache::is_node_resident(int32_t node_index) const {
    if (node_index < 0 || node_index >= (int32_t)_node_to_chunk.size()) return false;
    int32_t ci = _node_to_chunk[(size_t)node_index];
    if (ci < 0) return true; // non-leaf nodes have summary Gaussians which are always resident
    return is_chunk_resident(ci);
}

ChunkState HierarchyCache::chunk_state(int32_t chunk_index) const {
    std::lock_guard<std::mutex> lock(_cache_mutex);
    if (chunk_index < 0 || chunk_index >= (int32_t)_chunk_states.size()) return ChunkState::Failed;
    return _chunk_states[(size_t)chunk_index];
}

int64_t HierarchyCache::num_resident_chunks() const {
    std::lock_guard<std::mutex> lock(_cache_mutex);
    return (int64_t)_resident_chunks.size();
}

int64_t HierarchyCache::num_pending_requests() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(_queue_mutex));
    return (int64_t)_pending_requests.size();
}

void HierarchyCache::load_chunk_payload(int32_t chunk_index, CachedChunk& out) {
    if (chunk_index < 0 || chunk_index >= (int32_t)_chunk_table.size()) return;
    const auto& ce = _chunk_table[(size_t)chunk_index];

    std::ifstream in(_path, std::ios::binary);
    if (!in) return;

    in.seekg(ce.file_offset);
    const int64_t count = ce.splat_count;
    const int64_t coeffs = _hier.coeffs();
    const int64_t floats_per = splat_payload_floats(_hier.sh_degree, _hier.sb_lobes);

    out.node_index = ce.node_index;
    out.chunk_index = chunk_index;
    out.splat_count = count;

    out.means.resize((size_t)(count * 3));
    out.quats.resize((size_t)(count * 4));
    out.scales.resize((size_t)(count * 3));
    out.opacities.resize((size_t)count);
    out.dc.resize((size_t)(count * 3));
    if (coeffs > 0) {
        out.sh.resize((size_t)(count * coeffs * 3));
    }

    std::vector<float> row((size_t)floats_per);
    for (int64_t s = 0; s < count; s++) {
        in.read(reinterpret_cast<char*>(row.data()),
                (std::streamsize)(row.size() * sizeof(float)));
        const float* p = row.data();
        std::copy_n(p, 3, &out.means[(size_t)s * 3]); p += 3;
        std::copy_n(p, 4, &out.quats[(size_t)s * 4]); p += 4;
        std::copy_n(p, 3, &out.scales[(size_t)s * 3]); p += 3;
        out.opacities[(size_t)s] = *p++;
        std::copy_n(p, 3, &out.dc[(size_t)s * 3]); p += 3;
        if (coeffs > 0) {
            std::copy_n(p, coeffs * 3, &out.sh[(size_t)s * coeffs * 3]);
        }
    }
}

void HierarchyCache::worker_loop() {
    while (!_stop_worker) {
        ChunkLoadRequest req{-1, -1, 0.0f, 0};
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            _cv_queue.wait(lock, [&]() {
                return _stop_worker || !_pending_requests.empty();
            });
            if (_stop_worker) break;

            // Pick highest priority request
            auto best_it = std::max_element(_pending_requests.begin(), _pending_requests.end());
            req = *best_it;
            _pending_requests.erase(best_it);
        }

        if (req.chunk_index < 0) continue;

        {
            std::lock_guard<std::mutex> lock(_cache_mutex);
            if (_chunk_states[(size_t)req.chunk_index] == ChunkState::Resident) {
                continue;
            }
        }

        auto chunk = std::make_shared<CachedChunk>();
        chunk->last_used_frame = req.requested_frame;
        load_chunk_payload(req.chunk_index, *chunk);

        int64_t added_bytes = chunk->byte_size();
        int64_t added_splats = chunk->splat_count;
        {
            std::lock_guard<std::mutex> lock(_cache_mutex);
            _resident_chunks[req.chunk_index] = chunk;
            _chunk_states[(size_t)req.chunk_index] = ChunkState::Resident;
        }

        _resident_bytes.fetch_add(added_bytes);
        _resident_splat_count.fetch_add(added_splats);
    }
}

LodCutResult HierarchyCache::resolve_cut_with_fallback(
    const LodCutResult& desired_cut,
    uint64_t current_frame,
    int prefetch_depth)
{
    LodCutResult final_cut;
    final_cut.max_screen_error = desired_cut.max_screen_error;
    final_cut.num_nodes_culled = desired_cut.num_nodes_culled;
    final_cut.num_nodes_traversed = desired_cut.num_nodes_traversed;

    std::vector<ChunkLoadRequest> new_requests;
    std::unordered_set<int32_t> added_fallback_ancestors;

    for (const auto& item : desired_cut.items) {
        if (!item.is_leaf) {
            // Internal summary Gaussians are always resident
            final_cut.items.push_back(item);
            final_cut.total_rendered_splats++;
            final_cut.num_summary_nodes_selected++;
            continue;
        }

        int32_t nid = item.node_index;
        int32_t ci = (nid >= 0 && nid < (int32_t)_node_to_chunk.size()) ?
                     _node_to_chunk[(size_t)nid] : -1;

        bool resident = false;
        if (ci >= 0) {
            std::lock_guard<std::mutex> lock(_cache_mutex);
            if (_chunk_states[(size_t)ci] == ChunkState::Resident) {
                resident = true;
                auto it = _resident_chunks.find(ci);
                if (it != _resident_chunks.end() && it->second) {
                    it->second->last_used_frame = current_frame;
                }
            }
        }

        if (resident) {
            // Leaf is ready! Render original trained Gaussians
            final_cut.items.push_back(item);
            final_cut.total_rendered_splats += item.splat_count;
            final_cut.num_leaf_nodes_selected++;
        } else {
            // Chunk is missing! Request it with high priority
            if (ci >= 0) {
                std::lock_guard<std::mutex> lock(_cache_mutex);
                if (_chunk_states[(size_t)ci] == ChunkState::Unloaded) {
                    _chunk_states[(size_t)ci] = ChunkState::Loading;
                    new_requests.push_back({ci, nid, 1000.0f, current_frame});
                }
            }

            // Fallback: search upward for the nearest ancestor with summary Gaussians
            int32_t ancestor = (nid >= 0) ? _hier.nodes[(size_t)nid].parent : -1;
            while (ancestor >= 0 && _hier.nodes[(size_t)ancestor].summary.empty()) {
                ancestor = _hier.nodes[(size_t)ancestor].parent;
            }

            if (ancestor >= 0 && !_hier.nodes[(size_t)ancestor].summary.empty()) {
                if (added_fallback_ancestors.insert(ancestor).second) {
                    const auto& anc_nd = _hier.nodes[(size_t)ancestor];
                    for (int32_t s = 0; s < (int32_t)anc_nd.summary.size(); s++) {
                        SelectedLodItem fb_item;
                        fb_item.node_index = ancestor;
                        fb_item.is_leaf = false;
                        fb_item.summary_index = s;
                        fb_item.opacity_multiplier = item.opacity_multiplier;
                        final_cut.items.push_back(fb_item);
                        final_cut.total_rendered_splats++;
                        final_cut.num_summary_nodes_selected++;
                    }
                }
            } else {
                // If even root has no summary, keep item as stub
                final_cut.items.push_back(item);
                final_cut.total_rendered_splats += item.splat_count;
                final_cut.num_leaf_nodes_selected++;
            }
        }

        // Prefetch adjacent nodes (sibling or children of ancestor)
        if (prefetch_depth > 0 && nid >= 0) {
            int32_t parent = _hier.nodes[(size_t)nid].parent;
            if (parent >= 0) {
                int32_t sibling = (_hier.nodes[(size_t)parent].child_left == nid) ?
                                  _hier.nodes[(size_t)parent].child_right :
                                  _hier.nodes[(size_t)parent].child_left;
                if (sibling >= 0 && _hier.nodes[(size_t)sibling].is_leaf()) {
                    int32_t sib_ci = _node_to_chunk[(size_t)sibling];
                    if (sib_ci >= 0) {
                        std::lock_guard<std::mutex> lock(_cache_mutex);
                        if (_chunk_states[(size_t)sib_ci] == ChunkState::Unloaded) {
                            _chunk_states[(size_t)sib_ci] = ChunkState::Loading;
                            new_requests.push_back({sib_ci, sibling, 50.0f, current_frame});
                        }
                    }
                }
            }
        }
    }

    if (!new_requests.empty()) {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        _pending_requests.insert(_pending_requests.end(), new_requests.begin(), new_requests.end());
        _cv_queue.notify_one();
    }

    return final_cut;
}

void HierarchyCache::evict_if_over_budget(uint64_t current_frame) {
    if (_budget_bytes <= 0) return;
    if (_resident_bytes.load() <= _budget_bytes) return;

    std::vector<int32_t> evict_candidates;
    {
        std::lock_guard<std::mutex> lock(_cache_mutex);
        for (const auto& pair : _resident_chunks) {
            // Safeguard against thrashing: do not evict anything used recently (within 3 frames)
            if (pair.second && pair.second->last_used_frame + 3 < current_frame) {
                evict_candidates.push_back(pair.first);
            }
        }

        // Sort candidates by last_used_frame ascending (oldest first)
        std::sort(evict_candidates.begin(), evict_candidates.end(), [&](int32_t a, int32_t b) {
            return _resident_chunks[a]->last_used_frame < _resident_chunks[b]->last_used_frame;
        });

        for (int32_t ci : evict_candidates) {
            if (_resident_bytes.load() <= _budget_bytes) break;
            auto it = _resident_chunks.find(ci);
            if (it != _resident_chunks.end() && it->second) {
                int64_t freed_bytes = it->second->byte_size();
                int64_t freed_splats = it->second->splat_count;
                _resident_chunks.erase(it);
                _chunk_states[(size_t)ci] = ChunkState::Unloaded;
                _resident_bytes.fetch_sub(freed_bytes);
                _resident_splat_count.fetch_sub(freed_splats);
            }
        }
    }
}

void HierarchyCache::extract_render_splats(
    const LodCutResult& cut,
    std::vector<float>& out_means,
    std::vector<float>& out_quats,
    std::vector<float>& out_scales,
    std::vector<float>& out_opacities,
    std::vector<float>& out_dc,
    std::vector<float>& out_sh)
{
    const int64_t total = cut.total_rendered_splats;
    const int64_t coeffs = _hier.coeffs();

    out_means.resize((size_t)(total * 3));
    out_quats.resize((size_t)(total * 4));
    out_scales.resize((size_t)(total * 3));
    out_opacities.resize((size_t)total);
    out_dc.resize((size_t)(total * 3));
    if (coeffs > 0) {
        out_sh.assign((size_t)(total * coeffs * 3), 0.0f);
    } else {
        out_sh.clear();
    }

    std::lock_guard<std::mutex> lock(_cache_mutex);
    int64_t cursor = 0;

    for (const auto& item : cut.items) {
        if (item.is_leaf) {
            int32_t ci = (item.node_index >= 0 && item.node_index < (int32_t)_node_to_chunk.size()) ?
                         _node_to_chunk[(size_t)item.node_index] : -1;
            auto it = (ci >= 0) ? _resident_chunks.find(ci) : _resident_chunks.end();

            if (it != _resident_chunks.end() && it->second) {
                const auto& ch = *it->second;
                const int64_t count = ch.splat_count;
                const float op_mult = item.opacity_multiplier;

                for (int64_t s = 0; s < count; s++) {
                    const int64_t dst_idx = cursor + s;
                    for (int j = 0; j < 3; j++) {
                        out_means[(size_t)(dst_idx * 3 + j)] = ch.means[(size_t)(s * 3 + j)];
                        out_scales[(size_t)(dst_idx * 3 + j)] = ch.scales[(size_t)(s * 3 + j)];
                        out_dc[(size_t)(dst_idx * 3 + j)] = ch.dc[(size_t)(s * 3 + j)];
                    }
                    for (int j = 0; j < 4; j++) {
                        out_quats[(size_t)(dst_idx * 4 + j)] = ch.quats[(size_t)(s * 4 + j)];
                    }

                    float raw_op = ch.opacities[(size_t)s];
                    if (op_mult < 0.999f) {
                        float sig = sigmoid(raw_op) * op_mult;
                        out_opacities[(size_t)dst_idx] = logit(sig);
                    } else {
                        out_opacities[(size_t)dst_idx] = raw_op;
                    }

                    if (coeffs > 0 && !ch.sh.empty()) {
                        std::copy_n(&ch.sh[(size_t)(s * coeffs * 3)],
                                    coeffs * 3,
                                    &out_sh[(size_t)(dst_idx * coeffs * 3)]);
                    }
                }
                cursor += count;
            } else {
                // Leaf requested but not resident: emit transparent placeholder
                for (int64_t s = 0; s < item.splat_count; s++) {
                    const int64_t dst_idx = cursor + s;
                    for (int j = 0; j < 3; j++) {
                        out_means[(size_t)(dst_idx * 3 + j)] = 0.0f;
                        out_scales[(size_t)(dst_idx * 3 + j)] = -10.0f;
                        out_dc[(size_t)(dst_idx * 3 + j)] = 0.0f;
                    }
                    out_quats[(size_t)(dst_idx * 4 + 0)] = 1.0f;
                    out_quats[(size_t)(dst_idx * 4 + 1)] = 0.0f;
                    out_quats[(size_t)(dst_idx * 4 + 2)] = 0.0f;
                    out_quats[(size_t)(dst_idx * 4 + 3)] = 0.0f;
                    out_opacities[(size_t)dst_idx] = -100.0f;
                }
                cursor += item.splat_count;
            }
        } else {
            const auto& nd = _hier.nodes[(size_t)item.node_index];
            if (item.summary_index >= 0 && item.summary_index < (int32_t)nd.summary.size()) {
                const auto& sg = nd.summary[(size_t)item.summary_index];
                const int64_t dst_idx = cursor;

                for (int j = 0; j < 3; j++) {
                    out_means[(size_t)(dst_idx * 3 + j)] = sg.mean[j];
                    out_scales[(size_t)(dst_idx * 3 + j)] = sg.scale[j];
                    out_dc[(size_t)(dst_idx * 3 + j)] = sg.dc[j];
                }
                for (int j = 0; j < 4; j++) {
                    out_quats[(size_t)(dst_idx * 4 + j)] = sg.quat[j];
                }

                float sig = sigmoid(sg.opacity) * item.opacity_multiplier;
                out_opacities[(size_t)dst_idx] = logit(sig);

                cursor++;
            }
        }
    }
}

}  // namespace spirula

