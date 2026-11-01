# Memory forecasts

Training, dense, meshing and mapping each forecast the RAM and VRAM they will
peak at, in one shape: `ResourceForecast` (`app/ResourceForecast.h`) -- what
the job holds now, what everything else holds, the limit the job may reach,
the projected peak with its sigma, and the chance of running out. One view
draws all of them (`app/gui/ResourceForecastView.h`): a risk tag and a bar per
resource (this job solid, its growth to the projected peak faint, other
programs grey), and on hover a chart of each over the job with the forecast's
95% band and the ceiling in red.

## The risk

`assess_oom`: the normal tail of `peak + others` past the limit, with the
spread of others added unless the limit is commit (which already counts
them). Low below 10%, medium below 50%, high above -- the trainer's rule,
now everyone's.

The limit for RAM is what the job holds plus what is free, or plus the commit
still free when that is lower. For VRAM it is 99% of the device less others,
and on Windows also the job plus the free commit: WDDM charges device memory
to commit (`docs/notes/train-forecast.md`).

There is no "not settled yet" note anywhere. An estimate that cannot be
trusted yet has a sigma that says so, because a wide band is seen and a line
of small print is not.

## Where each peak comes from

**A child reports itself.** `RunReporter` (`app/RunReport.h`) samples the
process's RAM twice a second and writes `memory.json` in the child's
`--progress-dir`, with whatever peak the job planned. The GUI keeps the
history (`app/gui/RunMemoryView.h`) and takes the larger of the plan and the
high-water seen (a ceiling is handled as below).

- **Dense** reports the ceiling its own budgets allow (`dense/MemoryPlan.h`),
  marked as a bound: budgets need not fill. On 235 images (fast preset) the
  ceiling was 2.26 GB of RAM against a 1.14 GB peak and 4.79 GB of VRAM against
  3.07 GB. So the peak is taken to lie anywhere from the high-water seen to
  the ceiling -- the mean halfway, sigma half the gap plus a tenth of the
  ceiling for the allocator slack the budgets do not size -- which held both
  peaks within one sigma.
- **Meshing** plans from the model's size before each stage
  (`mesh/MeshProgress.h`): 270 bytes a point for the triangulation, 130 a
  raw face for the first merge, 12 a texel for the bake; on the device 45
  bytes a point for the occupancy renders, 100 a face for the culling BVH,
  12 a texel for the bake. Measured as working set on one run (993k splats:
  2.1 GB triangulating, 1.15 GB baking), so sigma is 30% of each predicted
  rise. On a second run of the same model the first report predicted
  2.33 +- 0.64 GB of RAM against 2.11 GB reached, and 0.97 +- 0.23 GB of
  VRAM against 1.01 GB.

**Mapping has no plan,** so its peak is extrapolated over its own progress
(`GrowthForecast`): the high-water against the mapping bar's work, a line
fitted over the last three quarters of it, projected to the end. Sigma adds
the fit's own, 0.5 of the projected growth per e-fold of work still to go
(the trainer's is 0.3; a model whose solves outgrow its image count earns
more), the swing of samples below the high-water, and 5% of it. Before a tenth
of the work there is no slope: the rest may cost as much again as what is held,
and sigma is that. Run in this process (the default), RAM is this process
and VRAM is what the module's own devices hold -- `sfm::DeviceMemoryTally`
counts every device-local buffer its contexts allocate, since nothing outside
the module sees those devices. Run as a child, the child reports the same.

**Training** keeps its own model (`docs/notes/train-forecast.md`) and fills
the same shape, adding the pool breakdown on hover.
