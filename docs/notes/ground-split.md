# Ground split

`spirula mesh --ground-masks <dir>` writes the mesh three times: whole
(`mesh.*`), the ground alone (`mesh_ground.*`) and everything else
(`mesh_noground.*`), in every format and colour the run was asked for. All
three come from the same mesh, so the two parts meet edge to edge and add up to
the whole one.

## The masks

One image per dataset image, named after it the way training masks are
(`frame_0001.png` for `frame_0001.jpg`), black where the image shows ground.
That is what `spirula sam` writes for the objects a prompt matches, so a text
prompt is the usual source, indoors or out:

```bash
spirula sam track --model sam3-q4_0.ggml --frames <dataset>/images \
    --out <dataset>/ground_masks --text "floor; ground; road; grass; carpet"
spirula mesh <run> --ground-masks <dataset>/ground_masks
```

Masks can come from anywhere else in the same convention. Masking runs on
the inference layer, which needs a Vulkan driver, and meshing doesn't: a CUDA
build meshes from masks made by any build, and one built with
`-DSS_BUILD_SAM=ON` makes them itself.

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
   Mask pixels within 1% of the image's longer side of the other label cast
   no vote: a mask is least sure along its edges and in small specks.
2. **Smoothing.** Each vertex's ground share is averaged with its faces'
   twice. Vertices no camera saw take their neighbours' share.
3. **Faces.** A face is ground when its corners' mean share is at least 1/2.
4. **Islands.** Connected faces of one label (sharing a vertex) under
   `--floater-min-faces` faces join the other label: a few mislabelled
   frames leave speckles, not holes.

`ground_split` (both builds, no GPU) labels a floor with a table on it from
eight cameras whose masks are 5% wrong, and checks that no vote reaches the
floor under the table.

## One indoor capture

536 fisheye views at 1920x1920, a 5M-splat 2dgs model meshed with `--method
tsdf --samples-per-gaussian 1` (3.6M faces, 7.5 min, 2.3 GB peak; the ground
stage 51 s), masks from SAM 2.1 base+ with Grounding DINO tiny, prompt
`floor`, 0.29 s a frame.

- Visibility does not lose floor: 0.4% of floor faces were seen by no camera,
  and the median floor face had 117 views.
- SAM misses floor more often than it calls something else floor, so a lower
  share covers more of the floor plane: 0.3 covered 89.8% against 80.7%. It
  was worse to look at all the same: what it added was the sides and bases of
  objects on the floor (38% of it facing up and 32% steep, against 61% and
  17% of the majority's ground), which a plane-distance score counts as
  floor. The majority stays.
- Ignoring mask edges made the ground cleaner (4.2% of it more than 0.1
  above the floor, from 5.3%) for less of the floor (72.0%, from 80.6%):
  the floor right beside objects loses the votes it had near their edges.

## One outdoor capture

Mip-NeRF 360 `garden`, 185 views, a 2dgs model meshed with `--method tsdf`
(1.2M faces, 43 s, 2.2 GB peak), masks from the same SAM, prompt
`ground; grass; paving stones` (`ground; grass` missed the paving).

- Grass and paving were ground; the table, hedges and planters were not.
- The mask edges held most of what was wrong. Ignoring them took the share
  of the tabletop called ground from 7.1% to 4.9% and of the legs from 18%
  to 10%, for 82.4% of the ground covered against 83.3%. A margin of 2%
  lost ground and gained nothing on the legs, 4% was worse on both.
