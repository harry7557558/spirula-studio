# GPS spatial blocks for aerial SfM

`--pairs spatial-blocks` matches aerial photographs without reading every
descriptor into RAM or running the all-pairs visual prefilter. It requires a
GPS position for every image, from the existing EXIF or telemetry reader.
Positions select candidates; the ordinary descriptor matcher and two-view
verification still decide whether a pair supplies reconstruction evidence.

```sh
spirula sfm auto images -o reconstruction \
  --pairs spatial-blocks --mapper bottom-up \
  --block-size 3000 --block-neighbours 40 --block-cache-mb 3000
```

For a separately extracted feature directory, give matching the original
photographs so it can read EXIF GPS:

```sh
spirula sfm match reconstruction/features -o reconstruction/matches.bin \
  --pairs spatial-blocks --image-dir images --block-size 3000 --block-cache-mb 3000
spirula sfm map reconstruction/matches.bin reconstruction/features \
  -o reconstruction/sparse --mapper bottom-up --images images \
  --pairs spatial-blocks --block-size 3000 --map-memory-mb 8192
```

In the built-in dataset panel, **Block SfM** and **Maximum core photos per block** are basic
settings available before reconstruction starts. Automatic selection enables
blocks only when there are at least 5,000 photographs and every photograph has
a valid GPS position. Below that count blocks stay off by default, but can be
enabled manually once GPS is complete. Missing or partial GPS, video inputs,
and pending metadata scans keep blocks disabled. Video inputs cannot be
overridden by the manual switch.

The checkbox records an explicit on/off choice. **Restore automatic selection**
returns to the count/GPS rule. A custom block size is preserved;
**Use recommendation** refreshes the memory snapshot and restores automatic
recommendations for both block size and descriptor cache. The advanced panel
still exposes neighbour count, radius, and cache budget. Enabling blocks selects
the existing bottom-up mapper. Dataset presets save these choices, and effective
matching settings participate in the dataset plan and stage signatures.

## GUI memory recommendation

`app/gui/SfmPartition.h` uses half of physical RAM to recommend the core photo
limit. Available RAM controls descriptor caching separately. `SfmRunner.h`
supplies the selected frontend's feature limit and
descriptor size. An explicit maximum feature count overrides the quality
preset; otherwise SIFT uses 2,048 features shifted by quality level (8,192 at
the default high quality). SIFT descriptor storage is 128 bytes per feature;
the learned frontends use their larger storage estimates.

The recommendation uses these byte estimates:

```text
B = total RAM / 2
per photo = feature count * (descriptor bytes per feature + 32) * 4
estimated photos = B / per photo
photos = floor(estimated photos / 500) * 500 when estimated photos >= 500
photos = floor(estimated photos) below 500; limited to 2..20,000 overall
cache budget = min(B / 2, available RAM / 4)
cache = min(photos * feature count * descriptor bytes per feature, cache budget)
cache MiB = floor(cache / 1,048,576), limited to 1..4,096
```

The factor reserves model/solver copies, observations and overlap beyond
descriptors and keypoints. Photo recommendations depend on total RAM rather
than temporary available-memory fluctuations. A manual core limit replaces
the recommendation and is preserved by presets and execution.
With 8,192 SIFT features, 32 GiB total / 16 GiB available recommends 3,000
photos and a 3,000 MiB cache; 64 GiB total / 32 GiB available recommends 6,500
photos and a 4,096 MiB cache. Unknown total RAM falls back to 2,000 photos and
512 MiB. If only available RAM is unknown, photos still use total RAM and cache
is capped at 512 MiB. These are planning estimates, not a whole-process RAM
guarantee. For small total RAM, estimates below 500 use individual photo
counts instead of raising the count to 500. The minimum of two photos and 1 MiB
is the CLI's accepted limit, not a promise that reconstruction can run at that
memory level; a single pair exceeding the cache still fails explicitly.
User-entered sizes and cache budgets are preserved until their recommendation
is restored.

This memory recommendation belongs to the GUI. CLI defaults are 2,000 photos
and 512 MiB; CLI users must pass the larger values explicitly, as in the
examples above. CLI `--pairs spatial-blocks` is an explicit request and does
not apply the GUI's 5,000-photo automatic threshold.

## Pair selection and boundaries

`feature/SpatialBlocks.h` builds a two-dimensional KD tree in GPS east/north
coordinates. Altitude is excluded: missing or noisy EXIF height must not move
an image out of its horizontal flight neighbourhood. Each image proposes its
nearest `--block-neighbours` others, optionally within `--block-radius` metres.
The union is canonicalized to unique unordered image pairs. It holds at most
N times K candidates, rather than N times (N-1)/2. With 30,000 photographs
and K=40 this is at most 1,200,000, compared with 449,985,000 exhaustive pairs.

The spatial tree's ordering is grouped into core blocks of at most
`--block-size` images. Matching processes candidates by the owning spatial
block. Neighbour searches cover the entire capture, so a block boundary does
not remove pairs: images across it form the block's halo and their descriptors
are read as needed. There is no hard geographic crop or independent camera
renumbering. Feature indices and image ids retain the sorted feature-file
order throughout extraction, verification, resumption, and mapping.

Spatial mapping assigns each photo to one core region and adds overlapping
photos selected by verified match strength. Each region runs the existing
bottom-up mapper, including its smaller atoms (`--bup-atom-size`). Region size
uses the selected core limit directly; there is no second density-based
reduction when creating the plan. Overlap photos are additional. Regions that
actually exceed guarded input/model/BA limits are split and retried; already
completed regions remain on disk. Resuming uses its saved plan, including any
previous splits, so completed regions need not be rebuilt for a policy update.

## Memory and controls

| Flag | CLI default | Meaning |
| --- | --- | --- |
| `--block-size` | 2000 | Maximum core-region size and host descriptor cache image count; mapping adds overlap |
| `--block-neighbours` | 40 | Neighbours proposed per image, including across boundaries |
| `--block-radius` | 0 | Maximum horizontal distance in metres; 0 leaves distance uncapped |
| `--block-cache-mb` | 512 | Host descriptor byte budget, in MiB |

Matching loads keypoints and metadata with `readFeatures(..., false)`.
`feature/DescriptorCache.h` reads only descriptor payloads, keeps an LRU cache,
and pins both images of every pair until its matcher call returns. Batches
split further when their unique image count or payload would exceed a bound.
Eviction frees capacity. A single pair larger than the budget fails and asks
for a larger cache instead of exceeding the configured bound. Cached entries
are released on both normal exit and exceptions. The log reports descriptor
peak bytes and image loads; this is **not total process RSS**.

Keypoints, colours, candidate pairs, verified correspondences, verifier work
queues, and model/BA buffers remain additional allocations. GPU descriptors
still use the existing matcher's separate VRAM policy. There is no fixed-RAM
guarantee for the whole reconstruction. Regional mapping limits the model and
BA windows sent to the solver, reducing their GPU working sets. Feature
compaction removes unused keypoints within each mapping region.

## Regional reconstruction and checkpoint resume

With `--pairs spatial-blocks`, both `auto` and `map` use the regional route.
The completed matches file is indexed, retaining camera calibration and pair
configurations. Each region reads only its selected features and internal
verified pairs, retaining the original global image and feature identities.
The complete matches database and a full-capture reconstruction are never
loaded together. Existing features and matches are read without modification.

Completed regional models are atomically saved under
`<feature-directory-parent>/.regional/<signature>/`. The signature includes
mapping settings and input metadata. Restarting with the same inputs/settings
skips completed regions and completed coordination passes. Changing inputs or
effective settings starts another checkpoint set. These checkpoints persist
after normal exit or failure; reserve disk space for them and external sorting
files. A pre-regional run has no such mapping checkpoints, so it must restart
mapping once, while retaining extraction and matching results.

The pipeline first aligns regions into a GPS-derived metric frame. Original
global `(image, feature)` identities join local points on disk to build a shared
landmark index. Connected block pairs require at least 50 noncollinear shared
points; distance trimming uses 40 times the upper median and retains at least
80 percent. Joint robust Sim3 optimization holds block zero as the reference.
Each coordination round fixes the shared coordinates during bounded local BA,
updates shared intrinsics by weighted consensus, retriangulates private tracks,
and triangulates shared tracks against the coordinated global camera poses.
BA cores grow through bounded co-visibility links, with spatial seed/tie order,
instead of consecutive image IDs. Disconnected components use separate windows.
Support images outside a core hold their frame poses during the solve, including
prior factors; rig members used by a held frame also remain fixed. Only the
core poses are committed. CPU and Vulkan dense/CG paths share these masks.
Windows select points across a 16 by 16 image grid, up to eight per cell;
shared points are retained and boundary points are fixed. This bounds BA work
without a full-capture BA or a full-capture in-memory model.

Shared landmark selection protects up to 128 points per primary model pair,
bounded by half of its total point/byte allowance, across spatial cells. Each
candidate belongs to its weakest overlap pair; pair allowances are equal even
when one seam is much denser. A deterministic global sample fills all remaining
point and byte capacity, excluding the protected points. Cells coarsen
when their metadata/quotas cannot fit the allowance. Sampling remains an
approximation: inadequate overlap still fails the 50-point alignment gate.
One quarter of the shared-index allowance holds a raw uniform alignment
reference, with observation conflicts checked by an external sort. When its
graph is connected, both uniform and protected samples fit block transforms.
The same uniform correspondences compare their robust residuals, with a Huber
knee at twice the uniform median. A worse protected transform falls back to
the uniform transform; protected landmarks remain available for later BA.
If only the protected sample connects the graph, it supplies the transform.

`--regional-iterations` defaults to eight, `--regional-cost-tolerance` to 0.0001,
and `--regional-landmarks` to 100000. The landmark count is a native memory cap,
not an inferred commercial default. Version-two coordination streams a fixed
validation observation file for each aligned initial model. Every round scores
the same observations using the committed global poses/intrinsics and updated
private/shared coordinates, in source pixels with Huber loss at `max-error`.
Changing observation counts or nonfinite residuals fails explicitly. A cost
increase, including in round one, retains the preceding accepted round. An
absolute change below the initial validation cost times the tolerance stops
refinement. Window solver costs are diagnostic, not the acceptance objective.
Duplicate observations across local models retain constant weights; this
validation objective differs from the final fused COLMAP objective, whose
retriangulated residuals are reported separately. Runtime worker failures
receive one retry after one second; cancellation and memory/index guard failures
are propagated. Round manifests commit models, shared landmarks, consensus and
cost state together; incomplete rounds reuse completed region workers. Changes
to these three coordination options keep initial region checkpoints reusable,
but create a separate versioned coordination directory.
The version-two policy also keeps initial mapping/local geometry reusable,
while retaining older coordination directories. Validation records need 32
bytes per original local observation plus a 24-byte header per model on disk;
scoring loads one local model and streams its observations. Windows checkpoint
replacement supports existing files, so retrying an incomplete stage can
overwrite its uncommitted model files without deleting completed regions.

The shared-landmark workflow follows the observable distributed coordinator
contracts of the comparison implementation. Its task selection, internal
triangulator, numerical solver, intrinsics policy and defaults are not fully
known. This implementation is independent and is not claimed to be numerically
equivalent to that product, full joint BA, or a distributed ADMM algorithm.

Final track fusion uses paged disk union-find and bounded external sorting.
Duplicate global `(image, feature)` observations connect regional points;
tracks with conflicting features in one image are rejected. Each unified point
is retriangulated and refined against coordinated global camera poses, then
filtered by reprojection error. COLMAP images and points are streamed with
reciprocal track references. The output reports unregistered images,
cross-region tracks, connected region groups and actual source-pixel residuals.
The old `sparse/0` is replaced only after the new export finishes.

The memory policy below also applies inside each region. With spatial blocks
disabled, mapping retains the ordinary whole-capture path and its global BA.

## Mapping working memory

Descriptor caching controls matching. Mapping now has a separate working
budget, configured for both `auto` and `map`:

| Flag | Default | Meaning |
| --- | --- | --- |
| `--map-memory-mb` | 0 | Mapping working budget in MiB; 0 derives it from total and available RAM |
| `--map-graph-cache-mb` | 0 | Additional cap on the heap correspondence-graph cache in MiB; 0 uses the automatic allocation |

The GUI advanced SfM settings expose the mapping working budget; the graph
cache override is a CLI control. Automatic mapping allocation starts at half
the physical RAM and is capped at three quarters of currently available RAM.
An explicit `--map-memory-mb` value is also capped by available RAM. If physical
RAM cannot be queried, the automatic starting value is 2,048 MiB. The resulting
working budget assigns at most one eighth, capped at 512 MiB, to graph caching;
half to host bundle-adjustment work; and one third to loaded completed models.
These are estimates with space reserved for transient copies. Allocation and
merge checks also account for current memory pressure. The log prints the
effective limits, which can be lower than the requested value.

The mapping changes address three separate sources of resident memory:

1. `map/CorrespondenceGraph.h` keeps the original in-memory CSR graph when it
   fits its cache. Larger graphs are written to an exclusive temporary
   directory and loaded per image through an LRU cache. Query views hold a lease
   so eviction cannot invalidate a correspondence being used. Oversized or
   concurrently pinned image rows can use read-only memory mapping without
   enlarging the heap cache. Those mapped pages are outside the heap-cache
   count, and can still contribute to process RSS.
2. `map/Mapper.h` creates the bookkeeping for unregistered images without
   eagerly duplicating every feature into full `points2D` and `point3D_ids`
   arrays. Registration and adoption materialize the arrays that a model needs.
   Known EXIF calibration can bootstrap cameras without first materializing a
   full-capture model and graph. Atom workers divide the configured host BA
   allowance among their actual worker count.
3. `map/Atoms.h` sends completed bottom-up atom models to `map/ModelStore.h`.
   RAM retains estimated resident bytes and registered-image ids. The temporary
   format preserves the complete reconstruction, including unregistered image
   records, camera pixel scale, EXIF orientation, rig calibration and detached
   members, feature indices, tracks, and the next point id. It does not use
   COLMAP interchange IO, which omits some of that runtime state.

When all completed models fit within a third of the working budget, bottom-up
reloads them and follows the existing joint-refinement and assembly schedule.
Otherwise it groups models by shared registered images within that resident
limit, applies the existing joint refinement, and runs one ordinary validated
merge level per group. The later groups reuse calibration from the first
solved group, following the existing batched-joint strategy. Rejected and
unmerged models are all retained. The resulting models go back to disk and
another level runs if necessary. These preliminary levels perform no regional
growth, reseeding, redundant-model removal, or finishing. Once the models fit,
the existing global joint solve, assembly, boundary growth, and finishing run.
Image, feature, and camera ids remain global; the verified match database and
its cross-block pairs are not cropped or discarded by grouping.

`core/ModelMemory.h` counts vector capacities and conservative map/set node
overhead. Model loading, merge copies, and host BA preparation are checked
before their large allocations. A single atom model over the resident limit,
a merge tree unable to shrink within the configured rounds, or a solve over
its host/device allowance produces an explicit failure rather than silently
dropping a component. The grouping changes optimization order and is not
promised to be numerically identical to one joint solve over every atom.

BA also has a separate 32-bit Jacobian-pool index limit, counted in scalar
elements rather than bytes. Packing checks the exact observation and camera
column counts before allocating observation arrays. Joint refinement catches
index-capacity failures and splits components into smaller batches; a single
model exceeding the limit remains an explicit failure. Increasing RAM or
selecting CPU/CG does not remove that index limit.

**The working budget is not a process RSS limit.** Source features, verified
matches, metadata, allocators, OS-mapped pages, GPU buffers and runtime staging
remain additional allocations. On the ordinary mapping path, the final
connected model and its BA still
require resident model and solver state: the final solve is not out of core.
A very large single model can exceed these limits and fail even after graph
caching and atom spill have reduced earlier peaks. Raising the budget needs
enough free RAM and does not remove a device-memory limit.

Ordinary mapper scratch graph and atom files are temporary and removed on normal exit or
exception cleanup; they are not persistent atom checkpoints. Existing
`features/` and `matches.bin` can be reused to retry mapping after freeing
memory or adjusting the budget, without extracting or matching again:

```sh
spirula sfm map reconstruction/matches.bin reconstruction/features \
  -o reconstruction/sparse-memory --mapper bottom-up --images images \
  --pairs spatial-blocks --block-size 3000 \
  --map-memory-mb 8192 --map-graph-cache-mb 128
```

The same flags can be passed to `auto`. Keep scratch/output storage on a disk
with enough free space for temporary graph rows and models.

GPS here does not imply an RTK-constrained survey adjustment or control-point
accuracy. Existing metric gauge and sensor prior settings retain their own
meaning. For oblique flights, repeated hover positions, multiple disconnected
surveys, or sparse coverage, inspect registration and the verified graph;
increase neighbour count or choose a suitable distance cap. Proximity alone
does not prove visual overlap. When CLI spatial blocks are explicitly requested,
matching requires complete finite GPS and does not silently select all pairs.
For mapping an already completed match database, unavailable GPS emits a
warning and uses the ordinary mapper. GUI selection leaves partitioning off
when its GPS eligibility check fails.

## Resumption and validation

The existing pair-list cache and verification journal are used unchanged.
Completed pairs are skipped by global image ids, and spatial ordering applies
only to the remaining work. Verified pairs are sorted canonically before the
database is returned, including resumed runs. Focal-calibration samples and
rig-mate matches use the same descriptor cache as ordinary matching.

`sfm_spatial_blocks_test` checks KD results against an exhaustive reference,
distance caps, deterministic ties, horizontal coordinates, boundary links,
the cache budget and eviction, descriptor equality for uint8 and float storage,
unchanged keypoint indices and colours, cache reuse, failure cleanup, oversized
pairs, truncated feature files, and candidate generation for a 30,000-image grid.
That grid checks planning scale; it does not measure real aerial reconstruction
accuracy or end-to-end memory. `preset_roundtrip_test` includes the new GUI
settings. Build with the existing development script and run those targets.

The GUI partition policy, preset roundtrip, dataset plan, and telemetry probe
tests pass for the updated controls. The policy test covers the automatic
threshold, incomplete/missing GPS, video inputs, manual overrides, RAM-based
recommendations, frontend estimates, and the low-memory bounds.

`sfm_model_store_test` checks complete intermediate-model round trips,
capacity-based estimates, bounded overlap groups, concurrent writers, invalid
files and exception cleanup. `sfm_correspondence_cache_test` compares RAM and
disk graph queries and checks eviction, view lifetimes and cache bounds.
`sfm_mapping_memory_test` checks memory-policy boundaries, EXIF camera
bootstrap without full model allocation, and sparse image preparation through
registration, continuation and adoption. These tests complement reconstruction
trials; they do not establish a maximum full-capture RSS.

The real aerial dataset contains 26,749 photographs with complete GPS. Its
earlier 512-photo / 40-neighbour plan produced 53 blocks, 557,498 candidate
pairs, and 103,022 cross-block pairs in one candidate component. A full run
with the earlier matching-only build completed extraction, matching and 1,188
atoms, producing 1,264 models. It then failed at the first joint BA with
`Jc pool exceeds 32-bit indexing`. This failure motivated the memory/index
guards and regional reconstruction path. A subsequent 28,148-photo capture
completed with the regional build; its results and the separate optimized-build
validation scope are recorded in `sfm-spatial-blocks-update.md`.
Earlier complete SIFT trials registered 128/128 and 288/288 photographs into
one model each, with mean reprojection errors of 1.617 and 1.687 pixels. These
trials used 32-photo blocks and a 16 MiB cache; they do not measure the new GUI
recommendation's end-to-end behaviour. The updated GUI has been checked with
three photographs without GPS (control disabled), the 128-photo GPS input
(automatic off, manual on), and the full 26,749-photo GPS input (automatic on,
3,000-photo recommendation on the test machine). Editing the 128-photo input's
block size to 4,200 preserves that value; restoring automatic selection turns
blocks off while preserving 4,200, and using the recommendation resets it to
3,000.

The updated GUI batch path also completed a fresh 128-photo trial with explicit
32-photo blocks and a 16 MiB cache. It registered 128/128 images into one model
with 95,729 points and 1.617 px mean reprojection error in 36.443 seconds. Its
match database SHA-256 equals the earlier 128-photo trials. This verifies manual
settings reach execution and a newly queued GPS scan does not fail batch launch;
it does not validate the automatic recommendation at full dataset scale.

The regional implementation is covered by `sfm_regional_test`: indexed camera
and pair metadata, core ownership, original feature identities, disk paging,
multi-pass external sorting, cross-region reciprocal tracks, retriangulation,
and checkpoint round trips. Real 288-photo trials additionally check unchanged
input files, byte-identical output on checkpoint resume, and reprojection
errors recomputed independently from the exported model. The new regional
pipeline's later 28,148-photo run completed mapping, coordination and export.
Sample results and a completed capture still do not establish general memory
or accuracy limits; see `sfm-spatial-blocks-update.md` for the measured scope.
# Alignment recovery

Local geometry is checkpointed before regional coordinate alignment, keyed by
the exact included image IDs. Completed aligned regions remain reusable.
If a component has neither a reliable shared-camera transform nor an observable
GPS gauge, its verified matches select additional cameras already registered in
completed regions. Only the unresolved region is rebuilt with this extra context;
core ownership stays fixed. Context grows by at most 64 cameras per retry and is
bounded by the regional input and mapping budgets. An unobservable component
still fails explicitly rather than accepting a rotation inferred from collinear
GPS positions.

Coordination starts from persisted aligned models. Its versioned directory
preserves the earlier two-pass checkpoints. `sfm_regional_landmarks_test` checks
cross-model identities, Sim3 recovery, reference gauge, disconnected graphs,
triangulation and checkpoint validation. `sfm_fixed_landmarks_test` checks exact
fixed point preservation, pose recovery and free point optimization through
both CPU and Vulkan CG solves, including a single-observation fixed point.
`sfm_fixed_frames_test` checks support/core writeback, rig-frame masks, prior
normal equations and CPU/Vulkan F64/F32 dense/CG solves. The regional quality
test rejects committed pose/intrinsic degradation and changed observation
counts, tests checkpoint replacement and truncated files, and checks that
window membership survives photo-ID permutation. The sampling test verifies
that a dense overlap does not consume the alignment support of a weaker seam.

## Coordination performance

Coordination reuses one scoped Vulkan context across BA windows, with individual
problem buffers released between solves. Validation streams bounded observation
chunks and reduces parallel costs deterministically. Private-point updates and
window observation sampling use bounded parallel batches. Fresh initial regions
avoid redundant model rereads. Timings, regression coverage and the distinction
between operation-level benchmarks and full-capture validation are documented
in `sfm-spatial-blocks-update.md`.
