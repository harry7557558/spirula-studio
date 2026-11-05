# RoMa dense implementation and validation

This is the record of native RoMa validation during dense-step development.
It supplements `src/roma/README.md` and `src/dense/README.md`; neither a
finite-output benchmark nor a numerical comparison proves scene quality.
Measurements were taken on one development machine and are observations, not
targets; captures are described by their camera mix, not named.

## Scope and implementation decisions

The existing sparse LoMa/LightGlue matcher cannot provide RoMa's dense warp,
overlap, and precision fields. The port reuses the existing `nn/` runtime,
attention, convolution, tensor storage, image resizing, and model-loading
infrastructure. RoMa-specific architecture lives in `src/roma/`; fixed-camera
reconstruction lives in `src/dense/`. Python reference tools remain manual
development checks, outside application and build dependencies.

The numerical reference uses FP32 weights and activations. Cooperative
attention is disabled in that path because it converts operands to FP16. A
separate mixed FP16/FP32 path (below) preserves the FP32 reference.

## Findings and corrections

- DPT uses an in-place ReLU on its residual input. Rectifying the convolution
  input alone left an incorrect skip path; both now use the rectified input.
- Single-channel confidence maps require an explicit three-dimensional tensor
  shape; implicit trailing-dimension removal broke map consumers.
- RoMa and LoMa share one bicubic implementation. Its existing Pillow behavior
  remains the default. RoMa selects FP32 coordinates and weights, normalizing
  each coefficient before accumulation. This matches the upstream FP32
  antialiased resize arithmetic, including non-integer ratios. See
  [PyTorch's antialias weight computation](https://github.com/pytorch/pytorch/blob/main/aten/src/ATen/native/cpu/UpSampleKernel.cpp).
- ImageNet normalization subtracts the mean before dividing by the standard
  deviation. Rewriting it as an affine multiply/add changes rounding.
- Local correlation adds its window in normalized coordinates before pixel
  conversion. Adding integer offsets afterwards is mathematically equivalent
  but has different FP32 rounding.

The CPU resize reference agrees exactly on twelve fixtures: two 1280x960 RGB
patterns resized to 320, 512, 640, 800, and 1280 square grids and a 48x32
grid. Identity and constant-image tests also pass. The generic native
GPU-operation suite passes 167 checks, including scalar references for
division, local correlation at larger resolutions, and wide addressing.

## Production-resolution parity remains open

The high-frequency repeating fixture (`aliasing`) becomes nearly uniform at
Turbo resolution, and small descriptor differences grow through the multiview
transformer and refinement. It is kept as a failing case, not relabeled.

After the resize, normalization, and correlation corrections, Precise's
preprocessing agrees exactly and its coarse outputs pass the 2e-4 development
gate. The complete 800-to-1280 refinement cascade still exceeds it. On pixels
with reference overlap at least 0.5, median warp error is under 0.0005 output
pixels and the 99th percentile under 0.061 pixels, but rare maxima reach tens
of pixels; cycle and geometry rejection must catch those.

With identical native preceding predictions, all 28 isolated resize/refiner
comparisons pass at production resolution (worst relative L2 1.695e-5). This
separates local refiner accuracy from the cascade's accumulated sensitivity;
it does not close end-to-end parity. The gate has not been relaxed.

A forty-case FP32 matrix uses exact native-decoded float RGB, two checkpoints
(the official `v2.0.1/romav2.0.1.pt` and a second `romav2.pt` whose state
dictionary has the same 907 keys but 323 differing entries), and the four
presets. The table reports the worst relative L2 among final warp, overlap and
precision; the gate remains 0.0002.

| Checkpoint | Input pair | Turbo | Fast | Base | Precise |
| --- | --- | ---: | ---: | ---: | ---: |
| Official | Textured | 4.330e-5 | 2.464e-4 | 4.852e-4 | 1.392e-3 |
| Official | Aliasing | 1.772e-2 | 2.638e-4 | 4.562e-4 | 1.126e-3 |
| Official | Photo | 4.076e-3 | 1.483e-3 | 1.592e-3 | 1.487e-1 |
| Official | Fisheye | 2.434e-5 | 9.877e-5 | 1.554e-3 | 3.406e-2 |
| Official | Panorama | 2.763e-5 | 3.683e-4 | 2.686e-4 | 4.829e-3 |
| Second | Textured | 3.125e-5 | 2.572e-4 | 2.478e-4 | 1.010e-3 |
| Second | Aliasing | 9.554e-2 | 1.134e-4 | 1.771e-4 | 3.661e-4 |
| Second | Photo | 2.098e-4 | 3.194e-4 | 4.064e-4 | 7.874e-2 |
| Second | Fisheye | 1.277e-4 | 7.036e-4 | 7.934e-4 | 1.393e-1 |
| Second | Panorama | 2.805e-5 | 2.209e-4 | 2.344e-4 | 7.886e-2 |

Nine cases pass and thirty-one fail. The official photo Precise case first
fails in the low-resolution scale-2 confidence cascade, after descriptors and
coarse matching pass. Because the two checkpoints differ, any image-level
comparison must control the checkpoint as well as pixels, preset, precision
and the pixel-coordinate convention.

## Diagnostics

`SS_ROMA_DUMP=DIR` enables `.npy` dumps for resized images, descriptor taps,
multiview blocks, embeddings, coarse outputs, and refinements. It is disabled
by default. `roma_profile_test CHECKPOINT PRESET DIR [textured|aliasing]`
writes final `.f32` outputs. `tools/roma/compare_pipeline.py --profile-preset`
compares them to pinned upstream sources; `--isolate-matcher` and
`--isolate-refiners` use native preceding inputs. `resize_rgb_test DIR` and
`tools/roma/compare_resize.py --native-dump DIR` check resizing independently.
`SS_ROMA_PROFILE_CACHE=off|reference|both` and `SS_ROMA_PROFILE_REPEAT=N`
select the feature-cache workloads below. Diagnostic dumps synchronize and
download tensors, so their timings are not comparable to normal runs.

## Reconstruction and workflow

Analytic tests cover centered DLT triangulation, robust point-only refinement,
distinct original-image support, geometry rejection, and large coordinates.
Shared camera-warp tests cover face/source pixel and ray round trips. The
shared PLY writer supports double coordinates, and the parser exposes
Nerfstudio's forward stored-frame transform.

The native processing entry point connects the RoMa session to dataset
parsing, GeometryWarp view preparation, mask sampling, pair selection,
persistent match caching, disk-backed observation collection, distinct-image
point refinement, surface-aware cell fusion, and double-coordinate PLY export.
The CLI exposes resolved settings through one field table and supports
settings JSON, progress, and cancellation. Atomic file replacement uses the
Windows replacement API rather than assuming that rename overwrites. The
processing parser selects `camera-mean` centering, which centers positions in
double before float pose storage and restores that center during export.

A posed four-view synthetic fixture (512x384 textured RGB, calibrated pinhole
cameras, known baseline) is the standing workflow check. Turbo matching of all
six source pairs exports 6,568 points; a rerun reuses all six predictions and
reproduces the cloud byte for byte. Overlap set to 1 produces the expected
empty-result error and leaves the previous cloud unchanged. The trainer loads
the cloud through `--seed-pointcloud dense/roma.ply`, completes two steps and
saves a checkpoint: a seed-handoff check, not a convergence claim.

The optional fifth dataset step is integrated into both reconstruction
runners, the shared planner, dataset records, and presets. The desktop panel
exposes common controls and a validated JSON editor for all settings.
Completed clouds open in the shared point viewer; trainer and batch handoff
use the existing seed field, and explicit seeds keep precedence. Planner tests
cover default-off compatibility, enabling, reuse, dense-only settings changes,
camera invalidation, explicit redo, disabling and output-size validation.

The manifest records input paths and stamps, resolved settings, checkpoint
and match identities, cloud size and SHA-256, counts, rejection statistics and
timing. Inputs changed during matching or refinement are rejected before
publication. Freshness watches camera transforms only, so recording the step's
own success does not invalidate its result.

## Host pipeline

Observation records are buffered and appended per tile; views decode on
workers ahead of the GPU; caching and `add_pair` run one pair behind on their
own thread; JPEG alpha is decoded once; mask intersection uses row/column
tables; file hashes are memoized by path, size and timestamp and computed in
parallel; warp plans are shared between cameras with identical calibration.
On a masked pinhole capture this cut a 12-pair run from 58.4 s to
25.5 s cold and 11.7 s warm with an identical cloud, leaving matching GPU-bound.
Match identities are unchanged, so existing pair caches stay valid.

Large-capture CPU costs were also material. Automatic source ranking counts
shared observations through an inverted sparse-track index, preserving the
previous ranking and degree cap. Calibration plans have a byte-budget LRU and
reuse exact source-pixel mask indices. Each source image is decoded, masked,
eroded and resized once for its cached faces. Source images and face pixels
share one LRU sized from physical RAM; decoder workers follow CPU threads, that
budget and image dimensions. With `sparse_face_pairs`, split-camera candidates
with no shared projected sparse track are skipped: a documented coverage
trade-off that sources without shared tracks do not take.

## Image-feature reuse

The [Lichtfeld densification plugin](https://github.com/shadygm/Lichtfeld-Densification-Plugin)'s
[matcher](https://github.com/shadygm/Lichtfeld-Densification-Plugin/blob/main/core/matcher.py)
computes reference DINO features once and reuses them for each neighbor.
Spirula's session has an equivalent optional cached entry into the same native
FP32 pipeline: immutable working-view IDs reuse resized RGB and DINO
descriptor maps for either side of a pair. Pair-dependent transformer
matching, VGG features and refinement stay per pair. No plugin code, Python
dependency or reduced precision was added.

The cache uses session-owned device allocations with LRU eviction. Its size
follows the selected device's memory and live allocations, within the device
budget after weights and reserved scratch, reduced further by
`VK_EXT_memory_budget` where the driver reports it. There is no fixed byte or
view-count cap. Both current pair views are protected during eviction; views
too large for the remaining space use ordinary inference.

`roma_pipeline_test` checks exact equality of cold/warm cached and uncached
warp, overlap and precision in both directions, plus reverse reuse, eviction,
zero-cache fallback, grid changes, cancellation and unload. Turbo medians over
16 matches of the textured fixture:

| Feature reuse | Relative throughput |
|---|---:|
| None | 1.00× |
| Fixed reference, fresh neighbor each pair | 1.35× |
| Both views already cached | 2.23× |

Fast measures 1.30× and 1.93×, Precise 1.13× and 1.30×; prediction dumps are
identical across cache modes for every preset.

## Mixed precision

The plugin's RoMa runs under BF16 autocast. Spirula reuses its native
packed-FP16 weight kernels and cooperative-matrix dispatch for a mixed path;
depthwise filters, normalization, biases, activations and accumulation stay
FP32, and geometry stays double. Automatic precision chooses mixed where the
selected device supports cooperative matrices and FP32 otherwise; disabling
them through the runtime override resolves to FP32. Mixed weights take about
half the device memory of FP32 (0.85 GB against 1.70 GB for the official
checkpoint). Mixed predictions have their own disk-cache namespace. Mixed
arithmetic changes accepted geometry and is not claimed as FP32 parity. The
session and cache regressions pass on a discrete and an integrated GPU.

## Masks

The dense mask switch bypasses alpha, sidecar and feature masks while
preserving invalid-lens filtering. Training uses one shared control for the
three mask modes (cut out background, ignore distractors, don't use masks); an
unset policy resolves to cut-out for a dense seed. The loss regression checks
the transmittance gradient, and the image-loader regression checks that masks
off means no alpha or sidecar mask in both cache modes.

## Live checkpoints and preview

Reconstruction appends every accepted observation to a buffered 15-byte point
file. A flushed prefix is published through atomic `model.bin` metadata every
1.5 seconds; final points go to a separate file, so a reader never races
finalization. The GUI reads on a background thread at about 5% duty with no
point-count ceiling, checks host working storage against 80% of available RAM,
and pauses with a message on OpenGL allocation errors. `dense_live_preview_test`
renders 8,000,001 points in a hidden context and covers prefix retry, the host
budget, safe filenames and older snapshot versions.

Defects found and fixed here: the reader once copied float coordinates' raw
bytes into doubles, collapsing the preview; dense snapshots once declared every
camera as PINHOLE, losing fisheye and panorama wireframes (they now carry the
parser's camera fields). An impossible saved combination (automatic pairing,
one neighbor, three-image support) threw during dataset restore; it is now
rejected only by run validation, so a draft stays editable.

A live preview of a mixed capture showed curved sheets. Rendering provisional
and final checkpoints showed these are unfiltered pairwise triangulations that
multi-view filtering removes; the preview now labels the provisional phase.

## Source-image matching

Rectified face matching stays the default; an explicit source mode matches
original images. It selects 80% coverage references, three directed
neighbors and 10,000 samples per reference. Source views keep calibration,
original dimensions and pose. Reconstruction uses centered ray intersection,
wrapped panorama residuals and positive distance along the reference ray.
Samples are chosen once per reference (15% coverage, 85% weighted);
candidates are grouped, initialized by inverse-error weights, refined and
rechecked for distinct-image support before the live checkpoint shows them.

A comparison with another RoMa pipeline motivated this mode. That pipeline
matches original images directly, checks reprojection in original pixels,
samples once per reference, and keeps a 400,000-point preview reservoir. Two of
its behaviors are easy to misread: it disables masks regardless of mask files,
and its confidence floor clamps scores upward rather than rejecting. Neither,
nor its unenforced two-image track minimum or display cap, became a native
default.

`dense_camera_test` checks GeometryWarp face mapping against the SfM camera's
projection and inverse bearing for a capture's real calibrations (OpenCV,
equirectangular and thin-prism fisheye): the largest discrepancy is 0.001076
pixels, and 3.67e-7 in unit rays. This validates conventions, not calibration
accuracy. An analytic full-longitude panorama fixture covers seams, poles and
masks; cropped equirectangular calibrations are not treated as periodic.

`dense_field_compare_test` feeds saved native and reference fields through the
same reconstruction and fusion core. Across twenty-four photo, fisheye and
panorama comparisons, eighteen keep identical reference locations and six
differ by a few points; the largest common-point separation is 3.4e-5 of the
reference range. On a known-plane fixture, an exact-disparity oracle recovers
the plane to 1.4e-6 units RMS, but the learned fields on repeating texture do
not: native and reference often agree on wrong depths. Agreement and tight
reprojection therefore do not prove surface accuracy.

## Caching, publication and fusion

Prediction identities depend on each view's content and preprocessing and on
each directed pair, separately from reconstruction settings. Moving all poses
reuses every prediction; a hidden edit of one image (same size and timestamp)
recomputes only its six directions; a corrupted prediction recomputes only
itself. Input contents are hashed on every run and again before publication.

Clouds and manifests publish as generations under `dense/generations/`; the
current record changes only after both files are complete and verified. Tests
inject failures after each publication phase, and readers keep the preceding
generation. Recovery removes only abandoned directories with an ownership
marker. Native cleanup tests reject redirected roots, nested entries and
parents on Windows junctions.

Fusion compacts spatial cells in independent disk partitions with a one-cell
halo and deterministic boundary owners; normal, plane-distance and bounded
merge-chain checks stay active. Oversized cells, observation groups,
accepted-point arrays and sampling spill to disk; serial, parallel and
forced-spill runs produce byte-identical clouds.

A large mixed-camera capture (thousands of pinhole, fisheye and panorama
images) completed source matching cold and then warm. The warm run reused
every prediction with no model load or inference, and both runs produced the
same cloud SHA-256, with minimum support three. Its low density remains a
quality question. Small connected subsets of each camera type, compared
against reference fields, found photo and panorama subsets empty under
three-image support; the camera-model review below addresses that.

## Camera-model compatibility review

Compatibility with every camera model now comes before sub-pixel agreement,
which the matcher cannot deliver at its own grid resolution.

- **Tolerance unit.** The source reprojection limit is now in matcher grid
  cells, scaled per axis from each image's resolution (`View::grid_scale`). It
  was in original pixels. The default moves from 0.08 original pixels to one
  cell, the cycle check's unit. On Base (640 cells), 0.08 pixels was 1/160 of
  a cell on an 8192-pixel panorama and 1/40 on a 2048-pixel photo, consistent
  with the empty subsets above. Points from earlier runs are not comparable.
- **Neighbour grouping.** Source samples previously grouped a reference's
  neighbour candidates by a 1% depth band along the reference ray. A
  wide-angle lens has fewer matcher cells per radian, so its depths scatter by
  more than that band. A neighbour now joins when the seed point reprojects
  into its observation within two tolerances. Refinement still applies the
  tolerance and distinct-image support unchanged. Rectified mode is unchanged.
- **Pair affinity field of view.** Pair ranking computed every camera's half
  field of view as `atan(r)`. That is wrong for fisheye and equisolid focals,
  which are per radian. It capped a 190-degree fisheye near 130 degrees.
  Equidistant and equisolid lenses now use their own angular mapping.
- **Inverse projection.** `camhost::pixel_ray` used to reject a pixel when its
  Newton solve stalled above 1e-7 pixels. It now accepts a stall below 1e-3
  pixels. This is defensive: the new matrix test also passes with the old
  inverse.

`dense_geometry_test` adds a round trip at 149 inner pixels for every model and
distortion tier, plus FOV, SIMPLE_DIVISION, EUCM, Fisheye624 and skewed-fisheye
source lenses, all within 0.001 pixels. It also checks that one tolerance
accepts a quarter-cell mismatch and rejects a 3.75-cell mismatch on a mixed 8K
panorama and 2K photo pair.

## Dense step tooltips, memory bar and edits

- **Tooltips.** Reference fraction, samples per reference and sampling seed
  each have their own tooltip on label and field, as does the source
  reprojection label. The live-preview status lines explain the three point
  phases.
- **Memory bar.** The dense step runs as a child process, so it reports its own
  memory (`app/RunReport.h`, `dense/progress/memory.json`, twice a second).
  The GUI draws RAM and VRAM bars with a risk tag and a hover card of history
  against the planned peak and capacity (docs/notes/memory-forecast.md).
  `dense_memory_plan_test` covers the plan; `resource_forecast_test` the risk
  rule and a torn report.
- **Edits.** Saving a dense cloud from the point editor re-signs its manifest
  and keeps the original as `roma.original.ply` (`commit_edit` in
  `dense/Generation.h`). `dense_edit_test` covers both artifact layouts, a
  second edit, and repairing a cloud an older build saved unsigned.

## Surface-driven pairing

Automatic pairing gave each reference a fixed number of neighbours and chose
references by sparse-track coverage in capture order. Pair counts therefore
followed the number of frames: a 5,730-face dual-fisheye room walk planned
13,545 Balanced pairs against 1,530 at every 8th frame, and a Nerfstudio
dataset without tracks made each of its 31,292 faces a reference (83,935
pairs). The cause on video was track fragmentation: a sparse point is tracked
through about four consecutive frames, so tracks barely overlap between
references and almost every frame looks uncovered.

Pairing now plans from geometry (`dense/Visibility.cpp`, `dense/PairSelection.cpp`;
`docs/dense.md`, "Automatic pairing"). The sweeps behind the constants ran the
earlier and the new planner in one process on the same views
(`SS_DENSE_PLAN_ONLY` and a temporary sweep hook), measuring both plans with
the same projected visibility:

- Coverage counted in angle-weighted units (full from 10 degrees) never
  reached its target on distant cells, so the 2S cap set the pair count;
  counting a partner fully from 3 degrees and using the 10-degree weight only
  to order partners matched the earlier pairing's coverage at about its pair
  count.
- With tracks for visibility, references stayed at about 90% of the frames of
  a dense video. Projected visibility removed that.
- Thinning the points to a fixed count let the sample spacing, and so the
  surface measured, follow how much cloud SfM made, and cells sized from the
  nearest camera alone grow finer as cameras get denser. With cells of 5% of
  the nearest-camera distance, floored at the median such distance, the room
  walk measures 24,648 cells at every 8th frame and 27,029 at full density
  when both keep the full point cloud; dropping the points no longer seen by
  two kept frames, as a thinned model does, measures less.
- A view qualified as a reference while a tenth of its own cells was
  uncovered, which a face seeing a sliver of surface met with a handful of
  cells; a tenth of a typical view's cells cut the room walk's full-density
  pairs by a quarter.

Calibration against the earlier presets on the nine Mip-NeRF 360 scenes,
geometric means of new over earlier per scene: Fast (R 12, S 2) references
per cell ×1.07, partners ×0.97, pairs ×1.02; Balanced (12, 3) ×0.95, ×1.00,
×0.96; High (16, 4) ×0.94, ×1.02, ×0.98. Garden Balanced at quarter
resolution, RTX 3070 laptop, 7,000 steps scored on every eighth image:

| Pairing | Pairs | Points | Dense time | PSNR / SSIM |
|---|---:|---:|---:|---:|
| Earlier Balanced | 508 | 4.15M | 952 s (build running) | 25.76 / 0.840 |
| Reference coverage 12 (Balanced) | 343 | 3.09M | 384 s (158 pairs cached) | 25.79 / 0.842 |
| Reference coverage 8 | 253 | 2.30M | 307 s | 25.75 / 0.840 |

The masked 160-image object capture, Balanced, same protocol: the earlier
pairing's 191 pairs (101 references, 142k points, 253 s) trained to 36.45 /
0.993, the surface-driven 147 (68 references, 92k points, 170 s) to 36.53 /
0.993.

## Windows commit and device memory

Balanced on Mip-NeRF 360 garden failed with "bad allocation" twice on a 16 GB
laptop (RTX 3070, a page file that leaves about 6.5 GB of commit free), at
pairs 194 and 246 of 508, with 6.45 GB committed. Commit minus working set
tracked the card's memory to within a few MB (2,223 MB against 2,220 MB at one
sample): Windows charges Vulkan device memory to the commit limit. The feature
cache grew into the card's free memory, and the host budgets planned 3.4 times
an eighth of RAM. The host budget now also takes a fifth of available commit
and its shares sum to 1.4 times it; on Windows and Apple the feature cache
stops at the available commit less 1 GB and the host plan's remaining growth.
The same run completed with a 4.7 GB commit peak and 1.2 GB working set, the
cache answering 806 of 1,016 descriptor requests, and the cloud matching the
earlier one's point count (4,145,929).

## Checkpoint loading

`Session::load` read the official checkpoint in 7-8 s on one thread: each
tensor reopened the ZIP, and every element was decoded through a string
comparison of its storage type and a division per axis. One archive per
reader thread, the type resolved once and contiguous runs decoded in bulk,
with up to 16 workers packing FP16 in parallel, load it in 0.8 s (mixed and
FP32 alike). The device weights are the same bytes in the same chunks:
predictions dumped by `roma_profile_test` on a garden pair are identical
between the two loaders in both precisions.

## Open validation

- Full native/upstream cascade parity and real-scene quality acceptance remain
  open. No failing comparison was renamed as a pass.
- A public-scene CUDA smoke training completes; broader hardware, other
  platforms and full-preset validation on more devices remain open.
- The official download uses the existing consent UI with both model terms,
  pinned size and SHA-256 validation; the shared downloader test covers
  cancellation and checksum rejection, not a live network download.
- Large mixed-scene quality and performance of final fusion, preview overhead,
  uncontended mixed-versus-FP32 throughput, masked-area leakage and
  allocation-failure behavior on more devices remain open.
