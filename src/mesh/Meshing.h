/*
 * Meshing.h
 *
 * Surface mesh extraction from a trained splat model. The opacity field:
 * samples around every splat (launch_pointcloud, mesh/MeshingDevice.h), their
 * Delaunay tetrahedralization, marching tetrahedra at `iso` with every cut
 * edge bisected, then merge, cleanup and export (mesh/MeshingHost.cpp). TSDF
 * fusion is the other method (docs/notes/tsdf.md). OccupancyEvaluator owns the
 * device buffers and answers every field query.
 */
#ifndef SPIRULA_MESHING_H
#define SPIRULA_MESHING_H

#include <cstdint>
#include <string>
#include <vector>

#include "mesh/MeshExport.h"   // MeshColorMode + format helpers
#include "mesh/MeshProgress.h"

namespace meshing {

// Opacity: the opacity field over a Delaunay of splat samples. Tsdf: per-camera
// depth fused into a truncated signed distance, over the same samples or a
// uniform grid (docs/notes/tsdf.md); it needs cameras.
enum class MeshMethod { Opacity, Tsdf };

struct MeshingConfig {
    MeshMethod method = MeshMethod::Opacity;
    float tsdf_voxel = 0.0f;      // world units of a uniform grid; 0: adaptive, the splat samples
    float tsdf_trunc = 4.0f;      // voxels; pixel footprints when adaptive
    float tsdf_max_depth = 0.0f;  // world units; 0: no cap
    float iso = 0.5f;            // isosurface level on occupancy in [0,1]
    float merge_factor = 1.0f;   // scales every merge limit below; <=0 disables merging
    float merge_flat_px = 1.0f;  // flatten bumps under this many pixels of footprint
    float merge_thin = 0.1f;     // thin parts move by at most this share of their thickness
    float merge_max_edge_px = 8.0f;   // longest edge a merge leaves, in pixels
    float merge_color = 0.05f;   // largest colour range (0-1 per channel) one vertex may merge; 0: any
    int   bisection_iters = 3;  // bisection steps per cut edge; a linear
                                // interpolation finishes the crossing estimate
    int   max_cameras = -1;      // cap on cameras used (k-means subset)
    int   max_grid_res = 512;    // per-axis cap on the acceleration grid
    float grid_cell_factor = 2.0f; // cell size = factor * mean Gaussian radius
    int   num_threads = 0;       // 0 = all hardware threads (Delaunay + host)
    bool  verbose = true;
    int   check = 0;             // MeshCheck.h: 1 the final mesh, 2 also after every stage

    // A merge collapse that turns a reshaped face's normal by more than this
    // many degrees is skipped; <=0 or >=180 disables the guard.
    float merge_max_flip_deg = 60.0f;

    // --- post-merge cleanup ---
    // Floater removal: drop connected components with fewer than this many
    // faces. <=1 disables.
    int   floater_min_faces = 100;

    // Narrow/degenerate face repair: any triangle whose smallest angle is below
    // this many degrees is removed by a fidelity-preserving local edit (collapse
    // the short edge of a needle, or flip the long edge of a cap). <=0 disables.
    float degenerate_angle_deg = 2.0f;

    // Valence-optimizing edge flips + guarded tangential smoothing iterations
    // (improves triangle angles/valences without moving the surface to first
    // order). <=0 disables.
    int   quality_iters = 3;

    // --- rasterize-and-sample occupancy/color (dataset path) ---
    // When camera intrinsics are supplied, occupancy/color come from rendering
    // each camera once (3DGUT) and sampling, instead of LBVH ray traversal.
    int   render_width = 0;      // 0 = use dataset-native width
    int   render_height = 0;     // 0 = use dataset-native height
    // A boundary loop is filled when EITHER criterion holds (<=0 disables that
    // one): its bbox diagonal is < fill_hole_ratio * its connected component's
    // bbox diagonal, OR it has <= fill_hole_max_edges edges (so tiny holes always
    // fill). Larger holes (sky / large unseen regions) stay open.
    float fill_hole_ratio = 0.05f;
    int   fill_hole_max_edges = 12;
    bool  cull_unseen = true;    // drop final mesh verts seen by no camera

    // --- how the cameras' views of a point are fused (docs/notes/mesh-field.md) ---
    float occ_clean = 0.2f;        // occupancy under which one camera's sight carves alone
    float occ_band_px = 6.0f;      // pixel footprints behind its surface a camera still votes fully
    float occ_deep_weight = 0.01f; // its vote for points deeper than twice that

    // --- export ---
    // None, per-vertex color, or a baked texture atlas (MeshUV.h). More than
    // one may be asked for: one extraction, written in each (MeshExport.h).
    std::vector<MeshColorMode> colors = {MeshColorMode::Vertex};
    std::vector<std::string> formats = {"ply"};   // any of ply obj gltf glb stl
    int   texture_size = 0;         // square texture atlas resolution;
                                    // 0 = auto from the observed-detail texel
                                    // budget (power of two in [1024, 8192])
    int   tex_gutter_px = 4;        // atlas spacing between charts (texels)
    float chart_angle_deg = 60.0f;  // max normal deviation within a UV chart

    // Told at every stage how far the run is and the memory it expects.
    MeshProgressSink on_progress;
};

// Full camera intrinsics/extrinsics for the rasterize-and-sample (dataset)
// path. All host pointers; when !valid() the evaluator falls back to the static
// LBVH path. Coefficient rows are 8 wide; the per-tier order is documented in
// core/CameraModel.h.
struct CameraParams {
    const float* viewmats = nullptr;    // [C*16] row-major world->cam 4x4
    const float* intrins = nullptr;     // [C*4]  fx, fy, cx, cy
    const float* dist_coeffs = nullptr; // [C*8] (may be null => treated as 0)
    const int* widths  = nullptr;       // [C] per-camera image width
    const int* heights = nullptr;       // [C] per-camera image height
    const int* camera_models = nullptr; // [C] CameraModelType values
    const int* distortions = nullptr;   // [C] CameraDistortionType values
                                        //     (may be null => NONE)
    bool valid() const {
        return viewmats && intrins && widths && heights && camera_models;
    }
};

// The distinct camera models in models[0..C), joined for a log line.
std::string camera_models_summary(const int* models, int C);

// Owns the device-side Gaussian buffers and an LBVH built over their support,
// plus the (optional) camera positions. Constructed once from host-side raw
// activated/un-activated splat parameters, then reused.
//
// All inputs are host pointers; the constructor uploads + activates internally.
//   means      : [N*3]  (x,y,z)
//   quats      : [N*4]  (w,x,y,z), un-normalized ok
//   log_scales : [N*3]  log std-devs (exp activation)
//   logit_opac : [N]    logit opacities (sigmoid activation)
//   features_dc: [N*3]  SH band-0 (DC) color coefficients
//   cam_pos    : [C*3]  camera centers in the SAME frame as means (may be null)
class OccupancyEvaluator {
public:
    OccupancyEvaluator(
        const float* means, const float* quats,
        const float* log_scales, const float* logit_opac, const float* features_dc,
        int num_splats,
        const float* cam_pos, int num_cameras,
        const CameraParams& cams,
        const MeshingConfig& cfg);
    ~OccupancyEvaluator();

    OccupancyEvaluator(const OccupancyEvaluator&) = delete;
    OccupancyEvaluator& operator=(const OccupancyEvaluator&) = delete;

    // Generate the 7-points-per-kept-Gaussian point cloud, interleaved (x,y,z).
    // Size = 3 * num_points(). float here; the caller widens to double only at
    // the Delaunay boundary (the one place that needs the extra precision).
    void generate_point_cloud(std::vector<float>& xyz_out);

    // Evaluate the occupancy field at host points [n*3] floats -> occ[n].
    void evaluate(const float* xyz, int n, float* occ_out);

    // The iso crossing on each cut edge (a,b), indices into `cloud_xyz`
    // [cloud_points*3], by bisection finished with a linear interpolation;
    // occ_a and occ_b are the ends' occupancies. xyz_out [n_edges*3].
    void bisect_edges(
        const float* cloud_xyz, int cloud_points,
        const int32_t* edge_a, const int32_t* edge_b,
        const float* occ_a, const float* occ_b,
        int n_edges, float* xyz_out);

    // Per-vertex RGB (in [0,1], interleaved) for host points [n*3]. Uses the
    // camera-aware compositing when cameras are present, else the static
    // density-weighted average, with a nearest-splat fallback.
    void colorize(const float* verts, int n, float* rgb_out);

    // Per-point observed screen-space resolution for host points [n*3]:
    // max over the cameras that see the point of focal_px / depth (pixels per
    // world unit). 0 for unseen points, and 0 everywhere when the render
    // (camera) path is unavailable. Used to budget texture-atlas texels by how
    // much detail the training views actually captured at each surface point.
    void view_texel_density(const float* verts, int n, float* dens_out);

    // view_texel_density and (with rgb_out) colorize at verts [nv*3], and
    // evaluate at probes [np*3], rendering each camera once for all three.
    void merge_fields(const float* verts, int nv, float* dens_out, float* rgb_out,
                      const float* probes, int np, float* occ_out);

    int64_t num_points() const { return num_points_; }
    int num_kept()   const { return num_kept_; }
    int64_t max_delaunay_points() const { return max_points_; }

    // True when the rasterize-and-sample (dataset / camera-intrinsics) path is
    // active, i.e. camera-based visibility culling is possible.
    bool has_render_cameras() const;

    // Camera indices selected for rendering (all cameras, or the k-means
    // medoids when cfg.max_cameras caps them). Empty without camera centers.
    const std::vector<int>& selected_cameras() const;

    // Mark which mesh vertices are seen by >= 1 camera: a vertex is seen when it
    // projects in-frame AND the segment to that camera is not blocked by a mesh
    // triangle that does not contain the vertex (occlusion via an LBVH over the
    // faces). Host verts [n*3], faces [n_faces*3] -> visible_out [n] (1 = keep).
    // When has_render_cameras() is false this is a no-op (all marked visible).
    void cull_unseen_vertices(const float* verts, int n,
                              const int* faces, int n_faces,
                              unsigned char* visible_out);

    // Render camera `cam_idx`'s occupancy moments (opacity, c0, c1; see
    // moments_to_depth in mesh/Tsdf.h) into `out` [width*height*3], row-major.
    // False without the camera render path.
    bool render_moments(int cam_idx, std::vector<float>& out, int& width, int& height);

private:
    struct Impl;
    Impl* impl_;
    int64_t num_points_ = 0;
    int num_kept_ = 0;
    int64_t max_points_ = 0;
};

// Full pipeline entry point. Writes a binary PLY to output_path. False on
// failure, or when `cfg.check` found defects in the mesh it wrote.
bool generate_mesh(
    const float* means, const float* quats,
    const float* log_scales, const float* logit_opac, const float* features_dc,
    int num_splats,
    const float* cam_pos, int num_cameras,
    const CameraParams& cams,
    const MeshingConfig& cfg,
    const std::string& output_path);

} // namespace meshing

#endif
