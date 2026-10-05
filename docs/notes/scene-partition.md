# Scene partitioning: train a reconstruction in parts and merge the models

`spirula partition` and the GUI's Partition panel (dataset screen, beside
"Open in Trainer" and "Edit Reconstruction") split one reconstruction into
parts that each fit a single training run, and merge the trained parts back
into one model. The code is portable host code in `data/ScenePartition.h`,
`data/LabelField.h`, `data/Region.h`, `data/RegionProgram.h`,
`core/GraphCut.h` and `checkpoint/SplatMerge.h`; the device test is
`shaders/region.slang` behind `kernels/densify/RegionWeight.cu`; the trainer
hooks are `--partition` / `--partition-part` and `--roi-region`
(`TrainerSession::apply_partition_config`, `setup_region`).

## Why not Voronoi over the cameras

The obvious split -- k-means on positions, Voronoi cells, crop by cell at the
merge -- already leaves surprisingly few visible seams, because a Gaussian's
look is set by nearby cameras. What survives comes from four places: Gaussians
straddling a cell boundary, sky and far geometry that every part reconstructs
on its own, per-image appearance drift between parts, and floaters that one
part grows in front of another part's cameras. The scheme here targets the
first and the last two directly; appearance drift is left for a later shared
initialization (see "Not done").

## The visibility cut (the default, `--method graph`)

What a part needs is that each of its cameras sees mostly its own region,
and that the region is one coherent piece of the scene rather than a plane's
worth of it: the cut of a triangle of three rooms must run along the walls,
not through one room. Both wishes are one number, so the split minimizes
it: the share of every camera's view that falls outside its part, plus a
smoothness term between neighbouring points, under the image cap. In graph
terms:

- **Nodes** are the cameras and the observed points, the points grouped
  into patches of up to 24 linked neighbours (`patch_points`; a patch never
  crosses a wall, since two faces of one share no observer). A capture of
  4000 frames and 800k points is a graph of ~70k nodes.
- **Camera-point edges** carry the observation's weight: `1/d^2`, the image
  area the point stands for, clamped at the camera's 5th-percentile
  distance and normalized so each camera's observations sum to 1. A
  label's weight on a camera is then its share of that camera's view --
  which is what the ROI masks later drop or keep -- and a point's observers
  weigh by how close they stand, so the votes are spatially coherent. With
  unit weights instead (each track a vote) the hall of Atrium came out as
  salt and pepper, because its cameras are tracked in a random few of the
  points they see.
- **Point-point edges** are the 12-NN affinities `own_points` uses (the
  share of one point's observers that are, or are covisible with, the
  other's), scaled so a point's neighbours weigh on average twice its
  observers (`kSmooth`; 4 and 8 cost view share on every capture tried, 1
  split Atrium into a fourth part).
- **The cut** is `graph::cut_labels` on that graph -- the same recursive
  normalized-cut bisection the view-graph method and the SfM mapper use --
  with node cost 1 per camera and ~0 per patch, so the balance, the leaf
  size and the tiny-part merges all count cameras. It recurses until every
  part's core is under `--max-images`; a camera's home is then the part
  holding most of its view, its ring the parts holding `--ring` (0.1) of it,
  and when core plus ring overruns the cap the cores are cut smaller in
  proportion and the cut runs again (at most six times).

Measured at `--max-images 2000 --ring 0.1` (`spirula partition split`
prints the per-part numbers; "interleaved" is the share of a point's ten
nearest points owned by another part):

| capture | method | parts | cameras see of own part | ring per part | cut of view graph | point owners interleaved |
|---|---|---|---|---|---|---|
| Atrium (4102 frames, one open hall with wings) | view graph | 3 | 54-58% | 671-1552 | 6.1% | 1.6% |
| | plane cuts | 4 | 60-66% | 456-642 | 20.4% | 2.3% |
| | **visibility cut** | 3 | 61-75% | 430-686 | 17.4% | 2.8% |
| utias (5314, a street loop with a dome) | view graph | 4 | 69-85% | 264-368 | 1.0% | 0.8% |
| | plane cuts | 6 | 40-80% | 340-805 | 14.8% | 2.6% |
| | **visibility cut** | 4 | 59-95% | 60-794 | 7.7% | 3.5% |
| myhal (7013, a building on five floors) | view graph | 6 | 65-78% | 175-672 | 1.7% | 1.4% |
| | plane cuts | 8 | 45-72% | 320-730 | 15.8% | 2.5% |
| | **visibility cut** | 6 | 69-91% | 35-388 | 3.0% | 2.2% |

The view-graph cut severs the least covisibility by construction, but its
regions come from camera positions, so a camera sees less of its part; the
plane cuts run through rooms and across floors (myhal's eight parts each
mix two or three storeys; the visibility cut's six are floors and wings).
The split takes 7-9 s on these.

Trained, 6000 steps per part at `--max-images 2000`, every 8th frame held
out, colour-corrected PSNR / SSIM of the merge; per-view differences pair
views by their ground-truth image (see below); myhal at half resolution:

| capture | model | PSNR / SSIM | mean dPSNR | views < -3 dB | worst | train time |
|---|---|---|---|---|---|---|
| Atrium | one model on everything | 22.73 / 0.793 | | | | 459 s |
| | view graph, 3 parts, ring 0.1 | 22.71 / 0.799 | -0.02 | 0 | -2.8 | 778 s |
| | visibility cut, 3 parts, ring 0.2 | 22.45 / 0.796 | -0.28 | 10 | -6.0 | 515 s |
| | visibility cut, 3 parts, ring 0.1 (default) | 22.63 / 0.799 | -0.10 | 2 | -3.4 | 776 s |
| myhal | one model on everything | 17.11 / 0.687 | | | | 613 s |
| | visibility cut, 6 parts, ring 0.2 | 17.11 / 0.694 | +0.00 | 2 | -3.8 | 1072 s |

The ring decides the trade: at `--ring 0.2` the visibility cut trains a
third faster than the view-graph split (rings of 245-465 cameras against
845-1336) and loses a quarter of a dB in ten views -- the end of the long
south corridor, where the merge shows smeared splats in front of the
camera, and a band across the hall where its seam runs; at 0.1 (the
default) it costs the same time and a tenth of a dB. On myhal the merge
matches the full model.

## The pipeline

1. **Covisibility graph** over the cameras (`build_covisibility`). One node per
   frame, edge weight = number of 3D points two frames both observe. Where the
   observations come from is the only thing that changes per dataset:
   - a COLMAP model (including this tool's own SfM output): its tracks, read by
     `read_sparse_stats`;
   - a Nerfstudio or Metashape dataset: the seed cloud projected into every
     frame with the host camera models (`camhost::ray_in_frame`), which is a
     covisibility proxy that ignores occlusion; at most `max_projected_points`
     points are projected and their observers are shared with the unsampled
     points beside them;
   - poses only: k nearest camera centres, weighted `1 + cos(view angle)`.
   `Auto` takes the first of these that works.
2. **Cut** (view-graph method, `--method viewgraph`; `graph::cut_labels`): recursive normalized-cut bisection by the
   Fiedler vector of the normalized Laplacian, the same code the bottom-up SfM
   mapper uses for its atoms, with three things the mapper does not want:
   each side of a bisection must hold at least 30% of the parent (a free
   normalized cut shaves off weakly attached clumps one at a time, which is
   what left a utias part in 437 pieces), a half the sweep left in pieces
   hands every piece but its largest to the other half, and a few passes of
   boundary refinement move a camera to the part it shares more with while
   the sizes stay within a quarter of the mean. Either exactly `--parts`
   parts (the costliest part is bisected until the count is reached) or as
   many as keep every part at or under `--max-images`. Parts under 2% of the
   cameras join their best-connected neighbour; an isolated camera joins the
   part of the nearest one. Labels are dense, largest part first, and the
   CLI says when a part is not one piece of the view graph.
   Frames are identified by the shortest tail of their path that is unique
   in the dataset, never the bare leaf: a rig has `cam0/00123.jpg` and
   `cam1/00123.jpg`.
3. **Point ownership** (view-graph method; `own_points`): each seed point starts as the part of
   its nearest camera, then those labels diffuse for 40 rounds over the
   points' 12 nearest neighbours -- an edge as strong as the share of one
   point's observers that are, or are covisible with, the other's, so the
   diffusion does not leak through a wall -- and last, any piece of a part
   smaller than a quarter of its largest joins the larger piece it borders
   most. What this replaced, a vote over each point's observers, interleaved
   two parts wherever both saw a surface: on a two-part classroom 36% of
   every point's ten neighbours belonged to the other part (1.5% now), and
   the merge, a salt-and-pepper mix of two models, lost 2.6 dB against its
   own parts. Measured alternatives, parts trained fresh and scored on
   held-out views (colour-corrected PSNR / SSIM, one model over everything
   for reference):

   | scene, steps | one model | nearest observer + smooth | 1/d^2 over observers + smooth | nearest camera | **nearest camera + smooth** |
   |---|---|---|---|---|---|
   | classroom, 2 parts, 7k | 19.64 / .741 | 19.43 / .743 | 19.50 / .739 | 19.56 / .744 | 19.54 / .742 |
   | Atrium, 7 parts, 2k | 21.22 / .764 | 20.95 / .767 | 20.98 / .768 | 21.18 / .771 | 21.14 / .771 |

   Anything built on the tracks loses: a point is tracked in a handful of the
   frames that see it, so "which part observed it" is mostly noise (on
   Atrium 47% of points have no observation from their nearest camera's
   part). The smoothing costs nothing measurable over plain nearest-camera
   Voronoi and guarantees one coherent region per part. What it gives up: a
   thin wall's far face, when it is nearer to the other room's cameras than
   to its own, goes to the other room, whose model never saw it. No capture
   tried here has that layout; the observer rule would fix it at the cost in
   the table.
4. **Owned region** (`LabelField::build`): every point of space belongs to
   the nearest *seed* -- the labelled points (strided to `--max-seeds`), each
   carrying the mean direction toward the cameras that observed it; the
   cameras are seeds only when there are no points, since a camera standing
   in another part's space would cut a hole in it. The metric is not
   Euclidean: the space *behind* a seed, relative to the side it was seen
   from, counts `kBehindWeight` (8) times farther, and a query that carries a
   normal facing away from a seed's viewing direction is `kOrientPenalty` (20)
   times farther (`shaders/region.slang`). So a wall photographed from one
   room does not claim the other room's air, and a splat on the wall's back
   -- seen only from the next room -- stays with that room's model.
   Density-adaptive and unbounded by construction: no grid, no resolution,
   and a far splat goes to the nearest thing that saw anything. Queried on the
   host through a BVH over the seeds, and on the device through the same
   layout.
5. **Ring**: an outside camera joins part k when at least `--ring` (0.1) of
   the points it sees (and `--ring-min-points` of them) are owned by k. The
   ring sees the seam from outside, so both neighbouring parts learn it under
   the same supervision. It was 0.05, which let cameras that see a sliver of
   a part in and doubled the images per part.
6. **Seed points per part**: everything it owns, and a hash-stable
   `outside_seed_fraction` (0.2) of what its cameras see outside. A ring
   camera whose image shows another part's region needs some geometry there
   to explain the pixels, or it grows floaters inside; all of it would only
   grow splats the merge discards.
7. **Training** (`--partition file --partition-part k`): after parsing, the
   trainer keeps the part's frames (matched by image leaf, as `SparseEdit`
   matches them) and its seed points, for both the train and the eval split.
   Nothing else changes: the run's output is in the dataset's frame as always,
   and its `scene_transform.json` records any centering or rescale.
8. **Merge** (`merge_partition_splats`): each part's splats go back through
   its run's `world_from_train`; each one's short axis is pointed at the
   nearest camera that trained it and the field is asked which part owns that
   oriented point; the winners get their SH padded to the highest degree
   present and are concatenated. Hard ownership, no feathering.
9. **Region of interest while training**: a partitioned run hands its part's
   label region to the engine (`engine_set_region`), and `--roi-region` hands
   any region JSON -- by default the first one the ROI editor saved in the
   dataset's `roi/` (docs/notes/roi-editor.md), intersected with the part's. At every refine step `region_weight_tensor` evaluates the
   compiled program at every splat centre -- normal oriented by the nearest
   training camera -- and a splat outside draws for relocation and growth
   with `--roi-outside-weight` (1e-4) instead of 1, in both the revised and
   the MCMC path. `--roi-outside-opacity-decay` would also scale their
   opacity at every refine step; it is off (1), because the pixels those
   splats explained still had to be explained, and the parts grew splats in
   front of their cameras to do it (-2 dB after the merge, table above).
   What does work is not supervising those pixels at all: with
   `--roi-mask-pixels` (on) the trainer projects the partition's whole seed
   cloud into each training image, labels every quarter-resolution cell by
   the nearest point over a footprint that grows as points come closer (so a
   sparse near floor still hides what lies behind it), fills holes from
   neighbours, keeps what nothing covers, widens what is inside by a margin
   so the seam stays supervised, ANDs any existing mask, and writes the
   result under `<run>/roi_masks/` (`write_region_masks`). On Atrium a
   third of the pixels drop out.
   `region_parity` holds the device test to the host mirror. The region is
   in the dataset's frame and the splats in the training frame
   (`relative_scale * (p - center)`), so `setup_region` moves the compiled
   program by that similarity first (`RegionProgram::apply_similarity`).

## The files

`partition.json` beside `partition.bin`, both written by `write_partition`:

- JSON: `format`, `version`, `dataset` (absolute), `source`, `options`,
  `num_parts`, `frame_names` (image leaves) and `frame_parts` (core label per
  frame), `parts[k].ring` (frame indices), `cut_fraction`, `binary`.
- BIN: `SSPT` v2, then the `LabelField` (`SSLF` v1: seeds [n,4] and BVH
  nodes [m,8] as floats), the camera centres [N,3], then u8 owner per seed
  point in file order, then per part a u32 count and u32 point indices.

The trainer needs the JSON's frame lists, the BIN's point tables and the
field (its region of interest); the merge needs the field and the centres. A run's `config.json` carries `partition` and
`partition_part`, which is how `find_partition_runs` pairs runs with parts
without any naming convention.

## Regions (`data/Region.h`, `data/RegionProgram.h`)

The ownership field is one `Region` among several: `BoxRegion` (oriented),
`SphereRegion`, the ROI editor's `EllipsoidRegion`, `CylinderRegion` and
`PrismRegion` (an extruded polygon), `HalfSpaceRegion`, `MeshRegion` (closed
mesh, ray parity over a BVH), `LabelRegion` (one label of a `LabelField`) and
`CsgRegion` (union, intersection, difference, complement). Every kind serializes through
`region_to_json` / `region_from_json`, and `contains_many` answers a whole
splat array in parallel on the host. Every kind but the mesh also compiles
(`compile_region`) to a post-order program of float4 nodes that
`shaders/region.slang` evaluates on both backends in one kernel, with the
label field's seeds and BVH as two more float4 arrays uploaded once per run;
a prism's polygon follows its node as whole payload nodes the evaluator steps
over. Constants and layouts live in the shader; `data/LabelField.cpp` and
`data/RegionProgram.cpp` are its host mirrors and `region_parity` pins them.

## The GUI

The Partition button on the dataset screen opens the panel over the parsed
reconstruction and computes nothing until Compute is pressed. "Queue parts in
Batch" saves the partition, then asks for the run's preset, splat cap, SH
degree and step count in a dialog; Queue is the confirmation, and if the
batch list still holds rows that have not finished it asks whether to clear
them first. With "merge once all have trained" ticked the list gets a final
Merge row (`BatchStage::Merge`), which finds the parts' runs under the runs
folder by their `config.json` and writes `<dataset>_merged_<stamp>.ply`
there. Rows whose tasks all finished are left unticked when the queue ends;
"Clear done rows" and "Clear list" both confirm first. While the Merge row
runs the screen follows it to the list, and when the queue ends on a merge
the merged model opens in the viewer.

In the trainer's engine view the region of interest is shown by greying
what lies outside it: each pixel's surface point, from the render's depth,
is tested against the region (every third pixel, on the host), and the
region's dashed silhouette is drawn over it. Before training, the preview
greys the seed points outside the region.

Regions are drawn as surfaces (`data/RegionMesh.h`: surface nets over the
inside test, crossings bisected onto the boundary, open where the region
leaves the box; `app/webviewer/RegionOverlay.h`): a translucent fill and a
dashed silhouette. The Partition panel shows every part's boundary in its
colour, or one part's when soloed. The trainer's viewport shows the run's
region of interest -- before training in the OpenGL preview, and during
training in the engine view, where `RenderWorker` rasterizes the mesh on the
host against the frame's depth so the fill and the outline fade behind
nearer geometry. The "region" switch beside "grid" hides it; pinhole views
only.

## Other ways to split, not taken

- Ground-plane tiles (VastGaussian, CityGS), or recursive plane cuts of the
  point cloud across its principal axes (the default before the visibility
  cut): a plane through a hall or a floor splits rooms, and on myhal every
  part mixed storeys.
- k-means on the points with Voronoi cells: compact, but the cells know
  nothing about which cameras they will need, and nothing about walls.
- Soft ownership: keep splats a band past the seam from both sides with
  opacity scaled by distance to the boundary, or fine-tune the band jointly.

## Not done, on purpose

- No seam-band joint refinement and no opacity feathering. Judge the hard
  ownership cut on real captures first; either is a small addition on top of
  the merge if the seams warrant it.
- No shared appearance initialization (a short capped global pass whose
  per-image appearance state every part starts from). Colour seams from
  exposure drift between parts are the one failure mode this design does not
  address.
- Projection covisibility ignores occlusion, so a wall between two rooms does
  not separate them the way tracks would. Tracks win whenever they exist.
- The region test on the device has no mesh leaf; a mesh region is host-only
  until a triangle BVH joins the program.
