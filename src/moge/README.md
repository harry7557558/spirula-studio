# Monocular geometry, MoGe-2 and MoGe-3 (`src/moge/`)

MoGe-2 and MoGe-3 on the inference layer (`src/nn/`): one image in, a metric
depth map, a surface normal map and a validity mask out, with no onnxruntime,
no PyTorch and no converter.

Status: **done and wired in.** `moge2-vitb` is what `spirula geometry` runs by
default, and the forward pass matches onnxruntime on the same checkpoint to a
relative L2 of 3.8e-6. `moge3-vitl` is the same network plus MoGe-3's sparse
3D depth refiner, which matches an independent float64 reference to 4.3e-7 in
log depth ([MoGe-3](#moge-3-the-sparse-3d-refiner)). Both sit beside
`src/metric3d/` rather than replacing it; `app/GeometryModel.h` is the seam that
picks between them, and `--model` is what spells the choice.

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
  the model libraries with no `shaders/` directory at all: the ops it needed
  -- `padding_mode='replicate'` on a convolution, an `exp` GEMM epilogue, and
  for MoGe-3 a sparse convolution (`nn::sparse_conv`) and a segment mean -- are
  general, went into `nn/`, and are tested in `nn_ops_test`.
- **No camera model reaches this directory.** The network sees a pinhole image;
  `app/GeometryWarp.h` is the seam that resamples a fisheye or a panorama into
  one. What does reach it is the pinhole's four numbers, because the shift
  solve needs them.
- **The checkpoint is someone else's file, parsed in process**: an ONNX export
  for MoGe-2, the torch.save `.pt` for MoGe-3. We host nothing and convert
  nothing.

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
model/Fetch.{h,cpp}   the four ids: URL + SHA-256, on nn/io/Fetch.h
model/Weights.{h,cpp} ONNX initializers or .pt tensors -> device tensors; the
                        transposes and the f16 decision; the positional
                        embedding stays on the host
model/Model.h         the loaded model and the two halves of the forward pass
model/Encoder.cpp     DINOv2 without registers, read against
                        DINOv2's own vision_transformer.py
model/Head.cpp        the shared neck and the three heads, read against
                        ConvStack in MoGe's modules.py
model/Refiner.cpp     MoGe-3's Sparse3DUNet: the voxel pyramid on the host, the
                        network on nn::sparse_conv
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
`--max-size 1064` a 16:9 frame tops out around 3200. A split face is sized
to the map it is written into, which on a panorama is about 900 tokens, so
`app/GeometryModel.h`'s `minFacePixels` raises every face to 1200 patches
where the frame has the pixels for it (src/metric3d/README.md, "Face size").

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

## MoGe-3: the sparse 3D refiner

`moge3-vitl` is MoGe-2's network -- MoGe's `v3.py` subclasses `v2.py`, and the
encoder, neck and heads load and run unchanged -- plus `Sparse3DUNet`, which
moves each pixel's log depth before the head output is resized. It is what
sharpens depth edges: a 2D decoder blends the two sides of a depth
discontinuity, and the refiner works where they are apart.

**One refine step** (`model/Refiner.cpp`, `--refine-steps`, 3 by default as
trained):

1. The points head's raw `(u, v, log z)` map, at 16x the patch grid, becomes
   one voxel per pixel at `(row, col, round(log z * 256))`, binned in fp32 with
   ties to even as `torch.round` does, and offset so the smallest bin is 0.
2. Four 2x poolings build the pyramid down to the patch grid: a voxel's parent
   is its coordinates halved, and pooling is the mean of the children present.
3. A U-Net of residual blocks (`x + conv(silu(conv(silu(LN(x)))))`, channels
   32 to 512) runs on it. The bottleneck fuses the encoder's patch features,
   uv included, sampled at each voxel's `(row, col)`.
4. The output is one log-depth delta per pixel, added in place.

The convolutions are FlexGEMM's submanifold 3x3x3: a voxel's output reads the
27 voxels at offsets `(di, dj, dz)` in `itertools.product` order, absent ones
as zero, and the checkpoint's `[Cout, 3, 3, 3, Cin]` weight is already the
`[Cout, 27*Cin]` matrix of that gather. `nn::sparse_conv` runs it as an
implicit GEMM, because the column matrix at level 0 would be 3.2 GB a
convolution. The voxel bookkeeping -- bins, neighbour tables, pooling parents
-- is host work over one download per step, about 60 ms at 0.9 M voxels.

**The checkpoint** is `Ruicheng/moge-3-vitl`'s `model.pt`, read by
`nn/io/TorchCheckpoint.h`, which executes nothing in the pickle. Unlike the
ONNX file it names every tensor by module path in `[out, in]` layout, so
nothing is transposed. The tapped encoder blocks, the pooling factors and the
bin resolution come from its `model_config`; everything else from the weights.

**Cost.** RTX 4060 Laptop, a 1064x709 frame at 3600 tokens:

| | per image |
|---|---|
| `moge2-vitl` | 0.95 s |
| `moge3-vitl`, `--refine-steps 0` | 1.0 s |
| `moge3-vitl`, 1 step | 1.65 s |
| `moge3-vitl`, 3 steps | 2.45 s |

A step is ~60 ms on the host and ~410 ms on the device, of which level 0
(0.9 M voxels, 32 channels) is a quarter. Its convolutions are bound by the
gathered loads, not arithmetic: on tensor cores they run 10% faster.

**Parity.** No PyTorch run is possible on Windows -- the official refiner needs
FlexGEMM on Triton -- so `tools/moge/check_refiner.py` is an independent
float64 NumPy refiner written from MoGe's and FlexGEMM's Python, fed our
dumped head output and encoder features:

```bash
SS_MOGE_F32_WEIGHTS=1 SS_NN_COOPMAT=0 SS_MOGE_DUMP=/tmp/ours \
    ./build_vulkan/moge_test --model moge3-vitl --image IMG.jpg --max-size 640 \
    --num-tokens 1200
python3 tools/moge/check_refiner.py --pt <the moge3 .pt> --ours /tmp/ours
```

Over 301 k pixels and 3 steps, where the refinement itself moved log depth by
up to 0.29:

| | median | p99 | max |
|---|---|---|---|
| f32 weights, no cooperative matrix | 8.3e-9 | 6.2e-8 | 4.3e-7 |
| f16 weights (what ships) | 2.4e-6 | 8.0e-5 | 4.3e-3 |

The shipping row's worst pixels are fp16 rounding tipping a pixel into the
next depth bin, which changes its neighbours rather than the arithmetic.

## Testing

```bash
./build_vulkan/nn_ops_test                 # the general ops, vs a scalar CPU reference
./build_vulkan/moge_test                   # cached checkpoints, or SKIP
./build_vulkan/moge_test --model M --image IMG.jpg --num-tokens 1800 --repeat 3
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
    ./build_vulkan/moge_test --model moge2-vitb --image IMG.jpg --max-size 448 \
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
- **A faster refiner.** FlexGEMM's masked variant sorts voxels by which of the
  27 neighbours they have, so a tile can skip the taps none of its rows has;
  at level 0 about two thirds are absent. The neighbour tables could also be
  built on the device: level 0's is most of the 140 MB uploaded a step.
- **`moge3-vitg`.** The 1.25 B-parameter variant is not offered and untested.
  Its encoder is DINOv2's giant, whose MLP is SwiGLU (`ffn_layer='swiglufused'`)
  where the ViT-L one `model/Encoder.cpp` runs is fc1/GELU/fc2.
