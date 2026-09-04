# Monocular geometry (`src/metric3d/`)

Metric3D v2 on the inference layer (`src/nn/`): one image in, a depth map and a
surface normal map out, with no onnxruntime, no PyTorch and no converter.

Status: **done and wired in.** `spirula geometry <dataset>` writes the
`normals/` and `depths/` a training run reads, and the forward pass matches
onnxruntime on the same checkpoint to a relative L2 of 1.5e-3. The GUI runs it
as the last step of a dataset run (`src/app/gui/GeometryRunner.h`) and tries it
on one frame first (`src/app/gui/GeometryPanel.h`).

It replaces `reference/scripts/predict_geometry.py`, which loaded the same
weights through `torch.hub` and needed a CUDA PyTorch, mmcv and the model's own
repository checked out beside it.

## Rules

- **Vulkan only**, like `src/sfm/`, `src/sam/` and `src/aliked/`. Built with
  `SS_BUILD_SAM` (which is what builds `ss_nn`).
- **Model-specific things live here, never in `nn/`.** The general ops this
  needed -- tanh / elu / silu, average pooling with padding, a
  fractional-scale nearest resize, a row softmax -- went into `nn/` and are
  tested there. What is left in `shaders/metric3d.slang` is only what could
  not be general: the RAFT decoder's convex upsampler, its GRU blend, and the
  two places its six-channel state is taken apart.
- **No camera model reaches this directory.** The network sees a pinhole
  image and knows nothing about the frame it came from; `app/GeometryWarp.h`
  is the seam that resamples a fisheye or a panorama into one.
- **The checkpoint is someone else's ONNX export, parsed in process.** We host
  nothing and convert nothing.

## The checkpoint is an ONNX file

`nn/io/Onnx.cpp` (shared with `src/aliked/`) is a varint walk of protobuf that
reads *initializers only* -- the graph structure is hard-coded in
`model/Encoder.cpp` and `model/Decoder.cpp`, so nothing has to understand a
node, an operator or a shape rule.

The artifacts are the [onnx-community](https://huggingface.co/onnx-community)
exports of Metric3D v2, which are the same weights
`torch.hub.load('yvanyin/metric3d', 'metric3d_vit_large')` loads. Running the
same bytes is what makes `tools/metric3d/compare_ort.py` a check of *our*
arithmetic rather than of two checkpoints.

Only the **fp16** exports are fetched. The inference layer uploads every matrix
weight as f16 anyway -- that is what the tensor-core GEMM takes -- so an fp32
download would be twice the bytes for numbers we round on the way in. It costs
3e-3 of relative error against an fp32 reference, which is below the fp32
accumulation noise of the pass itself.

### Everything is read off the file

The three variants differ in more than width, and every difference is visible
in the checkpoint, so there is one code path and the id is only a URL:

| | vit-small | vit-large | vit-giant2 |
|---|---|---|---|
| width x blocks | 384 x 12 | 1024 x 24 | 1536 x 40 |
| FFN | MLP | MLP | SwiGLU |
| decoder reads | one final LayerNorm, shared four ways | same | four taps at blocks 9/19/29/39, un-normed |
| refinement iterations | 4 | 8 | 8 |
| GRU width | 48 | 128 | 192 |
| weights on device | 71 MiB | 784 MiB | 2626 MiB |

`model/Hparams.h` says where each of those comes from. Two are not in any
tensor and are counted off the graph instead: the iteration count (the GRU
weights are shared across iterations, so it is invisible in the weight list)
and which module each anonymous `onnx::MatMul_*` belongs to.

## Layout

```
Metric3D.h            the public surface: Predictor, PredictOptions, Prediction
Common.h              the names borrowed from nn/, as aliked/Common.h does
model/Hparams.h       the shape of a checkpoint, all of it read off the file
model/Fetch.{h,cpp}   the three ids: URL + SHA-256, on nn/io/Fetch.h
model/Weights.{h,cpp} initializers -> device tensors; the transposes and the
                        f16 decision; the positional embedding stays on the host
model/Model.h         the loaded model and the two halves of the forward pass
model/Encoder.cpp     DINOv2 with registers, read against ViT_DINO_reg.py
model/Decoder.cpp     RAFTDepthNormalDPT5, read against
                        RAFTDepthNormalDPTDecoder5.py
model/Predictor.cpp   the public glue, arena planning, and the stage dumps
shaders/metric3d.slang  convex upsample, GRU blend, state pack/unpack
tests/metric3d_test.cpp checkpoint shapes and a finite, in-range prediction
```

## Four conventions that are not guessable

Each of these was wrong at some point, each failure looked like a working
network rather than a broken one, and in every case the exported graph settled
it where the module source did not.

1. **`ConvBlock` rectifies its input in place.** `self.act` is
   `nn.ReLU(inplace=True)`, so `out = self.act(x); ...; return x + out` adds
   `relu(x)`, not `x`. Getting this wrong survives the first FuseBlock -- whose
   input is a GELU, bounded below by -0.17 -- and diverges at the second.
2. **...and that rectification escapes the block.** `DecoderFeature` runs
   before `ContextFeatureEncoder` over the same feature list, so the context
   branches read `relu(x1)` and `relu(x2)`. `read_0` never passes through a
   `ConvBlock` (`upconv_0` is commented out upstream) and arrives untouched,
   which is why missing this breaks two of the three branches and not the third.
3. **The positional embedding's 0.1 does not reach the coordinate mapping.**
   DINOv2 asks torch for a scale of `(grid + 0.1) / 37`, but the export emits a
   **sizes**-based `Resize`, and an ONNX `Resize` given sizes maps coordinates
   by `out/in`. Carrying the 0.1 into the mapping is a 4% error on the
   embedding and 2.6% on the tokens -- small enough to look like rounding.
4. **`coords_grid()` returns zeros.** The decoder is written as a RAFT flow
   field with a `coords1 - coords0`, and there is no grid: the six-channel
   state *is* the flow, and the loop is `state += delta`. Nothing here
   reconstructs one.

## Output sizes

`Predictor::patchSize()` is 14 and `Predictor::sizeGranularity()` is 28, and it
is the second one a caller wants. The decoder works on a
`floor(3.5 * side/14)` grid and upsamples it by 4, so a side of `14*(2k+1)`
comes back two pixels short of what went in.

## The camera side

`app/GeometryWarp.h` is everything this directory refuses to know about. The
network estimates z-depth for a pinhole camera and was trained on nothing else,
so a wide frame is resampled into pinhole faces on the way in and the
prediction gathered back into the source camera's own frame on the way out.
One plan per camera, reused for every frame that shares it.

Two shapes. **Undistort** is one pinhole face at the camera's own intrinsics --
the cheap path, and the only sensible one for a perspective lens. **Split** is
a front face plus a ring of faces reaching the lens's edge, or a 6-face cube
map for a panorama. `--split auto` picks between them on
`camhost::pinhole_coverage` at 0.75, and the trainer's
`--input-depth-is-ray-depth` resolves against the same call (AGENTS.md).

The split is planned from what the lens actually holds
(`camhost::visible_boundary`, the largest visible angle per azimuth, by the
same validity test the trainer's warp applies), so it fits a 150-degree lens
and a 220-degree one alike:

- The front face spans 45 degrees times 1.1, cropped -- still centred -- to
  the visible box, so a cropped frame does not hand the network a grey band.
- The ring reaches in to 40 degrees and out to 3 degrees past the boundary.
  Its faces are **square and upright** (the image's up as close to the
  camera's as the tilt allows, for the networks' gravity prior), with a half
  field of view of 30 to 40 degrees as the band needs, and as many of them as
  that width takes to go round with 15% shared between neighbours: 8 for a
  180-degree lens, 7 for 220. Each face tilts to its own sector's boundary; a
  face whose band runs along an image axis is cropped to it, down to 4:3.
- Every face is sized at the lens's own pixel density where it points and
  then capped at `--max-size`, so the cost is the capture's pixels, in pieces.

The faces **overlap by design**, which is the part worth reading before
changing any of it:

- Each face is a separate call into the network, and a monocular network
  re-guesses its depth scale and its horizon every call. Faces abutted edge to
  edge write that disagreement into the output as a line down the middle of the
  frame, which is what a cube map produces.
- So the cross-fade ramps over the outer 20% of every face. The extent lives
  in the LENGTH of each face's two in-plane axes, so the same table drives both
  the resampling and the blend.
- Depth is put on one scale first (`alignFaces`): the median log ratio over
  each pair's strip, then the per-face offsets that best satisfy all of them,
  which is a small graph Laplacian. Cross-fading a scale error only smears it.
  The faces have different focals, so Metric3D's canonical depth is turned
  into millimetres per face BEFORE the blend.
- The blend runs in the log, where the trainer's Pearson-of-log depth loss
  reads it, and a face is weighted out wherever it saw the mid-grey fill
  instead of the frame.

Uncovered pixels are written **black** in the normal map and **0** in the
depth map, which are the two sentinels the loss masks on
(`gt_normal_valid` and `ref_depth != 0`). Mid-grey is only what the *model's
input* gets, where a black border would read as an edge; it is never carried
into the output, and the loss drops it as well -- a real normal is unit
length, and mid-grey decodes to zero.

The faces are private to this warp -- the output is written in the source
camera's frame, and a training run that splits the same capture does its own
split (docs/datasets.md, "The split"). `spirula geometry --check` is what
tests all of it, by round-tripping an analytic plane and by handing the faces
that plane at deliberately different scales to see the alignment take them back.

## Speed and memory

RTX 5070 Laptop, a 1064x476 frame, the second call onwards (the first builds
pipelines and measures the GEMM tiling):

| | weights | load | predict |
|---|---|---|---|
| vit-small | 71 MiB | 0.6 s | 119 ms |
| vit-large | 784 MiB | 4.8 s | 810 ms |
| vit-giant2 | 2626 MiB | 11.0 s | 2030 ms |

vit-large against input size, which is the lever that matters:

| longest side | tokens | predict |
|---|---|---|
| 616 | 880 | 260 ms |
| 1064 | 2660 | 810 ms |
| 1600 | 5928 | 2200 ms |

Cost grows faster than the pixel count: attention is quadratic in tokens and
the decoder's ConvGRU runs `3 * iterations` convolutions over a 1/4-resolution
map. **1064 is the size Metric3D's own inference pipeline runs at**, is about
2.5x quicker than 1600, and is `spirula geometry --max-size`'s default. It is a
ceiling on one face rather than on the frame: a split sizes its faces off the
full-resolution capture (`app/GeometryWarp.h`), so a fisheye still runs about
as many pixels as it was shot with, in five pieces.

The arena is planned up front (`Model::planArenaBytes`) because it will not
grow while anything is live. Its peak is the DPT fuse chain's upsample stage,
not the ViT.

## Testing

```bash
./build/nn_ops_test                 # the general ops, vs a scalar CPU reference
./build/metric3d_test               # cached checkpoints, or SKIP
./build/metric3d_test --model M --image IMG.jpg --max-size 1064 --repeat 3
spirula geometry --check            # the camera round trip, no network in it
```

`metric3d_test` is a shape and sanity gate: every weight the forward pass asks
for exists at the width the rest of the model assumes, and a prediction comes
back finite, inside the network's own 0.1..200 range, with unit normals. It
cannot check the numbers -- we do not own these weights and cannot embed a
golden copy.

The gate that matters is parity against onnxruntime:

```bash
pip install onnx onnxruntime numpy
SS_METRIC3D_F32_WEIGHTS=1 SS_METRIC3D_DUMP=/tmp/ours \
    ./build/metric3d_test --model model.onnx --image IMG.jpg --max-size 616
python3 tools/metric3d/compare_ort.py --onnx model.onnx --ours /tmp/ours
```

Measured on the fp32 vit-small and vit-large exports, over every stage:

| | worst element | worst relative L2 |
|---|---|---|
| f32 weights | 3.1e-3 / 4.4e-3 | 1.0e-3 |
| f16 weights (what ships) | 3.1e-3 / 7.4e-3 | 1.5e-3 |

Both columns are fp32 accumulation order over a 24-block ViT and 8 refinement
iterations. The worst *element* of the normal map is always the largest number
here and always a pixel where the normal is near-degenerate, which is why the
L2 column is there. `SS_METRIC3D_F32_WEIGHTS=1` exists for exactly this -- it
is not a quality knob, it is what makes the comparison tight enough that a real
bug cannot hide under the rounding.

It found three: the in-place ReLU above, its escape, and the positional
embedding's 0.1. Each was between 2e-2 and 6e-1 and each looked like a
plausible normal map.

### vit-giant2 has no onnxruntime reference

Both giant2 exports, fp32 and fp16, fail to **load** in onnxruntime:

```
Type Error: Type parameter (T) of Optype (Add) bound to different types
(tensor(int64) and tensor(float16)) in node (/depth_model/decoder/Add_6)
```

`Add_6` is `coords1 + depth_init`, and `coords_grid()` returns zeros -- so the
defective node is an identity add whose zero tensor was traced as int64. We
never build the graph, only read its initializers, so it runs here. The weights
are unaffected, and the cross-check that stands in for parity is that giant2
agrees with vit-large more closely than vit-small does (median 4.5 degrees
against 8.2 on the same frame), which a broken SwiGLU or a mistapped encoder
could not produce.

## Not done yet

- **Sky.** `predict_geometry.py` had a `--sky` mode that pulled in Depth
  Anything 3 and lang-segment-anything to find sky and flatten the depth there.
  Nothing of that is here; the 99.9th-percentile normalization in
  `spirula geometry` covers the part of it that mattered for the depth range.
- **Batching the faces.** A split frame runs its 5 or 6 faces through the
  encoder one at a time. Their total token count is the same as one full-size
  frame, so this costs launch overhead rather than arithmetic.
