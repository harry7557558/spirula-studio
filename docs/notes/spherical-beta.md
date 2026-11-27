# Spherical-beta lobes

Spherical beta (SB) is the view-dependent colour of Deformable Beta Splatting
(Liu et al., SIGGRAPH 2025): a few explicit specular lobes per splat in place
of, or on top of, the SH bands. The Gaussian kernel and the 3dgs / mip / 3dgut
primitives are unchanged; only the colour a splat shows the camera is. CUDA
builds render lobes (`kernels/projection/ProjectionLobes.cu` adds them after
the projection) but train only on Vulkan: the CUDA backward refuses them.

## The model

A lobe is six raw numbers, `(r, g, b, theta, phi, beta)`. With `d` the unit
direction from the camera to the splat (the one SH is evaluated in),

    axis   = (sin theta cos phi, sin theta sin phi, cos theta)
    colour = max(SH(d) + 0.5, 0) + sum over lobes of
             softplus(rgb) * max(dot(d, axis), 0) ^ (4 exp(beta))

`softplus` has sharpness 10 ln 2, so `softplus(0) = 0.1`. The lobes sit
outside the SH clamp, and their amplitude is non-negative, so they only ever
add light. This is the formulation of the reference code, gsplat's port and
PlayCanvas's renderer, and our files open in the latter two.

`--sb-lobes N` (1..8) turns them on; unless `--sh-degree` is also given it
drops to 0, so the lobes replace the bands -- the published setting is SH 0
with 2 lobes. Both together are allowed: the row then holds both. On
`--resume` the checkpoint's degree stays, so `--sb-lobes` adds lobes to a
trained model rather than discarding its bands.

## Where the lobes live

One splat's view-dependent colour is one row of `features_sh`: the SH bands
past the DC, then the lobes, two float3 each -- `(r, g, b)` and
`(theta, phi, beta)` (`core/ColorBasis.h`). Keeping them in the SH row rather
than a seventh attribute is what makes every path that only moves rows work
unchanged: densify's copies and relocations, the q8/q16 value codec in both
of its layouts, the 8-bit Adam state, the grad-quant accumulator, checkpoint
save / resume, scene slots. The cost is that a row's width no longer says the
SH degree, so the split travels with the row as a `ColorBasis`.

What does know the split:

| what | where |
|---|---|
| colour and its VJP | `src/shaders/spherical_beta.slang` (one lobe), `backend/vulkan/shaders/view_color.slang` (the row, fp32 or codec), shared by projection fwd / bwd / grad-quant / FPBO |
| per-cell optimizers | `sb_cells.slang` + `SbOptimCells`: own rate, no regularizer, no colour trust region; FPBO the same per splat |
| the file | `checkpoint/SplatPly.{h,cpp}`, `EngineCheckpoint.cpp` |
| a placement baked on save | `checkpoint/SplatTransform.cpp`: the axis turns with the model |
| seeding, resume, merge | `TrainerCore.cpp`, `checkpoint/Adapt.cpp`, `checkpoint/SplatMerge.cpp` |
| the standalone viewer | `viewer/src/viewer.cpp` activates lobes at load, `viewer/js/shaders.js` adds them |

## Training

A fresh lobe starts at amplitude 0 (0.1 after the softplus) and sharpness 0
(exponent 4), as the reference trainers seed it, but with its axis uniform on
the sphere (`theta = acos(1 - 2u)`) where they draw theta uniformly, which
crowds the poles. All six numbers share one Adam rate, `--features-sb-lr`.
`--sh-reg` does not reach the lobes, which generalize to held-out views
without it.

Every gradient flows: amplitude, axis, sharpness, and through the view
direction to the mean, as the SH bands' does. The weight's slope `n d^(n-1)`
is unbounded at the hemisphere's edge when `n < 1` (beta < -ln 4), and the
reference trainer takes it as it is; it only feeds the lobe's own Adam
there. Here it also reaches the mean, and in the split path an 8-bit
gradient block whose bound one spike sets, so for `n < 1` the backward
evaluates the slope no nearer the edge than `d = 1e-3` -- inside that, an
fp32 dot of unit vectors is mostly rounding -- which caps it near 53 times
the amplitude. The forward is exact, and so is the gradient for `n >= 1`,
which is continuous at the edge.

The lobes take no trust region. The colour one (3DGS^2-TR, on for linear
colour) bounds a band's step by how far it moves the splat's colour. Carried
to a lobe through its slopes in the worst view direction, it cost bonsai
0.08 dB and left the hdr preset on a private dataset within run-to-run spread
(below), so it was dropped.

## Quantized training

At the default `--quantization-level 1` the lobes ride the row's codecs as
they are: 16-bit values with one bound per 256 splats, and the 8-bit joint
`(u, log s)` Adam state. Nothing there is lobe-specific, and nothing needed
to be -- with one exception that was a bug for SH as well.

Densification copies a row into its destination's block, and that copy used
to clip to the block's current bound. A block an add is still filling has the
bound `(0, 0)`, so an added splat came out with every cell 0. For SH that
only drops the source's view dependence. For a lobe, zero is a lit lobe
(`softplus(0) = 0.1`, exponent 4) facing +z, every lobe of the splat the
same one, and the axis starts at the pole, where `phi` has no gradient. On
bonsai, 63% of the lobe axes ended within 37 degrees of +z (`cos theta >
0.8`) against 13% at level 0. The copy now records how far the destination
bound must widen, re-encodes the blocks that widen, then copies
(`kernels/densify/DensifyQuantCopy.cuh`, `densify.slang`); `densify_parity`
checks that every copied row decodes to its source's.

## Rotation

Turning a model by `R` turns each lobe's axis to `R axis`, written back as
angles (`sb::rotate_lobe`); amplitude and sharpness do not change. theta comes
back in `[0, pi]` and phi in `(-pi, pi]` -- the same axis, which is all the
colour depends on. `splat_transform_render` renders a model, bakes a
similarity into it, moves the camera by the same similarity and requires the
same image (mean |d| ~3e-7), and a control with unturned lobes must differ.

## The file

After `f_rest_*`, `sb_params_{f * L + l}` holds field `f` of lobe `l` --
field-major, as Deformable Beta Splatting's code and gsplat write it. A file
without them is an ordinary 3DGS PLY, and a viewer that does not know them
shows the base colour. The reference code's own reader also wants SH named
`sh0_*` / `shN_*` and a `beta` kernel property, so it does not open these;
gsplat and PlayCanvas do. `spherical_beta_test` pins the layout against a
file written by hand.

## Tests

- `sb_parity` (Vulkan): the projection forward against a double-precision
  host model in fp32 and through both codec layouts at 8 and 16 bits; the
  projection backward against central differences (lobes, mean, DC); the
  grad-quant accumulator within half a quantum of the plain backward; FPBO's
  in-kernel gradient against the plain one.
- `fpbo_split_parity` runs again with lobes: one step of FPBO and of the
  split-batch optimizers must agree at both quantization levels.
- `splat_transform_render`, `spherical_beta_test`: above.

## Results

mip-NeRF 360 at 30k steps, the default `3dgs` preset (1M splats), quantization
level 1, FPBO; `images_2` (`images_4` for garden), every 8th view held out.
`cc_psnr` is after a per-image colour correction. One run each, RTX 5070.

| scene | SH 3: psnr / cc_psnr / ssim | 2 lobes: psnr / cc_psnr / ssim |
|---|---|---|
| bonsai | 31.31 / 33.03 / 0.949 | 31.57 / 33.29 / 0.949 |
| counter | 28.70 / 29.62 / 0.917 | 29.55 / 30.15 / 0.920 |
| garden | 26.68 / 27.45 / 0.854 | 27.07 / 27.46 / 0.852 |
| room | 31.35 / 32.31 / 0.929 | 31.36 / 32.35 / 0.929 |

Two lobes match or beat SH 3 in psnr on all four, and ssim is within
0.002. With `--sh-reg` also applied to the lobes, as an earlier build did,
2 lobes scored 31.48 / 29.44 / 27.01 / 31.42 psnr. On bonsai (the last two
rows are from that build):

| bonsai | psnr | cc_psnr |
|---|---|---|
| SH 3 | 31.31 | 33.03 |
| 2 lobes | 31.57 | 33.29 |
| 4 lobes | 31.67 | 33.29 |
| SH 3 + 2 lobes | 31.95 | 33.59 |
| 2 lobes, split path | 31.51 | 33.26 |
| SH 3, split path | 31.19 | 33.02 |
| 2 lobes, level 0 | 31.54 | 33.27 |
| SH 3, level 0 | 31.20 | 32.95 |
| 2 lobes, `--features-sb-lr 0.005` | 31.50 | 33.12 |
| 2 lobes, lobe trust region | 31.40 | 33.09 |

Private dataset (hdr preset, 10k steps, 2 lobes, two runs each): cc_psnr
31.759 / 31.748 without the lobe trust region, 31.741 / 31.778 with it.

Cost on bonsai, summed GPU time over the run (`SS_PROFILE=1`) and the splat
buffers' high-water at 1M splats:

| bonsai | FPBO | all kernels | splat VRAM |
|---|---|---|---|
| SH 3 | 201 s | 539 s | 351 MiB |
| 2 lobes | 59 s | 388 s | 222 MiB |
| 4 lobes | 82 s | 410 s | 267 MiB |
| SH 3 + 2 lobes | 259 s | 597 s | 397 MiB |

In the split path the projection backward and optimizers take 77 s with
2 lobes against 218 s for SH 3. `fpbo_bench 2000000 40 <sh> <lobes>` times
the kernel alone: 20.7 ms (SH 3), 5.9 ms (2 lobes), 8.3 ms (4 lobes). An
SH-only model costs what it did before lobes existed (21.0 ms here against
21.4 ms on master).
