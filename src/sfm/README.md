# Structure from Motion (`src/sfm/`)

A standalone SfM pipeline — images in, a COLMAP `sparse/` model out — with a
GPU compute backend (Vulkan + Slang) and no heavy dependencies. It exists to
replace the `colmap` subprocess: `spirula sfm` is the CLI, and the native GUI
will drive the same library in-process instead of shelling out
(`docs/notes/sfm-port-plan.md` phase 5).

Imported 2026-07-27 from a separate development tree; that tree is now
read-only and this is upstream. What the port has not yet done is at the bottom
of this file and in the port plan.

## Rules

- **Vulkan only.** There is no CUDA path here and there will not be one. The
  module carries its own Vulkan context (`vk/VkContext.h`) and its own embedded
  SPIR-V, and shares nothing with the training engine — including, for now, the
  device. Built by default only for `SS_BACKEND=vulkan`.
- **No heavy dependencies.** Vulkan, Slang, C++17, and the repository's
  vendored `stb_image`. No Ceres, no Eigen on the hot path, no OpenCV, no
  SQLite, no PyTorch.
- **Compute on the GPU, control flow on the host.** Slang kernels do the
  per-pixel / per-feature / per-observation work; the host owns graph
  structure, RANSAC bookkeeping and the mapper's decisions. RANSAC in
  particular stays on the host deliberately: thousands of tiny minimal solves
  with data-dependent control flow, and not the bottleneck.
- **Port algorithms, not code.** COLMAP is the reference for behaviour and
  parameter defaults; the implementation is ours, against our own data
  structures. Numerics that had to match are called out in the comments by
  decision number (`docs/notes/sfm-design.md`).
- **COLMAP's on-disk formats are the interchange format**, and every stage is a
  subcommand reading and writing files. Any one stage can be swapped for
  COLMAP's equivalent to bisect a failure.

## Stage graph

```
images/ ──► extract ──► features/ ─┐
                                   ├─► match ──► matches.bin ──► map ──► sparse/0..N
            (pairing: exhaustive / │             (two-view          │
             sequential [+ loop    │              verification)     │
             closure] / GPU        │                                │
             pre-selection) ───────┘                                ▼
                                                    assemble (D63):
                                                    merge levels on shared
                                                    poses (Sim3) ► grow ► solve
                                                    ► audit ► split ► reseed
                                                                    │
                                                                    ▼
                                                            sparse/0 ► training
```

`spirula sfm auto` runs all of it from two knobs, `--quality` and `--data-type`.

`--progress-dir DIR` adds a second, optional output: `model.bin` (the poses and
a subsample of the points as they stand, coloured) and `pairs.bin` (per binned
image pair: the inliers, how many pairs were candidates and how many have been
verified, so a front end can tell "not reached yet" from "found nothing"),
rewritten at most every 1.5 s and renamed into place so a reader never sees
half of one. It is off unless asked for, and it exists so a
front end can show a run rather than tail it — the GUI passes it to its own
child and polls the two files (`src/sfm/core/Progress.h`,
`src/app/gui/SfmProgress.h`). Nothing in the pipeline reads them back.

The `map` box is one incremental reconstruction of the whole capture by default.
`--mapper bottom-up` replaces it with the opposite schedule (`map/Bottomup.h`,
D57): the verified view graph is cut by normalized cut into *atoms* of a few
dozen images (`map/Partition.h`), each is reconstructed by its own `Mapper` over
its own sub-database and all of them concurrently (`map/Atoms.h`, D59), and the
atoms are then merged upwards a level at a time — every model absorbing at most
one other per level, growing the ones that did not merge by PnP alone, with one
bundle adjustment across all of them, intrinsics shared per camera group,
between levels. It is a *schedule*, not a different algorithm: every model is
still built by `Mapper::run`'s own rules and joined by the same Sim(3) merger the
flat mapper's models are, which is what makes a regression in it impossible to
confuse with a regression in the geometry. What it changes is where the cost and the risk
sit — an atom is too small to get its own focal wrong, and too small for a
whole-model pass to be expensive. `Bottomup.h` carries the numbers.

**Neither mapper has a stage after it.** What each produces is a set of models,
and from there they need the identical thing: merge what belongs together, grow
what did not merge (merging cannot invent overlap that is not there), optimize
what changed, repeat — then, once, the passes no amount of merging can stand in
for: audit a new seam, break a model its own correspondences contradict,
register the tail, seed among what nothing reached, cut a fold. That is
`map/Assemble.h`, over passes that live in `map/ModelOps.h` so neither mapper
owns them (D63).

It replaced a separate manage loop (D44) that drove the same operations in
rounds. The loop re-ran the expensive ones over models nothing had touched, and
its audit repaired a model by growing it without a bound: on a 5356-image
capture it spent 65 minutes to merge three models and recover 119 images, and
ended with three near-copies of one reconstruction. `--no-manage` still names
the switch that turns those passes off.

Either schedule spends most of its time in bundle adjustment, and **most of
those solves are provisional** — a growth-phase refinement is followed by more
growth, a merge-tree level by another level. Those could run in fp32, which is
worth 25–35% of the mapping stage: on solves large enough to be
arithmetic-bound, halving the bytes every kernel moves is worth ~2×, and below
a few dozen images the scalar makes no difference at all because those solves
are bounded by dispatch count instead (`sfm_cholesky_test --bench` measures
both).

It is fp64 anyway (D61). The Schur and Jacobian kernels accumulate with
floating-point atomics, whose execution order is arbitrary, so no two solves
agree in their last bits. At fp64 that perturbation is ~1e-16 and never crosses
a decision threshold — the whole pipeline is reproducible run to run. At fp32
it is ~1e-7 and crosses them constantly, and LM's accept/reject plus the
mapper's filters turn a last-bit difference into a different reconstruction:
over three identical runs a 379-image capture scored 96.1 AUC@10 every time in
fp64, and 96.5 / 92.3 / 91.5 in fp32 — noisier *and* 2.7 points worse on
average. `--ba-real-coarse float` is there for anyone who wants the speed and
can live with that; the two scalars keep separate persistent contexts, because
a context caches the module it was compiled from.

Where the scalar does change is with the *tolerance*, not with the pass: a
growth refinement's first round stops at a loose threshold, every round after
it at the solver's full one. Asking fp32 for the latter means spending the
whole iteration budget on a threshold below its noise floor — on a 1194-image
capture that took the finishing passes from 48 s to 260 s.

### Which scalar a device can actually run

Not the one you asked for, necessarily, and the gap does not follow "bigger
GPU, more features". Each configuration needs something different, and
`VkContext::probeCaps` asks before `vkCreateDevice` can turn a missing feature
into an unattributable `VK_ERROR_FEATURE_NOT_PRESENT`:

| scalar | needs | seen on |
|---|---|---|
| `double` | fp64 arithmetic + fp64 and fp32 buffer atomic add | NVIDIA |
| `df` | `shaderInt64` + int64 buffer atomics | AMD (both ICDs), Intel Xe/RPL-S, llvmpipe |
| `float` | fp32 buffer atomic add | NVIDIA, AMD |
| `cpu` | nothing: it runs on the host (`sfm/ba/SolverCpu.h`) | anything |

`BundleSolver::init` steps down and says so once; ask the solver what it
settled on with `solver.real()` rather than reading `SolverOptions::real` back,
because packing `double` into buffers a `df` kernel reads is silent garbage,
not an error. The chain is **`double` -> `cpu`**: neither fp32-based
configuration is in it, because neither is a better default than a host solver
that is fp64 throughout — `float` stalls the normal equations above ~1e-7
relative accuracy (above), and `df` buys its ~48-bit accuracy with CAS-loop
atomics and emulated transcendentals. Both remain available on request, and
`df` is the one to ask for on a big GPU without fp64 atomics: it is accurate
enough for every real capture (0.01 px of reprojection against fp64), though
not for the two deliberately ill-conditioned convergence checks in
`sfm_map_test`, which report rather than assert when fp64 is missing. No AMD
part here has an fp64 buffer atomic add, so all of them take the host path
unless `--ba-real df` says otherwise.

A device that *had* the feature and then failed anyway — `VK_ERROR_DEVICE_LOST`
(what a Windows TDR reset looks like from here: the watchdog kills a driver
whose kernel runs past two seconds, and a thousand-image dense factorization
is one long kernel), or an allocation the driver refused — does not end the
run. `runGlobalBA` re-runs that solve on the host and sends every later solve
at least that big straight there, because the mapper's problems only grow;
`VkContext`'s `VK_CHECK` throws rather than exits so it can. The run says so
once, and `--ba-real cpu --ba-real-coarse cpu` (Spirula Studio: "Bundle
adjustment on the CPU", under Advanced) skips the failed GPU attempt next time
— worth setting on a card that resets once, since it resets again.

Two devices deserve naming:

- **Intel UHD 750 (Gen12, RPL-S desktop)** has *none* of the three: no fp64, no
  int64 atomics, no fp32 atomic add. It extracts and matches perfectly well and
  reaches the solver with nothing to run there, which is what the host solver
  exists for; a reconstruction on such a device costs roughly twice the
  bundle-adjustment time of an fp64 GPU and nothing else changes.
  It also lacks `VK_KHR_shader_integer_dot_product`, which is what the second
  build of the matcher (`match_nodot`, same integer result without DP4A) is
  for; `SS_SFM_NO_DOT4=1` forces that path on a device that has DP4A.
- **llvmpipe** runs `df`, but its fp32 `fma` is not single-rounded — `fma(a, b,
  -a*b)` returns exactly 0 where every real device returns the residual — so
  `df_two_prod` loses the low half of every product and `df` multiplication
  degrades to fp32 precision. `sfm_cholesky_test` fails there for that reason
  (7e-5 relative, against 1e-11 on hardware). The mapper's own checks still
  pass; treat llvmpipe as a way to run the pipeline, not to trust its last bits.

## Layout

```
SfmConfig.h  the one option surface: the aggregate of every stage's options
  .cpp         plus the descriptor table the CLI, --help and the GUI all read
vk/          VkContext.h   the Vulkan compute context every stage builds on
             EmbeddedSpirv.h  lookup for the SPIR-V compiled into the binary
core/        types shared by every stage, no Vulkan:
               Camera / CameraSetup   camera models, and which images share one
               Pose                   Rigid3, Sim3, angle-axis conversions
               Image / ImageLoader    decode, grayscale, the batch decode pool
               Exif                   focal prior + camera identity from headers
               Features / Matches     the on-disk feature and match formats
               Mask                   keypoint masking, sampled in uv
               Model                  Reconstruction + COLMAP binary IO
feature/     Sift (GPU), Matcher (GPU), Pairing, PairSelection (GPU),
               Verification (host worker pool), and the two seams the learned
               frontends plug into: Extractor.h and LearnedMatcher.h, neither
               of which includes an aliked/ or loma/ header
geometry/    Essential, Fundamental, Homography, P3P, AbsolutePose,
               Triangulation, TwoView, LinAlg
optim/       Ransac   LO-RANSAC with MSAC scoring
ba/          Problem (model registry + problem layout), Solver (LM, dense
               Cholesky / implicit-Schur PCG), SolverCpu (the same two on the
               host, for devices that run neither fp64 nor df), README.md
map/         Mapper, Bundle, CorrespondenceGraph, Merge, Profile,
               ModelOps (the passes over a *set* of models: merge validator,
                 audit, split, fold cut, prune -- shared, owned by neither)
               Assemble (the schedule both mappers run once they have models:
                 merge levels with growth and a joint solve, then the finish)
               Partition (view-graph normalized cut)
               Atoms (atoms reconstructed concurrently, one context per worker)
               Bottomup (the merge tree, and the schedule around it)
shaders/     common/ (Real, df, dmath, linalg, camera, loss), ba/, sift/, match/
tests/       one executable per file
```

`src/` is the include root, so every include is path-qualified:
`#include "sfm/core/Camera.h"`. Note that `sfm/core/Camera.h` and the engine's
`core/Camera.h` are different types with different jobs; the paths keep them
apart.

## Building

```bash
bash build_develop.bash -DSS_BACKEND=vulkan -DSS_BUILD_CLI=ON
```

`SS_BUILD_SFM` defaults ON for the Vulkan backend and OFF for CUDA (where
it can still be turned on if the Vulkan SDK is present). Shaders compile at
build time and are embedded, so nothing needs to sit next to the binary.

The bundle-adjustment kernels are compiled once per (Real, Loss) pair — nine
blobs at the default. Trim the matrix while iterating:

```bash
cmake -B build -DSS_SFM_REALS=df -DSS_SFM_LOSSES=trivial
```

Both are cached, so a trimmed value sticks until the full list is passed again
(`-DSS_SFM_REALS='float;double;df'`) or the build tree is wiped. Asking for
a variant that was trimmed out is a clear runtime error, not a crash.

## Running

```bash
spirula sfm auto IMAGES/ -o WORKSPACE/          # images -> sparse model
spirula sfm auto -o ws/                         # ./images + ./masks, all defaults
spirula sfm auto IMAGES/ -o ws/ --data-type video --quality medium
spirula sfm auto IMAGES/ -o ws/ --masks MASKS/  # drop keypoints on masked pixels
spirula sfm auto IMAGES/ -o ws/ --camera-model opencv-fisheye

spirula sfm extract IMAGES/ -o feats/
spirula sfm match   feats/ -o matches.bin
spirula sfm map     matches.bin feats/ -o sparse/ --images IMAGES/
spirula sfm map     matches.bin feats/ -o sparse/ --no-compact-unused-features
spirula sfm merge   sparse/ -o merged/
spirula sfm ba      problem.txt --real df       # solver benchmark on a BAL problem
spirula sfm ba      sparse/0 --real cpu        # ... the same solve, on the host
```

`spirula sfm --help` lists the commands, `spirula sfm <command> --help` (or
`spirula sfm help <command>`) prints that command's usage, its options with
their defaults and worked examples, and `spirula sfm --version` prints the
package version. A usage error names the flag, says what was wrong with it and
points at `--help`; it always exits 1, because `auto` spends exit codes 2 and 3
on *the reconstruction* being absent or partial.

Every line a default run prints is **localized**, in the language `--lang`,
`SS_LANG` or the OS says, and carries a translated stage tag padded to a common
width: `[extract] 12/512   frame_0012.png   Features: 4096`. The mechanism is
`core/Log.h`; the strings are `src/i18n/catalog/Sfm.h`. What stays English is
what is addressed to whoever is debugging the pipeline rather than to the
person waiting on it -- `--help`, `SS_SFM_MAP_PROF`, the seam-test and
focal-curve diagnostics, and the self-test binaries. The GUI reads its progress
bar out of those same tags rather than out of English text, so a run in
Japanese still moves the bar (`app/gui/SfmRunner.cpp`).

Environment: `SS_SFM_MAP_PROF=1` prints a mapper stage breakdown,
`SS_SFM_DUMP_SG` / `SS_SFM_CMP_STEP` are BA solver debug hooks
(`ba/README.md`).

`--compact-unused-features` is an in-memory representation change, on `auto`
and `map`, and is **on by default** (`--no-compact-unused-features` is the old
behaviour). It retains the feature rows referenced by stored match records
in stable order, remaps every stored match endpoint, and releases the temporary
index map before constructing `Mapper`. Image and pair order, pair
configuration, camera setup, match order, and referenced keypoint, color, and
descriptor rows are preserved. Feature and match files on disk are not
rewritten -- on `auto` the pass runs after `matches.bin` is written, which is
what keeps that file indexing the feature files. For a normally verified
`matches.bin`, the stored records are the verified correspondences; raw records
with configuration zero are preserved as well. A model written under the flag
indexes the compacted features, so `--resume` and `--audit` refuse a model whose
keypoint counts disagree with the current run -- which is the one thing to know
about the default change: a model written before it was on has to be resumed
with `--no-compact-unused-features`.

## Options

`SfmConfig.h` holds the pipeline's whole option surface: the stage option
structs unchanged (`SiftOptions`, `MatchOptions`, `PairSelectionOptions`,
`TwoViewOptions`, `CameraSetupOptions`, `MapperOptions`, `ManagerOptions`,
`MergeOptions`), the pipeline-level knobs that span stages, and
`SFM_CONFIG_FIELDS` — a descriptor table with one row per flag:

```c
F(member, name, cmds, tier, group, lo, hi, choices, help)
```

The table is the single source of truth for the CLI parser, for `--help`, and
(port plan phase 5) for the GUI's options editor; each is one macro expansion
over it in `SfmConfig.cpp`, so a new knob is one row and never three edits.
`cmds` is which subcommands accept the flag — a name may repeat across commands
with disjoint masks, which is how `--max-error` is the verification tolerance
for `auto`/`match`/`map` and the *alignment* tolerance for `merge`. `tier` is
`Basic` (what the GUI shows unfolded: quality, data type, camera model, camera
sharing, focal, masks, pair mode) or `Advanced`; `Alias` marks a second
spelling. A `bool` row is a switch and gets both `--name` and `--no-name`, and
`--help` prints whichever direction changes the default.

Three things stay hand-parsed in `src/app/cli/sfm_main.cpp`, because they do
not name one scalar field: `--camera-model PREFIX=MODEL`, `--focal PREFIX=F` and
`--distortion PREFIX=k1,k2,...` (which also feed the per-group override list),
`--no-manage` (four fields at once), and the flags that pick what a command
*does* rather than how — `-o/--output`, `map --audit`, `auto --no-masks`. The
CLI offers a token to the hand-parsed cases first, so those names always win.

Pipeline knobs are fanned out into the stage structs by `SfmConfig::finalize()`
and nowhere else, so the CLI and the GUI cannot disagree about what
`--max-error` (one tolerance, two struct fields — D47), `--device`, `--quiet`
or the camera settings mean.

The brute-force matcher takes 128-**or** 256-byte descriptors -- SIFT's and
ALIKED's, and DeDoDe-G's. The kernel is built at both widths
(`-DDESC_WORDS=64` halves the groupshared tile so it stays 8 KiB), and float
descriptors are L2-normalized on upload, which ALIKED's already were and
DeDoDe's are not. One matcher cannot mix widths, and says so.

`--features` picks the frontend: `sift`, `aliked-n16rot`, `aliked-n32`,
`loma-b128` or `loma-b`. The learned ones need the inference layer
(`SS_BUILD_SAM=ON`) and fetch a checkpoint on first use; `--matcher` then takes
`lightglue` for the ALIKED ones and `loma-b128` / `loma-b` / `loma-r` /
`loma-l` / `loma-g` for the LoMa ones. The families do not mix -- a learned
matcher only reads the descriptors it was trained on, and `auto` says so rather
than running.

`--quality low|medium|high|extreme` sets the working resolution, the feature cap
and the pair-selection breadth; `--data-type individual|video|internet` sets the
pairing mode, the seed angle and how cameras are grouped. Both are applied
*before* the table's overrides, so naming a flag explicitly always wins over the
preset, and `auto` reports every field a preset moved:

```
  preset    : --max-image-size 3200 -> 1000
  preset    : --max-features 8192 -> 2048
```

Sequential pairing — what `--data-type video` asks for — is a chain: image `i`
against the next `--overlap`, and nothing else. A capture that walks around a
subject and comes back has no pair crossing the seam, so any one weak step in
the sequence cuts the view graph and the mapper reports models where there
should be one. On a 262-frame walk around a plaza that was four models
(144 / 74 / 19 / 12 images) against one with 254 for the same frames matched by
pair selection. Two things fix it, and `auto` uses both:

- **At 100 images or more, `auto` retires the video preset for pair selection**,
  the same cutoff and for the same reason as exhaustive: a capture that long
  revisits places, and content-based selection is a fraction of the cost of
  matching. `--pairs sequential` explicitly still means sequential.
- **`--loop-closure` (on by default) covers the case below the cutoff**: it
  unions the `prefilter` shortlist into the temporal window, which is what
  COLMAP's `SequentialMatching.loop_detection` does with a vocabulary tree.
  Forced on that same capture it also produced one model (249 images), for 1.6 s
  of selection and 2.5x the pairs to match. `--no-loop-closure` is the old
  behaviour.

Everything else has a default that a beginner should not have to touch.

`--orient` (on by default) writes the finished model **upright, centred and
unit-scaled** instead of in the arbitrary gauge the seed pair left it in: the
mean camera up axis becomes +Z, the mean camera position becomes the origin,
and the furthest camera coordinate becomes 1. It is the identical similarity
the trainer computes from the poses it loads (`orientation_method="up"`,
`center_method="poses"`, `auto_scale_poses`), applied once at the point the
model is written, so the trainer's own normalization comes out as the identity
(`train_frame_scale=1`) and everything that carries no cameras — an exported
`splat.ply`, a mesh, a bare model in a viewer — is upright too rather than
tilted with no way left to recover the transform. `map/Orient.h` has the
algebra and the caveats; `--no-orient` keeps the mapper's raw gauge.

### The finishing passes

Reconstruction ends with up to two more global bundle adjustments, on models
nothing else will touch. That is what makes them safe: a parameter that turns
out to be badly conditioned can only spoil its own intrinsics, because there is
no growth pass left to build on the result.

The free intrinsics of a camera group are a **prefix** of `(focal, distortion,
cx, cy)` — the group's free count alone tells the solver how many columns it
owns, which is what buys COLMAP's three refine switches without a per-parameter
mask in the hot kernel (D50). Two consequences: releasing more is always
releasing a longer prefix, and holding the distortion holds the principal point
with it. Asked for both explicitly — `--refine-principal-point
--no-refine-extra-params` — the run stops with a usage error rather than
honouring half of it; in the finishing pass, where the principal point is on by
default, `--no-final-extra-params` simply takes `--final-principal-point` with
it.

- `--refine-principal-point` (off) and `--final-principal-point` (on) hold
  `cx,cy` while the model is built, where the parameter is nearly a camera
  rotation wearing a different name, then release them once — for a single
  camera group with at least `--pp-min-images` images behind it (D50, D51).
- `--refine-extra-params` (on) and `--final-extra-params` (on) are the same pair
  for the distortion coefficients, defaulting the other way because fitting them
  is most of what a distortion model is for. `--no-refine-extra-params` holds
  them at whatever the camera setup started them at and leaves them to the
  finishing pass (D72). That is worth having for a lens with little distortion,
  and for the early small models where the terms can absorb pose error before
  there is enough geometry to contradict them. Both off keeps a calibration you
  already trust exactly as given — principal point included, since it sits
  behind the distortion in the prefix.
- `--distortion k1,k2,...` is where those coefficients start, in the camera
  model's own order (`opencv`: `k1,k2,p1,p2`; `radial`: `k1,k2`). It takes
  `PREFIX=k1,k2,...` for one group, as `--camera-model` and `--focal` do, and it
  travels to `map` inside `matches.bin` with the rest of the camera setup.
- `--final-per-image-intrinsics` (off) adds one pass after those, in which every
  registered image gets its own camera copied from the group it was in (D73).
  Sharing a camera is what makes a focal observable while the model is being
  built; a finished model can afford to let each frame depart from the group,
  which is what follows a lens that drifted — a zoom that crept, a focus that
  breathed. `sanitizeCameras` deliberately does *not* run here: it clamps a
  camera back towards its group's default, and a camera of one image has no
  group left to be clamped to. The solve is much larger — every image owns up to
  twelve intrinsics columns of the reduced system — so on anything but a small
  capture `--ba-solver auto` lands on CG.

`--mapper flat|bottom-up` picks the schedule (see the stage graph; flat is the
default for every capture, and there is no size-based switch);
`--bup-atom-size` and `--bup-overlap` size the atoms and the overlap the
merge aligns on. `--merge-tracks` and `--rank-by-visibility` are on by default
and exist to be turned off when attributing a change.

Two things about the surface changed when it was unified, both deliberate:
`auto --no-merge` now disables *merging* only, which is what it already meant on
`map`; skipping the merge / grow / reseed / split passes entirely is
`--no-manage`,
which now exists on both commands. And `auto` accepts every advanced flag
`extract`, `match` and `map` accept, since it runs those stages — a run can be
tuned without decomposing it into three commands, and the GUI's editor has one
command's worth of fields to show.

## Tests

Each `tests/*.cpp` builds to an executable of the same name that prints
PASS/FAIL and returns 0/1 — the same convention as `src/backend/tests/`.

| binary | covers | needs a GPU |
|---|---|---|
| `sfm_sift_test` | GPU SIFT, matcher, batch decode, camera-model and format round trips | yes |
| `sfm_map_test` | synthetic reconstruction end to end, incl. assembly/audit/split | yes |
| `sfm_cholesky_test` | dense GPU Cholesky vs a CPU reference | yes |
| `sfm_geometry_test` | F, H, E, P3P, triangulation, RANSAC, SVD/eigen kernels | no |
| `sfm_merge_test` | Sim(3) algebra, model alignment, track splicing, fold detection | no |
| `sfm_mask_test` | mask uv sampling, decode, file discovery | no |

End to end, the check that matters is a reconstruction on a public dataset
scored against the reference that ships with it: `tools/sfm/eval_poses.py` reads
a COLMAP model (binary or text), a Nerfstudio `transforms.json` or a Metashape
`.xml` (`tools/sfm/metashape_ref.py`) and reports registration rate, an
alignment-free relative-pose AUC and Sim(3)-aligned absolute errors.

Two things about that metric are worth knowing before reading a number:

- **A pair touching an unregistered image counts as a 180 degree failure**, so
  the AUC is capped by `(registered / total)^2`. On a model whose registered
  poses are all good, AUC *is* the registration rate; do not read it as
  accuracy until coverage is accounted for.
- **The references mostly held the principal point at the image centre**, so a
  model that refines it (D51) is scored against one that absorbed the offset
  into its rotations. That shows up as a uniform relative-rotation error of
  about `dx / f` radians and it is not necessarily the model being worse.

## Not done yet

Inherited from the source tree — none of it is a regression introduced by the
port. Ordered by what blocks the most.

**Scale, past roughly 500-1000 images**

1. **Local BA.** Only global BA runs today. The measurement that decides its
   shape — host dense LM versus a persistent-kernel GPU BA on a 10-30 camera
   problem — was never taken. Take it first. Note what a 1000-image profile
   actually says before assuming this is the win: the mapper's bundle
   adjustments are *mostly small already*, because the model grows from two
   images, and its cost sits in the last few full-size ones. Those are the ones
   local BA does not replace. What made the difference at that size was the
   linear solver's dense/CG crossover (`--ba-solver`) and bounding track length
   (`kMergeMaxTrack`), both of which act on exactly those passes — and, once
   the CG path stopped requiring per-image intrinsics groups, the crossover
   started applying to ordinary captures at all (`ba/README.md`).
2. **Shared GPU primitives**: radix sort, prefix scan, segmented reduction, a
   descriptor-set cache, record-once/replay command buffers. The solver
   re-records a command buffer per LM iteration, which is fine at global-BA
   scale and fatal at local-BA scale. Also the clean fix for the SIFT
   extractor leaking buffers when they grow between images.
3. **Gauge fixing and constant-parameter masks**, per-observation weights, and
   mid-solve outlier down-weighting. LM damping regularizes the gauge today.
4. **Vocabulary tree / global-descriptor index** past ~3k images. Less urgent
   than it was: pair selection is now two-stage (a symmetric mini-vs-mini
   shortlist over every pair, then the reliable asymmetric score on the
   shortlist), which cut the quadratic term 5-6x — 59 s to 10 s on 1194 images —
   while keeping 98-99.5% of the same pair set. The term is still quadratic.

**Quality**

5. ~~Visibility-pyramid next-image scoring~~ — done (`--rank-by-visibility`,
   on by default): the next image is ranked by how its visible structure
   *spreads* over the frame, COLMAP's MIN_UNCERTAINTY default, and an image
   that already failed sorts behind every untried one.
6. ~~Track merging~~ — done (`--merge-tracks`, on by default):
   `Mapper::mergeTracks` fuses two 3D points a correspondence says are the same
   feature, subject to an all-inliers reprojection test, one observation per
   image, the union still subtending the minimum triangulation angle, and a cap
   on the merged track's length — the reduced camera system has an entry per
   image *pair* on a track, so unbounded fusion buys a twentieth observation of
   an already-pinned point and pays for it in the solver.
7. **Misregistration on large unordered sets** — images placed in the wrong part
   of the scene. Diagnosed, then parked in favour of throughput work; the
   visibility ranking of item 5 helped and did not close it.
7b. **The fold split's veto is calibrated on two points.** A real fold's cut
   severs 0.00% of the model's co-visibility (`sfm_merge_test`); the one sound
   model in an 80-dataset corpus that the conflicts talked into a cut severed
   1.30%, and taking it cost 568 images and 65 points of AUC. The veto is now
   0.5%, between them but nearer the fold. It fires on one dataset in eighty,
   so a third data point is worth having before trusting the number.
8. **Nister 5-point** (calibrated init) and **EPnP**.
9. **Automatic camera-model detection.** A fisheye capture run with the default
   rectilinear model reconstructs badly and nothing detects it. The focal
   bootstrap's peripheral inlier curve is a usable signal.

**Integration**

10. **Undistortion stage.** Never written. The dataset parser takes distortion
    parameters, so confirm this is wanted before building it.
11. **Equirectangular end to end.** The mapper writes `EQUIRECTANGULAR`
    (model 17); `ColmapParser` stops at model 10 and the renderer has no
    spherical camera, so such a model cannot currently be trained on.
12. **Faster decode.** A scaled JPEG decode straight to the working resolution
    would cut the CPU time. (The *peak* half of this is done: the decoder
    resamples out of stb's RGB buffer instead of building a full-resolution
    float image, so a concurrent decode costs 3 B per source pixel rather than
    7, and the pool's byte budget now comes from the machine's RAM instead of a
    fixed 1 GiB. Extraction is GPU-bound again on 21 MP inputs.)
13. **Verification, fewer model fits.** A pair with no real geometry still runs
    every RANSAC trial for both F and H. Do *not* re-attempt SPRT for this: it
    was measured and rejected (residual evaluation is a few percent of RANSAC's
    cost). The win is in not proposing hopeless pairs. Three cheaper things
    landed since: an exact early bail in the scoring loop (a model that cannot
    reach the incumbent's inlier count stops being scored), local optimization
    *inside* the trial loop rather than only after it (a better incumbent means
    fewer trials for the same confidence), and a homography residual with no
    transcendental or square root at all.
14. **Matcher register-blocking**, if it is still bandwidth-bound. Measured:
    the win was in the workgroup *width*, not in registers. A pair's train
    descriptors are streamed through groupshared once per workgroup, so the
    traffic is `ceil(nQuery / TQ) * nTrain`; `match_rows` (the pair-selection
    path, which has no column side to constrain its query packing) now runs
    256 wide instead of 64 and reads a scoring pair's train set twice instead
    of eight times. Register-blocking on top of that would halve the
    groupshared reads again at roughly half the occupancy -- untested.

**Unstarted**

15. ~~Learned frontend~~ -- done twice, behind the existing extractor and
    matcher interfaces: ALIKED + LightGlue (`src/aliked/`) and LoMa
    (`src/loma/`, DaD keypoints + DeDoDe descriptors, five matcher variants).
    `--features loma-b128 --matcher loma-b128` is the compact one and
    `--features loma-b --matcher loma-b` the accurate one. Both match
    onnxruntime on the same checkpoints; both are matchers for a SHORTLIST.
    What is left is a scored comparison of the three frontends on a public
    dataset -- nothing here says which to reach for.
16. A **global** (GLOMAP-style) mapper. The **bottom-up** one exists
    (`--mapper bottom-up`, `map/Partition.h` + `map/Bottomup.h`); what it has
    not got is parallel atom reconstruction, which needs a second `rec_` per
    worker and one shared `VkContext`.
17. Parity benchmarking on ETH3D / IMC.

**Deliberately out of scope**, so they are not silently skipped: rig
constraints in the mapper (a rig is used as a ground-truth-free *diagnostic*,
not a constraint), GPS / geo-registration, MVS / dense reconstruction,
incremental database updates, and relating two models that share no images.
