# Ground split

`spirula mesh --ground-prompt <text>` or `--ground-masks <dir>` writes the mesh
three times: whole (`mesh.*`), the ground alone (`mesh_ground.*`) and
everything else (`mesh_noground.*`), in every format and colour the run was
asked for. All three come from the same mesh, so the two parts meet edge to
edge and add up to the whole one.

## The masks

With a prompt, the mesh command masks every dataset image itself, before it
meshes, and keeps the masks in `<base>_ground_masks/` for a later run's
`--ground-masks`:

```bash
spirula mesh <run> --method tsdf --ground-prompt "floor; ground; grass" \
    --ground-model <models>/sam2.1_hiera_base_plus_f16.ggml --ground-detector gdino-tiny
```

A SAM 3 checkpoint reads the words itself and takes no detector. The GUI's
mesh screen has the same prompt and uses the masking model picked on the
Dataset screen. Name every kind of ground the scene has: on a garden,
`ground; grass` missed the paving that `paving stones` found.

The masks are SAM's own outline, not grown. `spirula sam` grows each object
by 5% of its size by default (`--dilate-ratio`), which for a floor that fills
the frame reaches tens of pixels up the base of everything standing on it.

`--ground-masks <dir>` takes masks made anywhere else: one per dataset image,
named after it the way training masks are (`frame_0001.png` for
`frame_0001.jpg`), black where the image shows ground and tight to it.

Masking runs on the inference layer, which needs a Vulkan driver; meshing
doesn't. `--ground-prompt` is in builds with `-DSS_BUILD_SAM=ON`, CUDA ones
included, and any build meshes from masks.

The dataset's own training masks are left as they are and still apply:
where one ignores a pixel (the operator of a 360 capture, say), that pixel
casts no ground vote, whatever the ground mask says there.

A mask is resampled to the camera's render size by nearest sample. A camera
whose image the parser had to resample (`ParsedDataset::redistort`) and a
camera without a mask cast no vote.

## How a face is labelled

The split happens on the finished mesh, after cleanup and colour, so it is the
same for the opacity field and both TSDF fusions (`src/mesh/GroundSplit.cpp`):

1. **Votes.** Every camera renders its median depth (the TSDF's
   `moments_to_depth`). A vertex facing the camera, projecting in frame and
   lying within 3 pixel footprints plus 2% of its distance of that depth is
   seen, and votes with its pixel's label. An occluded vertex votes for
   nothing, so a wrong mask on a table cannot reach the floor under it.
2. **Smoothing.** Each vertex's ground share is averaged with its faces'
   twice. Vertices no camera saw take their neighbours' share.
3. **Faces.** A face is ground when its corners' mean share is at least 1/2.
4. **Islands.** Connected faces of one label (sharing a vertex) under
   `--floater-min-faces` faces join the other label: a few mislabelled
   frames leave speckles, not holes.
5. **Growth.** Ground spreads to every face sharing an edge with it whose
   normal is within 20 degrees of the voted face it spread from. It crosses
   the floor that SAM missed beside objects and stops where a leg or wall
   turns up. The normal is the voted face's, not the last grown one's, so it
   cannot round a smooth corner a few degrees at a time.

`ground_split` (both builds, no GPU) labels a floor with a table on it from
eight cameras whose masks are 5% wrong, and checks that no vote reaches the
floor under the table, and grows ground over a strip that tilts by 10 degrees
and then turns up into a wall.

## One indoor capture

536 fisheye views at 1920x1920, a 5M-splat 2dgs model meshed with `--method
tsdf --samples-per-gaussian 1` (3.6M faces, 9 min, 2.8 GB peak), prompt
`floor`, SAM 2.1 base+ with Grounding DINO tiny: masking took 163 s, the
ground stage 39 s.

- Visibility does not lose floor: 0.4% of floor faces were seen by no camera,
  and the median floor face had 117 views.
- Masks grown by `spirula sam`'s default 5% put the bases of objects in the
  ground; without that growth, 3.9% of the ground lay more than 0.1 above
  the floor (from 5.3%), but it covered 63% of the floor.
- A vote share of 0.3 covered more floor (90%) by taking the sides and bases
  of objects too (38% of what it added faced up, 32% was steep). Growth
  instead reaches 85% with ground that is cleaner than the grown masks'
  (4.6% above the floor, 12% steep against 16%).

## One outdoor capture

Mip-NeRF 360 `garden`, 185 views, a 2dgs model meshed with `--method tsdf`
(1.2M faces, 99 s with masking, 3.0 GB peak), prompt `ground; grass; paving
stones`, masking 56 s.

- With grown masks, 7% of the tabletop and 18% of the table legs were
  ground. Without, 0.3% and 1.9%, and growth takes the ground covered from
  71% to 80%. At 30 degrees growth climbs 12% of the legs.
- The opacity field and the uniform-grid TSDF split the same way.
