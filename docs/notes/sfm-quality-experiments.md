# SfM quality experiments

Completed on 2026-11-07 (UTC), after about 19 hours. The retained tables contain
332 reconstruction records, 183 splat-training runs, and 94 mapping holdout
evaluations. These are repeated and overlapping controls, not independent
datasets. This report accompanies the
[ranked source review](sfm-quality-review.md) and the
[experiment switches](../../src/sfm/README.md#quality-experiments).

The baseline is `d0106d859a8080b502308e504176c2fb0938cf01`. Experiments run on
an RTX 5070 with 12 GB VRAM, a Ryzen 7 7800X3D, and 32 GB system RAM.
Reconstruction and training
jobs are serialized, limited to 12 GiB of cgroup memory with no swap
allowance, and normally use four host threads. Frozen executables preserve
each prototype while subsequent work continues. No trainer or BA arithmetic
precision is changed. Component merging keeps its existing acceptance
machinery; the acute-parallax experiment also affects its shared angle checks.

## Default decision

The selected defaults enable supporting-image association voting with tie
deferral (`SFM_TRACK_ASSOCIATION=2`) and guarded first-round completion
(`SFM_TRACK_COMPLETION=3`). The completion
pass runs before the first BA round of each final refinement only after
`max(3, min_model_size)` images register; it does not alter the growth-phase
schedule. Track additions and fusions participate in convergence on those
selected refinements. Existing no-retriangulation and seam-protection controls
still apply.

Keep the ordinary fusion cap at 20, existing preset frontend choices (SIFT
as the general default), current feature budgets, shared camera grouping and
intrinsics schedules, and fp64 BA defaults. Leave the
other new geometry, matching, extraction, and deduplication policies disabled.
Diagnostics are opt-in and do not require retaining per-run research artifacts.

The combination is a practical compromise across the tested captures, not a
claim of universal accuracy or a fix for all false/refused merges. It has
larger rendering gains on two difficult unordered comparisons and the bracketed
comparison, a smaller public-scene gain, and fewer large learned-frontend pose
disagreements, although the 4096-feature ALIKED control loses eleven cameras.
Voting alone loses on the H2/H4s native-cloud tests; completion
alone does not repair the learned ambiguity failure or reproduce the combined
hard-scene gains. M2 regresses with the combination under both initialization
protocols, despite gains from voting alone. The reserved hard capture has
nearly neutral native-cloud LPIPS-A and worse SSIM, while reconstruction
repeats with telemetry remain
unstable for legacy and combined policies alike.

The table summarizes matched combined-policy minus legacy LPIPS-A deltas.
Negative is better. Common-cloud values average two training seeds; native
seed counts are shown explicitly. These are separate captures and protocols,
not a pooled score or independent statistical trials. H2's native entry uses
all registered main-component training views with fixed evaluation names.
Full protocols, PSNR/SSIM, and color-corrected results follow below.

| Capture | Iterations / image divisor | Common-cloud LPIPS-A delta | Native-cloud LPIPS-A delta | Native seeds |
|---|---|---:|---:|---:|
| Bonsai | 30000 / 2 | -0.001043 | -0.000480 | 1 |
| DrJohnson | 30000 / 1 | -0.000277 | -0.000754 | 1 |
| M4d | 15000 / 1 | -0.011761 | -0.013408 | 1 |
| M2 | 15000 / 2 | +0.001354 | +0.001929 | 1 |
| H4s | 7500 / 2 | -0.002548 | -0.002837 | 2 |
| H2, unordered | 15000 / 4 | -0.014095 | -0.010523 | 2 |
| H3s, reserved | 7500 / 4 | +0.003944 | +0.000227 | 2 |

Fresh H2 telemetry runs have a separate stability limitation: the combined
policy improves native-cloud LPIPS-A against the first legacy run in one
reconstruction repeat and regresses in another. Legacy repeats also vary
substantially. Two training seeds for one reconstruction do not measure its
reconstruction failure probability. The selected defaults do not claim to
solve that instability.

Adoption priority reflects measured value, regression risk, and implementation
effort. The more focused follow-up experiments are listed at the end.

| Priority | Prototype / decision | Quality and generality | Risk | Runtime / memory | Code scope / effort |
|---|---|---|---|---|---|
| 0 | Measurement (option 0): retain opt-in | Separates coverage, track retention, topology, prediction and rendering; no accuracy claim from residuals alone | Low operational risk; reference/holdout leakage and correlated views remain | Audits are offline; profile counters add work only when enabled, apart from small convergence bookkeeping | Medium: diagnostics and a native audit/evaluation executable |
| 1 | Voting + guarded completion (options 1/2): enable together | Best practical balance in the completed matrix; benefits span SIFT and learned association controls, with named exceptions below | Moderate: branch changes, correlated rig/bracket votes, coverage losses and repeat variability | Extra final passes; Bonsai mapper about +33%; no large measured VRAM increase | Small: local registration and global-refinement policy |
| 2 | Targeted frontend choice / feature budget (option 8): keep available, retain SIFT defaults | Learned matching helps bracketed coverage/rendering but can fail topology and costs more; no universal replacement | Moderate, dataset dependent | M4d ALIKED/LoMa 100.8/205.4 s versus high SIFT 34.7 s at the tested unequal budgets; higher VRAM | Small: existing frontend choices; no new model implementation |
| 3 | Fusion cap (option 3): retain 20, expose override | Small mixed coverage/pose gains; mean track length is not sufficient evidence | Moderate as cap grows; seam cap remains independent | H2 uncapping modestly increases CG/dense time; historical dense slowdown not reproduced here | Small: track-fusion policy |
| 4 | Robust point/fusion refinement (option 6): opt-in | Synthetic geometry passes, but real-data evidence does not show a general gain beyond the chosen policy | Moderate: can reinforce an incorrect association | More host work per hypothesis/union, bounded point optimizer; no large device allocation | Medium: geometry helper and track creation/fusion |
| 5 | Pair expansion (option 4): opt-in | Can connect a component while dropping other views; M4d total coverage regresses | Moderate/high on repeated structure | Bounded extra pair verification; larger graph and verification memory | Medium: pair-selection/verification orchestration |
| 6 | Calibrated five-point solver (option 7): opt-in | Useful geometric prototype; real screen does not justify EXIF-based activation | High calibration-prior/conditioning risk; two exact minimal cases missed | Matching about 3.2x on Bonsai and 6.6x on M2 when forced | Large: new minimal solver, shared polynomial roots, verification and seeding |
| 7 | Pixel convention, SIFT grid, acute angle, isolated two-view exclusion, landmark uniqueness (options 2/8 and related controls): opt-in | Physical/invariant tests support several individually; end-to-end coverage/topology and rendering do not support enabling them globally | Mixed: tiny coordinate changes can change mapping branches; deduplication regresses learned cases | Grid may describe up to 4x features; others chiefly host work or reduce retained structure | Small/medium per control: extraction/cache keys, geometry gates, or pose commit |

## What the prototypes test

| Review option | Implemented experiment | Main risk / cost |
|---|---|---|
| 0: measurement | Track/backreference audit, correspondence-component conflicts, spatial/radial support, mapping holdouts, paired-lens checks, repeated rendering | Fitted residuals and correspondence-derived scores can favor internally consistent mistakes; reporting must retain coverage and exclusions |
| 1: completion | Complete/retriangulate before the first final or growth BA round; count added/fused observations in convergence; an established-model guard | Completing an unstable two-view seed can destroy registration; extra full-model passes cost time |
| 2: associations | Count distinct supporting image IDs, defer tied support, and select one feature per landmark either before PnP or by residual afterward | Brackets and rigs make support correlated; choosing a feature before the pose is known can discard the correct observation |
| 3: track cap | Independently vary ordinary track-fusion union length; retain the seam-weld cap | Longer tracks increase dense Schur work quadratically; PCG and host fallback need separate measurement |
| 4: pair selection | Bounded expansion from verified two-hop support and broader retrieval, prioritizing disconnected/weak views | More verification work and more opportunities for repeated-structure false links |
| 6: triangulation | Score up to four parallax hypotheses with unique-view support, robustly refine the point, optionally refine proposed fusion unions | Local refinement can reinforce a wrong association; extra CPU work per point/merge |
| 7: relative pose | Native calibrated five-point bearing estimator in verification and seed selection; retain homography competition | EXIF focal initialization is not exact calibration; conditioning and focal/distortion error can change acceptance |
| 8: frontend/calibration | Physical pixel-center convention, spatial SIFT selection, resolution/count budgets, learned frontends, and existing intrinsics schedules | More features/resolution consume time and memory; learned matches can improve coverage or produce worse topology |

Two related controls test acute triangulation angle and graph-isolated
two-view tracks. The latter excludes a feature pair only when each endpoint
has exactly the other as its correspondence neighbor. It does not discard
every final track of length two. Acute angle folds an obtuse ray intersection
onto its supplement because nearly opposed collinear rays also leave depth
unobservable. It changes maximum-parallax pair ranking as well as the existing
angle gates in seeding, point filtering, and shared-structure merge checks.

## Captures and controls

Public scenes include Bonsai and Garden from mip-NeRF 360, DrJohnson and
Playroom from Deep Blending, and Huerstholz from the PPISP dataset.
Private captures appear only under anonymous labels:

| Label | Input used | Calibration / metadata context |
|---|---:|---|
| M1 | 1541 images | Exposure brackets, OPENCV camera model |
| M2 | 435 images | Mixed rectilinear and fisheye cameras, four camera groups |
| M3a | 225 images | Exposure brackets, OPENCV camera model |
| M4d | 145 images | Exposure brackets with incomplete reference registration |
| H1s / H1d | 320 / 320 images | Sparse trajectory sample / contiguous paired-lens window from the same capture |
| H2 | 1039 images | Mixed cameras, multiple rigs and sequences |
| H3s | 1800 images | Paired windows from multiple fisheye rigs and sequences |
| H4s | 401 images | Stratified mixed-camera subset, including paired lenses and ordinary photos |
| H5s | 800 images | Contiguous fisheye sequence window |

H5s also has a pose reference recovered from the active Metashape project
archive using the existing reference reader and exact photo-path matching.
Its largest component contains 791 of the 800 sampled images. This reference
uses the full capture, so the experimental subset may omit connecting views.
The extracted reference is used only for pose comparisons.
Windowed or stratified subsets can remove connecting views on the other
captures too; their component counts describe the supplied subset.

Camera models and grouping come from existing reconstructions or saved input
settings. Experimental reconstruction does not start from their fitted poses
or intrinsics. The initial screen explicitly overrides `high` with a
1600-pixel long edge, 4096 SIFT features, and 32 retrieval neighbors. The actual
`high` preset is tested separately at 2400 pixels and 8192 SIFT features.
The learned-frontend screen uses 2048 features, followed by a 4096-feature
capacity check; 2048 is not the normal learned `high` preset.

Mapper ablations share unchanged extracted features and verified matches.
They run without an image directory or manifest. Frontend `auto` runs may
consume EXIF GPS/attitude; they are not repetitions of standalone `map`.
Manifest comparisons retain rigs/sequences, capture telemetry, or both.
Removing capture streams does not disable the automatic EXIF fallback: the
`metadata_rig` control still uses available EXIF GPS, including on H4s. These
labels describe manifest contents, not a claim that every other metadata
source is absent. The standalone mapper has no image directory from which to
read EXIF GPS, but its supplied verified match graph may already reflect it.
Sequence permutations preserve named images, features, and matches while
changing traversal IDs. No masks are applied.

Telemetry adds another reuse distinction. Fresh matching can calibrate the
gyro-to-camera relationship on putative matches before geometric verification;
reusing `matches.bin` recalibrates it from the verified database. H2's logs
show different pair counts and calibration residuals on those paths. Cached
metadata comparisons therefore use the same reuse path for every policy,
with separate fresh-matching controls. A fresh-to-cached change is not an
exact-input mapper repeat, even when the feature and match files agree.

The [evaluation protocol](sfm-quality-review.md#prototype-evaluation-protocol)
details the limits of pose references and correspondence holdouts. In
particular, mapping holdouts have already been seen by geometric verification
and focal initialization. They are not an independent frontend accuracy test.

## Findings established by the diagnostic screen

### Mean track length is strongly affected by point-retention policy

The first independent mapper comparison imports exactly the Spirula SIFT
features, verified matches, camera grouping, and verification-stage camera
initialization into official CPU PyCOLMAP 4.2.1. It supplies no fitted poses
or rig/telemetry priors. This wheel is an external benchmark tool, not a
project dependency. It is distinct from the unbuilt latest-source reference
pinned in the review. See the official
[PyCOLMAP documentation](https://colmap.github.io/pycolmap/index.html).

| Bonsai, same input correspondences | Registered | Points | Observations | Mean track | Two-view fraction |
|---|---:|---:|---:|---:|---:|
| Spirula legacy mapper | 292 | 156476 | 812839 | 5.195 | 35.0% |
| COLMAP default mapper | 292 | 99652 | 753656 | 7.563 | 6.2% |
| COLMAP retaining isolated two-view tracks | 292 | 137078 | 828126 | 6.041 | 31.8% |
| Spirula excluding isolated two-view tracks | 292 | 118933 | 737567 | 6.202 | 14.5% |

COLMAP's default ignores graph-isolated two-view tracks. Its longer mean
coexists with fewer total observations. Retaining those tracks brings its
mean much closer to Spirula's, while leaving about 1.9% more observations and
12.4% fewer points than legacy Spirula. Policy explains much of the difference,
but not all of it. These counts establish neither better calibration nor
better splats. Rendering and a pixel-threshold scale control are separate tests.
The scale control multiplies COLMAP's pixel thresholds by the median
source/extraction scale; it approximates per-camera scaling on mixed-resolution
captures. Imported focal-prior flags include Spirula's bootstrapped focals,
so the two mappers can still treat the same initialization differently.
The initial mapper times were 28.3 s for instrumented GPU Spirula and
171.8 s for CPU COLMAP; this is an implementation/backend comparison, not an
algorithm-only speed ratio.

### Weak seeds need protection from early completion

On H1s, unconditional first-round completion reduces registration from 66
images in two components to two images. Requiring an established model before
first-round final completion preserves 67 images. The guard requires at least
`max(3, min_model_size)` registered images; existing later-round completion
remains available. This is why a larger track count on an easy scene is
insufficient evidence for unconditional completion.

### Association ambiguity can affect topology

On M2 with ALIKED/LightGlue at 2048 features, the legacy mapper registers 325
images. Relative to the existing reference, 26 cameras have rotation
disagreements above five degrees that are absent from the SIFT reconstruction.
Supporting-image voting with tie deferral registers 329 images: 21 of those
26 remain registered and fall below five degrees; five are omitted. One
additional disagreement is shared by SIFT, voted ALIKED, and LoMa, so it may
be a reference error. Rendering is needed before interpreting the remaining
small pose differences as accuracy gains.

This is a targeted robustness improvement, not proof that a learned frontend
should become the default. SIFT registers 430 images with the same metadata-free
mapper setup; learned coverage remains substantially lower at this budget.

Deduplicating landmarks before PnP is not a safe unconditional substitute.
Combining that policy with voting keeps 329 images but reintroduces additional
large reference disagreements. A second prototype keeps the PnP candidate
pool unchanged and selects the best finite-residual observation per landmark
only when committing the pose. It enforces one observation per image in a
track, but does not make the PnP consensus count independent of duplicate
landmarks. On the same ALIKED case, voting plus post-pose selection registers
313 images and introduces additional large reference disagreements. A clean
track invariant is insufficient evidence for promoting either deduplication
policy. Both remain experiments.

With LoMa on M2, voting increases registration from 334 to 352 images in
the mapper-only comparison; the original `auto` run registers 333. All
352 camera centers fit the reference alignment, and rotation disagreement at
the 95th percentile is 0.37 degrees. The previously shared large disagreement
is omitted, not corrected. On H2, metadata-free SIFT voting registers 1034
images versus 1024 for the legacy mapper and improves the sampled reference
AUC at five degrees from 88.6% to 90.7%. The high-preset rendering tests below
use separate reconstructions; these mapper scores alone do not select a default.

On H1d, ALIKED voting increases total registered coverage from 251 to 291
images. The largest component shrinks from 198 to 167 images, but its reference
center-alignment support rises from 66 to 159 and its 95th-percentile rotation
disagreement falls from 1.79 to 0.31 degrees. Separate components and large
remaining disagreements still prevent calling this capture solved. Both
coverage across components and conditional geometric agreement matter.

Combining voting with pixel-center correction and acute parallax on M2
ALIKED registers 348 images, but reintroduces five large rotation disagreements
absent from voting alone. Pixel correction with voting alone registers 359
images and also leaves five additional large disagreements. The three-policy
LoMa run instead registers 363 images with no disagreement above five degrees.
Physical convention tests therefore do not establish
that these geometry changes compose safely with the mapper's current
thresholds and branching decisions. The high-preset default comparison
separates legacy, voting, guarded completion, and their combination.

Keeping tied support and breaking ties by track length is a weaker alternative
in the targeted controls. On M2 ALIKED it registers 331 images, but retains
three large reference disagreements absent from legacy SIFT, compared with
none for tie deferral. On M2 LoMa it registers 310 images versus 352 with tie
deferral. On high-preset H2 it registers 1010 images and scores 63.2% reference
AUC at five degrees, versus 1024–1025 images and 72.7–76.3% in two tie-deferral
mapper runs on the same cached inputs. These
comparisons support deferring unresolved associations when voting is enabled.

Guarded completion also needs a separate composition check. Adding it to
voted ALIKED on M2 raises coverage from 329 to 336 images, but introduces two
large reference disagreements among the 316 cameras shared with the voting-only
run. One of the 21 corrected disagreements from legacy ALIKED becomes large
again. The additional cameras themselves account for neither new disagreement.
Against legacy ALIKED, the combined policy still reduces the total number of
large disagreements from 27 to three while registering eleven more cameras.
The new disagreements relative to voting alone are a composition risk, not
evidence that the combination is worse than the current default overall.
Voted LoMa instead grows from 352 to 365 images with no disagreement above
five degrees. A coverage gain on one frontend does not establish a safe
universal completion schedule.

Completion alone leaves the original ALIKED failure essentially unchanged:
324 cameras register, all 26 additional large disagreements remain, and none
is corrected. Its mapper takes 135.3 seconds. Voting supplies the topology
improvement in this comparison; completion cannot substitute for resolving
the ambiguous associations.

### Conflict-free learned tracks are a biased evaluation subset

For Bonsai, the correspondence graph places 262799 of 885793 matched SIFT
features inside components with conflicting image assignments. The equivalent
counts are 547172 of 585418 for ALIKED and 584030 of 592370 for LoMa at the
2048-feature budget. A few bad or duplicate links can connect many otherwise
correct observations transitively. These counts do not mean all those matches
are wrong. They do mean that discarding conflicted components leaves a small,
unrepresentative learned-feature holdout, so that score cannot decide the
frontend default.

### Held-out predictions and structural audits answer narrower questions

On Bonsai's 3487 selected triplets, legacy mapping predicts the withheld
third bearing with median error 0.01675 degrees. Final-only completion gives
0.01671 degrees, while the fraction below 0.1 degrees changes from 90.48% to
90.42%. On H4s's 595 triplets, the same comparison changes the median from
0.2377 to 0.2233 degrees and the fraction below 0.5 degrees from 60.50% to
61.85%. These early schedule controls do not include the established-model
guard or the combined policy. They show a useful diagnostic signal, not an
independent confirmation of the final default: verification already saw the
correspondences, the selector fixes an observable subset, and failed
predictions receive a 180-degree penalty.

The H3s high-preset audit finds no inconsistent track backreferences or
nonfinite projections of recorded observations in either legacy or combined
output. Repeated-image observations within tracks nevertheless rise from
828 to 932. Median per-image support barely changes, from 1792 to 1795, while
occupied cells in an 8-by-8 image grid rise from 56.25% to 57.81%. Voting is
therefore not a guarantee of one observation per image, and valid recorded
projections do not establish that an entire fisheye projection domain is
well behaved. The separate uniqueness experiments enforce a stronger
invariant but regress on the learned-feature capture.

### Coverage depends on frontend budget and capture type

On M4d, the low-budget SIFT screen registers 115 images in components of
105 and 10. The actual high SIFT preset registers 128 in components of 118
and 10, taking 34.7 s instead of 22.0 s. The 2048-feature ALIKED and LoMa
runs each register 135 images in one component, taking 100.8 s and 205.4 s.
Those gains do not transfer to every capture: on H1d, SIFT registers all
320 input images across overlapping components, while ALIKED and LoMa
register 251 and 265. Their largest components contain 198 and 191 images,
compared with SIFT's 300. Reference alignment indicates substantial geometric
disagreement for the ALIKED component and mainly a coverage limitation for
LoMa. Raising ALIKED to 4096 features on M2 registers 332 images, versus 325
at 2048, but the 95th-percentile reference rotation disagreement remains
161 degrees. The larger budget takes 879 s versus 532 s. Capacity alone does
not resolve this failure; a LoMa capacity control was not measured.

A matched-input standalone mapper comparison also tests the combined policy
on those 4096-feature ALIKED data. The feature count matches learned `high`,
but extraction remains limited to 1600 pixels. Legacy mapping registers 325
images with 48 rotation disagreements above five degrees; voting plus guarded
completion registers 314 with two. Of the original 48, 29 are corrected,
17 are omitted, and two remain. There are no new large disagreements on
shared cameras or among the 42 newly registered cameras, but 53 legacy
cameras disappear, giving a net coverage loss of eleven. Mapper time falls
from 176.2 to 86.5 seconds on these different reconstruction trajectories.
This extends the association result beyond the 2048-feature screen while
retaining a coverage cost. It is a pose-reference check, not a rendering test
or a measurement of the full 2400-pixel learned preset; the earlier 332-image
result is a separate automatic-pipeline run.

The M4d frontend comparison also measures the resource tradeoff. These are
full automatic runs; SIFT uses the actual high preset, while the learned
frontends use the 1600-pixel, 2048-feature screen.

| Frontend | Registered | Elapsed seconds | Peak process RSS, MiB | Sampled device use, MiB |
|---|---:|---:|---:|---:|
| SIFT, high | 128 | 34.7 | 798 | 859 |
| ALIKED + LightGlue | 135 | 100.8 | 851 | 2408 |
| LoMa | 135 | 205.4 | 848 | 3657 |

Device use includes the desktop and is sampled every two seconds. These
figures describe this capture and feature budget, not a frontend-wide memory
limit. Learned matching improves this bracketed capture's coverage and
rendering, but the extra cost and failures on other captures argue against a
universal frontend replacement.

With the actual high SIFT preset (2400 pixels, 8192 features), the two leading
mapper candidates have the following coverage. Each cell gives unique
registered images, followed by the number of components. Overlapping images
are counted once.

| Capture | Legacy | Voting, defer ties | Voting + guarded completion | Guarded completion alone |
|---|---:|---:|---:|---:|
| Bonsai | 292 (1) | 292 (1) | 292 (1) | 292 (1) |
| M4d | 128 (2) | 128 (2) | 128 (2) | — |
| DrJohnson | 262 (1) | 263 (1) | 263 (1) | — |
| M2 | 423 (1) | 429 (1) | 430 (1) | 427 (1) |
| H4s | 378 (1) | 375 (1) | 375 (1) | 377 (1) |
| H1d | 319 (2) | 320 (1) | 320 (1) | — |
| H2 | 1003 (2) | 1025 (1) | 1020 (1) | 1003 (2) |

Voting improves coverage or connectivity on several captures, with a loss of
three images on H4s. Completion adds one image on M2 but loses five on H2.
The H2 legacy components contain 994 and 57 images, with 48 shared images;
adding their sizes would conceal the coverage deficit. The completion runs
reuse extracted features and verified pairs, so their recorded elapsed times
must not be compared directly with the other columns' full automatic runs.
Separate unprofiled mapper repeats use identical cached inputs.
Across these seven high-preset captures, the legacy and voting runs have
byte-identical verified match databases and mapper-visible feature records
(keypoints, colors, extraction dimensions, and EXIF fields). The combined
runs reuse the voting features and matches.
The completion-only control preserves H2's total coverage but leaves 993- and
54-image components. Its sampled reference AUC is 80.8%; it does not reproduce
the combined policy's connectivity. A dash denotes an unmeasured control.

Coverage does not settle the H2 comparison. On the fixed reference sample,
the high-preset legacy and voting runs both score about 77.1% AUC at five
degrees, while voting plus guarded completion scores 88.3%. Mapper repeats
vary substantially in both coverage and reference agreement. The direct-pixel
rendering controls below check the practical effect and reveal a fisheye
projection-domain confound that these pose scores miss.

Bounded pair expansion also has mixed effects. On M4d, eight additional
ranked pair opportunities per image produce one 110-image component, including
five images from the original small component, but omit its other five.
Total registered coverage falls from 115 to 110. A larger main component alone
would hide that loss.

The intrinsics-schedule screen on M3a keeps all 225 images in every run.
Freeing per-image intrinsics raises 95th-percentile reference rotation
disagreement from 0.021 to 0.110 degrees. Fixing distortion throughout raises
it to 0.683 degrees; fixing the principal point in the final pass raises it
to 0.625 degrees. Delaying distortion refinement until the final pass gives
0.049 degrees. These are comparisons with an existing reconstruction, not
ground truth, but they provide no evidence for replacing the existing shared
camera groups and refinement schedule with a universal alternative.

Repeated unchanged mapper runs on H4s register between 376 and 379 images.
Small coverage deltas must therefore be checked against repeat variability
and image-ID permutations, in addition to pose and rendering measurements.
On three H4s image orderings, reference AUC at five degrees is respectively
65.06%, 66.68%, and 68.21% for the legacy mapper, versus 68.14%, 67.95%, and
66.05% for voting with tie deferral. Voting improves two orderings and worsens
one. The original-order improvement alone overstates its generality.

On H5s, guarded completion increases metadata-free mapper coverage from 641
to 680 images, while the output remains fragmented into eleven components.
With sequence hints, coverage rises from 651 to 681. The guarded runs' largest
component has 221 images whose centers all fit the reference alignment, with
rotation disagreements below one degree. That conditional agreement does not
validate every component or resolve the missing connections. Largest-component
pose AUC is dominated by missing coverage here.

Voting with tie deferral registers 682 images on the same H5s mapper input,
in ten components. Its largest component has 222 images, with 221 centers
supporting the reference alignment. The increased coverage still leaves a
substantially fragmented reconstruction.
Combining voting with guarded completion registers 686 images in ten
components, again with a 222-image largest component. It takes 122.4 seconds
without profiling. The four extra unique registrations over voting do not
resolve the missing inter-component connections.

### Reserved-capture validation remains mixed

H3s is reserved while the candidate policies are developed on other captures.
The fixed voting-plus-guarded-completion candidate is then tested against
legacy mapping at the actual high preset, with the same features, verified
matches, camera grouping, and no rig or telemetry manifest. Legacy registers
1789 of 1800 images; the candidate registers 1792. Both retain three
components. Their largest components contain 1127 and 1136 images, but
overlap between components means that the larger component counts do not
represent eleven additional unique registrations.

On the fixed 598-image reference sample, relative-pose AUC at five degrees
falls from 30.68% to 29.52%. Conditional median rotation disagreement rises
from 0.304 to 0.540 degrees, the 95th percentile from 0.586 to 0.647 degrees,
and center RMSE from 1.740% to 1.879% of reference scene extent. These modest
coverage and reference changes do not establish a general improvement.
Paired original-pixel rendering uses all 1127 shared main-component images,
141 evaluation images, 7500 iterations, fourfold downscaling, and two seeds
with both common and native initialization clouds:

| H3s initialization | Legacy LPIPS-A, seeds 1 / 2 | Voting + completion LPIPS-A, seeds 1 / 2 | Completion only LPIPS-A, seeds 1 / 2 |
|---|---|---|---|
| Common 50000-point cloud | 0.485199 / 0.483678 | 0.489902 / 0.486864 | — |
| Native sparse cloud | 0.458545 / 0.458071 | 0.457289 / 0.459780 | 0.455862 / 0.456493 |

The common-cloud LPIPS-A regression repeats. With native initialization its
sign changes between seeds, leaving a negligible mean increase of 0.00023.
Native-cloud mean PSNR falls from 18.511 to 18.475 dB, SSIM from 0.69002 to
0.68712, and LPIPS-V rises from 0.42284 to 0.42401. This reserved capture does
not confirm a general quality gain, although the native-cloud LPIPS-A result
also does not support treating the common-cloud loss as an equally large
end-to-end regression.
The common cloud aligns with 1126 of 1126 shared baseline centers and 1100
of 1126 candidate centers, so it is also a more favorable initialization for
legacy. Native runs initialize 500000 splats for both policies after the cap,
from 637859 and 640590 cohort points respectively. These short runs do not
establish the ranking after full convergence.

A separate fixed-policy check uses guarded completion alone. It registers
1792 unique images in components of 1129, 503, and 164, preserving all 1127
original main-component cameras. Reference AUC is 30.89%, with 378 sampled
cameras registered and 325 supporting the center alignment; rotation p95 is
0.556 degrees. Its native-cloud LPIPS-A improves in both training seeds, by
0.00213 on average, and color-corrected LPIPS-A improves in both too. PSNR and
SSIM changes are smaller and change sign between seeds. The common-cloud
completion-only control is unmeasured.

### Telemetry reuse can change the policy ranking

H2's metadata controls use the reduced 1600-pixel/4096-feature budget, retaining
the supplied rig, sequence, and telemetry information. Reusing verified
matches and recalibrating from freshly rematched putatives produce different
gyro priors, as described above. Comparisons within each path give opposite
rankings against the same pose reference:

| Calibration path / policy | Unique images | Reference AUC at 5 degrees | Rotation disagreement, p95 | Center-alignment support |
|---|---:|---:|---:|---:|
| Cached matches / legacy | 1028 | 67.22% | 1.994 degrees | 481 |
| Cached matches / voting + completion | 1035 | 92.77% | 0.354 degrees | 504 |
| Fresh matching / legacy | 1033 | 90.92% | 0.391 degrees | 497 |
| Fresh matching / legacy, repeat | 1033 | 42.89% | 2.538 degrees | 265 |
| Fresh matching / voting + completion | 1034 | 78.97% | 1.535 degrees | 465 |
| Fresh matching / voting + completion, repeat | 1039 | 74.29% | 1.763 degrees | 450 |
| Fresh matching / guarded completion only | 1035 | 91.68% | 0.420 degrees | 502 |

The reference sample contains 511 images. Both legacy runs register 509 sampled
images; the first combined run registers 509, its repeat all 511, and completion
alone 510. Missing reference coverage does not explain the large changes.
All five fresh runs' verified match
files are byte-identical to each other and to the original metadata run.
The legacy repeat also changes which images register, despite its unchanged
total. Instability is therefore present in the baseline as well as the
candidate; these two repetitions per policy cannot estimate a failure rate.
The legacy logs diverge during growth, before final sensor orientation:
registration order first differs after 70 matching entries, and the seventh
reported global BA already retains a different point count. The pre-verification
gyro pair counts and rounded residuals agree. Fresh-versus-cached calibration
is consequently not the only source of variation; the first numerical or
discrete divergence has not been isolated.
The original fresh legacy run also has high reference agreement, at 89.79%
AUC. The combined repeat reaches full input registration without resolving
the reference disagreement. Native-cloud rendering on 1029 common cameras,
with 129 fixed evaluation views, 15000 iterations, and fourfold downscaling,
favors the first combined reconstruction in both training seeds:

| H2 fresh metadata / native cloud | PSNR, seeds 1 / 2 | SSIM, seeds 1 / 2 | LPIPS-A, seeds 1 / 2 |
|---|---|---|---|
| Legacy | 20.185 / 20.193 | 0.6871 / 0.6855 | 0.390944 / 0.393332 |
| Voting + completion | 20.460 / 20.448 | 0.6917 / 0.6893 | 0.384166 / 0.385940 |
| Voting + completion, independent SfM repeat | 20.572 / 20.555 | 0.6890 / 0.6883 | 0.398600 / 0.398127 |
| Guarded completion only | 20.232 / 20.241 | 0.6894 / 0.6893 | 0.388273 / 0.388738 |

LPIPS-A improves on 68% and 66% of evaluation views, with improvements in
color-corrected LPIPS-A and LPIPS-V too. Saved first-seed target images have
identical hashes. Reference disagreement and practical rendering quality
diverge here; reference AUC alone cannot select the default. World-geometry
accuracy remains uncertain because rendering uses each reconstruction's fitted
cameras. The independent combined SfM repeat preserves the same 1029-camera
cohort and targets, but reverses the LPIPS-A result: its mean increase is
0.00622, despite higher PSNR and SSIM. The first combined reconstruction's
mean decrease is 0.00709. Repeating training on one fixed reconstruction
therefore understates the uncertainty in the SfM policy itself.

The legacy repeat omits two cameras from that 1029-camera cohort. A separate
matched comparison uses their 1027-camera intersection, 129 evaluation views,
native clouds, one training seed, and the same 15000-iteration/divisor-four
budget. Legacy's LPIPS-A rises from 0.388730 to 0.399930, SSIM falls from
0.692396 to 0.682512, and LPIPS-V rises from 0.368860 to 0.377692; PSNR rises
slightly from 20.283 to 20.381 dB. Baseline reconstruction variability therefore
also affects practical rendering. This smaller cohort has its own evaluation
names: its absolute scores must not be mixed with the 1029-camera table.
Neither two SfM repetitions nor their training seeds establish which policy
has the lower failure rate.

The completion-only control preserves the full original 1029-camera cohort.
Its mean LPIPS-A decreases by 0.00363, with improvements in PSNR, SSIM,
color-corrected LPIPS-A, and LPIPS-V in both training seeds. Those seeds
optimize one fixed completion-only reconstruction; they do not measure its
SfM repeat variability.

### Fusion limits and calibrated estimation have mixed returns

The H2 solver controls use identical reduced-budget features and verified
matches, with standalone mapping and diagnostics enabled. They isolate the
ordinary fusion cap; seam welding retains its existing cap.

| BA solver | Fusion cap | Unique images | Components | Mean track | Mapper seconds | Peak RSS, MiB |
|---|---:|---:|---:|---:|---:|---:|
| CG | 20 | 1026 | 2 | 4.214 | 129.6 | 1757 |
| CG | Unbounded | 1027 | 1 | 4.153 | 133.9 | 1587 |
| Dense | 20 | 1025 | 1 | 4.186 | 180.8 | 1574 |
| Dense | Unbounded | 1023 | 1 | 4.237 | 192.2 | 1567 |

Reference AUC at five degrees changes from 88.2% to 89.2% for CG and from
89.1% to 89.2% for dense BA. Final geometry and refinement trajectories differ,
so neither runtime nor memory is a pure per-track solver cost. The severe
dense-BA slowdown recorded in the source review is not reproduced on this
capture. These modest, mixed changes do not justify removing the protective
cap globally.

The calibrated five-point prototype passes its geometric tests, including
wide-FOV rays, noise, outliers, and planar competition. Forcing it on every
camera is expensive in the real-data screen: M2 matching takes 530.1 seconds
versus about 80 seconds with the existing estimator, and final registration
falls from 431 to 428 images. Bonsai retains all 292 images with nearly
unchanged reference agreement; matching rises from 8.4 to 27.0 seconds.
The `given`-focal controls on these two captures have no eligible camera pairs
and therefore do not exercise the new solver. Synthetic calibrated success
does not establish a reliable policy for EXIF or bootstrapped intrinsics.

### Rendering repeats are necessary

An exact-input Bonsai 2500-iteration repeat with the same nonzero sampling seed
changes LPIPS-A from 0.17434 to 0.17222. A single improvement of roughly 0.002
therefore does not establish a calibration gain. Training seed zero is clock-derived
and excluded from paired decisions.

At 30000 iterations and half resolution, an exact-input Bonsai repeat changes
LPIPS-A from 0.108467 to 0.108873 and PSNR from 30.8763 to 30.8352 dB.
Hashes of the cameras, images, sparse points, and common seed cloud remain
identical. This smaller long-run variation is still material when comparing
improvements of only a few ten-thousandths in LPIPS-A.

On that same 30000-iteration comparison, guarded completion produces LPIPS-A
0.107171 and 0.107499 for sampling seeds one and two, versus 0.108467 and
0.108891 for legacy mapping. The improvement survives both seeds on this
public scene. The result does not establish a universal default: coverage and
component behavior are mixed on harder captures, and the high-preset combined
policy has separate rendering controls.
With native sparse points at the same 30000-iteration, half-resolution budget,
the single-seed legacy/guarded results are LPIPS-A 0.107024 / 0.105941,
PSNR 30.965 / 31.070 dB, and SSIM 0.94795 / 0.94934. The smaller native-cloud
gain still supports the public-scene completion result without relying only
on a shared seed cloud.

Common-cloud comparisons use at most 50000 baseline sparse points, robustly
aligned to each reconstruction using shared camera centers. Native-cloud runs
measure seed-cloud convergence separately. Rendering uses the largest
component; separate components are not concatenated without an alignment.
All variants in a common-cohort comparison share
registered-image cohorts and held-out views; full-coverage experiments report
their additional cameras separately. Full-coverage runs hold evaluation names
fixed while training on each main component's remaining registered views.
This checks whether additional cameras help or harm the rendered result.
The trainer is the frozen baseline
executable, with a 500000-splat cap, disk image caching, and depth, normals,
masks, and the viewer disabled. Resolution and iteration count are held fixed
within each comparison. `min-init-fraction=0.1` retains the trainer's 50000-point
initialization floor; native-cloud runs below that floor therefore include
the trainer's normal seed expansion. Exposure-bracketed runs initialize
exposure from EXIF, report raw and color-corrected metrics, and include splits
that hold out every exposure of a viewpoint. These images are held out from
splat training; SfM has seen them and supplies their evaluation cameras.

The actual high-preset Bonsai comparison keeps all 292 cameras and the same
37 evaluation images, using 30000 iterations and half-resolution pixels:

| Bonsai policy | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 |
|---|---|---:|
| Legacy | 0.107386 / 0.107581 | 0.105084 |
| Voting | 0.107244 / 0.107525 | 0.104863 |
| Voting + guarded completion | 0.106585 / 0.106297 | 0.104604 |
| Guarded completion only | 0.106827 / 0.106479 | 0.104077 |

The combination improves common-cloud LPIPS-A by 0.00104 on average. Its
native-cloud improvement is smaller, 0.00048, with SSIM rising from 0.94930
to 0.95015 and PSNR almost unchanged at 31.079 versus 31.071 dB. Completion
alone gives the better native-cloud result on this scene. Voting alone is
near neutral. Each native result has only one training seed, and the small
differences should be read against the exact-input variation above.

DrJohnson uses 262 common cameras, 33 evaluation images, 30000 iterations,
and full-resolution source pixels:

| DrJohnson policy | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 |
|---|---|---:|
| Legacy | 0.178815 / 0.179052 | 0.179189 |
| Voting | 0.179325 / 0.177896 | 0.178664 |
| Voting + guarded completion | 0.178129 / 0.179185 | 0.178434 |

The combined common-cloud LPIPS-A change is small and changes sign between
seeds: its mean improvement is 0.00028. PSNR and SSIM improve in both seeds,
by 0.128 dB and 0.00061 on average, while mean LPIPS-V is nearly unchanged.
The single native-cloud run improves LPIPS-A by 0.00075, color-corrected
LPIPS-A by 0.00080, PSNR by 0.081 dB, and SSIM by 0.00069. This is a modest
public-scene result, with substantially less evidence for a perceptual gain
than the larger private-capture differences below.

M4d's high-preset comparison uses 118 common cameras, 15 evaluation images
from three held-out viewpoints with all five exposures grouped together,
15000 iterations, full-resolution source pixels, and EXIF exposure
initialization:

| M4d policy | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 | Native cloud color-corrected LPIPS-A |
|---|---|---:|---:|
| Legacy | 0.450022 / 0.450402 | 0.452360 | 0.366506 |
| Voting | 0.439225 / 0.437574 | 0.438406 | 0.350752 |
| Voting + guarded completion | 0.439319 / 0.437582 | 0.438953 | 0.350088 |

The combination improves common-cloud LPIPS-A by 0.01176 and color-corrected
LPIPS-A by 0.01441 on average, with raw LPIPS-A improving on 96.7% of views
averaged over the two seeds. Its native-cloud gain is 0.01341 raw and 0.01642
color-corrected; native PSNR rises by 0.143 dB and SSIM by 0.00580. Voting
alone is similarly effective here. These full-resolution scores have a
different cohort and budget from the earlier frontend screen and must not be
compared directly with its absolute scores. Native initialization has one
training seed. Aggregate mean track length stays about 5.24 in legacy and
combined output; that statistic does not predict this rendering gain.

The separate M4d frontend comparison uses 7500 iterations, half-resolution
pixels, EXIF exposure initialization, and 15 fixed evaluation images from
three whole bracket groups. Native-cloud runs train on each largest
component's remaining registered images, so they include coverage differences:

| M4d frontend | Main-component cameras | Native cloud LPIPS-A, seeds 1 / 2 | Native cloud color-corrected LPIPS-A, seeds 1 / 2 |
|---|---:|---|---|
| Reduced-budget SIFT | 105 | 0.382225 / 0.375981 | 0.302837 / 0.300041 |
| High SIFT | 118 | 0.380735 / 0.381478 | 0.302644 / 0.304920 |
| ALIKED + LightGlue, 2048 features | 135 | 0.370457 / 0.372258 | 0.289574 / 0.292749 |
| LoMa, 2048 features | 135 | 0.347667 / 0.348825 | 0.264175 / 0.263833 |

LoMa improves mean native color-corrected LPIPS-A by 0.03744 versus reduced
SIFT and improves both seeds. A separate 100-camera common-cloud comparison,
with the same whole-bracket split, gives color-corrected LPIPS-A
0.291665 / 0.288834 for LoMa versus 0.301363 / 0.301748 for SIFT. That smaller
gain controls the training cohort and seed cloud; the larger native result
also includes added training views and a different sparse initialization.
The frontend budgets and resource costs are recorded above. This supports
targeted frontend choice on bracketed captures, not universal replacement or
an isolated equal-budget matcher comparison.

M2's actual high-SIFT comparison uses 422 common cameras, 53 evaluation views,
15000 iterations, and half-resolution original pixels with both warps disabled:

| M2 high-SIFT policy | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 | Native cloud color-corrected LPIPS-A |
|---|---|---:|---:|
| Legacy | 0.288773 / 0.288958 | 0.284895 | 0.265233 |
| Voting | 0.286758 / 0.287045 | 0.284127 | 0.264634 |
| Voting + guarded completion | 0.290007 / 0.290432 | 0.286824 | 0.267439 |

This is a regression for the combination. Common-cloud LPIPS-A worsens in
both seeds, by 0.00135 on average, with mean PSNR lower by 0.120 dB and SSIM
lower by 0.00119. The native run worsens LPIPS-A by 0.00193, color-corrected
LPIPS-A by 0.00221, and PSNR by 0.135 dB; native SSIM is nearly unchanged.
Voting alone improves all these metrics. The combination's additional seven
registered images over legacy and higher reference AUC do not predict better
rendering on this common cohort. Native initialization has one seed.

The separate M2 direct-pixel association comparison uses 287 common registered images,
36 evaluation images, and 7500 training iterations. Both common-cloud sampling
seeds and the native-cloud control favor voting with tie deferral.

| Frontend / association | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 | Native cloud color-corrected LPIPS-A |
|---|---|---:|---:|
| SIFT / legacy | 0.300126 / 0.299758 | 0.301457 | 0.281131 |
| ALIKED / legacy | 0.321401 / 0.323069 | 0.310732 | 0.292199 |
| ALIKED / voting, defer ties | 0.317394 / 0.315915 | 0.305371 | 0.285574 |

With a common cloud, voting improves LPIPS-A on 61% and 78% of the held-out
views in the two seeds. Its native-cloud PSNR/SSIM are 18.659 dB / 0.754191,
versus 18.540 dB / 0.746977 for legacy ALIKED. SIFT still scores better on
these views. The native-cloud comparison narrows that gap, illustrating why
seed-cloud convergence must be measured separately from calibration.

H4s provides a second example of initialization changing the ranking. At the
high preset, its 375-camera common cohort has 47 evaluation views. With 7500
iterations and half-resolution original pixels, voting improves common-cloud
LPIPS-A in both seeds, but loses with its own sparse cloud. The combined policy
improves LPIPS-A under both initialization protocols:

| H4s policy | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seeds 1 / 2 |
|---|---|---|
| Legacy | 0.515594 / 0.515909 | 0.506456 / 0.507397 |
| Voting | 0.512820 / 0.514517 | 0.510304 / 0.511174 |
| Voting + guarded completion | 0.511787 / 0.514621 | 0.504638 / 0.503541 |

The combined policy's native-cloud PSNR is slightly lower than legacy in both
seeds, while SSIM is higher. These are modest, metric-dependent gains, not
evidence that every additional observation improves calibration.

Completion-only controls at the actual high preset separate that intervention
from voting. These use identical camera/evaluation cohorts within each pair,
two common-cloud seeds, and one native-cloud seed. Deltas below are guarded
completion minus legacy; negative LPIPS-A is better.

| Capture | Iterations / image divisor | Evaluation views | Common-cloud LPIPS-A delta, mean of two seeds | Native-cloud LPIPS-A delta | Native-cloud PSNR delta |
|---|---|---:|---:|---:|---:|
| Bonsai | 30000 / 2 | 37 | -0.000831 | -0.001007 | +0.082 dB |
| M2 | 15000 / 2 | 53 | -0.000854 | -0.000229 | -0.023 dB |
| H4s | 7500 / 2 | 47 | -0.000053 | +0.000620 | -0.019 dB |
| H2 | 15000 / 4 | 122 | -0.002002 | -0.000878 | +0.017 dB |

The small Bonsai gain survives native initialization. M2's native-cloud
LPIPS-A change is much smaller than its common-cloud change, while H4s is
close to neutral and changes sign between common-cloud seeds. Completion
alone does not reproduce the combined policy's H4s rendering improvement.
H2's completion-only intersection has 973 cameras and 122 evaluation views.
Its full-component control below uses the original 123 evaluation names;
each protocol must be compared with its own matched baseline.

The reduced-budget M3a completion-only control keeps all 225 cameras and
holds out six complete bracket viewpoints, 30 images. It uses 15000 training
iterations, half-resolution pixels, and EXIF exposure initialization:

| M3a policy | Common cloud LPIPS-A, seeds 1 / 2 | Common cloud color-corrected LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 | Native cloud color-corrected LPIPS-A |
|---|---|---|---:|---:|
| Legacy | 0.302219 / 0.303056 | 0.180671 / 0.179059 | 0.302896 | 0.184535 |
| Guarded completion | 0.301411 / 0.302676 | 0.179773 / 0.179836 | 0.300803 | 0.180557 |

The common-cloud raw LPIPS-A gain is small, 0.00059 on average; its
color-corrected change is nearly zero and changes sign between seeds.
Common-cloud color-corrected SSIM is lower in both seeds. The single native
run improves raw/color-corrected LPIPS-A by 0.00209 / 0.00398, with a small
raw SSIM loss and higher color-corrected SSIM. Mean track length stays about
5.16 and reference rotation disagreement changes very little. This is a
metric- and initialization-dependent result, not a uniform calibration gain.

Coherent geometric distortion can escape the rendering metric because
evaluation cameras are also reconstructed from the input images.
Fisheye workflow runs use the `360-camera` preset, whose perspective-face
warping depends on the estimated intrinsics. Their targets and face coverage
can differ even for the same source images. The focused M2 association test
disables both warp switches and scores original camera pixels; its metrics
must be kept separate from the warped workflow comparisons.

### Fisheye rendering can expose a calibration-domain failure

On H2's 981-image common cohort, voting alone produces almost identical
regressions with two training seeds: raw LPIPS-A rises from
0.397377 / 0.397428 to 0.414082 / 0.413950. Combined voting and guarded
completion gives 0.383320 / 0.383295. These use original pixels, 123 fixed
evaluation images, 15000 iterations, a common initialization cloud, and a
fourfold image reduction. On the same cohort with each reconstruction's own
cloud, first-seed LPIPS-A is 0.374293 for legacy, 0.390901 for voting,
and 0.360816 for the combination. Both the voting regression and the combined
policy's gain survive this initialization control.
Full-component controls keep the same 123 evaluation images but admit each
largest component's remaining registered cameras into training. Both seeds
retain the combined policy's gain:

| H2, native cloud and full main component | Training cameras | PSNR, seeds 1 / 2 | SSIM, seeds 1 / 2 | LPIPS-A, seeds 1 / 2 |
|---|---:|---|---|---|
| Legacy | 871 | 20.174 / 20.243 | 0.6954 / 0.6968 | 0.371650 / 0.371540 |
| Guarded completion only | 870 | 20.246 / 20.152 | 0.6963 / 0.6961 | 0.373020 / 0.372788 |
| Voting | 902 | 19.192 / 19.217 | 0.6796 / 0.6799 | 0.390662 / 0.390445 |
| Voting + guarded completion | 897 | 20.614 / 20.584 | 0.7098 / 0.7087 | 0.361985 / 0.360159 |

The extra registrations alone do not rescue voting. The combined policy has
fewer registered cameras than voting and better rendering on these fixed
evaluation views. All four policies initialize 500000 splats after the common
cap; legacy, completion, voting, and the combination respectively supply
642104, 647781, 653504, and 683308 input points.
Completion alone also loses LPIPS-A here, by 0.00131 on average, while its
PSNR/SSIM changes are near zero and change sign between seeds. Neither
individual intervention reproduces the combination's gain on this control.

The loss is concentrated in one THIN_PRISM_FISHEYE group with 21 evaluation
views. Its whole-image PSNR drops 5.45 dB and LPIPS-A rises 0.0994. Saved renders
show geometry filling the black area outside the lens circle. A post-hoc
radial check of the saved, quantized PNGs separates this from the scene
interior; radius is measured from the image center in units of half the
shorter image dimension:

| H2 group G7, first common-cloud seed | Legacy | Voting | Voting + guarded completion |
|---|---:|---:|---:|
| PSNR inside radius 0.8 | 23.846 | 23.375 | 23.844 |
| PSNR inside radius 0.9 | 23.344 | 22.577 | 23.374 |
| PSNR outside radius 1.0 | 21.853 | 11.735 | 21.639 |

These regions are a diagnostic, not replacement scores or a measured lens
mask. All three camera fits have similar focal lengths. The voted radial
polynomial turns at approximately 113.3 degrees, then turns back at 150.1
degrees. For example, a ray 160 degrees off-axis on the horizontal meridian
projects to normalized radius 1.714, inside the image. The local distortion
Jacobian check in [projection_utils.slang](../../src/shaders/projection_utils.slang) accepts this second
branch despite the intervening fold. The legacy and combined fits reject
that ray. This provides a concrete mechanism consistent with the border
artifact, although a controlled renderer fix has not been tested here.

Consequently, the full-image regression does not measure pose error alone.
It is still a failure of the resulting training workflow, and the central
region also loses some accuracy. A follow-up should constrain projection to
the branch connected to the optical axis, audit distortion over the observed
field of view, and preserve the physical lens mask. Restricting every fisheye
to 90 degrees would incorrectly remove valid wide-angle rays. No renderer
or calibration-domain policy is changed by these prototypes.

### Same-correspondence external mapper comparison

The M2 COLMAP comparison keeps 430 common cameras and 54 evaluation images,
uses the reduced-budget Spirula SIFT features and verified matches, and trains
for 15000 iterations on half-resolution original pixels. It compares legacy
Spirula with CPU COLMAP's default mapper and the extraction-scale-adjusted
pixel-threshold control described above:

| M2 mapper | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 | Native cloud PSNR | Native cloud SSIM |
|---|---|---:|---:|---:|
| Spirula legacy | 0.291742 / 0.291443 | 0.290167 | 18.810 dB | 0.77001 |
| COLMAP default | 0.272545 / 0.273585 | 0.273973 | 19.658 dB | 0.77999 |
| COLMAP scaled thresholds | 0.272658 / 0.274054 | 0.273892 | 19.678 dB | 0.78070 |

COLMAP default improves common-cloud LPIPS-A by 0.01853 on average, PSNR by
0.856 dB, and SSIM by 0.01005. Its native-cloud LPIPS-A gain is 0.01619, with
color-corrected LPIPS-A improving by 0.01645. The scaled control is similar.
The gain persists across both common-cloud seeds and native initialization,
so initialization count alone does not explain this mapper gap. Native runs
still have only one seed, and fitted evaluation cameras do not establish
absolute world-geometry accuracy.

This cohort and extraction budget differ from the high-preset default-policy
comparison. Their absolute scores cannot be subtracted to estimate a selected
default versus COLMAP delta. This experiment identifies remaining quality
headroom in the legacy mapper on one capture; it does not identify the
responsible COLMAP behavior or justify a wholesale replacement. The CPU
mapper takes 266.1 seconds with default thresholds and 291.3 seconds with
scaled thresholds on this input, with 435 uniquely registered images.
The comparison imports Spirula's legacy feature coordinates, correspondences,
and focal-prior flags, so it
also retains the input and prior-semantics limitations described earlier.

Bonsai gives the opposite result with the same-correspondence protocol. These
runs keep all 292 cameras and 37 evaluation images, use 7500 iterations and
quarter-resolution pixels, and compare against their own matched baseline:

| Bonsai mapper | Common cloud LPIPS-A, seeds 1 / 2 | Native cloud LPIPS-A, seed 1 |
|---|---|---:|
| Spirula legacy | 0.061424 / 0.061187 | 0.053239 |
| COLMAP default | 0.070823 / 0.070405 | 0.064466 |
| COLMAP scaled thresholds | 0.065351 / 0.064107 | 0.058561 |

COLMAP default worsens common-cloud LPIPS-A by 0.00931 on average and native
LPIPS-A by 0.01123. Its native PSNR is lower by 0.543 dB and SSIM by 0.00898.
Threshold scaling narrows the LPIPS-A losses to 0.00342 and 0.00532, but does
not reverse them. Native initialization has one seed. The common-cloud
control removes initial point-count differences, while the 7500-iteration
budget still leaves longer-run convergence unmeasured. COLMAP's longer mean
tracks do not predict a rendering advantage on this scene at this budget.

## Validation and interpretation

The final Vulkan build passes 22 native test executables with all research
overrides cleared, exercising the selected compiled defaults. The track-policy
fixture passes 19 checks, including an established-model guard below and at
its configured image threshold. Existing integration expectations were not
relaxed. The frozen build manifest records source hashes for the modified C++
files. The changes are confined to the Vulkan-only SfM subsystem and its CLI
path; trainer and CUDA-backend arithmetic are unchanged.

Fresh automatic-pipeline smoke reconstructions also pass with the compiled
defaults, normal feature compaction, and no experiment overrides: Bonsai keeps
all 292 cameras in one component, and H4s keeps 375 in one component. Full
pipeline times are 81.9 and 66.2 seconds respectively. These confirm the
production path; they are not additional rendering comparisons or isolated
mapper timing controls.
Both models have zero inconsistent track backreferences and zero nonfinite
projections at recorded observations. They still contain 1940 and 108 extra
same-image track observations respectively. Voting does not enforce landmark
uniqueness; the opt-in uniqueness prototypes address that invariant but
regress the learned-feature reconstruction controls. A successful coverage
smoke check is not a claim that every track is independent or every camera's
full projection domain is valid.

A disconnected-input control combines 80 Bonsai images with 80 DrJohnson
images. Legacy mapping produces separate components of 80 and 73 images.
Voting plus guarded completion keeps the scenes separate, but splits the
DrJohnson subset into overlapping 66- and 20-image components covering only
71 unique images. The refined-point/fusion-plus-completion control retains
the original 80/73 split. Preventing a false cross-scene merge therefore does
not establish that legitimate within-scene connections remain intact. These
visually different public scenes also provide a weaker false-merge challenge
than repeated rooms or facades.

The native tests cover the five-point solver, robust point gradients and
wide-FOV bearings, association ties and duplicate candidates, pair expansion,
spatial selection, physical SIFT localization, color/mask sampling, and
anisotropic coordinate scaling. Existing mapper, merge, rig, sequence, BA,
resume, feature-compaction, and global-mapper regressions are also exercised.
All six mapper/rig/sequence/merge regressions pass with post-pose deduplication,
which nevertheless regresses on the real ALIKED capture. These tests guard
implementation behavior; they do not establish a generally better default.
Cache checks verify that coordinate/grid changes invalidate extraction, while
verification/pair-expansion changes invalidate matching.
The SIFT localization check runs the actual extractor on 32 synthetic blobs
across sizes and subpixel phases; the corrected mean bias is about 0.0012
pixels per axis on this device. The learned-coordinate checks cover sampling
and anisotropic resizing. Downscaled splat scores are weak tests of such small
coordinate corrections, so absence of a rendering gain does not disprove the
physical convention. Coverage and topology regressions still matter when
deciding whether to change extraction defaults.
The five-point minimal-solver screen recovers the expected solution within
its accuracy threshold on 1498 of 1500 randomized exact cases and returns
4117 candidate matrices. The two misses remain a numerical limitation of the
prototype. Noisy/outlier RANSAC and planar/panoramic competition tests pass;
that success does not remove the cost or calibration-prior risks seen on real
captures.

Screening timings include association diagnostics. Production timing must
unset `SS_SFM_MAP_PROF`; assigning `0` still enables the existing
presence-tested switch. Device-memory samples include desktop use and are
taken every two seconds, so they are a coarse whole-device peak rather than
an exact per-process allocation measurement.

Driver-cache state is another timing confound. On M2, the untouched executable
takes about 20 seconds and a prototype with all policies disabled initially
takes about 29 seconds on the same inputs. Profiling attributes the difference
to Vulkan pipeline creation. All twelve embedded SfM SPIR-V blobs are
byte-identical between those executables. In a separate temporary cache, the
same prototype's BA initialization falls from 17.8 seconds cold to 2.6 seconds
warm under file tracing, with the same number of BA iterations. Untraced
warm-cache comparisons are recorded separately; traced runs and warmups must
not be pooled with ordinary reconstruction timings. Production cache settings
are unchanged. The vendor variables used for the isolated test are described
in [NVIDIA's driver documentation](https://download.nvidia.com/XFree86/Linux-x86_64/595.99.02/README/openglenvvariables.html);
their effect on these Vulkan runs was checked through file tracing.

The untraced M2 warm-cache controls use the same high-preset features and
verified matches, with profiling disabled. Different final image counts
also mean that the timed runs do different amounts of reconstruction work.

| Mapper policy | Registered images, two repeats | Mapper seconds, two repeats |
|---|---|---|
| Untouched executable | 423 / 423 | 20.18 / 19.60 |
| Prototype, legacy policies | 428 / 423 | 21.77 / 19.61 |
| Voting, defer ties | 430 / 428 | 18.68 / 18.48 |
| Voting + guarded completion | 428 / 428 | 23.03 / 24.03 |

For the repeated 423-image outcome, disabled prototypes match the untouched
executable's runtime. The apparent nine-second source-code regression does not
survive this cache control. These measurements do not establish a universal
speed ratio across captures or drivers.

Two other unprofiled high-preset timing controls retain the normal driver
cache. These are separate reconstructions from the rendering models above:

| Capture | Legacy images, repeats 1 / 2 | Legacy mapper seconds | Combined images, repeats 1 / 2 | Combined mapper seconds |
|---|---|---|---|---|
| Bonsai | 292 / 292 | 42.33 / 42.05 | 292 / 292 | 56.09 / 56.01 |
| H2 | 990 / 999 | 227.26 / 270.40 | 1005 / 1028 | 293.62 / 290.91 |

Bonsai's additional completion work costs about 33% more mapper time, with
unchanged registered coverage. H2's changed outcomes make a per-algorithm
speed ratio less useful. Across these repeats, peak process RSS is
1100 versus 1122 MiB for Bonsai and 2631 versus 2380 MiB for H2, taking the
larger measurement within each policy. Sampled whole-device peaks are
1097 versus 1104 MiB and 1776 versus 1773 MiB respectively. The isolated warm
M2 controls peak at 959 versus 960 MiB RSS and 834 versus 838 MiB device use.
No large VRAM increase is observed in these controls; the two-second device
samples are not a bound on short peaks or larger datasets.

## Next experiments

Further work should first isolate reconstruction repeat divergence and validate
the full fisheye projection domain, then add independent evidence for component
joins and targeted recovery of missing connections. Do not loosen global merge
thresholds based on a larger main component alone. The disconnected public
control avoids cross-scene joins but still loses legitimate within-scene
connectivity; it is weaker than a repeated-room false-join challenge.

The next experiments should have narrower success criteria:

1. **Repeat stability:** preserve byte-identical inputs, log the first
   diverging registration/refinement decision, and compare deterministic
   reductions/orderings against repeated current runs. H2's fresh legacy
   repeats already diverge before final sensor orientation. A fixed RANSAC
   seed or fp64 alone is not a reproducibility guarantee. Judge any fix by
   failure frequency, coverage and rendering, including runtime cost.
2. **Fisheye projection domain:** check the branch connected to the optical
   axis over observed angular support, and test the camera/trainer boundary
   with synthetic folded distortion. A locally positive Jacobian can accept
   a second branch after a radial fold. Preserve valid wide-FOV rays; clipping
   every ray beyond 90 degrees would discard legitimate observations. This
   requires coordinated camera/renderer validation and was not implemented.
3. **Component joins:** measure independent, spatially distributed evidence
   on both sides of each proposed join, retain reasons for refusal, and
   challenge acceptance with repeated-room/facade examples. Extra candidate
   pairs should target specific missing bridges. Larger main components,
   more points, or a lower fitted residual alone are insufficient success
   criteria. The current merge acceptance machinery remains in place.
4. **Matched mapper gap:** isolate calibration refinement, observation
   weighting, and track support on the same verified inputs used by the
   external-mapper comparison. Include the selected defaults and both seed
   clouds on one fixed cohort. The M2 rendering difference survives threshold
   scaling and native initialization; importing a bundle of COLMAP settings
   would not identify which behavior transfers to diverse captures.

## Reproducing mapper comparisons

Build through `bash build_develop.bash -DSS_BACKEND=vulkan`. Extract and verify
once in a dedicated workspace with the intended camera grouping and models:

```bash
build_vulkan/spirula sfm auto DATA/images -o WORK/input \
  --quality high --pairs prefilter --prefilter-neighbors 32 \
  --threads 4 --decode-threads 4 --no-compact-unused-features
```

Add the dataset's camera-model and manifest arguments to that command. The
initial reduced-budget screen also adds `--max-image-size 1600
--max-features 4096`; omit those overrides for the actual high preset.
Keep all other experiment variables unset. Explicit policy values make
comparisons independent of the executable's compiled defaults:

```bash
env -u SS_SFM_MAP_PROF -u SSPLAT_SFM_MAP_PROF \
  SS_SFM_TRACK_ASSOCIATION=0 SS_SFM_TRACK_COMPLETION=0 \
  build_vulkan/spirula sfm map WORK/input/matches.bin WORK/input/features \
  -o WORK/legacy/sparse --threads 4 --no-compact-unused-features

env -u SS_SFM_MAP_PROF -u SSPLAT_SFM_MAP_PROF \
  SS_SFM_TRACK_ASSOCIATION=2 SS_SFM_TRACK_COMPLETION=0 \
  build_vulkan/spirula sfm map WORK/input/matches.bin WORK/input/features \
  -o WORK/vote/sparse --threads 4 --no-compact-unused-features
```

Repeat the second command with completion `3` and another output directory
for the combined policy. These mapper commands omit runtime metadata; to
retain it, rerun `auto` with the same image directory and manifest, copied
matches, unchanged feature indices, and explicit `--reuse-features keep
--reuse-matches keep`. Count unique registered image names across every output
component, since overlapping components must not inflate coverage.

For rendering, hold the image cohort, evaluation names, resolution, iteration
count, trainer, and initialization protocol fixed. Use nonzero sampling seeds
and both common-cloud and native-cloud runs. Bracket groups must stay together
in the split, with `--ppisp-exposure-from-exif 1`; original-pixel fisheye
comparisons additionally use `--warp-to-pinhole 0
--warp-spherical-to-pinhole 0`. A comparison of differently warped targets is
a workflow comparison, not the same image-space experiment.

For example, the long public-scene comparison uses a prepared dataset with
the common camera cohort in `sparse/0`, images under `images`, and the aligned
common cloud in `seed.ply`:

```bash
build_vulkan/spirula train 3dgs --data DATASET --data-format colmap \
  --colmap-recon-dir sparse/0 --output-dir-prefix WORK/train \
  --output-dir-name run --num-iterations 30000 --steps-per-save 30000 \
  --cap-max 500000 --min-init-fraction 0.1 --train-resolution-divisor 2 \
  --eval-mode interval --eval-interval 8 --sampling-seed 1 \
  --seed-pointcloud DATASET/seed.ply --random-init never \
  --eval-lpips 1 --lpips-checkpoints MODEL_CACHE --save-eval-images 1 \
  --cache-images disk --load-depths 0 --load-normals 0 --load-masks 0 \
  --ppisp-exposure-from-exif 0 --disable-viewer 1 --keep-viewer-alive 0
```

Repeat with seed two; replace the seed cloud with `sparse` for native
initialization. The private mixed-camera comparisons use `360-camera` with
both warp switches disabled and their recorded resolution/iteration budgets.
Bracketed comparisons use EXIF initialization and a prepared filename split
that holds every exposure of each evaluation viewpoint together. The recorded
measurements use a frozen baseline trainer throughout; changing the trainer
creates a new experiment even when the command-line settings agree.
