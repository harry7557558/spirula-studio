# Native-resolution training memory plan

Status: implemented and Vulkan-verified. CUDA validation was not performed; a CUDA 13.4 build attempt stopped at the repository's existing CCCL 3 incompatibility in `IntersectTile.cu`.

## Goal and boundaries

Make high-resolution panorama training memory-bounded, with a clear refusal when
it cannot fit, while preserving requested RGB resolution and keeping CUDA and
Vulkan working. Native resolution fitting a particular GPU is a validation
outcome, not a promise.

- Do not reproduce the native-resolution crash on the 12 GB Intel Arc GPU.
- Use CPU-only sizing checks, small synthetic scenes, and bounded runs at known-safe resolutions.
- Do not silently lower resolution, drop supervision, or change the splat cap.
- Investigating the Windows bugcheck is separate from implementing trainer memory safety.

## Investigation baseline

- A 15,520 × 7,760 panorama becomes six 4,482 × 4,482 pinhole faces. Half
  resolution produces six 2,242 × 2,242 faces: approximately four times fewer pixels.
- All six uniform faces currently run together. The split/FPBO resolver considers
  input batch size, and the warped dispatcher splits input images and face-size
  groups rather than individual faces.
- Reference normals at 1,064 × 532 are expanded to full training-face resolution.
- With normals, PPISP, RGB/normal bilateral grids, and the configured loss maps,
  retained full-scale image buffers plus upload staging calculate to about
  16.95 GiB native versus 4.24 GiB half-resolution. Lower loss scales add about
  3.51 GiB versus 0.70 GiB. These are source-derived subtotals, not measured peaks;
  model, projection, sorting, and driver allocations are additional.
- Vulkan budget information is available for reporting, but allocation currently
  proceeds without a proactive budget check.

## Phase 1 — Memory-budget protection

Build a default-on circuit breaker with three layers: deterministic training
preflight, allocation-time enforcement, and coordinated abort. It prevents
predictable trainer overcommit and reports ordinary allocation failures cleanly;
it does not establish that GPU memory pressure caused the Windows bugcheck.

### Budget and error contracts

Extend the backend runtime API with an explicit budget snapshot containing query
status, available bytes, device identity, and Vulkan heap identity where
applicable. Keep these outcomes distinct:

- a deliberate budget refusal with requested and available byte counts;
- unavailable budget telemetry;
- a telemetry query/backend error, including a prior asynchronous CUDA error;
- an ordinary allocation failure after a successful check.

For CUDA, the snapshot is the current `cudaMemGetInfo` free amount. For Vulkan,
it is saturated `heapBudget - heapUsage` for the actual selected heap. Vulkan's
budget already includes this process, so tracked process bytes must not be
subtracted again. Existing process and category counters remain diagnostics, not
an allocation ledger.

Auto mode subtracts a fixed, validated safety margin from backend-reported
headroom. An optional GiB ceiling further limits Spirula's committed and reserved
bytes for reproducibility; it never expands or substitutes for live headroom. If
telemetry is unavailable or its query fails, protection refuses setup with the
distinct status rather than inventing a universally safe fallback. The normal
checked allocation path remains necessary after every valid budget check.

Carry refusal data through trainer, CLI, and GUI catches as structured fields.
Format user-facing text in the relevant i18n catalog; retain `what()` only as a
diagnostic fallback.

### Deterministic trainer preflight

Factor the real CPU batch/face planning into a pure shared helper and call it
after `load_dataset()` has resolved post-split dimensions and modalities. Place
the first check before output-directory/config creation so an intentional
refusal leaves no run artifacts. If host seed sizing cannot be resolved there,
perform a second check after target seed/resume adaptation but before
`set_data_3dgs()` or any `engine_init_*` call.

Estimate the known minimum from:

- input staging and actual post-warp face dimensions;
- effective image batches and face passes;
- enabled supervision, PPISP, bilateral grids, and loss-map scales;
- target splat layout and retained/preallocated capacities;
- the adapted resume target layout rather than the source checkpoint layout.

Report the result as a known minimum with an explicit unknown dynamic term.
Projection intersections, sorting scratch, pool high-water, and later
densification cannot be certified before running. Existing `VramCategory`
metadata can calibrate the estimator on safe runs, but must not be presented as
a preflight measurement.

### Allocation-time enforcement

Activate a scoped training-engine budget at the raw backend allocation boundary;
do not make an old global estimate reject unrelated viewer, mesh, `nn`, or `sfm`
allocations. After Vulkan selects the memory type, charge
`VkMemoryRequirements::size` to that type's heap, including host-visible memory
when it resides on a device-local heap.

The reservation lifecycle is:

1. Query headroom without holding allocation-map locks.
2. Under a dedicated budget lock, atomically reserve against both live headroom
   and the optional app ceiling.
3. Call the driver without holding accounting locks.
4. Commit the reservation on success or roll it back on failure.
5. Keep freed bytes charged until synchronization and reclamation complete, or
   track them explicitly as pending-free bytes.

Apply delayed credit to both CUDA and Vulkan; their current counters decrement
before the underlying free completes. Reusing retained capacity creates no new
reservation. Define one lock order and never hold allocation-map or budget locks
across `cudaMemGetInfo`, `cudaFree`, Vulkan budget queries, `vkAllocateMemory`,
or stream/device synchronization.

Inventory every raw allocation caller before enabling refusal. In particular,
convert Vulkan sort/scan `GrowBuffer` and every other silent-null path to
propagate failure. Clear stale pointer/capacity state before a throwing grow so
unwinding cannot leave freed storage appearing owned. Budget protection is not
complete while denied scratch can silently skip work.

### Coordinated abort

Reuse `TrainError`; do not add another GUI failure state. A setup refusal before
engine readiness can publish it directly. Failure after attachment needs a
one-shot teardown transition:

1. Under synchronization, set `_engine_ready` false and reject new render work.
2. Stop the web viewer outside `TrainRunner::_mu`.
3. Have the GUI thread detach and drain native viewport/comparison consumers.
4. Only after all render consumers are drained, reset the process-global engine
   under its established mutex.
5. Publish the structured, localized `TrainError`.

Do not rely on current `shutdown()`, which joins the training worker before
stopping the web viewer. The worker/UI handoff must not join while holding
`engine_mutex`, stop the viewer while holding `TrainRunner::_mu`, or free engine
state while a render worker can still enter it. CLI cleanup follows the same
engine invalidation/reset rules without the GUI detach step.

An interrupted step preserves the last completed checkpoint. Do not emergency
save partial optimizer state, automatically retry it, lower resolution, disable
supervision, or change the splat cap.

### Controls and acceptance

Add one next-run setting through `SS_CONFIG_FIELDS`: GPU memory limit,
Auto/optional GiB. Add all typed translations in `TrainFields.h`. Extend the
existing VRAM readout with tracked use, effective allowance, and preflight
minimum rather than adding another panel.

**Primary areas:** `src/backend/api/BackendRuntime.h`, CUDA/Vulkan runtime memory
queries and allocators, `src/backend/vulkan/SortScanVulkan.cpp`,
`src/core/Tensor.h`, `src/app/TrainerCore.cpp`, and GUI/CLI failure transport.

**Acceptance:** the verified 120 MP CPU-only case is refused before large
allocation; tiny injected budgets cover exact-boundary refusal, competing
reservations, rollback, delayed-free credit, missing/query-error telemetry, and
post-check driver failure. A forced refusal after setup drains renderers,
preserves the last checkpoint, and permits a subsequent clean small run. Normal
small runs remain unchanged.

## Phase 2 — True cube-face sub-batching

Add a warp-aware scheduler that processes uniform panorama faces in bounded
sub-batches. It may reuse existing gradient-accumulation primitives, but the
heterogeneous path currently rejects warped and multi-face batches, so merely
enabling it or changing the split/FPBO resolver is insufficient. Disable FPBO
whenever gradients must accumulate across passes.

Preserve post-split camera/grid indices, face transforms, loss weighting,
densification statistics, and one optimizer update per original training step.
Ensure staging and pool reuse bound retained memory, not just individual dispatch
sizes. Keep single-camera and mixed-face-size datasets working.

**Primary areas:** `src/engine/EngineDataManager.cpp`,
`src/engine/EngineTrainStep.cpp`, and `src/data/DataManager.cpp`.

**Acceptance:** small six-face split/unsplit runs agree within established parity
tolerances, and measured image-buffer capacity follows the face sub-batch rather
than all six faces. Changing `warp_face_fit` alone is not this fix.

## Phase 3 — Preserve reference-normal resolution

Use the existing mixed-resolution loss support to avoid unnecessarily expanding
low-resolution reference normals. Keep warp coordinates, face intrinsics, normal
rotation, validity masks, bilateral-grid processing, and gradient dimensions
consistent. Derived normals from rendered depth may still require render-sized
buffers; do not count them as automatic savings.

**Primary areas:** `src/engine/EngineSetupWarped.cpp`,
`src/engine/EngineLoss.cpp`, geometry bilateral-grid handling, and the CUDA/Slang
warp implementations if their contracts need changes.

**Acceptance:** analytic normal fields and missing-normal cases retain correct
geometry and gradients on both backends; safe-resolution runs demonstrate lower
reference-normal memory without disabling supervision.

## Phase 4 — Safe integration validation

- Add targeted checks for budget snapshots, refusal transport, reservation
  concurrency, rollback, delayed-free credit, six-face accumulation,
  mixed-resolution normals, and engine reset after failure. Follow
  [the testing conventions](../testing.md); use the development build scripts.
- Prove the deterministic estimator with CPU-only 120 MP sizing. Exercise runtime
  refusal only with deliberately tiny limits on small workloads; never run the
  dangerous native-resolution configuration.
- Obtain CUDA and Vulkan evidence, using a CUDA-capable machine/reference dumps
  where needed. Do not treat an unavailable backend as tested.
- Use existing `SS_PROFILE=1` reporting on bounded, known-safe runs. Hold quality,
  splat cap, losses, and camera settings constant. Compare allocated high-water
  capacity by category, not only logical tensor sizes; record splat counts,
  visible pairs, and tile intersections where available.
- Verify default color/normal supervision, mixed camera shapes, repeated runs,
  and a clean run immediately after forced failure. Full-resolution training on
  the affected machine is not an acceptance test.

## Execution order and completion

1. Define backend budget/query status and structured refusal types.
2. Correct CUDA/Vulkan reservation, rollback, and delayed-free accounting.
3. Inventory and fix every unchecked raw allocation failure.
4. Implement coordinated trainer, web-viewer, and native-renderer teardown.
5. Factor shared batch/face planning and add the two-stage known-minimum preflight.
6. Add Auto/custom configuration, translations, and the existing-panel readout.
7. Run CPU sizing and deliberately low-budget CUDA/Vulkan checks.
8. Implement and verify face sub-batching, then mixed-resolution normals.

Stages 1–4 are the safety boundary and must land before GPU smoke runs. Shared
runtime, trainer, and GUI files stay serialized. Face scheduling and normal
resolution follow after the guard is stable; their accumulation and warp
contracts overlap too much to implement independently.

Complete when unsafe training allocations are refused without silent kernel
skips, attached consumers cannot access failed engine state, a subsequent run is
clean, face sub-batching preserves training behavior, reference-normal memory is
reduced, and CUDA/Vulkan evidence supports each claim. Training-backend coverage
does not include the independent `nn`/`sfm` Vulkan allocators or system RAM;
integrate those separately before describing the policy as app-wide.

Reference: [existing splat/image memory analysis](vram-splat-x-img.md).
