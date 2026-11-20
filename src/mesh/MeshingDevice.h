#pragma once

/*
 * MeshingDevice.h
 *
 * The meshing pipeline's DEVICE SEAM: every GPU kernel the meshing pipeline
 * launches, declared backend-neutrally so the host orchestration compiles
 * once and runs on either backend.
 *
 *   host  : mesh/OccupancyEvaluator.cpp   (the Meshing.h object)
 *           mesh/MeshingRasterHost.cpp    (the per-camera render driver)
 *   CUDA  : mesh/Meshing.cu + mesh/MeshingRaster.cu
 *   Vulkan: backend/vulkan/kernels/Meshing.cpp
 *           (+ shaders/meshing.slang, shaders/meshing_raster.slang)
 *
 * Same contract as src/backend/api/: this header must parse under
 * -DSS_BACKEND_VULKAN, so it stays CUDA-include-free. The camera model
 * travels as a plain int (CameraModelType's value) rather than the enum, and
 * so does the distortion tier (CameraDistortionType's value), because those
 * enums have two spellings -- Common.cuh's (CUDA translation units) and
 * CameraModel.h's (portable ones) -- and no translation unit may see both.
 *
 * Every launch below is asynchronous on the default stream unless noted; the
 * host synchronizes where it reads results back.
 */

#include <cmath>
#include <cstdint>

#include "backend/api/BackendTypes.h"

namespace meshing {

// ---------------------------------------------------------------------------
// Device-side view of the whole scene (Gaussians + LBVH + cameras). POD: the
// CUDA kernels take it by value, the Vulkan launcher flattens it into the
// kernel params block. Layout is host-side only -- neither backend reads it
// from a buffer -- so member order is free.
// ---------------------------------------------------------------------------
struct GpuScene {
    // Gaussian SoA, indexed by ORIGINAL splat id [0, N)
    const float3* __restrict__ mean = nullptr;
    const float3* __restrict__ ax0 = nullptr;  // unit principal axes (cols of R)
    const float3* __restrict__ ax1 = nullptr;
    const float3* __restrict__ ax2 = nullptr;
    const float3* __restrict__ invs2 = nullptr;  // 1 / sigma^2 per axis
    const float*  __restrict__ opac = nullptr;   // sigmoid opacity
    const float*  __restrict__ k2 = nullptr;     // 2 ln(opac / ALPHA): cutoff^2
    const float3* __restrict__ gcol = nullptr;   // base RGB from SH DC, [0,1]

    // LBVH over kept Gaussians (leaf index = "kept position" kp)
    const int*    __restrict__ kept = nullptr;     // [num_kept] kp -> orig id
    const float3* __restrict__ leafMin = nullptr;  // [num_kept]
    const float3* __restrict__ leafMax = nullptr;  // [num_kept]
    const int2*   __restrict__ internal = nullptr;  // [num_kept-1] child links
    const float3* __restrict__ nodeAABB = nullptr;  // [2*(num_kept-1)] min,max
    int num_kept = 0;

    // cameras
    const float3* __restrict__ campos = nullptr;
    int num_cameras = 0;

    float iso = 0.5f;
};

// Coordinate remap: identity near the origin, ~ x^(1/k) for large |x|. Applied
// only to Morton ordering so a few distant splats don't starve the near-origin
// region of spatial resolution. HOST mirror of the device function in
// Meshing.cu (remap_coord_dev) and meshing.slang (remap_coord) -- the host
// needs it to remap the root bounds the codes are normalized against, and the
// three must agree exactly.
inline float remap_coord(float x, float rel_scale) {
    const float k = 2.5f;
    return k * std::sinh((1.0f / k) * std::asinh(x / rel_scale)) * rel_scale;
}

// ===========================================================================
// Gaussian preparation, point cloud, LBVH build  (mesh/Meshing.cu)
// ===========================================================================

// Activate the raw splat parameters: quaternion -> principal axes, log-scale
// -> 1/sigma^2, logit -> opacity, SH DC -> base RGB, plus the Mahalanobis
// cutoff k2, a bounding radius, and the keep flag (opacity above threshold).
void launch_activate(
    int N,
    const float* means, const float* quats, const float* logsc,
    const float* logit, const float* fdc,
    float3* mean, float3* ax0, float3* ax1, float3* ax2, float3* invs2,
    float* opac, float* radius, float* k2, int* valid, float3* gcol);

// Floor on each axis's sample offset, as a share of the largest. Without it a
// 2dgs disc (third sigma e^-20) puts both normal samples on its centre, which
// spikes and holes flat surfaces; 0.1 doubles a cube of discs (docs/notes/2dgs.md).
constexpr float kSampleMinAxisRatio = 0.25f;

// 7 points per kept Gaussian (center + +/-k sigma along each principal axis,
// each sigma at least min_axis_ratio * the largest), interleaved xyz into
// out[num_kept * 21].
void launch_pointcloud(
    int num_kept, const int* kept,
    const float3* mean, const float3* ax0, const float3* ax1,
    const float3* ax2, const float3* invs2, const float* k2,
    float min_axis_ratio, float* out);

// Per kept Gaussian: leaf AABB (real space) + remapped Morton code + iota.
void launch_bvh_prep(
    int num_kept, const int* kept,
    const float3* mean, const float3* ax0, const float3* ax1,
    const float3* ax2, const float3* invs2, const float* k2,
    float3 remap_min, float3 remap_inv_ext, float rel_scale,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota);

// Karras 2012 single-level radix tree over the sorted Morton array. Shared by
// the Gaussian LBVH and the mesh-triangle LBVH (identical tree, different
// leaves). `parent` must be pre-filled with -1 (0xff bytes).
void launch_lbvh_internal(
    int n, const uint64_t* morton_sorted, const int* argsort,
    int2* internal, int* parent);

// Seed the [2*n_internal] (min,max) node AABBs with an inverted box.
void launch_lbvh_init_aabb(int n_internal, float3* nodeAABB);

// Bottom-up node AABB merge over the radix tree (atomic min/max walk to root).
void launch_lbvh_aabb(
    int n, const int2* internal, const int* parent,
    const float3* leafMin, const float3* leafMax, float3* nodeAABB);

// ===========================================================================
// Occupancy field / bisection / vertex color  (mesh/Meshing.cu)
// ===========================================================================

// occ[i] = occupancy at pts[3i..3i+2]; `dynamic` selects the camera-segment
// (visual hull) field over the static density aggregation.
void launch_occ(const GpuScene& s, const float* pts, int n, float* occ,
                int dynamic);

// Per cut edge (a,b) into the point cloud, bisect toward the iso crossing and
// finish with a linear interpolation; writes out[n*3].
void launch_bisect(
    const GpuScene& s, const float* cloud,
    const int* ea, const int* eb, const float* oa, const float* ob,
    int n, int iters, int dynamic, float* out);

// Per-vertex RGB from the splats' DC color.
void launch_colorize(const GpuScene& s, const float* verts, int n, float* rgb,
                     int dynamic);

// occ = 1 - (1 - occ_static)(1 - occ): combine the render occlusion term with
// the view-independent density term.
void launch_occ_combine(int n, float* occ, const float* occ_static);

// Fill in verts the render color path left negative, from the static color.
void launch_colorize_fallback(const GpuScene& s, const float* verts, int n,
                              float* rgb);

// ===========================================================================
// Rasterize-and-sample driver kernels  (mesh/MeshingRaster.cu)
// ===========================================================================

// How one camera's view of a point is folded in (docs/notes/mesh-field.md).
// Plain scalars: the Vulkan launcher copies them into its params block.
struct OccFusion {
    int   tsdf = 0;            // 1: mean truncated signed distance, not the vote
    float iso = 0.5f;          // the field's level; a camera's surface is where it reaches it
    float band_px = 6.0f;      // pixel footprints behind its surface a camera still votes fully
    float deep_weight = 0.01f; // its vote for points deeper than twice that
    float clean = 0.2f;        // occupancy under which one camera's sight carves alone
};

// Folds one camera's view of each query point into acc[n], which starts zeroed:
// its occupancy by the rendered moments, or its signed distance under f.tsdf,
// each weighted by how sharply the camera sees the point.
void launch_sample_occ(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, int W, int H, const OccFusion& f, float4* acc);

// occ[i] from the folded views; 0 for points no camera saw.
void launch_finalize_occ(int n, const float4* acc, const OccFusion& f, float* occ);

// Accumulate transmittance-weighted rendered color for one camera.
void launch_sample_color(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, const float3* rgb_img,
    int W, int H, float3* num, float* den);

// rgb = num/den, or (-1,-1,-1) when no view contributed.
void launch_finalize_color(int n, const float3* num, const float* den,
                           float* rgb);

// dens[i] = max over cameras that see the point of focal_px / depth.
void launch_sample_view_density(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, int W, int H, float* dens);

// Per mesh triangle: leaf AABB + centroid Morton code + iota (the LBVH build
// then reuses launch_lbvh_* above).
void launch_tri_prep(
    int nf, const int* faces, const float* verts,
    float3 bmin, float3 inv_ext,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota);

// Sets visible[i] = 1 for each i in [first, first+count) one of the C cameras
// sees unoccluded and leaves the rest, so launches accumulate into one zeroed
// buffer. uint32, not bytes: sub-word stores need a Vulkan device capability.
void launch_cull(
    const float* verts, int first, int count, const int* faces, int nf,
    const float* viewmats, const float* intrins, const float* dist,
    const int* Ws, const int* Hs, int camera_model, int distortion, int C,
    const float3* leafMin, const float3* leafMax,
    const int2* internal, const float3* nodeAABB,
    uint32_t* visible);

// device_synchronize(), then throw if the backend recorded an error. Vulkan
// reports a lost device only that way; unchecked, readbacks return garbage.
void sync_checked(const char* stage);

}  // namespace meshing
