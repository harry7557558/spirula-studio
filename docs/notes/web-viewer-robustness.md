# Holding up in web viewers

A model is looked at in more renderers than the one it was trained with, and
the web viewers draw splats differently. Two flags address that:
`--viewer-kernel-fraction` (share of training steps drawn with PlayCanvas's
kernel) and `--noise-gate-peak-weight` (which splats get exploration noise).
Both came out of a parameter sweep that scored colour-corrected LPIPS-A and
DISTS in five renderers: gsplat, PlayCanvas, Spark, and PlayCanvas on the SOG
and SPZ compressions of the PLY (3dgs-trainer-comparison, eval methods 3, 6-9),
along with a much stronger, per-coefficient `--sh-reg` (below).

## Defaults

`--viewer-kernel-fraction 0.5`, `--noise-gate-peak-weight` on, `--sh-reg 0.04`.
Over the eight scenes of the sweep (the six below plus london_undistorted at
8M splats and garden_full at 3M, full resolution) that scores +3.8 against the
earlier defaults, +3.0 without the synthetic lego, positive on every scene
(london +0.3, the least), gsplat alone -0.3, and trains 4% faster. For a model
that is mostly looked at in Spirula's own viewer, `--viewer-kernel-fraction
0.3 --sh-reg 0.02` keeps gsplat ahead instead (+0.6; +3.2 overall, london
-0.2). The floater-suppression presets now set `--sh-reg` 0.067 / 0.1, above
the default; those two values were not benchmarked.

On the two large scenes the gains are smaller than on the six: garden_full
+1.7 (SOG +5.5% LPIPS-A, PlayCanvas +1.1%), london +0.3, where the web
renderers already beat gsplat and the kernel share costs gsplat 1-1.6%.

## What the viewers do differently

Measured on one model (sedan, Ref-NeRF real, 646x420, 1M splats) by relaxing
one PlayCanvas default at a time in a diagnostic copy of its page:

| PlayCanvas default | what it does | cc LPIPS-A, 10 views (gsplat: 0.164) |
|---|---|---|
| all defaults | | 0.204 |
| kernel | cut at 2 sqrt(2) sigma, tail renormalized: `(g - e^-4) / (1 - e^-4)` | 0.192 as a plain gaussian to 3.33 sigma |
| `minPixelSize` 2 | skips a splat whose footprint is under 2 px (sigma < 0.45 px) | 0.202 at 0 |
| compact data format | 8-bit log scales, 8-bit alpha, 11/11/10-bit colour | 0.204 with `large` |
| `colorUpdateAngle` 10 | re-evaluates SH only after a 10 degree turn | 0.204 at 0 |

No sub-pixel shift between the renderers (phase correlation, +-0.05 px).
Spark cuts at 2 sqrt(2) sigma without renormalizing, stores centres as
float16 and log scales in 8 bits. Spirula draws to 3.33 sigma (down to alpha
1/255), gsplat to 3 sigma.

The kernel is the one difference that a trainer can do something about.
Brush's models lose nothing in PlayCanvas on this scene (0.190 / 0.190) though
its rasterizer has the same gaussian kernel as gsplat and Spirula: its splats
are larger and fainter (median opacity 0.10 against Spirula's 0.21). Most of
Spirula's remaining PlayCanvas gap is spread over all fine texture and is not
explained by any single default above.

## `--viewer-kernel-fraction`

A hash of the step picks that share of training steps; those render (forward
and backward) with PlayCanvas's kernel, the rest with the full gaussian. The
choice is `LossConfig::raster_kernel_cut` (4, i.e. sigma^2/2 < 4, or 0), held
in the engine's forward state for the step and reset after it, so evaluation,
preview and export renders are always the full gaussian. The kernel itself is
`Primitive3DGS::kernel_vis` on CUDA and `kernel_vis` in `raster_common.slang` on
Vulkan, which both Vulkan passes call so that they agree on alpha to the bit;
`raster_bwd_parity` covers the cut.

Tiling follows the cut: `ProjEllipseView::max_sigma` caps the intersector's
ellipse at `min(ln(255 o), 4)`, so a cut step bins no tile past the kernel's
support. Only splats with opacity above e^4 / 255 = 0.21 change (the others
already fall under alpha 1/255 inside the cut); their tile footprint shrinks to
`4 / ln(255 o)` of its area, 0.72 at full opacity. The uncut steps still size
the intersection buffers, so peak VRAM drops only at a share of 1.

The rasterizers' own per-micro-tile culls -- the forward's hit test and the
backward's survivor compaction, which matter once a binning tile is larger than
the 8 px micro tile -- use the cut kernel's exact reach, `kernel_reach`: the
sigma at which `o` times the kernel falls to 1/255,
`-ln(e^-4 + (1 - e^-4) / (255 o))`, 3.81 at full opacity against the uncut
5.54. Both passes compute it identically, so the backward still undoes exactly
the splats the forward blended.

`raster_bench <splats> <iters> <macro_log2> <cut>` times it on fixed state
(2M synthetic splats, 1237x822, RTX 5090, ms per call, median of three):

| | intersections | raster fwd | raster bwd | binning |
|---|---|---|---|---|
| before this change, Vulkan | 5.99 M | 0.099 | 0.543 | 1.389 |
| cut 0, Vulkan | 5.99 M | 0.104 | 0.546 | 1.390 |
| cut 4, Vulkan | 5.34 M | 0.102 | 0.482 | 1.210 |
| cut 0, CUDA | 5.99 M | 0.084 | 0.441 | 0.711 |
| cut 4, CUDA | 5.34 M | 0.077 | 0.428 | 0.608 |

An uncut step pays almost nothing for the branch; a cut step bins 11% fewer
intersections and is 12% faster across the three kernels on Vulkan. A whole
training step does not notice: rasterization is a minority of it, and trained
models have fewer splats above 0.21 than this synthetic set (79%).

With the culls following the cut (1M synthetic splats, RTX 3070 Laptop, Vulkan,
the training variant with the peak-weight output, ms per call, interleaved
runs, mean of three):

| binning tile | | raster fwd | raster bwd |
|---|---|---|---|
| 16 px | cut 0, before / after | 0.86 / 0.89 | 2.89 / 2.70 |
| 16 px | cut 4, before / after | 0.89 / 0.79 | 2.97 / 2.52 |
| 64 px | cut 0, before / after | 1.14 / 1.12 | 3.07 / 3.03 |
| 64 px | cut 4, before / after | 1.22 / 1.08 | 3.24 / 2.75 |

The cut stays a push constant, not a specialization constant. Folded into the
pipeline it took a further 4% off the forward on cut steps only, while the
backward's cut-0 pipeline ran 7% slower -- a codegen difference on this GPU;
the shader from before the cut existed ran the same -- and at cut 4 the
backward matched the push-constant one. Folding also lets the driver round
`exp(-4)` its own way, which the forward and backward must not disagree on.

Six scenes (lego 50k, sedan, train, drjohnson, kitchen, struktur28_auto; score
= -100 ln(value / defaults), mean of the five renderers x two metrics; noise
about +-0.5..1 per scene). Time is training time over the defaults', geometric
mean, runs one at a time; a second defaults run at the end of each scene's
queue drifted 0.4% on average, 1.8% at most:

| share | score | gsplat only | time |
|---|---|---|---|
| 0.3 | +1.1 (6/6 scenes) | -0.8 | 1.007 |
| 0.5 | +1.6 (6/6) | -1.8 | |
| 0.7 | +2.4 (6/6) | -2.3 | 1.011 |
| 0.3, with the gate below | +1.7 (5/6) | -0.6 | 0.974 |
| 0.5, with the gate | +2.2 (6/6) | -1.1 | 0.977 |
| 0.3, the gate, `--sh-reg 0.00067` | +2.1 (6/6) | -0.2 | 0.974 |

A larger share buys the web viewers more at gsplat's expense; Spirula's own
viewer draws the full gaussian. Peak VRAM did not move (within 2%).

## `--sh-reg`

`--sh-reg` weighs the squared SH rest coefficients, each RGB coefficient alike
(its channel mean, averaged over splats). It used to be divided by the
coefficient count as well, so one value meant 5x more per coefficient at
degree 1 than at 3, and FPBO divided by the active degree while the other
optimizer paths divided by the buffer's. The values here are per coefficient;
the old default 0.001 at degree 3 is today's 0.001 / 15 = 6.7e-5.

Ten times that default (6.7e-4) was tried for the compressed formats, which
lose almost all of their quality through spherical harmonics (on sedan, SPZ's
4-bit bands 2-3 account for 1.044 of its 1.049 LPIPS-A ratio through
PlayCanvas, SOG's SH palette for 1.104 of 1.108). It gained a little in every
renderer instead (LPIPS-A / DISTS: gsplat +0.3 / +0.6, PlayCanvas +0.3 / +0.5,
Spark +0.8 / +0.5, SOG +0.4 / +0.8, SPZ +0.0 / +0.5), so it acts as less
view-dependent overfitting rather than as robustness to quantization.
Stronger keeps paying, up to about 0.02. On 0.3 with the gate:

| `--sh-reg` | score | without lego | worst scene | gsplat only |
|---|---|---|---|---|
| 0.002 | +2.3 | +2.0 | +0.9 | -0.2 |
| 0.0067 | +3.0 | +2.3 | +1.9 | +0.4 |
| 0.02 | +3.9 | +3.0 | +2.1 | +0.9 |
| 0.067 | +4.0 | +3.3 | +1.2 | +0.6 |

0.02 is better than the defaults on every scene and every renderer, gsplat
included; 0.067 gains on sedan and loses on kitchen and lego. With the 0.7
share and the gate, 0.02 scores +4.8 (gsplat -1.1); per renderer over the
defaults (LPIPS-A / DISTS): gsplat -1.7 / -0.6, PlayCanvas +5.2 / +6.5, Spark
+4.9 / +5.4, SOG +8.1 / +10.1, SPZ +4.4 / +5.4.

## `--noise-gate-peak-weight`

The revised exploration noise moves a splat by `(1 - opacity)^150` of a step,
so only faint splats explore. With the gate, the opacity it sees is
`min(opacity, peak alpha * T)` (the running peak weight that dead-splat
detection already tracks, while it is tracked), so an opaque splat that stays
hidden behind others explores too. Alone +0.4 (gsplat +0.4); with the kernel
share above it lifts 0.3 from +1.1 to +1.7. It also trains 2.6% faster (6.7% on
kitchen), presumably because hidden splats that move stop adding to the
overdraw (not measured).

## Tried, not kept

Same scenes and score, each on the defaults unless noted:

- A minimum on-screen size (the `max_screen_size` hinge mirrored, pushing a
  splat's largest sigma to 0.5 or 0.7 px): +0.1 at 0.5; at 0.7 sedan -3.5,
  PlayCanvas -12 there although Spark and SPZ gained.
- Spark's kernel (cut, not renormalized) in 20% of steps on top of 0.3: +1.1
  against +0.8.
- Late opacity binarization (`o (1 - o)` ramped in over the second half):
  -1.6..+2.9 per scene, gsplat -2.5 to -3.5.
- Exploration noise to more splats (`(1 - opacity)^75` or `^40`): +0.3 / -0.4,
  struktur28_auto -2.1 at 75, gsplat -5.4 at 40; a step clamp of 2 sigma
  instead of 1: no gain.
- Densification scores from the mean's Adam moments (signal-to-noise ratio,
  its inverse, step length in own sigmas): -0.5 / -0.1 / -0.6.
- Long-axis-split shape (offset 0.65 sigma; long 0.6 / minor 0.9): 0.0 /
  -0.6. Scale reg on the largest axis only: -0.3. Isotropic scale-agnostic mean
  update: -0.9. Refine every 200 with matched growth: -0.3.
- Losses with the 0.3 kernel share: `--perceptual-loss flip` -10.1,
  `--perceptual-loss ssim` +0.7 (struktur28_auto -2.0), `--num-loss-scales 1`
  +1.3.
- On 0.3 with the gate (+1.7): `--dead-after-epochs 1` +1.9, `--min-opacity
  0.01` +1.6, `--num-loss-scales 1` +1.6, refine every 400 with growth 1.2155
  and split fraction 0.4 +1.4 (lego -0.1), `--opacity-reg 0.02` stopped after
  two scenes (lego at 50k splats -7.4, sedan +4.1).
- Rounding SH to SPZ's grid on 30% of steps, straight-through (a gsplat
  fine-tune of a finished sedan model): SPZ got back a third of its loss, SOG
  next to nothing, gsplat -0.5..0.9%, and rounding in a frame other than the
  exported one kept only 40% of the gain. Not built.
