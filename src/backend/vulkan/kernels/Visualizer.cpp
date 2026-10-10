// Vulkan implementation of the visualizer entry points defined in
// kernels/visualize/Visualizer.cu: engine_viewer_init / set_camera_size / set_grid /
// capture_thumbnails, engine_blit_view, blit_train_cameras_tensor, plus the
// LBVH build they share. Host orchestration mirrors the CUDA file; device
// work runs shaders/visualizer.slang (see its header for the
// portability substitutions: mapped-u32 float atomics, groupshared gray fit,
// word-packed uint8 I/O + RGB8 pack pass).
//
// Deliberate simplifications vs CUDA:
//  - No high-priority viewer stream: everything runs on the default stream
//    (the single Vulkan queue serializes submission order anyway; noted as a
//    limitation in the backend README stream section).
//  - The final [H,W,3] uint8 output is written by a pack kernel in 4-byte
//    words; the up-to-3-byte tail lands in the allocation's 4-byte rounding
//    (backend::device_malloc guarantees it).
//
// This TU references engine() (EngineState), so it only links into targets
// that also link the portable engine objects; the kernel-level parity tools
// that link ss_backend_vulkan alone never pull this object in.

#include <kernels/visualize/Visualizer.cuh>

#include <core/Tensor.h>
#include <engine/EngineState.h>
#include <engine/EngineCommon.h>

#include "backend/common/SortScan.h"
#include "backend/vulkan/kernels/KernelCommon.h"
#include "core/PixelFormat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

inline constexpr int kNumFrustumSegments = 16;
inline constexpr int kNumFrustumFaces = 8;

std::mutex& viewer_mutex() {
    static std::mutex m;
    return m;
}

// Order-preserving float<->u32 mapping (host mirror of the shader helpers).
uint32_t fmap_ordered(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return (b & 0x80000000u) != 0 ? ~b : (b | 0x80000000u);
}
float funmap_ordered(uint32_t u) {
    uint32_t b = (u & 0x80000000u) != 0 ? (u ^ 0x80000000u) : ~u;
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}


/* ---- param mirrors (see shaders/visualizer.slang) ---- */

struct VisFrustumParams {
    uint64_t intrins, widths, heights, camera_models, dist_coeffs, distortions,
        camera_to_worlds, lss_buffer, tri_buffer;
    float size;
    uint32_t N;
    uint32_t _pad0;
};
static_assert(sizeof(VisFrustumParams) == 9 * 8 + 3 * 4 + 4 /*pad*/,
              "layout");

struct VisAabbParams {
    uint64_t buffer, aabb_reduced;
    uint32_t num_elem, wgs_per_row, _pad0;
};
static_assert(sizeof(VisAabbParams) == 2 * 8 + 3 * 4 + 4, "layout");

struct VisMortonParams {
    uint64_t buffer, splat_keys;
    float rmin_x, rmin_y, rmin_z;
    float rmax_x, rmax_y, rmax_z;
    uint32_t num_elem, wgs_per_row;
};
static_assert(sizeof(VisMortonParams) == 2 * 8 + 8 * 4, "layout");

struct VisIdentityParams {
    uint64_t data;
    uint32_t n, wgs_per_row, _pad0;
};
static_assert(sizeof(VisIdentityParams) == 8 + 3 * 4 + 4, "layout");

struct VisLbvhNodesParams {
    uint64_t morton, element_indices, internal_nodes, parent_nodes;
    uint32_t num_elements, wgs_per_row;
};
static_assert(sizeof(VisLbvhNodesParams) == 4 * 8 + 2 * 4, "layout");

struct VisTreeInitParams {
    uint64_t treeAABB;
    uint32_t num_cells, wgs_per_row, _pad0;
};
static_assert(sizeof(VisTreeInitParams) == 8 + 3 * 4 + 4, "layout");

struct VisLbvhAabbParams {
    uint64_t buffer, internal_nodes, parent_nodes, treeAABB;
    uint32_t num_elements, wgs_per_row, _pad0;
};
static_assert(sizeof(VisLbvhAabbParams) == 4 * 8 + 3 * 4 + 4, "layout");

struct VisUnmapParams {
    uint64_t words;
    uint32_t n, wgs_per_row, _pad0;
};
static_assert(sizeof(VisUnmapParams) == 8 + 3 * 4 + 4, "layout");

struct VisMinMaxParams {
    uint64_t depths, min_max;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(VisMinMaxParams) == 2 * 8 + 3 * 4 + 4, "layout");

struct VisBlitParams {
    uint64_t render_rgbs, render_depths, render_alphas;
    uint64_t view_intrins, view_viewmat, view_dist;
    uint64_t lss_buffer, lss_nodes, lss_aabb;
    uint64_t tri_buffer, tri_nodes, tri_aabb;
    uint64_t overlay_colors, thumbnails, min_max, out_rgba;
    int32_t view_camera_model, width, height, rgb_channels;
    int32_t num_cam_lss, show_cams, show_overlay, thumb_w, thumb_h;
    int32_t has_lss, has_tri;
    int32_t alpha_stride;
};
static_assert(sizeof(VisBlitParams) == 16 * 8 + 12 * 4, "layout");

struct VisPackParams {
    uint64_t rgba, out_rgb8;
    uint32_t total_bytes, wgs_per_row;
};
static_assert(sizeof(VisPackParams) == 2 * 8 + 2 * 4, "layout");

struct VisThumbParams {
    uint64_t rgb, cam_indices, thumbnails, done_mask, alpha_mask;
    int32_t H_rgb, W_rgb, B_post, N, S, H_alpha, W_alpha, has_alpha;
    uint32_t rgb_fmt;
    int32_t _pad0;
};
static_assert(sizeof(VisThumbParams) == 5 * 8 + 10 * 4, "layout");

/* ---- LBVH build (mirrors Visualizer.cu build_bvh<>) ---- */

struct BvhResult {
    void* nodes;  // int2[num_elem-1]
    void* aabb;   // float3[2*num_elem] (internal-node AABBs, unmapped)
};

struct RootBox {
    float min[3], max[3];
};

// vis_prim: 0 = linear swept sphere, 1 = triangle (kVisPrim spec constant).
BvhResult build_bvh_vk(uint32_t vis_prim, uint32_t num_elem,
                       const void* buffer, const RootBox& root,
                       const std::string& key_prefix) {
    auto& pool = DevicePool::global();
    const VramCategory kBvhCat = VramCategory::Viewer;
    const backend::vk::SpecList spec{vis_prim};

    void* morton = pool.acquire_dynamic(kBvhCat, key_prefix + ".morton",
                                        (size_t)num_elem * 8);
    {
        VisMortonParams p{};
        p.buffer = (uint64_t)buffer;
        p.splat_keys = (uint64_t)morton;
        p.rmin_x = root.min[0];
        p.rmin_y = root.min[1];
        p.rmin_z = root.min[2];
        p.rmax_x = root.max[0];
        p.rmax_y = root.max[1];
        p.rmax_z = root.max[2];
        p.num_elem = num_elem;
        vkk::dispatch_flat("visualizer.vis_fill_morton", spec, num_elem, 256,
                          &p, sizeof(p), &p.wgs_per_row);
    }

    void* argsort_in = pool.acquire_dynamic(
        kBvhCat, key_prefix + ".argsort_in", (size_t)num_elem * 4);
    {
        VisIdentityParams p{};
        p.data = (uint64_t)argsort_in;
        p.n = num_elem;
        vkk::dispatch_flat("visualizer.vis_fill_identity",
                          backend::vk::SpecList{}, num_elem, 256, &p,
                          sizeof(p), &p.wgs_per_row);
    }

    void* sorted_morton = pool.acquire_dynamic(
        kBvhCat, key_prefix + ".sorted_morton", (size_t)num_elem * 8);
    void* argsort = pool.acquire_dynamic(kBvhCat, key_prefix + ".argsort",
                                         (size_t)num_elem * 4);
    backend::DoubleBuffer<int64_t> d_keys((int64_t*)morton,
                                          (int64_t*)sorted_morton);
    backend::DoubleBuffer<int32_t> d_values((int32_t*)argsort_in,
                                            (int32_t*)argsort);
    // morton keys use 3*21 = 63 bits; bit 63 is always 0
    backend::sort_pairs(d_keys, d_values, (int64_t)num_elem, 0, 63);

    void* internal_nodes = pool.acquire_dynamic(
        kBvhCat, key_prefix + ".nodes", (size_t)(num_elem - 1) * 8);
    void* parent_nodes = pool.acquire_dynamic(
        kBvhCat, key_prefix + ".parents", (size_t)(num_elem - 1) * 4);
    backend::memset_sync(parent_nodes, 0xff, (size_t)(num_elem - 1) * 4);
    {
        VisLbvhNodesParams p{};
        p.morton = (uint64_t)d_keys.current();
        p.element_indices = (uint64_t)d_values.current();
        p.internal_nodes = (uint64_t)internal_nodes;
        p.parent_nodes = (uint64_t)parent_nodes;
        p.num_elements = num_elem;
        vkk::dispatch_flat("visualizer.vis_lbvh_nodes",
                          backend::vk::SpecList{}, (int64_t)num_elem - 1, 256,
                          &p, sizeof(p), &p.wgs_per_row);
    }

    void* treeAABB = pool.acquire_dynamic(kBvhCat, key_prefix + ".aabb",
                                          (size_t)num_elem * 2 * 12);
    {
        VisTreeInitParams p{};
        p.treeAABB = (uint64_t)treeAABB;
        p.num_cells = num_elem - 1;
        vkk::dispatch_flat("visualizer.vis_tree_init_aabb",
                          backend::vk::SpecList{}, (int64_t)num_elem - 1, 256,
                          &p, sizeof(p), &p.wgs_per_row);
    }
    {
        VisLbvhAabbParams p{};
        p.buffer = (uint64_t)buffer;
        p.internal_nodes = (uint64_t)internal_nodes;
        p.parent_nodes = (uint64_t)parent_nodes;
        p.treeAABB = (uint64_t)treeAABB;
        p.num_elements = num_elem;
        vkk::dispatch_flat("visualizer.vis_lbvh_aabb", spec,
                          (int64_t)num_elem - 1, 256, &p, sizeof(p),
                          &p.wgs_per_row);
    }
    {
        VisUnmapParams p{};
        p.words = (uint64_t)treeAABB;
        p.n = (num_elem - 1) * 6;
        vkk::dispatch_flat("visualizer.vis_unmap_aabb",
                          backend::vk::SpecList{},
                          (int64_t)(num_elem - 1) * 6, 256, &p, sizeof(p),
                          &p.wgs_per_row);
    }

    return {internal_nodes, treeAABB};
}

// Frustum geometry + root AABB for a camera set; shared by the engine-cached
// and the legacy stateless paths.
RootBox compute_root_box(const void* root_aabb_dev) {
    uint32_t mapped[6];
    backend::memcpy_sync(mapped, root_aabb_dev, sizeof(mapped),
                         backend::MemcpyKind::DeviceToHost);
    float h_aabb[6];
    for (int k = 0; k < 6; k++) h_aabb[k] = funmap_ordered(mapped[k]);
    float cx = 0.5f * (h_aabb[3] + h_aabb[0]);
    float cy = 0.5f * (h_aabb[4] + h_aabb[1]);
    float cz = 0.5f * (h_aabb[5] + h_aabb[2]);
    float ex = 0.5f * (h_aabb[3] - h_aabb[0]);
    float ey = 0.5f * (h_aabb[4] - h_aabb[1]);
    float ez = 0.5f * (h_aabb[5] - h_aabb[2]);
    float ms = 1.01f * std::max(std::max(ex, ey), ez);
    RootBox box;
    box.min[0] = cx - ms;
    box.min[1] = cy - ms;
    box.min[2] = cz - ms;
    box.max[0] = cx + ms;
    box.max[1] = cy + ms;
    box.max[2] = cz + ms;
    return box;
}

// The tier travels per camera (p.distortions), not as kDistortion: the table
// may mix tiers, and the view camera's tier says nothing about the dataset's.
void fill_frustums(int64_t n, const void* intrins, const void* widths,
                   const void* heights, const void* camera_models,
                   const void* dist_coeffs, const void* distortions,
                   const void* c2w, float size, void* lss_buffer,
                   void* tri_buffer) {
    VisFrustumParams p{};
    p.intrins = (uint64_t)intrins;
    p.widths = (uint64_t)widths;
    p.heights = (uint64_t)heights;
    p.camera_models = (uint64_t)camera_models;
    p.dist_coeffs = vkk::or_fallback(dist_coeffs);
    p.distortions = (uint64_t)distortions;
    p.camera_to_worlds = (uint64_t)c2w;
    p.lss_buffer = (uint64_t)lss_buffer;
    p.tri_buffer = (uint64_t)tri_buffer;
    p.size = size;
    p.N = (uint32_t)n;
    vkk::dispatch("visualizer.vis_fill_frustum",
                 backend::vk::SpecList{0u, 0u}, (uint32_t)n, 1, 1, &p,
                 sizeof(p));
}

// root_aabb: u32[6] mapped; min words init 0xffffffff, max words 0.
void reduce_root_aabb(void* root_aabb, uint32_t vis_prim, const void* buffer,
                      int64_t num_elem) {
    VisAabbParams p{};
    p.buffer = (uint64_t)buffer;
    p.aabb_reduced = (uint64_t)root_aabb;
    p.num_elem = (uint32_t)num_elem;
    vkk::dispatch_flat("visualizer.vis_compute_aabb",
                      backend::vk::SpecList{vis_prim}, num_elem, 256, &p,
                      sizeof(p), &p.wgs_per_row);
}

void init_root_aabb(void* root_aabb) {
    backend::memset_sync(root_aabb, 0xff, 12);              // min: u32 max
    backend::memset_sync((char*)root_aabb + 12, 0x00, 12);  // max: u32 min
}

// min_max: u32[2] mapped; blit decodes.
void* compute_min_max(PoolSlot slot, const void* depths, int64_t total) {
    void* min_max = DevicePool::global().acquire<float>(slot, 2);
    backend::memset_sync(min_max, 0xff, 4);
    backend::memset_sync((char*)min_max + 4, 0x00, 4);
    VisMinMaxParams p{};
    p.depths = (uint64_t)depths;
    p.min_max = (uint64_t)min_max;
    p.total = (uint32_t)total;
    vkk::dispatch_flat("visualizer.vis_min_max", backend::vk::SpecList{},
                      total, 256, &p, sizeof(p), &p.wgs_per_row);
    return min_max;
}

struct BlitGeom {
    const void* lss_buffer = nullptr;
    const void* lss_nodes = nullptr;
    const void* lss_aabb = nullptr;
    const void* tri_buffer = nullptr;
    const void* tri_nodes = nullptr;
    const void* tri_aabb = nullptr;
    const void* overlay_colors = nullptr;
    int num_cam_lss = 0;
    bool show_cams = false;
    bool show_overlay = false;
};

void run_blit(const TorchTensorView& render_rgbs,
              const TorchTensorView& render_depths,
              const TorchTensorView& render_alphas, int view_camera_model,
              uint32_t dist_spec, const TorchTensorView& view_intrins,
              const TorchTensorView& view_viewmat,
              const TorchTensorView& view_dist_coeffs, const BlitGeom& geom,
              const void* thumbnails, int thumb_w, int thumb_h,
              const void* min_max, const TorchTensorView& out_rgb) {
    auto& rgb_shape = std::get<2>(render_rgbs);
    const int64_t h = rgb_shape[0], w = rgb_shape[1], c = rgb_shape[2];

    auto& alpha_shape = std::get<2>(render_alphas);
    const int64_t alpha_stride = alpha_shape.empty() ? 1 : alpha_shape.back();

    void* rgba = DevicePool::global().acquire_dynamic(
        VramCategory::Viewer, "viewer.blit_rgba", (size_t)h * w * 4);

    VisBlitParams p{};
    p.render_rgbs = std::get<0>(render_rgbs);
    p.render_depths = std::get<0>(render_depths);
    p.render_alphas = std::get<0>(render_alphas);
    p.view_intrins = std::get<0>(view_intrins);
    p.view_viewmat = std::get<0>(view_viewmat);
    p.view_dist = vkk::or_fallback(std::get<0>(view_dist_coeffs));
    p.lss_buffer = vkk::or_fallback(geom.lss_buffer);
    p.lss_nodes = vkk::or_fallback(geom.lss_nodes);
    p.lss_aabb = vkk::or_fallback(geom.lss_aabb);
    p.tri_buffer = vkk::or_fallback(geom.tri_buffer);
    p.tri_nodes = vkk::or_fallback(geom.tri_nodes);
    p.tri_aabb = vkk::or_fallback(geom.tri_aabb);
    p.overlay_colors = vkk::or_fallback(geom.overlay_colors);
    p.thumbnails = vkk::or_fallback(thumbnails);
    p.min_max = vkk::or_fallback(min_max);
    p.out_rgba = (uint64_t)rgba;
    p.view_camera_model = view_camera_model;
    p.width = (int32_t)w;
    p.height = (int32_t)h;
    p.rgb_channels = (int32_t)c;
    p.num_cam_lss = geom.num_cam_lss;
    p.show_cams = geom.show_cams ? 1 : 0;
    p.show_overlay = geom.show_overlay ? 1 : 0;
    p.thumb_w = thumb_w;
    p.thumb_h = thumb_h;
    p.has_lss = geom.lss_buffer ? 1 : 0;
    p.has_tri = geom.tri_buffer ? 1 : 0;
    p.alpha_stride = (int32_t)alpha_stride;

    uint64_t params_addr = 0;
    void* params_mapped = nullptr;
    if (!backend::vk::params_alloc(sizeof(p), &params_addr, &params_mapped))
        throw std::runtime_error("Vulkan backend: params ring failed");
    std::memcpy(params_mapped, &p, sizeof(p));
    vkk::dispatch("visualizer.vis_blit", backend::vk::SpecList{0u, dist_spec},
                 (uint32_t)((w + 7) / 8), (uint32_t)((h + 3) / 4), 1,
                 &params_addr, sizeof(params_addr));

    VisPackParams pk{};
    pk.rgba = (uint64_t)rgba;
    pk.out_rgb8 = std::get<0>(out_rgb);
    pk.total_bytes = (uint32_t)(3 * h * w);
    const int64_t words = (pk.total_bytes + 3) / 4;
    vkk::dispatch_flat("visualizer.vis_pack_rgb8", backend::vk::SpecList{},
                      words, 256, &pk, sizeof(pk), &pk.wgs_per_row);
}

/* ---- engine-cached BVH build (mirrors _viewer_build_bvh) ---- */

void _viewer_build_bvh() {
    auto& v = engine().viewer;
    int64_t n = v.N_post;

    uint32_t num_cam_lss = (uint32_t)(n * 8 * kNumFrustumSegments);
    uint32_t num_lss = num_cam_lss + (uint32_t)v.num_overlay;
    uint32_t num_tri = (uint32_t)(n * 4 * kNumFrustumFaces * kNumFrustumFaces);

    float4* lss_buffer = DevicePool::global().acquire<float4>(
        PoolSlot::ViewerLss, (size_t)num_lss * 2);
    float4* tri_buffer = DevicePool::global().acquire<float4>(
        PoolSlot::ViewerTri, (size_t)num_tri * 4);

    fill_frustums(n, v.d_intrins.data_ptr(), v.d_widths.data_ptr(),
                  v.d_heights.data_ptr(), v.d_camera_models.data_ptr(),
                  v.d_dist_coeffs.data_ptr(), v.d_distortions.data_ptr(),
                  v.d_camera_to_worlds.data_ptr(), v.camera_size, lss_buffer,
                  tri_buffer);

    if (v.num_overlay > 0)
        backend::memcpy_sync(lss_buffer + (size_t)num_cam_lss * 2,
                             v.d_overlay_segs.data_ptr(),
                             (size_t)v.num_overlay * 2 * sizeof(float4),
                             backend::MemcpyKind::DeviceToDevice);

    float3* root_aabb =
        DevicePool::global().acquire<float3>(PoolSlot::ViewerRootAabb, 2);
    init_root_aabb(root_aabb);
    reduce_root_aabb(root_aabb, 0, lss_buffer, num_lss);
    reduce_root_aabb(root_aabb, 1, tri_buffer, num_tri);
    RootBox root = compute_root_box(root_aabb);

    auto lss_bvh = build_bvh_vk(0, num_lss, lss_buffer, root,
                                "viewer.lss_bvh");
    auto tri_bvh = build_bvh_vk(1, num_tri, tri_buffer, root,
                                "viewer.tri_bvh");
    v.bvh_lss_nodes_ptr = lss_bvh.nodes;
    v.bvh_lss_aabb_ptr = lss_bvh.aabb;
    v.bvh_tri_nodes_ptr = tri_bvh.nodes;
    v.bvh_tri_aabb_ptr = tri_bvh.aabb;

    v.bvh_num_lss = (int)num_lss;
    v.bvh_num_tri = (int)num_tri;
    v.bvh_num_cam_lss = (int)num_cam_lss;
    v.bvh_camera_size = v.camera_size;
    v.bvh_built = true;
}

/* ---- zoom-adaptive grid overlay (host math copied from Visualizer.cu) ---- */

void _viewer_build_grid(float view_dist, const float target[3]) {
    auto& v = engine().viewer;
    const float radius = v.grid_radius;
    const float s =
        powf(10.0f, floorf(log10f(std::max(view_dist, 1e-6f) / 2.0f)));
    int half = (int)ceilf(radius / s);
    half = half < 20 ? 20 : (half > 100 ? 100 : half);
    bool recenter =
        fabsf(target[0] - v.grid_center[0]) > 0.25f * half * s ||
        fabsf(target[1] - v.grid_center[1]) > 0.25f * half * s;
    if (v.num_overlay > 0 && s == v.grid_spacing && half == v.grid_half &&
        !recenter)
        return;
    const long gx0 = lroundf(target[0] / s), gy0 = lroundf(target[1] / s);
    const float cx = gx0 * s, cy = gy0 * s;

    const float rr = 0.004f * s;
    const float kMinor[3] = {0.28f, 0.30f, 0.33f};
    const float kMajor[3] = {0.45f, 0.47f, 0.50f};
    const float kAxis[3][3] = {{0.98f, 0.20f, 0.31f},
                               {0.55f, 0.86f, 0.00f},
                               {0.16f, 0.55f, 0.98f}};

    std::vector<float> segs, colors;
    segs.reserve(((size_t)(4 * half + 2) * 2 * half + 3 * half) * 8);
    colors.reserve(segs.capacity() * 3 / 8);
    auto seg = [&](float ax, float ay, float az, float bx, float by, float bz,
                   float r, const float c[3]) {
        segs.insert(segs.end(), {ax, ay, az, r});
        segs.insert(segs.end(), {bx, by, bz, r});
        colors.insert(colors.end(), {c[0], c[1], c[2]});
    };
    for (int i = -half; i <= half; i++) {
        const float* cX = ((gx0 + i) % 10 == 0) ? kMajor : kMinor;
        const float* cY = ((gy0 + i) % 10 == 0) ? kMajor : kMinor;
        for (int j = -half; j < half; j++) {
            seg(cx + i * s, cy + j * s, 0, cx + i * s, cy + (j + 1) * s, 0,
                rr, cX);
            seg(cx + j * s, cy + i * s, 0, cx + (j + 1) * s, cy + i * s, 0,
                rr, cY);
        }
    }
    for (int a = 0; a < 3; a++)
        for (int j = 0; j < half; j++) {
            float p0[3] = {0, 0, 0}, p1[3] = {0, 0, 0};
            p0[a] = j * s;
            p1[a] = (j + 1) * s;
            seg(p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], 2.0f * rr,
                kAxis[a]);
        }

    int64_t M = (int64_t)colors.size() / 3;
    v.d_overlay_segs = _hv_to_dv<float>(
        PoolSlot::ViewerOverlaySegs,
        TorchTensorView((uint64_t)segs.data(), 4, {M * 8}));
    v.d_overlay_colors = _hv_to_dv<float>(
        PoolSlot::ViewerOverlayColors,
        TorchTensorView((uint64_t)colors.data(), 4, {M * 3}));
    v.num_overlay = (int)M;
    v.grid_spacing = s;
    v.grid_half = half;
    v.grid_center[0] = cx;
    v.grid_center[1] = cy;
    v.bvh_built = false;
}

}  // namespace

/* ==== API definitions matching kernels/visualize/Visualizer.cu ==== */

void engine_viewer_init(
    TorchTensorView camera_models,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    TorchTensorView distortions,
    TorchTensorView camera_to_worlds,
    TorchTensorView widths,
    TorchTensorView heights,
    float camera_size)
{
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    int64_t N = std::get<2>(intrins)[0];
    if (N <= 0)
        throw std::runtime_error("engine_viewer_init: N_post must be > 0");

    v.N_post = (int)N;
    v.camera_size = camera_size;

    v.d_intrins = _hv_to_dv<float>(
        PoolSlot::ViewerIntrins,
        TorchTensorView(std::get<0>(intrins), 4, {N * 4LL}));
    v.d_widths = _hv_to_dv<int32_t>(PoolSlot::ViewerWidths, widths);
    v.d_heights = _hv_to_dv<int32_t>(PoolSlot::ViewerHeights, heights);
    v.d_camera_models =
        _hv_to_dv<int32_t>(PoolSlot::ViewerCmodels, camera_models);
    v.d_dist_coeffs = _hv_to_dv<float>(
        PoolSlot::ViewerDist,
        TorchTensorView(std::get<0>(dist_coeffs), 4,
                        {N * (int64_t)kCameraDistortionParams}));
    v.d_distortions =
        _hv_to_dv<int32_t>(PoolSlot::ViewerDistTier, distortions);
    v.d_camera_to_worlds = _hv_to_dv<float>(
        PoolSlot::ViewerC2w,
        TorchTensorView(std::get<0>(camera_to_worlds), 4, {N * 12LL}));

    constexpr int S = VIEWER_THUMBNAIL_SIZE;
    v.thumbnails.resize(PoolSlot::ViewerThumb, (int64_t)N * S * S * 4);
    backend::memset_sync(v.thumbnails.data_ptr(), 0,
                         (size_t)N * S * S * 4);
    v.thumbnail_done_mask.resize(PoolSlot::ViewerThumbDone, N);
    backend::memset_sync(v.thumbnail_done_mask.data_ptr(), 0, (size_t)N);
    v.host_seen_mask.assign((size_t)N, 0);
    v.pending_thumb = (int)N;

    v.bvh_built = false;
    v.initialized = true;
}

void engine_viewer_set_camera_size(float camera_size) {
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized) return;
    v.camera_size = camera_size;
}

void engine_viewer_set_grid(float radius, float view_distance)
{
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized) return;
    v.grid_radius = std::max(radius, 1e-6f);
    v.grid_spacing = 0.0f;
    const float origin[3] = {0, 0, 0};
    _viewer_build_grid(view_distance > 0.0f ? view_distance : v.grid_radius,
                       origin);
}

void engine_viewer_capture_thumbnails(TorchTensorView cam_indices_tv) {
    if (!engine().viewer.initialized || engine().viewer.pending_thumb <= 0)
        return;
    if (!engine().gt.has_gt) return;
    const TorchTensorView& gt_rgb = engine().gt.rgb;
    if (std::get<0>(gt_rgb) == 0) return;

    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized || v.pending_thumb <= 0) return;

    const auto& gt_shape = std::get<2>(gt_rgb);
    int64_t B_post = gt_shape[0];
    int64_t H = gt_shape[1];
    int64_t W = gt_shape[2];
    if (B_post <= 0 || H <= 0 || W <= 0) return;

    uint64_t ci_ptr = std::get<0>(cam_indices_tv);
    if (ci_ptr == 0) return;

    std::vector<int32_t> host_ci((size_t)B_post);
    if (_is_device_ptr((void*)ci_ptr)) {
        backend::memcpy_sync(host_ci.data(), (const void*)ci_ptr,
                             (size_t)B_post * sizeof(int32_t),
                             backend::MemcpyKind::DeviceToHost);
    } else {
        std::memcpy(host_ci.data(), (const void*)ci_ptr,
                    (size_t)B_post * sizeof(int32_t));
    }

    int newly = 0;
    for (int j = 0; j < B_post; ++j) {
        int idx = host_ci[j];
        if (idx >= 0 && idx < v.N_post && !v.host_seen_mask[(size_t)idx]) {
            v.host_seen_mask[(size_t)idx] = 1;
            ++newly;
        }
    }
    if (newly == 0) return;
    v.pending_thumb -= newly;

    const int32_t* d_ci = engine().bilagrid_cur_cam_indices.data_ptr();
    if (d_ci == nullptr) {
        int32_t* tmp_ci = DevicePool::global().acquire<int32_t>(
            PoolSlot::ViewerCapIdx, (size_t)B_post);
        backend::memcpy_sync(tmp_ci, host_ci.data(),
                             (size_t)B_post * sizeof(int32_t),
                             backend::MemcpyKind::HostToDevice);
        d_ci = tmp_ci;
    }

    const uint8_t* d_alpha_mask = nullptr;
    int H_alpha = 0, W_alpha = 0;
    if (engine().gt.has_mask && engine().gt.alpha.data_ptr() != nullptr
        && engine().gt.alpha.size<0>() == B_post) {
        d_alpha_mask = engine().gt.alpha.data_ptr();
        H_alpha = (int)engine().gt.alpha.size<1>();
        W_alpha = (int)engine().gt.alpha.size<2>();
    }

    VisThumbParams p{};
    p.rgb = std::get<0>(gt_rgb);
    p.rgb_fmt = (uint32_t)pixel_format(gt_rgb);
    p.cam_indices = (uint64_t)d_ci;
    p.thumbnails = (uint64_t)v.thumbnails.data_ptr();
    p.done_mask = (uint64_t)v.thumbnail_done_mask.data_ptr();
    p.alpha_mask = vkk::or_fallback(d_alpha_mask);
    p.H_rgb = (int)H;
    p.W_rgb = (int)W;
    p.B_post = (int)B_post;
    p.N = v.N_post;
    p.S = VIEWER_THUMBNAIL_SIZE;
    p.H_alpha = H_alpha;
    p.W_alpha = W_alpha;
    p.has_alpha = d_alpha_mask ? 1 : 0;
    vkk::dispatch("visualizer.vis_update_thumbnails", backend::vk::SpecList{},
                 (uint32_t)B_post, 1, 1, &p, sizeof(p));
}

/*[AutoHeaderGeneratorExport]*/
void engine_blit_view(
    std::string     buffer_key,
    TorchTensorView render_buffer,
    TorchTensorView render_depth,
    TorchTensorView render_alpha,
    int             view_camera_model,
    std::string     distortion,
    TorchTensorView view_intrins,
    TorchTensorView view_viewmat,
    TorchTensorView view_dist_coeffs,
    bool            show_training_cameras,
    bool            show_overlay,
    float           grid_dist,
    float           grid_target_x,
    float           grid_target_y,
    float           grid_target_z,
    TorchTensorView out_rgb)
{
    const uint32_t dist_spec = vkk::distortion_spec(distortion);
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized) {
        throw std::runtime_error(
            "engine_blit_view: viewer not initialized; call "
            "engine_viewer_init() first.");
    }
    if (show_overlay && grid_dist > 0.0f && v.grid_radius > 0.0f) {
        const float target[3] = {grid_target_x, grid_target_y,
                                 grid_target_z};
        _viewer_build_grid(grid_dist, target);
    }
    show_overlay = show_overlay && v.num_overlay > 0;

    auto& rgb_shape = std::get<2>(render_buffer);
    int64_t h = rgb_shape[0], w = rgb_shape[1], c = rgb_shape[2];
    (void)buffer_key;

    const void* min_max = nullptr;
    if (c == 1)
        min_max = compute_min_max(PoolSlot::ViewerMinMax,
                                  (const void*)std::get<0>(render_buffer),
                                  h * w);

    BlitGeom geom;
    if (show_training_cameras || show_overlay) {
        if (!v.bvh_built || v.bvh_camera_size != v.camera_size)
            _viewer_build_bvh();
        geom.lss_buffer = DevicePool::global().acquire<float4>(
            PoolSlot::ViewerLss, (size_t)v.bvh_num_lss * 2);
        geom.lss_nodes = v.bvh_lss_nodes_ptr;
        geom.lss_aabb = v.bvh_lss_aabb_ptr;
        if (show_training_cameras) {  // thumbnails only with the frusta
            geom.tri_buffer = DevicePool::global().acquire<float4>(
                PoolSlot::ViewerTri, (size_t)v.bvh_num_tri * 4);
            geom.tri_nodes = v.bvh_tri_nodes_ptr;
            geom.tri_aabb = v.bvh_tri_aabb_ptr;
        }
        geom.overlay_colors = v.d_overlay_colors.data_ptr();
        geom.num_cam_lss = v.bvh_num_cam_lss;
        geom.show_cams = show_training_cameras;
        geom.show_overlay = show_overlay;
    }

    run_blit(render_buffer, render_depth, render_alpha, view_camera_model,
             dist_spec, view_intrins, view_viewmat, view_dist_coeffs, geom,
             v.thumbnails.data_ptr(), VIEWER_THUMBNAIL_SIZE,
             VIEWER_THUMBNAIL_SIZE, min_max, out_rgb);
}

/*[AutoHeaderGeneratorExport]*/
void blit_train_cameras_tensor(
    TorchTensorView render_rgbs,
    TorchTensorView render_depths,
    TorchTensorView render_alphas,
    const int view_camera_model,
    std::string distortion,
    TorchTensorView view_intrins,
    TorchTensorView view_viewmat,
    TorchTensorView view_dist_coeffs,
    TorchTensorView intrins,
    TorchTensorView widths,
    TorchTensorView heights,
    TorchTensorView camera_models,
    TorchTensorView dist_coeffs,
    TorchTensorView distortions,
    TorchTensorView camera_to_worlds,
    TorchTensorView thumbnails,
    float camera_size,
    bool show_training_cameras,
    TorchTensorView out_rgb
) {
    const uint32_t dist_spec = vkk::distortion_spec(distortion);
    auto& rgb_shape = std::get<2>(render_rgbs);
    int64_t h = rgb_shape[0], w = rgb_shape[1], c = rgb_shape[2];
    int64_t n = std::get<2>(intrins)[0];
    auto& thumb_shape = std::get<2>(thumbnails);
    int thumb_h = (int)thumb_shape[1], thumb_w = (int)thumb_shape[2];

    const void* min_max = nullptr;
    if (c == 1)
        min_max = compute_min_max(PoolSlot::VisMinMax,
                                  (const void*)std::get<0>(render_rgbs),
                                  h * w);

    if (!show_training_cameras) {
        BlitGeom geom;  // no frusta, no overlay
        run_blit(render_rgbs, render_depths, render_alphas,
                 view_camera_model, dist_spec, view_intrins, view_viewmat,
                 view_dist_coeffs, geom, (void*)std::get<0>(thumbnails),
                 thumb_w, thumb_h, min_max, out_rgb);
        return;
    }

    uint32_t num_lss = (uint32_t)(n * 8 * kNumFrustumSegments);
    uint32_t num_tri = (uint32_t)(n * 4 * kNumFrustumFaces * kNumFrustumFaces);
    float4* lss_buffer = DevicePool::global().acquire<float4>(
        PoolSlot::VisLss, (size_t)num_lss * 2);
    float4* tri_buffer = DevicePool::global().acquire<float4>(
        PoolSlot::VisTri, (size_t)num_tri * 4);
    fill_frustums(n, (const void*)std::get<0>(intrins),
                  (const void*)std::get<0>(widths),
                  (const void*)std::get<0>(heights),
                  (const void*)std::get<0>(camera_models),
                  (const void*)std::get<0>(dist_coeffs),
                  (const void*)std::get<0>(distortions),
                  (const void*)std::get<0>(camera_to_worlds), camera_size,
                  lss_buffer, tri_buffer);

    float3* root_aabb =
        DevicePool::global().acquire<float3>(PoolSlot::VisRootAabb, 2);
    init_root_aabb(root_aabb);
    reduce_root_aabb(root_aabb, 0, lss_buffer, num_lss);
    reduce_root_aabb(root_aabb, 1, tri_buffer, num_tri);
    RootBox root = compute_root_box(root_aabb);

    auto lss_bvh = build_bvh_vk(0, num_lss, lss_buffer, root, "vis.lss_bvh");
    auto tri_bvh = build_bvh_vk(1, num_tri, tri_buffer, root, "vis.tri_bvh");

    BlitGeom geom;
    geom.lss_buffer = lss_buffer;
    geom.lss_nodes = lss_bvh.nodes;
    geom.lss_aabb = lss_bvh.aabb;
    geom.tri_buffer = tri_buffer;
    geom.tri_nodes = tri_bvh.nodes;
    geom.tri_aabb = tri_bvh.aabb;
    geom.overlay_colors = nullptr;
    geom.num_cam_lss = (int)num_lss;  // no overlay in the legacy path
    geom.show_cams = true;
    geom.show_overlay = false;
    run_blit(render_rgbs, render_depths, render_alphas, view_camera_model,
             dist_spec, view_intrins, view_viewmat, view_dist_coeffs, geom,
             (void*)std::get<0>(thumbnails), thumb_w, thumb_h, min_max,
             out_rgb);
}
