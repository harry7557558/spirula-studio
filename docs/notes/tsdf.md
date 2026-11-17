# TSDF meshing

`spirula mesh --method tsdf` (in the GUI: Surface method, Depth fusion) meshes
the way 2D Gaussian Splatting (Huang et al. 2024) reports its DTU Chamfer:
render a depth map per training camera, fuse them into a truncated signed
distance volume, extract its zero level. The opacity-field extractor
(`src/mesh/Meshing.h`) stays the default. Comparing a 2dgs model with the
paper's numbers needs the paper's extraction, or a gap could be the
extractor's rather than the model's.

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
4. **Defaults.** Voxel = diagonal of the 5-95 percentile box of the splat
   centres / 1024 (the paper's large-scene setting is 1024 cells over the
   depth range), truncation 4 voxels (its DTU setting: 0.016 at 0.004), no
   depth cap. `--tsdf-voxel`, `--tsdf-trunc`, `--tsdf-max-depth` override.

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

## Open

- DTU Chamfer against the paper's table. If the moments depth of a 2dgs
  model falls short there, render the depth with the model's own primitive
  through the engine (`forward_3dgs` with `output_median`) instead.
- The opacity-field sampler takes two of its seven points per splat along the
  third axis, which for a 2dgs splat (`e^-20`) both land on the centre.
