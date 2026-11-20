# Mesh merge: decimation that keeps thin and detailed parts

`finish_mesh` (`src/mesh/MeshingHost.cpp`) merges three times: once on the raw
marching-tetrahedra mesh before the camera cull, then after each cull. Every
pass collapses edges under per-vertex limits, so flat surfaces thin out while
thin structures, vegetation and colour detail keep their vertices.

## A collapse

- **Where the survivor goes**: one end of the edge or its midpoint, whichever
  moves the surface least. Ends keep vertices on the extracted surface.
- **Error**: the area-weighted RMS distance to the planes of the faces around
  both ends, plus planes standing on their boundary edges so borders do not
  creep. After the raw pass each vertex also carries the planes of every face
  merged into it (Garland-Heckbert quadrics), so the error is against the mesh
  the pass started from and a rough surface cannot drift a little per collapse.
- **Guards**: the link condition, refusing also a closed tetrahedron (it
  would fold into two faces on the same corners) and an interior edge
  joining two boundary vertices (a pinch); the 60-degree fold guard
  (`--merge-max-flip-deg`); and no reshaped face may cross another or any
  face within one edge of its corners ([mesh-check.md](mesh-check.md)).

## The limits

Per vertex, with `merge_factor` (`--merge-factor`, the GUI's "Surface detail")
scaling all of them:

- **Footprint**: world units per pixel of the closest camera that sees the
  vertex unoccluded (`view_texel_density`); the local edge length without
  cameras.
- **Thickness**: the occupancy field probed at 0.5, 1.5, 4 and 10 units of
  max(footprint, edge length) along both normal directions; the longer occupied
  run. Past the last probe the surface is solid. Both sides, because a vertex
  nudged off the surface or with a flipped normal (hole fills are oriented
  later) reads empty at its first probe on one side. Without camera renders it
  is not probed: a 2dgs disc's bare density is empty off its plane.
- **Tolerance** = min(`merge_flat_px` footprints, `merge_thin` x thickness).
  Solid surfaces lose bumps under a pixel; a spoke or leaf moves at most a
  tenth of its thickness.
- **Longest new edge**: `merge_max_edge_px` footprints; in the raw pass also
  twice the local edge length, which keeps that pass short before culling.
- **The raw pass** probes no thickness: its tolerance is also capped at a
  quarter of the local edge length, which thin parts survive. Probing its
  5.5M vertices on the dessert capture took 2 s and 1.1 GB of VRAM on top of
  the evaluator's own, the run's peak.
- **Colour**: after the raw pass, vertex colours bound what one vertex may
  absorb to a `merge_color` range per channel. A vertex-colour export needs
  vertices where the colour changes; a texture export does not. Colour and
  the probes share one render of each camera (`OccupancyEvaluator::merge_fields`);
  the footprint needs a render of its own, as it spaces the probes.

## Parallelism

Vertices are sorted along a Morton curve and cut into blocks of 32768. Each
block runs a sequential greedy queue (lowest error first), collapsing only
edges whose ends' closed 1-rings lie inside it; a second phase shifts the cuts
by half a block. No thread writes what another reads, and the outcome does not
depend on the thread count. A vertex outside a block but next to one cannot
move during the phase (its own block cannot satisfy that condition), so the
crossing test reads its position; its faces it skips, as their block may be
rewriting them. Refusing such collapses instead cost 28% of them.

Rounds of independent collapses over the whole mesh (each claiming its 2-ring)
were tried first: about 4% of the nominated edges won a round, so thinning a
flat region took ~400 rounds, 22 s on the dessert capture against 5 s here.

## Why the earlier merge lost spokes

It collapsed every edge shorter than its vertex's mean edge length, three
times. A spoke's cross-section edges are short against its length, so the
spokes that marching tetrahedra did extract (`--merge-factor 0` shows them on
bicycle) were collapsed away.

## Results

Default settings against the previous merge, Vulkan, uncovered = background
pixels in renders from three training views, F = `reference/python/mesh_fscore.py`
against the dessert dense cloud with the mesh cropped to the cloud's box. These
runs predate the cleanup in [mesh-check.md](mesh-check.md), which has current ones:

| model | vertices | time | uncovered | F |
|---|---|---|---|---|
| bicycle `images_4`, 2dgs preset | 499k -> 1.42M | 49 -> 84 s | 4.0 / 4.1 / 0% -> 3.1 / 2.7 / 0% | |
| bicycle `images_4`, meshing preset | 883k -> 2.15M | 63 -> 109 s | 11.8 / 14.1 / 3.8% -> 6.6 / 8.7 / 1.3% | |
| dessert, 2dgs | 484k -> 145k | 32 -> 37 s | under 1% | 0.706 -> 0.710 |
| dessert, 3dgs | 432k -> 625k | 30 -> 44 s | under 1% | 0.814 -> 0.811 |
| dessert, 2dgs, `--method tsdf` | 834k -> 1.15M | 32 -> 52 s | | 0.641 -> 0.641 |
| dessert, 2dgs, `--no-data` | 890k -> 1.43M | 44 -> 65 s | | 0.750 -> 0.768 |

Faces after each merge pass as a share of the pass before, over the nine runs
of [mesh-check.md](mesh-check.md): 0.52-0.73 for the raw pass, 0.11-0.52 for the
first culled one (bicycle's vegetation keeps half), 0.45-0.85 for the last
(`kShrink` in `src/mesh/MeshProgress.cpp`).

On bicycle the vertices went to grass, hedges and the bench (median triangle
edge in view 40: grass 3.5 -> 2.7 px, trees 4.1 -> 3.6, the left path 7.9 ->
10.2), and the hedges lost to floater removal came back. The extra time is
mostly cleanup and the quality pass on the larger mesh.

- `--merge-flat-px` 2 or 3 instead of 1 barely changed the meshing-preset
  bicycle (1.88M -> 1.74M vertices): on vegetation the thickness limit binds.
- A 16 px edge cap without the colour limit left dessert at 98k vertices and
  blurred the chocolate stripes of the vertex colours; 8 px with 0.05 keeps
  them at 145k.
- Without quadrics the error drifted on foliage and the meshing-preset bicycle
  lost its hedges (14-20% uncovered).

## Still open

Spokes: the merge keeps what marching tetrahedra extracts of them, but the
field itself gives a row of two-sided discs no thickness to extract
([mesh-field.md](mesh-field.md), "Still open").
