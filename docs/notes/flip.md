# FLIP perceptual loss

`--perceptual-loss flip` swaps what `ssim_lambda` weights: instead of
`ssim_lambda * (1 - SSIM)` the photometric loss carries
`ssim_lambda * mean(FLIP)`, where FLIP is NVIDIA's LDR-FLIP (Andersson et al.,
"FLIP: A Difference Evaluator for Alternating Images", HPG 2020; reference
code at https://github.com/NVlabs/flip). FLIP already lies in [0, 1], so it
takes no rescale. The L1/L2 terms keep their `1 - ssim_lambda` share as
before. Status: a prototype for comparing against SSIM and S-CIELAB; the
default stays `ssim`.

## What is computed

Per window centre (`FusedSSIM.cu`'s `fused_flip_kernel`, and the `flip_*`
entries of `backend/vulkan/shaders/fused_ssim.slang`), for both images:

1. sRGB decode to linear, then Y, X - Y and Y - Z of D65-normalized XYZ. That
   is FLIP's YyCxCz less its offset and scale factors, which the normalized
   blurs commute with. The decode is right for any training colour space, as
   for S-CIELAB (`s-cielab.md`).
2. Colour: each channel through FLIP's contrast-sensitivity blur, back to XYZ,
   CIELAB, Hunt-adjusted (a and b scaled by L/100), and the HyAB distance to
   the reference taken the same way. HyAB^0.7 is redistributed with FLIP's
   pc = 0.4 and pt = 0.95 against cmax = 41.28 (HyAB^0.7 from Hunt-adjusted
   green to blue): that is the colour error ΔE_c.
3. Features: edge and point detectors (a Gaussian's first and second
   derivative, each signed lobe summing to 1, as FLIP normalizes them) on the
   unblurred linear luminance. ΔE_f = (max(|Δ‖edge‖|, |Δ‖point‖|) / √2)^0.5.
4. FLIP = ΔE_c^(1 - ΔE_f).

## The viewing distance

FLIP's filters are defined in degrees of visual angle. The reference default
is 67 px/degree (a 4K screen 0.7 m wide, seen from 0.7 m), where its CSF
radius is 10 px and the detectors' Gaussian has σ 2.75 px, which puts the
point detector's positive lobes at 4.8 px: none of it fits 11x11.

This uses 36.6 px/degree, chosen so the detectors' σ is SSIM's 1.5 px. At
that distance FLIP's own radius rule (3σ, rounded up) gives 5 for every
filter, so 11x11 is FLIP's footprint rather than a truncation of it.
36.6 px/degree is a 1920-pixel-wide image filling about 52° of view, a desktop
monitor at arm's length. Each coarser level of the loss pyramid
(`loss_scale_min_pixels`) halves it.

| filter | σ (px) | taps |
|---|---|---|
| achromatic CSF | 0.55 | radius 2 (the dropped taps weigh < 6e-7) |
| red-green CSF | 0.59 | radius 2 |
| blue-yellow CSF | 0.76 of σ 1.65, 0.24 of σ 1.30 | radius 5 |
| edge / point detectors | 1.5 | radius 5 |

The taps are `cFlipTaps` (CUDA) and `kFlTaps` (Slang), copied rather than
shared, like S-CIELAB's.

## Where it departs from the reference

- **Blue-yellow blur.** FLIP's is a sum of two Gaussians, which is not
  separable. Its 1D marginal is applied along x and then y: at most 7e-4 off
  per 2D tap, 1% in L1 over the kernel.
- **No clamp after the blur.** The reference clamps the blurred linear RGB to
  [0, 1], its LDR assumption. Here CIELAB's cube root continues above 1 and its
  linear toe below 0, so a render that overshoots a highlight or goes negative
  still has an error and a gradient. ΔE_c can then pass 1; ΔE_f is clamped to
  1.
- **Borders and masks.** The reference pads the CSF input by replicating the
  edge and the detectors' by reflection. Here taps off the image, under the
  mask, or dropped by the clip test leave the window. A CSF blur divides by
  the weight left, as S-CIELAB does; a detector reads each dropped tap as the
  centre pixel's own value, so a flat region scores no feature error at the
  image border or at a mask edge. `mask_loss_semantics` checks both, and that
  the mask path's coverage matches the closed form used without one.
- **Gradient floors.** FLIP is a power law near a perfect match (HyAB^0.7,
  then ΔE_c^(1 - ΔE_f), and ΔE_f is a square root), so its exact gradient is
  infinite there. The gradient alone treats HyAB below 1 L* (about one JND) as
  1 and ΔE_c with it, and a feature difference below 0.002 in linear luminance
  (about half an 8-bit step at mid grey) as 0.002. The reported value and the
  map are exact.

None of the first three moves the number on real renders. Scored in numpy
against a reimplementation of the reference's `flip.py`, on the 37 held-out
views of two 3000-step mip-NeRF 360 bonsai runs (`images_4`), the per-pixel
maps correlate at 0.9998 with the reference run at 36.6 px/degree, and the
mean FLIP agrees to four digits. The reference at its own 67 px/degree scores
the same renders 3% lower.

## The kernel

One fused forward+backward kernel per 16x16 block, like S-CIELAB's. Per image
it decodes each pixel of the tile once into all three channels, runs the five
horizontal passes the luminance and red-green tiles need (the detectors'
three, A and RG) together, then one vertical pass for every window centre's
four detector responses and two blurs, and blue-yellow last: four barriers
per image. Each thread keeps the window centres it owns (3 of the
block-plus-halo's 26x26) in registers across the passes, so shared memory
holds only the tiles and, at the end, the seven adjoint grids: 31.4 KB, under
the 32 KB floor. With a mask or the clip test on, a pre-pass writes each
pixel's seven window coverages as float16 (16 B per pixel,
`PoolSlot::FlipMaskWeight`).

Per call on bonsai at 780x520 (Vulkan, RTX 3070 Laptop, GPU timestamps under
`SS_PROFILE=1`, 600 steps, each loss run first once and last once): FLIP
514-521 µs, SSIM 516-586 µs, S-CIELAB 400-445 µs. Loading one channel per
tile, with three passes per load, put FLIP at 934 µs.

## Densification map

`densify_loss_map_mode auto` takes `flip_feature` when training FLIP: the
feature error alone marks where edges and points disagree, the structure more
splats can add, and leaves out flat colour shifts that more splats do not fix
-- the same reason `ssim_cs` drops SSIM's luminance term. `flip` maps FLIP
itself, and both have `_nms` variants. A FLIP map while training SSIM or
S-CIELAB runs the FLIP kernel for its map alone, without a gradient, and the
other families' maps work the same way while training FLIP; `loss_full` adds
the trained term. A map of the perceptual term alone is weighted as in
`s-cielab.md`.

## Display

The CLI and the loss dict report `flip` (mean FLIP over all pixels, masked
ones counting 0) in place of `ssim` when FLIP is trained; `rgb_loss` includes
`ssim_lambda * flip`.

## First numbers

Smoke runs only: mip-NeRF 360 bonsai, `images_4` (780x520), 3000 steps, every
8th view held out (37), Vulkan on an RTX 3070 Laptop (2026-10-19), one run
each unless two are given. Held-out FLIP is scored in numpy as above.

| run | psnr | ssim | FLIP |
|---|---|---|---|
| `ssim` (default) | 28.69 | 0.933 | 0.1259 |
| `s_cielab` | 28.52 | 0.914 | 0.1275 |
| `--ssim-lambda 0` (L1 only) | 28.40 | 0.916 | 0.1289 |
| `flip` | 28.29, 28.41 | 0.911 | 0.1290, 0.1275 |
| `flip`, `ssim_cs` map | 28.26 | 0.913 | 0.1285 |
| `flip`, `--ssim-lambda 0.1` | 28.40 | 0.913 | 0.1271 |
| `flip`, `--ssim-lambda 0.5` | 28.07 | 0.908 | 0.1297 |

At this length training on FLIP does not lower held-out FLIP below what L1
or S-CIELAB reach (the two `flip` runs differ by 0.0015), and SSIM is ahead on
all three numbers. On CUDA the same `flip` run lands at 28.24-28.28 and
trains in the time SSIM does (52.6 s against 52.2 and 54.9 s). It also trains
cleanly on a 360-camera capture with a quarter of every frame masked, on both
backends.
