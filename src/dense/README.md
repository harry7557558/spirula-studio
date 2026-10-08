# Dense reconstruction (RoMa v2)

Fixed-camera dense point clouds: registered cameras stay exactly where SfM put
them, RoMa v2 supplies dense correspondences, and this directory triangulates,
filters and fuses them into a PLY that can seed training. The geometry core is
portable host C++ and builds with both trainer backends; matching needs the
optional Vulkan inference layer (`src/roma/`, built under `SS_BUILD_SAM`).

User-facing behaviour and every setting: [`docs/dense.md`](../../docs/dense.md).
The matcher itself: [`src/roma/README.md`](../roma/README.md). History,
measurements and the gates still open: [`docs/notes/roma-dense-validation.md`](../../docs/notes/roma-dense-validation.md).

---

## Quick start for agents

Read in this order: this file, then `docs/dense.md`, then `src/roma/README.md`.
The validation log is long; search it for the topic you are changing.

**Build and test** (Windows; `build_develop.bash` on Linux/macOS):

```bat
build_develop.bat -DSS_BACKEND=vulkan
```

The script runs codegen and the comment, i18n and file-macro checks first.
Close any running `build_vulkan\spirula.exe` before building, or the final link
fails with `LNK1104`. The portable tests need no GPU or model:

```text
dense_geometry_test  dense_reconstruction_test  dense_panorama_test
dense_config_test    dense_pair_selection_test  dense_fusion_test
dense_artifact_test  dense_spill_test           dense_memory_report_test
```

`dense_camera_test`, `dense_field_compare_test` and `dense_live_preview_test`
take dataset arguments and are manual diagnostics (see [Diagnostics](#diagnostics)).

**Rules that are easy to break**

- Rectified matching is the default; original-image (`source`) matching is
  opt-in. Do not change either default, and keep old saved settings loading
  as they did.
- Every camera goes through `data/CameraMath.h` (`pixel_ray`, `ray_pixel`,
  `projection_jacobian`, `pixel_difference`). There is no second camera
  implementation, and none may test `z > 0` on a spherical camera.
- Source-mode tolerances are in **matcher grid cells** (`View::grid_scale`),
  not original pixels. That is what lets one setting serve a 2K photo, a
  fisheye and an 8K panorama in one dataset.
- Do not weaken masks, minimum confidence or distinct-image support to raise
  point counts. Point count is not a quality score.
- Every host working set is a share of one budget, `image_cache_bytes`. If you
  add one, update `planned_host_bytes` in `MemoryReport.cpp`, or the GUI's
  memory projection silently under-reports.
- A new GUI control needs an `SS_MSG` in `i18n/catalog/Dense.h` in all 13
  languages and a `ui::help_on_hover_disabled` tooltip on its label and its
  input. `tools/check_i18n.sh` and `tools/check_font_coverage.py` must pass.
- Anything that changes which predictions are valid must change the cache
  identity (see [Caches](#caches-and-publication)); anything that changes the
  meaning of a published cloud bumps `reconstruction_revision` in `Artifact.h`.

**Adding a setting:** field in `DenseConfig.h`, a row in `ConfigFields.h` (that
one table drives the CLI, JSON, GUI presets and freshness), validation in
`DenseConfig.cpp`, the control and tooltip in `app/gui/DensePanel.cpp`, the
message in `i18n/catalog/Dense.h`, a round trip in
`app/gui/tests/preset_roundtrip_test.cpp`, and a line in `docs/dense.md`.

---

## File map

| Path | Role |
|---|---|
| `src/dense/Geometry.h/.cpp` | Views (rectified face or original calibrated camera), observations, two-view triangulation, robust point-only refinement, depth consistency. |
| `src/dense/Reconstruction.h/.cpp` | Turns pair predictions into disk observation tiles, completes one reference at a time, samples, groups, refines, publishes filtered checkpoints, then fusion and export. |
| `src/dense/ReferenceSampling.h` | Deterministic coverage plus confidence-weighted sampling, in-memory and external. |
| `src/dense/PairSelection.h/.cpp` | Automatic, sequential, exhaustive and explicit pairing; pose-coverage reference selection for source mode. |
| `src/dense/Fusion.h/.cpp` | Disk-partitioned neighbouring-cell fusion with deterministic boundary ownership. |
| `src/dense/DiskArray.h`, `DiskTable.h`, `ExternalSort.h` | Spill-to-disk arrays, a paged read-only table, stable external merge sort. |
| `src/dense/DenseConfig.h/.cpp`, `ConfigFields.h` | The one settings struct, presets, the source-workflow setup, and the field table. |
| `src/dense/Artifact.h`, `Generation.h` | Freshness probes, content verification, immutable generations and the current-generation record. |
| `src/dense/MemoryReport.h/.cpp` | The child's memory report, the host plan, and the out-of-memory risk rule. |
| `src/app/DenseProcessing.cpp` | `run_dense`: parsing, view preparation, decoding, pair jobs, RoMa inference, caches, live snapshot, memory reporter, publication. |
| `src/app/cli/dense_main.cpp` | `spirula dense`. The GUI runs this same command as a child process. |
| `src/app/gui/DensePanel.cpp` | Dense settings and their tooltips; live-preview polling. |
| `src/app/gui/DenseRunner.cpp` | Launches the child and parses its translated progress lines. |
| `src/app/gui/DenseMemoryView.h/.cpp` | Memory risk tag, RAM/VRAM bars and the projection card in the strip above the log. |
| `src/data/CameraMath.h/.cpp` | Shared host camera models: every projection, inverse and Jacobian used here. |
| `src/core/HostMemory.h` | Physical, available and per-process RAM. |
| `src/i18n/catalog/Dense.h` | Every dense message and tooltip, in 13 languages. |
| `src/roma/` | The native RoMa v2 matcher (`Session`, model, shaders). |
| `tools/roma/*.py` | Hand-run reference comparisons. Never a build or runtime dependency. |

---

## Pipeline

```text
parse dataset ──► prepare views ──► plan pairs ──► decode (workers, ahead of GPU)
                                                        │
              RoMa inference (one GPU thread) ◄─────────┘   verified cache hit skips it
                        │
          add_pair (consumer thread, one pair behind)
                        │  observation tiles on disk, per reference
          reference complete? ──► refine_reference (bounded worker pool)
                        │             sample → triangulate → group → refine → normals
                        ▼
          filtered checkpoint (append-only, live preview)
                        │
          fuse_surfaces (disk partitions + halos) ──► optional exact outlier filter
                        │
          PLY + manifest as an immutable generation ──► current-generation record
```

**Views.** In rectified mode, `GeometryWarp` resamples each image (wide lenses
into up to six pinhole faces) and every face is a pinhole `View`. In source
mode, each original image is one `View` that keeps its model, distortion tier
and any fitted source lens (`source_model`), and RoMa sees the original pixels
resized only to its grid. Faces from one image share `source_image`, so they
never count as independent support.

**Pairs.** Rectified automatic mode pairs *views*: every face of a split lens
is a `PairImage` of its own (`face` > 0), faces of one image are never paired,
and split faces pair only through shared sparse points. A reference's
candidates come from the inverted track index, each shared point weighted by
`min(1, (angle/10°)²)` of the angle the two centres see it from, and from a
kd-tree over camera centres when tracks run short, so nothing loops over all
view pairs. `reference_coverage` (C) walks views in capture order and keeps a
view as a reference only while a tenth of its sparse-point cells (2% of the
median view depth) are seen by fewer than C earlier references; views with
under 32 cells always stay. Source automatic
mode picks `reference_fraction` of the images by farthest-point coverage in
pose space and emits directed reference→neighbour jobs. Its sequential,
exhaustive and explicit modes emit both directed jobs, and in rectified mode
expand image pairs to every overlapping face pair. Rectified jobs are
undirected; `add_pair` reconstructs both directions from one prediction. Jobs
are sorted on disk; a reference is complete when its last job has been
consumed.

**Faces.** Dense plans `GeometryWarp` with `FaceLayout::Cube`: upright front,
side and (past 270°) back faces cropped to the visible lens, so a fisheye is
five views rather than the geometry step's cross-fading ring of up to fifteen.
Cube plans have no gather map; only the geometry step gathers.

**Reference completion.** All of a reference's pairs land in disk tiles keyed
by matcher pixel. On completion the reference is processed alone, so a
published point has already seen every planned neighbour. References run on a
worker pool sized from CPU count and memory, and they publish in submission
order, so output is deterministic.

---

## The math

Notation: a matcher grid of `W × H` cells; a view with `width × height` pixels;
`R` world-to-camera rotation; `c` camera centre.

### Pixels, rays and the grid

Pixel centres are at `+0.5`. A grid cell `(x, y)` maps to the view pixel
`((x + 0.5)·width/W, (y + 0.5)·height/H)`. RoMa's warp is normalized to
`[-1, 1]` with `align_corners=false`, so a target pixel is
`((u + 1)/2 · width, (v + 1)/2 · height)`.

`pixel_ray` inverts any camera: `generate_ray` on the fitted model gives a
seed, then Gauss–Newton on the true projection (`ray_pixel`, which uses the
fitted source lens when there is one) with a numeric Jacobian and backtracking.
It accepts at 1e-6 px, or a stall below 1e-3 px, and the result must land
back inside the frame. Panorama longitude wraps by `2π·fx`
(`pixel_difference`), so residuals across the seam are short.

### Tolerances in matcher cells

Each source view carries `grid_scale = (W/width, H/height)`. Every acceptance
residual is

```text
e = ‖(Δx·grid_scale.x, Δy·grid_scale.y)‖        (residual_length)
```

That is, in units of the matcher's own resolution, which is also the cycle
check's unit. Rectified views keep `grid_scale = (1, 1)`, so their tolerance
stays in face pixels as before. The default source tolerance is one cell.

### Triangulation

Rectified: DLT (`sfm::triangulateDLT`) in a frame centred on the first camera,
which keeps precision far from the origin. Source: the midpoint of the two
world rays `c_a + s·r_a` and `c_b + t·r_b`:

```text
d = r_a·r_b,  b = c_b − c_a,  det = 1 − d²
s = (r_a·b − d·r_b·b) / det,   t = (d·r_a·b − r_b·b) / det
X = c_a + (s·r_a + b + t·r_b) / 2
```

It rejects `det ≤ 1e-12` (parallel rays) and `s ≤ 0` or `t ≤ 0`, which is the
correct cheirality test for fisheye rays past 90° and for panoramas. Then the
triangulation angle must be at least `min_angle_degrees`, and both residuals
at most the tolerance. Reprojection is also infinite when the point lies
behind the observed ray (`(R(X − c))·ray ≤ 0`).

### Grouping a reference sample

Every neighbour gives one candidate point for a sampled reference pixel. The
seed is the candidate that most distinct source images agree with. In source
mode, candidate `r` agrees with point `X` when `X` lies in front along the
reference ray and reprojects into `r`'s target observation within
**two tolerances**. Testing in the image where the error happened works the
same for every lens. A relative depth band does not: a wide lens has few cells
per radian, so its honest depth estimates scatter more. Rectified mode keeps
the original band, `|z_a − z_b| ≤ ε·min(z_a, z_b)` with
`ε = max_relative_depth_error`.

The initial estimate is the inverse-error-weighted mean of agreeing
candidates: `w = 1 / max(e_reference, e_target)`.

### Robust point-only refinement

Cameras are fixed; only `X ∈ ℝ³` moves. Each observation `i` has a 2×2
precision `Λᵢ` from RoMa, converted from grid to pixel units by `Λ·s²`
(`s = W/width`), and an overlap weight `oᵢ`. The Mahalanobis residual is
`dᵢ = √(Δᵢᵀ Λᵢ Δᵢ)` and the cost is Huber:

```text
C(X) = Σ oᵢ · ρ(dᵢ),   ρ(d) = d²/2 if d ≤ k,  else k(d − k/2),   k = tolerance
```

Each iteration solves the IRLS normal equations `(Σ wᵢ Jᵢᵀ Λᵢ Jᵢ) δ = −Σ wᵢ Jᵢᵀ Λᵢ Δᵢ`
with `wᵢ = oᵢ·min(1, k/dᵢ)`. They are diagonal-normalized with a 1e-9
ridge, followed by a halving line search on `C`. `Jᵢ = ∂pixel/∂X` is analytic
for a pinhole (`f/z · (rᵀ − (q/z)·r₃ᵀ)`) and otherwise
`projection_jacobian(q)·R`, numeric through the true camera. After up to three
passes, any observation whose plain-cell residual exceeds the tolerance is
dropped. The point survives only if it keeps `min_source_images` distinct
images, still includes the reference, and has a triangulation angle of at least
`min_angle_degrees`.

### Sampling

With `N = samples_per_reference` (0 = every eligible cell), 15% of the samples
come from a `columns × rows` grid sized to the image aspect, taking the
highest-confidence cell in each block. The rest use Efraimidis–Spirakis
weighted reservoir sampling: each candidate gets the key
`log(u)/confidence` with `u = hash(seed ⊕ pixel)`, and the largest keys win.
The result is confidence-proportional, deterministic for a seed, and possible
in one streaming pass (an external variant spills when `N` is large).
Cells below `min_overlap` are rejected before sampling, never clamped up.

### Spacing, normals, fusion

Point spacing is depth times the angle between neighbouring sampled rays
(source mode), or `z·stride·√(width·height / (W·H·fx·fy))` (rectified). The
voxel is the median spacing unless `voxel_size` is set. Surface radius is
`z·max_relative_depth_error`. Normals come from the cross product of
grid-neighbour differences when both neighbours are depth-consistent.

Two surfaces fuse when they lie within one voxel and either

- both have normals with `|n_a·n_b| > 0.9` (about 26°) and each lies within
  the smaller radius of the other's plane, or
- one lacks a normal and they lie within `min(voxel, radius)`.

Merges are support-weighted, and a cluster may not grow beyond one voxel in
diameter, which prevents chains. Partitions carry one-cell halos. Each
boundary pair belongs to the lower cell key, so nothing merges twice or
duplicates across a boundary.

---

## Memory model

The run sizes everything from one host budget,
`B = image_cache_bytes` (auto: `min(physical/8, available/2)`). During
matching, these coexist:

| Working set | Bound |
|---|---|
| Decoded view pixels (LRU) | B |
| Queued predictions awaiting reconstruction | B |
| Warp plans (rectified only) | B |
| Pending observation tiles | B/8 |
| Reference workers, together | B/4 |
| Paged job table | B/32 |

`planned_host_bytes(B, rectified)` is that sum: `2.406·B`, or `3.406·B`
rectified. The reporter adds the process size at start, giving the **planned
host ceiling**. On the device, RoMa preflights `weights + planned scratch`
against 80% of VRAM and lets its feature cache grow to a limit inside the
driver budget. The **planned device ceiling** is current use plus the cache's
remaining headroom.

`MemoryReporter` (`DenseProcessing.cpp`) writes `dense/progress/memory.json`
twice a second, atomically. The GUI's `DenseMemoryView` reads it and draws it in
a strip above the log, right-aligned like the training VRAM readout. The risk rule is
`demand = max(planned, now) [+ other programs on the device] / supply`, where
host supply is this process plus free RAM and device supply is the device
size. Below 0.8 is low, below 0.95 medium, otherwise high; the shown risk is
the worse of the two. `VK_EXT_memory_budget` reports *this process's* heap
usage, so "other programs" on the device is device size minus our budget, not
usage minus ours.

---

## Caches and publication

Predictions are cached per directed pair under `dense/cache/pairs-v4/<inference>/`.
`<inference>` digests the checkpoint SHA-256, precision, grid sizes, direction
and colour settings; each pair digests the two views' pixel identities (image
content, matching space, EXIF turns, and for rectified mode the full face
calibration). Geometry, masks, sampling and fusion changes therefore reuse
predictions. Every file has a `.sha256` and is verified before use. A reverse
prediction is reused only when it exists; a forward field is never inverted.

The final PLY and manifest are written as an immutable generation, then one
small current-generation record is replaced atomically. Preview, planner and
training all resolve that record and verify its checksum. A writer lock
serializes runs. At startup, under that lock, `cleanup_abandoned_runs` deletes
`run-*` work folders left by cancelled (force-killed) or crashed runs. Cleanup only touches validated, application-owned
directories. Before publishing, every input is re-hashed. An edit that kept
sizes and timestamps still fails the run.

The live preview is `dense/progress/model.bin` (live-model protocol version 7,
`app/gui/SfmProgress.h`) over an append-only points file. Flag bit 16 marks
legacy raw observations, bit 32 filtered points awaiting fusion. A separate
file holds the final cloud.

---

## Tests

| Test | Covers |
|---|---|
| `dense_geometry_test` | Analytic plane, pixel centres, rotated and far-from-origin cameras, robust multi-view support, source lenses, spherical cheirality, seam wrapping, and a pixel→ray→pixel round trip for every model × distortion tier plus FOV, SIMPLE_DIVISION, EUCM, Fisheye624 and skewed-fisheye source lenses; cell-unit tolerance across resolutions. |
| `dense_reconstruction_test` | Disk observations, cycle and mask filtering, three-image support, fusion, streaming PLY, source-frame transform, cancellation. |
| `dense_panorama_test` | Calibrated panorama reconstruction of a sphere, periodic colour/mask sampling, wrapped cycle interpolation. |
| `dense_config_test` | Defaults, presets, source-workflow setup, validation. |
| `dense_pair_selection_test` | Ranking, bounded degree, sequences, explicit/exhaustive modes, cancellation. |
| `dense_fusion_test` | Cell and partition boundaries, normals, depth layers, chain bounds, parallel/spilled equality. |
| `dense_artifact_test` | Locking, cleanup, generations, interrupted publication, legacy adoption, checksums. |
| `dense_spill_test` | Spill arrays, external merge, sampler equivalence. |
| `dense_memory_report_test` | Host plan, risk thresholds, device gating, report round trip, torn files. |

---

## Diagnostics

- `dense_camera_test COLMAP_DATASET` checks each calibration against the SfM
  projection and bearings, without loading a model or a GPU.
- `SS_DENSE_DUMP_VIEWS=DIR` writes the exact post-conversion float RGB, keep
  masks and metadata. `SS_DENSE_DUMP_PAIRS=DIR` writes native prediction
  fields as `profile_AB_{warp,overlap,precision}.f32` plus `pair.json`. They
  contain private paths; keep them out of tracked files.
- `dense_field_compare_test DATASET SPEC_JSON OUT_DIR` runs native and
  reference fields through the same geometry, sampling, support and fusion and
  writes `comparison.json`. It takes an optional `expected_plane` for
  known-plane distances.
- `dense_live_preview_test PROGRESS_DIR POINT_RECORDS OUT_PNG [DATASET]`
  renders a checkpoint through the shared renderer, and with a dataset compares
  it against the published PLY from the same viewpoint.
- On Windows the executable manifest opts into long paths, so nested
  prediction cache paths can exceed `MAX_PATH` where the OS policy allows.
