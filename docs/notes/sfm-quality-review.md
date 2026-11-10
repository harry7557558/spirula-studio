# SfM quality and robustness review

Review date: 2026-11-06. The source review below describes the starting point.
Options 0, 1, 2, 3, 4, 6, 7 and 8 have experimental implementations;
the prototype protocol is recorded at the end of this document. The
[experiment report](sfm-quality-experiments.md#default-decision) selects
supporting-image voting with tie deferral and guarded final completion as
defaults, and records the measured gains, regressions, and retained opt-in
policies. The rankings below are the pre-experiment hypotheses.

Users report sharper splats from RealityScan or COLMAP poses, particularly
for unordered images, shorter Spirula tracks despite more reconstructed points,
incorrect joins, and refused legitimate merges. These reports motivate the
review; they do not establish which stage is responsible. RealityScan was
not inspected. Measurements in the source-review sections come from repository
notes and comments; new experiments are identified separately.

The strongest first experiments are **completion/refinement scheduling,
ambiguity-aware 2D–3D association, and track-fusion policy**. Follow those with
targeted pair recovery and stronger independent merge validation. A wholesale
mapper replacement, more features everywhere, or universally looser thresholds
would make attribution harder and have higher regression risk.

## Scope and references

Spirula was reviewed at `d0106d859a8080b502308e504176c2fb0938cf01`.
COLMAP's latest default branch was cloned for source comparison at
`5b6da880d2b1b12a0b2bdc7264548d8ff617ef4a` (2026-11-06).
References below are pinned to that revision; defaults from older releases can
differ. That checkout was not built. Separate mapper experiments use the
official CPU [PyCOLMAP wheel](https://colmap.github.io/pycolmap/index.html),
version 4.2.1, installed outside the repository. COLMAP is an evaluation
reference and adds no production or build dependency.

Primary Spirula paths reviewed:

- [Pipeline.cpp](../../src/sfm/Pipeline.cpp),
  [SfmConfig.cpp](../../src/sfm/SfmConfig.cpp),
  [PairSelection.h](../../src/sfm/feature/PairSelection.h),
  [Matcher.h](../../src/sfm/feature/Matcher.h),
  [Verification.h](../../src/sfm/feature/Verification.h), and
  [TwoView.h](../../src/sfm/geometry/TwoView.h): selection through verification.
- [Mapper.h](../../src/sfm/map/Mapper.h): registration, triangulation,
  completion, refinement, seam checks, and shared-structure alignment.
- [Merge.h](../../src/sfm/map/Merge.h),
  [ModelOps.h](../../src/sfm/map/ModelOps.h), and
  [Assemble.h](../../src/sfm/map/Assemble.h): alignment, validation, recovery,
  splitting, and scheduling.
- [Bundle.h](../../src/sfm/map/Bundle.h),
  [Problem.h](../../src/sfm/ba/Problem.h), and
  [BA README](../../src/sfm/ba/README.md): parameterization and solver costs.
- [CameraSetup.h](../../src/sfm/core/CameraSetup.h),
  [global SfM](global-sfm.md), [rig constraints](sfm-rig-constraints.md),
  [sensor priors](sensor-priors.md), and
  [feature compaction](sfm-unused-feature-compaction.md): existing capabilities
  and constraints on changes.

## What the code already does, and where the hypotheses come from

### Short tracks are not evidence that completion is absent

`collectCompletion` follows correspondences transitively and enforces one
observation per image. `completeAndRetriangulate` completes tracks, creates new
ones, then calls `mergeTracks`. Visibility-pyramid registration ranking, robust
BA, reprojection/parallax filtering, and transactional refinement already exist.

There are nevertheless specific reasons to investigate fragmented tracks:

1. **Completion is conditional on reaching another refinement round.** In
   `globalRefine`, it runs only for `i > 0`, with retriangulation enabled.
   The stopping test uses `removedObs / before`, default `0.0005`. A first BA
   that removes little can finish without completing or merging tracks made
   compatible by its updated poses. A later round also ignores additions in
   its convergence statistic. This is a source-level observation, not proof
   that every final model misses tracks. COLMAP's
   [global refinement loop](https://github.com/colmap/colmap/blob/5b6da880d2b1b12a0b2bdc7264548d8ff617ef4a/src/colmap/sfm/incremental_mapper.cc)
   completes/merges and retriangulates before the loop, and counts completion,
   merging, and filtering changes after BA.
2. **Track fusion is capped at 20 combined observations.** `mergeOne` rejects
   larger unions and `mergeTracks` bounds repeated absorption to eight hops
   per starting point. This is **not a universal track-length limit**:
   creation and completion can produce longer tracks. The comment records
   uncapped fusion increasing mean length from 3.5 to 7.2 and BA iteration
   time from 0.03 to 0.36 seconds on one fisheye capture. That is a meaningful
   regression to reproduce, but not a universal cost model: dense Schur
   assembly has pairwise track work, whereas current implicit-Schur PCG
   avoids forming those pairs. Its iteration count and preconditioner still
   matter; longer tracks are not free there either.
3. **New tracks start from one maximum-parallax pair.** `featureTrack` tests
   free one-hop correspondences, picks the viable pair with greatest angle,
   and accepts other observations against that position. It does not first
   optimize a robust multi-view point. A noisy wide-baseline pair can exclude
   useful support. `attachExisting` and completion accept the first feasible
   assignment; fusion tests a track-length-weighted position against every
   observation. The weighted mean/all-inlier merge is also present in
   [COLMAP's triangulator](https://github.com/colmap/colmap/blob/5b6da880d2b1b12a0b2bdc7264548d8ff617ef4a/src/colmap/sfm/incremental_triangulator.cc),
   so that alone is not an explanation for a difference. COLMAP's creation
   path does use robust multi-view triangulation.
4. **Two-view population affects the comparison.** Spirula accepts tracks of
   length two. COLMAP's
   [triangulator defaults](https://github.com/colmap/colmap/blob/5b6da880d2b1b12a0b2bdc7264548d8ff617ef4a/src/colmap/sfm/incremental_triangulator.h)
   ignore isolated two-view tracks; this is not equivalent to deleting every
   current length-two track. Thus more points and a lower mean length may be
   partly a policy difference. Removing short tracks can improve the reported
   mean without improving any camera.

### Unordered registration discards alternative associations early

`gatherCorrespondences` chooses one 3D point per feature. Without a sequence,
it takes the first registered neighbor's point and breaks. With a sequence,
it prefers a nearby image. When correspondence edges reach fragmented tracks
or different repeated structures, unordered PnP receives an arbitrary member
of that ambiguity rather than the alternatives. The graph's iteration order
comes from stored pairs, so filename/pair-order perturbation is a useful test.

The PnP floor is 15 inliers; the ratio is 0.25 and, by default, its denominator
excludes points behind or outside the hypothesized camera. Visibility ranking
decides which image to attempt; it is not a spatial-support requirement on
the accepted inliers. The code records severe regressions from substituting
absolute support for the ratio, including an unsuccessful rival-pose remedy.
Repeating that experiment under another name is not a priority. Improving the
associations supplied to the estimator and measuring their independent support
is a different intervention.

### Pair selection can miss useful overlap before mapping starts

`auto` normally switches to preselection at 100 images unless pairing was
explicitly chosen. Defaults score 512 ranked features against the other image,
retain 32 neighbors per image, and use a 256-feature/128-neighbor coarse
shortlist from 200 images. Symmetric union can give an image more than 32
edges; 32 is not a hard final degree limit. The `high` SIFT preset uses 2400 px
and 8192 features; `extreme` changes resolution, feature count, and neighbor
count together. A preset comparison does not isolate the cause of a gain.

Large-scale SIFT features guide retrieval; other frontends use detection
scores. A small doorway, a loop revisit, or a weak image can lose to many
near-duplicates. No amount of mapper completion recovers an edge that was
never matched. Mapping normally releases descriptors and uses a fixed
correspondence graph, which makes feedback-driven rematching a pipeline
change, not just a new mapper threshold.

### Both false joins and refused joins already have defenses

Assembly merges, grows, solves, audits, splits, reseeds, and checks folds.
Merges use shared images, calibrated rig frames, or verified shared structure
through `structureLinks` / `alignByStructure`. “Needs shared images” is not a
complete description of the current assembly path.

`MergeSession::tryMergeImpl` checks splice conflicts, hollow incoming images,
and anchor observation loss. `seamValidator` tests cross-seam pair agreement,
can use a relative reference bar, and can refine/rescore a disputed merge.
`pairAgreement` primarily tests bearing epipolar agreement, which cannot
establish translation magnitude and can agree with repeated structure.
`checkSeam` takes up to the 600 strongest pairs by match count, so many
correlated pairs can dominate. Fewer than ten tested pairs skips the seam
test; this is a lack of evidence, not a positive seam verdict. Other merge
checks still apply.

The relative seam reference can be cached once sufficient evidence exists.
Captures with very different regions merit testing regional reference scores
rather than assuming one cached reference describes every later seam. The
existing BA rescue and rollback should be preserved, not proposed as missing.
Fold cuts and seam welding already have guards, but their documented
calibration set is limited; neither is a general solution to perceptual aliasing.

## Ranked options

Rank is recommended **investigation order**, balancing likely benefit against
regression risk and implementation effort. Effects are hypotheses, not measured
gains. Risk means risk to correct reconstruction; S/M/L scope means a contained
host change, several interacting stages, or solver/pipeline/model work. Runtime
and memory refer to the affected stage; end-to-end costs require measurement.
All proposals remain native, and shared behavior belongs in existing seams.

| Rank | Proposal | Expected quality and reach | Risk | Speed / RAM / VRAM | Scope |
|---|---|---|---|---|---|
| 0 | Record track lifecycle and merge evidence; establish controlled baselines | No direct gain; essential to distinguish all reported failures | Low | Small reporting cost; bounded samples | S–M: profile/events, evaluation tooling |
| 1 | Ensure completion/fusion participates in final refinement convergence | Potentially high; all frontends, especially sparse unordered graphs | Low–medium | Extra graph pass and possibly BA; observations and memory may grow | S: mapper scheduling and counters |
| 2 | Resolve ambiguous 2D–3D associations by support, with bounded alternatives | High potential for wrong placements and missed registrations; broadly useful | Medium | More host scoring/RANSAC only where ambiguous; little device memory | M: mapper association, classification, retry |
| 3 | Make fusion limits reflect solver cost and geometric value | Medium–high for fragmented loops/seams; little benefit where cap never binds | Medium | Can sharply increase dense BA cost; profile PCG and host fallback separately | S for ablation; M for adaptive policy |
| 4 | Recover missed overlap with targeted pair expansion | High when retrieval is the bottleneck; broad unordered benefit | Medium | Extra matching and graph storage; budgeted descriptor reloads | M–L: pipeline, pairing, graph/cache lifecycle |
| 5 | Judge merges using diverse, partly held-out evidence; distinguish uncertain from contradicted | High robustness value for repeats and legitimate drifted joins | Medium–high | Targeted matches and trial BA; snapshots cost RAM | M: mapper seam checks, assembly and merge reports |
| 6 | Robust multi-view triangulation and optimized fusion candidates | Medium–high geometric consistency across datasets | Medium | Extra small host solves; longer tracks increase BA work | M: geometry and mapper track operations |
| 7 | Calibrated essential estimation and stronger degeneracy checks | Medium–high for calibrated, wide-baseline and high-outlier pairs | Medium | Smaller minimal samples can save trials; model fitting adds cost | M: geometry, verification, initialization |
| 8 | Better feature localization and calibrated camera-group refinement | Potentially high for final sharpness; gain depends on resolution/lens errors | Medium | More extraction/patch work; per-image intrinsics can greatly enlarge BA | M–L: feature/camera/refinement paths |
| 9 | Guided rematching of trustworthy pairs | Medium recall/track gain; weak texture and repeated descriptors | Medium–high | Extra matching, descriptors and geometry buffers | M–L: matcher, verification, pipeline |
| 10 | Local BA with fixed boundaries and explicit gauge handling | Medium quality potential; broad incremental benefit | Medium–high | Extra small solves, possible savings in global work; bounded local VRAM | L: BA parameter masks, host/device solver, mapper |
| 11 | Use existing sequence/rig/telemetry more reliably when genuinely available | High on suitable captures; unavailable on true unordered collections | Low–medium for plumbing; high for hard inferred constraints | Often saves matching/registration work; modest factor memory | S–M: provenance/UI/pipeline; calibration is larger |
| 12 | Validate existing global/bottom-up schedules; graph-based PCG coarse groups | Potential quality or speed gain, dataset-dependent | High for schedule changes; medium for preconditioner | Potentially faster large sets; graph/tracks can raise peak RAM/VRAM | M–L; reuse existing implementations |
| 13 | Rolling-shutter or temporal intrinsics models; feature-metric refinement | Potentially high for diagnosed camera-model/localization failures; narrower reach | High | More unknowns or image features; substantial compute/memory | L: camera, projection, BA, serialization/export |

### 0–3: first experiment group

**0. Add evidence before changing acceptance.** Preserve stage timings and
current refusal reports, but expose distributions and a reason ledger:

- Track-length histogram (2, 3, 4–7, 8–19, 20+), observation-weighted and
  point-weighted summaries, unique images, parallax, and spatial coverage.
- Observations created, completed, fused, removed and recreated per round;
  merge refusals by cap, same-image conflict, reprojection and parallax.
  Count compatible cross-region links lost to the cap, not just long points.
- Registration support by unique features, unique 3D points, supporting
  cameras and image area; number of features with multiple candidate points.
- Merge candidate provenance, independent support, shared-image/structure
  counts, skipped tests, pre/post-rescue score and observations lost. Separate
  “never proposed,” “no overlap,” “alignment failed,” “contradicted,” and
  “rescue could not run / exhausted budget.”
- Pair attempts versus verified successes, rejection reasons, and untested
  candidate overlap. A missing verified edge does not tell whether it was
  untested, failed matching, or failed verification.

**1. Change scheduling in isolation.** First try one explicit final completion
and fusion pass followed by BA/filtering, keeping every threshold unchanged.
Then compare a bounded loop whose stopping statistic includes additions,
merges and removals. Keep the 0.75 hysteresis and track churn separately:
net-zero observations can hide repeated deletion/recreation. Preserve
intentional `no_retri` finishing/weld stages. A dirty-track queue is a later
speed optimization, after proving the quality effect.

**2. Improve the PnP input, not just its inlier gate.** Gather distinct candidate
point IDs per feature, count independent supporting views, and prefer
well-triangulated, well-fit support. For ambiguous features, test a small
number of covisibility-consistent hypotheses rather than one flattened pool.
Score at most one observation per feature and prevent duplicated point votes.
Defer unresolved placements until support changes; cap retries. Evaluate
spatial spread and depth/baseline diversity as confidence signals rather than
universal hard thresholds that reject corridor or facade captures. Test
shuffled filenames and pair ordering, repeated rooms, and sparse true overlap.

**3. Separate track identity from BA cost.** Ablate fusion caps 20/40/unbounded
on fixed matches, with explicit dense/PCG and host/device runs. Measure loop
constraints, actual accepted lengths, iteration count and peak memory. The
choice may be a larger final-only cap or selective fusion of tracks connecting
weakly tied regions. A solver-aware policy must account for fallback: selecting
PCG initially does not guarantee every subsequent solve uses it. If temporary
BA observation subsampling becomes necessary, retain full track identity for
completion/export and test a final full solve; selecting observations can lose
information even when it saves memory. Do not replace the cap with another
unvalidated constant or claim shorter tracks are always adequate because the
point position is already well constrained: connecting two camera groups has
value beyond refining that point's depth.

### 4–6: better overlap and safer joining

**4. Expand pairs where the reconstruction lacks support.** Begin with existing
knobs: fixed extraction, vary shortlist size and neighbor count, and use
exhaustive matching on manageable subsets. Then reserve a budget for
low-degree images, cross-component candidates and diverse rather than only
near-duplicate neighbors. Compare spatially stratified retrieval features and
two-hop proposals; verify all new edges. Later, use reconstruction feedback to
request candidates, including some independent of current poses so a wrong
model cannot veto its own correction. Keep descriptor reloads bounded and
rebuild/remap the graph deliberately. Feature compaction currently changes
in-memory indices, so new matches require a stable original-index mapping or
a controlled restart from the persisted database. Avoid retaining all GPU
descriptors for the entire reconstruction.

**5. Validate evidence independent of the fitted transform.** Sample across
image neighborhoods, baseline/depth ranges and spatial image regions; report
effective independent support instead of treating hundreds of neighboring
video pairs as independent votes. Hold some links out of alignment and trial
BA. Add checks based on multi-view reprojection, triangulated ties and
cheirality alongside epipolar agreement; translation scale needs such evidence.
For insufficient support, request targeted verification or defer the merge
while retaining both components. Do not turn every skipped seam check into an
automatic rejection: genuine sparse connections would be lost.

For a contradicted seam, distinguish global misalignment from component drift.
The existing transactional BA rescue is the starting point; try bounded
overlap-focused refinement only after fixed boundary parameters are supported.
Evaluate local reference bars with a conservative absolute floor, uncertainty
and rollback, rather than allowing a poor model's reference score to authorize
arbitrary merges. Do not accept a join simply because optimization lowered its
own training residual. A scene with truly indistinguishable repeated parts
may require additional imagery, trustworthy metadata or user ties.

**6. Fit points to several views before deciding they lack support.** Try
bounded robust multi-view hypotheses initialized by good-parallax pairs, then
point-only nonlinear refinement before attaching further observations. For
fusion, optimize the candidate point with cameras fixed before the all-inlier
test; keep one-feature-per-image, cheirality and parallax checks. Test conflict
splitting separately from fusion. A rejected bad observation should not force
two otherwise compatible tracks to remain fragmented forever, but automatic
outlier trimming can also make a false union look valid. Use withheld views
and post-BA survival to judge that tradeoff. Compare isolated two-view track
policies independently; don't make deleting points the measure of success.

### 7–10: geometry, calibration, and refinement

**7. Use known calibration in minimal estimation.** The general bearing path
fits a seven-point epipolar matrix and only imposes the essential constraint
during pose recovery. Add a tested calibrated five-point path when intrinsics
are reliable, retaining uncalibrated F and homography competition when they
are not. COLMAP's
[two-view estimator](https://github.com/colmap/colmap/blob/5b6da880d2b1b12a0b2bdc7264548d8ff617ef4a/src/colmap/estimators/two_view_geometry.cc)
is a useful reference for calibrated ray-space verification. Keep fisheye and
spherical rays, extraction-pixel/angular conversion, pure rotation, planar
scenes and uncertain focal initialization in the tests. Add spatial-support
and watermark/static-overlay detection only with false-positive tests. Rotation
cycle checks can flag inconsistent links but cannot validate a bridge with no
cycle, or expose every consistently repeated structure.

**8. Measure localization and camera residuals.** Cross resolution and feature
count independently. Inspect residual vectors against image radius, row,
feature scale, camera group and time. Spatially balanced features and
coarse-to-fine localization of existing correspondences may be more useful
than doubling their number. Any patch refinement needs occlusion, exposure and
repeated-texture checks and must preserve match/feature indexing. Benchmark
existing SIFT, ALIKED/LightGlue and LoMa paths under comparable resource budgets;
learned matching is already implemented and is not guaranteed better.

EXIF grouping, focal bootstrapping, distortion control and optional final
per-image intrinsics already exist. Try justified group splits and staged
intrinsics release before adding new camera models. Penalized deviations from
shared calibration are worth investigating for focus/zoom changes; unconstrained
per-image intrinsics can absorb pose mistakes and increase solver dimension.
Judge calibration by unseen observations/rendering, not only fitted residual.

**9. Guided matching is a second pass, not relaxed verification.** Rematch a
reliable pair within its geometric search band, then independently reverify
and check multi-view consistency. Use bearing-space constraints for wide-FOV
cameras. Keep new edges distinguishable from evidence used to fit the guide,
and prevent guided confirmation of repeated structures. COLMAP exposes
[guided matching](https://github.com/colmap/colmap/blob/5b6da880d2b1b12a0b2bdc7264548d8ff617ef4a/src/colmap/feature/matcher.h)
but defaults it off; its availability does not establish that it explains
users' COLMAP results.

**10. Local BA is a quality-control proposal as well as a speed proposal.**
Refine a newly registered image, covisible neighbors and their points before
they seed much more geometry, with external boundary cameras fixed. Current
mapping relies on periodic global BA and Huber loss. Fixed parameter masks and
component-aware gauge handling need a coherent host/device implementation;
LM damping is not a substitute for an observable reference frame. Rigs and
metric priors must remove only gauges they actually constrain. Benchmark the
CPU versus GPU for small solves, retain periodic global loop closure, and
avoid assuming local BA removes the cost of the last large solves. A final
robust-to-quadratic loss schedule is a separate ablation after outlier control,
not an immediate replacement for Huber.

### 11–13: conditional improvements and larger projects

**11. Preserve real metadata without relying on it for the core fix.** Rigs,
sequence-aware PnP, IMU rotations, GPS factors and camera calibration already
have substantial implementations. Audit whether import/extraction preserves
their provenance and whether the selected configuration activates them. Video
auto-pairing switches to preselection at 100 images; `prefilter_sequential`
defaults false. Inspect explicit sequence/rig pairing separately rather than
assuming a video label supplies all constraints. Where order is known, test
retrieval plus temporal neighbors and loop links. Never derive a hard temporal
constraint from arbitrary photo filenames, or treat GPS/IMU as infallible.
Validate timing, uncertainty and conflicting sensors. Every general proposal
above must also work with metadata removed.

**12. Evaluate the global and bottom-up paths that already exist.** Reuse their
track conflict handling and relative-rotation consistency where justified;
do not build another SfM implementation. Global initialization remains
experimental ([limits](global-sfm.md)); a wrong bridge or poor calibration can
affect the whole model. Bottom-up changes overlap and merge demands, not the
underlying ambiguity. Separately, the BA coarse preconditioner groups adjacent
frames: covisibility-based groups may improve unordered PCG conditioning
without changing the objective. Benchmark shuffled order at equal convergence
tolerance; this is chiefly a speed enabler for higher-quality solves.

**13. Reserve model complexity for diagnosed residuals.** Rolling shutter,
stabilization/cropping and changing intrinsics can leave systematic pose error
that more tracks cannot fix. Estimate an appropriate model only after testing
observability and metadata support. A rolling-shutter SfM pose exported as a
single ordinary camera may still be inconsistent with the trainer's projection;
include the dataset/export/training contract in the design. Feature-metric or
photometric pose refinement is a research option with illumination, occlusion
and memory risks. Neither should be the first response to missed matches or
incorrect topology.

## Evaluation and decision gates

### Separate causes before comparing products

Use a fixed image manifest, masks, decoded pixels, orientation handling,
camera grouping and train/eval split. Record tool revision, resolved options,
feature resolution, feature/match counts and hardware. Use fresh workspaces
when changing extraction so stale matches cannot masquerade as a quality
regression. Existing interchange is a starting point, not a claim that native
`features.bin` / `matches.bin` can be passed directly to COLMAP. A stage-swap
experiment needs an explicit database adapter preserving coordinates, feature
indices, camera models and IDs; do not merge independently extracted models
by assuming their feature indices agree.

Compare (a) each product's normal workflow and (b) matched frontends/budgets
where possible. First hold features/matches fixed for mapper experiments;
then hold the mapper fixed for retrieval/localization experiments. Match pixel
thresholds in physical meaning: Spirula uses extraction pixels with
`pixel_scale`, so equal numeric flags need not be equal tests. Match two-view
track policy or report its effect separately. RealityScan exports can
serve as another pose/calibration baseline if available, without speculation
about its proprietary internals.

### Dataset coverage

Use held-out captures as well as development scenes, with independent scoring
where available (e.g. ETH3D, Tanks and Temples, image-matching benchmarks).
Include small object orbits, indoor rooms, repeated corridors/facades,
vegetation, low texture, mixed-camera internet photos, forward motion,
wide-FOV/fisheye, long looped videos and weakly overlapping components. Include
disconnected scenes that **must stay separate**, correct difficult joins, and
known false joins. Test native unordered captures as well as shuffled sequences;
shuffling a video alone is not representative of diverse photos. Repeat the
sequence/rig cases with metadata removed, corrupted and correctly supplied.

### Score structure, poses, rendering and resources together

- Registration coverage per component, largest correct component, and false
  placement / false merge / false split counts. Report both precision and
  recall of joins; one model is not automatically better than several.
- Track distributions, usable observations per image, spatial support,
  triangulation angles, duplicate physical structure and constraint strength
  across weak cuts. Mean length and raw point count are diagnostic, not goals.
- Relative rotation/translation errors, drift and intrinsics plausibility;
  residual distributions in extraction and source pixels, including held-out
  observations. Existing pose AUC penalizes missing images, so show coverage
  and accuracy on common registered images separately. Account for reference
  principal-point conventions rather than treating every pose delta as error.
- Train the same splat configuration with fixed images, budget, resolution,
  exposure handling and repeated seeds. Evaluate fixed held-out views with
  PSNR/SSIM/LPIPS and aligned edge/detail crops, checking floaters and ghosting.
  Do not evaluate only each model's easiest registered images. For camera
  accuracy, use independent reference cameras when available, and also train
  with a common aligned initialization cloud to separate pose/calibration
  effects from different sparse seeding. Native clouds are a second,
  end-to-end comparison. Report coverage failures separately from common-view
  render quality; more aggressive sharpening is not evidence of better poses.
- Wall time by extraction, retrieval, matching, verification, mapping, BA and
  assembly; peak resident RAM and VRAM, host fallback, BA/PCG iterations,
  and repeated-run variance. Include a constrained-memory device and shuffled
  order. Keep arithmetic precision unchanged during algorithm comparisons.

Start with rank 0 and separate rank 1/2/3 ablations. Promote a default only if
it improves the intended metric on held-out scenes without new catastrophic
joins or disproportionate resource regressions. If the only gain is more
points, longer reported tracks, or lower fitted residual, the quality claim
has not passed. If ranks 1–3 do little, use the stage-swap evidence to choose
retrieval, localization/calibration, or merge work instead of stacking knobs.
Maintain a Pareto comparison of quality, robustness, time and memory rather
than hiding a severe failure in an average score.

Suggested future regression tests: a late loop link with almost no filtered
observations; fragmented tracks crossing the fusion cap; multiple 3D candidates
for one unordered feature; repeated structures with correlated false support;
a real drifted seam versus a false seam; a correct join with few independent
pairs; truly disconnected components; and compaction/rematching index fidelity.
Extend existing `sfm_map_test`, `sfm_merge_test`, `sfm_seam_weld_test`, geometry,
rig and BA tests where they fit. Synthetic success is necessary but does not
replace the real-data/rendering gates above.

## Changes not recommended without new evidence

- Globally lower inlier ratios, widen reprojection limits, disable seam/fold
  checks, or force components into one model. The repository already records
  failures from broad acceptance relaxations.
- Drop all short tracks to make mean length look better, or remove the fusion
  cap globally without measuring the active solver and fallback paths.
- Assume a learned matcher, global mapper, more features, or a new camera
  model improves every dataset. Each can amplify existing bad associations.
- Use fp32 BA as the budget for a quality change: the documented precision
  regressions would confound the experiment.
- Add a second implementation, a Python runtime dependency, or COLMAP/Ceres
  as an unreviewed production dependency. These ideas fit existing native
  interfaces; solver changes must preserve supported host/device configurations.

## Prototype evaluation protocol

The experiment baseline is the revision above. Frozen executables preserve
each prototype while later changes are evaluated. No training implementation,
bundle-adjustment arithmetic precision, seam acceptance threshold, or camera
model is changed as part of these comparisons. The camera models come from
existing reconstructions or saved dataset settings; their poses and fitted
intrinsics are not used to initialize the experimental reconstructions.

The initial screen uses a 1600-pixel long edge, 4096 SIFT features, 32 retrieval
neighbors, four host threads, and disabled feature compaction. These explicit
limits override the `high` preset and are not measurements of its normal
2400-pixel/8192-feature configuration. Mapper ablations reuse the exact same
features and verified matches. Frontend ablations use fresh workspaces or
explicit rematching. Learned-frontends use 2048 features and the same input
image-size limit; their different detector/matcher costs are reported. Their
normal `high` preset uses 4096 features, so the 2048-feature screen is a budget
ablation. A separate 4096-feature run tests the mixed-camera failure case.

Standalone `map` comparisons receive no image directory or capture manifest.
`auto` comparisons can read EXIF GPS and attitude, so their results must not be
treated as repetitions of standalone mapping. Separate manifest experiments
retain rig/sequence declarations, capture telemetry, or both. No masks are used
in this screen. Private captures are identified only by anonymous labels in
reported results.

The measurements have complementary limitations:

- Reconstruction audits count component coverage, track distributions,
  observation backreferences, repeated views within a point, image-grid
  support, and radial residuals. A correspondence-graph audit counts the
  observations inside conflicting connected components, since one large
  component can invalidate many tracks while counting as only one conflict.
  More points, a longer mean track, or lower fitted residual is not a success
  criterion.
- Mapping holdouts remove entire connected components of verified feature
  correspondences before reconstruction, with a deterministic 10% selection.
  Evaluation triangulates from two held-out views and predicts a third,
  charging missing views and failed cheirality as 180 degrees. The verifier
  and focal bootstrap have already seen these correspondences; this isolates
  mapper behavior and is not an independent test of the frontend calibration.
  A second score fixes the triples using the legacy holdout reconstruction:
  at most 24 evenly sampled views per track, a triangulation pair with greatest
  usable ray angle, and at least 1.5 degrees of parallax. This excludes
  co-located brackets/lenses and reports the excluded count. Its cohort favors
  baseline coverage; both scores still depend on correspondence correctness.
- Cross-frontend prediction evaluates a reconstruction against another
  frontend's verified tracks, using the same baseline-selected triples. It
  provides another localization check, but those matches can be wrong and
  conflict exclusion can leave a small, unrepresentative subset. Scores on
  the reconstruction's own frontend are fitted consistency measurements.
- Paired-lens consistency is measured on unconstrained reconstructions:
  rotation scatter, center separation normalized by scene radius, and paired
  frame coverage. Scatter on a rig-constrained result is zero by construction.
  A consistently wrong global arrangement can still pass this local check.
- Existing reconstructions provide supporting relative-pose comparisons, not
  ground truth. Image-ID permutations test sensitivity to unordered traversal
  while preserving the same named images, features, and matches.
- Rendering comparisons use identical common registered images, a fixed
  trainer, held-out views, explicit nonzero sampling seeds, and PSNR, SSIM,
  LPIPS-A and LPIPS-V. Seed `0` is clock-derived and is excluded from paired
  decisions. Short runs screen candidates; longer runs and native sparse
  initialization check convergence and seeding effects. Images are held out
  from splat training, not from SfM. Evaluation uses each reconstruction's
  own cameras, so a coherent geometric deformation can still score well.
  The `360-camera` preset also warps fisheyes into perspective faces using the
  estimated intrinsics; those workflow scores do not share identical target
  pixels. A focused association comparison disables warping and evaluates the
  original camera pixels. Report the two protocols separately.
- A common initialization cloud contains at most 50,000 baseline points,
  aligned by robust similarity fitting of shared camera centers. Alignment
  support is recorded. Native-cloud runs retain each reconstruction's seeding
  advantage; neither comparison alone isolates every geometric difference.
- Bracketed captures enable EXIF exposure initialization. Both raw and
  color-corrected rendering scores are retained because evaluation renders do
  not fit a new per-view exposure. A separate grouped split holds out every
  exposure of selected viewpoints, avoiding sibling-exposure leakage.

GPU experiments run serially. Each reconstruction/training process has a
12-GiB cgroup memory limit and no swap allowance, with smaller limits
for tests and audits. Wall time and peak resident RAM are measured per process;
device memory is sampled externally. The test machine has 32 GB system RAM
and a 12 GB RTX 5070. These limits bound experimental resource use; they do not
establish a universal memory requirement for the algorithms.

Track diagnostics are enabled during the exploratory screen. They enumerate
association alternatives even for the legacy policy, so final performance
comparisons must also run with `SS_SFM_MAP_PROF` unset. Setting it to `0` still
enables profiling because the existing switch tests whether it is present.
