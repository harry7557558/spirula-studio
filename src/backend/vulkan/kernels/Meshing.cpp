// Vulkan implementation of the meshing device seam (mesh/MeshingDevice.h).
// Device work: shaders/meshing.slang (activation, point cloud, LBVH build,
// occupancy, bisection, color) and shaders/meshing_raster.slang (per-camera
// sampling + the visibility cull). The host orchestration that calls these is
// portable and shared with CUDA -- mesh/OccupancyEvaluator.cpp and
// mesh/MeshingRasterHost.cpp.
//
// Two things differ from the CUDA kernels and are handled entirely here, so
// the seam stays identical:
//   * node AABBs are merged in fmap_ordered u32 space (a plain
//     InterlockedMin/Max instead of a float CAS loop), so launch_lbvh_aabb
//     runs an extra unmap pass and every consumer still reads floats;
//   * the camera model is a specialization constant, not a kernel argument.

#include <mesh/MeshingDevice.h>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// ---------------------------------------------------------------------------
// Params mirrors. Every struct lists ALL pointers first and ALL 4-byte scalars
// after, exactly as the .slang declarations do, so there is no internal
// padding for the two languages to disagree about.
// ---------------------------------------------------------------------------

#define MESH_SCENE_PTRS                                                     \
    uint64_t sc_mean, sc_ax0, sc_ax1, sc_ax2, sc_invs2, sc_opac, sc_k2,     \
        sc_gcol, sc_kept, sc_leafMin, sc_leafMax, sc_internal, sc_nodeAABB, \
        sc_campos;
#define MESH_SCENE_SCALARS \
    int32_t sc_num_kept;   \
    int32_t sc_num_cameras;\
    float sc_iso;
constexpr size_t kSceneP = 14 * 8;   // MESH_SCENE_PTRS bytes
constexpr size_t kSceneS = 3 * 4;    // MESH_SCENE_SCALARS bytes

struct MeshActivateParams {
    uint64_t means, quats, logsc, logit, fdc;
    uint64_t mean, ax0, ax1, ax2, invs2, opac, radius, k2, valid, gcol;
    uint32_t N, wgs_per_row;
};
static_assert(sizeof(MeshActivateParams) == 15 * 8 + 2 * 4, "layout");

struct MeshPointCloudParams {
    uint64_t kept, mean, ax0, ax1, ax2, invs2, k2, out_xyz;
    float min_axis_ratio;
    uint32_t num_kept, wgs_per_row, samples_per_gaussian;
};
static_assert(sizeof(MeshPointCloudParams) == 8 * 8 + 4 * 4, "layout");

struct MeshBvhPrepParams {
    uint64_t kept, mean, ax0, ax1, ax2, invs2, k2, leafMin, leafMax, morton,
        iota;
    float remap_min_x, remap_min_y, remap_min_z;
    float remap_ext_x, remap_ext_y, remap_ext_z;
    float rel_scale;
    uint32_t num_kept, wgs_per_row;
};
static_assert(sizeof(MeshBvhPrepParams) == 11 * 8 + 9 * 4 + 4 /*pad*/,
              "layout");

struct MeshLbvhNodesParams {
    uint64_t morton, argsort, internal, parent;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(MeshLbvhNodesParams) == 4 * 8 + 2 * 4, "layout");

struct MeshLbvhInitAabbParams {
    uint64_t nodeAABB;
    uint32_t n_internal, wgs_per_row;
};
static_assert(sizeof(MeshLbvhInitAabbParams) == 8 + 2 * 4, "layout");

struct MeshLbvhAabbParams {
    uint64_t internal, parent, leafMin, leafMax, nodeAABB;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(MeshLbvhAabbParams) == 5 * 8 + 2 * 4, "layout");

struct MeshUnmapParams {
    uint64_t words;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(MeshUnmapParams) == 8 + 2 * 4, "layout");

struct MeshOccParams {
    MESH_SCENE_PTRS
    uint64_t pts, occ;
    MESH_SCENE_SCALARS
    uint32_t n, dynamic, wgs_per_row;
};
static_assert(sizeof(MeshOccParams) == kSceneP + 2 * 8 + kSceneS + 3 * 4,
              "layout");

struct MeshBisectParams {
    MESH_SCENE_PTRS
    uint64_t cloud, ea, eb, oa, ob, out_xyz;
    MESH_SCENE_SCALARS
    uint32_t n, iters, dynamic, wgs_per_row;
};
// ... + 4 bytes of trailing padding to the struct's 8-byte alignment. Only
// the field OFFSETS have to match the shader; a longer tail is harmless (the
// ring copy just carries it).
static_assert(sizeof(MeshBisectParams) == kSceneP + 6 * 8 + kSceneS + 4 * 4 + 4,
              "layout");

struct MeshColorizeParams {
    MESH_SCENE_PTRS
    uint64_t verts, rgb;
    MESH_SCENE_SCALARS
    uint32_t n, fallback_only, wgs_per_row;
};
static_assert(sizeof(MeshColorizeParams) == kSceneP + 2 * 8 + kSceneS + 3 * 4,
              "layout");

struct MeshOccCombineParams {
    uint64_t occ, occ_static;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(MeshOccCombineParams) == 2 * 8 + 2 * 4, "layout");

// --- meshing_raster.slang ---

struct MeshSampleOccParams {
    uint64_t xyz, viewmat, intrin, dist, moments, acc;
    uint32_t n, W, H, tsdf;
    float iso, band_px, deep_weight, clean;
    uint32_t wgs_per_row, _pad0;
};
static_assert(sizeof(MeshSampleOccParams) == 6 * 8 + 10 * 4, "layout");

struct MeshFinalizeOccParams {
    uint64_t acc, occ;
    uint32_t n, tsdf;
    float iso, clean;
    uint32_t wgs_per_row, _pad0;
};
static_assert(sizeof(MeshFinalizeOccParams) == 2 * 8 + 6 * 4, "layout");

struct MeshSampleColorParams {
    uint64_t xyz, viewmat, intrin, dist, moments, rgb_img, num, den;
    uint32_t n, W, H, wgs_per_row;
};
static_assert(sizeof(MeshSampleColorParams) == 8 * 8 + 4 * 4, "layout");

struct MeshFinalizeColorParams {
    uint64_t num, den, rgb;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(MeshFinalizeColorParams) == 3 * 8 + 2 * 4, "layout");

struct MeshViewDensityParams {
    uint64_t xyz, viewmat, intrin, dist, moments, dens;
    uint32_t n, W, H, wgs_per_row;
};
static_assert(sizeof(MeshViewDensityParams) == 6 * 8 + 4 * 4, "layout");

struct MeshTriPrepParams {
    uint64_t faces, verts, leafMin, leafMax, morton, iota;
    float bmin_x, bmin_y, bmin_z, ext_x, ext_y, ext_z;
    uint32_t nf, wgs_per_row;
};
static_assert(sizeof(MeshTriPrepParams) == 6 * 8 + 8 * 4, "layout");

struct MeshCullParams {
    uint64_t verts, faces, viewmats, intrins, dist, Ws, Hs, leafMin, leafMax,
        internal, nodeAABB, visible;
    uint32_t first, count, nf, C, wgs_per_row, _pad0;
};
static_assert(sizeof(MeshCullParams) == 12 * 8 + 6 * 4, "layout");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Every scene pointer is loaded through unconditionally by some traversal
// path, and a device address a shader may load from is never null here (see
// KernelCommon.h's null_fallback rationale): a scene with < 2 kept Gaussians
// has no internal/nodeAABB buffers, and the < 2 branch is a runtime one.
template <typename P>
void fill_scene(P& p, const meshing::GpuScene& s) {
    p.sc_mean = vkk::or_fallback(s.mean);
    p.sc_ax0 = vkk::or_fallback(s.ax0);
    p.sc_ax1 = vkk::or_fallback(s.ax1);
    p.sc_ax2 = vkk::or_fallback(s.ax2);
    p.sc_invs2 = vkk::or_fallback(s.invs2);
    p.sc_opac = vkk::or_fallback(s.opac);
    p.sc_k2 = vkk::or_fallback(s.k2);
    p.sc_gcol = vkk::or_fallback(s.gcol);
    p.sc_kept = vkk::or_fallback(s.kept);
    p.sc_leafMin = vkk::or_fallback(s.leafMin);
    p.sc_leafMax = vkk::or_fallback(s.leafMax);
    p.sc_internal = vkk::or_fallback(s.internal);
    p.sc_nodeAABB = vkk::or_fallback(s.nodeAABB);
    p.sc_campos = vkk::or_fallback(s.campos);
    p.sc_num_kept = s.num_kept;
    p.sc_num_cameras = s.num_cameras;
    p.sc_iso = s.iso;
}

// Spec IDs for meshing_raster.slang: 0 = camera model, 1 = distortion tier.
backend::vk::SpecList camera_spec(int camera_model, int distortion) {
    if (camera_model < 0 || camera_model > 3 || distortion < 0 ||
        distortion > 3)
        throw std::runtime_error(
            "meshing: unsupported camera model / distortion tier");
    return backend::vk::SpecList{(uint32_t)camera_model, (uint32_t)distortion};
}

// Ring dispatch of a flat 1D range (the params structs above the push floor).
void dispatch_flat_ring(const char* entry, const backend::vk::SpecList& spec,
                        int64_t total, uint32_t block, void* params,
                        uint32_t size, uint32_t* wgs_per_row_field) {
    if (total <= 0) return;
    vkk::Fold f = vkk::fold_1d(total, block);
    *wgs_per_row_field = f.per_row;
    vkk::dispatch_ring(entry, spec, f.per_row, f.rows, 1, params, size);
}

}  // namespace

namespace meshing {

// ===========================================================================
// Gaussian preparation, point cloud, LBVH build
// ===========================================================================

void launch_activate(
    int N,
    const float* means, const float* quats, const float* logsc,
    const float* logit, const float* fdc,
    float3* mean, float3* ax0, float3* ax1, float3* ax2, float3* invs2,
    float* opac, float* radius, float* k2, int* valid, float3* gcol
) {
    if (N <= 0) return;
    MeshActivateParams p{};
    p.means = (uint64_t)means;
    p.quats = (uint64_t)quats;
    p.logsc = (uint64_t)logsc;
    p.logit = (uint64_t)logit;
    p.fdc = (uint64_t)fdc;
    p.mean = (uint64_t)mean;
    p.ax0 = (uint64_t)ax0;
    p.ax1 = (uint64_t)ax1;
    p.ax2 = (uint64_t)ax2;
    p.invs2 = (uint64_t)invs2;
    p.opac = (uint64_t)opac;
    p.radius = (uint64_t)radius;
    p.k2 = (uint64_t)k2;
    p.valid = (uint64_t)valid;
    p.gcol = (uint64_t)gcol;
    p.N = (uint32_t)N;
    dispatch_flat_ring("meshing.mesh_activate", {}, N, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_pointcloud(
    int num_kept, const int* kept,
    const float3* mean, const float3* ax0, const float3* ax1,
    const float3* ax2, const float3* invs2, const float* k2,
    float min_axis_ratio, int samples_per_gaussian, float* out
) {
    if (num_kept <= 0) return;
    MeshPointCloudParams p{};
    p.kept = (uint64_t)kept;
    p.mean = (uint64_t)mean;
    p.ax0 = (uint64_t)ax0;
    p.ax1 = (uint64_t)ax1;
    p.ax2 = (uint64_t)ax2;
    p.invs2 = (uint64_t)invs2;
    p.k2 = (uint64_t)k2;
    p.out_xyz = (uint64_t)out;
    p.min_axis_ratio = min_axis_ratio;
    p.num_kept = (uint32_t)num_kept;
    p.samples_per_gaussian = (uint32_t)samples_per_gaussian;
    dispatch_flat_ring("meshing.mesh_pointcloud", {}, num_kept, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_bvh_prep(
    int num_kept, const int* kept,
    const float3* mean, const float3* ax0, const float3* ax1,
    const float3* ax2, const float3* invs2, const float* k2,
    float3 remap_min, float3 remap_inv_ext, float rel_scale,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota
) {
    if (num_kept <= 0) return;
    MeshBvhPrepParams p{};
    p.kept = (uint64_t)kept;
    p.mean = (uint64_t)mean;
    p.ax0 = (uint64_t)ax0;
    p.ax1 = (uint64_t)ax1;
    p.ax2 = (uint64_t)ax2;
    p.invs2 = (uint64_t)invs2;
    p.k2 = (uint64_t)k2;
    p.leafMin = (uint64_t)leafMin;
    p.leafMax = (uint64_t)leafMax;
    p.morton = (uint64_t)morton;
    p.iota = (uint64_t)iota;
    p.remap_min_x = remap_min.x;
    p.remap_min_y = remap_min.y;
    p.remap_min_z = remap_min.z;
    p.remap_ext_x = remap_inv_ext.x;
    p.remap_ext_y = remap_inv_ext.y;
    p.remap_ext_z = remap_inv_ext.z;
    p.rel_scale = rel_scale;
    p.num_kept = (uint32_t)num_kept;
    dispatch_flat_ring("meshing.mesh_bvh_prep", {}, num_kept, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_lbvh_internal(
    int n, const uint64_t* morton_sorted, const int* argsort,
    int2* internal, int* parent
) {
    if (n < 2) return;
    MeshLbvhNodesParams p{};
    p.morton = (uint64_t)morton_sorted;
    p.argsort = (uint64_t)argsort;
    p.internal = (uint64_t)internal;
    p.parent = (uint64_t)parent;
    p.n = (uint32_t)n;
    vkk::dispatch_flat("meshing.mesh_lbvh_internal", {}, n - 1, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_lbvh_init_aabb(int n_internal, float3* nodeAABB) {
    if (n_internal <= 0) return;
    // The build runs in fmap_ordered u32 space; this seeds it with the mapped
    // image of (+1e30, -1e30), matching the CUDA kernel's float init.
    MeshLbvhInitAabbParams p{};
    p.nodeAABB = (uint64_t)nodeAABB;
    p.n_internal = (uint32_t)n_internal;
    vkk::dispatch_flat("meshing.mesh_lbvh_init_aabb", {}, n_internal, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_lbvh_aabb(
    int n, const int2* internal, const int* parent,
    const float3* leafMin, const float3* leafMax, float3* nodeAABB
) {
    if (n < 2) return;
    MeshLbvhAabbParams p{};
    p.internal = (uint64_t)internal;
    p.parent = (uint64_t)parent;
    p.leafMin = (uint64_t)leafMin;
    p.leafMax = (uint64_t)leafMax;
    p.nodeAABB = (uint64_t)nodeAABB;
    p.n = (uint32_t)n;
    vkk::dispatch_flat("meshing.mesh_lbvh_aabb", {}, n - 1, 256, &p, sizeof(p),
                       &p.wgs_per_row);

    // Back to plain float bits for every consumer. A separate dispatch, so the
    // backend's post-dispatch barrier orders it after the whole merge.
    MeshUnmapParams u{};
    u.words = (uint64_t)nodeAABB;
    u.n = (uint32_t)(2 * (n - 1) * 3);
    vkk::dispatch_flat("meshing.mesh_lbvh_unmap_aabb", {}, u.n, 256, &u,
                       sizeof(u), &u.wgs_per_row);
}

// ===========================================================================
// Occupancy field / bisection / vertex color
// ===========================================================================

void launch_occ(const GpuScene& s, const float* pts, int n, float* occ,
                int dynamic) {
    if (n <= 0) return;
    MeshOccParams p{};
    fill_scene(p, s);
    p.pts = (uint64_t)pts;
    p.occ = (uint64_t)occ;
    p.n = (uint32_t)n;
    p.dynamic = dynamic ? 1u : 0u;
    dispatch_flat_ring("meshing.mesh_occ", {}, n, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_bisect(
    const GpuScene& s, const float* cloud,
    const int* ea, const int* eb, const float* oa, const float* ob,
    int n, int iters, int dynamic, float* out
) {
    if (n <= 0) return;
    MeshBisectParams p{};
    fill_scene(p, s);
    p.cloud = (uint64_t)cloud;
    p.ea = (uint64_t)ea;
    p.eb = (uint64_t)eb;
    p.oa = (uint64_t)oa;
    p.ob = (uint64_t)ob;
    p.out_xyz = (uint64_t)out;
    p.n = (uint32_t)n;
    p.iters = (uint32_t)std::max(iters, 0);
    p.dynamic = dynamic ? 1u : 0u;
    dispatch_flat_ring("meshing.mesh_bisect", {}, n, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_colorize(const GpuScene& s, const float* verts, int n, float* rgb,
                     int dynamic) {
    // `dynamic` selected Meshing.cu's color_camera, whose call site is
    // `dynamic && false` -- dead on both backends. See meshing.slang.
    (void)dynamic;
    if (n <= 0) return;
    MeshColorizeParams p{};
    fill_scene(p, s);
    p.verts = (uint64_t)verts;
    p.rgb = (uint64_t)rgb;
    p.n = (uint32_t)n;
    p.fallback_only = 0u;
    dispatch_flat_ring("meshing.mesh_colorize", {}, n, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_colorize_fallback(const GpuScene& s, const float* verts, int n,
                              float* rgb) {
    if (n <= 0) return;
    MeshColorizeParams p{};
    fill_scene(p, s);
    p.verts = (uint64_t)verts;
    p.rgb = (uint64_t)rgb;
    p.n = (uint32_t)n;
    p.fallback_only = 1u;
    dispatch_flat_ring("meshing.mesh_colorize", {}, n, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_occ_combine(int n, float* occ, const float* occ_static) {
    if (n <= 0) return;
    MeshOccCombineParams p{};
    p.occ = (uint64_t)occ;
    p.occ_static = (uint64_t)occ_static;
    p.n = (uint32_t)n;
    vkk::dispatch_flat("meshing.mesh_occ_combine", {}, n, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}

// ===========================================================================
// Rasterize-and-sample driver kernels
// ===========================================================================

void launch_sample_occ(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, int W, int H, const OccFusion& f, float4* acc
) {
    if (n <= 0) return;
    MeshSampleOccParams p{};
    p.xyz = (uint64_t)xyz;
    p.viewmat = (uint64_t)viewmat;
    p.intrin = (uint64_t)intrin;
    p.dist = vkk::or_fallback(dist);
    p.moments = (uint64_t)moments;
    p.acc = (uint64_t)acc;
    p.n = (uint32_t)n;
    p.W = (uint32_t)W;
    p.H = (uint32_t)H;
    p.tsdf = (uint32_t)f.tsdf;
    p.iso = f.iso;
    p.band_px = f.band_px;
    p.deep_weight = f.deep_weight;
    p.clean = f.clean;
    vkk::dispatch_flat("meshing_raster.mesh_sample_occ",
                       camera_spec(camera_model, distortion), n, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_finalize_occ(int n, const float4* acc, const OccFusion& f, float* occ) {
    if (n <= 0) return;
    MeshFinalizeOccParams p{};
    p.acc = (uint64_t)acc;
    p.occ = (uint64_t)occ;
    p.n = (uint32_t)n;
    p.tsdf = (uint32_t)f.tsdf;
    p.iso = f.iso;
    p.clean = f.clean;
    vkk::dispatch_flat("meshing_raster.mesh_finalize_occ", camera_spec(0, 0), n,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

void launch_sample_color(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, const float3* rgb_img,
    int W, int H, float3* num, float* den
) {
    if (n <= 0) return;
    MeshSampleColorParams p{};
    p.xyz = (uint64_t)xyz;
    p.viewmat = (uint64_t)viewmat;
    p.intrin = (uint64_t)intrin;
    p.dist = vkk::or_fallback(dist);
    p.moments = (uint64_t)moments;
    p.rgb_img = (uint64_t)rgb_img;
    p.num = (uint64_t)num;
    p.den = (uint64_t)den;
    p.n = (uint32_t)n;
    p.W = (uint32_t)W;
    p.H = (uint32_t)H;
    vkk::dispatch_flat("meshing_raster.mesh_sample_color",
                       camera_spec(camera_model, distortion), n, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_finalize_color(int n, const float3* num, const float* den,
                           float* rgb) {
    if (n <= 0) return;
    MeshFinalizeColorParams p{};
    p.num = (uint64_t)num;
    p.den = (uint64_t)den;
    p.rgb = (uint64_t)rgb;
    p.n = (uint32_t)n;
    vkk::dispatch_flat("meshing_raster.mesh_finalize_color",
                       camera_spec(0, 0), n, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_sample_view_density(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, int W, int H, float* dens
) {
    if (n <= 0) return;
    MeshViewDensityParams p{};
    p.xyz = (uint64_t)xyz;
    p.viewmat = (uint64_t)viewmat;
    p.intrin = (uint64_t)intrin;
    p.dist = vkk::or_fallback(dist);
    p.moments = (uint64_t)moments;
    p.dens = (uint64_t)dens;
    p.n = (uint32_t)n;
    p.W = (uint32_t)W;
    p.H = (uint32_t)H;
    vkk::dispatch_flat("meshing_raster.mesh_sample_view_density",
                       camera_spec(camera_model, distortion), n, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_tri_prep(
    int nf, const int* faces, const float* verts,
    float3 bmin, float3 inv_ext,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota
) {
    if (nf <= 0) return;
    MeshTriPrepParams p{};
    p.faces = (uint64_t)faces;
    p.verts = (uint64_t)verts;
    p.leafMin = (uint64_t)leafMin;
    p.leafMax = (uint64_t)leafMax;
    p.morton = (uint64_t)morton;
    p.iota = (uint64_t)iota;
    p.bmin_x = bmin.x;
    p.bmin_y = bmin.y;
    p.bmin_z = bmin.z;
    p.ext_x = inv_ext.x;
    p.ext_y = inv_ext.y;
    p.ext_z = inv_ext.z;
    p.nf = (uint32_t)nf;
    vkk::dispatch_flat("meshing_raster.mesh_tri_prep", camera_spec(0, 0), nf,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

void launch_cull(
    const float* verts, int first, int count, const int* faces, int nf,
    const float* viewmats, const float* intrins, const float* dist,
    const int* Ws, const int* Hs, int camera_model, int distortion, int C,
    const float3* leafMin, const float3* leafMax,
    const int2* internal, const float3* nodeAABB,
    uint32_t* visible
) {
    if (count <= 0) return;
    MeshCullParams p{};
    p.verts = (uint64_t)verts;
    // A mesh with no faces still runs the projection test; nf == 0 short-
    // circuits every triangle read, but the address must not be null.
    p.faces = vkk::or_fallback(faces);
    p.viewmats = (uint64_t)viewmats;
    p.intrins = (uint64_t)intrins;
    p.dist = vkk::or_fallback(dist);
    p.Ws = (uint64_t)Ws;
    p.Hs = (uint64_t)Hs;
    p.leafMin = vkk::or_fallback(leafMin);
    p.leafMax = vkk::or_fallback(leafMax);
    p.internal = vkk::or_fallback(internal);
    p.nodeAABB = vkk::or_fallback(nodeAABB);
    p.visible = (uint64_t)visible;
    p.first = (uint32_t)first;
    p.count = (uint32_t)count;
    p.nf = (uint32_t)nf;
    p.C = (uint32_t)C;
    dispatch_flat_ring("meshing_raster.mesh_cull",
                       camera_spec(camera_model, distortion),
                       count, 256, &p, sizeof(p), &p.wgs_per_row);
}

}  // namespace meshing
