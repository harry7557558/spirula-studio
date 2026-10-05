# Render counters: unrendered decay, split budget, view sampling

Two reports of training collapsing into a fog of low-opacity splats (an indoor
floor at 20M splats, a sparse forest path at 40M) and a local reproduction on
the blokbrug capture share one budget: densification takes opacity out of
splats faster than rendering puts it back. Every long-axis split leaves both
children at `k` times the opacity, relocation charges a live source the same
way for every dead splat it revives, and the per-splat regularizers used to
run on every splat every step, rendered or not. Adam normalizes the tiny
regularizer gradient away, so a fresh child that no camera rendered lost about
one learning rate of opacity and log scale per step and reached the
`min_opacity` cull in ~170 unrendered steps. Measured on blokbrug (1360
images, cap 3M): the dead fraction per refine step climbed from 4.7% at step
600 to 30% at step 4900, and PSNR sat flat near 19 instead of rising.

## What the engine keeps

`SplatOptim::visit_counters` (`core/SplatVisitState.h`), one `uint32` per
splat: renders since creation or last split in the low 16 bits, steps since
the last render in the high 16, both saturating. Both optimizer paths update
it (`FusedProjectionBwdOptim_kernel.cuh` for batch size 1, the fused geometry
kernel otherwise). A splat counts as rendered when its world opacity gradient
is nonzero, which is what excludes splats behind walls and under masks; a
frustum test would not. Relocation and growth reset the counters of source and
child. The Vulkan copies of the bit layout are in `visit.slang`.

## The rules

| flag | default | effect |
|---|---|---|
| `reg_rendered_only` | on | per-splat regularizers (opacity, scale, erank, quat norm, DC/SH pull, screen-size hinge) act only on rendered splats |
| `dead_after_epochs` | 2 | a splat unrendered for this many passes over the train set is relocated as dead |
| `split_weight_by_renders` | on | the split draw weight is the refinement score times the renders since the last split; the refinement-score view shows the product |
| `max_split_fraction` | 0.1 | revivals plus growth per refine step capped at this share of the live count, revivals first |
| `min_renders_per_refine` | 0 | images per step raised until a splat at `render_quantile` of views is rendered this often between refine steps, up to `max_train_batch_size` |
| `view_sampling` | uniform | `deficit` draws images by how under-rendered the splats they last contributed to are (`view_deficit_power`, `view_deficit_max_ratio`) |

`reg_rendered_only` leaves the regularizer formulas alone; it zeroes their
gradient for a splat with no photometric gradient this step. The Adam moments
of such a splat still decay, which is ordinary Adam on a zero gradient. The
DC/SH regularizer inside the separate colour kernels of the non-fused path is
not gated; only the fused paths gate it.

The dead rule counts steps, so `dead_after_epochs` is converted with the
run's steps per pass (`RunState::steps_per_epoch`) and clamped to the 16-bit
counter.

`max_split_fraction` does not change `SplatSchedule`'s forecast: growth only
loses its share when revivals exceed the budget, which is the collapse case.

## View sampling

`visit_camera_stats_tensor` folds, after every backward, the render counters
of the splats each camera contributed to into per post-split camera sums
(`EngineState::visit`), pooled per input image by the DataManager. Every 100
steps the trainer pushes the sums to the DataManager, and each epoch draws a
multiset of the group's images with expected count proportional to
`clamp((median+1)/(mean+1), 1/R, R)^p`, stochastically rounded. Images with
no stats yet draw at weight 1. Occluded splats never enter a camera's mean,
because contribution is read off the screen opacity gradient.

`min_renders_per_refine` estimates views per seed point from the camera
frusta (`estimate_views_per_point`, no occlusion), so a building full of rooms
over-counts and the rule stays close to the plain steps-per-pass rule there.
It only ever raises the batch size.

## Log line

On refine steps the CLI prints `num_dead`, `num_relocated` and `num_added`
next to PSNR. `num_dead` rising refine step over refine step is the early
sign; a healthy run holds it near the growth share.

## Measured (2026-09-30, RTX 5070)

Blokbrug, cap 3M, dead splats per refine step as a share of the live count,
and train PSNR near step 10k:

| run | step 1500 | 3000 | 5000 | 7000 | 9600 | PSNR ~10k |
|---|---|---|---|---|---|---|
| baseline | 8.2% | 18.8% | 29.6% | 26% | 24% | 19.4 |
| `reg_rendered_only` alone | 2.4% | 9.1% | 18.3% | 6.6% | 3.3% | 22.5 |
| all three (0.1 budget, 2 passes, render-weighted draw) | 1.6% | 5.3% | 6.8% | 3.6% | 2.5% | 21.6 |

The gate alone halves the death rate at every step and also recovers after the
cap, only later; the split budget and the render-weighted draw cut the
transient (18% to 7% peak) and the recovery takes half as long.

Garden (Mip-NeRF 360, 1/4 of the 4x-downsampled images, cap 1M, 30k steps,
every 8th image held out): PSNR 29.72 baseline vs 29.64 with the three on,
SSIM 0.9416 vs 0.9413, which is run-to-run noise; the dead share peaks at 2.3%
instead of 7.4%. All five parity tests that touch the changed kernels pass on
Vulkan against CUDA references.

What the counters say about the two datasets at step 600 (`SS_VISIT_LOG=1`):
garden renders the median splat on 36% of steps and 5% of splats never; the
360-camera blokbrug renders the median splat on 53% of steps and the 10th
percentile on 18%, also with 5% never rendered. So on blokbrug a splat is
rarely unrendered for long, and the gate helps because the regularizer only
competes with a photometric gradient on steps that have one; the long
unrendered streaks the frustum estimate would predict do not occur there.

Deficit view sampling on Stategallery (647 images, rooms behind walls): draw
weights span 0.59 to 2.83 in the first pass and settle to 0.73 to 2.29 by the
third; nothing runs away, because occluded splats never enter a camera's mean.

Throughput, GPU otherwise idle, old behaviour vs the new defaults: garden at
1297x840 for 2000 steps 10.7 s vs 10.8 s, blokbrug for 1500 steps 214.2 s vs
214.9 s, both within 0.5%. At 324x210 (2.8 ms a step) the same 3000 steps go
from 8.4 s to 9.0 s: the extra launches (one counter pass in the optimizer,
the render-weighted score copy each step) are fixed per step and only show
when a step is that short.
