# Binning tile size

Splats are binned into **macro tiles** of `TILE_SIZE_X << macro_log2` pixels
(`src/core/Common.cuh`); each `TILE_SIZE_X x TILE_SIZE_Y` micro tile inside one
is a rasterizer workgroup that walks that macro tile's splat list. The default
`macro_log2 = 1` gives the historical 16 px binning tile.

`macro_log2` is a runtime value threaded through the intersect and raster
launches — it sizes no groupshared array and appears in no launch bound, so
retuning it costs one uniform shift per workgroup rather than a pipeline or
kernel instantiation.

## What coarsening trades

Sort, key write and raster forward all scale with the splat-tile pair count;
the backward scales with the redundant per-micro-tile scan, `(T/8)^2` times
that count. So coarsening buys the first at the expense of the second, and the
balance depends on both the scene (mean splat footprint `a`) and the machine
(how expensive pair-count work is against scan work). Fitting

    cost(T) = N * (1 + a/T)^2 * (A + B * (T/8)^2)

gives `T_opt = cbrt(a/k)` with `k = B/(64A)` — a clean closed form, but `k` is
a *machine* constant: measured 3.4e-4 and 4.3e-4 from two datasets on a GB10,
and a discrete RTX 5070 does not share it. Roughly half of a step is work the
binning does not touch, and isolating the other half would need a device sync
inside the step, so `A` and `B` cannot be recovered from a step time either.
**The search below measures the decision instead of the constants**: two
adjacent windows at different granularities differ only in the tile-dependent
part, so differencing them cancels the half we cannot measure.

## Measurements (NVIDIA GB10, Vulkan)

GPU-exec per run, binning tile 16 / 32 / 64 / 128 px:

| dataset | 16 | 32 | 64 | 128 |
|---|---|---|---|---|
| synthetic 40M, 7680², `a` = 89 px | 36.0 s | 15.2 | **13.7** | 18.1 |
| peak VRAM, same | 56.8 GiB | 28.4 | 19.8 | 17.0 |
| dual-fisheye 4M, 3840², `a` = 14 px | 13.9 s | **11.0** | 12.9 | 17.9 |

On a discrete RTX 5070 (DJI equirect 7680x3840, 1M splats, `a` = 65 px) the
same ordering holds but the spread is far smaller: 5635 / 5205 / 5295 ms at
16 / 32 / 64 px, VRAM 6139 -> 5444 MiB. Do not A/B this on a small scene —
bicycle_4 (1236x821) varies 28% run to run, which swamps the effect.

A 100M-splat batch at 7680² makes 4.5e9 pairs at 16 px binning and 0.58e9 at
64 px, so coarsening is what keeps it under the 2^31 `flatten_ids` limit.

## Behaviour

`--bin-tile-size` forces a size. `0` (the default) searches: it **starts at
64 px** and trials one granularity at a time, keeping a change only when the
mean fwd+bwd time over a 4-step window (its slowest step left out) drops by
more than 6%. It starts coarse
because the pool's high-water only grows — starting fine would pin the
intersection buffers at the finest size the run ever touches, and descending
reaches the optimum from above. A rejected direction is retried every 40
windows, so the choice keeps following the splat size as training changes it.
`SS_BIN_TILE_LOG=1` prints each decision.

### What a finer trial costs in memory

A finer bin makes more splat-tile pairs -- halving the tile turns `p` pairs a
splat into `(2 sqrt(p) - 1)^2`, 2.5-3x at the 4-9 pairs a splat of a trained
scene -- and the pool keeps what any step took. So a trial that is rejected
used to leave the intersection list and the sort's arena phase sized for a
bin the run never adopted, and the retry every 40 windows renewed it at each
new splat count: on bonsai at 1/2 resolution the peak splat-tile pairs were
5.1M-6.6M where a step at the adopted 16 px makes 1.6M-2.0M (6.3M at most),
about 100 MiB of a 590 MiB pool for an 8 px bin that lost every one of its
trials by 25-90%. Three rules now bound it (`EngineForward.cpp`):

- **A rejected finer trial hands its memory back.** The search marks the
  arena and the pair peak before the trial (`DevicePool::arena_mark`) and
  releases to the mark on rejection: the arena shrinks at the next phase
  switch, `isect.flat_a` at its next acquire -- to exactly what they were,
  not to what the next step asks for, so nothing regrows a view at a time.
  The trial's own steps still need the memory, once.
- **A finer size that lost twice running by twice the margin is not tried
  again where it would grow the pool** (`finer_trial_hopeless`), until the
  pairs per splat at the current size have fallen 30% -- splats that much
  smaller against the tile are a different question. Twice, because one slow
  view is enough for one such loss: 16 px read 30% over 32 px once in a run
  where the two tie. A retry that fits what the pool holds still runs.
- **A window's cost leaves out its slowest step.** One step in 64 reads 8 ms
  longer than the rest on bonsai: the Vulkan params ring wraps there and waits
  for the device (`params_alloc`), which moves the previous step's tail into
  this step's measurement without costing the run anything. A baseline
  window that catches it reads 15% slow. Once the skipped trials above made
  the retry cycle 80 windows -- 5 x 64 steps -- the baseline before every
  coarser trial caught it, and the search flipped between 16 and 32 px, each
  "better" than the other by 15%. Three runs since: 32 px kept, every 16 px
  trial within 4% of it, and the same 581 MiB each time.
- **The allocator's time is not the bin's.** The step time the search
  compares is taken less the host time the pool spent allocating
  (`DevicePool::alloc_seconds`): a trial that has to grow its buffers is
  otherwise slower by exactly the thing being handed back.
- **No finer trial whose extra pairs exceed a tenth of the free device
  memory** (`finer_bin_fits`): a few percent of speed is not worth the run.

The losses are kept per size, so a size the search comes back to is not
tried finer again from scratch.

What is left is the trial itself: a finer size that is adopted has the next
one tried at once, so a run can hold the pairs of two sizes down for a dozen
steps (garden at 1M splats: 0.71M pairs at 32 px, 1.45M at 16 px adopted,
2.9M-3.2M in the two 8 px trials that followed, +44 MiB on a 457 MiB process
for 11 steps each). The trainer's VRAM forecast asks the search for the pair
peak at the finest size it may still try (`engine_bin_finer_isects`, 0 when
the rules above or the floor rule it out) and carries a quarter of the
difference, +- half (docs/notes/train-forecast.md).

A coarser trial is the dangerous direction: every micro tile walks its whole
macro tile's list, so one step up quadruples that scan, and on a dense scene
at low resolution the pair count barely falls to pay for it. Two guards:

- No coarser probe while the last measured pairs per visible splat is under
  `(1 + 1/2)^2 = 2.25` -- a mean footprint under half a tile, where doubling
  the tile cuts pairs by under a third.
- A coarser trial whose first step (warm-up included) takes over twice its
  baseline is abandoned on that step instead of running its whole 6-step
  window.

What made these necessary: a 5M-splat run on 432 px views (a 360 capture
split into five faces, M2 Max) lost its device to macOS's GPU watchdog,
`kIOGPUCommandBufferCallbackErrorImpactingInteractivity`, on the first step
of a 128 px trial, after 3000 steps at 16-64 px; with `SS_VK_DEBUG_SYNC=1`
the last kernel to finish was `rasterize_fwd_2d` at the 128 px grid. The
Vulkan backend now also splits a rasterization past the submit budget
(docs/notes/gpu-submit-budget.md), so a trial that still slips through costs
time, not the device.

### Sizes no training step searches

The search is fed by the training step, so it owns only the image sizes a
training step renders at: `_engine_step_fwd_bwd_only` claims its size
(`engine_bin_tile_claim`) before the forward, which is what keeps the 64 px
start for training. Any other size -- the viewer's viewport, its
half-resolution frames while the camera moves, a preview -- is forward only
and is never measured, so it bins at **16 px** (`kMacroLog2Render`,
`EngineForward.cpp`) rather than at a start nothing would move it from. The
forward walks the macro tile's list once per micro tile just as the backward
does, so 64 px costs a forward-only frame the same 4x scan.

A 30M-splat SH3 model (a 10M-splat trained scene duplicated three
times), 1600x800 viewport, RTX 5070, Vulkan, ms per viewer frame:

| view | 8 px | 16 px | 32 px | 64 px |
|---|---:|---:|---:|---:|
| whole model in view | 79 | 84 | 121 | 456 |
| 4x closer | 64 | **59** | 63 | 109 |
| 16x closer | 49 | **42** | 44 | 55 |

16 px is within 6% of the best in every row. 8 px wins whole-model views by
6-8%, at 1600x800 and at the 800x400 of a moving camera alike, and loses 8-14%
closer in. Searching per view could gain that, but would spend probe frames
doing it.

The intersect additionally coarsens *within* a step, re-running the count pass
(~0.3% of a step, and flat in tile size), whenever the exact pair total would
overflow int32 — a forced size is a starting point, not a hard failure. That
size then becomes a floor. Both the search and the floor are keyed by image
size, so a viewport render at another resolution cannot retune what training
binned with.

Renders are **not** bit-identical across binning sizes: dumping at 16 px and
comparing at 64 px puts 0.11% of floats past `engine_render_parity`'s
tolerance (0.12% at 128 px), while the 8-bit blit output is bit-identical and
the same size compares at `max_abs 0`. Reads as tie-breaking and the
`T < 1e-4` early-out on low-coverage pixels, not a different splat set — but it
does mean a run pinned with `--bin-tile-size` and the same run on `0` are not
expected to match bit for bit.
