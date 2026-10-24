// HierarchyRenderer.cpp -- engine upload and rendering integration.

#include "lod/HierarchyRenderer.h"
#include "engine/Engine.h"

#include <chrono>
#include <cmath>

namespace spirula {

namespace {

inline TorchTensorView make_view(const float* ptr, std::vector<int64_t> shape) {
    return {(uint64_t)(uintptr_t)ptr, (uint32_t)sizeof(float), std::move(shape)};
}

}  // namespace

void lod_level_color(int depth, float rgb[3]) {
    // Distinct rainbow palette for hierarchy levels:
    // Level 0 (coarse root) = Red, Level 1 = Orange, Level 2 = Yellow,
    // Level 3 = Green, Level 4 = Cyan, Level 5 = Blue, Level 6+ = Magenta
    static const float kPalette[][3] = {
        {1.0f, 0.2f, 0.2f}, // Red
        {1.0f, 0.5f, 0.1f}, // Orange
        {1.0f, 0.9f, 0.1f}, // Yellow
        {0.2f, 0.9f, 0.2f}, // Green
        {0.1f, 0.9f, 0.9f}, // Cyan
        {0.2f, 0.4f, 1.0f}, // Blue
        {0.8f, 0.2f, 0.9f}, // Magenta
        {0.9f, 0.6f, 0.9f}  // Pink
    };
    int idx = std::clamp(depth, 0, (int)(sizeof(kPalette) / sizeof(kPalette[0])) - 1);
    rgb[0] = kPalette[idx][0];
    rgb[1] = kPalette[idx][1];
    rgb[2] = kPalette[idx][2];
}

HierarchyRenderer::HierarchyRenderer(std::shared_ptr<GaussianHierarchy> hier)
    : _hier(std::move(hier))
{
}

HierarchyRenderer::HierarchyRenderer(std::shared_ptr<HierarchyCache> cache)
    : _cache(std::move(cache))
{
}

const GaussianHierarchy* HierarchyRenderer::hierarchy() const {
    if (_hier) return _hier.get();
    if (_cache) return &_cache->hierarchy();
    return nullptr;
}

void HierarchyRenderer::reset_history() {
    _traversal.reset_history();
    _last_uploaded_items.clear();
    _has_uploaded = false;
}

LodFrameStats HierarchyRenderer::update_and_upload(
    const LodViewParams& view,
    const LodRenderConfig& cfg,
    int scene_slot)
{
    LodFrameStats stats;
    const GaussianHierarchy* hier = hierarchy();
    if (!hier || hier->nodes.empty()) return stats;

    _frame_counter++;

    if (_cache && cfg.vram_budget_mb > 0) {
        if (_cache->budget_bytes() != cfg.vram_budget_mb * 1024 * 1024) {
            _cache->set_budget_mb(cfg.vram_budget_mb);
        }
    }

    auto t0 = std::chrono::steady_clock::now();

    LodCutResult desired_cut = _traversal.select_cut(*hier, view, cfg);

    LodCutResult final_cut;
    if (_cache) {
        final_cut = _cache->resolve_cut_with_fallback(
            desired_cut, _frame_counter, cfg.prefetch_depth);
    } else {
        final_cut = std::move(desired_cut);
    }

    const int64_t coeffs = hier->coeffs();
    const int sb_lobes = hier->sb_lobes;
    const int64_t bytes_per_splat = (int64_t)(12 + 16 + 12 + 4 + 12 + coeffs * 12);

    // Enforce hard VRAM budget envelope on real GPU allocations
    if (cfg.vram_budget_mb > 0) {
        const int64_t max_gpu_splats = (cfg.vram_budget_mb * 1024 * 1024) / bytes_per_splat;
        if (final_cut.total_rendered_splats > max_gpu_splats) {
            std::vector<size_t> leaf_indices;
            for (size_t i = 0; i < final_cut.items.size(); i++) {
                if (final_cut.items[i].is_leaf) leaf_indices.push_back(i);
            }
            std::sort(leaf_indices.begin(), leaf_indices.end(), [&](size_t a, size_t b) {
                int32_t na = final_cut.items[a].node_index;
                int32_t nb = final_cut.items[b].node_index;
                float ea = (na >= 0 && na < (int32_t)hier->nodes.size()) ?
                           hier->nodes[(size_t)na].error_radius : 0.0f;
                float eb = (nb >= 0 && nb < (int32_t)hier->nodes.size()) ?
                           hier->nodes[(size_t)nb].error_radius : 0.0f;
                return ea < eb;
            });

            for (size_t idx : leaf_indices) {
                if (final_cut.total_rendered_splats <= max_gpu_splats) break;
                auto& item = final_cut.items[idx];
                int32_t p = (item.node_index >= 0 && item.node_index < (int32_t)hier->nodes.size()) ?
                            hier->nodes[(size_t)item.node_index].parent : -1;
                if (p >= 0 && !hier->nodes[(size_t)p].summary.empty()) {
                    final_cut.total_rendered_splats -= (item.splat_count - 1);
                    item.is_leaf = false;
                    item.node_index = p;
                    item.summary_index = 0;
                    item.splat_count = 1;
                    final_cut.num_leaf_nodes_selected--;
                    final_cut.num_summary_nodes_selected++;
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    stats.traversal_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    const int64_t num_splats = final_cut.total_rendered_splats;

    // Check if GPU buffers need updating or if last upload is still valid
    bool cut_changed = false;
    if (!_has_uploaded ||
        _last_uploaded_items.size() != final_cut.items.size() ||
        _last_debug_colorized != cfg.debug_colorize_levels) {
        cut_changed = true;
    } else if (scene_slot >= 0 ? !engine_scene_loaded(scene_slot) : engine_get_cur_num_splats() != num_splats) {
        cut_changed = true;
    } else {
        for (size_t i = 0; i < final_cut.items.size(); i++) {
            const auto& a = final_cut.items[i];
            const auto& b = _last_uploaded_items[i];
            if (a.node_index != b.node_index ||
                a.is_leaf != b.is_leaf ||
                a.splat_offset != b.splat_offset ||
                a.splat_count != b.splat_count ||
                a.summary_index != b.summary_index ||
                std::abs(a.opacity_multiplier - b.opacity_multiplier) > 0.02f) {
                cut_changed = true;
                break;
            }
        }
    }

    if (cut_changed) {
        auto t2 = std::chrono::steady_clock::now();

        if (_cache) {
            _cache->extract_render_splats(
                final_cut,
                _upload_means, _upload_quats, _upload_scales,
                _upload_opacities, _upload_dc, _upload_sh);
            _cache->evict_if_over_budget(_frame_counter);
        } else {
            HierarchyTraversal::extract_render_splats(
                *hier, final_cut,
                _upload_means, _upload_quats, _upload_scales,
                _upload_opacities, _upload_dc, _upload_sh);
        }

        if (cfg.debug_colorize_levels && num_splats > 0) {
            int64_t cursor = 0;
            constexpr float inv_c0 = 1.0f / 0.28209479177387814f;

            for (const auto& item : final_cut.items) {
                int depth = 0;
                if (item.node_index >= 0 && item.node_index < (int32_t)hier->nodes.size()) {
                    depth = hier->nodes[(size_t)item.node_index].depth;
                }
                float rgb[3];
                lod_level_color(depth, rgb);

                float dc_r = (rgb[0] - 0.5f) * inv_c0;
                float dc_g = (rgb[1] - 0.5f) * inv_c0;
                float dc_b = (rgb[2] - 0.5f) * inv_c0;

                int64_t count = item.is_leaf ? item.splat_count : 1;
                for (int64_t s = 0; s < count; s++) {
                    int64_t idx = cursor + s;
                    _upload_dc[(size_t)(idx * 3 + 0)] = dc_r;
                    _upload_dc[(size_t)(idx * 3 + 1)] = dc_g;
                    _upload_dc[(size_t)(idx * 3 + 2)] = dc_b;

                    if (coeffs > 0) {
                        std::fill_n(&_upload_sh[(size_t)(idx * coeffs * 3)], coeffs * 3, 0.0f);
                    }
                }
                cursor += count;
            }
        }

        if (num_splats == 0) {
            _upload_means.assign(3, 0.0f);
            _upload_quats.assign({1.0f, 0.0f, 0.0f, 0.0f});
            _upload_scales.assign(3, -20.0f);
            _upload_opacities.assign(1, -20.0f);
            _upload_dc.assign(3, 0.0f);
            if (coeffs > 0) _upload_sh.assign((size_t)(coeffs * 3), 0.0f);

            TorchTensorView v_means   = make_view(_upload_means.data(),     {1, 3});
            TorchTensorView v_quats   = make_view(_upload_quats.data(),     {1, 4});
            TorchTensorView v_scales  = make_view(_upload_scales.data(),    {1, 3});
            TorchTensorView v_opac    = make_view(_upload_opacities.data(), {1, 1});
            TorchTensorView v_dc      = make_view(_upload_dc.data(),        {1, 3});
            TorchTensorView v_sh      = make_view(_upload_sh.data(),        {1, coeffs, 3});

            if (scene_slot >= 0) {
                engine_scene_set_data_3dgs(
                    scene_slot, 0,
                    v_means, v_quats, v_scales, v_opac, v_dc, v_sh, sb_lobes);
                engine_scene_activate(scene_slot);
            } else {
                set_data_3dgs(
                    0,
                    v_means, v_quats, v_scales, v_opac, v_dc, v_sh, sb_lobes);
            }
        } else {
            TorchTensorView v_means   = make_view(_upload_means.data(),     {num_splats, 3});
            TorchTensorView v_quats   = make_view(_upload_quats.data(),     {num_splats, 4});
            TorchTensorView v_scales  = make_view(_upload_scales.data(),    {num_splats, 3});
            TorchTensorView v_opac    = make_view(_upload_opacities.data(), {num_splats, 1});
            TorchTensorView v_dc      = make_view(_upload_dc.data(),        {num_splats, 3});
            TorchTensorView v_sh      = make_view(_upload_sh.data(),        {num_splats, coeffs, 3});

            if (scene_slot >= 0) {
                engine_scene_set_data_3dgs(
                    scene_slot, num_splats,
                    v_means, v_quats, v_scales, v_opac, v_dc, v_sh, sb_lobes);
                engine_scene_activate(scene_slot);
            } else {
                set_data_3dgs(
                    num_splats,
                    v_means, v_quats, v_scales, v_opac, v_dc, v_sh, sb_lobes);
            }
        }

        auto t3 = std::chrono::steady_clock::now();
        stats.upload_time_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
        stats.upload_bytes = num_splats * bytes_per_splat;
        stats.gpu_buffers_updated = true;

        _last_uploaded_items = final_cut.items;
        _last_debug_colorized = cfg.debug_colorize_levels;
        _has_uploaded = true;
    } else {
        stats.upload_time_ms = 0.0;
        stats.upload_bytes = 0;
        stats.gpu_buffers_updated = false;
    }

    stats.total_rendered_splats = final_cut.total_rendered_splats;
    stats.num_leaf_nodes_selected = final_cut.num_leaf_nodes_selected;
    stats.num_summary_nodes_selected = final_cut.num_summary_nodes_selected;
    stats.num_nodes_culled = final_cut.num_nodes_culled;
    stats.num_nodes_traversed = final_cut.num_nodes_traversed;
    stats.max_screen_error = final_cut.max_screen_error;

    if (_cache) {
        stats.resident_splats = _cache->resident_splat_count();
        stats.resident_mb = (float)_cache->resident_bytes() / (1024.0f * 1024.0f);
    } else {
        stats.resident_splats = hier->num_splats;
        int64_t bytes = hier->num_splats * (12 + 16 + 12 + 4 + 12 + coeffs * 12);
        stats.resident_mb = (float)bytes / (1024.0f * 1024.0f);
    }

    return stats;
}

}  // namespace spirula

