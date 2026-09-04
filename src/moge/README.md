# Monocular geometry, MoGe-2 (`src/moge/`)

MoGe-2 on the inference layer (`src/nn/`): one image in, a metric depth map, a
surface normal map and a validity mask out, with no onnxruntime, no PyTorch and
no converter.

Status: **done and wired in.** `moge2-vitb` is what `spirula geometry` runs by
default, and the forward pass matches onnxruntime on the same checkpoint to a
relative L2 of 3.8e-6. It sits beside `src/metric3d/` rather than replacing it;
`app/GeometryModel.h` is the seam that picks between them, and `--model` is
what spells the choice.

## Why it is the default

Against Metric3D v2 it brings three things this repository wanted:

- **A sky mask.** MoGe predicts where its own geometry is meaningless, and
  those pixels are written as the trainer's two "no ground truth here"
  sentinels -- depth 0 and a black normal -- instead of as a wall at the
  horizon. `predict_geometry.py`'s old `--sky` mode wanted Depth Anything 3 and
  lang-segment-anything to do this; here it is one head of the same network.
- **Metric depth.** The scale head emits metres per unit, so `--depth-units mm`
  is the prediction times 1000 rather than a canonical depth times a focal.
- **Speed.** vit-base is ~0.4 s a frame where Metric3D vit-large is ~0.8.

What it costs is a solve: MoGe's point map is affine, correct up to one unknown
z shift, and turning it into depth needs the camera's focal length. We have one
-- `app/GeometryWarp.h` resamples every frame into a pinhole face and knows
exactly what that face's focal is -- so this is a one-parameter fit rather than
the two-parameter guess MoGe's own CLI makes.

## Rules

- **Vulkan only**, like `src/sfm/`, `src/sam/`, `src/aliked/` and
  `src/metric3d/`. Built with `SS_BUILD_SAM` (which is what builds `ss_nn`).
- **Model-specific things live here, never in `nn/`.** This is the only one of
  the model libraries with no `shaders/` directory at all: the two ops it
  needed -- `padding_mode='replicate'` on a convolution and an `exp` GEMM
  epilogue -- are general PyTorch features, went into `nn/`, and are tested in
  `nn_ops_test`.
- **No camera model reaches this directory.** The network sees a pinhole image;
  `app/GeometryWarp.h` is the seam that resamples a fisheye or a panorama into
  one. What does reach it is the pinhole's four numbers, because the shift
  solve needs them.
- **The checkpoint is someone else's ONNX export, parsed in process.** We host
  nothing and convert nothing.

## The checkpoint is an ONNX file

`nn/io/Onnx.cpp` (shared with `src/aliked/` and `src/metric3d/`) is a varint
walk of protobuf that reads *initializers only* -- the graph structure is
hard-coded in `model/Encoder.cpp` and `model/Head.cpp`, so nothing has to
understand a node, an operator or a shape rule.

The artifacts are Ruicheng's own exports, which are the same weights
`MoGeModel.from_pretrained('Ruicheng/moge-2-vitb-normal')` loads. Running the
same bytes is what makes `tools/moge/compare_ort.py` a check of *our*
arithmetic rather than of two checkpoints. There is no fp16 export to prefer as
there is for Metric3D, so these are fp32 on disk and the loader rounds the
matrices on the way to the device.

### Everything is read off the file

The three variants differ in more than width, and every difference is visible
in the checkpoint, so there is one code path and the id is only a URL:

| | vit-small | vit-base | vit-large |
|---|---|---|---|
| width x blocks | 384 x 12 | 768 x 12 | 1024 x 24 |
| blocks the head reads | 5, 11 | 5, 11 | 5, 11, 17, 23 |
| residual blocks, neck | 1 per level | 1 | **2** |
| residual blocks, heads | 1 per level | 1 | 1 |
| class embedding | shared with the class token | its own | its own |
| download | 141 MB | 419 MB | 1.3 GB |
| weights on device | 66 MiB | 198 MiB | 629 MiB |

`model/Hparams.h` says where each of those comes from. The tapped blocks are
the one thing not in any tensor -- all of the taps share the encoder's single
final LayerNorm, so the weight list cannot tell you how many there are -- and
are counted off which block output each `/encoder/norm*` node consumes.

## Layout

```
Moge.h                the public surface: Predictor, PredictOptions, Prediction
Common.h              the names borrowed from nn/, as metric3d/Common.h does
model/Hparams.h       the shape of a checkpoint, all of it read off the file
model/Fetch.{h,cpp}   the three ids: URL + SHA-256, on nn/io/Fetch.h
model/Weights.{h,cpp} initializers -> device tensors; the transposes and the
                        f16 decision; the positional embedding stays on the host
model/Model.h         the loaded model and the two halves of the forward pass
model/Encoder.cpp     DINOv2 without registers, read against
                        DINOv2's own vision_transformer.py
model/Head.cpp        the shared neck and the three heads, read against
                        ConvStack in MoGe's modules.py
model/Recover.{h,cpp} the focal / z-shift solve; host math with no Vulkan in it
model/Predictor.cpp   the public glue, arena planning, and the stage dumps
tests/moge_test.cpp   checkpoint shapes, a finite prediction, and the solve
                        against an analytic plane
```

## Four conventions that are not guessable

Each of these was settled by the exported graph where the module source did
not, and getting any of them wrong looks like a working network rather than a
broken one.

1. **The class positional embedding IS the class token.** DINOv2 holds
   `pos_embed[:, 0] == cls_token` bit for bit in all three checkpoints, and the
   vit-small export deduplicated them into one initializer -- so reading
   `encoder.cls_token` where the class embedding belongs is correct there and
   not a substitution. vit-base and vit-large keep both, so the loader prefers
   the separate one and falls back.
2. **Every 3x3 convolution in the neck and the heads pads by REPLICATION.**
   Zero padding differs only on one ring of pixels, which is exactly the ring a
   depth map's edge artefacts live on.
3. **The residual blocks carry no normalization at all.** `res_block_in_norm`
   and `res_block_hidden_norm` are both `'none'`, so a block is
   `x + conv(relu(conv(relu(x))))` and there are no affine parameters to look
   for. What makes this visible in the file is that `layers.0` and `layers.3`
   have no weights.
4. **The level-3 resampler is a bilinear 2x upsample; the other three are
   transposed convolutions.** Nothing but the presence of
   `resamplers.<i>.0.weight` says which, and a transposed convolution in its
   place trains fine and matches badly.

And one that is not a convention but bites the same way: **vit-large's neck
runs two residual blocks per level where its own heads run one.** Reading a
single count off the neck and using it for the heads asks for weights that are
not there; reading it off a head and using it for the neck silently skips a
block.

## The token budget, not the image size

MoGe resamples its input to a grid of about `num_tokens` patches at the image's
own aspect ratio, runs the ViT there, and resamples the prediction back to
whatever size went in. So **the image size does not set the cost** -- this
does, and the prediction comes back at exactly the resolution it was handed,
with no granularity to round to (`Predictor::sizeGranularity()` is 1 for MoGe
against Metric3D's 28).

`--num-tokens` is capped at what the image holds: asking for more tokens than
it has patches would upsample into the network for nothing. At
`--max-size 1064` a 16:9 frame tops out around 3200.

RTX 5070 Laptop, a 1064x598 frame, the second call onwards (the first builds
pipelines and measures the GEMM tiling):

| tokens | vit-small | vit-base | vit-large |
|---|---|---|---|
| 1200 | 113 ms | 125 ms | 212 ms |
| 1800 | 166 ms | 199 ms | 338 ms |
| 3600 (capped ~3200) | 300 ms | 390 ms | 670 ms |

Load time is 0.5 / 1.3 / 4.1 s. MoGe's own inference offers 1200..3600 and
defaults to the top, which is what `spirula geometry` defaults to.

The arena is planned up front (`Model::planArenaBytes`) because it will not
grow while anything is live -- growing rebases it and invalidates every pointer
handed out before, which is silent corruption rather than a fault, so
`Predictor::predict` checks the capacity did not move rather than trusting the
plan. Peak is the level-3-to-4 resampler, whose upsampled copy is 64 channels
at 16x the patch grid.

## Turning a point map into a depth map

The network's `points` is affine: the camera-space point is `(X, Y, Z + shift)`
for one unknown `shift`, and `Z` is an exponential so it is always positive.
`model/Recover.cpp` solves

```
min over shift of  sum | focal * xy / (z + shift) - uv |^2
```

on a 64x64 nearest-neighbour sample of the map, restricted to the pixels the
mask kept -- which is MoGe's own `solve_optimal_shift`, with two differences:

- **The focal is known**, because the caller hands us the pinhole face's. MoGe
  recovers it from the point map instead, which is a guess; pass `fx = 0` and
  this does the same, by the closed form for the optimal focal at a given
  shift. `moge_test` checks both against an analytic plane.
- **A bracketed scan and golden section**, not the Levenberg-Marquardt scipy
  runs. The objective has a barrier at `shift = -min(z)` that a step-based
  method walks into.

Non-square pixels are absorbed into the `v` target rather than carried as a
second focal, which keeps this a one-parameter fit and is exact wherever
`fx == fy` -- which is always, for a split face.

`depth = (z + shift) * metric_scale`, in metres, and 0 wherever the mask scored
below 0.5 or the shifted z came out negative.

## Testing

```bash
./build/nn_ops_test                 # the general ops, vs a scalar CPU reference
./build/moge_test                   # cached checkpoints, or SKIP
./build/moge_test --model M --image IMG.jpg --num-tokens 1800 --repeat 3
spirula geometry --check            # the camera round trip, no network in it
```

`moge_test` is a shape and sanity gate plus one real check: the shift solve
against an analytic plane, at three shifts, with the focal known and unknown.
It cannot check the network's numbers -- we do not own these weights and cannot
embed a golden copy.

The gate that matters is parity against onnxruntime:

```bash
pip install onnx onnxruntime numpy
SS_MOGE_F32_WEIGHTS=1 SS_MOGE_DUMP=/tmp/ours \
    ./build/moge_test --model moge2-vitb --image IMG.jpg --max-size 448 \
    --num-tokens 800
python3 tools/moge/compare_ort.py --onnx <the same .onnx> --ours /tmp/ours
```

Measured over every stage, worst element and worst relative L2:

| | vit-small | vit-base | vit-large |
|---|---|---|---|
| f32 weights, no cooperative matrix | 2.5e-5 / 2.6e-6 | 2.3e-5 / 5.1e-6 | 3.3e-5 / 3.8e-6 |
| f32 weights | 2.5e-3 / 6.5e-4 | 4.1e-3 / 1.2e-3 | 2.0e-3 / 7.4e-4 |
| f16 weights (what ships) | 4.3e-3 / 1.2e-3 | 5.4e-3 / 1.3e-3 | 2.3e-3 / 9.6e-4 |

The first row is the one that says the arithmetic is right: at 2.6e-6 relative
L2 over a whole forward pass there is nothing left but fp32 accumulation order.
The other two are the tensor-core path -- `flash_attn_coop` takes fp16 operands
for `Q @ K^T` whatever the weight dtype, which is why turning the weights to
f32 alone does not get you the first row. `SS_NN_COOPMAT=0` and
`SS_MOGE_F32_WEIGHTS=1` are what make the comparison tight enough that a real
bug cannot hide under the rounding; neither is a quality knob.

## Not done yet

- **Batching the faces.** A split frame runs its faces through the network
  one at a time, as `src/metric3d/` does.
- **The panorama path.** MoGe ships an `infer_panorama.py` that splits an
  equirectangular frame into 12 overlapping views and merges in the spherical
  domain. `app/GeometryWarp.h`'s 6-face cube map with a log-median alignment is
  what runs instead, and it is the same idea with fewer faces.
