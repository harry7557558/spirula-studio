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
mean fwd+bwd time over a 4-step window drops by more than 6%. It starts coarse
because the pool's high-water only grows — starting fine would pin the
intersection buffers at the finest size the run ever touches, and descending
reaches the optimum from above. A rejected direction is retried every 40
windows, so the choice keeps following the splat size as training changes it.
`SS_BIN_TILE_LOG=1` prints each decision.

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
