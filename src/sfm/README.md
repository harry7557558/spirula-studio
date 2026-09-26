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
  SQLite, no PyTorch. `core/Manifest.cpp` reads its file through the
  repository's header-only `data/Json.h` + `data/Yaml.h` rather than growing a
  second parser; that is the one thing here that reaches outside `sfm/` and
  `core/`, and it costs nothing at link time.
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
whose submit runs past two seconds, which the solver's submit sizing is there
to prevent, ba/README.md "Watchdog"), or an allocation the driver refused —
does not end the run. `solveBundle` re-runs that solve on the host, from the parameters the
device last checkpointed (every 5 s of progress), and sends every later solve
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
               Attitude               the gimbal yaw / pitch / roll a drone writes
                                        into each photo's XMP
               Telemetry              the IMU / GPS a video carries (GPMF, Insta360,
                                        DJI, CAMM), read by content --
                                        docs/notes/imu-gps-for-sfm.md
               PriorSource            what a sensor tells the mapper, the verifier
                                        and the pair list -- docs/notes/sensor-priors.md
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
               host, for devices that run neither fp64 nor df), Priors (the
               camera-side sensor factors both take), README.md
map/         Mapper, Bundle, CorrespondenceGraph, Merge, Profile,
               SensorPriors (the IMU / GPS as a PriorSource), ImuExtrinsic,
               ImuScale, SensorGauge (the gauge of a finished model),
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

### `features.bin`

Little-endian throughout. The header is `char[4] "VKFT"`, `u32 version`,
`i32 width, height`, `u32 count, dim, dtype`; then `count` keypoints of
`{ f32 x, y, scale, orientation }`, then the descriptor blob
(`count * dim * dtypeSize` bytes).

Every version since appends a section at the end, and the reader takes all of
them — a stale cache is reused, never rejected:

| version | appends | an older file reads back as |
|---|---|---|
| v2 | `u8 has_colors`, then `count*3` bytes of per-keypoint RGB | no colors |
| v3 | `f64 exif_focal`, `u32 len`, `len` bytes of `exif_camera` | no EXIF |
| v4 | `i32 extract_width, extract_height` | 0, so `pixelScale() == 1` — thresholds in source pixels, which is what those files were produced under |
| v5 | `u8 has_scores`, then (if 1) `count` f32 detection scores | every response 0, which is what SIFT persisted |
| v6 | `u8 exif_orientation` | 1, the orientation of a file carrying no tag |

## Building

```bash
bash build_develop.bash -DSS_BACKEND=vulkan
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
spirula sfm ba      sparse/0 refined/0          # the mapper's global BA on a model
spirula sfm ba      sparse/0 sparse/0 --real cpu  # ... in place, on the host
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
- **`--prefilter-sequential` (on by default) is the converse, above the
  cutoff**: pair selection takes the sequential window too, so a weak link the
  content score ranked just outside an image's top-k is still matched when the
  file order says the two are neighbours. It applies to a folder of photos as
  much as to video (named in shooting order, it is the same thing); the
  `internet` preset turns it off. On a 470-image `.insv` walk the shortlist
  held only 1540 of the window's 5990 pairs, and 3644 of the 4450 it added
  verified (median 53 inliers, against 223 for the shortlist's own).

Every sequential window runs per folder -- a rig's lenses, several clips --
and, with `--quadratic-overlap` (on), also links each image to the ones 16,
32, 64 ... ahead, up to 2^(overlap-1), as COLMAP's `quadratic_overlap` does.

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

`--level ground` (the default) then levels that frame on the ground rather
than on how the cameras were held: the editor's Auto align
(`core/SceneAlign.h`) finds the plane the points stand on within 60 degrees of
the cameras' up, puts it at z = 0, turns the walls onto the axes and centres
the footprint, keeping the scale. A model levelled this way says `oriented 1`,
`up ground` in `gauge.txt`, so the viewer shows it in its own frame. Where the
ground is not found the camera frame stands and the log says so. A model whose
up something measured (the sensors, the attitude, GPS without altitude) keeps
its tilt and is only moved along Z to put its ground at 0; `horizontal` GPS
takes its tilt from the ground too. `--level cameras` is the camera frame
alone, as the trainer would compute it.

Which up that is, is `--exif-orientation`'s business. A phone held upright
writes a landscape file plus a tag saying to turn it, so the frame's own up
is 90 degrees from the photographer's; `orient` (the default) reads the tag
for the gauge and leaves the pixels alone, `apply` turns the pixels instead
and fits cameras to the turned frame, `none` ignores it. A training run over
the model must be given the SAME value — `docs/datasets.md`, "EXIF
orientation", has the table and the mirrored-tag compromise.

`--exif-attitude` (`auto` by default) replaces the cameras' mean up axis with
a measurement wherever the images carry one: a drone writes its gimbal's yaw, pitch and roll
into every photo (DJI's `drone-dji:Gimbal*Degree` XMP, `core/Attitude.h`).
Each registered image votes for up with its pitch and roll and, under `auto`,
for north with its yaw (`map/AttitudeGauge.h`). A vote more than 10 degrees
from the consensus counts against the set, and a set with more of those than
agreeing votes is refused with a warning, so a wrong convention or a stale
tag cannot tilt a model; scattered headings alone leave the model level and
north unset. `up` takes the tilt alone, `none` ignores the tags, and
`--no-orient` turns it off with the rest.

It matters where the mean camera up axis is no answer at all: a camera looking
down, or a gimbal pitched past the nadir so that every other frame is upside
down. Measured against the vertical and heading of a full similarity fitted to
RTK camera positions (centimetre altitude), on three flights of one DJI M4E:

| capture | cameras' mean up | attitude: votes agree to | attitude: tilt | heading |
|---|---|---|---|---|
| 200 frames of a five-direction oblique survey (pitch -45, -55 rolled 180, -90) | 40.8 deg | 1.05 deg | 0.32 deg | 0.57 deg |
| the whole survey, 1271 frames | 2.0 deg | 1.04 deg | 0.04 deg | 0.45 deg |
| 136 frames flown by hand, pitch +7 to -88 | 7.4 deg | 0.28 deg | 0.17 deg | 0.55 deg |
| 100 frames at pitch 0 | 0.13 deg | 0.34 deg | 0.13 deg | 1.11 deg |

Over a whole survey the four oblique directions nearly cancel in the mean; a
part of one -- a flight cut short, a subset, a model the mapper split off --
is where the guess fails. On the 200 frames that is also the difference
between `--metric-gps horizontal` fitting (200/200 cameras within 5 m, 0.04 m
RMS) and being refused (64/200), since the horizontal fit takes its tilt from
whatever levelled the model; on the hand-flown flight, 0.05 m RMS against
2.0 m.

`--metric-positions FILE` and `--metric-gps` fix that same gauge from an
outside measurement instead, so the model is written **in metres**. The first
reads per-image camera positions in COLMAP's `model_aligner --ref_images_path`
format (`image_name X Y Z` per line, any right-handed metric frame — a LiDAR
trajectory, ARKit, RTK); the second reads each registered image's own EXIF GPS
and converts it to a local east-north-up frame. Either way a similarity is
fitted from the camera centres with LO-RANSAC over the same `estimateSim3` that
model merging uses, and `--metric-max-error` is its inlier radius in metres (0
picks 5 for GPS, 0.5 for a positions file).

`--metric-gps` takes `none` (the CLI default; the GUI asks for `horizontal`),
`horizontal` or `full`, and the difference is the altitude.
`full` fits all seven parameters, so the reference's
vertical sets the model's tilt; `horizontal` fits only scale, heading and place,
against latitude and longitude, and leaves which way is up to the recorded
attitude where the images carry one and to the cameras' own mean up axis where
they do not — the same claims `--orient` makes. A phone's altitude is the worst
component it reports, and over a capture wider than it is tall the fit converts
that error into tilt: on an 850-image walk around a city square (150 m across,
level ground) `full` came out **5.05 degrees off vertical**, spreading the
cameras over 12.9 m of fake height, where `horizontal` leaves them within 2.5 m
and recovers a scale 0.2 % away. The east and north residuals were the same to
2 % either way, so the vertical is what was paid for. `horizontal` also has no
collinearity gate: a turn about the vertical is resisted by the whole in-plane
radius, so a street walked end to end — which `full` refuses — fits.

The fit is refused rather than approximated, and **what refuses it is geometry,
not a noise model**. Fewer than three positioned cameras, reference positions
that do not spread wider than the inlier radius, under half the cameras inlying,
or (full only) cameras lying so close to a line that the reference amplifies
orientation error more than 20x — each reports its own reason with the numbers
behind it; the model is then still written, in the ordinary orient gauge, and
the exit status is 4.

`merge` accepts a single model when a metric reference is given: there is
nothing to merge, and it re-gauges the model in place. That is the way to put
metres on a finished reconstruction without rebuilding it —
`spirula sfm merge ws/sparse --in-place --metric-gps horizontal --images ws/images`.

The scale and orientation uncertainties are **reported and never gated on**.
They come from the inlier residuals assuming uncorrelated noise, and measured
against a reference whose error is correlated — GPS drift — they under-state
the real error by 3.9-4.5x: on one flight a 2 % gate on them passed a 3.6 %
scale error. A gate that passes what it exists to catch is worse than no gate,
so they are printed as the lower bounds they are. `map/MetricGauge.h` has the
algebra (D74).

`--telemetry VIDEO` fixes the same gauge from the **video's own sensors**,
which is the default whenever the GUI extracted the frames from a file that
carries them (Insta360 `.insv`, GoPro `.360`/`.mp4`, DJI `.OSV`, CAMM): its
manifest lists one `captures:` entry per video, and the frame stems carry the
source frame index, which is how a frame gets its time on the sensor clock.
`map/SensorGauge.h` treats every reading as a factor on one small state (a
Sim(3), the IMU biases, and per-lens nuisances), initialises each block in
closed form and refines them together in one robust Levenberg-Marquardt solve:

- **up** from the accelerometer, once the IMU-to-lens rotation is calibrated
  from the reconstruction itself (`map/ImuExtrinsic.h`: the gyro's relative
  rotations must match the poses' and every frame's gravity must land on one
  world vector, both linear in the rotation's nine entries). A left-handed
  sensor frame and the sign of the gyro integration are tested as hypotheses,
  and the IMU clock offset against the video is searched for first;
- **scale** from the accelerometer through pre-integration between
  consecutive frames (`core/Preintegration.h`), in the velocity-free form of
  Mur-Artal and Tardós, solved jointly with both biases because on a gentle
  walk the bias error is as large as the signal. The gravity vector solved
  alongside is the check: 9.82 m/s² within 0.3° of up on a 118 s X5 walk;
- **scale, heading and place** from the GPS log, interpolated at each frame
  and fitted exactly as `--metric-gps horizontal` fits EXIF.

What is missing or degenerate is refused by its own uncertainty rather than
by a rule: a camera that only pans gets up and no scale, a stale phone fix
gets no GPS, a file with an attitude stream but no raw gyro (the Osmo 360)
pre-integrates from the attitude instead, and a per-frame accelerometer has
the attenuation its own aliasing noise causes taken back out. Two scale
sources are combined by information and reported separately, and an
IMU-versus-GPS disagreement beyond three sigma keeps the more certain one
and says so. `--sensor-gauge up` takes the orientation alone; `none` ignores
the sensors. On the X5 walk the whole fit
takes 0.3 s; a metric reference the user passes still outranks an upright-only
sensor frame. `docs/notes/imu-gps-for-sfm.md` records what the files carry
and what was measured.

**The same sensors inside the reconstruction.** Everything above fixes the
gauge of a finished model. With telemetry present the run also uses it while
the model is built (`docs/notes/sensor-priors.md`), through one seam a
future sensor implements the same way (`core/PriorSource.h`;
`map/SensorPriors.h` is the IMU/GPS one):

- **Verification** (`--sensor-verify`, on): a pair whose rotation the gyro
  knows is also verified with that rotation fixed -- a two-point RANSAC over
  the translation (`geometry/KnownRotation.h`) -- and keeps that inlier set
  when it explains 70% of what the free estimate did. Matches to equipment
  moving with the camera, or to a copy of the scene the camera did not turn
  towards, cannot pass it however many there are. The IMU-to-lens rotation
  it needs is calibrated first from a sample of time-adjacent pairs' own
  two-view rotations (the hand-eye fit of `map/ImuExtrinsic.h`, with the
  clock offset searched on the rotation angles), and the stage reports per
  camera group what it got and how many pairs it overruled or disagreed with.
- **Registration** (`--sensor-map`, on): a PnP pose that turns more than
  `max(2 deg, 3 sigma)` off what a placed neighbour and the gyro predict is
  re-solved with the rotation fixed (`ransacPnPKnownRotation`, and the rig
  form for a whole frame) and refused when that finds fewer than the
  registration's own inlier floor; the seed pair takes the gyro's rotation
  when it agrees; the audit does not unseat a pose the gyro vouches for.
- **Bundle adjustment** (`--sensor-map`): every solve, growth and joint alike,
  takes camera-side factors evaluated on the host and added to whichever
  linear system the solver builds (`ba/Priors.h`, `ba/README.md`): the gyro's
  relative rotation between consecutive frames of each lens, gravity in each
  frame against a world up refitted per solve, the accelerometer's metric
  scale as one velocity-free triple constraint per three consecutive frames,
  and GPS positions through a similarity refitted per solve. Every gauge
  quantity is re-estimated from the poses before each solve and frozen inside
  the factors, so the solver carries no global parameter and the model stays
  in its own gauge; the finishing gauge fit above then runs as before.
- **Pairing** (`--sensor-pairs`, on): images the GPS puts within
  `--sensor-pair-radius` metres (20) of each other are matched whatever the
  shortlist thought.

`--sensor-max-dt` (3 s) bounds the gap a gyro rotation may span. The mapper
ends with how many registrations the gyro re-solved or refused and how many
factors the last solve held; `SS_SFM_PRIOR_DUMP=1` prints each one. With no
telemetry none of it runs and the pipeline is the one before it existed.

The sources run in order -- the video's sensors, the recorded attitude, a
metric reference, the fallback -- and read each other: `gauge.txt`'s two bits
are the state as well as the record, so a reference is not fitted over a model
the sensors already made metric, the attitude does not re-level a model the
sensors levelled, `horizontal` skips its own upright pre-transform where either
already levelled the model, and the mean-camera-up fallback runs in exactly one
place, over models nothing measured. A gauge a sensor
settled is never overwritten by the guess it was consulted to replace.

Whatever settled a model's gauge, `sparse/N/gauge.txt` records it beside the
model — `oriented` (is +Z up because something measured it, rather than the
mean camera up axis guessing), `metric` (is a unit a metre), and which source
each came from. Plain text, and read by the viewer: a model that says
`oriented 1` is shown in its own frame with the up guess switched off, and its
grid legend is in metres. Nothing else depends on the file, so a reconstruction
COLMAP wrote is simply one that says nothing.

## Rigs

A rig is a set of lenses with a fixed relative pose -- the two sides of a
dual-fisheye camera, the ten faces a `.360` unwraps into, two cameras on one
mount -- and the reconstruction can be told so (`src/sfm/core/Rig.h`,
docs/notes/sfm-rig-constraints.md). A definition names its **members** as path
prefixes; the images under them with the same path form a **frame**:

```bash
spirula sfm auto IMAGES/ -o ws/ --rig cam0,cam1            # cam0/x.jpg + cam1/x.jpg
spirula sfm auto IMAGES/ -o ws/ --rig 'clip1,clip2:cam0,cam1'   # one rig behind two videos
spirula sfm auto IMAGES/ -o ws/ --rig '*:cam0,cam1'        # ... behind every top-level folder
spirula sfm auto IMAGES/ -o ws/ --rig cam0,cam1 --rig cam2,cam3   # two rigs
spirula sfm auto IMAGES/ -o ws/ --rig dual-fisheye=cam0,cam1     # a 360 camera's two lenses
```

The form with captures keeps frames apart per capture (a stem repeats across
clips) while the calibration is one. The manifest's `rigs:` list spells the same
thing, and may carry a member's known `cam_from_rig` (quaternion and
translation; a zero translation is honoured as such, a nonzero one is used for
its rotation until the model has a scale) and, with `refine:`, which of its
parameters bundle adjustment may move: `all`, `axial` (the rotation and the
translation along the lens's own optical axis), `baseline` (that translation
alone), `translation` or `none`. Members without extrinsics are estimated from
the frames against the first member that has them. `spirula sfm map` takes
`--rig` too. An image claimed by two rigs, or a member no image matches, is an
error.

**Known lens geometry.** A rig whose rotations are given is used from the seed
pair on, instead of after enough frames registered each lens on its own.
`kind: dual-fisheye` (`--rig dual-fisheye=...`) says the first two members are
the back-to-back lenses of one 360 camera: the second turned 180 degrees about
the image's vertical, refined in all 6 DOF; `refine: axial` holds the baseline
to the lens's optical axis.
Insta360 X, DJI Osmo 360 and a PortalCam's two fisheyes all calibrate within
0.8-1.4 degrees of that rotation, so it is refined; the Osmo and the PortalCam,
measured against something metric, put the baseline within a millimetre of
the axis. Spirula Studio sets it for an `.insv` or `.OSV` whose tracks were
extracted in lockstep, starts a `.360`'s views at the rotations it cut them at
(the views of one lens sharing its centre), and offers
it for two photo folders on one rig. `docs/notes/sfm-rig-constraints.md`,
"Known lens geometry", has the measurements.

**Rig-mates in matching.** On a `dual-fisheye` rig, `--rig-pairs` (on)
extends every pair of two frames that verified with at least
`--rig-pair-min-inliers` (30) to the other lens: cam0-cam0 brings cam1-cam1,
cam0-cam1 (the camera turned round) brings cam1-cam0. It is a second, smaller
verification pass over what the first one confirmed, only for frame pairs the
first pass joined weakly, so it composes with pair selection, the sequential
window and loop closure alike. 72-95% of the mates verify on the dual fisheyes
measured; on a `.360`'s narrow views 6% did, which is why it stops there
(`docs/notes/sfm-rig-constraints.md`, "Rig-mates in matching").

What the run does with it, in the order it happens:

- **Calibration** (`Mapper::calibrateRigs`). Frames whose lenses registered on
  their own give the relative pose of each member to the rig's reference lens
  (the member registered alongside another most often); a robust average over
  at least `--rig-min-frames` (3) frames establishes it, and the median angular
  deviation is reported. A member is declined with a line saying so when its
  median deviation exceeds `--rig-max-spread` (1 deg) or fewer than 90% of its
  frames agree within three times that -- the two `.insv` tracks extracted
  frame by frame do this, since each track kept its own sharpest frame, and
  agree on only 55-80% of frames -- and its images register as they always did.
- **Frames register as one thing** (`Mapper::registerFrame`). Once a member is
  calibrated, a candidate whose frame has no lens placed yet brings the whole
  frame, and the frame -- not a lens -- is what the PnP estimates: every
  calibrated member's 2D-3D correspondences go into one pool, and one LO-RANSAC
  over that pool solves for the frame's pose (`ransacRigPnP`,
  `geometry/AbsolutePose.h`). A minimal sample is three correspondences drawn
  from the pool, scored against **all** the members, so a hypothesis that
  explains one lens and contradicts the other nine loses to one that explains
  the frame. Three rays that meet at a lens's optical centre solve as P3P;
  three that do not -- because the lens that supplied one of them was too small
  to fill the sample -- solve as a generalized camera (`geometry/GP3P.h`,
  gp3p), so a frame can be posed that no single lens could pose. The local
  optimization refines the frame pose over every member's inliers
  (`refineFramePose`), and the inlier and ratio gates judge the frame's total.
  Every member is then placed, a lens with nothing of its own to offer on the
  rig's word alone (`--no-rig-blind` turns that off), which is how ten lenses
  that barely overlap -- each too weak to register alone -- or a lens on the
  sky get a pose at all. The ranking that picks the next candidate counts a
  frame's correspondences together for the same reason.

  The sample is drawn from one lens whenever the lens the first draw landed on
  can fill it. Three rays of one lens are exact whatever the calibration is
  worth, while a sample spanning lenses carries the calibration's own error --
  and a rig estimated from the reconstruction is good to a fraction of a
  degree, not to a pixel. Measured on a `.360` capture and on a dual-fisheye
  one, drawing across lenses regardless cost coverage; drawing within one and
  scoring across all of them is what the numbers in
  `docs/notes/sfm-rig-constraints.md` were taken on.
- **Frames stay whole.** A frame with a lens already placed places the others,
  always, and places them together (`Mapper::completeFrame`). The lens that is
  there predicts the frame's pose; that one pose is then refined on **all** the
  waiting lenses' correspondences at once (`refineFramePose`) and kept when it
  explains enough of them and stays within the calibration's spread (floor 1
  deg), and stands as predicted otherwise. A lens with a workable set of its own
  may then take a correction on top of the frame's pose, under the same bound
  and only when it explains more than the frame's pose did -- because the
  extrinsics are estimated rather than exact, and a lens that sees better than
  they know should say so. So the frame pose carries the lenses that have little
  to say, and the ones that have plenty are not held back by it. The summary line
  counts the images placed with no inlier of their own. Every de-registration
  pass judges a frame by its members' points together and drops it whole, and
  every refinement ends by placing the mates of whatever is registered, so no
  lens of a placed frame is ever left out.
- **Bundle adjustment** (`map/Bundle.h`, `ba/README.md` "Rigs"). A frame is one
  6-DOF block and each member one shared `cam_from_rig`, refined unless
  `--no-refine-rigs` or the definition fixed it. A refined member costs every
  one of its observations six more Jacobian columns (measured 1.7x on the
  Schur assembly), so the growth refines hold the extrinsics and let them move
  each time the model has doubled; a final pass refines them in its first round. With
  the extrinsics held a rigged frame has half the pose columns of its images,
  which is where a rigged run gains its time. Images with no observations ride
  along on their frame. The joint solve over several models keeps one
  calibration per model, since each is in its own scale.
- **Merging.** Two models holding different lenses of the same frames align
  through the calibration exactly as if they shared those images, so a 360
  capture that reconstructs as one component per direction is merged rather
  than written as pieces (`map/Merge.h` `poseCorrespondences`).
- **Splitting.** The consistency split (`splitInconsistent`) groups images by
  the verified pairs a model still agrees with, and counts a calibrated rig
  frame as joining its images too. Back-to-back fisheyes share no matches, so
  without that a 4000-image dual-fisheye model with every pair agreeing split
  into its two lenses and spent five minutes merging them back.

`--final-free-rig` (off) runs one last bundle adjustment with the rig set
aside, for a mount that flexed or lenses that did not fire together. With no
`--rig` and no `rigs:` nothing above runs and the mapper is byte-for-byte the
one before rigs existed.

Extraction is where a video's lenses become a rig or not. `spirula sam extract
--sync` (Spirula Studio: "Synchronize lenses") decodes a multi-track file in
lockstep under one sharpness window, so every frame is a rig frame; without it
each track keeps its own sharpest frame and only the coincidences are. A
`.360` is always extracted in lockstep, and its ten views share a stem.

## Sequences

A sequence is a range of images the user says were taken in file-name order:
a video's frames, or a folder shot in a walk. It is told to the run as path
prefixes, exactly as a rig is:

```bash
spirula sfm auto IMAGES/ -o ws/ --sequence .              # the whole folder, in name order
spirula sfm auto IMAGES/ -o ws/ --sequence cam0,cam1      # a dual-fisheye video's two tracks
spirula sfm auto IMAGES/ -o ws/ --sequence clip1 --sequence clip2   # two clips, each in order
```

Images under two members with the same path share one *position* (cam0/00017
and cam1/00017), so a rig's lenses are one sequence and not two. The manifest's
`sequences:` list spells the same thing (`members:` per entry), and Spirula
Studio writes one entry per video and per photo folder ticked "Shot in order",
unless "Use the frame order" is off. `match` and `map` take `--sequence` too.
With no sequence given nothing below runs and the pipeline is byte-for-byte
the one before sequences existed.

**What it is for.** A repeated structure -- one turn of a spiral staircase
against the next, the two identical faces of a racing gate, the same corridor
one floor up -- verifies against the wrong copy of itself with as many
inliers as against the right one, and two-view verification cannot tell,
because the geometry is genuinely consistent. The incremental mapper then
ranks candidates by how much triangulated structure they see, so an image of
the copy is placed on the original as soon as the original is in the model,
and every image that registers through it follows. The model is
self-consistent (0.5 px of reprojection), and wrong: a 488-frame walk up and
down a spiral staircase came out with the turns stacked on one another, and a
walk around four gates with all cameras on one side.

What the view graph lacks is which images were taken *next to each other*,
and a duplicate cannot fake that: the correspondences between an image and
its neighbours in the sequence are to the copy the camera was actually in
front of. So those are consulted first, and the rest of the model only where
they are silent (D79):

- **Seeding.** The seed pair is searched among neighbour pairs (positions at
  most `--overlap` apart) through the whole relaxation ladder; every other
  pair is offered only once those are exhausted. The seed is the one decision
  no consensus can check afterwards.
- **Ordering.** A candidate whose neighbours' points alone could register it
  (at least `min_num_pnp_inliers` of them) ranks ahead of every other,
  whatever its visibility spread, so the model grows along the sequence from
  its frontier and reaches the copy through the walk that connects them,
  rather than by content. Within the frontier the visibility ranking stands.
- **The pose.** An image's 2D-3D pool prefers the neighbour's point for a
  feature both a neighbour and a far image have triangulated, and each pool
  entry is marked near or far. Beside the whole pool's PnP a second one runs
  over the near entries alone; the two poses are judged by *near* inliers
  first and total inliers second, so a pose that explains three hundred
  points of the duplicate and none of the neighbours' loses to one that
  explains the neighbours'. A rig frame does the same over every lens's
  near entries at once.
- **The ratio gate.** Under the sequence, far correspondences the pose does
  not explain are the duplicate elsewhere, not evidence against; so when the
  near entries carry the pose by themselves (their own count and their own
  inlier ratio pass the ordinary gates) the whole pool's ratio is not
  required. When they do not, everything is judged as before.
- **The audit.** An alternative pose that explains more of what the image did
  not bring cannot unseat one its neighbours' points support better.

Nothing assumes a frame rate or smooth motion. Two neighbours with no
verified correspondence between them are simply not near for any purpose
above; an image none of whose neighbours is placed, or whose neighbours'
points cannot register it, takes the ordinary path with the whole pool -- so
a cut in the walk costs what it always cost, a second model to merge, and
never a folded one. What the sequence cannot repair is a neighbour that was
itself placed wrongly: the chain then follows it, as a tracker's would.

`--overlap` is the one number: the matcher's window along each sequence
(matched whatever `--pairs` chose, so the pairs the mapper trusts exist) and
how far apart two images still count as neighbours in mapping. The run
reports one line per sequence and, at the end, how many poses the neighbours
settled against the whole pool and how many registrations they carried past
the ratio gate; both are zero on a capture with no duplicate structure, where
the sequence changes the order of registration and nothing else.
`sfm_sequence_test` walks a synthetic corridor whose far room repeats the
near one feature for feature: without the sequence the copy folds onto the
original, with it the walk comes out at its true length.

Measured on a 488-frame dual-fisheye walk up and down a spiral staircase
(976 images), the turns that used to stack came out as a helix climbing 24 m
and returning, with one residual: at a feature-poor landing a few frames sit
one floor up before the chain rejoins, because on either floor a pose explains
only a quarter of their neighbours' correspondences and the two-view links
across the landing are as thin. That is the limit of what the order alone can
say; what would decide it is evidence the mapper does not use yet -- the
adjacent pairs' own two-view geometry, or the video's IMU. `SS_SFM_SEQ_DUMP=1`
prints one line per registration attempt (near and whole-pool inliers, and
the rival's) to read such a spot from the log.

### Retriangulation

Every global refinement round after the first starts by completing tracks and
retriangulating the whole model (`completeAndRetriangulate`, COLMAP's
CompleteTracks + Retriangulate): each point's correspondences are tested
against it, and each registered image's free features against their
registered partners. Both were serial whole-model passes, so on a long video
they were the stretch between two bundle adjustments where the GPU idles and
one core works -- 34 s of a 3000-frame capture's 332 s of mapping, as much as
the seed search and registration together.

Each half now collects its per-item result on every core against the state
the pass starts from, and commits in the old serial order. A claim only ever
takes a free feature, so an item's collected result can be wrong only if an
earlier commit took one of the features it claims; that item is redone
serially at its turn. The output is the serial pass's exactly -- checked by
running both on the same state, 0 differences over 373 passes and every
growth-time triangulation of a 638-image capture. Growth-time triangulation
after each registration goes the same way, feature by feature.

Most of what the pass then does is futile: on a 4000-image dual-fisheye video
it tried 166M candidate pairs a pass for 13M free features and made no point
from any of them in steady state -- neighbouring frames 1/30 s apart are all
far under the 1.5 degree triangulation angle. Two rays can only reach that
angle if the angle between them is within both reprojection tolerances of it
(each bounded as twice the tolerance over the focal), so a candidate further
off is dropped before triangulation, on world-frame rays cached per pass in
float (`worldRays`; the fisheye bearing behind each is an iterative inversion,
and computing it per candidate was the actual cost). That is 94% of the
candidates there; results are identical with the filter on and off on all
three stress captures. A pass over the 4000-image model went from 27.3 s to
4.9 s, over a 3000-frame DJI walk from 8.1 s to 0.6 s.

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
| `sfm_rig_test` | rig bundle adjustment, GPU against host; a synthetic two-lens rig through the mapper and the merger | yes |
| `sfm_ba_cpu_test` | host bundle adjustment against the written-out normal equations, rigs included | no |
| `sfm_cholesky_test` | dense GPU Cholesky vs a CPU reference | yes |
| `sfm_geometry_test` | F, H, E, P3P, triangulation, RANSAC, SVD/eigen kernels | no |
| `sfm_merge_test` | Sim(3) algebra, model alignment, track splicing, fold detection | no |
| `sfm_attitude_test` | the XMP attitude, its angle convention, the gauge's vote and refusals | no |
| `sfm_mask_test` | mask uv sampling, decode, file discovery | no |
| `sfm_telemetry_test` | the four telemetry carriers on synthetic files, and the sanity checks; `sfm_telemetry_test FILE` prints what a video carries | no |
| `sfm_sequence_test` | the sequence table and its window pairs (`--no-gpu` stops there); a synthetic walk past a duplicated room through the mapper | yes |
| `sfm_prior_test` | pose priors in bundle adjustment: Jacobians against central differences, device against host, a gauge recovered from priors alone (`--no-gpu` keeps to the host) | yes |
| `sfm_sensor_prior_test` | the fixed-rotation two-view and PnP estimators on scenes with equipment and outliers; the telemetry source's calibration, rotations and factors on the synthetic walk | no |

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
11. **Equirectangular end to end.** Done: the mapper writes `EQUIRECTANGULAR`
    (model 17), `ColmapParser` reads it, and the trainer splits it into cube
    faces (`warp_spherical_to_pinhole`). What is untested is how well the
    learned front ends match on a panorama's polar distortion, which is one
    reason a 360 capture is unwrapped into perspective views by default
    (`docs/datasets.md`, "360 cameras").
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

**Deliberately out of scope**, so they are not silently skipped: GPS /
geo-registration beyond the metric gauge, MVS / dense reconstruction,
incremental database updates, and relating two models that share neither an
image nor a rig frame.
