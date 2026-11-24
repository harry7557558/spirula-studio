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

`ground_split` (both builds, no GPU) labels a floor with a table on it from
eight cameras whose masks are 5% wrong, and checks that no vote reaches the
floor under the table.

Not tuned on real captures yet: the visibility band and the island size are
first guesses.
