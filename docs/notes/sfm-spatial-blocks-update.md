# GPS regional SfM contribution

Large GPS-equipped aerial captures can finish feature matching but exhaust host
memory or a bundle-adjustment index pool when the mapper assembles the entire
capture. This contribution extends spatial blocking through matching, regional
mapping, coordination and COLMAP export. Its production code is native C++17
and Slang, uses the existing Vulkan SfM backend, and adds no solver dependency.

## User controls

- The GUI enables automatic partitioning for at least 5,000 photographs with
  complete GPS. Smaller captures, missing/incomplete GPS and video input keep
  automatic partitioning off. Eligible captures can be enabled manually.
- The recommended core-photo limit derives from half the machine's physical
  RAM and frontend/descriptor estimates. Users can override it. This limit
  counts core photographs; overlap/support cameras can increase a region's
  input count, subject to separate memory limits.
- Settings round-trip through presets and participate in dataset reuse plans.
  Disabling spatial blocks selects the ordinary whole-capture mapper, while
  the general memory/index guards still apply.
- CLI options and the cache, mapping and coordination budgets are documented
  in [sfm-spatial-blocks.md](sfm-spatial-blocks.md).

## Pipeline changes

1. Generate horizontal GPS neighbour pairs globally, including links across
   block boundaries. Process descriptor batches through a bounded, pinned LRU
   cache without changing image or feature identities.
2. Stream indexed match metadata into overlapping regions. Spill large
   correspondence graphs and completed atom models to disk. Guard host BA
   allocations and 32-bit index capacity; subdivide work or use the supported
   CPU fallback when necessary.
3. Persist initial local geometry and aligned regions. Recover alignment with
   verified shared cameras/landmarks and an observable gauge; fail explicitly
   for an unobservable or disconnected component.
4. Coordinate shared landmarks, camera poses and intrinsics in bounded graph
   windows. Fix support cameras and shared points during local solves, preserve
   rig/prior constraints, and use seam-aware landmark sampling with a uniform
   alignment reference. Score the same observation set each round and retain
   the preceding accepted state if its robust validation cost increases.
5. Fuse global tracks using external sorting and paged union-find, reject
   conflicting observations, retriangulate against coordinated poses, and
   stream reciprocal COLMAP image/point records. Replace the previous sparse
   result only after export succeeds. No final full-capture BA is allocated.

Initial-region and coordination checkpoints are separate. Changes to the
coordination iteration, cost-tolerance or landmark limits can reuse completed
initial regions. Incomplete rounds reuse their completed region workers.
Extraction and matching need not be repeated to map an existing match database.

## Performance work

Regional coordination reuses a scoped Vulkan context while releasing each
problem's buffers. Validation reads observations in bounded chunks and scores
them in parallel with deterministic reduction. Private-point updates use bounded
parallel batches. Window sampling selects points before copying observations,
parallelizes preparation, and merges deterministically. Initial mapping avoids
two redundant model reads per freshly completed region. Logs distinguish graph
construction, BA preparation, solve and private-point update times.

Read-only measurements on an aerial region, taken on an RTX 4070 SUPER machine
while another reconstruction was running:

| Operation | Reference | Optimized | Local speedup |
| --- | ---: | ---: | ---: |
| Score 17,561,386 observations | 4.032 s | 0.858 s | 4.70x |
| Update 100,000 private points | 0.606 s | 0.085 s | 7.11x |
| Prepare a 328-camera window | 0.500 s, one thread | 0.247 s, parallel | 2.03x |
| Repeated F32 window solve | 0.672 s, fresh-context average | 0.310 s, reused context | 2.16x |

These are operation-level measurements against the preceding regional build,
not an end-to-end comparison against upstream. The factors cannot be multiplied.
The private-point coordinates were bitwise identical and window observations
were identical. Parallel validation was deterministic across thread counts,
with a less than 1e-10 relative difference from the old accumulation order.
Vulkan F32 solver costs varied by less than 0.001% in this measurement.

## Validation recorded before upstream integration

The Windows Vulkan development build completed its comment checks. Eighteen
native regression targets passed: fixed frames, fixed landmarks, regional
quality, regional sampling, shared landmarks, regional storage/export, host BA,
BA index capacity, mapping memory, CPU BA (`--quick`), spatial blocks, resume,
GUI partition policy, dataset plans, preset round-trip, rigs, priors and model
storage. Earlier memory-stage checks also covered correspondence caching,
bottom-up memory scheduling and telemetry parsing.

A real 288-photo trial registered all photographs, exported 206,908 points and
767,718 observations, and had independently recomputed mean/median reprojection
errors of 1.748/1.339 pixels. Reciprocal tracks were valid. Resume preserved
63 checkpoint metadata files, and the exported COLMAP binaries were identical
to the preceding regional build. This trial verifies the optimized coordination
and resume path; it is not a full large-capture speed benchmark.

The preceding regional build completed a separate 28,148-photo capture:
28,051 registered images, 30,371,708 points, 130,264,995 unique observations,
1.703/1.452 pixel mean/median reprojection error, and one connected region group.
Mapping, coordination and export took approximately 5 h 23 min. A subsequent
optimized run completed all eight coordination rounds but its GUI crashed in
a graphics-driver call during export. The integrated upstream build then
reused all 28,148 feature files, 565,367 matched pairs, 12 completed regions and
eight committed rounds. Recovery and final export completed in about 13 min
29 s (export itself about 10 min 21 s), yielding 28,051 registered images,
29,829,000 points, 128,008,411 observations, mean/median residuals of
1.726/1.464 pixels and one connected region group. This validates checkpoint
compatibility and export after integration; it is not a fresh full-capture
mapping/BA benchmark of the integrated code.
Private photographs, GPS data, machine paths and commercial-program analysis
are excluded from the contribution.

## Upstream integration verification

The contribution was integrated onto upstream
`97d994f5a0219be136584dd8d7592c6446182ff1`. Three-way integration retained
upstream's compact match lists, packed in-memory correspondence graph, match
spill support, Jacobian accessors/storage layout, BA host-table release and
new GUI/preset fields. Regional graph caching retains its leased per-image
disk rows; those rows use two-word entries. Memory probes use upstream's common
`core/HostMemory.h`. MSVC builds enable `/bigobj` for the header-heavy SfM target
and its consumers.

The Windows Vulkan GUI build completed through `build_develop.bat`, with
`SS_BUILD_SAM=OFF` (SIFT frontend). Comment-length, Windows-macro, font-coverage
and private-path checks passed. All 26 selected native regression targets
passed: the 18 development targets listed above, correspondence caching,
bottom-up memory, telemetry, upstream match/graph spill, merge, feature
compaction, live-match reading and ordinary mapping. The spill test additionally
checks bounded regional graph queries over upstream's spilled match lists.
The combined executable's `sfm map --help` also succeeded. This does not verify
a CUDA build, the optional learned frontends, or a fresh full-capture mapping/BA
run after integration. The recovery/export check above used existing region and
coordination checkpoints.

## Limits and review points

Partitioning currently requires GPS; a visual-graph-only partitioner is not
implemented. GPS proximity does not guarantee visual overlap, and RTK metadata
does not make the result a survey-grade control-point adjustment. Regional
coordination approximates full joint BA and is not claimed to be numerically
equivalent to it or to a commercial solver. The fixed validation objective
includes duplicate observations from overlapping regions; final fused-model
residuals are reported separately.

Memory allowances are estimates. Mapped graph pages, drivers and unrelated
processes can contribute additional RAM/VRAM pressure. Disk space and I/O are
part of the tradeoff. A single oversized local solve can still hit device
limits. The Gaussian training engine and its existing scene-partitioning
implementation are outside this contribution.

The cumulative implementation was developed against upstream commit
`f113417049a275ff180a067f2aa68bad805498a3`. Measurements above refer to that
development baseline with this contribution applied. The upstream integration
checks above establish build/regression compatibility separately from those
capture measurements.
