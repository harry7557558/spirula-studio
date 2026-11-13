# Calibration warp

A per-image smooth warp of the ground truth onto the render, fitted during
training, so a camera's calibration residual (focus breathing, electronic
stabilization, rolling shutter, a small pose error) stops blurring the scene.
The rasterizer, its backward, the loss and the loss map are untouched: the warp
only changes which ground truth the loss reads.

| flag | default | |
|---|---|---|
| `--calib-correction` | `basic` | `off`, `basic` (the warp), `accurate` (the warp with parallax) |
| `--use-calib-warp` | 1 | what `basic` and `accurate` turn on |
| `--calib-warp-parallax` | 0 | add a term divided by the rendered depth; `accurate` turns it on |
| `--calib-warp-grid` | 9 | control points along each side, 2 to 16 |
| `--calib-warp-start-iter` | 1000 | |
| `--calib-warp-step` | 0.5 | fraction of each Gauss-Newton step taken |
| `--calib-warp-smooth` | 0.05 | neighbour prior, relative to the image's curvature |
| `--eval-align` | `auto` | also score held-out views aligned; `auto` follows `--use-calib-warp` |

`calib_correction` is a macro (`train_resolve_macros`), the dropdown under the
bilateral grid and PPISP in the GUI's basic options; the `synthetic` and
`academic-baseline` presets set it to `off`. The GUI's compare view shows the
ground truth through the warp, as the loss reads it, while its "Calibration
warp" box is ticked and "Original file" is not.

## Model

Each post-split camera has an `n x n` grid of control points spanning the image
corner to corner. A control point holds a displacement in milliradians, so the
table means the same at any training resolution: pixels are `mrad * f / 1000`
with the camera's own `fx`, `fy`. With parallax it also holds a term divided by
the rendered expected depth, `sum(w d) / (1 - T)`, the shape a camera-centre
error takes. The warp is bilinear in the control points; the ground truth is
resampled Catmull-Rom, clamped to its four nearest taps. Bilinear at a
half-pixel offset halves the response at half Nyquist and the loss fits that
blur: on capture A it cost 0.006 LPIPS-A of the 0.015 the warp gains. A pixel
whose sample falls outside the image takes the render's own value, so it adds
no loss and no gradient. The copy the loss reads is float16.

The mask, depth and normal ground truth go through the same table at their own
resolution. Depth and normals are bilinear over the valid taps, renormalized,
the way the loss resamples them (`core/Interpolation.cuh`), so a hole does not
bleed; with no valid tap the pixel is invalid. The mask takes the nearest tap,
clamped to the image. Alpha supervision reads a masked pixel as "should be
empty", so a 0 is a label, not a gap: written off the edge, it carves a slab
through an object that crosses the frame border, a visible slice in other
views. For the same reason the mask is cut by the unwarped one only when the
forward skipped the tiles that one excludes, which it does only without alpha
supervision; the cut keeps the warped mask out of those tiles. On a masked
object capture (`centered-object`, two seeds), alcc LPIPS-A 0.368 with both 0s,
0.364 clamped, 0.362 clamped and uncut. With a depth or normal bilateral grid,
its gradient comes back at the warped positions, a sub-pixel shift on a grid
that smooth.

## Fitting

After the loss, each image of the step takes one damped Gauss-Newton step on
its own table rows. The residual is the loss's own gradient on the render
(standing in for its gradient on the warped ground truth, up to the SSIM
asymmetry), scaled by `3 C H W eps` to the units of an IRLS (Charbonnier,
`eps` 0.03) residual, whose weights the Hessian uses. Image gradients are
central differences on the warped copy itself, which is resampled at
`p + flow` already; the flow's Jacobian is ~1 over a pixel.

The Hessian is lumped onto the control points (`H_k = sum_p w_kp J^T J`).
The bilinear weights sum to one at every pixel, so the lumped matrix majorizes
the true one and the per-point solve cannot overshoot. The smoothness term
(neighbour Laplacian, diagonal `2 x degree`), the magnitude prior (0.01) and
the damping (0.1) are relative to the image's mean curvature, so they mean the
same at any resolution and contrast. A step is capped at 1.5 pixels, what the
linearization holds, shortened by one factor across all of a point's parts:
displacement and parallax nearly cancel where depth varies little, and
clipping them apart does not. Masked pixels are skipped.

The table is a checkpointed pool slot (`eng.calib_warp.params`), so `--resume`
continues it. A resume with another grid, parallax setting or image count
drops it and fits it again (`checkpoint/Adapt.cpp`).

## Evaluating it

A held-out camera has its own residual, which no per-image warp can know, so
the renders are scored after the same kind of correction `cc_` applies to
colour, for every run alike, warp or not:

- `reference/python/eval_align.py` (the reference protocol) fits a smooth
  flow (17-point bicubic control grid, coarse to fine, Charbonnier, after
  colour correction, masked pixels left out) that warps each render onto its
  ground truth, then scores `al_` (aligned) and `alcc_` (aligned, then colour
  corrected) next to `raw` and `cc_`, masked pixels taking the GT value. It
  recovers a planted 4.7 px warp to 0.08 px, and the ranking of runs held at
  eval grids of 8, 16 and 24.
- `--eval-align` does the same inside the trainer's eval and writes `al_*` /
  `alcc_*` into `metrics.json`, plus `masked_al_*` / `masked_alcc_*` when the
  views have masks, the way `masked_cc_*` sit beside `cc_*`; the fit and its
  colour correction leave masked pixels out. It runs the training kernels with
  the roles swapped (the render is warped onto the ground truth), 16 x 16
  points, 1/8 to full resolution, Catmull-Rom for the final resample. On one
  capture A run it read alcc PSNR 28.65 / LPIPS-A 0.1857 against the
  reference's 28.53 / 0.1839; its anti-ringing resample is a little softer
  than torch's bicubic.

Compare methods on `alcc_lpips_alex`.

## Results

`spirula train 3dgs`, 15k steps, every 8th image held out, Vulkan, RTX 5070;
reference protocol, aligned then colour corrected; `basic`. Capture A is a
phone video of a tabletop object (178 frames), B a close-up phone video in two
clips (281), C a mixed rig: a 360 camera pair, a phone video and stills (2484).
Baseline seed-to-seed spread: A 0.002 LPIPS-A, B 0.0002.

| capture (training size) | baseline LPIPS-A | warp | LPIPS-VGG | PSNR | SSIM |
|---|---|---|---|---|---|
| A (960 x 540) | 0.1429 | 0.1276 (-11%) | 0.1888 -> 0.1677 | 28.91 -> 29.25 | 0.9211 -> 0.9299 |
| A (1920 x 1080) | 0.2466 | 0.2245 (-9%) | 0.2544 -> 0.2370 | 28.22 -> 28.60 | 0.9152 -> 0.9234 |
| B (960 x 540) | 0.2005 | 0.1954 (-3%) | 0.2452 -> 0.2331 | 26.50 -> 26.65 | 0.8639 -> 0.8699 |
| C (divisor 4) | 0.2871 | 0.2807 (-2%) | 0.3022 -> 0.2959 | 21.81 -> 22.00 | 0.7187 -> 0.7282 |

The full-resolution row ran an earlier build whose gradient pass resampled the
source rather than reading the warped copy (the same setting read 0.1285 and
0.1257 with the two at 960 x 540). The warp gives up some unaligned `cc_psnr`
on A (about 0.25 dB): the sharper render pays more for each held-out camera's
own misalignment.

Choices, on A at 960 x 540 (LPIPS-A, baseline 0.1429):

- The loss gradient (0.1345 over two seeds, bilinear) beat registering the
  warped ground truth onto the render (0.1374); both together added nothing
  (0.1348). On C registration made LPIPS-A worse (+0.008): with seven visits
  an image and a weak render, the render is a poor target. Registration coarse
  to fine matched on LPIPS-A there but drifted the scene off the nominal
  cameras (unaligned PSNR -0.7 dB). All of it is gone from training;
  `--eval-align` still registers, since there the ground truth is the target.
- Catmull-Rom resampling of the ground truth: 0.1345 -> 0.1285.
- Grid (registration runs): 5 x 5 0.1402, 9 x 9 0.1374, 13 x 13 0.1372.
  Three Gauss-Newton iterations a visit instead of one: no change.
- The parallax term (`accurate`): 0.1285 -> 0.1256, for the depth render it
  forces (+10% step time).
- The magnitude prior: 0.1 instead of 0.01 was slightly worse on B (0.1977
  against 0.1963).
- Holding each sensor's mean warp at zero (every 100 steps): under 0.001 on A
  and B, and 0.002 worse on C, whose three sensors' biases the scene cannot
  take at once. Removed.
- Adam on the table: not run. An
  image is visited every few thousand steps on C, and Adam's moments decay
  between visits; the Gauss-Newton step converges in a few visits regardless.

The learned fields are mostly smooth, with a large non-affine part: per-frame
zoom of up to 15 mrad at the edge on B (focus breathing on a close subject),
and rotations and local zooms on A. About a fifth of A's warp is common to all
its images (mean field 0.28 mrad against 1.28 per image). A frame whose
background is optically defocused drifts (34 mrad on one B frame); the warp
cannot model defocus, and nothing but the priors stops it.

## Cost

At 1920 x 1080, 15k steps, idle RTX 5070: 306 s against 294 s (+4.3%) and
+13 MiB of VRAM (the float16 copy); at 960 x 540 about +3 MiB, on C +2% and
+7 MiB. `calib_warp_parity bench 1920 1080`, Vulkan / CUDA, ms a launch:

| launch | Vulkan | CUDA |
|---|---|---|
| forward, Catmull-Rom, float16 out | 0.162 | 0.146 |
| accumulate, loss gradient on the warped copy | 0.193 | 0.193 |
| accumulate, resampling the source (eval alignment) | 0.335 | 0.293 |
| update | 0.003 | 0.002 |

So the warp costs 0.36 ms a step at 1080p. A 3000-step control with the table
held at zero (the float16 copy and every launch, no warping) cost what the
launches cost; the rest of the 4.3% is the sharper scene itself, which
densifies into more splat-tile pairs.

## Not done

Warping the render onto the ground truth instead needs the warp's
transpose for the image gradient and for the loss map that densification
reads, and it would train the splats against a resampled, softened render; the
measured cost of bilinear resampling above says that matters.
