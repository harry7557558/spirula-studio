# Mesh check: what a bad mesh is, and where the mesher made them

`spirula mesh --check 1` runs `check_mesh` (`src/mesh/MeshCheck.h`) on the mesh
it writes and exits 1 on any defect; `--check 2` also reports after every
stage of `finish_mesh` (`src/mesh/MeshingHost.cpp`), which is how the sources
below were found. Given a mesh file instead of a model, `spirula mesh
mesh.ply --check 1` only checks it. The report stays English, like the other
deep diagnostics.

## What counts

| category | defect |
|---|---|
| indexing | index out of range, a face naming one vertex twice, a NaN/inf position, a vertex no face uses |
| duplicates | a vertex at another's exact position; an edge or face over the same positions as another |
| topology | an edge with 3+ faces; a vertex whose faces form 2+ edge-connected fans (bowtie); an edge both faces run the same way; a closed part with negative volume; a part under `--floater-min-faces` |
| geometry | two faces crossing anywhere but at the corners they share; adjacent faces closer than 10 degrees (a fold); a zero-area face |
| quality | an angle under 1 degree; aspect ratio (longest edge over 2 sqrt 3 inradius) over 30; an interior valence-3 vertex whose faces turn under 30 degrees; valence over 16 |

Boundary edges and loops are reported, not counted: an open scene has them.
A closed part seen from inside (a room) has negative volume by design;
`orient_mesh` faces such a part inward when its box holds most cameras, and the
check would then flag it.

Crossings are tested exactly enough for float input: double orientations
with a 1e-10 relative tolerance, which only absorbs touching. Corners are
shared when bit-identical, so split copies count as shared. Face pairs come
from a BVH over the faces in Morton order (`FaceBVH`, leaves of 4); the whole
check of a 400k-face mesh takes 0.1 s on 8 threads.

## Where the defects came from

The dessert capture, meshing preset, through the pipeline as
[mesh-merge.md](mesh-merge.md) left it: 19,209 crossing face pairs,
4,313 folds, 1,099 duplicate vertices and 354 inside-out parts in the result.
By stage (the field's own share, most of what follows, is in
[mesh-field.md](mesh-field.md)):

- **Hole filling** made most crossings (2.7k -> 28k pairs in one pass). Any
  part with a boundary of at most 20 edges was closed, including small sheets
  and strips, whose fill then lay on the part itself: duplicate faces, coplanar
  overlaps and closed zero-volume pillows. Fill faces also got arbitrary
  winding, which swung `orient_mesh`'s vote on small parts.
- **The merge**: 30 -> 3.2k pairs in the raw pass. Nearly all crossings were
  local: of the crossing pairs, 62% shared a vertex and 36% were one edge apart.
  It also folded closed tetrahedra into two-face pillows (2.5k duplicate faces)
  and joined boundaries across the surface.
- **Quality flips and smoothing**: +7k pairs, on curved patches.
- **The degenerate-face fix**: a few hundred, from collapses without a
  crossing test.
- **Bowtie splitting** left the copies at one position (the duplicate vertices).

## What the pipeline does now

Most of the defects above came from the field, not from the stages that
exposed them: [mesh-field.md](mesh-field.md) removes the bubbles, blobs and
pinholes the raw mesh carried. The stages themselves:

- The merge refuses a collapse whose reshaped faces cross one another or any
  face within one edge of their corners; a patch whose faces all lie within 45
  degrees of their mean normal is a height field and skips the test. It also
  refuses a closed tetrahedron and an interior edge joining two boundary
  vertices. Faces of a vertex outside the thread's block are skipped; a
  vertex next to the block cannot move during the phase, so the outcome stays
  independent of thread count.
- **Culling works on patches, not vertices** (`despeckle_visibility`). A
  vertex no camera sees inside seen surface is a pit or a crease, and dropping
  it and its faces opens a hole: on the meshing-preset dessert 17.5k of the
  17.8k unseen patches of the raw mesh had under 100 vertices, 9% of the unseen
  vertices between them, and culling them left 11.8k boundary loops. So seen
  patches under 64 vertices (sight lines through a gap) first turn unseen, then
  unseen patches under 64 turn seen; 8 in the second pass, whose mesh is about a
  tenth as dense. A part with nothing seen still goes whole. 5.4k loops after
  the first cull instead of 11.8k, 2.8k with the first step.
- Hole filling winds a patch against the faces on its loop, leaves a loop that
  is half its part's size or more (a rim), and rules out folds inside the
  minimum-weight triangulation (Liepa weighs dihedral angles; here a fold is
  simply not allowed). **A slit** (a loop enclosing under 15% of the area a
  circle of its length would: a 1:18 rectangle) is closed whatever its length. When the whole
  patch crosses or folds, **ears** are clipped instead, best-shaped first, each
  kept only if it neither folds nor crosses; what no ear can close stays open.
- Flips act on quads within 20 degrees and pass the crossing test; smoothing
  moves only vertices whose faces lie within 30 degrees, and moves are undone
  where a face then crosses another.
- Bowtie copies move 5% of the way into their own fan; the last repair round,
  which nothing follows, cuts the smaller fans instead.
- `fix_degenerate_faces` runs on `LocalEdit`: a collapse or flip that
  keeps the mesh manifold, adds no fold or crossing and raises the worst angle
  around it. Needles collapse (or flip a long edge when the collapse would
  pinch), caps flip or collapse their apex, folds flip or collapse, flat
  valence-3 vertices collapse.
- A last repair cuts what is left (crossing faces; the faces around slivers,
  folds, flat valence-3 and valence over 15) and refills the holes, three
  rounds; a fourth only cuts. Its limits sit inside the check's (1.5 degrees,
  aspect 25, 32 degrees, folds under 12 degrees). With the field as it was it
  cut 20% of the 2dgs dessert's faces; now 0.5%.
- `orient_mesh` faces closed parts out by the sign of their volume.

## Results

`--check 1`, default flags, before -> after the field of
[mesh-field.md](mesh-field.md) and the culling and hole filling above.
Vulkan; every run passes on both backends:

| model | vertices | boundary edges | time | peak RAM | VRAM over idle |
|---|---|---|---|---|---|
| dessert, meshing preset | 216k -> 148k | 78.4k -> 28.8k | 31.6 -> 27.8 s | 2.4 GB | 0.85 -> 0.92 GB |
| dessert, 2dgs | 139k -> 70k | 41.9k -> 4.2k | 37.8 -> 27.8 s | 2.4 GB | 1.09 -> 0.94 GB |
| dessert, 3dgs | 538k -> 673k | 75.1k -> 45.5k | 43.9 -> 43.4 s | 2.4 GB | 1.11 -> 1.05 GB |
| dessert, 2dgs, `--method tsdf` | 1129k -> 81k | 611k -> 9.7k | 48.3 -> 28.9 s | 3.3 -> 2.4 GB | 1.88 -> 0.95 GB |
| dessert, 2dgs, `--no-data` | 1060k -> 1046k | 95.7k -> 70.8k | 48.4 -> 48.5 s | 2.4 GB | 0.43 GB |
| dessert, meshing preset, `--no-data` | 352k -> 334k | 17.0k -> 4.4k | 30.3 -> 29.1 s | 2.4 GB | 0.42 GB |
| bicycle `images_4`, 2dgs | 1308k -> 1078k | 412k -> 34.4k | 68.7 -> 48.2 s | 2.7 -> 2.4 GB | 1.44 -> 1.01 GB |
| bicycle `images_4`, meshing preset | 1988k -> 1495k | 788k -> 272k | 80.6 -> 61.4 s | 3.1 -> 2.8 GB | 1.75 -> 1.4-1.8 GB |

`--method tsdf` was the uniform grid before and is the adaptive fusion after
([tsdf.md](tsdf.md)); the grid at `--tsdf-voxel 0.004` takes 75 s, 5.6 GB of
RAM and 2.6 GB of VRAM for 390k vertices, and bicycle's 2dgs model takes 58 s
and 1.1 GB adaptively.

The last repair's first round cuts 0.6% of the faces on the 2dgs dessert (it
was 20%), 0.9-2.5% on the others, and a third on a 2dgs model without cameras,
as before: its bare density still gives every disc a thin closed shell.

F against the dessert's dense cloud: 2dgs 0.738 -> 0.77, meshing preset 0.749
-> 0.77, 3dgs 0.813 -> 0.80, 2dgs through `--method tsdf` 0.658 -> 0.805. Part
of the 2dgs gain is given back by closing slits and keeping pits (0.80 without):
precision falls where the mesh now has surface and the cloud has none, recall
does not.

CUDA, the five runs of the earlier table: 26.0 -> 24.1 s (meshing preset),
32.9 -> 24.8 s (2dgs), 44.8 -> 26.4 s and 3.2 -> 2.3 GB of RAM (tsdf), 43.1 ->
42.5 s (`--no-data`), 68.0 -> 55.1 s and 1.92 -> 1.57 GB of VRAM (bicycle,
meshing preset).

## Speed and memory

Where the time and memory went, beyond the smaller raw mesh (8-10M faces where
there were 11-13M, and a tenth of the parts):

- **Sealing** is a lock-free union-find over the vertices: 0.1 s for 47M
  tetrahedra.
- **The merge's thickness probes**, eight per vertex, go 16.7M to a loop over
  the cameras; all at once they were bicycle's VRAM peak (1.4 GB of 1.9).
- **Ear clipping** keeps a queue of candidate ears and tests one when it comes
  up; scanning the loop for the best ear each time took 42 s on one loop of the
  bicycle TSDF mesh.
- **Merge limits**: one render of each camera serves the probes and colour,
  and the raw pass probes nothing.
- **The cleanup passes** index faces by vertex offsets, not hash maps;
  the degenerate fix runs in Morton blocks in parallel like the merge, and after
  its first round revisits only faces near an edit; it fixes only the worst
  (5 degrees) before the second merge, which reshapes the rest anyway.
- **Hole filling**: loops over 64 edges are split at a chord before the cubic
  table (at 250 the table took 5 of bicycle's 7 s of filling).
- **Quality smoothing** checks for crossings only from faces it moved.
- **The merge's crossing test** is skipped for a collapse whose fan stays within
  30 degrees of flat.

The check costs 0.1 s on dessert's 272k faces and about 1 s on bicycle's 2.7M;
the stats line every run prints comes from the same pass without the
intersection test. Peak RAM is still the Delaunay (2.3 GB for 7M points).
