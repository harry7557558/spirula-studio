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
| mandeltorus 40M, 7680², `a` = 89 px | 36.0 s | 15.2 | **13.7** | 18.1 |
| peak VRAM, same | 56.8 GiB | 28.4 | 19.8 | 17.0 |
| dye-alley 4M, 3840², `a` = 14 px | 13.9 s | **11.0** | 12.9 | 17.9 |

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
