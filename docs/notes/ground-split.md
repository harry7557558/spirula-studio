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
2. **Smoothing.** Each vertex's ground share is averaged with its faces'
   twice. Vertices no camera saw take their neighbours' share.
3. **Faces.** A face is ground when its corners' mean share is at least 0.3
   (`kGroundShare`), not a majority: see "Tuning".
4. **Islands.** Connected faces of one label (sharing a vertex) under 200
   faces (`kGroundIsland`) join the other label: a few mislabelled frames
   leave speckles, not holes.

`ground_split` (both builds, no GPU) labels a floor with a table on it from
eight cameras whose masks are 5% wrong, and checks that no vote reaches the
floor under the table.

## Tuning

One indoor 360 capture: 536 fisheye views at 1920x1920, a 5M-splat 2dgs
model meshed with `--method tsdf --samples-per-gaussian 1` (3.6M faces, 7.5
min, 2.3 GB peak; the ground stage 51 s). Masks from SAM 2.1 base+ with
Grounding DINO tiny, prompt `floor`, 0.29 s a frame. The floor is one plane,
so a plane fitted to the ground part scores the split: floor cover is the
up-facing area within 1% of the room's extent of the plane that comes out
ground, stray ground the ground area more than 3% off it.

- Visibility is not what misses floor: 0.4% of floor faces were seen by no
  camera, and the median floor face had 117 views.
- SAM's errors are one-sided. Off-floor surfaces almost never collect floor
  votes (99.2% of their area under a 0.1 share), while missed floor mostly
  lost narrowly: far and grazing views do not call it floor.

| share | island | floor cover | stray ground |
|---|---|---|---|
| 0.5 | 10 | 80.7% | 0.28% |
| 0.4 | 200 | 86.1% | 0.39% |
| **0.3** | **200** | **89.8%** | **0.66%** |
| 0.2 | 200 | 92.4% | 1.7% |

Most of what 0.3 still leaves is a few patches SAM firmly rejects (median
share 0.15), likely flat objects at floor height rather than floor.
