# Progressive training resolution

Training can start on smaller images and step up to full resolution as the
run goes on. Early steps then settle the coarse shape and colours more cheaply,
and later steps spend full-resolution time on detail. It is off by default;
existing presets and saved settings train exactly as before.

## Turning it on

In the desktop app, tick **Progressive resolution** under the training options,
next to the training resolution. On the command line:

```text
spirula train --data DATASET --progressive-resolution true
```

"Full resolution" means the size the dataset is loaded at. Progressive
resolution applies on top of **Training resolution divisor**, so a run at 1/2
with progressive resolution starting at 1/4 begins at 1/8 of the files' size.

## The schedule

**Automatic.** `progressive_resolution_start` (default 4) is the first divisor;
it halves stage by stage down to full size. `progressive_resolution_full_at`
(default 0.3) is the fraction of the run by which full size is reached. Each
earlier stage is half as long as the one after it, because a step at twice the
resolution costs about four times as much. With the defaults on a 30,000-step
run:

| Steps | Image size |
|---|---|
| 0 to 3,000 | 1/4 of each side |
| 3,000 to 9,000 | 1/2 |
| 9,000 to the end | full |

A start of 8 adds a 1/8 stage first: on 30,000 steps with `full_at` 0.3, the
switches fall at about 1,286, 3,857 and 9,000.

**Manual.** `progressive_resolution_schedule` replaces the automatic stages
with your own `step:divisor` pairs, for example `0:4, 2000:2, 6000:1`. Divisors
may only shrink, and any whole divisor from 1 to 64 is allowed. A schedule whose
first step is not 0 starts at its first divisor. Leave it empty to use the
automatic stages.

## Equal passes per image

Resolution changes only between epochs, full passes in which every training
image is used once. A switch requested at a given step moves to the nearest
epoch boundary, so every image is trained the same number of times at each
resolution. The run log lists the planned stages when training starts and
each switch when it happens, at the planned step. An epoch that would run past
a switch ends there: this happens when batch packing makes a pass slightly
uneven, and after resuming from a checkpoint saved mid-epoch, so a resumed run
switches on the same steps as one that was never interrupted.

With disk streaming (`cache_images disk`), steps decode in parallel and are
used as they finish. Near a boundary, the steps still in the prefetch pipeline
(a few dozen at most) can therefore interleave across the switch. Each image
still gets exactly one pass per epoch at that epoch's size.

## Splat budget

Splats grow by about 5% every 100 steps until they reach the maximum (**Max
splats**, `cap_max`). With the default settings that happens around step 5,000,
well before full resolution at step 9,000. Every splat would then be placed
from blurred images, where the error that guides placement shows only the
coarse shape. After the cap, only nearly transparent splats are ever moved, so
the solid ones placed early stay where they landed.

**Grow splats with resolution** (`progressive_splat_budget`, on by default
with progressive resolution) holds each stage below a limit and raises it at
each switch.

**Automatic** (the default) spreads the limits from the starting point count,
the seed cloud the run begins with, to **Max splats**, raising them by the same
factor at each stage and reaching the maximum at full resolution. Growth is
multiplicative, so equal factors are equal shares of the growing. With 1.1
million starting points and a 4 million maximum:

| Stage | Steps (defaults) | Growth may reach |
|---|---|---|
| 1/4 | 0 to 3,000 | about 1.7 million |
| 1/2 | 3,000 to 9,000 | about 2.6 million |
| full | 9,000 on | 4 million |

The trainer uses the exact count after seeding, and logs it with the limits
when training starts. The GUI shows the same preview below the setting: the
point count of the seed cloud when one is set, else the dataset's sparse points
once it is open. A seed at or above the maximum holds nothing back.

**Manual, per stage** gives one box per resolution stage. Each takes a splat
count (`500000`), a fraction of the maximum (`0.25`) or a percentage (`25%`);
a blank box uses the automatic value. Limits may only grow from stage to stage,
and values above the maximum are clamped to it. Switching to manual fills the
boxes with the automatic values. On the command line,
`progressive_splat_budget_schedule` takes the same values separated by commas,
one per stage, for example `25%, 0.5, 4000000`; `step:splats` pairs such as
`0:25%, 3000:50%` set limits at explicit steps instead.

Growth waits at a stage's limit and resumes when the next stage begins. Stage
switches move to whole passes over the dataset, and the limits move with them;
two stages that land on the same pass keep the later limit.

Growth stops at `refine_stop_iter` (14,000 by default) or at the run length
minus `refine_stop_num_iter`, whichever is later. A ceiling that rises after
that point is never reached, and the run log says so when training starts.
The time and GPU memory forecasts replay the same ceilings.

Whether the budget improves a given scene is best judged by two runs that
differ only in this setting, compared on their held-out views.

**GPU memory.** Without the budget, growth reaches the maximum early, so a
maximum the GPU cannot hold fails within the first few thousand steps. With it,
the largest growth comes after the switch to full resolution, which is also
when each step's image buffers are largest. A maximum that does not fit then
fails late in the run, soon after the switch. The GPU memory forecast cannot
warn about this in advance (see Known limits). If you are unsure a maximum
fits, train it briefly without progressive resolution first, or lower it.

## What changes, and what does not

- Each training batch's colour images are area-filtered down, and the camera
  intrinsics are scaled to match exactly. For wide lenses that are split into
  pinhole faces, the faces render smaller and the source camera, including
  fitted lens models, is scaled with its image.
- Masks, depth and normal maps are left as they are; the loss already samples
  them at their own resolution.
- Evaluation, the image comparison view and exported results always use the
  full loaded size.
- With `cache_images cpu` (images held in RAM), the smaller copies are
  prepared while images load, which costs about a third more RAM at a start
  divisor of 4. Each copy is released once its stage has passed.
- With `cache_images disk` (the default), every step decodes its full-size
  image and then shrinks it. Low-resolution steps are cheap on the GPU, so
  during those stages the CPU decode becomes the bottleneck: expect the CPU
  near 100% while the GPU waits.
- Densification measures splat size as a fraction of the frame, so its limits
  mean the same at every resolution. How many splats it may add follows the
  splat budget above.
- Resuming a stopped run continues the schedule from the checkpoint's step.

**Known limits.** Step-time estimates early in a run reflect the cheaper
low-resolution steps, so the remaining time reads short until full resolution
is reached. The GPU memory projection, likewise, is measured on the smaller
images: it does not foresee the larger image buffers of later stages, so it
can show no risk for a run that will run out of memory at full resolution. It
firms up only after the last switch.

## Settings reference

| Setting | Default | Meaning |
|---|---|---|
| `progressive_resolution` | `false` | Turn the schedule on. |
| `progressive_resolution_start` | `4` | First divisor; a power of two of at least 2. |
| `progressive_resolution_full_at` | `0.3` | Fraction of the run by which full size is reached. |
| `progressive_resolution_schedule` | empty | Manual `step:divisor` stages; replaces the automatic ones. |
| `progressive_splat_budget` | `true` | Hold splat growth below a limit per stage. |
| `progressive_splat_budget_schedule` | empty | Empty is automatic; else one limit per stage (count, fraction or percent), or `step:splats` pairs. |

The schedule and the budget are `src/data/ResolutionSchedule.h`; the batches
are built by `DataManager` (`src/data/DataManager.cpp`), and the plan is logged
by `TrainerSession` (`src/app/TrainerCore.cpp`). The engine applies the
ceilings through `densify_cap_at` in `src/engine/EngineConfig.h`, which the
forecast in `src/app/TrainForecast.h` replays.
