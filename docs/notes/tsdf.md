# TSDF meshing

`spirula mesh --method tsdf` (in the GUI: Surface method, Depth fusion) meshes
the way 2D Gaussian Splatting (Huang et al. 2024) reports its DTU Chamfer:
render a depth map per training camera, fuse them into a truncated signed
distance, extract its zero level. The opacity-field extractor
(`src/mesh/Meshing.h`) stays the default.

There are two fusions:

- **Adaptive** (the default). The signed distance is sampled where the opacity
  field is: at the Delaunay vertices of the splat samples, so the mesh is
  dense where the Gaussians are and a large or unbounded scene costs what its
  Gaussians cost. It shares the extraction and the fusion pass of
  [mesh-field.md](mesh-field.md); see "Adaptive fusion" below.
- **A uniform grid**, with `--tsdf-voxel <size>`: the paper's extraction, kept
  for comparing a 2dgs model with the paper's numbers, where a gap could
  otherwise be the extractor's rather than the model's. The rest of this note
  up to "Adaptive fusion" describes it.

## How it works

1. **Depth from the moments render.** `OccupancyEvaluator::render_moments`
   renders each camera for any primitive as (opacity, c0, c1): a line fit of
   `sqrt(-ln T)` against ray distance, which the opacity field reads as
   occupancy `min(opacity, 1 - exp(-(c0 + c1 z)^2))`. `moments_to_depth`
   takes the depth where that reaches 1/2, `(sqrt(ln 2) - c0) / c1`, the
   median depth the paper meshes DTU with and the level the opacity field
   meshes at. No new kernels, so both backends have it. Depth is ray
   distance along unit rays, so every camera model fuses the same way.
2. **Fusion on the host** (`src/mesh/Tsdf.cpp`). A sparse volume of 8^3-voxel
   blocks in a hash map. Per camera, each pixel with a depth `d` allocates the
   blocks on its band `[d - trunc, d + trunc]`; then every voxel of those
   blocks is projected with `data/CameraMath.h` (each camera model and
   distortion tier), reads its pixel's depth at the nearest pixel, and folds
   `min(d - |v - o|, trunc)` into a running mean, weight 1, as Open3D does.
   Voxels more than `trunc` behind the surface are left alone.
3. **Marching tetrahedra** over the allocated blocks, six per cube along its
   main diagonal, with the opacity-field extractor's tables
   (`src/mesh/MarchingTets.h`) instead of the paper's marching cubes: the same
   linear interpolation of the same field without a second case table. Each
   cut edge belongs to its lower grid point, so a vertex is made once and the
   mesh comes out welded; a tetrahedron counts only when all four of its
   voxels were observed. The mesh then takes the opacity field's finishing
   chain unchanged (merge, culling, cleanup, orientation, colour, export).
4. **Parameters.** `--tsdf-voxel` in scene units (the paper's large-scene
   setting is 1024 cells over the depth range), truncation `--tsdf-trunc` 4
   voxels (its DTU setting: 0.016 at 0.004), `--tsdf-max-depth` no cap.

Finding the blocks a camera touches dominated the first version, 0.32 s a
camera against 0.03 s for the voxel updates, from sorting every pixel's keys;
a per-thread hash set that keeps only new blocks brought the fusion of 488
cameras at 2688x1512 from 157 to 42 s, building the same 327k blocks.

`tsdf_sphere` (both builds, no GPU) fuses analytic depth of a unit sphere seen
by pinhole, fisheye and orthographic cameras: the mesh is closed, its faces
wound outward, and its vertices are within a voxel of the sphere (median 0.05
voxel; the nearest-pixel lookup, not the interpolation, sets the error).

## Results

An action-cam capture (488 cameras at 2688x1512), meshes scored by
`reference/python/mesh_fscore.py` against the capture's dense matching cloud
(F moves by ~0.02 between reruns):

| model | extractor | precision | recall | F | time | RAM peak |
|---|---|---|---|---|---|---|
| 2dgs preset, 30k | opacity field | 0.849 | 0.585 | 0.693 | 97 s | |
| 2dgs preset, 30k | TSDF | 0.778 | 0.607 | 0.682 | 116 s | |
| 3dgs, 1M | opacity field | 0.659 | 0.490 | 0.562 | | |
| 3dgs, 1M | TSDF | 0.532 | 0.427 | 0.474 | 4 min | 6.0 GB |

On a 2dgs model the two extract equally well: TSDF covers more of the surface
(fewer holes) and is rougher. On a 3dgs model TSDF falls behind: it averages
each camera's depth and clears free space only near the surface, where the
opacity field takes the least occupancy over the cameras, so floaters that one
camera sees survive. Hence the GUI's advice, depth fusion for 2dgs models
only. The volume was 2.06 GB of that 6.0 GB peak; `mesh/MeshProgress.cpp`
sizes the fusion stages from this run.

## Adaptive fusion

`launch_sample_occ` with `OccFusion::tsdf` folds, per camera and query point,
the distance from the camera's surface (the nearest pixel's, where its profile
crosses the level) to the point, in units of a band of `--tsdf-trunc` pixel
footprints (4), clamped at +1 and dropped beyond -1, weighted by how sharply
the camera sees the point. The mean maps to the level at zero distance, so
marching tetrahedra and the cut-edge bisection run unchanged. A point in no
camera's band is inside if some camera saw it deeper than its surface, free
otherwise, which closes the surface where the uniform volume leaves it open;
the visibility cull removes what that adds.

The band is in pixels, not world units, because a camera resolves depth about
as well as it resolves position: a voxel grid's world-unit band is too wide
near the cameras and too narrow far away, which is the other half of handling
an unbounded scene.

Dessert 2dgs model: F 0.805 with 81k vertices in 29 s and 2.4 GB of RAM. The
grid at its former automatic size (the 5-95% box of the splat centres over
1024) gave 0.658 with 1.13M vertices in 48 s and 3.3 GB; at `--tsdf-voxel
0.004` with the pixel profile of [mesh-field.md](mesh-field.md) it gives 0.808
with 390k vertices in 75 s and 5.6 GB. So most of the gain is the profile,
and the adaptive fusion gets it at a fifth of the vertices and under half the
memory.

## Open

- DTU Chamfer against the paper's table. If the moments depth of a 2dgs
  model falls short there, render the depth with the model's own primitive
  through the engine (`forward_3dgs` with `output_median`) instead.
