#pragma once

/*
 * MeshingRaster.h
 *
 * Rasterize-and-sample render driver for the meshing pipeline. Holds device
 * copies of the (raw) splat parameters and the camera intrinsics, and renders
 * one camera at a time via the existing 3DGUT projection + tile intersection +
 * (moment / color) rasterization. The per-camera render replaces the
 * per-point/per-camera LBVH traversal on the dataset path.
 *
 * Opaque RenderContext so OccupancyEvaluator.cpp (which owns the evaluator)
 * can drive rendering without pulling in the heavy Tensor / projection
 * headers.
 *
 * The driver itself (mesh/MeshingRasterHost.cpp) is portable; its per-point
 * sampling kernels are declared in mesh/MeshingDevice.h and implemented per
 * backend.
 */

#include <cstdint>

namespace meshing {

struct RenderContext;
struct OccFusion;   // mesh/MeshingDevice.h

// All pointers are HOST arrays, uploaded here, laid out as Meshing.h's
// CameraParams; the splat params are raw (un-activated). `verbose` reports
// progress per camera, as each render loop covers every selected camera.
RenderContext* render_context_create(
    const float* means, const float* quats, const float* log_scales,
    const float* logit_opac, const float* features_dc, int num_splats,
    const float* viewmats, const float* intrins, const float* dist,
    int num_cameras, const int* widths, const int* heights,
    const int* camera_models, const int* distortions,
    const OccFusion& fusion, bool verbose);

void render_context_destroy(RenderContext*);

int render_context_width(const RenderContext*, int cam_idx);
int render_context_height(const RenderContext*, int cam_idx);
int render_context_num_cameras(const RenderContext*);

// Render camera `cam_idx`'s occupancy moments into `d_moments` (device,
// [width*height] float3 = (m0, mean_depth, std_depth)). Caller owns d_moments.
void render_camera_moments(RenderContext*, int cam_idx, void* d_moments);

// Occupancy at device query points d_xyz [n*3] into d_occ [n] (device): each
// camera in cam_indices is rendered once and its view of every point fused as
// the context's OccFusion says. Points no camera saw are free (0).
void render_evaluate_occupancy(
    RenderContext*, const int* cam_indices, int num_cams,
    const float* d_xyz, int n, float* d_occ);

// Evaluate vertex color at device query points d_xyz [n*3], writing d_rgb [n*3]
// (device, RGB in [0,1]). Per camera, renders the full-ray DC color + moments,
// samples color (bilinear) weighted by transmittance T(z) until the point, and
// averages over cameras. Points with no usable view get rgb = (-1,-1,-1) so the
// caller can fall back to the static density-weighted color.
void render_evaluate_color(
    RenderContext*, const int* cam_indices, int num_cams,
    const float* d_xyz, int n, float* d_rgb);

// Evaluate the observed screen-space resolution at device query points d_xyz
// [n*3]: d_dens [n] receives max over the cameras that see the point (frustum
// + moment-based occlusion test) of focal_px / depth, i.e. pixels per world
// unit. Points seen by no camera get 0.
void render_evaluate_view_density(
    RenderContext*, const int* cam_indices, int num_cams,
    const float* d_xyz, int n, float* d_dens);

// The three above in one pass over the cameras, each rendered once: view
// density and (with d_rgb) colour at d_verts [nv*3], occupancy at d_probes
// [np*3]. Same outputs as the separate calls.
void render_evaluate_merge(
    RenderContext*, const int* cam_indices, int num_cams,
    const float* d_verts, int nv, float* d_dens, float* d_rgb,
    const float* d_probes, int np, float* d_occ);

// Visibility cull: visible[v]=1 iff some cam in cam_indices[0..num_cams) (or
// all when null/<=0) sees verts[v] unoccluded by other faces (HOST arrays).
void render_cull_unseen_vertices(
    RenderContext*, const int* cam_indices, int num_cams,
    const float* verts, int nv, const int* faces, int nf, unsigned char* visible);

} // namespace meshing
