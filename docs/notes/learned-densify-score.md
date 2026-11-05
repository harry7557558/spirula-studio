# Learned densification score

`--densify-learned-score 1` (off by default) replaces how a splat's
densification score is made from the per-pixel error map. The default
aggregates; this fits.

## What the default does, and where it goes wrong

The raster backward reduces the conditioned error map to one number per
splat, with `densify_accum_mode avg`:

    a_i = sum_p w_ip E_p / sum_p w_ip,        w_ip = alpha_i T_i at pixel p

where `E = L^4` (`densify_loss_map_power`), then raises it to
`densify_score_power` (0.4) and averages that over the steps since the last
refine. A pixel's error is shared out by blending weight alone, whether or not
the splat could do anything about it. A large background splat seen through
the semi-transparent rim of a foreground object (sky behind foliage) collects
a share of every rim pixel, and because the power 4 comes before the footprint
average, those few rim pixels dominate it: a splat with 2% of its weight on a
rim keeps 0.02^0.4 = 21% of a rim splat's score, more than a splat in uniformly
mediocre texture at a quarter of the rim's error ((1/4)^1.6 = 11%). The
per-step machinery computes what it says; the bias is the formula.

Two non-default ways out. Moving the power after the footprint average
(`--densify-loss-map-power 1 --densify-score-power 1.6`, the same `L^1.6` for
a uniform footprint) removes the max-like weighting of rare rim pixels. The
learned score below also stops the background taking the rim's blame.

## The fit

Give each splat a score `s_i` and render it like a colour, normalized by
coverage as depth is: `R_p = sum_i w_ip s_i / sum_i w_ip`. Choose the scores so
that `R` reproduces the conditioned map before its power, `L` -- minimizing a
Poisson deviance `sum_p W_p (R_p - L_p ln R_p)` (`W_p` the pixel's coverage),
which is scale-equivariant, so no learning rate is tied to the error's
magnitude. Its EM update is Richardson-Lucy deconvolution:

    s_i <- s_i * r_i,    r_i = (sum_p w_ip L_p / R_p) / (sum_p w_ip)

`r_i` is exactly `avg` accumulation applied to the map `L / R`, so the raster
backward is unchanged: the forward renders `R` as one extra channel
(`SplatScoreRender`, a template axis of the CUDA forward and a specialization
constant on Vulkan), the map conditioning divides by it in the pass that would
otherwise apply the power, and `avg` accumulates the ratio. From a flat start
the first update is the `avg` score of `L`: the default aggregation is the
first iteration of this fit, and later ones explain error away. The ratio is
capped at 10^4 either way.

Fitting `L^4` instead was tried: per-view ratios of a fourth power are so
noisy that the fit never settled (median |ln L^4/R| about 2 on bicycle, 0.4
for `L`).

## Storage and the powers

No new per-splat buffer. `optim.accum_buffer`, which the default resets every
refine, holds `(s^P, n)` with `P = densify_loss_map_power *
densify_score_power` (1.6), so what the draw, the clip,
`densify_final_score_power` and the GUI's `refinement_score` view read is in
the units of the default's `mean_t(a_t^0.4)` -- for a uniform footprint both
are `L^1.6`. The forward reads `s = x^(1/P)`. The buffer is not reset at refine
steps, split and relocated splats inherit their parent's score, and a clipped
or powered score goes to the side buffer, never back into what the forward
renders. `densify_score_mode` is unused; `densify_accum_mode` is forced to
`avg`.

## The stochastic update

Each step sees one batch of views. The update is an EMA of each view's RL
target in the same power domain the default averages in,
`x <- x * ((1 - eta) + eta * r^P)` with `eta = max(0.02, 1/(n+1))` -- the
first updates average -- followed by a pull of `ln x` towards the prior,
`ln x <- 0.99 ln x + 0.01 P ln prior`. The prior is the geometric mean score
of the updated splats (an EMA read back one step late, no device sync); a
splat no update has reached renders and starts from it, and before any update
it is 1, which makes every splat's first update its `avg` score.

The pull is not optional. The fit is underdetermined -- a pixel holds many
splats -- and a splat with a small share of its pixels gets back a ratio that
does not depend on its own score, so without a restoring force its score
random-walks. Bicycle, 500k splats, score log `SS_DENSIFY_SCORE_LOG=1`:

| update (eta 0.1 unless given) | PSNR | LPIPS-A | draw weight on smallest / largest fifth by size |
|---|---|---|---|
| default (two runs) | 24.39-24.61 | 0.244-0.247 | 13% / 28% |
| EMA in ln s, fit L^4, map not coverage-normalized | 23.87 | 0.344 | 7% / 40% |
| EMA in s^0.4, fit L^4 | 24.19 | 0.315 | 13% / 30% |
| EMA in s^1.6, fit L | 23.92 | 0.316 | 86% / 1% |
| ... pull 0.01 | 24.36 | 0.278 | 94% / 1% |
| ... pull 0.05 | 24.64 | 0.245 | 17% / 24% |
| ... eta 0.02, pull 0.01 (shipped) | 24.54 | 0.250 | 18% / 23% |

A log-domain EMA is biased low by Jensen for every noisy splat, small ones
most. The null-space spread scales with sqrt(eta) at a fixed
pull / eta, hence the small rate. Score quantiles end within 30% of the
default's (p50 0.0069 vs 0.0105, p99 0.036 vs 0.044).

`SS_DENSIFY_SCORE_LOG=1` prints, at every refine step, the share of the draw
weight held by each fifth of the splats ranked by world size and the score
quantiles -- the measure of where densification goes, in either mode. It
copies the scores to the host and sorts them, ~50 ms a refine at 500k splats.

## Cost

Off: nothing -- the forward's score axis is compiled out and the engine passes
an empty `SplatScoreRender`. On: one float accumulated per splat-pixel in the
forward, one `[C,H,W]` float image, a division fused into the map conditioning,
and one per-splat kernel per step in place of `densify_update_weight`.
