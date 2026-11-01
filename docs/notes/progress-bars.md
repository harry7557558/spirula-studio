# Progress bars

Every bar in the GUI is a `ProgressClock` (`app/gui/ProgressClock.h`) drawn by
`app/gui/ProgressView.h`. The rules it holds every bar to:

- **The bar is time, not work.** It shows `elapsed / (elapsed + time left)`,
  so a step whose items get dearer as it goes does not race through its first
  half and crawl through its second.
- **It never goes back.** A total that grows, or an estimate that rises,
  holds the bar where it is until the clock catches up.
- **It errs toward jumping forward near the end.** Every estimate below is
  biased long, so a wrong one ends in a jump rather than a crawl.
- **It moves when the job reports, not in between.** No interpolation: the bar
  steps, and the time left counts down between reports.
- **It always says elapsed and time left as hh:mm:ss**, `--:--:--` while the
  time left is unknown. Logs may keep milliseconds; bars do not.

## How the time left is estimated

A job reports `work`, 0..1: its own measure of how far it is, which need not
be linear in time. From the reports the clock takes the larger of

- a power law `t = T w^p` fitted in log-log over the last three quarters of
  the work, `p` held to [0.5, 3] -- it follows a job whose cost per item
  grows (the flat mapper) or shrinks;
- the pace over the last 90 seconds, which catches a start-up cost the fit
  would charge to the remaining work, and a phase that slowed down.

A job that knows its own time left (training's forecast,
`docs/notes/train-forecast.md`; the batch queue's per-stage averages) hands it
over and the clock takes it as given. A stall counts the time left down to a
fifth of the last estimate and holds there rather than reading as done.

`work` that falls is read as a total that grew: earlier reports are rescaled,
so the pace stays right and the bar holds.

## The jobs

**Dataset steps** (`PrepProgress.h`). One clock per step. A step entered again
with a new label is a new phase counting something new -- reading features,
then choosing pairs, then matching -- and gets a fresh bar; `detail()` only
relabels.

**Matching.** The rig-mate pass verifies pairs only the first pass's results
name, so its count is not known until then. The first pass's total includes
an upper bound on it: the mates of every candidate pair, without the weak-link
test (`rigMatePairs(..., weak_only=false)`), which bounds the mates of any
subset. When the real count arrives the bar jumps forward.

**Mapping** (`MappingWork`, `SfmProgress.h`) is one bar across seeding,
placing, merging and the finishing solves, whichever order a seed retry
visits them in:

| | placing | merge levels | finishing solves |
|---|---|---|---|
| flat | 0.75, as `x^1.5` of images placed | -- | 0.25 |
| bottom-up | 0.40, linear | 0.25 | 0.35 |

The finishing solves took 19-28% of the flat mapper's step on three captures
of 39-226 images. Placing is `x^1.5` because an image costs what the model it
joins costs to solve (`docs/notes/sfm-design.md` D77). Bottom-up places nearly
every image in its concurrent atom phase, so its count saturates long before
the step ends; the merge levels report themselves (`mergeUpwards` emits
`Stage::Merge` with `ceil(log2 models) + 2` levels -- two over, for the levels a
refused pair earns, so a tree that finishes early jumps forward). On a
235-image capture the step split 48% seed and atoms, 18% three merge levels,
34% the finishing passes and the final solve, 5.6 s of which report nothing;
the shares above follow that one run. A deep tree on thousands of images
spends more in its levels, which the clock's pace estimate then follows.

**Meshing** (`mesh/MeshProgress.h`). The child estimates each stage's time
from the sizes known so far -- points, cut edges, raw faces, the mesh after
each merge, the atlas's texels -- and writes the share done into its report
(`app/RunReport.h`); each stage's real size revises the stages after it.
Estimated sizes are raised a fifth, so revisions move the bar forward. The
costs were measured on one run (993k splats, 226 cameras, RTX 3070 laptop,
125 s): the 8192^2 texture alone was 42% of it, where the old stage table put
everything after colouring in the last 9% of the bar. Replayed with the
estimates, the share done was 0.52 at 55% of the time and 0.875 at 88%.

**Training** takes the trainer's ETA forecast. **Batch** tasks take their
runner's clock, and the queue takes the per-stage averages of finished tasks.
Evaluation, mask propagation, video export, edit saves and model downloads
use the plain clock over their item or byte counts.
