# Per-pixel VRAM at high resolution

`docs/notes/vram-splat-x-img.md` is about the memory that follows the splat
count. This note is about the other half: buffers sized by the image. At
60 MP a single float32 RGB image is 689 MiB and a float32 scalar map 230 MiB,
so a step that keeps a dozen of each spends more on pixels than on a
million-splat model. What one image costs is what decides whether a capture
trains at all, and the training loop never splits an image to save it (see
"Tiling" below for why that stays a fallback).

## The measuring set

An user-provided dataset (6336x9504, 60.2 MP) on the RTX 5070
(12 GB), 1M splats, `SS_PROFILE=1`, 100 steps. "Worst" is the most expensive
combination the presets offer: `hdr` + `--floater-suppression strong` +
masks + depth maps (`--depth-supervision-weight 0.05`) + normal maps, the
depth and normal maps at `spirula geometry`'s 1064 px. The 120 MP rows are
40 of its frames upscaled to 8960x13440, 30 steps.

| pool + scratch, MiB | CUDA, before | CUDA, after | Vulkan, before | Vulkan, after |
|---|---|---|---|---|
| `hdr`, 60 MP | 9211 | 4057 | 9164 | 3919 |
| worst, 60 MP | OOM (13.5 GB projected) | 8696 | OOM | 8576 |
| `hdr`, 120 MP | OOM | 7180 | OOM | 7180 |

Per pixel, `hdr` went from ~140 bytes to ~60 and worst from ~220 to ~141.
Time for the 100 `hdr` steps at 60 MP: 55.7 s to 50.0 s on CUDA, 66.7 s to
60.8 s on Vulkan -- the fused appearance chain takes passes out of each
direction, and float16 images halve what the grid and the loss read. At
120 MP a step is 0.49 s (CUDA) and 0.81 s (Vulkan).

Quality (`cc_psnr`, which moves ~0.1 dB between identical runs), `hdr` at
quarter resolution, 3000 steps: 30.74 / 30.74 (CUDA) and 30.73 / 30.83
(Vulkan) before; 30.81 / 30.61 / 30.75 / 30.77 / 30.75 / 30.68 (CUDA) and
30.79 / 30.77 (Vulkan) after. With strong floater suppression, 30.71 before
and 30.70 after the fused chain (means of five). `meshing` at an eighth,
2000 steps: 28.69 before and 28.69 after (means of five, 28.62 to 28.78).

## What changed

### One kernel for the appearance chain

Between the rasterizer and the bilateral grid a render went through up to
three stages -- background blend, PPISP ahead of or after the display
encode, the encode -- each writing a new float32 image because its backward
needed its input. `kernels/pixelwise/AppearanceChain.cu` (and
`backend/vulkan/shaders/appearance_chain.slang`) runs them as one launch and
recomputes the intermediates in the backward from the raw render, which it
has to keep anyway. The background's transmittance gradient is added into
`v_render_Ts` in place, so its scratch map went too. What remains is the raw
render, the grid's input and the grid's output: three images instead of five.

`SS_FUSED_APPEARANCE=0` restores the per-stage kernels; `appearance_parity
check` holds the two paths together on either backend (72 configurations:
every background mode, PPISP layout and position, transfer and the
overexposure term). A step whose color-shift regularizer is on keeps the
per-stage path, because that term reads a stage's input.

### Ground truth in the form it was decoded in

`GTData::rgb` is a view whose element size says how the image is stored
(`core/PixelFormat.h`): an 8-bit image that needs no colour conversion stays
8-bit on the device (3 bytes a pixel instead of 12), and every reader -- the
per-pixel loss, the fused SSIM and its mask coverage, the pyramid's first
step, the viewer thumbnails -- decodes it the way the upload kernel did, so
the result is bit-identical (`pixel_format_parity`). The edge-aware densify
maps take float images; they widen a compact one into loss scratch.

### float16 between the rasterizer and the loss

`EngineStepConfig::optim.image_bits` (16 whenever `quantization_level` is 1,
the default) stores every full-resolution image after the rasterizer as
float16: the appearance chain's output, the bilateral grid's output, and the
render and reference levels of the loss pyramid. `ForwardCache::rgb_fmt` says
how `renders.rgb` is held at each point, and each stage reads and writes
through `core/PixelFormat.h`. A step whose color-shift regularizer is on keeps
float32, since that term reads a stage's input.

float16 is the encoding an HDR value range wants: it rounds to within 2^-11
of the value everywhere from 6e-5 to 65504 -- an eighth of an 8-bit level at
white, and the same fraction at the darkest normal value. A linear 16-bit code
has that precision only near white, and RGBE's 8-bit mantissas are four times
coarser on the brightest channel and worse on the others. The gradients, the
transmittance and the raw render a distortion term reads stay float32: they
are differences of nearly equal numbers, or are scaled down by the pixel count
far below float16's range.

The appearance chain also writes a float16 copy of the raw render and computes
from that rounded value, so its backward can replay it exactly. Two buffers
follow from the copy:

- With no distortion term (which is what reads the float32 raw render), the
  loss puts `v_rgb` in the raw render's buffer instead of a buffer of its own.
- When a bilateral grid reads the chain's output, that output is dead from the
  grid's forward to the grid's backward. It is a slice of the arena's
  tile-intersect phase, which is open from the intersection to the loss and
  is the smaller phase at high resolution, and the grid's backward replays the
  chain into a new `ImageBwd` phase (one extra pass, ~6 ms at 60 MP).

`pixel_format_parity` checks that a float16 input gives exactly what the same
values as float32 give, and a float16 output is the float32 result rounded
once, for the chain, the three colour grids (forward and both backwards) and
the pyramid; `engine_train_parity` runs two steps of it across backends.

**Formats are compile-time on the hot paths.** Reading a format per load cost
the fused SSIM backward 65% and the PPISP grid backward 13% on CUDA, 55% and
15% on Vulkan. CUDA instantiates them per format
(`memory_efficient_ssim_backward_kernel`'s `kF1`/`kF2`, the grid kernels'
`kF` through `bilagrid_with_format`); Vulkan makes the formats specialization
constants (`kSsimFmts`, the grid modules' `kRgbFmt`/`kOutFmt`).

### The loss's scratch shares the alias arena

The multi-scale pyramid, its per-scale gradients, the coarse loss maps, the
NMS scratch and the perceptual losses' blur and coverage maps are all dead
when `compute_multi_scale_per_pixel_losses` returns, so they live in a
`PoolPhase::Loss` slice of the arena the tile intersector and the raster
backward already share. The arena then costs the largest of the phases, which
at high resolution is the loss.

### Depth only when something reads it

The rendered depth and its gradient are two float32 maps. A training step
none of whose terms reads depth -- no depth or normal maps, no depth
distortion, no median-depth terms -- tells the forward to leave it out.

### The RGB distortion gradient is derived

The loss gives the RGB and the depth distortion terms the same mask and the
same normalizer, so the RGB gradient is the depth one times
`w_rgb / (3 w_depth)`. The raster backward derives it instead of reading a
float32 RGB map (`raster_bwd_parity` checks the two give the same splat
gradients).

### Warp passes have a pixel budget

A panorama warped to pinhole faces trains each run of equal-size faces as
one pass, and a pass sizes every per-pixel buffer of the step. A 120 MP
equirect's faces are each ~15 MP, so the six of them made a 90 MP pass. Runs
are now cut at 16 MP (`kWarpPassPixelBudget`); the split's weighting already
makes the loss independent of where a run is cut.

## A fix found on the way

With RGB distortion on (floater suppression), the raster backward rebuilt
its second moment from `fwd.renders.rgb` -- which by then pointed at the
image after the background, the display encode, PPISP and the bilateral
grid, not at the colour the splats accumulated. The RGB distortion gradient
was computed against the wrong mean. `ForwardCache::raw_rgb` now carries the
rasterizer's own colour to the backward.

## The GUI's images view

The images view renders one training frame between steps
(`engine_preview_forward`) and reads a loss map off it. Its forward used to
skip the arming a step does before its own: float32 images, the rendered
depth, PPISP as a stage of its own, the grid's input in an owned buffer. The
map-only loss also gave `v_rgb` and `v_depth` buffers of their own. The pool
never shrinks, so the first refresh grew it for the rest of the run: `hdr` at
60 MP went from 4026 to 7327 MiB, with the arena growing from 648 to 1005 MiB
at the next step.

The preview now arms its forward with the step's own function
(`_engine_arm_step_forward`), runs PPISP and the grids in the step's order
(`_engine_step_image_stages`), and its loss puts `v_rgb` where a step's does.
The same refresh adds 0 MiB on either backend, and its render and error map
match the previous float32 ones to float16 rounding. With colour matching off
the chain's output keeps a float16 buffer of its own (345 MiB at 60 MP),
because no grid follows it to make it transient.

The view's three textures are ~0.7 GB at 60 MP. The GUI frees them while the
3D view is shown, but NVIDIA's GL driver hands back only about a third and
keeps the rest cached.

## float16 without 16-bit storage

The Vulkan baseline has no `storageBuffer16BitAccess`, so a kernel that
writes float16 cannot store a half by itself: two lanes would write the same
word. `rgb_store3_f16_pair` (`backend/vulkan/shaders/rgb_format.slang`) pairs
an even lane with the odd one after it -- which own pixels `2k` and `2k+1` in
a flat dispatch -- and the even lane writes both pixels' six halves as three
whole words. Every lane must reach the call, the dispatch must be flat (a
pixel's parity is its lane's), and the buffer must hold an even pixel count.

## What a pixel still costs

`hdr` at 120 MP, ~59 bytes a pixel:

| bytes | buffers |
|---|---|
| 12 | the raw render, float32; `v_rgb` after the loss |
| 12 | the grid's output and the raw render's float16 copy |
| 16 | transmittance, last splat ids, `v_Ts`, the densify loss map |
| 4 | SSIM mask coverage (masks or a saturation threshold) |
| 3 | the 8-bit ground truth |
| ~12 | the arena: the loss pyramid and per-scale gradients, which also hold the grid's input and its replay |

The worst case adds ~81: the RGB and depth distortion maps and their
gradients, `v_rgb` apart from the raw render (the distortion backward reads
the raw render), the rendered depth, the normals derived from it and their
gradients (~65 together), and their pyramid levels in the arena (~16). They
stay float32: the distortion backward rebuilds a second moment from the
distortion map and the raw render, the cancellation-prone step.

A panorama warped to pinhole faces is bounded by the pass budget, not by its
pixel count, so a 120 MP equirectangular image costs what a 16 MP pass does.

## Measured dead ends

- **The SSIM mask coverage as float16.** With masks or a saturation threshold
  (the `hdr` preset) the coverage map is 4 bytes a pixel. The window variances
  are E[x^2] - mu^2 over that coverage, a cancellation: a float16 rounding
  error e becomes an error of about e * mu^2 in the variance -- the size of
  SSIM's C2 constant in a flat window next to a mask.
- **The coverage computed inside the SSIM kernel.** It needs its own 34x34
  tile of shared memory, and the CUDA kernel already uses 47.7 of 48 KB (the
  Vulkan one 30 of the 32 KB an Apple GPU has); past that, occupancy halves.

## Tiling

gsplat and LichtFeld Studio split a large image into tiles, and splitting
the dataset that way has been reported to improve metrics -- it changes how
the data manager batches. It is kept as the fallback: a 120 MP photo should
train as one image, and everything above is what lets it.
