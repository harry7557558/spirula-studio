# TSDF meshing — implementation plan

Goal: `spirula mesh --method tsdf`, the extraction 2D Gaussian Splatting
(Huang et al. 2024) reports its DTU Chamfer with: render a depth map per
training camera, fuse them into a truncated signed distance volume, extract
the zero level by marching cubes. The current extractor (opacity field on a
Delaunay tetrahedralization of splat samples, `src/mesh/Meshing.h`) stays the
default. Comparing a 2dgs model against the paper's numbers needs the paper's
extraction, or a gap cannot be told apart from the extractor's.

## Status

T0-T2 done, T3 but for the GUI: `spirula mesh --method tsdf` runs on both
backends. First run, a 3dgs model of the action-cam capture (1M splats, 488
cameras at 2688x1512): 4 min, 6.0 GB RAM at peak, mesh F 0.474 against 0.56
for the opacity field of the same model. TSDF averages each camera's depth
and only clears free space near surfaces, where the opacity field takes the
least occupancy over the cameras, so floaters one camera sees survive; the
comparison that matters is on a 2dgs model, next.

## Design

1. **Depth from the moments render.** `render_camera_moments`
   (`mesh/MeshingRaster.h`) already renders each camera, for any primitive,
   as (opacity, c0, c1): a line fit of `sqrt(-ln T)` against ray distance.
   The median depth, where T falls to 1/2, is `(sqrt(ln 2) - c0) / c1`; a
   pixel is used when its opacity reaches 1/2 and `c1 > 0`. No new kernels,
   so both backends get it at once. Depth is ray distance (unit world rays),
   as everywhere in the eval3d path, so fisheye and 360 cameras need nothing
   special.
2. **Fusion on the host.** A sparse volume of 8^3-voxel blocks in a hash
   map, OpenMP over blocks. Per camera: back-project each valid pixel and
   allocate the blocks within the truncation band of its point; then for
   every allocated block in the frustum, project each voxel centre with
   `data/CameraMath.h` (`project_ray`, every camera model and distortion),
   read the pixel's depth `d`, and fold `clamp(d - |v - o|, -trunc, trunc)`
   into the voxel's running mean (weight 1, as Open3D). Voxels behind the
   band (`d - |v - o| < -trunc`) are not updated.
3. **Marching tetrahedra** over allocated blocks, six per cube along its main
   diagonal, with the opacity-field extractor's tables (`mesh/MarchingTets.h`)
   rather than the paper's marching cubes: the same linear interpolation of
   the same field, no second table. A vertex per cut edge, keyed by the edge's
   lower grid point and direction, so the mesh comes out welded; a tetrahedron
   is skipped unless its four voxels have weight. The result enters the existing finishing
   chain in `MeshingHost.cpp` (merge, cleanup, culling, orientation, colour,
   export) unchanged.
4. **Defaults.** Voxel = diagonal of the 5-95 percentile box of the splat
   centres / 1024 (the paper's large-scene setting is 1024 cells over the
   depth range), truncation 4 voxels (its DTU setting, 0.016 at 0.004; the
   large-scene one uses 5), no depth cap. Its DTU run uses median depth.
   Flags: `--method opacity|tsdf`, `--tsdf-voxel`, `--tsdf-trunc` (voxels),
   `--tsdf-max-depth`.

## Phases

### T0. Split the extractor from the finishing chain
`generate_mesh` runs sampling, Delaunay, occupancy and marching tetrahedra,
then the shared tail. Move the tail into its own function taking a `Mesh`;
the opacity-field output must stay byte-identical (mesh a model before and
after).

### T1. Depth maps
`render_camera_depth(ctx, cam, float* host_depth)` in `MeshingRasterHost.cpp`:
render moments, download, convert on the host. Unit check: a plane seen
head-on gives its distance.

### T2. Volume and marching cubes
`mesh/Tsdf.{h,cpp}`, host only. `tsdf_sphere` (both builds, no GPU):
analytic depth maps of a sphere from pinhole, fisheye and orthographic
cameras; vertices within a voxel of it (median 0.05: depth is read at the
nearest pixel, as Open3D does), the mesh closed, faces wound outward.

### T3. CLI, GUI, memory
Flags above, help in all 13 languages (`check_ui_text.py`); the GUI's mesh
options gain the method; `mesh/MeshProgress.h` plans the volume's RAM
(blocks touched by the first cameras, extrapolated), so the memory forecast
covers it. `--no-data` with `--method tsdf` is an error: fusion needs cameras.

### T4. Validation
The action-cam capture, 3dgs and 2dgs models, both extractors, scored with
`reference/python/mesh_fscore.py`; then DTU Chamfer in the 2dgs plan's phase
7, against the paper's table. If the 3DGUT moments depth of a 2dgs model
falls short there, render depth with the model's own primitive through the
engine (`forward_3dgs` with `output_median`) instead.

## Side item

The opacity-field sampler takes 7 points per splat, two along the third
axis. For a 2dgs splat that axis is `e^-20`, so both land on the centre: drop
them for 2dgs models.
