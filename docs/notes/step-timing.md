# Where a training step's time goes

The status strip's **details** link (beside the ms/step readout) shows a
step's time split two ways: by what each piece is *for* (rendering, loss,
the optimizer, appearance correction, depth and normals, ...) and by what
it *grows with* (splats, splats x pixels, pixels, training images, nothing).
Hover for a card; click to pin it and switch between **per step** (the last
`refine_every` steps, averaged) and **whole run** (measured so far, the rest
forecast), and between two **lists** and a **grid** of one axis by the other.
Under either sit the largest savings the numbers can quantify. The VRAM
card has the same for memory. `SS_STEP_TIMING_LOG=1` prints the per-step
breakdown every 100 steps; `SS_STEP_TIMING=0` turns the measurement off.

Vulkan only. The CUDA build shows no link.

## Measuring

`backend::dispatch_timing_arm()` makes every dispatch, fill and copy write a
pair of `BOTTOM_OF_PIPE` timestamps into a query pool of its own (the
`SS_PROFILE` pool and the events keep theirs), tagged with the recording
thread's scope; `dispatch_timing_collect()` drains the device and reads them
back. Dispatches are already serialized by the barrier after each one, so
the intervals do not overlap and their sum is GPU-busy time. A step that
records more pairs than the pool holds leaves the rest untimed -- they land
in *unaccounted* -- and doubles the pool for the next armed step (2048
queries to start, 128k at most).

The trainer arms one step in ten -- the steps the ETA's stage timer already
drains on -- and every step `step % refine_every == 0`, the densify steps.
It arms at the top of the step, before the yield to a pending viewer render,
so a render the step waited for is in it. The host spans are timed on the
host: the wait for decoded images (`engine_take_data_wait_seconds`) and the
checkpoint save.

What it costs: garden at 1/4 resolution, 3000 steps of ~6 ms, three
alternating pairs: 18.19 s with it, 18.36 s without -- within run-to-run
noise. Resetting each pair's two queries in the command buffer, instead of
the whole pool once at the span's first write, cost 2.5% on the same runs.
The readback asks for availability rather than waiting, so a pair that was
never written is dropped instead of stalling the step.

## Classifying

Every Vulkan entry has one row in `engine/StepTimingTable.h`: its purpose
(`StepPart`) and what its time grows with (`StepScale`). `spirv_tool embed`
fails the build on an entry without a row and on a row that names no entry,
so a new kernel cannot be missed; an entry the table does not know at run
time is counted as *unclassified kernels*, not dropped.

Some kernels serve whoever calls them -- sort and scan, Adam, the quantile
select, fills and copies. Their rows say `Scope`, and the caller opens a
`StepTimingScope` naming the purpose and the scale: the forward pass opens
three (projection per splat, tiling and rasterization per splat x pixel),
and the loss, backward, optimizer, densify, appearance, calibration, PCA
and background stages one each. On the trainer thread with no scope open
they count as *other*, fixed. A dispatch from any thread but the one that
armed -- the GUI viewport, the web viewer -- is *viewer*, whatever its row.

The compromises, where one kernel does two jobs:

- `fpbo` is the optimizer, though it also carries the projection backward;
  without FPBO that work shows under backpropagation.
- `ppl_fwd`/`ppl_bwd` are the loss, though they compute the depth and
  normal terms too; *depth and normals* is the separate work (the depth
  bilateral grids, depth-to-normal, downsampling the reference maps).
- The rasterizer writes the depth the geometry terms read; that is rendering.
- The appearance chain fuses PPISP, the bilateral grid and the colour-space
  encode; all of it is appearance correction.
- The loss-map kernels (Canny, Tukey, NMS) feed the densify score, so they
  are densification, every step.
- The bilateral grids' optimizer and TV run over every image's grid, so
  they grow with *training images*; a step's sampling of them with pixels.

## Unaccounted, and the host spans

*Unaccounted* is the wall time nothing else explains: host work between
kernels (recording, submission, the readbacks a step waits on) and an idle
GPU. It is wall time minus the GPU cells minus the two host spans. *Image
loading* is the host's wait for decoded images plus the uploads under its
scope; *checkpoints* is the save's wall time (its own copies are dropped
from the GPU cells, they are inside it). When the cells and spans add up to
more than the wall clock -- the image wait overlapped the GPU finishing the
step before -- the image wait gives way first.

In the lists it is a grey row in both columns; in the grid a grey strip
across every column, its height its share like any row.

## Averaging

Per step is the last `refine_every` steps (20 to 500, the ETA's window).
A card shows every amount in one unit and precision, picked from its total --
milliseconds per step; seconds, minutes or hours over the run -- each column
right-aligned.
Timed steps are two strata, densify steps and the rest, each averaged over
its own samples and weighted by its share of the window's steps. Without
that, a densify step -- one in 100, and always timed -- would count ten
times over. A stratum with no sample in the window keeps its latest one.

## The whole run

What has run is summed as it happens: each step's wall time and host spans,
and its GPU cells -- measured, or its stratum's average for an untimed step.
What is still to come is the per-step breakdown, each column scaled over the
remaining schedule (the splat count from `SplatSchedule`, the SH degree, the
progressive-resolution stages):

| column | per step, relative to now |
|---|---|
| splats | `N/N0`, weighted by the ETA model's SH term `b0 + b1 k` |
| splats x pixels | `(N/N0)^0.5 (r/r0)^0.45` |
| pixels | `r/r0` |
| training images, fixed, unaccounted | 1 |

and then scaled together so the remainder is the ETA's
(docs/notes/train-forecast.md) minus its checkpoint saves, which are added
as they are forecast there. So the run's total is always elapsed plus the
ETA; the breakdown only says how it divides.

## What the time grows with, measured

Paired runs, the default preset, 5000 steps to a 1M cap, RTX 5070: the same
scene at two resolutions, compared step for step (the splat count follows the
schedule, so it is the same at every step).

| | garden 1/4 vs 1/8 | bonsai 1/2 vs 1/4 |
|---|---|---|
| pixels column, `r^q` | q = 0.91-0.92 | q = 0.99-1.00 |
| splats x pixels column, `r^q` | 0.52 at 169k splats, 0.38 at 1M | 0.53 at 251k, 0.39 at 1M |
| splats column | unchanged | unchanged |
| splat x img bytes, `r^q` | 0.30-0.50 | 0.09-0.45 |

Within a run, from ~200k to 1M splats: the splats column is linear (garden
1/4: 0.59 -> 3.42 ms for 5.9x the splats); the splats x pixels column grows
as `N^0.36-0.58` -- splats shrink as they multiply, so each covers fewer
tiles -- and splat x img bytes as `N^0.3-0.5`. Hence
`kSplatPixelTimeExp = 0.45`, `kSplatPixelSplatExp = 0.5` and
`kSplatXImgVramExp = 0.35` (`app/TrainForecast.h`). The splats x pixels
column falls toward `r^0.4` as a run goes on; the constant is the run's mean.

At the 1M cap, per step:

| | garden 1/4 | bonsai 1/2 |
|---|---|---|
| total | 10.2 ms | 14.1 ms |
| splats / splats x pixels / pixels | 3.4 / 3.5 / 2.5 | 3.4 / 5.9 / 3.9 |
| optimizer / backprop / rendering / loss | 2.9 / 2.3 / 1.6 / 1.5 | 2.9 / 3.9 / 2.4 / 2.4 |
| appearance / calibration | 0.87 / 0.20 | 1.35 / 0.29 |
| unaccounted | 0.50 | 0.50 |

## Settings that change the splat kernels

Some settings do not add a row of their own but change what the rasterizer,
the projection or the optimizer costs. `app/SettingCosts.h` holds each
value's factors on those cells, relative to the defaults; a switch scales the
cells by the ratio of its two values' factors, the settings taken as
independent. Paired runs, default preset, 5000 steps to a 1M cap, RTX 5070,
the last 300 steps' cells against two runs at the defaults (which agree to
within ~5%):

| setting | raster fwd | raster bwd | projection | optimizer | own row |
|---|---|---|---|---|---|
| floater suppression mild | 0.98 / 1.00 | 1.43 / 1.45 | | | |
| moderate | 0.95 / 0.97 | 1.37 / 1.42 | | | |
| strong | 0.90 / 0.90 | 1.29 / 1.30 | | | +2.2 ms PCA, both |
| calibration correction off | 1.01 / 1.01 | 1.00 / 0.99 | | | 0 |
| accurate | 0.99 / 0.99 | 0.99 / 0.99 | | | 1.44 / 1.47 |
| primitive mip | 0.99 / 1.10 | 0.99 / 1.08 | | | |
| 3dgut | 1.60 / 1.62 | 1.88 / 1.86 | | | |
| SH degree 2 | | | 0.83 / 0.84 | 0.74 / 0.75 | |
| 1 | | | 0.64 / 0.65 | 0.56 / 0.56 | |
| 0 | | | 0.55 / 0.55 | 0.28 / 0.28 | |

garden at 1/4 / bonsai at 1/2; blank is within noise of 1. The distortion
terms floater suppression adds weigh on the raster backward; its forward
gets cheaper as `max_screen_size` caps how large a splat may grow, and strong
adds the local-PCA regularizer. Calibration correction does not move the
rasterizer here because the default normal term renders the depth it reads.
mip's 3D filter is within noise. The SH degree is projection and optimizer
work, and mostly the latter.

## Savings

Read off the whole-run breakdown, each naming the setting by its label and
where it is -- "Halve “Max splats” in Basic Options: about 29% less time" --
and shown when it saves at least 5%, the three largest:

- half the image resolution: pixels x 3/4, splats x pixels x `1 - 0.25^0.45`;
- half the splat cap: splats x 1/2, splats x pixels x `1 - 0.5^0.5`;
- a Basic Options switch's row: the colour corrections (bilateral grid and
  PPISP together), the dataset's depth and normal maps;
- a Basic Options dropdown one step down, through the factors above:
  calibration correction, floater suppression, the SH degree, and 3dgs for
  another primitive;
- the viewport's *live* refresh, the checkpoint interval, a faster disk for
  the image wait, half the training images.

A run that split its frames into views is listed whatever its rank, since
with appearance correction on it is often the largest thing to try: every
view is an image with its own colour grids, so the 360-camera preset makes
`views / frames` (about 5) times the per-image work and memory. The tip says
to switch the preset to general purpose (or, without the preset, to turn off
the split itself) and takes the split's own row plus the training-images
column x `1 - frames / views`. On a mixed fisheye and pinhole indoor capture
(428 frames, 1208 views, 3000 steps) appearance correction was the largest
row, 2.0 of 9.2 ms a step, and the training-images column 1.6 ms of it.

The VRAM card's: half the resolution takes the Image category x 3/4 and the
projected splat x img peak x `1 - 0.25^0.35`; half the cap takes the Splat
category (sized for `cap_max`) x 1/2 and the peak x `1 - 0.5^p`, `p` the
VRAM forecast's fitted exponent; one SH degree less takes the SH rows and
their optimizer state x `1 - K(d-1) / K(d)`, `K(d) = (d+1)^2 - 1`, read off
the pool's SH buffers (3 -> 2 -> 1 -> 0 freed 84 / 53 / 38 MiB of 176 at 1M
splats, within 7% of that); the preset switch the Appearance category x
`1 - frames / views`.
