# Dense point clouds with RoMa v2

The native `spirula dense` command reconstructs a colored point cloud from
registered images and existing camera poses. It keeps the cameras fixed and
uses RoMa v2's full correspondence grids, overlap scores, and precision
estimates. Dense reconstruction lives in `src/dense/`; it does not run camera
estimation or bundle adjustment in the SfM mapper.

RoMa runs through the existing Vulkan inference layer. The application has
no Python, PyTorch, or ONNX Runtime dependency. The checkpoint reader accepts
the official tensor-state ZIP without executing serialized Python objects.
Inference requires `SS_BUILD_SAM=ON`; an inference-disabled build still
recognizes the command and explains that the module is unavailable.

## Running the command

```text
spirula dense DATASET --checkpoint /path/to/romav2.0.1.pt --preset precise
spirula train --data DATASET
```

Training starts from the finished dense cloud whenever the dataset has one
and `seed_pointcloud` is empty; `--seed-pointcloud sparse` keeps the sparse
points. `config.json` records the cloud a run used.

Use the official [RoMa v2 checkpoint](https://github.com/Parskatt/RoMaV2/releases/download/v2.0.1/romav2.0.1.pt).
The expected SHA-256 is
`1557dec0d21b62366465f7ff4d5fdf228cc695d0582e196ad2b80e05230828b7`.
Weights are not bundled. A local checkpoint path is accepted; the default
`romav2.0.1` identifier requires the checkpoint to be present in
the existing model cache. The GUI can download and verify it after showing
the model terms. The CLI does not download weights automatically.
Review the [RoMa terms](https://github.com/Parskatt/RoMaV2/blob/main/LICENSE)
and [DINOv3 terms](https://github.com/facebookresearch/dinov3/blob/main/LICENSE)
before obtaining or using the weights.

The parser supports COLMAP, Nerfstudio, and Metashape. Sparse points are
optional. `--recon-dir`, `--metashape-xml`, `--metashape-psx`, and
`--metashape-component` select the same inputs as the existing dataset
parsers. Images are read through the shared color conversion.

Lenses are handled automatically, as the depth and normal step does: a lens a
single undistorted pinhole view can hold is undistorted into one view, and a
wider one (a fisheye, or a panorama) is split. The split for matching is a set
of upright cube faces about the lens axis -- front, right, left, up, down, and
back only for a lens that sees past 270 degrees -- each cropped to the part of
its cell the lens covers and dropped when that is under 5%. A 190-degree
fisheye becomes five views where the depth step's cross-fading ring uses up to
fifteen, and a panorama becomes six. Faces stay upright because the matcher
does not tolerate large in-plane rotation. Faces of one image are never
matched with each other, and support always counts original images, so they
cannot satisfy the support threshold on their own.

Original-image matching is available with `--matching-space source`. It
passes each source image through the existing color conversion and RoMa resize
path, without making rectified faces. Geometry uses the original calibrated
camera, including fisheye rays beyond the forward hemisphere and full
spherical panorama rays. Rectified matching remains the default, including
when loading settings written before this option existed.

```text
spirula dense DATASET --source-workflow true --preset base --checkpoint CHECKPOINT
```

The source-workflow setup selects source matching, 80% reference coverage,
three neighbors, 10,000 samples per reference, sampling seed zero, and a
one-cell source reprojection tolerance. These are editable workload and
quality settings. The setup preserves the selected preset, dimensions,
checkpoint, precision, masks, and distinct-image support requirement.
`--samples-per-reference 0` evaluates every eligible grid location.

Source reprojection error is projected through the original camera but
measured in matcher grid cells, scaled per axis from each image's resolution.
One cell is the matcher's own resolution, so a single value means the same for
a 2K photo, a fisheye and an 8K panorama in one dataset. The cycle check uses
the same unit. A neighbour joins a reference sample when the sample's point
reprojects into that neighbour within two tolerances; final refinement then
applies the tolerance itself. The separate rectified tolerance retains its
existing working-view pixel meaning. Switching matching spaces preserves both
sets of saved controls. Settings saved with the earlier 0.08 original-pixel
default keep that number, which now means 0.08 of a cell; use the
source-workflow setup to restore the default.

`spirula dense --help` lists every editable setting and its starting value.
Boolean flags take `true`/`false` or `1`/`0`. Set a preset before overriding
its individual settings. The three presets set matching size, direction,
neighbours, reference coverage and sampling stride together:

| Preset | Matching grid | Refinement grid | Both directions | Neighbors | Reference coverage | Stride |
|---|---:|---:|---|---:|---:|---:|
| Fast | 512 × 512 | Disabled | No | 4 | 2 | 2 |
| Balanced (default) | 640 × 640 | Disabled | No | 6 | 3 | 2 |
| High quality (`high`) | 800 × 800 | 1280 × 1280 | Yes | 8 | 0 (all) | 1 |

Measured on an RTX 5070 (12 GB), each cloud then seeding 7,000 training steps
scored on every eighth image. A 160-image 1920 × 1080 object capture with
masks:

| Preset | Pairs | References | Time | Points | Peak device memory | Training PSNR |
|---|---:|---:|---:|---:|---:|---:|
| Fast | 101 | 36 | 27 s | 54k | 2.2 GB | 36.25 |
| Balanced | 191 | 48 | 59 s | 142k | 3.3 GB | 36.49 |
| High quality | 560 | 160 | 704 s | 3.8M | 9.0 GB | 36.47–36.56 |
| (sparse SfM seed) | | | | 15k | | 35.78 |

The 185-image Mip-NeRF 360 garden at a quarter resolution, rich texture and no
masks:

| Preset | Time | Points | Training PSNR / SSIM |
|---|---:|---:|---:|
| Fast | 65 s | 1.6M | 25.58 / 0.836 |
| Balanced | 156 s | 4.1M | 25.71 / 0.841 |
| (sparse SfM seed) | | 139k | 25.60 / 0.818 |

Stride 1 at Balanced's grid made garden take 586 s for 23.6M points, almost
all of it CPU refinement and fusion, with about 60 GB of temporary tiles, for
25.75 / 0.842; on the object capture it scored 36.61. Training subsamples a
seed to its splat cap, so the extra points seldom pay for themselves.

Device memory beyond the weights and matcher scratch is the view feature cache,
which grows into whatever the card has free: with all but 5.5 GB of the card
held by another process, Balanced peaked at 3.3 GB and High quality at 4.0 GB.
The weights and scratch alone take 1.6 GB for Balanced and 3.5 GB for High
quality (2.5 and 4.4 GB on a GPU without cooperative-matrix support, which runs
FP32). Repeated training runs on the same seed differ by about 0.1 dB.

The older matcher-only presets `turbo`, `base` and `precise` still load from
saved settings and the command line; they set only the matching grids and
direction (`precise` is High quality's matcher). `fast` is now the full preset
above, whose matcher is the old `fast`.

Custom dimensions use multiples of 16 for the initial grid and multiples
of 4 for refinement. Disable refinement by setting both high dimensions
to zero. A memory estimate rejects resolutions that cannot fit the selected
GPU budget; it never silently lowers resolution.

```text
spirula dense --preset fast --write-config dense-settings.json
spirula dense DATASET --config dense-settings.json --checkpoint CHECKPOINT
spirula dense DATASET --config dense-settings.json --min-overlap 0.6
```

The settings file records resolved values. JSON uses underscore names;
command-line names use hyphens. Reading and writing a settings file does not
run reconstruction.

## Using the desktop app

The dataset screen offers **Enable dense reconstruction** right after
**Estimate depth and normals**, as one more checkbox rather than a section of
its own. It is off by default, including for older presets. Both built-in SfM
and COLMAP use the same dense child command over their finished cameras.

Once it is ticked, the screen shows only what a first run needs:

- **Quality**: Fast, Balanced (the default) or High quality. Each sets the
  matching size, refinement, directions, neighbors, reference coverage and
  stride together. It reads **Custom** while those settings match none of the
  three; choosing one again resets them.
- The model: **Dense matching model ready**, or **Get the dense matching
  model** with its download size, a progress bar with Stop while it
  downloads, and the error if the download failed. The same label appears on
  the fetch button beside **Create dataset** when the dense model is the
  missing one.
- **Preview dense cloud**, once a cloud exists.

**Advanced dense reconstruction** holds every other control as a compact field
with its label beside it: matching and refinement size, both directions,
precision; neighbors, reference coverage, images that must agree, stride;
minimum match confidence, reprojection limit, minimum triangulation angle,
outlier removal; splitting wide lenses into pinhole views, sparse-track face
selection, masks, and the matching space (the four original-image controls
appear only when original images are selected); the CPU image cache and the
checkpoint identifier or path; **Use for training** with the training **Mask
mode**; and performance logging. Matching sizes snap to the multiples the
matcher needs when their field is left, other numbers are clamped to their
valid range, and a combination that is still invalid is explained in red under
the section. The JSON editor is gone from the GUI; the complete configuration
remains available through `spirula dense --config` and `--write-config`.

These settings are saved in dataset presets and the dataset step record.
Settings can be changed until the dense step starts, when the runner takes a
fixed copy. Hover over any dense control to see its purpose, processing cost,
and effect on the point cloud used to initialize training; help remains
available when settings are disabled during a run. The live-preview status
lines explain provisional, filtered and final points. The training **Mask
mode** uses the trainer's existing explanation; dense mask filtering and
training mask supervision are separate choices.

Updating a dataset regenerates a missing or stale cloud. Dense filtering
changes leave camera reconstruction reusable. Rebuilding cameras, frames,
or applicable masks invalidates the cloud. To force matching again, use
**Dense cloud again** under **Re-do one step**: that one run ignores saved
pair predictions (`--rebuild true` on the command line).
The planner checks output size and input metadata to offer reuse promptly.
Before reusing a cloud, the background runner verifies input contents and
the cloud checksum. Edits that preserve sizes and timestamps therefore still
invalidate reuse. Processing hashes inputs freshly in every run and verifies
them again before publication. The fingerprint ledger avoids repeated hashing
within a run; earlier size/timestamp entries do not establish content validity.

While the step runs, the dataset screen's model view shows the complete growing
cloud. The child starts with `--progress-dir dense/progress` and appends every
accepted reference result to a buffered disk checkpoint. Every 1.5 seconds it
flushes that file and atomically publishes `model.bin` metadata: the registered
cameras, checkpoint filename, and committed point count. The GUI reads exactly
that immutable prefix; it never reads points still being written. This is
version 7 of the shared live-model protocol (`app/gui/SfmProgress.h`); existing
SfM and dense snapshots through version 6 remain readable. Camera records preserve the
parser's lens model, distortion tier, intrinsics and all eight distortion
coefficients, so fisheye and panorama wireframes use the shared camera math.
No cumulative cloud is held in the matcher, and
publication does not rewrite all previous points. Each point occupies 15 bytes
on disk: three float coordinates in the preview frame and three color bytes.
The shared preview reader converts each float coordinate to the dataset's
double coordinate representation before scene normalization and rendering.

The GUI loads checkpoints on a background thread. Its refresh interval follows
measured reading and OpenGL upload time, allowing roughly 5% duty for refreshes
with a minimum half-second poll interval. Neither the checkpoint nor the point
renderer has a fixed point-count cap. Drawing a large cloud can still lower
frame rate, and full-cloud display requires sufficient memory. Before loading,
the GUI estimates host working storage against 80% of currently available RAM;
OpenGL allocation errors also pause the live view. A preview error is visible
and does not stop processing or reduce reconstruction quality. Hiding and
showing the preview retries loading. **Show what the run is doing** switches
live loading and drawing off; hiding the dense preview releases its cloud and
OpenGL buffers. Recording the disk checkpoint continues independently.

### Performance stats

**Log performance stats** in the dense options records the computer and the
step once a second into a new folder under `dense/perf/`: CPU per core, RAM,
disk, GPU use per adapter, NVIDIA clocks and temperature, and the phase with
its progress. `python tools/perf/perf_report.py <that folder>` reports what
limited each phase. See `tools/perf/README.md`.

### Editing the cloud before training

The dense cloud can be cleaned up before it seeds training: cropped to the
subject, floaters deleted, stray background removed. Open it with **Preview
dense cloud** (or open its `roma.ply` from File), edit it with the point tools,
and choose **Save**, which writes back over the file it was opened from.

Training checks every dense cloud against the checksum its manifest recorded,
so that a damaged or half-written cloud is never used. Saving an edit from the
editor re-signs the manifest: it records the new cloud's checksum, size and
point count, and an `edited` entry with the time and the original's checksum.
The dataset is then parsed again, and training accepts the edited cloud. The
edit is written with double-precision coordinates, as the dense step writes
them.

The first edit keeps the untouched cloud as `roma.original.ply` beside the
edited one. Later edits keep that file, so it always holds what the dense step
produced. To go back, open `roma.original.ply` in the editor and choose **Save a
copy...** onto `roma.ply`, which signs it like any edit; or use **Dense cloud
again** (`--rebuild true`). Renaming files by hand fails the check. Running the
dense step again
publishes a new cloud and leaves the edited one in its own generation.

A cloud changed by any other program still fails the check. Open it in the
editor and choose **Save** to sign it as an edit. Builds before this change
saved edits without signing them; saving such a cloud again repairs it. If the
dense folder's own `dense/roma.ply` still holds the original, that copy is kept
as `roma.original.ply` before it is replaced.

### Time and memory strip

While the dense step runs, a strip above the log shows how long it has run and
the time left in its current phase (preparation, matching, refinement or
fusion). The estimate uses the rate over the last 90 seconds, so it follows
changes of pace such as cached pairs giving way to inference. Later phases are
not included until they start, and it reads `--:--` until a few items finish.

The same strip shows the step's memory, right-aligned
where the training screen keeps its VRAM bar: an out-of-memory risk tag, a RAM
bar and, once the matcher has loaded, a VRAM bar, each labelled in front with
its figures after it. Each bar is stacked from the left: this run solid, its
expected growth up to the **planned ceiling** faint, then other programs grey.
The planned ceiling is the most the run's own budgets allow, which is its
projected peak. The colour follows the risk for that resource. Hovering
the tag or either bar shows a card with the history of both, the planned
ceiling dashed, capacity in red, and the in-use, ceiling, other-program and
capacity figures.

The dense step runs as a separate `spirula dense` process, so the GUI cannot
measure it directly. The child writes `dense/progress/memory.json` twice a
second, and the GUI reads it. The host ceiling is the process size at start
plus the matching stage's budgets, all shares of **CPU image cache**:
about 2.4 times the cache in original-image mode and 3.4 times in rectified
mode. The device ceiling is RoMa's weights and planned scratch plus its feature
cache's remaining room. Risk is the ceiling over what the machine can supply:
low below 80%, medium below 95%, high above. If it is high before matching
starts, lower the CPU image cache, choose a smaller matching preset, or reduce
samples per reference. The run's budgets bound its own memory, but other
programs started later can still take what it planned on.

During matching, displayed points have completed their reference's
multi-view support, depth-consistency, and robust point-refinement checks.
They are labeled as filtered points awaiting fusion. Each reference is
processed after all of its planned neighbor contributions are available;
raw pairwise triangulations do not enter the normal live preview. Different
references may still describe the same surface. After successful fusion and
outlier filtering, a separate checkpoint holds every exported point. The final
publication switches the viewport to that file, so the point count can decrease
as unsupported points and duplicates disappear. Checkpoint files remain under
`dense/progress`; they are previews rather than resumable reconstruction state.
Verified pair predictions remain the resume mechanism. The progress bar counts
matched pairs against the planned total.

Automatic pairing requires `neighbors + 1 >= min_source_images`. For example,
one neighbor cannot meet three-image support, even when a source has several
virtual faces. Such settings are rejected before processing. Two neighbors are
the minimum for three-image support; sparse overlap and consistency can still
leave individual points unsupported.

Saved dataset settings and presets can still load this combination for editing.
Loading preserves their neighbor count, support requirement, masks and preset;
it does not start processing or change the saved file. Before restarting an
automatic run with three-image support, set **Neighbors** to at least two.
Choosing two-image support with one neighbor is also allowed when explicitly
requested. The GUI runner and CLI reject incompatible settings before dense
processing starts.

After completion, **Preview dense cloud** opens the shared point viewer.
The trainer seeds from the dense cloud by itself; turning off **Use dense
cloud for training** sets `seed_pointcloud` to `sparse` for the dataset this
run made (and for a batch dataset step's training). An explicit seed choice
retains precedence. Resume, an existing splat initialization, and forced
random initialization skip the automatic pick. The trainer's **Starting
points** choice switches between the dense cloud, the sparse points and
another PLY.

The official model download uses the existing consent and download UI,
with pinned size and SHA-256 validation before publication. RoMa's MIT terms
and the DINOv3 backbone terms are linked in that dialog. A local path avoids
the download step.

## Reconstruction controls

`--precision auto` selects mixed FP16/FP32 arithmetic when the selected Vulkan
device advertises the cooperative-matrix configuration supported by `nn/`;
otherwise it retains FP32. `--precision float32` pins the reference path.
`--precision mixed` uses packed FP16 matrix/convolution weights on any supported
device, with ordinary Vulkan kernels when cooperative matrices are unavailable.
Normalizations, biases, depthwise filters, activations, and accumulation stay
FP32; geometry stays double precision. Mixed results can differ from FP32.
The manifest reports resolved precision. Mixed pair predictions have a separate
cache identity; FP32 continues to reuse the existing cache. Feature eviction
does not change precision, resolution, or support thresholds.

Automatic pairing works on views: an ordinary photo is one view, and each
pinhole face split from a wide lens is a view of its own. Views split from the
same image are never paired with each other. A view's neighbours are the views
that share its sparse points, each shared point weighted by the angle the two
camera centres see it from: a full weight from 10 degrees up, falling with the
square of the angle below that, so near-duplicate frames with almost no
baseline rank behind partners that triangulate well. Views without enough
track partners fall back to the nearest camera centres whose views overlap.
Faces split from a wide lens are paired only through shared points while
`--sparse-face-pairs` is on (the default). Every step uses the track index and
a nearest-neighbour search over camera centres, so planning is close to linear
in the number of views and observations.

`--reference-coverage` thins densely captured scenes. Sparse points are
counted in small cells of space (2% of the median camera-to-point distance),
because SfM breaks one surface into many short tracks: on an every-frame
video the median track spans four frames. In capture order, a view becomes a
reference only while at least a tenth of its cells are seen by fewer than
that many earlier references; otherwise it is matched only as some
reference's neighbour. A view with fewer than 32 sparse points always
stays a reference, since it may be looking at untextured surface. A turntable
or an every-frame video then costs about as much as the scene needs rather
than as many frames as were shot, while a sparse capture keeps every view as
a reference. `0` makes every view a reference. `--neighbors` caps the pairs of
each reference; with every view a reference both ends of a pair are capped,
as before.

Sequential pairing respects camera folders. Exhaustive pairing streams all
unique pairs. Explicit pairing reads two registered image names per line from
`--pair-list`, with quoted names supported. Zero-baseline pairs are rejected.

Source automatic mode selects references by deterministic pose coverage,
normalizing translation by scene scale. It plans directed reference-to-neighbor
jobs and ranks neighbors through the existing overlap/affinity implementation.
`reference_fraction` controls this automatic reference selection; sequential,
explicit, and exhaustive source modes retain their requested pairs and process
both reference directions. Coverage selection does not allocate an image-by-image
distance matrix. A cached reverse field is reused only when it actually exists.

Sequential, exhaustive and explicit modes pick image pairs and then match
every geometrically overlapping pair of their faces; `--sparse-face-pairs`
drops face pairs that share no sparse point. Disable it to match every
overlapping face pair, including areas the sparse model does not represent.

The default density uses every matching-grid pixel (`--stride 1`). Filters
require overlap of at least 0.5, a triangulation angle of at least 1 degree,
reprojection error at most 2 working-view pixels, and relative depth
agreement within 1%. The initial support requirement is three original
images. Set `--min-source-images 2` deliberately for two-view data; the
pipeline never lowers support automatically.

Source-mode sampling is performed once per reference, after masks, valid rays,
confidence rejection, and configured cycle checks. Fifteen percent of the
requested samples provide spatial coverage; the remainder use deterministic
confidence-weighted selection. Low confidence is rejected rather than clamped
upward. Candidate points are grouped by reference location and depth agreement,
combined with inverse-error weights, refined, and checked again through every
supporting original camera. Panorama horizontal residuals wrap at the seam.
Source depth consistency uses positive distance along the observed reference
ray. The default distinct-image minimum remains three.

Bidirectional matching enables forward/reverse filtering with a default
one-grid-pixel tolerance. Disabling bidirectional matching makes the cycle
check inactive while retaining its saved preference. In a one-direction
rectified run, target observations are also assigned to target reference pixels so
that images appearing on either side of a pair can gain multi-view support.

Training masks, image alpha, and feature masks are applied by default.
Individual switches, mask polarity, threshold, and Euclidean boundary
offset are adjustable. Invalid lens rays and coordinates near invalid mask
samples are excluded. Feature masks normally live in `feature_masks/`.
`--use-masks false` bypasses all three optional mask sources; invalid lens
rays are still excluded. The GUI exposes this independently of training.

The dense panel also exposes the trainer's **Mask mode**: **Cut out
background** supervises excluded pixels toward zero opacity, **Ignore
distractors** removes their color/depth/normal supervision, and **Don't use
masks** disables sidecar and image-alpha masks. These use the existing
`load_masks` and `apply_loss_for_mask` training settings and saved training
presets. For CLI training, choose `--load-masks true
--apply-loss-for-mask true` for transparency or `--load-masks false` to
disable masks. An unset mask policy defaults to cut out for a `dense/roma.ply`
seed as well as alpha-only images; an explicit training policy wins. Old
training presets that saved an explicit false value retain Ignore distractors
until changed. RGB behind the mask is not used as a black background target.

Point refinement is robust and changes only 3D positions. Fusion sorts
points into spatial cells on disk, compacts compatible estimates within
each cell, and considers neighboring cells through disk-backed halos. Local grid
neighbors provide surface normals where depth is consistent; normal and
surface-distance checks preserve distinct surfaces. Where a normal is
unavailable, fusion uses a conservative distance check. Automatic cell size
uses a bounded, deterministic sample of accepted point spacing. An explicit
`--voxel-size` is expressed in dataset units. A point limit keeps the first
accepted points in deterministic spatial-owner order. Each boundary candidate
has one owner; partition workers produce candidates independently, then merges
are reconciled in a fixed order. A merged cluster's bounding-box diagonal must
remain within the voxel distance, preventing chains from joining a long strip
of otherwise nearby points. Incompatible points remain separate.

Optional statistical outlier filtering reuses the existing exact kd-tree.
Its centered query coordinates are floats; exported positions remain
doubles. This optional pass must fit the host cache budget and otherwise
fails with an actionable error. It does not silently disable filtering.
`cpu_workers` bounds fingerprinting, view planning, reference refinement,
and the outlier queries; zero chooses the hardware thread count.
`image_cache_bytes=0` uses the smaller of one eighth of physical RAM and
half of currently available RAM; a positive value is an explicit byte
budget. The decoder worker count follows the CPU thread count and that budget
divided by the largest source image's estimated working memory, with at least
one worker. Decoded source RGB and masks are shared across all of its faces;
source images and rectified views share one LRU budget. Calibration plans use
the same byte budget rather than a fixed camera-count cap. Mask projections
reuse the exact source-pixel indices computed during calibration planning.
During matching, workers prepare views ahead of the GPU, and a
separate thread caches each prediction and adds its observations while the
GPU runs the next pair. Independent references refine on CPU workers with
memory-derived concurrency and bounded queues. Completed reference results
enter the checkpoint in submission order. Large observation groups, sample
selection, and accepted-point arrays spill to disk. Fusion partitions and
concurrency follow the host budget and CPU count. Oversized cells spill rather
than failing at a fixed group limit. Boundary reconciliation uses a bounded
paged table, and external sorting opens two input runs at a time.

Within a run, RoMa also caches the resized RGB inputs and the two FP32 DINO
descriptor maps for each working view. Both reference and neighbor views can
be reused. The cache size follows the selected GPU's memory, other inference
allocations, and reserved scratch within `memory_budget_bytes` (or the
default 80% device budget). There is no fixed byte or view-count cap.
When the driver exposes `VK_EXT_memory_budget`, its changing budget and usage
also constrain the cache. The device's allocation-count limit is respected.
Least recently used views are evicted. If a view
cannot fit, inference computes it normally; resolution, precision, pairing,
and filtering stay as configured. Pair-dependent matching and VGG refinement
still run for every uncached pair. Features are released at the end of the
run and are separate from the persistent pair-prediction cache.

## Outputs, caching, and cancellation

Successful runs publish a cloud and manifest under `dense/generations/`, then
atomically replace `dense/current.json`. Only a signed edit from the editor
changes a published cloud afterwards (see "Editing the cloud before training"). First-party
preview, planning, and training consumers resolve that record and verify the
generation manifest and cloud checksums. Training freezes the selected generation
for the session. Compatibility copies remain at `dense/roma.ply` and
`dense/manifest.json`; datasets without a generation record still use those
legacy paths. The cloud
is binary little endian, with double coordinates and 8-bit sRGB colors.
The parser's centering is restored before export, including Nerfstudio's
forward `applied_transform`, so the result is in the source file frame
expected by `seed_pointcloud`.

The manifest records settings, checkpoint and input fingerprints, cloud
checksum, image/view/pair counts, rejection statistics, cell size, and
elapsed time split into `prepare_seconds`, `match_seconds`, `finish_seconds`
and final `verification_seconds`. The manifest's total covers processing
through input/cloud verification; publication and process shutdown can add
to externally measured wall time.
`model_load_seconds` and `inference_seconds` separate checkpoint loading and
native matching within the matching phase. `reference_work_seconds` sums elapsed
reference-worker time, which overlaps matching rather than adding to the total.
`refinement_workers` records its effective concurrency. `fusion_workers`,
`fusion_partitions` and `fusion_boundary_candidates` describe final processing.
Source references,
directed jobs, unique image pairs, inference calls, and prediction-cache hits
are recorded separately. `filtered_reference_quality` records distinct-source
support, maximum and mean point-maximum reprojection residuals, and a sixteen-bin
histogram normalized by the resolved tolerance. Its pixel frame is original
image pixels in source mode and working-view pixels in rectified mode. These
statistics describe filtered reference points before fusion, not reprojected
final fused points. `peak_vulkan_buffer_bytes` includes tracked device and
host-visible Vulkan allocations; it is not physical VRAM residency.
`peak_matcher_scratch_bytes` records the session scratch peak. Both are zero
when no model is loaded. `feature_cache` records view hits,
misses, evictions, final and peak bytes, and the effective byte limit. Hits
count image reuse, not pair-cache reuse. Inference time includes resizing,
GPU computation and prediction downloads, but excludes waiting for image
decoding and reconstruction. Both generation files are complete before the
current record changes, so interrupted publication preserves the preceding
generation for readers. Compatibility copies are separate replacements;
external consumers requiring a consistent pair should also resolve the
current-generation record. The GUI child runner verifies the selected cloud
checksum before reporting success. An exclusive workspace writer lock prevents
two dense processes from publishing to the same dataset concurrently.

Each run keeps its temporary observation tiles and sorting files in a
`dense/cache/<settings>/run-<number>` folder and removes it when it finishes.
Cancelling from the GUI ends the child process immediately, so that removal
cannot run, and a crash has the same effect. Each run therefore starts, while
holding the writer lock, by deleting every leftover `run-<number>` folder and
any settings folder left empty. Prediction caches, `pairs-v4`, the fingerprint
ledger and anything not named that way are never touched.

Complete pair predictions are stored under `dense/cache/pairs-v4/`, with dimensions,
format revision, and SHA-256 sidecars. Resume verifies the complete cache
file before reading it. Corrupt or interrupted pairs are recomputed.
Filtering, density, support, and fusion changes reuse these matches.
Each view has a content/preprocessing identity and each directed pair has its
own prediction identity. Editing one source image invalidates its affected
pairs; source-camera pose and calibration changes can reconstruct from unchanged
source predictions. Rectified calibration changes invalidate the warped pixels.
Checkpoint bytes, resolved precision, dimensions, direction, and inference
revision participate in prediction identity. Masks, support, sampling, and
fusion affect reconstruction separately. Legacy rectified predictions are
promoted only from a verified compatible namespace; they are never reused as
source-image predictions.

Observations are buffered within the host budget and appended to reference-view
tiles. Reference sorting uses stable external merging; RGB prefetch follows
decoder concurrency and estimated image bytes. Source mode retains one decoded
RGB/mask allocation per cached image instead of copying it into a second view.
Fusion cells, boundary candidates and reconciliation state also have disk-backed
paths; available memory controls their working sets rather than the cloud size.
`--rebuild true` ignores cached predictions. Predictions are written while
the run goes, so a cancelled or failed run resumes from them, and are removed
after a successful publication unless `--keep-cache true`. They are large:
about 10 MB a pair at 640, 80 MB at 1280 in both directions, so a few hundred
High quality pairs fill tens of gigabytes. A run stops writing new ones while
the disk has less than 4 GB or 5% free. Settings saved before this default
changed load with it off.
Completed and failed temporary observation/sort directories are removed after
their workers and streams have closed. Cleanup validates the application-owned
run path and rejects redirected paths; image or dataset junctions are not cleanup
targets. Verified predictions and the last successful generation remain usable.
Failed publication removes its unpublished generation. Recovery removes owned
pending generations after acquiring the writer lock, while retaining published
generations and unmarked directories. Published generations remain available
for sessions that selected them explicitly.

Checkpoint resume and explicit initialization keep precedence over the
automatic dense seed.

## Implementation and validation status

See [the implementation log](notes/roma-dense-validation.md) for the native
port history, measured memory and timing, stage comparisons, known numerical
discrepancies, and workflow tests. Image-feature caching reduces repeated
work without changing FP32 arithmetic. Capability-selected mixed inference is
implemented, with separate validation from the native FP32 development path.
The native reference
tests are development tools outside application and build paths.

The CLI, reconstruction core, optional GUI step, presets/planner, download
dialog, and preview/training handoff are implemented. A posed synthetic
dataset completed matching, reconstruction, cache reuse, and a two-step
training run. Analytic tests cover masks, geometry, source transforms,
streaming export, outlier filtering, point limits, and cancellation.

Full native/upstream end-to-end numerical parity, real-scene quality acceptance
and broader hardware/platform validation remain open. Both CUDA and Vulkan
inference-enabled GUI builds passed the portable dense checks and full-cloud
preview regressions; later changes were built and tested on Vulkan only.

Original-image matching originally measured reprojection in original pixels
with a 0.08-pixel limit, far finer than the matcher's grid. Under that limit,
controlled masked three-image photo and panorama subsets produced no points.
The limit is now one matcher cell and neighbours are grouped by reprojection,
so every camera model shares one meaningful tolerance. A user's mixed-camera
original-image run on the Fast preset then showed about 780,000 filtered points
at under a third of its pairs. That is an in-progress observation, not quality
acceptance. Known-plane repeating patterns expose inaccurate depth in both the
native and reference implementations, and the measured Precise cascade has rare
large correspondence differences even though isolated stages compare closely.
Treat the current implementation as usable, with the remaining validation
limits documented in the log. For the code structure, the geometry and the
memory model, see [`src/dense/README.md`](../src/dense/README.md).
