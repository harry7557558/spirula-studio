# Native 120 MP processing on 12 GB GPUs: execution plan

**Status:** Early-state native Arc B580 CLI fit and desktop GUI crash-path smoke passed; mature one-million-live-splat demand remains unqualified.
Baseline: `79b3f6c9` (`trainer: bound native-resolution memory`).

This plan follows the investigation of native 15,520 x 7,760 panoramas. It extends,
not repeats, the implemented [memory-safety plan](native-resolution-memory-plan.md).
That document's investigation baseline describes the pre-fix code. The current
training defaults already split cube faces and preserve reference-normal resolution.

## Objective and completion contract

Process the target native-resolution dataset within a 12 GB device's **available
budget**, including training and enabled evaluation, without silently lowering RGB
resolution, dropping supervision, or reducing the requested splat cap.

Completion requires a qualified target configuration and measured success, not a
passing preflight, a small-scene benchmark, or a controlled refusal. The target
profile must state which reconstruction, masking, geometry, and GUI stages are
included; training results do not certify those independent stages.

Constraints:

- Keep the existing allocation-time circuit breaker, reserve, typed errors, and
  coordinated teardown. Do not raise the limit or bypass protection to make a run fit.
- Do not rerun the original dangerous native workload during investigation or early
  implementation. Native qualification requires the explicit safety gate below.
- Preserve native source files, camera geometry, enabled losses, output dimensions,
  model capacity, and one optimizer/densification update per original training step.
- Keep backend-neutral code portable and maintain both implementations when a kernel
  contract changes. CUDA validation is deprecated for this work: do not build or run
  it as an acceptance gate, and do not claim it was validated.
- Run GPU verification through Vulkan on the named device. A Vulkan run on an NVIDIA
  GPU is not CUDA validation.
- Do not describe the trainer budget as app-wide: independent `nn`/SfM allocators,
  GUI textures, and host RAM are outside its accounting scope.
- Do not add a second renderer, dataset parser, memory allocator, or configuration
  system. Reuse the existing face passes, pool slots, budget API, and typed catalogs.

## Verified baseline

### Implemented already

- Training defaults to `split_batch=true`; the data manager receives
  `max_faces_per_pass=1`.
- The split/FPBO resolver handles multiple face passes before its single-input-batch
  optimization. A single panorama does not disable its six face passes.
- Reference normals retain their own resolution through warping and loss gradients.
- Source RGB is staged as packed bytes and converted during warping, not expanded
  into a full-source float RGB image first.
- CLI evaluation remains inside the session's allocation-time budget lifetime.

### Resolved evaluation gap

Before Phase B, evaluation accumulated every rendered and reference image in
RAM before writing. Evaluation now advances one input and one face pass at a
time, performs ordered readback, and caps encoding concurrency.

Relevant entry points:

- `src/app/TrainerCore.cpp`: `TrainerSession::eval`, `estimate_training_memory`.
- `src/engine/EngineDataManager.cpp`: `_install_and_forward`,
  `engine_eval_forward`, `engine_preview_forward`, `_resolve_split_vs_fpbo`.
- `src/data/DataManager.h` and `src/data/DataManager.cpp`: face-pass configuration,
  `build_face_passes`.

### Measurements from the investigation

The following `SS_PROFILE=1` results are **retained pool capacities**, not a
continuously sampled total-driver peak. The synthetic scene used 4,096 live splats,
`cap_max=1000000`, preallocation enabled, a 256 x 128 reference-normal image,
PPISP and bilateral grids enabled, two training steps, and a 3 GiB application
limit. Temporary datasets/checkpoints were removed after measurement.

| Source / scenario | Device, Vulkan | Splat MiB | Splat x image MiB | Image MiB | Appearance MiB | Pool MiB |
|---|---|---:|---:|---:|---:|---:|
| 2048 x 1024, training | RTX 3060 | 371.74 | 0.09 | 35.54 | 8.86 | 416.30 |
| 4096 x 2048, training | RTX 3060 and Arc B580 | 371.74 | 0.13 | 141.77 | 32.93 | 546.73 |
| 4096 x 2048, three loss scales | Arc B580 | 371.74 | 0.13 | 173.56 | 32.93 | 578.52 |
| 4096 x 2048, training then held-out eval | Arc B580 | 371.74 | 4.15 | 382.42 | 32.93 | 791.40 |

Small arena/other terms and rounding account for category-total differences. The
2048 and 4096 inputs produce 592 x 592 and 1184 x 1184 training faces respectively,
with one active face per training pass. The evaluation case has one training input
and one held-out input; it renders six held-out faces in one forward call.

During evaluation, `gt.rgb` and `renders.rgb` each grew from 16.04 to 96.26 MiB.
The extra five faces' RGB/GT/depth/transmittance/ID buffers cost
`5 * 36 * 4482 * 4482` bytes, approximately **3.37 GiB**, at the native reference
size. This is source-derived arithmetic, not a native GPU measurement.

Other established evidence:

- CPU `training_memory_preflight` passed and printed 23.01 GiB unsplit / 4.68 GiB
  split. Its synthetic case has no supplied mask/depth/normal modalities; these
  figures are estimator outputs, not guaranteed lower bounds or fit certificates.
- The small `engine_split_faces` Vulkan check passed one/two-step split comparisons
  and injected split-failure recovery.
- Initial NVIDIA inspection reported 12,288 MiB total and 3,107 MiB occupied. Device
  occupancy varies; that snapshot is not the Vulkan heap budget for another run.
- Native 120 MP training, mature-scene densification peaks, and desktop teardown
  were not exercised. The failed scene's exact cap and overlap remain unqualified.

## Execution order

Run the smallest verified fixes first. Region processing is a conditional capacity
phase, not an excuse to skip the target-resolution objective. It may be omitted
only when the earlier changes meet the final qualification criteria.

### Phase A — Freeze the target and measurement contract

- [x] Recover the intended run configuration from existing config/checkpoint/log
  artifacts: preset, resolved `cap_max`, seed/resume size, preallocation, SH degree,
  quantization, source/face dimensions, mask/depth/normal dimensions, PPISP, grids,
  loss modes/scales, batch policy, held-out evaluation, and GUI usage.
- [x] Record the preprocessing stages and their actual options. Defaults use bounded
  inference resolutions; raised geometry/SfM limits and native GUI textures must
  be assessed separately. Obtain missing target choices from the user only after
  available artifacts have been exhausted. Do not substitute the one-million-splat
  control configuration for the requested workload.
- [x] Recreate a deterministic small equirectangular fixture with a distinct signal
  on every face and a held-out image. Keep original image dimensions explicit and
  use an existing test location rather than a new benchmark framework.
- [x] Capture setup, first forward/backward, warmed steps, and a small controlled
  densification transition using existing profiling/budget APIs. Record pool
  category capacities, committed/reserved bytes, driver budget/headroom, maximum
  active face count, visible-pair count, intersection count, and wall time.
- [x] Separate retained high-water totals from temporary growth overlap and actual
  driver usage. If existing reporting misses a deciding peak, add the smallest
  diagnostic at the existing allocation/phase boundary; do not infer it from the
  final pool snapshot.

**Gate A:** the control case is reproducible and bounded. The exact target profile
is recorded, or native qualification is explicitly blocked on missing configuration.
Use deliberately low limits; no full-resolution GPU experiment is authorized here.

#### Recorded target and Phase A evidence

The user selected the recovered saved **medium / 3dgs / 1,000,000-splat cap /
30,000-step** configuration, not the older 200,000-splat run. Its config SHA-256 is
`12fce282c7c3bd9479b8a514a718c828d33b5b8e9bb277fcd269b3dfd89476f5`.
Qualification includes the original `eval_mode=all` profile and a separate
`eval_mode=interval`, `eval_interval=8` variant at unchanged native resolution and
capacity. It includes training and the GUI using the existing reconstruction and
normal maps; reconstruction, masking, and geometry inference are excluded.

- Source RGB: 15,520 x 7,760; uniform faces: 4,482 x 4,482. Existing normals:
  1,064 x 532. No mask or depth inputs. The latest resolved dataset log reports
  59 input cameras / 354 faces; the sparse point artifact contains 36,063 points.
- Preallocation on; SH3; quantization level 1; packed projection; split batches.
  Six face passes resolve the requested fused projection-backward optimizer off.
  Adaptive loss threshold 1,920 resolves three scales at the native face size.
- Normal supervision 0.01; normal regularization 0.04 with 6,000-step warmup;
  SSIM 0.2 and L1 1. RGB PPISP grid 16 x 16 x 8; geometry grid 8 x 8 x 4;
  PPISP `no_crf_no_vig`; appearance AdaGrad; per-splat bias correction.
- Existing preprocessing metadata records builtin high-quality individual-image
  reconstruction, equirectangular/folder cameras, flat mapper, SIFT/bruteforce,
  and no masking. The latest GUI run did not rerun geometry inference.
- A mature `step-000015000.ckpt` records the target SH3/q1/fused layout, but
  its manifest has `full_resume: 0`; it is layout evidence only and cannot
  exercise resume adaptation. No mature full-resume checkpoint was found.

The `engine_split_faces export-fixture <empty-directory> [width]` control uses
256 x 128 RGB, 64 x 32 normals, six 74 x 74 faces, one training input, two distinct
held-out inputs, 64 seeds and capacity 512. Widths above 4,096 and nonempty output
directories are refused. Six real trainer steps grow 64 -> 80 splats at step 2
and reach 100 by completion. The application limit is 1 GiB with the unchanged
256 MiB reserve.

Windows Vulkan build, the extended engine check, and CLI train-to-eval completed
on RTX 3060 (driver 616.56) and Arc B580 (driver 101.8992). These measurements use
`SS_MEMORY_TRACE=1` and `SS_PROFILE=1`; cold pipeline compilation affects timings.

| Vulkan device | Setup / first / warmed / densify / eval, ms | Maximum accounted + reserved, MiB | Maximum sampled heap usage, MiB | Minimum sampled available, MiB |
|---|---:|---:|---:|---:|
| RTX 3060 | 10 / 43 / 19 / 20 / 34 | 2.630 | 12.980 | 11327.020 |
| Arc B580 | 33 / 2109 / 875 / 243 / 458 | 2.632 | 164.266 | 11185.672 |

Both controls emit 36 one-face training forwards and two six-face eval forwards.
Maximum visible pairs/intersections are 100/140 in training and 111/131 in eval;
bin size is 32 or 64 pixels. Retained image capacity grows from 0.56 MiB in
training to 1.50 MiB after eval; appearance remains 0.91 MiB and splat state
0.19 MiB. The largest individual reservation is 0.422 MiB.

Allocation/reservation events capture accounted growth overlap, including the
old allocation while its replacement is reserved. Heap usage is sampled at
existing budget queries, **not a continuously observed driver peak**. Neither
quantity is the retained pool total or a native-fit certificate.

The baseline also exposes a correctness defect for Phase B: the Arc CLI run
wrote 12 result slots but repeated all six GT/render pairs from its first input.
The training scheduler is not an ordered evaluation cursor. A result count alone
does not establish that both held-out inputs were scored; the bounded reference
must fetch each input explicitly before comparing face-pass outputs.

### Phase B — Make evaluation face-bounded

Primary ownership: `src/app/TrainerCore.cpp`, `src/engine/EngineDataManager.cpp`,
`src/engine/Engine.h`, `src/backend/tests/engine/engine_split_faces.cpp`.

- [x] Set evaluation's face-pass cap to one while retaining its existing uniform
  camera fit and per-face metric definitions.
- [x] Make evaluation iterate a decoded input's passes before advancing to the next
  input. Reuse the pass-aware preview/forward mechanism where applicable; do not
  duplicate `_install_and_forward` or merely remove the multiple-pass rejection.
- [x] Read back each completed face immediately. Preserve face ordering, indices,
  image dimensions, per-face PNG names, scoring, and metrics aggregation. Do not
  emit one PNG per region or add an equirectangular stitching feature.
- [x] Keep the budget active through evaluation and preserve normal failure/reset
  handling. Avoid concurrent forwards against the process-global engine.
- [x] Extend the existing engine check with a bounded all-faces reference and a
  pass-wise result. Cover all six distinct faces, consecutive inputs, exhaustion,
  and refusal followed by a clean run. Also exercise the CLI train-to-eval path.

**Gate B:** all faces are produced exactly once and agree with the old bounded
reference within established tolerances. Eval-only GT/render capacities follow
one face instead of six; the matched profile no longer shows their sixfold growth.
Report the actual category delta rather than requiring an exact historical MiB total.

#### Phase B evidence

The Windows Vulkan build and extended engine check pass on both devices. After
six training steps and densification to 100 live splats, all 12 GT and render
buffers match the same model's bounded all-faces reference with maximum absolute
difference zero. The check covers distinct consecutive inputs, all six local face
indices, repeated exhaustion, typed allocation refusal, and a real PNG-write
failure followed by engine reset and a clean evaluation.

The CLI train/checkpoint/eval path also passes on both devices: 12 metric rows and
24 correctly named 74 x 74 PNGs. Evaluation now visits inputs in dataset order;
the old RTX run reversed the two input groups and the old Arc run repeated one.
Within-input face order is unchanged. New RTX GT PNGs match the old RTX image set
exactly after correcting that permutation, and new Arc GT PNGs match new RTX.
Scoring formulas and aggregation are unchanged; metrics from separate training
runs are not used as same-state parity evidence.

The matched control emits 36 one-face training forwards and 12 one-face eval
forwards. Retained image capacity stays at 0.56 MiB rather than growing to
1.50 MiB: a measured 0.94 MiB reduction. Maximum accounted bytes including
reservations fall from 2.630 to 1.692 MiB on RTX and 2.632 to 1.695 MiB on Arc.
These are bounded-control results, not native or continuous driver-peak claims.

An existing Arc recovery comparison showed 1.35e-6 maximum roundoff across
repeated runs. Its splat absolute tolerance is now 1e-5 with the existing
1e-6 relative-RMS gate retained; the new evaluation comparisons are exact.

### Phase C — Align preflight with resolved allocation layouts

Primary ownership: `src/app/TrainerCore.cpp`, `src/app/TrainerCore.h`,
`src/engine/EngineLoss.cpp`, `src/core/Tensor.h`,
`src/backend/tests/training_memory_preflight.cpp`.

Inspect `src/backend/vulkan/kernels/PerPixelLoss.cpp` for the actual scale allocation
contract; change it only if sharing sizing logic requires a narrowly scoped change.

- [x] Derive effective loss scale count from face dimensions and the configured
      threshold.
- [x] Size only tensors enabled by the resolved loss and appearance paths, at
      their actual per-scale dimensions.
- [x] Size model storage from target capacity, SH degree, quantized value/state
      layout, fused-mode resolution, bias state, and the temporary startup
      layout.
- [x] Separate retained allocations, deterministic transition allowance, and
      explicitly unknown dynamic intersection/driver terms.
- [x] Keep raw allocator enforcement authoritative for unbounded dynamic demand.
- [x] Add CPU-only regression coverage for layout thresholds, quantized model
      storage, startup overlap, heterogeneous packed steps, synthesized masks,
      fixed-key densification scratch, and exact refusal boundaries.

**Gate C:** CPU sizing reflects the selected layouts and reconciles with bounded
allocation evidence, with residual unknowns named. Existing budget/refusal checks
remain green. This phase improves decisions; it is not itself a VRAM reduction.

Evidence (2026-09-11):

- `training_memory_preflight` passes scale, mixed-modality, quantized-layout,
  startup-overlap, packed-step aggregation, synthesized-mask, fixed-key
  densification-scratch, overflow, and one-byte refusal checks.
- `checkpoint_adapt` passes fused/cell q8 Adam and q16 value-layout adaptation
  checks.
- The Vulkan development build completes with generated-source and comment
  checks enabled.
- The exact target estimates 3,556,367,580 retained bytes plus a 361,305,600
  byte transition allowance: 3,917,673,180 bytes (3.649 GiB) total. The
  interval/8 variant estimates 3,914,842,140 bytes (3.646 GiB).
- Both estimates retain `dynamic_unknown = 1`; neither is a native-run claim.

### Phase D — Select the remaining reduction from measured categories

- [x] Repeat matched bounded profiles for single-scale, three-scale, evaluation,
      fresh startup, resumed startup, and controlled densification.
- [x] Reconcile measured pool high-water with the corrected retained estimate
      and deterministic transition allowance.
- [x] Remove no image/loss workspace: the measured three-scale delta matched the
      estimator and exposed no duplicate retained allocation.
- [x] Keep existing binning: splat-times-image/intersection high-water remained
      0.05 MiB or lower in the bounded controls.
- [x] The authorized early-state native run fit, and the one-million-cap retained
      estimate fits the guarded budget. Phase E remains conditional on a measured
      mature-state failure.
- [x] Keep persistent state untiled: the one-million-splat q1 model high-water
      was 371.67 MiB and was not the limiting term.
- [x] Observe the native GUI training path on the actual desktop through the
      previous failure point; no separate GUI failure appeared during the
      accepted smoke window.
- [ ] Measure GUI texture/preview VRAM separately only if a future run identifies
      it as the limiting stage.

Evidence (2026-09-11):

- On RTX 3060, pool high-water was 549.74 MiB for single-scale evaluation,
  581.72 MiB for three-scale evaluation, 582.86 MiB for fresh three-scale
  training, and 582.30 MiB after full-checkpoint resume.
- The corrected retained estimates were 564.45, 596.83, and 597.50 MiB,
  respectively. Their 172.08--172.93 MiB allowances cover startup replacement
  overlap rather than pretending to bound dynamic intersections or driver use.
- On Arc B580, the three-scale/evaluation run completed six training steps and
  all 12 held-out face views at 581.72 MiB pool high-water; a full-checkpoint
  resume completed two further steps at 582.30 MiB.
- Driver records: NVIDIA 616.56 on RTX 3060 and Intel 101.8992 on Arc B580.
- No measured retained allocation justified deletion or coarser binning.
  Phase E is therefore not triggered before an authorized native run.

**Gate D:** record the selected change, expected bytes removed, measured bytes
removed, and remaining uncertainty. Do not approve or reject Phase E solely from
preflight figures or the low-overlap control scene.

### Phase E — Conditional native-resolution region processing (not triggered)

Execute only when an authorized native profile shows that one face cannot fit.
If triggered, settle the region contract before this coordinated engine change.

#### E1. Region contract and forward path

Primary ownership: `src/data/DataManager.h`, `src/data/DataManager.cpp`,
`src/engine/EngineTrainStep.cpp`, `src/engine/EngineSetupWarped.cpp`,
`src/engine/EngineDataManager.cpp`.

- [ ] Extend the existing pass metadata with parent face identity/dimensions, region
  origin, scored interior, and required support bounds. Keep logical camera/grid
  identity separate from the temporary render extent; do not invent new cameras
  or appearance parameters for regions.
- [ ] Plan non-overlapping scored interiors at native pixel density. Derive support
  margins and pyramid-grid alignment from the complete forward/backward operators;
  do not assume a guessed SSIM radius alone is sufficient.
- [ ] Reuse the pinhole-face renderer with correctly shifted pixel coordinates.
  Preserve full-face semantics for projection gradients, screen-size thresholds,
  regularizers, and any camera-dependent quantities that must not shrink with the
  region. Validate every affected primitive, not only RGB projection.
- [ ] Warp only the required output support using the existing source sampling and
  panorama seam handling. Keep packed-source staging and low-resolution GT where
  appropriate. Do not allocate a full float panorama as an intermediate.
- [ ] Count and budget visible records/intersections before allocating large
  buffers. Subdivide unexecuted work when necessary; retain the allocator refusal
  for competing external pressure. Do not retry a partially updated step.
- [ ] Reuse region-sized pool slots across passes. Account for grow overlap and
  high-water retention: shorter dispatches alone are not a memory fix.

A 1024 x 1024 region is a useful test size, not a new hardcoded universal policy.
It contains about 19 times fewer pixels than a 4482 x 4482 face before support
margins; only region-sized arrays receive that reduction, not total VRAM.

#### E2. Loss, appearance, and update semantics

Primary ownership: `src/engine/EngineLoss.cpp`, `src/engine/EngineBilagrid.cpp`,
`src/engine/EngineBackground.cpp`, `src/backend/vulkan/kernels/PerPixelLoss.cpp`,
`src/backend/vulkan/shaders/ppisp_image.slang`. Main owns integration into the E1
scheduler. Inventory matching CUDA/Slang launchers before changing a shared contract.

- [ ] Resolve loss scales from full-face dimensions and preserve each modality's
  original sampling lattice, masks, validity, and normal rotation.
- [ ] Evaluate losses on the scored interior, with sufficient rendered support for
  their derivatives. Preserve global face normalization and original face weights,
  including partial edge regions; averaging region means is not equivalent.
- [ ] Preserve face-wide depth statistics, quantiles, robust losses, and color
  regularizers. Reuse exact existing reductions; budget compact full-face maps or
  use a staged reduction/recompute approach where necessary. Do not substitute an
  approximate histogram or local statistics without an explicit contract change.
- [ ] Reuse existing patched bilateral samplers where their scalar and
  camera-index contracts match. Keep PPISP and background coordinates tied to
  the logical face, and accumulate appearance gradients without double-counting.
- [ ] Preserve masks, depth, normals, all enabled loss modes, and primitive behavior.
  Unsupported combinations must be resolved, not silently routed to an unsafe
  full-face allocation or disabled to obtain a smaller result.

#### E3. Shared consumers and proof

- [ ] Use the bounded forward path for evaluation and any qualified preview/export
  consumer. Assemble regions into the existing per-face host output contract;
  preserve output resolution, names, metrics, and ordering.
- [ ] Compare one-region and multi-region results on deterministic small scenes:
  rendered pixels, losses, parameter updates, appearance updates, densification,
  and repeated steps. Include seams, partial edge regions, empty visibility,
  mixed-resolution GT, global-statistic modes, and enabled primitives.
- [ ] Exercise a small controlled densification event and failure after partial
  accumulation; verify reset/recovery and last-completed-checkpoint preservation.
- [ ] Measure region-area scaling of image/appearance capacity and actual maximum
  intersection demand. Demonstrate that no consumer restores a full-face/six-face
  allocation behind the scheduler.

**Gate E:** semantic comparisons pass and the measured workspace is bounded by the
chosen region policy plus explicitly budgeted global state. A passing crop-render
check without loss/update parity is insufficient.

### Phase F — Qualification and completion

- [x] Run the Windows development build entry point (`build_develop.bat
  -DSS_BACKEND=vulkan`) and the affected targeted executables after integration.
  Reuse `training_memory_preflight`, `engine_split_faces`, `memory_budget`,
  `engine_reset_state`, and relevant warp/loss/bilateral/render checks; extend them
  only where the new behavior creates a plausible regression.
- [x] Run bounded end-to-end CLI train/eval/resume cases on both the Arc B580 and
  RTX 3060 via Vulkan. Record exact configurations, device/driver identity, phase
  measurements, pool categories, budget inputs, instantaneous device usage, and
  timing. Do not sum category maxima and describe that sum as an observed
  simultaneous driver peak.
- [x] Run the affected native training path on the actual desktop through the
  previous failure point and monitor it for another five minutes. The user ended
  the check there; full-run completion and the remaining compare/refusal cases
  were not required for this qualification.
- [x] **Native-test authorization gate:** Arc B580 was explicitly approved after
  bounded/refusal/recovery checks and the reconciled memory envelope passed.
- [x] Run an early-state native input fit check: setup, forward/backward,
  controlled densification, enabled evaluation, full-checkpoint output, and a
  resumed step all completed without reducing input resolution, supervision,
  SH degree, quantization level, or the one-million-splat configured cap.
- [x] No early-state native gate refused. The available mature checkpoint is not
  resumable, so mature one-million-live-splat visibility/intersection demand is
  not claimed.

Evidence (2026-09-11, Arc B580, Intel 101.8992):

- Early-state fresh interval/8 smoke: one native step in 9 seconds at 36,063
  live splats, 48 held-out face views, a full checkpoint, 3,349.07 MiB pool
  capacity, and 4,278.35 / 12,118 MiB instantaneous driver use.
- Early-state full-checkpoint resume: one further native step, 48 held-out face
  views, and another checkpoint at 3,350.48 MiB pool capacity and
  4,279.68 MiB driver use.
- Early-state controlled native densification used the same data, layouts,
  losses, and cap with only the schedule advanced for the check. It grew
  36,063 to 37,866 splats in two steps and reached 3,353.55 MiB pool capacity
  and 4,282.98 MiB driver use.
- Desktop GUI smoke: the user observed the native run past the previous crash
  point; the process then remained running for another five-minute monitoring
  interval, reaching 7 minutes 57 seconds of process uptime. The run was
  intentionally not allowed to finish.

Completion is earned only when the qualified native workload completes within the
available device budget without reduced RGB resolution, supervision, or model cap,
all affected consumers retain their output contracts, and refusal/recovery remains
correct. Do not claim arbitrary splat capacities or untested custom preprocessing
resolutions fit simply because one profile does.

## Orchestration and ownership

1. Before writing waves, record branch/status/stashes. Main retains ownership of
   pre-existing dirty paths, shared contracts, integration, and validation. Do not
   commit unless requested.
2. Phase A can investigate measurement plumbing and target preprocessing/config
   independently. Keep GPU measurements serialized to avoid contaminating peaks.
3. Phase B and C share `TrainerCore.cpp`: serialize their writes. Independent test
   design/review may run concurrently, but do not have lanes negotiate conflicting
   production APIs after editing starts.
4. After Gate D, dispatch only reductions justified by the measured category gap.
   A preview-only change may run independently of an engine-only change when both
   are required by the target profile and their files do not overlap.
5. For Phase E, Main defines the region/loss/update contracts first. E1 establishes
   metadata and scheduler interfaces; E2 can then consume those interfaces in
   disjoint files. Main serializes shared scheduler mutations and final integration.
6. Use isolated worktrees for writing lanes and keep the parent checkout read-only
   until merges settle. Assign at most four concurrent agents, no nested delegation,
   explicit target files, observable acceptance criteria, and abort/report on an
   unresolvable gap. Subagents skip builds/tests/formatters; Main runs the applicable
   smoke checks and gates over the integrated changes.
7. Never close a phase solely from a lane's completion report. Verify its outputs,
   run the phase gate, resolve failures, and then advance. Restore user WIP and
   resolve any unexpected stash/merge state before final delivery.

After successful smoke verification, update the existing memory notes with measured
results and remaining limits, remove temporary experiment scaffolding, and check
that this plan's status matches the evidence. Do not reopen completed safety work
or introduce unrelated allocator/inference refactors as cleanup.
