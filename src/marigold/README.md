# Marigold v2 depth and normals (`src/marigold/`)

[Marigold v2](https://github.com/huawei-bayerlab/marigold-v2) (HUAWEI Bayer
Lab, SIGGRAPH Asia 2026) fine-tunes Qwen-Image-Edit-2509 — a 20-billion-
parameter diffusion transformer (DiT) and its VAE — into one-step dense
predictors. This directory runs it on the inference layer (`src/nn/`) with no
PyTorch, no bitsandbytes and no converter step for the user: `spirula geometry
--model marigold-v2` and the GUI's geometry model list reach it through
`app/GeometryModel.h`.

It is the slowest geometry model here by two orders of magnitude and the one
with the finest detail. It exists for users who want the best geometry a
monocular prior can give and will wait for it.

## What is used, and why

| | checkpoint | output |
|---|---|---|
| depth | `depth/Log-stage2` (the paper's model) | affine-invariant **log** depth |
| normals | `normals` | camera-space unit normals |

`Log-layered` (see-through depth behind glass) is the wrong surface for
splatting, the disparity and uniform ablations are worse than `Log-stage2` in
the paper, and albedo is not geometry. Each task is a rank-128 LoRA on every
projection of the DiT plus its own fine-tuned VAE decoder; the base and the
encoder are shared.

**Depth needs an anchor.** The prediction is `log z = a + b * rel` with `a`
and `b` unknown per image. `app/GeometryModel.cpp` runs MoGe-2 base on the same
face and fits `a, b` by least squares in log space (refitted on the 90% of
pixels that fit best). That makes the output metric like MoGe's, lets the
warp blend split faces in one unit, and reuses MoGe's sky mask: both maps
are written as "no ground truth" where MoGe gave up. MoGe costs ~0.4 s beside
Marigold's seconds.

**Normals** come out in OpenGL's axes (y up, z towards the viewer); the
trainer reads OpenCV's facing the camera, `(x, -y, -z)`.

## Install: 45 GB in, 11 GB kept, no shard ever on disk

`model/Install.cpp` streams the published files through `curl` (Hugging Face;
the same bytes on ModelScope when a request fails) and converts each tensor as
it arrives:

- every DiT projection is quantized to **NF4 exactly as bitsandbytes does**
  (blocksize 64, the even element in the high nibble), because that NF4 base is
  what the LoRA was trained on. The scale is stored as bf16, which is lossless:
  the max of bf16 weights is a bf16. The timestep embedding goes through it and
  comes out bit-identical to bitsandbytes' (below).
- everything that only ever sees one value is **spent**: the network runs at a
  single timestep and a fixed prompt, so the timestep embedding, all 120
  `img_mod` / `txt_mod` projections (6.8B parameters, a third of the DiT), the
  final AdaLayerNorm's `(scale, shift)`, and the text stream's input
  `txt_in(txt_norm(prompt))` are computed once and stored as vectors.
- the VAE is a causal 3D conv net; on one frame each 3D conv sees only its
  last temporal tap, so only that 2D slice is kept.

| file | holds | size |
|---|---|---|
| `base.safetensors` | the NF4 DiT, the modulation vectors, the VAE encoder | 7.30 GB |
| `depth.safetensors` / `normals.safetensors` | the LoRA (fp16, A transposed), the decoder, the task's constants | 1.75 GB each |

Peak disk is the final size: each output is sized up front and filled as its
tensors arrive. Every source's SHA-256 is hashed on the fly, and the hash state
is checkpointed with the byte position at tensor boundaries, so an interrupted
install resumes at the tensor it stopped on and still verifies the whole file.
The free space is checked before anything streams (`InstallNeeds`, 1 GB kept
spare), on the command line and in the GUI. `SS_HF_ENDPOINT` replaces
`https://huggingface.co` (a mirror, or `file://` for a local copy).

Measured: 44.8 GB streamed from Hugging Face in 561 s (80 MB/s); from a local
copy (`SS_HF_ENDPOINT=file://...`) 145 s, bound by hashing and quantizing.

### The constants are bitsandbytes' arithmetic, not ours

The constants above were computed in training under bf16 autocast, so they are
reproduced in its precision (`model/HostMath.cpp`). Two traps:

- A one-row input goes through bitsandbytes' `gemv_4bit`, whose code book is
  **bf16** (`bf16(bf16(level) * absmax)`, not dequantize_4bit's
  `bf16(level * absmax)`), which rounds every product to bf16, and which adds
  the bias after rounding. Emulated, the timestep embedding is bit-identical to
  bitsandbytes' and 8 of 2.2M modulation values differ (block 0's one
  unquantized projection, which goes through cuBLAS).
- The timestep frequencies are fp32 (diffusers' `embeddings.py`); the
  transformer file's own copy casts them to bf16 and is not the one called. At
  500 rad one bf16 step is a different angle.

Multi-row cuBLAS GEMMs (the text stream) use reduced-precision bf16
reductions that vary by GPU; the converter computes the exact sum, and the
reference sets `allow_bf16_reduced_precision_reduction = False` to match.

## The forward pass

`model/Dit.cpp`, one pass per task over a batch of images:

- **Expansion.** Each projection's fp16 weight is built just before its GEMM:
  the LoRA's `B A` (a tensor-core GEMM, rank 128) plus the NF4 base
  (`nn::dequant_nf4`), times the block's gate where it multiplies the output.
  Biases are gated the same way. Scratch is one projection: 75 MB fp16 + 151
  MB fp32.
- **Fused q/k/v** in the file, so one expansion and one GEMM serve all three.
- **Activations run to 3e9** in the residual stream (Qwen-Image's massive
  activations) and to 5.3e4 at the text MLP's output projection on an ordinary
  photo. The stream is fp32; the tensor-core GEMM's fp16 input is prescaled by
  2^-8 (`LinearOpts::x_scale`).
- **Attention is fp32.** Qwen's q/k-normed logits are large enough that fp16
  `Q K^T` (the cooperative-matrix attention) moved block 0 by 3e-2 relative
  and the depth by 1.3e-2; the scalar kernel keeps both near 1e-4 for 12% more
  time.
- The final projection and the latent unpacking are one
  `conv_transpose_patch`: its packed column order is diffusers'
  `_pack_latents` order, the latent std is folded into the weight and the
  per-subpixel bias goes on as a 2x2 tile.

`model/Vae.cpp`: the mid-block attention has one 384-wide head, past the fused
kernel's 256, so it is chunked GEMMs and a row softmax. The decoder's
upsample-and-conv runs in bands of rows; its full-size input would be the
largest buffer in the VAE. The posterior mean is used where Marigold samples:
its std is ~2e-4 against unit latents.

## Memory: RAM, VRAM and PCIe

`model/Streamer.cpp` keeps as many of the 60 blocks resident as the VRAM plan
allows (149 MB each: NF4 base + the current task's LoRA) and streams the rest
through two device slots. A thread reads a block from the file into pinned
memory; the copy runs on the device's **transfer queue** (`nn::vk::Transfer`,
a DMA engine beside the compute queue), ordered against the compute stream by
two timeline semaphores. Without a transfer-only queue family the copies ride
the compute queue in order, and only the file reads overlap.

The weights are read from the file every pass and never held in process
memory: the OS page cache keeps what fits, so host RSS stays near 1 GB.
`SS_MARIGOLD_VRAM_MB` caps what Marigold plans for (tests, a shared GPU).

RTX 5070 (12 GB), both maps, 2048x1360 photo at 3626 tokens:

| VRAM plan | blocks resident | seconds an image | peak VRAM |
|---|---|---|---|
| everything | 60 | 12.3 | 11.1 GB |
| `SS_MARIGOLD_VRAM_MB=6000` | 22 | 12.2 | 6.1 GB |
| `SS_MARIGOLD_VRAM_MB=4500` | 12 | 12.2 | 4.6 GB |

The streaming hides completely. A depth pass at that size is 5.2 s: MLPs 2.1 s
and q/k/v 0.9 s (GEMMs at 16–17 TFLOP/s), attention 2.0 s (fp32, 5.8 TFLOP/s),
output projections 0.25 s.

**Batching.** Images of one size share a pass (`Predictor::batchCapacity`,
from the VRAM plan): the expansions and the streamed weights are spent once
per batch. At ~1000 tokens, both maps: 4.24 s an image alone, 3.16 s in a
batch of six. `spirula geometry` batches whole frames, so a fisheye's or a
panorama's faces and consecutive pinhole frames go together. A batch returns
each image exactly what it returns alone (`marigold_test --batch`).

## Processing size

`--num-tokens` sets the cost, as for MoGe: tokens are 16x16 pixels, and an
image holding more is downscaled to the budget (Pillow-style bicubic); one
holding fewer runs at its own size. The default 3600 is ~1 MP; Marigold v2 was
trained at 768x576 (1728 tokens) and evaluated at native resolution.

## Layout

```
Marigold.h              the public surface: Predictor, PredictOptions, Prediction
Common.h                names borrowed from nn/
model/Layout.h          the network's shape and the installed files' tensor names
model/Fetch.{h,cpp}     the sources (URL, mirror, SHA-256) and the install paths
model/Remote.{h,cpp}    a byte range of a remote file through curl, read ahead by a thread
model/HostMath.{h,cpp}  NF4 and the bf16 arithmetic the installer reproduces
model/Install.{h,cpp}   the streaming converter, its resume and disk checks
model/Files.{h,cpp}     raw and NF4 reads of the installed files
model/Streamer.{h,cpp}  block residency and streaming
model/Dit.{h,cpp}       the transformer
model/Vae.{h,cpp}       the VAE encoder and decoder
model/Predictor.cpp     sizing, the VRAM plan, the passes, post-processing
tests/marigold_test.cpp host checks, installed constants, predictions
```

## Testing

```bash
./build_vulkan/marigold_test                                  # host arithmetic
./build_vulkan/marigold_test --install depth,normals          # or `spirula geometry --install marigold-v2 --depth`
./build_vulkan/marigold_test --constants REF --task depth     # installed constants vs a reference run
./build_vulkan/marigold_test --image IMG --task both --tokens 3600 --repeat 2 --batch 4 --out DIR
SS_MARIGOLD_DUMP=/tmp/ours ./build_vulkan/marigold_test --rgb RGB.npy --task depth
python3 tools/marigold/reference.py --compare REF --ours /tmp/ours
```

`tools/marigold/reference.py` is the parity reference: diffusers' own
`QwenImageTransformerBlock` and VAE in PyTorch, one block on the GPU at a time,
with weights put through bitsandbytes' NF4 round trip and the constants
computed by bitsandbytes' own `Linear4bit`. It needs the original checkpoints.
On a 576x384 photo against its fp32 mode:

| | depth (rel L2) | final latents | block 59 |
|---|---|---|---|
| `SS_NN_COOPMAT=0` (fp32 everywhere) | 1.3e-4 | 1.2e-4 | 2.5e-5 |
| default (tensor-core GEMMs, fp32 attention) | 3.1e-4 | — | — |
| Marigold v2's own bf16-autocast inference | 1.8e-2 | 2.6e-2 | — |

The last row is the reference run the way Marigold v2's own code runs it, for
scale: the default here is closer to the fp32 model than the original is.
Normals, default path: 1.0e-3 on the decoded image.

## Not done yet

- **fp32 attention is 40% of a pass.** A split-precision (hi + lo fp16)
  `Q K^T` on tensor cores would keep the accuracy at a fraction of the time.
- **The text stream costs 340 tokens a face.** Small split faces pay it each;
  nothing in the model shares it.
- **No tiling in the VAE.** The working memory is the VAE's, 1.5 KB a pixel:
  1.45 GB at the default ~1 MP, 3.2 GB at 2 MP. Banding the full-resolution
  blocks as the upsample is banded would bound it.
