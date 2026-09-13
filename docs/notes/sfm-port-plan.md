# Porting the standalone SfM pipeline into this repo

Plan for moving the Vulkan/Slang Structure-from-Motion pipeline (developed in a
separate private tree) into Spirula Studio, making this its permanent home,
and retiring the COLMAP subprocess for Vulkan builds.

Status: **phases 1-2 landed** (mechanical import; one configuration surface),
**phase 5 landed in a different shape than planned** (2026-08-01, see below),
**phase 4 landed piecemeal**. Phases 3 and 5 are superseded by
[`sfm-in-process-plan.md`](sfm-in-process-plan.md), which plans the library
seam and the GUI's use of it as one change. This document is the plan; delete
the phase checklists as they land and fold the surviving content into
`src/sfm/README.md`.

---

## 1. What lands, and what it replaces

The incoming module is a complete image-to-`sparse/` SfM pipeline with no heavy
dependencies — Vulkan + Slang, C++17, one vendored image decoder. Roughly 22k
lines of C++/Slang plus Python evaluation tooling:

| Stage | What it does | Runs on |
|---|---|---|
| `extract` | GPU SIFT: pyramid → DoG extrema → orientation → top-K → 128-D RootSIFT, EXIF read, keypoint masking, per-keypoint color sampling | GPU + host decode pool |
| `match` | GPU brute-force descriptor matching, GPU pair pre-selection, two-view geometric verification (F/H/E, LO-RANSAC/MSAC) | GPU + host worker pool |
| `map` | Incremental mapper: seed → P3P register → triangulate → global BA → filter → retriangulate → de-register/undo; multi-model output | host control flow, GPU BA |
| `merge` | Sim(3) alignment on shared poses, track splicing, transactional merge | host |
| manage | merge / audit / grow / prune / reseed rounds, duplicate-structure split, joint BA | host |
| BA solver | LM with dense Cholesky or implicit-Schur PCG, `float`/`double`/`df` reals, Huber/Cauchy | GPU |
| `auto` | all of the above from two knobs (`--quality`, `--data-type`) | — |

It replaces `src/app/gui/ColmapRunner.cpp`'s COLMAP half: `colmap
feature_extractor / *_matcher / mapper / model_merger / bundle_adjuster`, the
vocabulary-tree download, and the COLMAP version check. It does **not** replace
ColmapRunner's other half — ffmpeg frame extraction, sharpest-frame selection,
multi-track `.insv` splitting, and AI masking via `reference/scripts/mask.py` — which is
shared, not COLMAP-specific, and gets factored out for both paths (phase 5).

Output is unchanged in kind: `<workspace>/sparse/0/{cameras,images,points3D}.bin`
in COLMAP's binary format, read by the existing `ColmapParser`.

---

## 2. Ground rules

1. **Vulkan only.** The SfM module has no CUDA path and will not get one. It is
   built by default in `SS_BACKEND=vulkan` builds and is opt-in elsewhere.
2. **No Python, no Torch, on the SfM path.** No entry in
   `cmake/sources.txt`; the module stands alone.
3. **This repo becomes upstream.** After the import the source tree is
   read-only; its `AGENTS.md`/`README.md` get a pointer here, and the two rules
   that were specific to it (update `status.md` every session, log every ADR)
   are dropped in favour of this repo's conventions: `src/sfm/README.md` tracks
   coverage the way `src/backend/vulkan/README.md` does, and non-obvious
   numerics go in `docs/notes/sfm-design.md`.
4. **Nothing is dropped in the port.** Section 7 is the inventory; every row
   must be accounted for as ported, deliberately deferred (section 9), or
   deliberately out of scope.
5. **Sanitize on the way in** (section 8). This repo is public; the source is
   not. Machine-local paths, private capture names, and the per-dataset
   benchmark tables do not cross over — including bare names with no path
   attached. Standard academic dataset names (Mip-NeRF 360, Tanks and Temples,
   Deep Blending, ZipNeRF, KITTI, MegaDepth, PPISP) are fine.
6. **Fit this repo's conventions.** PascalCase `.h`/`.cpp` named after what they
   do, path-qualified includes from `src/`, section banners, comments that
   explain *why*. The incoming tree is snake_case and ~90% header-only; both
   change (phases 1 and 3).

---

## 3. Build matrix

New options in `cmake/SsOptions.cmake`:

| Option | Default | Meaning |
|---|---|---|
| `SS_BUILD_SFM` | `ON` for `SS_BACKEND=vulkan`, `OFF` for `cuda` | build the SfM library, CLI and GUI integration |
| `SS_SFM_REALS` | `float;double;df` (`float;double` on Windows) | BA scalar configurations to compile |
| `SS_SFM_LOSSES` | `trivial;huber;cauchy` | BA robust losses to compile |

Resulting targets:

```
SS_BUILD_SFM=ON                 ss_sfm            (static lib)
  (always)                      spirula sfm            (CLI, alongside
                                                           spirula train and,
                                                           on CUDA, spirula mesh)
  + SS_BUILD_GUI=ON             the GUI gains the built-in SfM path
  (always, Vulkan build)            src/sfm/tests/* test executables
```

A CUDA build with `-DSS_BUILD_SFM=ON` is supported (it needs the Vulkan
loader + headers) but is not the default and is not what CI should assume; the
CUDA GUI keeps the COLMAP subprocess path.

`REALS`/`LOSSES` trimming (`-DSS_SFM_REALS=df -DSS_SFM_LOSSES=trivial`,
one blob instead of nine) stays available for fast iteration; the CLI already
reports "variant not built into this binary" for a trimmed-out combination.

---

## 4. Target layout

```
src/sfm/                       the SfM subsystem — Vulkan-only, self-contained
├── README.md                  authoritative subsystem doc + coverage table
├── SfmConfig.h/.cpp           the one options aggregate + its descriptor table
├── Log.h/.cpp                 log sink, progress sink, cancellation token
├── vk/                        VkContext.h/.cpp, EmbeddedSpirv.h
├── core/                      Camera, CameraSetup, Pose, Model, Image,
│                                ImageLoader, Exif, Features, Matches, Mask
├── feature/                   Sift, Matcher, Pairing, PairSelection, Verification
├── geometry/                  Essential, Fundamental, Homography, P3P,
│                                AbsolutePose, Triangulation, TwoView, LinAlg
├── optim/                     Ransac
├── ba/                        README.md (solver design + benchmarks),
│                                Problem, Solver, BalIo
├── map/                       Mapper, Bundle, CorrespondenceGraph, Merge,
│                                Manager, Profile
├── shaders/                   common/ (Real, df, dmath, linalg, camera, loss)
│                                ba/ sift/ match/
└── tests/                     one .cpp per test executable

src/app/cli/sfm_main.cpp       the spirula sfm CLI (next to main.cpp, mesh_main.cpp)
src/app/gui/DatasetPrep.{h,cpp}  video/insv/mask preparation, shared by both runners
src/app/gui/SfmRunner.{h,cpp}    in-process SfM job driver (ColmapRunner's shape)
cmake/SsSfm.cmake          library + shader targets
tools/sfm/                     evaluation and benchmark tooling (Python)
docs/notes/sfm-design.md       condensed decision log
```

Naming notes:

- `src/sfm/` mirrors the incoming `src/{core,feature,geometry,optim,ba,sfm}`
  one-for-one; the incoming `sfm/` becomes `map/` so the path does not read
  `sfm/sfm/`.
- `src/sfm/core/Camera.h` and the engine's `src/core/Camera.h` are different
  types with different jobs. Path-qualified includes keep them apart; do not
  merge them, and do not rename one to look like the other.
- The vendored `third_party/stb_image.h` is dropped in favour of
  `src/external/stb_image.h`. Check API compatibility first and bump the
  repo's copy if the SfM decoder needs a newer one — one copy, not two.

---

## 5. Phases

Each phase ends with a green build of both backends and a runnable artifact.
Do not start the next phase with the previous one's tests red.

### Phase 0 — audit and staging (half a day)

1. Take a full inventory of the source tree against section 7; anything not in
   the table gets added to it.
2. Grep the incoming tree for machine-local paths and private capture names
   (section 8) and write the substitution list.
3. Confirm the two Slang pins agree (both trees pin slangc 2026.12.0.1) and
   that the SfM shaders compile with this repo's `ss_find_slangc`.
4. Decide the disposition of every Python tool in section 7.

*Done when:* the substitution list exists and the shader compile is proven.

### Phase 1 — mechanical import — **landed**

Squash-import, no history: the source repo's commit messages carry private
capture names, so a subtree merge would leak them. Record the source commit id
in the import commit message and in `src/sfm/README.md`.

1. Copy the tree into `src/sfm/` per section 4, renaming files to PascalCase
   `.h` and rewriting every include to be `src/`-relative
   (`#include "sfm/core/Camera.h"`).
2. Apply the section 8 substitutions to comments as they are moved. Do not
   defer this — it is much cheaper here than after the split in phase 3.
3. `cmake/SsSfm.cmake`: the `ss_sfm` static library, the shader
   variant matrix, the embed step. Included from the top-level `CMakeLists.txt`
   after the backend module and before `SsApps.cmake`.
4. Shader build: factor `ss_build_spirv_tool()` out of
   `src/backend/vulkan/shaders/SpirvShaders.cmake` into `cmake/SsSlang.cmake`
   so there is one `spirv_tool` binary for the repo. Give the tool the
   `nocontract` subcommand the `df` blobs need (slangc emits no
   `NoContraction`, and a contracting driver silently destroys the error-free
   transforms the type is built on), and a symbol-prefix option on `embed` so
   the SfM blob table does not collide with the engine's.
   The SfM blobs are whole-module compiles with `-fvk-use-entrypoint-name`,
   which the engine's discover/embed flow does not model — keep the SfM
   enumeration in its own CMake function, sharing only the tool and the slangc
   lookup.
5. `src/app/cli/sfm_main.cpp` and the `spirula sfm` target in
   `SsApps.cmake`. Fold the BA CLI's two useful entry points (BAL problem
   benchmarking, `--selftest-chol`) in as `spirula sfm ba-bench` and
   `spirula sfm selftest-chol` rather than shipping a second executable.
6. `src/sfm/tests/`: split the 2.5k-line self-test TU into one executable per
   area — `sfm_sift_test`, `sfm_geometry_test`, `sfm_map_test`,
   `sfm_mask_test`, `sfm_merge_test`, `sfm_cholesky_test` — built the way
   `src/backend/tests/*.cpp` are (glob → one exe per file, same name).

*Done when:* `bash build_develop.bash -DSS_BACKEND=vulkan`
builds `spirula train` and `spirula sfm`, every `sfm_*_test` passes, and
`spirula sfm auto` reproduces a known-good reconstruction on a public dataset
(Mip-NeRF 360 `garden`, 25-frame subset) with the same registered count and
reprojection error as the source tree.

### Phase 2 — one configuration surface — **landed**

`src/sfm/SfmConfig.{h,cpp}`: the aggregate of the eight stage option structs
(unchanged) plus the pipeline-level knobs, and `SFM_CONFIG_FIELDS`, a
descriptor table of `{member, name, cmds, tier, group, range, choices, help}`
that the CLI parser, `--help` and (phase 5) the GUI editor each read as one
macro expansion. `SfmConfig::finalize()` is the only place pipeline knobs fan
out into the stage structs. Presets live beside the table and report every
field they moved. Everything that does not name one scalar field is still
hand-parsed in `src/app/cli/sfm_main.cpp`, which offers a token to those cases
before the table. The surface is documented in `src/sfm/README.md` §Options;
the CLI also gained generated per-command help, `--version`, and usage errors
that stay off `auto`'s exit codes 2 and 3.

Verified: all 136 pre-port flags still parse; `auto`, `extract`, `match`, `map`
and `merge` produce byte-identical logs before and after on a public dataset
(Mip-NeRF 360 `garden`, 25 frames), at defaults and under a wide flag sweep.
Note the pipeline is not bit-deterministic run to run — the GPU bundle
adjustment is not — so "bit-identical model files" was replaced by "identical
mapper/BA/registration output", which the pre-change binary does not meet
against itself.

Two deliberate behaviour changes, both in `src/sfm/README.md`: `auto
--no-merge` now disables merging only (as it already did on `map`) with
`--no-manage` added to `auto` for the old meaning, and `auto` accepts the
advanced flags of the stages it runs.

### Phase 3 — library-ization — **superseded**

The module is a CLI that prints to stdout/stderr and cannot be stopped. The
three seams it needs to live inside the GUI — log sink, progress, cancellation
— and the header/translation-unit split are planned in
[`sfm-in-process-plan.md`](sfm-in-process-plan.md) §3 and §8, together with the
two this phase did not have: hoisting `auto`'s stage sequencing out of the CLI,
and read accessors on a job that is still running.

### Phase 4 — dataset-parser and format checks — **mostly landed**

Whether every camera model the mapper emits survives the round trip into
training.

1. **Landed:** `EQUIRECTANGULAR` (id 17) is carried rather than refused —
   `ColmapParser` reads it and checks the 2:1 aspect, the projection kernels
   have variants, and the GUI offers it. One gap left: `unproject_raydir`
   (`src/shaders/pixel_wise.slang`) falls back to pinhole for it, so depth and
   normal supervision on an equirect camera is silently wrong.
2. Verify a round trip for every other model the mapper emits: reconstruct,
   parse, train one step. `THIN_PRISM_FISHEYE` and `OPENCV_FISHEYE` in
   particular.
3. **Landed:** intermediates are deleted on success, `--keep-intermediate`
   keeps them, and the GUI exposes it as a checkbox, off.

*Done when:* every camera model the mapper can produce either trains or is
refused with an actionable message.

### Phase 5 — GUI integration — **landed 2026-08-01, out of order**

It landed while the segmentation stack was being merged, and it landed
**without phase 3**: `SfmRunner` drives `spirula sfm` as a **child process**
rather than calling the library in-process, reading stage and progress out of
the child's stdout and its `--progress-dir` snapshots. The reasons that shape
was chosen are in `SfmRunner.h`; what it costs, and what replaces it, are in
[`sfm-in-process-plan.md`](sfm-in-process-plan.md).

The user installs nothing either way: the child is our own binary, found via
`AppPaths::exe_path`, not via PATH.

Everything else landed as written: `DatasetPrep` (item 1, and it grew built-in
video decoding and masking with the ffmpeg/Python subprocesses kept as
fallbacks), `Screen::NewDataset` and the engine selector (item 3), the beginner
panel with its auto-detection (item 4), and the settings persistence (item 6).

Item 5 — the "All SfM options" editor over the phase-2 descriptor table — was
**not** built. The panel surfaces the dozen knobs a beginner or intermediate
user needs, plus a free-form "extra spirula sfm flags" field that reaches the
other ~120. That is a deliberate trade for now: mirroring the table into ImGui
widgets is real work, and the flag field costs nothing and cannot go stale.
Revisit if users actually reach for it.

### Phase 6 — one Vulkan device (2–3 days)

Until this phase the process can hold two Vulkan devices: the engine's
`backend::vk::Context` (a lazily-created singleton) and the SfM `VkContext`.
That works — the GUI sequences dataset creation before training — but it
doubles driver overhead, can select *different* physical devices, and makes
`SS_VK_DEVICE` mean two things.

1. Extend `backend::vk::Context` capability probing with the optional features
   SfM wants: `shaderFloat64`, `VK_KHR_shader_integer_dot_product` (the
   matcher's packed uint8x4 distances), and buffer int64 atomics. Probe and
   enable when present; every one of them already has a fallback path on the
   SfM side, so absence must degrade, not fail.
2. Give `VkContext` an adopt-external-device mode: when the Vulkan backend is
   live, it borrows instance/physical/device/queue-family and submits through
   `backend::vk::Context::submit` so the queue-level timeline semaphore and
   its mutex stay authoritative. When SfM runs standalone (the CLI, or a CUDA
   build) it owns its own device exactly as today.
3. One device-selection knob: `SS_VK_DEVICE` and `--device` resolve to the
   same physical device.
4. Document the VRAM constraint: global BA on a large model and a running
   trainer must not share a device budget. The GUI already sequences them;
   write it down rather than relying on it.

*Done when:* a GUI run that creates a dataset and then trains reports one
device in both subsystems, and `SS_VK_DEVICE` moves both.

### Phase 7 — tools, tests, docs (2 days)

1. **Tools** into `tools/sfm/` (section 7), sanitized: no default data roots,
   no committed private collection lists. `bench_scenes.py` keeps the four
   public academic collections in a committed
   `tools/sfm/collections.json` and reads anything else from a
   user-supplied `--collections-file`.
2. **Tests** documented in `docs/testing.md`: what each `sfm_*_test` covers,
   and the end-to-end check (`spirula sfm auto` on a small public dataset,
   scored with `tools/sfm/eval_poses.py` against the COLMAP ground truth that
   ships with it).
3. **Docs**:
   - `src/sfm/README.md` — the authoritative subsystem document: stage graph,
     data model, on-disk formats, GPU/host split, the numerics that must not
     be re-derived, and the coverage/"not done yet" table (section 9). This is
     the SfM analogue of `src/backend/vulkan/README.md`.
   - `src/sfm/ba/README.md` — the BA solver's design notes and its Ceres
     benchmark numbers, with the machine-local paths removed.
   - `docs/notes/sfm-design.md` — the condensed decision log: one short
     section per decision that still binds behaviour or forbids a retry
     (see section 8 for what gets cut).
   - `AGENTS.md` — `src/sfm/` in the repo map, the Vulkan-only rule, the new
     build options, and the gotchas worth hitting first.
   - `docs/README.md`, `docs/build.md`, `docs/architecture.md`, root
     `README.md` — index rows, the option table, where SfM sits, user-facing
     usage.
4. **Retire the source tree**: its `README.md`/`AGENTS.md` point here, and the
   repo goes read-only.

---

## 6. What stays untouched

- `setup.py` / `pyproject.toml` / `cmake/sources.txt` — the pip build never
  sees SfM.
- `src/bindings/` — no Python binding for SfM.
- `reference/scripts/process_data_colmap.py`, `reference/scripts/run_colmap.bash`,
  `reference/scripts/colmap_utils.py` — standalone Python preprocessing tools with their
  own users. They are not on the GUI path and are out of scope here; revisit
  once `spirula sfm` has run on enough datasets to be the obvious default.
- `ColmapRunner` — kept and working. It is the CUDA GUI's dataset path and the
  fallback everywhere else.

---

## 7. Inventory — nothing here may be silently dropped

**Pipeline features.** GPU SIFT (masking, EXIF, color sampling, budget-bounded
parallel decode, directory batch); GPU brute-force matching (DP4A distances,
resident descriptors, batched submits); GPU pair pre-selection; exhaustive /
sequential / prefilter pairing; two-view verification (F 7- and 8-point, H DLT,
E from F and from bearings, LO-RANSAC/MSAC, model selection, degeneracy checks);
P3P (lambdatwist) + DLT PnP + LM pose refinement with joint focal; DLT
triangulation with angle checks; incremental mapper (strict seed with stepwise
relaxation, seed dedup and memoization, forward-motion seeding, focal bootstrap
by trial reconstruction, COLMAP acceptance gates, incremental next-image
scoring, retriangulation and track completion, reprojection/tri-angle
filtering, de-registration and transactional undo, convergence-adaptive BA
cadence, persistent BA context); multi-model output; model merging (Sim(3) from
shared poses, track splicing, transactional attempts); the manage loop (merge /
audit / grow / prune / reseed, duplicate-structure fold split, joint BA);
`--resume` from models on disk; `--check` and `--audit`; per-group camera
setup carried in `matches.bin`; BA solver (dense Cholesky, implicit-Schur PCG,
`float`/`double`/`df`, Huber/Cauchy, dof-tiered Schur kernels).

**Camera models.** SIMPLE_PINHOLE, PINHOLE, SIMPLE_RADIAL, RADIAL, OPENCV,
FULL_OPENCV, OPENCV_FISHEYE (Kannala-Brandt, past 180°),
THIN_PRISM_FISHEYE, EQUIRECTANGULAR — with the principal point held constant
by default and one principal-point-free global BA at the end for a single
camera group.

**CLI surface.** `auto`, `extract`, `match`, `map`, `merge`, the self-tests,
and the BA solver's BAL benchmark + Cholesky self-test.

**Tests → `src/sfm/tests/`.** SIFT/GPU self-checks, synthetic reconstruction,
two-view geometry, keypoint masking, model merging, dense Cholesky. Add the
two the source tree owed itself: a host-vs-shader camera projection agreement
test, and a COLMAP binary model IO round trip.

**Tools → `tools/sfm/`.** `colmap_io.py`, `eval_poses.py` (COLMAP /
Nerfstudio / Metashape references), `eval_metashape.py`, `metashape_ref.py`,
`compare_sift.py`, `match_graph.py`, `rig_check.py`, `bench_diff.py`,
`bench_rescore.py`, `bench_scenes.py`, `bench_mipnerf360.py`, `genpoly.py`
(offline minimax coefficients), and the BAL benchmark script as
`bench_ba.bash` with no default data root.

**Not ported.** The Ceres reference benchmark (`ceres_ref/`) — it needs a
CUDA-enabled Ceres build and a local path, and the numbers it produced are
already recorded in the BA README. It stays in the source tree; if the BA
solver is reworked, re-run it there.

---

## 8. Sanitization

**Paths.** Home directories, local mount roots, Windows user directories —
none survive; `tools/check_private_paths.sh` has the patterns.
The two CMake cache defaults pointing at local Ceres/CUDA trees go with
`ceres_ref`; the BAL benchmark's `BAL_DIR` default becomes required.

**Private capture names.** In code, two comments name private captures to
explain a numeric choice; rewrite them to describe the *capture*, not the
dataset ("a dual-fisheye 360 rig", "a 24 mm DSLR walk-around") — which reads
better anyway. In docs, the per-decision benchmark tables are mostly private
scene names and go entirely; keep the conclusion, drop the table. In
`bench_scenes.py`, the two `misc` collections are a mix of public and private
captures — move all of it to the user-supplied manifest and commit only the
four academic collections.

**The two large internal docs.** The 3.3k-line decision log and the 2.1k-line
status log do not come over as they are.

- The decision log becomes `docs/notes/sfm-design.md`: one short section per
  decision that still binds — what the choice was, why, and what it forbids.
  Prioritize the ones that would otherwise be re-derived or re-attempted: the
  scoring/RANSAC cost breakdown that makes SPRT not worth retrying, the `svd3`
  relative rank test, Householder QR for minimal-sample null spaces, Slang
  constant tables as brace initializers, the canonical feature ordering that
  makes runs reproducible, the pixel-threshold-per-resolution rule, holding the
  principal point, why fisheye verification runs on bearings, and the fold /
  duplicate-structure judgement. Target a few hundred lines, not three
  thousand.
- The status log's per-session narrative does not come over at all. Its
  forward-looking half becomes section 9 and the coverage table in
  `src/sfm/README.md`; its local-environment notes stay behind.

**Gate.** Extend `tools/check_private_paths.sh` to also scan the new `docs/`
and `src/sfm/` content (`docs/` is currently excluded wholesale) and to accept
an optional word list of private names supplied out-of-tree — an uncommitted
file named by an environment variable, since committing the denylist would
leak exactly what it exists to catch. Run it before the import commit and in
the pre-push habit.

---

## 9. Left-behind work — the to-do the port inherits

None of this is a regression introduced by the port; it is what the source tree
had not finished. It belongs in `src/sfm/README.md`'s coverage table, roughly
in priority order.

**Blocking scale beyond ~500–1000 images**

1. **Local BA.** Global-only BA is what runs today. The latency question that
   shapes the design — is a host-side dense LM enough for a 10–30 camera
   problem, or is a persistent-kernel GPU BA needed — was never measured. Do
   the measurement first; it is a few hours and it decides the rest.
2. **Shared GPU primitives** (radix sort, prefix scan, segmented reduction,
   descriptor-set cache, record-once/replay command buffers). The solver
   re-records a command buffer per LM iteration, which is fine at global-BA
   scale and fatal at local-BA scale. This is also the clean fix for the
   SIFT extractor leaking buffers when they grow between images.
3. **Gauge fixing and constant-parameter masks**, plus per-observation weights
   and mid-solve outlier down-weighting. LM damping regularizes the gauge
   today, which works but is not the same thing.
4. **Vocabulary tree / global-descriptor index** past ~3k images. Pair
   selection is a cheap O(N²) scoring pass and is fine below that.

**Quality**

5. **Visibility-pyramid next-image scoring.** A raw correspondence count today.
6. **Track merging** in the triangulator (fusing two 3D points bridged by a
   correspondence). Continuation, creation and completion exist; merging does
   not.
7. **Misregistration on larger unordered datasets** — images placed in the
   wrong part of the scene, diagnosed and deliberately parked in favour of
   throughput work. Re-examine alongside item 5.
8. **Nister 5-point** (calibrated initialization) and **EPnP**.
9. **Automatic camera-model detection.** A fisheye capture reconstructed with
   the default rectilinear model reconstructs badly and nothing detects it.
   There is a usable signal — the peripheral inlier curve from the focal
   bootstrap peaks in field of view for a fisheye and is flat otherwise.

**Integration**

10. **Undistortion stage.** Never written; the trainer takes distortion
    parameters through the parser today, so this is a convenience, not a
    blocker — confirm before building it.
11. **Equirectangular end to end.** The mapper writes it; the parser and
    renderer do not read it (phase 4).
12. **Faster decode.** `stb_image` is the floor on the extract stage now that
    decode is off the critical path; a scaled JPEG decode (idct 1/2 straight to
    the working resolution) would cut both CPU time and the peak that sizes the
    decode pool.
13. **Verification, fewer model fits.** A pair with no real geometry still runs
    every RANSAC trial for both F and H. Do **not** re-attempt SPRT for this —
    it was measured and rejected (residual evaluation is a few percent of
    RANSAC's cost); the win is in not fitting hopeless pairs at all.
14. **Matcher register-blocking** — the next lever if it is still
    bandwidth-bound. Measure before tuning.

**Stretch, unstarted**

15. Learned frontend (ALIKED / SuperPoint + LightGlue) behind the existing
    extractor/matcher interfaces, via a small Slang inference layer.
16. Global (GLOMAP-style) and hierarchical mappers — they play to the GPU BA's
    strength, and COLMAP 4.x has both in-tree as references.
17. Parity benchmarking on ETH3D / IMC.

**Deliberately out of scope** (documented as such, not forgotten): rig
constraints in the mapper — though a rig *is* used as a ground-truth-free
diagnostic in `rig_check.py` — GPS/geo-registration, MVS/dense reconstruction,
incremental database updates, and relating two models that share no images.

---

## 10. Risks

| Risk | Mitigation |
|---|---|
| Two Vulkan devices in one process between phases 1 and 6 | GUI sequences dataset creation before training; phase 6 removes it. Do not ship a GUI release with both live concurrently. |
| MSVC / Windows never exercised on this code | It is portable C++17 with no POSIX headers, which is a good start. Build with `build_develop.bat` at the end of phase 1 and fix there, not at the end. |
| MoltenVK: no `shaderFloat64`, possibly no integer dot product | The `df` (double-float) real exists for exactly this; the matcher has a scalar fallback. Verify both on Apple silicon and record the supported real/loss matrix per platform. |
| Nine BA blobs + SIFT + match blobs lengthen configure/build | Each blob is its own build-graph edge already, so `-j` bounds it; the `REALS`/`LOSSES` trim stays documented for iteration. |
| The phase-3 header split is 20k lines of churn | Do it directory by directory with a green build after each, and land it *after* phase 1 proves the port reproduces known results — so a numeric regression is never confounded with a mechanical one. |
| Two dataset-creation paths in the GUI | The engine selector hides whichever is unavailable, so neither audience sees a dead option; `DatasetPrep` means the shared 60% is written once. |
| Reconstruction quality regressions go unnoticed | Phase 1's done-when is a reproduction test on a public dataset, and `bench_scenes.py` over the four academic collections becomes the regression sweep. Run it before any change to mapper numerics. |
