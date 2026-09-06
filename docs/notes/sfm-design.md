# SfM design notes

The decision log the SfM module was built against. Each `D<n>` is referenced
from the code by number; the code comment carries the reasoning, this table
carries the index.

**Distillation is unfinished.** The full text — context, alternatives measured,
consequences — is being condensed from the source repository's ADR log into
per-decision sections here (`docs/notes/sfm-port-plan.md` phase 7). Until that
lands, a `D<n>` in a comment resolves to the row below and to the surrounding
comment, which is where the load-bearing part of each decision was written down.

Priority for the distillation, because these are the ones that would otherwise
be re-derived or re-attempted: D25, D26, D27, D11, D16, D47, D50, D45, D46.

| # | Decision |
|---|---|
| D1 | Feature frontend: hand-written GPU SIFT first, behind a pluggable interface |
| D2 | Mapper: incremental (COLMAP-style) primary |
| D3 | Scope: splatting-grade first, COLMAP parity as a stretch goal |
| D4 | Interchange via COLMAP's on-disk formats |
| D5 | Image decode: one small vendored decoder |
| D6 | Build system: adopt CMake now |
| D7 | GPU SIFT: VLFeat algorithm, storage-buffer scale space, host top-K |
| D8 | Matching: GPU brute-force MVP behind a pluggable interface |
| D9 | Geometry: own small linalg, F/H uncalibrated verification, E-from-F pose |
| D10 | Incremental mapper MVP: P3P registration, GPU global BA, COLMAP output |
| D11 | Slang constant tables use brace initializers, not helper calls |
| D12 | Geometric verification runs on a worker pool fed by the GPU matcher |
| D13 | Batch image decode: budget-derived pool, strict in-order delivery |
| D14 | `sfm auto`: one command from images to a COLMAP model |
| D15 | Registration retries: ranked candidates, not a one-strike blacklist |
| D16 | Canonical feature order: the pipeline has to be reproducible |
| D17 | Multiple cameras: per-image intrinsics, mixed resolutions, sub-folders |
| D18 | Focal-length search when a camera group registers its first image |
| D19 | Seed retry: discard a model that is too small and start over |
| D20 | Internet photos get per-image intrinsics, not per-resolution |
| D21 | Matcher distances via the hardware packed uint8x4 dot product |
| D22 | Resident descriptors and batched submissions, not per-pair uploads |
| D23 | One distance matrix per pair, reduced along both axes |
| D24 | Jacobi rotations without transcendentals, and a relative convergence test |
| D25 | svd3's rank test has to be relative, not absolute |
| D26 | SPRT rejected: model *scoring* is ~4% of RANSAC, not the cost |
| D27 | Minimal-sample null spaces by Householder QR, not an eigensolver |
| D28 | Point colors sampled at extraction, stored per keypoint (features.bin v2) |
| D29 | Camera-model expansion: OpenCV distortion + fisheye (incl. >180°), on bearings |
| D30 | Camera models centralized; SIMPLE_PINHOLE/PINHOLE added; Schur kernels dof-tiered |
| D31 | Geometry core on unit bearings (D29 phase B) |
| D32 | Kannala-Brandt fisheye model (COLMAP OPENCV_FISHEYE); per-real atan by Newton |
| D33 | Wide-FOV cheirality + physical fisheye focal init (D29 phase D) |
| D34 | THIN_PRISM_FISHEYE + FULL_OPENCV models |
| D35 | GPU pair selection instead of exhaustive pairing (or a vocab tree) |
| D36 | Mapper robustness: refined+gated registration, iterated refinement with retriangulation, and undo |
| D37 | Mapper time: incremental next-image scoring + retuned BA cadence |
| D38 | Mapper speed without the D37 quality regression: persistent solver, convergence-adaptive BA, seed memoization |
| D39 | Keypoint masking at extraction, sampled in uv, with mask files found by convention |
| D40 | One camera per folder by default, resolution always splits, OpenCV distortion by default |
| D41 | Multiple models: every component is written, not just the best one |
| D42 | Pair-selection breadth follows the quality preset; it is not the lever for weak graphs |
| D43 | Model merging: shared *poses*, a transactional attempt, and a splice test that has teeth |
| D44 | Mapping and merging are one loop, and an assembled model gets audited |
| D45 | A fisheye is not a pinhole at verification time, one lens is one set of intrinsics, and a fold is detected from what is missing |
| D46 | Intrinsics describe the images on disk, camera model and focal are per group, EXIF is a measurement, and a fold is judged by what cutting it costs |
| D47 | The camera setup travels with the matches, and a pixel threshold means the same thing at every resolution |
| D48 | A forward-motion capture is not a degenerate one, and a focal nobody measured has to be measured |
| D49 | A spherical camera is not a wide fisheye: the image is the calibration, and every direction is in front |
| D50 | The principal point is not a free parameter: it is a camera rotation wearing a different name |
| D51 | Refining the principal point at the end: on, for a single camera group, and the gain is predictable |
| D52 | Next-image ranking is how the visible structure *spreads*, not how much of it there is |
| D53 | A rectilinear focal is measured from the fundamental matrix, and a measured focal is refined rather than searched |
| D54 | Track merging must leave a triangulation, or the filter undoes it and the pair churns |
| D55 | The bottom-up mapper is a *schedule*: atoms are reconstructed by the same mapper and glued by the same merger |
| D56 | Pair selection is two-stage: a cheap symmetric shortlist over every pair, the reliable asymmetric score on the shortlist |
| D57 | Bottom-up merges a level at a time with intrinsics shared across every model in flight, so nothing is averaged at merge time |
| D58 | A seed retry claims what it registered, so the retry looks where the last one stopped instead of rebuilding it |
| D59 | Atoms are reconstructed concurrently, each by its own mapper over its own sub-database, one Vulkan context per worker |
| D60 | The bottom-up mapper is the whole mapper: it finishes with the manage loop's own passes instead of handing over to it |
| D61 | Bundle adjustment stays in fp64: floating-point atomics make fp32 a lottery, and the losing tickets are worse reconstructions, not just different ones |
| D62 | The seed search re-tests planar/panoramic on the inliers, because verification judged the putative matches and the two disagree |
| D63 | Both mappers share one assembler: merge levels with growth and a joint solve, then the finishing passes once -- there is no manage loop |
| D64 | A merge with a well-determined alignment is not refused for disagreeing about shape: it is refined and re-judged on evidence the alignment never used |
| D65 | A joint solve that does not fit the device is split into batches, because the models it spans are coupled only through the intrinsics |
| D66 | Overlap between models is evidence, not waste: a model is never discarded for it, and growth into a neighbour continues while the pass is still finding ground of its own |
| D67 | A piece the fold detector wants to cut off is a duplicate only if it stands where something else stands; otherwise it goes back |
| D68 | A seam is judged against what the capture's own non-crossing pairs explain, not an absolute fraction: a verified pair is evidence, not ground truth |
| D69 | The PnP inlier ratio is measured over the correspondences the pose could see, not every one offered: a point behind the camera is not evidence against it |
| D70 | Aligning two models on the structure they both triangulated reaches pairs sharing no image -- but is off, because it is then judged on the evidence that produced it |
| D71 | The audit's repair is off: guessing a pose from evidence weaker than registration demands cost a 5356-image capture 28 points of AUC and the largest bill of any finishing pass |
| D72 | Distortion coefficients can be held through mapping and recovered in the finishing pass, like the principal point -- and holding them holds the principal point too, because the free intrinsics are a prefix |
| D73 | Per-image intrinsics as a finishing pass: reconstruct with the cameras shared, then split them one per image for a last bundle adjustment, with no clamp back to the group |
| D74 | The metric gauge is fixed where the model is written, from paired camera positions, and what refuses a fit is geometry -- spread and collinearity -- not the residual-derived uncertainties, which assume uncorrelated error and were measured under-stating a correlated reference by 3.9-4.5x |
