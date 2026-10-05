# Training ETA and VRAM forecast

`app/TrainForecast.{h,cpp}` predicts, while a run trains, how long it has left
and how much device memory it will peak at. `TrainerSession::train()` feeds it
one sample per step; the CLI's progress line, `/progress`, the GUI's status
strip and the VRAM bar's hover card read it. `train_forecast_test` checks both
against synthetic runs whose answers are known, and `SS_FORECAST_LOG=1` prints
its state every 100 steps for checking it against a real run.

Both models lean on the same fact: until the model reaches `cap_max`, the
splat count still to come is **known**. `engine_densify_step` grows it by
`densify_grows_at` / `densify_target` (`engine/EngineConfig.h`), and
`SplatSchedule` replays that same rule forward. The SH degree schedule
(`min(step / sh_degree_warmup_every, sh_degree)`) is known the same way. So
nothing extrapolates the count or the degree -- only what they cost.

## ETA

A step costs `a + (b0 + b1 * k) * N^gamma`: `N` in millions of splats, `k`
the SH coefficients the step trains (0, 3, 8, 15), `a` everything that does
not scale with the model (loss, bilateral grids, PPISP, data loading, host
overhead). Per window of `refine_every` steps -- so every window holds the
same share of densify steps -- a Kalman filter updates `[a, b0, b1, c]` from:

- the window's wall time per step, outliers past 3x the median dropped (the
  pipeline builds when the SH degree steps up are one-offs);
- the GPU time of the splat stages, `c + (b0 + b1 * k) * N^gamma`.
  `SplatStageTimer` (`engine/EngineStepTiming.cpp`) brackets the forward, the
  raster / projection backward, the optimizer and densify with timestamp
  events on one step in ten; on Vulkan every bracket is a queue submission.

Those stages are NOT all per-splat: rasterization has a large per-pixel part,
~45% of their time on bonsai at 1/4 resolution and nearly all of it at 1/2.
Treating their time as `b * N` overshot the ETA there by +130%. Hence the
intercept `c`: the GPU measurement pins the slope once the count moves (it
has less host noise than the wall clock), and says nothing about it before.
While the count holds still the slope is a prior -- half the splat stages'
time, +-70% -- and the ETA's sigma says so.

No single `gamma` fits: whole-run fits gave 1.0 on bonsai at 1/4 resolution,
0.85 at 1/2 and 0.8 on garden. So three filters run, at 0.75, 0.875 and 1.0,
each scored on how well it predicted each window before seeing it
(discounted by 0.9 a window), and the ETA is the weighted mixture. Updates
are gated at 2 sigma with a 10% floor: the laptop GPU these were measured on
ran whole 100-step windows 40-60% slow, four times in one run.

The ETA also adds the checkpoint saves still to come, at the seconds per
million splats the saves so far took; a save is excluded from its step's
wall time for that reason.

Replayed on three recorded runs, error in the time left:

| step | 300 | 1000 | 2000 | 3000 | 4000 | 5000 |
|---|---|---|---|---|---|---|
| bonsai 1/4, naive (last 100 steps) | -66% | -65% | -59% | -50% | -29% | +3% |
| bonsai 1/4, model | -25% | -14% | -10% | -12% | -4% | +4% |
| bonsai 1/2, naive | -62% | -63% | -61% | -54% | -43% | -36% |
| bonsai 1/2, model | +27% | +55% | +16% | -10% | -3% | -1% |
| garden 1/4, naive | -23% | -25% | -23% | -30% | -15% | -24% |
| garden 1/4, model | +34% | +22% | +23% | +30% | 0% | -3% |

The naive estimate is biased low everywhere the count still grows; the model
is not biased, and its early error is what the prior cannot know.

## VRAM

Only `splat x img` grows during training (docs/notes/vram-splat-x-img.md),
so ours at a future step is everything else now plus the growth, projected.
Two measurements decide what "the growth" is:

- It is read off the process **high-water**, not the pool. A step that
  overflows the alias arena holds private buffers beside it until the next
  phase switch, so the real peak sits above the steady cap: 179 against 156
  MiB on bonsai at 1/4, from one step near the end.
- The first densify step allocates `splat x img` scratch of its own -- 10 ->
  55 MiB on garden, count unchanged -- so the fit starts after it. Before
  that the forecast is provisional: growth all per splat, +-70%, plus 15% +-
  8% of the splat category for the live-splat scratch the step reserves
  (14.9% measured on bonsai).

The growth is `g0 + g1 * N`, a Kalman filter over `[g0, g1]` from one
high-water measurement per window, starting at `g0 = 0`: fitting a fixed
chunk from the first windows under-read bonsai's peak by 14 sigma, while
putting it all per splat over-reads garden's (its 55 MiB chunk) by up to
36% -- the safe side. Two terms widen the projection: the expected largest
per-step demand over the draws still to come at each count, minus what a
window already saw (the pool keeps the largest draw, and a view that sees
more of the model sets it); and 0.3 relative sigma per e-fold the count is
extrapolated.

"Others" is device use minus ours (on CUDA that includes the context), an
exponential average with its spread. The chance of running out is the normal
tail of `peak + others` above 99% of the device -- the last percent is
fragmentation. Low below 10%, medium below 50%, high above; the trainer logs a
warning the first time the risk reaches each of medium and high.

On the three runs, from the first densify step on, the projected peak stayed
within 2 sigma or above the truth, by at most 8% (bonsai 1/4), 10% (bonsai
1/2) and 36% (garden) of it.
