# The meshing field: from one ray to one surface

The opacity-field extractor (`src/mesh/Meshing.h`) meshes the level set of a
field sampled at Delaunay vertices. With cameras, the field is built in four
steps, each of which replaced something that put holes or junk in the mesh:

1. a depth **profile** per pixel of each camera (`rasterize_moments`),
2. a **fusion** of the cameras' views of a point (`launch_sample_occ`),
3. the splats' own **density**, for those with volume,
4. **sealing** of free space that no camera connects to (`seal_pockets`).

Numbers are from the dessert capture (178 cameras at 480x270; a 2dgs model, a
3dgs model and the meshing preset) unless said otherwise. F is
`reference/python/mesh_fscore.py` against the capture's dense cloud, mesh
cropped to the cloud's box; reruns move it by about 0.01.

## 1. One ray: the profile

Each pixel stores `(m0, c0, c1)`: the opacity its ray accumulates and a line
`c0 + c1 z` for `sqrt(-log T)` at ray depth `z`, so that occupancy at any depth
is `min(m0, 1 - exp(-(c0 + c1 z)^2))`.

The line is fitted to **the middle half of the light the ray loses**: the
fragments that take the transmittance `T` from 0.75 to 0.25. It passes through
their mean depth, weighted by each one's share of that band, with the slope
their spread gives (`band_line`). A single opaque layer gives a step at that
layer; two layers give a ramp between them that moves continuously as their
opacities change. The first quarter (haze in front) and the last (whatever is
behind) do not enter.

- A fragment's depth is where the ray enters the Gaussian: one sigma, along the
  ray, before its peak. Zero for a 2dgs disc.
- Once a ray has lost half its light, a relative depth gap of 2% ends the fit:
  what lies beyond is another surface, and `m0` is the opacity of the first.

The fit this replaced was a line through two fragments chosen so that it never
under-estimated the opacity (an upper hull). It put a camera's surface in front
of the splats by an amount that grew with the spacing of the layers behind:
on the 2dgs model the old mesh sat 2.1e-3 scene units (0.75 px) outside the
opaque surfels' centres. Taking the least occupancy over 178 cameras carved
part of that back; an average does not, and came out 3.5e-3 outside, with F
0.674 against the old 0.738. With the band fit the same average gives 0.79.

## 2. Many cameras: the fusion

The field at a point used to be the least occupancy any camera gave it. One
camera suffices to carve, and 178 cameras offer 178 chances of a wrong one. Of
the Delaunay vertices that the average of the cameras called solid, 33% (2dgs)
to 55% (meshing preset) were carved by the least. Two causes:

- **Silhouettes.** Occupancy is blended over four pixels. Where they straddle
  an object's outline, a point just under the surface reads as seen. For a
  fifth of the carved vertices some camera reported an occupancy under 0.05.
- **Weak layers.** A pixel whose front layer absorbs under half the light sees
  through to the next.

Those vertices were isolated: the raw 2dgs mesh had 16,376 parts, nearly all
bubbles under the surface and blobs above it. Culling, hole filling and the
last repair then worked on a double-walled, pitted surface, and the repair cut
20% of the faces.

`launch_sample_occ` folds each camera into four numbers per point, and
`launch_finalize_occ` reads the field off them:

- **A vote.** The field is the mean occupancy over the cameras that see the
  point's neighbourhood, weighted by how sharply each sees it (pixels per world
  unit there). A camera for which the point lies more than `occ_band_px` (6)
  pixel footprints behind its own surface is not looking at this surface; its
  weight falls to `occ_deep_weight` (0.01) over the next 6. That is what a
  quantile over all cameras lacked: a concavity seen by two side cameras is not
  outvoted by a hundred front cameras, because for those it is far behind
  their surface and they do not vote.
- **Clear sight.** One camera still carves alone when the largest occupancy
  among its four pixels is under `occ_clean` (0.2), and it sees the point at
  least half as sharply as the sharpest camera that votes it solid. This keeps
  strict carving where the vote would thicken a thin part's far side, without
  the silhouette error (all four pixels must agree) or a distant camera erasing
  what it cannot resolve.

The surface then sits where the cameras' profiles agree on average, not at the
deepest of them, so it is also smoother before any mesh processing.

A fused point costs 16 bytes on the device (it was 8), and the largest batch is
the merge's thickness probes, eight per vertex. They go 16.7M to a loop over
the cameras (`kProbesPerLoop`); bicycle's 43M probes at once were 1.4 GB of its
1.9 GB peak.

`--method tsdf` uses the same pass with a signed distance in place of the
vote ([tsdf.md](tsdf.md)).

Sweeping the three parameters (clean 0.1-0.35, band 3-12 px, deep weight
0-0.05) moved F by at most 0.02 on any of the three models; a deep weight of
exactly 0 doubles the parts, as points behind every surface then have no vote.

## 3. The splats' own density

The field is joined with the density of the Gaussians at the point
(`1 - (1 - field)(1 - density)`), which gives a 3D Gaussian's surface its
front shell: the cameras' median alone lies about a sigma deeper, 1.6e-3 on
the 3dgs model (F 0.67 without the term, 0.80 with).

A 2dgs disc has no volume. Its density is solid at exactly one of its seven
samples, the centre, which makes every opaque disc lying in front of the
agreed surface an island: 17k parts on the 2dgs model, and 99k faces for the
repair to cut. So a Gaussian's density is scaled by its thinnest over its
widest sigma, reaching full weight at 0.05 (`kSolidThickness`). On the 3dgs
model, 45% of whose Gaussians are flat, that alone raised F from 0.797 to 0.814.

Without cameras the density is the whole field, as before.

## 4. Free space connects to a camera

The camera centres join the Delaunay as free vertices. After the field is
sampled, free vertices are joined through tetrahedra whose four corners are
all free; any free vertex that such a chain does not connect to a camera is
made solid. Without cameras the seeds are the cloud's extreme vertices along
26 directions. A cut edge into a sealed vertex takes its midpoint, as the
field has no crossing on it.

This is the answer to a shell with a small seam: a passage must be at least a
tetrahedron wide to let free space through, so pinholes do not hollow what is
behind them. A seam wider than that still opens the interior to whatever the
cameras really saw through it.

It is a lock-free union-find over the vertices (a root only ever hooks under
a smaller index, so the result does not depend on the thread count): 0.1 s for
47M tetrahedra on 8 threads. It seals 5k vertices on the 2dgs dessert, 16k on
the meshing preset, 375k on the 3dgs model.

## What it does to the meshes

Dessert, default flags, before -> after:

| model | parts at extraction | faces cut by the last repair | boundary edges | F |
|---|---|---|---|---|
| 2dgs | 16,376 -> 149 | 61.7k of 304k -> 0.8k of 136k | 41.9k -> 4.2k | 0.738 -> 0.77 |
| meshing preset | 9,774 -> 1.2k | 16.4k of 381k -> 7.0k of 277k | 78.4k -> 28.8k | 0.749 -> 0.77 |
| 3dgs | | 30k of 1.3M | 75.1k -> 45.5k | 0.813 -> 0.80 |

The boundary edges and the last two digits of F include the mesh-side changes
of [mesh-check.md](mesh-check.md), which has the full table with bicycle,
times and memory. With the field alone the 2dgs model scores 0.80: closing
slits and keeping pits adds surface where the dense cloud has none.

## What did not work

- **The exact median** (the depth of the fragment where `T` crosses 1/2, with
  a step for a profile): F 0.79 on the 2dgs model, but the vote then jumps
  between surfel layers from place to place. 17.6k parts; 136k faces cut.
- **The quantile inside one Gaussian** (its extinction along the ray taken as
  a normal curve, the crossing found within it): 0.686 on the 3dgs model,
  0.671 without, 0.708 with the fixed one-sigma entry that stayed.
- **A lower level for 3D Gaussians.** `--iso 0.3` without the density term
  gives the 3dgs model 0.82 and the meshing preset 0.69; the density term
  gives 0.80 and 0.76-0.79, and leaves 2dgs models (0.74 at 0.3) alone.
- **Votes as a share of the first surface's opacity**, so that a
  half-transparent sheet counts as a full surface: thin fins along every
  silhouette on bicycle, and -0.02 on the 2dgs dessert.
- **A lower threshold on the vote** (0.4 or 0.33 of the weight): fragments of
  bicycle's spokes appear, the rest fattens (2dgs dessert 0.75, 0.73).
- **Clear sight from any camera**, however far: 0.71 on the meshing preset
  against 0.76 with the sharpness condition.
- **Making a surface seen from opposite sides solid** within a pixel of it (the
  cameras' directions at the surface summed; a sheet where they cancel): no
  spoke came of it, even with floater removal off. Removed.
- **Culling single unseen vertices.** See [mesh-check.md](mesh-check.md).

## Still open

**Spokes.** A spoke is a row of discs seen from both sides. Each camera finds
it (it is there in every camera's depth), but the vote on its plane is one
half from either side, and no sample lies inside so thin a slab. The beads the
old field made of them came from the density at disc centres, which is what
section 3 removes. A sheet needs a thickness the samples can resolve before
either term can make it solid.
