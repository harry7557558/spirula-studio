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
and [DINOv3 terms](https://github.com/facebookresearch/dinov3/blob/main/LICENSE.md)
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
neighbours, reference coverage and sampling stride together. Neighbours and
reference coverage are per surface, not per image (see
[automatic pairing](#automatic-pairing)):

| Preset | Matching grid | Refinement grid | Both directions | Neighbors | Reference coverage | Stride |
|---|---:|---:|---|---:|---:|---:|
| Fast | 512 × 512 | Disabled | No | 2 | 12 | 2 |
| Balanced (default) | 640 × 640 | Disabled | No | 3 | 12 | 2 |
| High quality (`high`) | 800 × 800 | 1280 × 1280 | Yes | 4 | 16 | 1 |

Settings saved with a preset's earlier values (4 and 2, 6 and 3, 8 and 0)
load as that preset, and a dataset whose cloud was made with them does not
count as stale.

Measured on an RTX 5070 (12 GB) with the earlier pairing, a fixed number of
neighbours per image; each cloud then seeded 7,000 training steps scored on
every eighth image. A 160-image 1920 × 1080 object capture with masks:

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
all of it CPU refinement and fusion, for 25.75 / 0.842; on the object capture
it scored 36.61. Training subsamples a seed to its splat cap, so the extra
points seldom pay for themselves.

Device memory beyond the weights and matcher scratch is the view feature cache,
which grows into whatever the card has free (on Windows, whatever the commit
limit allows, see below): with all but 5.5 GB of the card
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
about 1.16 times the cache in original-image mode and 1.41 times in rectified
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
with pinned size and SHA-256 validation before publication. That dialog shows
the DINOv3 backbone terms and RoMa's MIT terms in full (licence family `roma`,
`LICENSES/`). A local path avoids the download step.

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

### Automatic pairing

Automatic pairing works on views: an ordinary photo is one view, and each
pinhole face split from a wide lens is a view of its own. Views split from the
same image are never paired with each other.

Pairs follow the surface the cameras see, not the number of images. The
sparse points (the COLMAP model's, a Nerfstudio dataset's point cloud or a
Metashape project's; without any, points along each view's rays at the depth
where its optical axis meets its neighbours') are sampled one per small cell
of space, 5% of the distance to the nearest camera but never less than 5% of
the median such distance, at most 32,768 samples. Each sample is projected
into every view, and counts as hidden there when it lies more than 15% behind
the nearest sample in the 3 × 3 cells around it of a 24 × 24 grid over the
frame. SfM tracks do not decide what a view sees: on an every-frame video a
sparse point is tracked through about four frames, far fewer than the views
that see its surface, and pairing by tracks gave every frame of a dense
capture references of its own.

Visible samples are grouped into cells 4% of their distance from the cameras
that see them. `--reference-coverage` (R): references are added one at a time,
the view that sees the most cells still short of R references first, until no
view would add a tenth of a typical view's cells. `--neighbors` (S): each
reference then takes partners until every cell of its view is seen by S of
them. A partner counts fully for a cell it sees from 3 degrees or more apart,
and by the angle squared below that, so a distant cell that no pair sees
widely does not draw partners without end. Among partners that cover as much,
wider angles go first (full weight from 10 degrees, as in Goesele et al.
2007). A reference takes at most 2S partners, and at least two whatever they
add, which three-image support needs. A pair between two references counts
for both. `0` reference coverage makes every view a reference. Faces split
from a wide lens are paired only through samples they share while
`--sparse-face-pairs` is on (the default).

The presets' values reproduce, on the nine Mip-NeRF 360 scenes at quarter
resolution, how often the earlier pairing saw each surface: reconstructions
per cell and partners per reconstructing view are within 7% of it on average,
for about as many pairs (Fast ×1.02, Balanced ×0.96, High ×0.98, geometric
mean over the scenes). Denser captures then stop adding pairs once every
surface has its references, and sparse ones keep every view. Pairs planned,
earlier pairing against this one, Fast / Balanced / High:

| Capture | Views | Earlier | Surface-driven |
|---|---:|---:|---:|
| Mip-NeRF 360 garden | 185 | 297 / 508 / 678 | 227 / 343 / 577 |
| garden, every 2nd image | 93 | 176 / 265 / 355 | 178 / 268 / 366 |
| garden, every 4th image | 47 | 86 / 131 / 179 | 93 / 138 / 182 |
| Dual-fisheye room walk | 5,730 | 9,433 / 13,545 / 16,738 | 3,462 / 5,406 / 9,420 |
| the same, every 2nd frame | 2,865 | 4,807 / 6,696 / 8,341 | 3,060 / 4,687 / 7,607 |
| the same, every 4th frame | 1,435 | 2,371 / 3,234 / 4,136 | 2,369 / 3,547 / 5,070 |
| the same, every 8th frame | 720 | 1,166 / 1,530 / 2,058 | 1,360 / 2,028 / 2,705 |
| Dual-fisheye outdoor walk | 5,970 | 6,914 / 10,386 / 13,563 | 3,038 / 4,652 / 7,596 |
| the same, every 8th frame | 750 | 976 / 1,381 / 1,767 | 1,180 / 1,751 / 2,411 |
| 707 photos, three cameras | 707 | 933 / 1,680 / 2,378 | 689 / 1,043 / 1,700 |
| Dual-fisheye building walk, no tracks | 31,292 | 53,752 / 83,935 / 113,788 | 7,073 / 11,351 / 20,168 |

Views are faces: a fisheye frame is five. The thinned captures keep the full
capture's point cloud, so only the camera density changes. Where the earlier
pairing planned fewer pairs on a thinned capture it also left more of the
surface unreconstructed: on the room walk at every 8th frame, 59% of the
cells three or more views see had a view with two partners seeing them,
against 76%. The building walk is a Nerfstudio dataset with a point cloud but
no tracks, so the earlier pairing made every face a reference with pose
neighbours; at the 0.9 s a pair garden matched in, that is 21 hours against 3.

The Balanced clouds train to the same quality, 7,000 steps scored on every
eighth image (RTX 3070 laptop). Garden: 25.76 / 0.840 from the earlier
pairing's 508 pairs, 25.79 / 0.842 from this one's 343, and 25.75 / 0.840 from
253 pairs at reference coverage 8. The 160-image object capture above: 36.45 /
0.993 from 191 pairs in 253 s, 36.53 / 0.993 from 147 pairs in 170 s.

Planning is close to linear in views and samples: the building walk's 31,292
views, hashing its 5,908 images included, planned in 21 to 25 s with 775 MB
committed at the peak.

Sequential pairing respects camera folders. Exhaustive pairing streams all
unique pairs. Explicit pairing reads two registered image names per line from
`--pair-list`, with quoted names supported. Zero-baseline pairs are rejected.

Source automatic mode selects references by deterministic pose coverage,
normalizing translation by scene scale. It plans directed reference-to-neighbor
jobs and ranks neighbors through the existing overlap/affinity implementation;
`--neighbors` there is the number of neighbours of each reference, and
reference coverage does not apply.
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
pipeline never lowers support by default. With three, the opt-in
`--two-image-points auto` admits a two-image point only under the conditions
in [Point filters](#point-filters).

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
disable masks. An unset mask policy defaults to cut out only for alpha-only
images, whatever the seed; an explicit training policy wins. Old
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
`image_cache_bytes=0` uses the smallest of one eighth of physical RAM, a
third of currently available RAM and a fifth of the available commit (what the
operating system will still promise; on Windows a small page file puts it well
below free RAM, and device memory counts against it too); a positive value is
an explicit byte budget. The decoder worker count follows the CPU thread count and that budget
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
Windows charges device memory to the system commit limit as it does host
memory, and Apple's device memory is the same RAM, so there the cache also
leaves the available commit 1 GB plus the host plan's remaining growth. On a
16 GB laptop with an RTX 3070 and a small page file, Balanced on Mip-NeRF 360
garden failed with "bad allocation" at pair 194 of 508 (6.45 GB committed, of
which 4 GB was the card's memory); it now completes with a 4.7 GB peak and
the cache still answering 806 of 1,016 descriptor requests.
Least recently used views are evicted. If a view
cannot fit, inference computes it normally; resolution, precision, pairing,
and filtering stay as configured. Pair-dependent matching and VGG refinement
still run for every uncached pair. Features are released at the end of the
run and are separate from the persistent pair-prediction cache.

### Point filters

Five filters act on refined points. Each is a setting, each is counted in the
manifest's `filters` block, and the command prints one summary line.

- `--two-image-points auto|off` (default `off`; `auto` acts only with three-image support).
  A point seen by exactly two original images is kept when its reprojection
  error is no worse than the median error of the run's three-or-more-image
  points. With `--free-space-test true` (default) it must also not be seen
  through: per view, a grid holds the nearest distance at which a three-image
  point was seen there, 512 cells per turn on a panorama or a split image and
  512 across an ordinary photo. A two-image point more than 5 %
  in front of the nearest of the 3 x 3 cells around it, in a view of an image
  outside its own pair, is dropped. A cell with no observation is no evidence.
  Two-image points also carry the depth-precision bar below: the setting's
  when it is on, else the automatic one.
  The bar is an exact median and the grid is sized by the views, so the host
  budget cannot change the cloud. It is opt-in because it is slow: samples that
  three-image grouping would drop at once go through full refinement instead.
- `--max-depth-error-per-cell` (default -1, off for points with three or more
  images; 0 is automatic). A point
  is dropped when its best pair moves its depth by more than this share per
  matcher cell: `1 / (f sin(parallax))`, `f` the coarser view's focal length in
  matcher cells. Automatic is `max(2 %, 1 / (f sin(theta / 2)))`, with `theta`
  the median angle the planned image pairs subtend at the sparse points both
  see, and `f` the median view focal. Without shared sparse points automatic
  is off.
- `--far-isolated true|false` (default true). After fusion and before the point
  limit, a point more than a margin outside the sparse points' p0.5 to p99.5
  box (max-norm) with at most two other points within a radius is dropped. The
  margin is 2 on a model whose `gauge.txt` says metric, else 0.2 x the box
  diagonal. The radius is 8 x the sparse points' median nearest-neighbour
  spacing. Under 100 sparse points it does not run, nor when the radius reaches
  the margin (a metric model whose sparse points are 0.25 or more apart): the
  neighbour search would then count points inside the box and hold the whole
  cloud in memory.
- `--max-baseline` (default -1, off; 0 is automatic). In automatic
  pairing on a metric model, a neighbour farther than 3 x the median planned
  baseline is refused and the next-ranked image takes its place. A positive
  value applies as given, on any model. It is unmeasured: the arms below use
  explicit pairs, which it does not touch.
- `--reprojection-check true|false` (default false) removes nothing. The written
  PLY is read back, the output transform undone, and each point projected into
  the views whose observations produced it. A fused point carries the id of its
  best-supported member, so the figure includes fusion's displacement. The
  manifest records p50 and p95 in view pixels and p95 in matcher cells, from a
  fixed histogram of 0.01 bins up to 100, so the check's memory does not grow
  with the cloud. With the check off, and the two-image rule off, no
  observation files are written.

Measured on a basement capture: the 24 equirectangular panoramas that see a
staircase most, 69 explicit image pairs, the `balanced` preset, masks
inverted. Every arm uses one binary and one set of cached predictions, so only
the filters differ. Violations are the share of stairs points within 1 cm of
5,000 sampled camera-to-sparse-point segments; sd is over 3 segment draws.
Adding 1 % synthetic floaters to arm A reads 1.285 %, so the measure sees
floaters.

| Arm | Filters | Points | Stairs points | Violations | Seconds |
|---|---|---|---|---|---|
| A | none | 1,943,024 | 144,795 | 0.441 % (sd 0.025) | 197, 184 |
| B | all | 4,009,116 | 332,670 | 0.402 % (sd 0.022) | 1,027, 1,025 |
| C | two-image and free space | 4,416,243 | 373,304 | 0.483 % (sd 0.020) | 1,236 |
| D | two-image, no free space | 4,578,979 | 396,753 | 0.511 % (sd 0.023) | 1,252 |
| E | depth precision | 1,915,587 | 143,408 | 0.449 % (sd 0.029) | 183 |
| F | far isolated | 1,943,023 | 144,795 | 0.441 % | 187 |

These arms ran with the reprojection check on, which adds about 31 s (181.5 s
against 150.5 s on one binary). E removes 27,437 points and a random removal
of as many reads 0.444 %, so on points with three images the bar shows no
benefit and is off by default. On two-image points it does: B against C, it
lowers violations from 0.483 % to 0.402 %, which is why two-image points carry
it. F removes one point, outside the stairs. Two-image points cost about
6.5 times the run, because samples that three-image grouping drops at once go
through full refinement; they are opt-in. B is
`--two-image-points auto --max-depth-error-per-cell 0`; `--two-image-points
auto` alone differs from it by keeping the three-image points the bar removes
(E's 27,437), and is unmeasured.

With every filter off the cloud is byte-identical to the pipeline without
them, and with the defaults it differs only by the far isolated points.

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
`dense/cache/<settings>/run-<number>` folder and removes it when it finishes;
a reference's tiles go as soon as it is refined, so the folder holds the
references still in progress rather than every pair matched so far. The cache
folder is opened in Windows' extended-length form, so its nested names work
past the 260-character limit whether or not the machine enables long paths
(issue 178).
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
