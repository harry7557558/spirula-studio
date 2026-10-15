// HierarchyTraversal.cpp -- runtime LOD cut selection and extraction.

#include "lod/HierarchyTraversal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>

namespace spirula {

namespace {

inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

inline float logit(float p) {
    p = std::clamp(p, 1e-6f, 1.0f - 1e-6f);
    return std::log(p / (1.0f - p));
}

// Distance from point to AABB, 0 if point is inside.
float dist_point_aabb(const float p[3], const Aabb3& box) {
    float d2 = 0.0f;
    for (int i = 0; i < 3; i++) {
        if (p[i] < box.lo[i]) {
            float d = box.lo[i] - p[i];
            d2 += d * d;
        } else if (p[i] > box.hi[i]) {
            float d = p[i] - box.hi[i];
            d2 += d * d;
        }
    }
    return std::sqrt(d2);
}

// Maximum distance from point to any corner of AABB.
float max_dist_point_aabb(const float p[3], const Aabb3& box) {
    float d2 = 0.0f;
    for (int i = 0; i < 3; i++) {
        float d = std::max(std::abs(p[i] - box.lo[i]), std::abs(p[i] - box.hi[i]));
        d2 += d * d;
    }
    return std::sqrt(d2);
}

// Extract 6 frustum planes from a 4x4 row-major view-projection matrix.
// Plane equation: Ax + By + Cz + D >= 0 is inside.
void extract_frustum_planes(const float m[16], float planes[6][4]) {
    // Left: m3 + m0
    planes[0][0] = m[12] + m[0];
    planes[0][1] = m[13] + m[1];
    planes[0][2] = m[14] + m[2];
    planes[0][3] = m[15] + m[3];

    // Right: m3 - m0
    planes[1][0] = m[12] - m[0];
    planes[1][1] = m[13] - m[1];
    planes[1][2] = m[14] - m[2];
    planes[1][3] = m[15] - m[3];

    // Bottom: m3 + m1
    planes[2][0] = m[12] + m[4];
    planes[2][1] = m[13] + m[5];
    planes[2][2] = m[14] + m[6];
    planes[2][3] = m[15] + m[7];

    // Top: m3 - m1
    planes[3][0] = m[12] - m[4];
    planes[3][1] = m[13] - m[5];
    planes[3][2] = m[14] - m[6];
    planes[3][3] = m[15] - m[7];

    // Near: m3 + m2 (or m2 for [0, 1] z)
    planes[4][0] = m[12] + m[8];
    planes[4][1] = m[13] + m[9];
    planes[4][2] = m[14] + m[10];
    planes[4][3] = m[15] + m[11];

    // Far: m3 - m2
    planes[5][0] = m[12] - m[8];
    planes[5][1] = m[13] - m[9];
    planes[5][2] = m[14] - m[10];
    planes[5][3] = m[15] - m[11];

    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float len = std::sqrt(planes[i][0] * planes[i][0] +
                              planes[i][1] * planes[i][1] +
                              planes[i][2] * planes[i][2]);
        if (len > 1e-8f) {
            float inv = 1.0f / len;
            planes[i][0] *= inv;
            planes[i][1] *= inv;
            planes[i][2] *= inv;
            planes[i][3] *= inv;
        }
    }
}

}  // namespace

bool test_aabb_frustum_pinhole(const Aabb3& box, const float view_proj[16]) {
    if (!box.valid()) return false;

    float planes[6][4];
    extract_frustum_planes(view_proj, planes);

    for (int p = 0; p < 6; p++) {
        // Find the p-vertex (point with maximum projection along plane normal)
        float px = (planes[p][0] > 0.0f) ? box.hi[0] : box.lo[0];
        float py = (planes[p][1] > 0.0f) ? box.hi[1] : box.lo[1];
        float pz = (planes[p][2] > 0.0f) ? box.hi[2] : box.lo[2];

        if (planes[p][0] * px + planes[p][1] * py + planes[p][2] * pz + planes[p][3] < 0.0f) {
            return false; // Box is completely outside this plane
        }
    }
    return true;
}

bool test_aabb_equirectangular(const Aabb3& box, const float cam_pos[3], float z_near, float z_far) {
    if (!box.valid()) return false;

    float d_min = dist_point_aabb(cam_pos, box);
    if (d_min > z_far) return false;

    float d_max = max_dist_point_aabb(cam_pos, box);
    if (d_max < z_near) return false;

    return true;
}

float compute_screen_error(const HierarchyNode& node, const LodViewParams& view) {
    if (node.error_radius <= 0.0f) return 0.0f;

    // Conservative distance from camera to the bounding box
    float d = dist_point_aabb(view.cam_pos, node.bounds);
    d = std::max(d, view.z_near);

    if (view.camera_model == LodCameraModel::Equirectangular) {
        // Angular error in radians ~ error_radius / distance
        float angle_rad = node.error_radius / d;
        // 360 deg longitude = width pixels, 180 deg latitude = height pixels
        float px_per_rad = std::max((float)view.width / (2.0f * 3.14159265f),
                                    (float)view.height / 3.14159265f);
        return angle_rad * px_per_rad;
    } else {
        // Pinhole projection: error_px = (error_radius / z) * focal_length
        float focal = std::max(view.fx, view.fy);
        return (node.error_radius / d) * focal;
    }
}

LodCutResult HierarchyTraversal::select_cut(
    const GaussianHierarchy& hier,
    const LodViewParams& view,
    const LodRenderConfig& cfg)
{
    LodCutResult res;
    if (hier.nodes.empty()) return res;

    std::unordered_set<int32_t> next_selected_nodes;

    // BFS/DFS queue: node indices to visit
    std::vector<int32_t> queue;
    queue.push_back(hier.root);

    const float base_threshold = std::max(0.001f, cfg.quality_threshold);
    const float h_band = std::clamp(cfg.hysteresis_band, 0.0f, 0.9f);
    const float t_band = std::clamp(cfg.transition_band, 0.0f, 0.5f);

    while (!queue.empty()) {
        int32_t nid = queue.back();
        queue.pop_back();
        res.num_nodes_traversed++;

        if (nid < 0 || nid >= (int32_t)hier.nodes.size()) continue;
        const auto& node = hier.nodes[(size_t)nid];

        // Frustum culling
        bool visible = true;
        if (view.camera_model == LodCameraModel::Equirectangular) {
            visible = test_aabb_equirectangular(node.bounds, view.cam_pos, view.z_near, view.z_far);
        } else {
            // If view_proj is set (any non-zero element), use pinhole frustum culling
            bool has_vp = false;
            for (int i = 0; i < 16; i++) {
                if (view.view_proj[i] != 0.0f) { has_vp = true; break; }
            }
            if (has_vp) {
                visible = test_aabb_frustum_pinhole(node.bounds, view.view_proj);
            }
        }

        if (!visible) {
            res.num_nodes_culled++;
            continue;
        }

        // Screen-space error computation
        float error_px = compute_screen_error(node, view);
        res.max_screen_error = std::max(res.max_screen_error, error_px);

        // Apply hysteresis: if this node was previously selected in the cut,
        // it requires a slightly higher error before expanding to children.
        float effective_threshold = base_threshold;
        if (!_prev_selected_nodes.empty() && _prev_selected_nodes.count(nid)) {
            effective_threshold *= (1.0f + h_band);
        }

        bool should_refine = (error_px > effective_threshold) && !node.is_leaf();

        if (cfg.freeze_lod && !_prev_selected_nodes.empty()) {
            // If freeze_lod is enabled, keep previous selection state
            if (_prev_selected_nodes.count(nid)) {
                should_refine = false;
            }
        }

        if (should_refine) {
            // Recurse into children
            if (node.child_left >= 0) queue.push_back(node.child_left);
            if (node.child_right >= 0) queue.push_back(node.child_right);
        } else {
            // Stop at this node!
            next_selected_nodes.insert(nid);

            // Compute transition weight if within transition band
            float opacity_weight = 1.0f;
            if (t_band > 0.0f && !node.is_leaf()) {
                float low_bound = effective_threshold * (1.0f - t_band);
                float high_bound = effective_threshold * (1.0f + t_band);
                if (error_px >= low_bound && error_px <= high_bound) {
                    float t = (error_px - low_bound) / (high_bound - low_bound);
                    opacity_weight = 1.0f - t * 0.5f; // smoothly decay as approaching refinement
                }
            }

            if (node.is_leaf()) {
                SelectedLodItem item;
                item.node_index = nid;
                item.is_leaf = true;
                item.splat_offset = node.splat_offset;
                item.splat_count = node.splat_count;
                item.opacity_multiplier = opacity_weight;
                res.items.push_back(item);
                res.total_rendered_splats += node.splat_count;
                res.num_leaf_nodes_selected++;
            } else {
                // Use summary Gaussians of this parent node
                int32_t s_count = (int32_t)node.summary.size();
                for (int32_t s = 0; s < s_count; s++) {
                    SelectedLodItem item;
                    item.node_index = nid;
                    item.is_leaf = false;
                    item.summary_index = s;
                    item.opacity_multiplier = opacity_weight;
                    res.items.push_back(item);
                }
                res.total_rendered_splats += s_count;
                res.num_summary_nodes_selected++;
            }
        }
    }

    _prev_selected_nodes = std::move(next_selected_nodes);
    return res;
}

void HierarchyTraversal::extract_render_splats(
    const GaussianHierarchy& hier,
    const LodCutResult& cut,
    std::vector<float>& out_means,
    std::vector<float>& out_quats,
    std::vector<float>& out_scales,
    std::vector<float>& out_opacities,
    std::vector<float>& out_dc,
    std::vector<float>& out_sh)
{
    const int64_t total = cut.total_rendered_splats;
    const int64_t coeffs = hier.coeffs();

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

    int64_t cursor = 0;
    for (const auto& item : cut.items) {
        if (item.is_leaf) {
            const int64_t count = item.splat_count;
            const int64_t off = item.splat_offset;
            const float op_mult = item.opacity_multiplier;

            for (int64_t s = 0; s < count; s++) {
                const int64_t src_idx = off + s;
                const int64_t dst_idx = cursor + s;

                for (int j = 0; j < 3; j++) {
                    out_means[(size_t)(dst_idx * 3 + j)] = hier.means[(size_t)(src_idx * 3 + j)];
                    out_scales[(size_t)(dst_idx * 3 + j)] = hier.scales[(size_t)(src_idx * 3 + j)];
                    out_dc[(size_t)(dst_idx * 3 + j)] = hier.features_dc[(size_t)(src_idx * 3 + j)];
                }
                for (int j = 0; j < 4; j++) {
                    out_quats[(size_t)(dst_idx * 4 + j)] = hier.quats[(size_t)(src_idx * 4 + j)];
                }

                float raw_op = hier.opacities[(size_t)src_idx];
                if (op_mult < 0.999f) {
                    float sig = sigmoid(raw_op) * op_mult;
                    out_opacities[(size_t)dst_idx] = logit(sig);
                } else {
                    out_opacities[(size_t)dst_idx] = raw_op;
                }

                if (coeffs > 0) {
                    std::copy_n(&hier.features_sh[(size_t)(src_idx * coeffs * 3)],
                                coeffs * 3,
                                &out_sh[(size_t)(dst_idx * coeffs * 3)]);
                }
            }
            cursor += count;
        } else {
            // Summary Gaussian from parent node
            const auto& nd = hier.nodes[(size_t)item.node_index];
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

                // Summary Gaussians have zero higher-order SH to avoid high-frequency ringing at low detail.
                cursor++;
            }
        }
    }
}

}  // namespace spirula
