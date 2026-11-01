# S-CIELAB perceptual loss

`--perceptual-loss s_cielab` swaps what `ssim_lambda` weights: instead of
`ssim_lambda * (1 - SSIM)` the photometric loss carries
`ssim_lambda * mean(dE) / 100`, where dE is the S-CIELAB colour difference
(Zhang & Wandell, "A spatial extension of CIELAB for digital color image
reproduction", 1997) between the render and the photo. The L1/L2 terms keep
their `1 - ssim_lambda` share as before. Status: a prototype for comparing
against SSIM; the default stays `ssim`.

## What is computed

Per pixel of both images (`FusedSSIM.cu`'s `fused_scielab_kernel`, and the
`scielab_*` entries of `backend/vulkan/shaders/fused_ssim.slang`):

1. sRGB decode to linear. The loss always compares display-encoded values --
   the render is converted to the image's display space before the loss, and
   so is the reference at upload (`EngineState.h`, `ColorSpaceState`) -- so
   the decode is right for linear and wide-gamut runs too.
2. Linear sRGB -> XYZ (D65) -> the Poirson-Wandell opponent channels S-CIELAB
   uses (luminance, red-green, blue-yellow), folded into one 3x3.
3. One separable Gaussian per opponent channel.
4. Back to XYZ, divided by the D65 white, then CIELAB; dE is the plain
   dE*ab (1976) distance between the two images' Lab.

`/ 100` (`kScielabLossPerDeltaE`) makes L* span the same [0, 1] as the image,
so `ssim_lambda` means roughly what it means for SSIM and a plain L1 of the
same weight is on the same scale for grey-level errors.

## The blur radii

All kernels are 11 taps (radius 5), the same footprint as SSIM:

| channel | sigma (px) |
|---|---|
| luminance | 0.5 |
| red-green | 1.5 (SSIM's window) |
| blue-yellow | 2.0 |

S-CIELAB's own filters are sums of Gaussians given in degrees of visual
angle. At ~40 px/degree their narrow lobes are sigma ~0.4 / 0.7 / 1.1 px, but
the chroma channels also carry a wide lobe (~40% of the weight) at sigma
10-14 px, and luminance a small negative one at ~100 px. Neither wide lobe
fits 11x11. The radii above keep luminance close to per-pixel and blur chroma
1.5-2 px, roughly in the ratio of the eye's acuities (luminance resolves
several times finer than red-green, and blue-yellow is the coarsest). The coarser
levels of the loss pyramid (`loss_scale_min_pixels`) widen every kernel by
2x per level, which is what stands in for the wide lobes on large images.
The taps are `cScielabGauss` / `kScGauss`; changing a sigma means changing
both, since they are copied rather than shared.

Taps off the image or dropped by the mask / clip test leave the window, and
the blurred value is divided by the kernel weight left (per channel), so
neither the image border nor a mask edge darkens the comparison. SSIM keeps
zero padding at the border; S-CIELAB had no result to stay bit-identical to.

## Densification map

`densify_loss_map_mode` defaults to `auto`: `ssim_cs` when training SSIM,
`s_cielab` (dE per pixel) when training S-CIELAB, so the map comes from the
kernel the loss runs anyway. Any map can still be named explicitly; a map
from the other family (`s_cielab` while training SSIM, or an `ssim_*` map
while training S-CIELAB) runs that kernel for its map alone, without a
gradient. `loss_full` adds the trained term.

A map made of the perceptual term alone is weighted by `ssim_lambda`, or by 1
when `ssim_lambda` is 0 -- otherwise `--ssim-lambda 0` would leave an
all-black map and a zero error score for every splat. Everything downstream
of the map (power, accumulation, score power, the draw) is scale-free, so the
weight matters only in that case. The kernel runs whenever the map needs it,
whatever `ssim_lambda` is.

## Display

The CLI and the loss dict report `delta_e` (mean dE over all pixels, masked
ones counting 0) in place of `ssim` when S-CIELAB is trained; `rgb_loss`
includes `ssim_lambda * delta_e / 100`.

## First numbers

mip-NeRF 360 bonsai, `images_2`, 30k steps, eval every 8th view, CUDA on an
RTX 5070 (2026-10-18), everything else default:

| run | psnr | cc_psnr | ssim | train s |
|---|---|---|---|---|
| `ssim` (default) | 31.17 | 32.96 | 0.949 | 362 |
| `--perceptual-loss s_cielab` | 31.19 | 32.86 | 0.944 | 354 |
| `--ssim-lambda 0` (L1 only, `ssim_cs` map) | 31.21 | 32.98 | 0.945 | 355 |

One scene, one run each, and run-to-run spread was not measured.
S-CIELAB costs no more than SSIM per step. Nothing here measures colour
fidelity directly (the eval has no dE metric), which is what S-CIELAB is
for; judge it on scenes with colour noise or chroma subsampling.
