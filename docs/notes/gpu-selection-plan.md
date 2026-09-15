# GPU selection: native workflows first

Status: native routing implemented; acceptance matrix partially verified; external compatibility tail deferred.
## Current implementation status

Phase A-C native routing is integrated. Vulkan selection resolves one canonical
UUID per process and carries it through built-in SfM, SAM/masking, geometry,
training, rendering, mesh children, and GUI workers. CUDA training remains a
separate ordinal selection, rebound on each CUDA worker thread.

The resolver/lifecycle smoke passed, including failed-init recovery and a real
NN upload/download round trip. Targeted MSVC C++17 compiles passed for the
Vulkan and CUDA GUI/CLI guards. Real SfM, SAM, geometry, training, and desktop
GUI acceptance remain unverified because this checkout has no model weights or
bounded capture fixtures and no GUI launch was performed.

The external compatibility tail is intentionally not implemented. COLMAP is
not installed, the external Python masking dependencies are unavailable, and
the installed ffmpeg path has no application-level GPU routing in this plan.
Do not describe external COLMAP/Python work as honoring the native selector.

## Goal and scope

Make the selected GPU control **built-in SfM, masking, and geometry**, in the GUI and native CLI. Selection means the physical device that executes the work, not merely the name shown by the training picker. Complete and verify this native milestone before implementing external COLMAP/Python routing.

| Native milestone | Required coverage |
|---|---|
| Built-in SfM | SIFT, brute-force matching and pair prefiltering, ALIKED/LightGlue, LoMa extraction/matching, mapper/BA, merge/refinement, bottom-up workers; GUI in-process and self-child modes plus CLI entry points |
| Masking | SAM 2 and SAM 3; GUI preview and dataset masking; native segment, track, mask, extract, and GPU-backed video paths |
| Geometry | MoGe-2 and Metric3D v2; GUI preview, dataset-run child process, and CLI |
| Supporting integration | Existing Vulkan training choice, early preview/capability probing, native decoder sharing of NN, and runtime teardown/recreation |
| Separate compatibility tail | External COLMAP and the existing external Python masking path; their flags, device namespaces, visibility rules, and fallback policy |

Not included: multi-GPU execution, one GPU assignment per stage, hot switching, merging the three Vulkan runtimes, model/kernel changes, a new Python dependency, a new recovery system, or CUDA validation. Keep patented decoding disabled by default. External ffmpeg hardware-acceleration routing is not added by this plan; preserve the existing ffmpeg fallback.

## Current behavior and concrete gaps

The implementation sources, rather than older architecture plans, establish this baseline:

| Area | Evidence and consequence |
|---|---|
| GUI picker | `src/app/gui/GuiApp.cpp`, `draw_train_settings` (around 5473–5503), calls only `backend::device_select(i)`. It lives on the training screen, has no Auto row, and does not save a choice. `launch_training` sets `_device_locked` around line 901, after preprocessing can already have used a GPU. |
| Backend Vulkan | `src/backend/vulkan/VulkanContext.cpp`, `enumerate_devices`, `resolve_device_index`, and `device_select`, use raw Vulkan enumeration ordinals, including disabled unusable rows. Precedence is explicit backend selection, then `SS_VK_DEVICE`, then automatic ranking. The ordinal is assumed to remain stable between separate instances. |
| NN | `src/nn/vk/Context.cpp:299–308,342–403` owns a lazy process singleton. Numeric `SS_VK_DEVICE` overwrites an explicit option index; a name environment value overwrites the option name but does not defeat a nonnegative option index. Once initialized, later options are ignored. The three native selectors do not share an identity or a single automatic decision. |
| NN failure path | `Context::get` assigns `g_ctx` before calling `init(opts)`. An initialization exception leaves a non-null, partially initialized singleton, and the next call returns it. This must be corrected as part of making selection errors recoverable. |
| SfM | `src/sfm/SfmConfig.cpp:471–476` fans an integer device into all stage options. `src/sfm/vk/VkContext.h`, `choosePhysical`, independently uses that ordinal or the first discrete device; it does not read `SS_VK_DEVICE`. |
| Learned SfM | `src/sfm/feature/Extractor.cpp` and `LearnedMatcher.cpp` load ALIKED/LoMa/LightGlue through NN without applying their copied device fields. Setting SfM `--device` therefore does not select the learned models' GPU. |
| BA selection errors | `src/sfm/ba/Solver.h`, `cachedDeviceCaps` and `BundleSolver::init`, cache capabilities by ordinal. A failed device lookup can become empty capabilities and then a CPU arithmetic fallback, rather than a selection error. |
| Masking | `src/sam/Masking.cpp:176–182` places `MaskOptions.device` into `ModelParams.device_match`: a numeric track selector becomes a name substring. `sam_main.cpp` separately parses segment selectors and does not apply its video selector; `sam_extract.cpp` instead changes the process environment before decoding. These are different implementations of the same option. |
| Native job propagation | `src/app/gui/SfmRunner.cpp`, `recon_args`, omits `--device`; the same arguments feed `SfmInProcess.cpp` and the self-child path. `GeometryRunner.cpp` also omits it. `SegmentPanel`, `DatasetPrep`, and `GeometryPanel` do not receive the training choice. |
| Premature first use | With `SS_HAVE_VIDEO`, `DatasetPrep::backends()` caches a call to `VideoPipeline::availability()`, which creates the NN context. Video preview/open can also use NN before model loading. Merely presenting preprocessing capabilities can therefore consume the first-use decision. |
| Teardown | `nn::shutdown()` destroys stream, pipelines, pools, allocations, and context. Dataset preparation invokes it at job end; preview panels can retain the context after unloading weights. A later context generation currently selects afresh. The training backend has a different, process-lifetime context. |
| External processes | `Subprocess.cpp` uses an inherited environment (`CreateProcess` with a null environment block, or `execvp`). `ColmapRunner` and the Python masking launch in `DatasetPrep` provide no explicit GPU routing. COLMAP's existing `ba_use_gpu` boolean is not a device selection. |

`backend::DeviceInfo`, `nn::DeviceInfo`, and the private SfM context expose no device UUID today. In a CUDA build the existing training picker enumerates CUDA devices, not Vulkan devices; its integer must never be forwarded as a native Vulkan index.

The current GUI settings, training recovery marker, checkpoint configuration, and batch rows do not carry a GPU request. Do not treat them as an already-implemented selection store.

## Decisions for this plan

### One native choice, not a scheduler

- Expose one **Native GPU** choice in the GUI's shared settings surface, reachable before opening a masking/geometry preview or starting dataset preparation. Move/reuse the existing Vulkan picker rather than introducing three independent pickers.
- In a Vulkan build, the existing training selection and native workflow selection must agree. Training behavior is integration scope, not a trainer redesign.
- If native modules are enabled in a CUDA build, keep CUDA training selection explicitly separate from Native GPU selection. Native Auto applies to Vulkan, not CUDA: `BackendRuntimeCuda.h` exposes `cudaGetDevice`/`cudaSetDevice`, without Vulkan's requested/current state or post-context switch guard. Do not infer a Vulkan device from a CUDA ordinal or introduce a CUDA Auto policy. Cross-API unification is not required for the native milestone, and no CUDA validation work is added.
- Keep the current session-level persistence scope: no new saved GPU preference, per-dataset assignment, or GPU recovery file. The user may choose another execution GPU before resuming a checkpoint. If independent recovery work introduces an atomic job state, carry the same request there as an execution option, re-resolve it on recovery, and never add a second marker or make a checkpoint hardware-bound.
- Freeze the choice before dispatching the first GPU-using job or preview. Disable editing thereafter and explain that changing it requires restarting the application. NN shutdown, model unload, cancellation, or a completed child process does not unfreeze the application's choice.

### Request, identity, and precedence

Use one small shared request/resolution implementation, not a device-manager framework:

1. **Precedence:** an explicit CLI option or GUI choice, then nonempty `spirula::env("VK_DEVICE")`, then Auto. An explicit Auto choice also overrides the environment. An already-frozen in-process request is an invariant: a conflicting later explicit request is an error, not a new winner.
2. Retain `--device` for native CLIs. Support a raw Vulkan ordinal, a unique case-insensitive name substring, and a canonical `uuid:<32 hex digits>` selector through that option. Preserve SfM's documented `-1` Auto spelling and accept `auto` consistently. Do not introduce a competing `--gpu` flag or a second selector parser.
3. Parse whole values and reject malformed numbers, overflow, unsupported negative values, absent devices, and ambiguous matches. An empty environment value is absent, not device zero. Numeric input is a transient lookup, not stored identity.
4. Resolve to the Vulkan physical **device UUID** plus a diagnostic name. Keep ordinal and reported memory as display information only. Each runtime resolves that UUID in its own instance and verifies the selected physical device; do not transfer `VkPhysicalDevice` or `VkDevice` handles between runtimes.
5. Auto uses shared ranking—discrete, integrated, remaining usable devices, then VRAM—with the entry point's applicable capability checks. Resolve Auto once for the invocation/application choice and pass the resulting identity to all participating runtimes. A later stage cannot independently rerank and move the work elsewhere. Keep capability probes local to their runtime; training's `usable` flag is not proof that NN or SfM can use a device.
6. An explicit missing/unusable identity fails with the requested device and the failing workload/requirement. A missing or non-unique UUID cannot fall back to an ordinal or a similar name. Do not silently select another GPU.
7. Preserve supported same-device algorithm fallbacks, including NN's non-cooperative kernels and SfM's established CPU/scalar BA fallback. Validate the device request before considering those fallbacks, so an invalid index cannot masquerade as unsupported arithmetic. Existing CPU execution is not proof that GPU routing worked.

UUID is used to correlate the current native run and its children, not promised as a permanent hardware serial number. Vulkan documents topology/driver caveats for persistent use. Re-resolve any recovered request and require reselection when its identity is unavailable; never silently retarget it. See [Vulkan device identity properties](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceIDProperties.html).

### Lifecycle and child-process contract

- Separate **configuring the desired device** from **creating a logical device**. Register/carry the resolved identity without calling `nn::vk::Context::get()` solely to select hardware.
- At the first real `Context::get(opts)`, apply the shared device request while preserving that caller's `want_video`, validation, and profiling options. An explicit request bypasses the environment lookup; it must not be overwritten later in `pickPhysicalDevice`. Default allocations inherit the configured request.
- Retain the configured identity in the small process-level NN request state added in Phase A, outside disposable context state. Teardown frees GPU resources, not the application's frozen selection; every new context generation inherits that identity.
- Construct and initialize the NN context in a local exception-safe owner under the existing mutex; publish `g_ctx` only after success. Use a `unique_ptr` with a class-accessible deleter as needed for the private destructor. Failed initialization must destroy every partially created Vulkan resource and leave no published context. Preserve the generation/entry-point-cache invariant during failed and successful attempts.
- Reusing an initialized context with the same resolved device succeeds; requesting another device produces a clear restart-required error. Ordinary default `get()` calls do not reinterpret Auto as a new request or retry environment selection.
- The GUI freezes and snapshots the request on its control thread before any worker/preview starts. Workers consume immutable job data, not mutable GUI state. Capability-only enumeration/display must not create a logical device or lock the picker.
- Stop the eager device probe in `DatasetPrep::backends()`: keep static build availability separate from runtime decoder availability, and perform the real native video probe at the first extraction/preview operation after selection is frozen. Reuse `VideoPipeline`'s actual capability checks then; preserve ffmpeg fallback and do not invent a second video-capability policy.
- Native self-children receive `--device uuid:...` explicitly. The same SfM argument vector continues to feed both in-process parsing and the self-child; do not write two routing implementations. Child selection must override inherited `SS_VK_DEVICE`.
- Do not implement native routing by repeatedly mutating the parent process environment. That is racy in GUI workers, affects unrelated children, and loses the distinction between defaults and explicit selection.
- Keep existing resource ownership and job sequencing. Destroy model/decoder owners before `nn::shutdown`; do not switch devices underneath pools, trackers, pipelines, cached capabilities, or resident weights.

## Implementation phases

### Phase A — Shared selection and safe first use

**Ownership:** a narrowly scoped host-only Vulkan selection helper under `src/core/` (proposed `VulkanDeviceSelection.h`, with a source file only if needed); `src/nn/Device.{h,cpp}`; `src/nn/vk/Context.{h,cpp}`; Vulkan backend enumeration/selection; `src/sfm/vk/VkContext.h`.

1. Add the shared request parser and UUID correlation using existing Vulkan enumeration/probe paths. Extend existing device-list records rather than building a parallel global catalogue. The helper may depend on Vulkan and the standard library, not on GUI, model, or training-engine code.
2. Make native listing side-effect-free and expose UUID alongside name/ordinal. Preserve each runtime's detailed capability checks and error channel, including the backend's nonthrowing `ok()/last_error` boundary.
3. Add the NN configure-without-create seam, normalize precedence, retain the configured selection through NN teardown, and fix exception-safe singleton publication. Preserve first-use `want_video` and context-generation behavior.
4. Make SfM and Vulkan backend selection accept the resolved identity, including capability probes. Migrate affected callers cleanly; retain integer parsing only at supported CLI/API input boundaries, not as a competing internal identity system.
5. Define the immutable native request field consumed by GUI job structures and all native entry points. Freeze must happen before asynchronous dispatch; enumeration must not freeze it.
6. If a shared implementation file is needed, link it into every native standalone/application target that uses it, not only the training library. Keep optional-module and headless builds intact.

**Acceptance:** listing does not initialize NN/backend; explicit input beats conflicting environment input; reordered enumeration and duplicate names cannot redirect a UUID request; an invalid request fails without publishing NN state; a corrected request can initialize and execute a tiny real NN operation after a failed attempt. Same-device reuse works; a conflicting live request fails. Each recreation honors its first real caller's feature options while preserving the configured physical identity.

### Phase B — Complete the three native workflows

The following lanes share Phase A's resolved-device contract. They may run concurrently with disjoint ownership; the shared GUI and catalog integration stays serialized in Phase C.

#### B1. Built-in SfM

**Files:** `src/sfm/SfmConfig.{h,cpp}`, `Pipeline.{h,cpp}`, `feature/Extractor.{h,cpp}`, `feature/LearnedMatcher.{h,cpp}`, SIFT/matcher/pair-selection options, mapper/BA/atom/merge device plumbing, `src/app/cli/sfm_main.cpp`, `sfm_ba.cpp`, and `src/app/gui/SfmRunner.{h,cpp}` / `SfmInProcess.cpp`.

- Resolve/freeze the request at the shared SfM entry boundary before extraction, matching, mapping, merge, or direct BA probes allocate or select hardware. `Pipeline::run_auto` and standalone stage/direct-BA commands must observe the same contract.
- Apply the identity to every SfM context, including temporary prefilter matchers, both mapper scalar contexts, bottom-up worker contexts, merge/refinement, and directly constructed BA solvers.
- Configure NN before the first ALIKED, LightGlue, or LoMa model allocation. Reuse their process-wide inference device; do not add a per-model Vulkan runtime or needless new model constructor parameters. Make the existing adapter device fields effective or remove them in favor of the shared request.
- Key capability caches by resolved identity rather than an ordinal/Auto sentinel. Distinguish device-resolution failure from a valid device lacking a BA arithmetic mode; preserve the latter fallback.
- Add the canonical selector to `SfmRunner`'s existing argument builder so both `parse_auto_args`/`run_auto` and the self-child receive it.

**Acceptance:** classical and learned SfM use the selected UUID in both SfM and NN contexts. Cover ALIKED/LightGlue and LoMa, not just SIFT. Exercise lazy GPU BA and worker-created contexts. An invalid explicit selection fails even on a BA-only path; a legitimate arithmetic fallback remains distinct and visible.

#### B2. Masking and its decode boundary

**Files:** `src/sam/Masking.{h,cpp}`, `Sam.h`, `pipeline/Session.cpp`, `src/app/cli/sam_main.cpp`, `sam_extract.cpp`, `src/app/FrameExtract.{h,cpp}`, `src/app/gui/DatasetPrep.{h,cpp}`, `SegmentPanel.{h,cpp}`, `PreviewFrames.cpp`, and the existing video availability call boundary.

- Feed GUI preview and dataset jobs the immutable native selection before frame decoding, capability probing, or `Masker::init`.
- Replace the separate segment/track/extract selector conversions with the shared parser. Numeric `sam track --device N` must mean index N, not a substring of a GPU name. Apply the selector for GPU-backed `sam video` too.
- Remove selection-specific `SS_VK_DEVICE` mutation in `sam_extract`; configure the native request before any decoder or model first use. Leave unrelated validation/profiling behavior unchanged.
- Have Session/model reuse compare effective physical identity, not two textual spellings of the same selector. Default model allocation and trackers inherit the configured device.
- Preserve both SAM families, existing prompt/tracking semantics, and model unloading. Native video and masking share the selected NN device where decoding is built in. The normal ffmpeg decode fallback remains available on a device without video support.
- Keep external Python masking unchanged in this lane; the UI must not claim that it honors the native selector yet.

**Acceptance:** actual SAM 2 visual and SAM 3 text/visual masks run on the chosen UUID in preview and batch paths; tracking spans multiple frames. Segment, track, extract, and GPU-backed video agree on numeric/name/UUID semantics. Preview followed by a dataset job, NN shutdown, and another preview retains the choice. A decode capability probe cannot select a different device first.

#### B3. Geometry

**Files:** `src/app/cli/geometry_main.cpp`, `src/app/gui/GeometryPanel.{h,cpp}`, `GeometryRunner.{h,cpp}`, and the existing `src/app/GeometryModel.{h,cpp}` boundary only as necessary.

- Register the immutable native request before preview frame handling or loading a geometry predictor.
- Make the CLI use the shared parser and precedence rather than its separate numeric/name conversion and selection-only eager context creation.
- Pass the canonical selector in `GeometryRunner`'s existing child argv; do not rely on the child's environment matching the GUI.
- Keep `GeometryModel` as the single MoGe/Metric3D seam. Both predictors' existing NN allocations inherit selection; no third geometry implementation or duplicated model-specific device parameter chain.

**Acceptance:** GUI preview, dataset child, and CLI execute real MoGe-2 and Metric3D v2 inference on the requested UUID and produce aligned depth/normal outputs. A conflicting inherited environment cannot change the child GPU. `geometry --check` alone does not count: it is a host camera-math check.

### Phase C — GUI cutover and native acceptance

**Ownership:** shared `src/app/gui/GuiApp.{h,cpp}`, common application job integration, shared i18n catalogs/help, and build/documentation integration. Serialize edits to these files after the native lanes merge.

1. Relocate/reuse the Vulkan picker in the common settings surface. Add an explicit Auto choice, show the resolved device after freeze, and keep driver-provided names raw through `ui::` wrappers.
2. Populate the Phase B job fields from the one frozen choice. Cover dataset preparation, mask preview, geometry preview, SfM in-process/self-child, geometry child, ordinary training, and checkpoint-open/resume paths that can initialize the backend. `open_splat`/CompareView and mesh/render workers that acquire the engine also commit the same Vulkan choice before dispatch; they must not bypass the lock.
3. Remove the training-only interpretation of `_device_locked` and any duplicate picker state. Do not allow a later training action to disagree with a device already chosen by native preprocessing.
4. Keep CUDA training selection separate when built; do not forward its indices into native jobs. Explicitly freeze its GUI picker before first engine use, including training, `open_splat`/CompareView, and mesh/render workers, and retain that lock thereafter. CUDA's `device_select` cannot enforce this lock by rejecting a later change, so the common GUI launch boundary must enforce it. Preserve backend compile guards without adding CUDA Auto, cross-API routing, or CUDA validation.
5. Add localized selection, ambiguity, unavailable-device, scope, and restart-required messages through the existing catalogs; update native `--device` help in all 13 languages. Follow font-coverage generation when new catalog characters require it.
6. Report the actual resolved UUID/name at workload startup through existing log facilities. Verify identity at every context creation, including later SfM worker contexts; avoid per-kernel logging or a new telemetry system. A copied argv value or one process-global name banner is insufficient evidence.
7. Run the native acceptance matrix below. After it passes, update the existing subsystem/app/backend documentation that describes independent selection and environment precedence; remove throwaway smoke artifacts. Do not create an unrelated documentation or recovery subsystem.

**Native completion gate:** all three built-in workflows obey one selection in their real GUI/CLI surfaces, lifecycle/error cases pass, and native child routing is explicit. This gate can ship without external COLMAP or Python device routing. Mark external routing as independent in interface/help text rather than implying an app-wide guarantee it does not yet provide.

### Phase D — External compatibility tail

This is a separately reviewable follow-on, not a hidden prerequisite of Phases A–C.

**Files:** `src/app/gui/ColmapRunner.{h,cpp}`, external Python launch in `DatasetPrep.cpp`, `src/app/gui/Subprocess.{h,cpp}`, and `reference/scripts/mask.py` only if its existing CLI needs an explicit device option.

1. Inventory the actual launched COLMAP/Python GPU operations and supported versions. Check the installed command's help for GPU index/use flags; do not assume the same option names across COLMAP releases. Extraction, matching, and GPU BA are separate operations. Preserve existing fisheye/CPU algorithm decisions.
2. Define a compatibility mapping from native physical identity to the external runtime's identity/visible ordinal. Never pass a Vulkan integer straight to CUDA. Match a queried identity where supported; otherwise require an explicitly labeled external-runtime device override. Names alone are not sufficient on a machine with identical GPUs.
3. Account for the caller's existing visibility restrictions and ordinal remapping. Scope any environment override to the specific child process; leave the parent and unrelated children unchanged. Extend the existing `run_process` environment support only as much as these launch sites require, on Windows and POSIX.
4. For COLMAP, emit only supported device flags at each GPU operation and retain its CPU controls. A boolean such as `ba_use_gpu` cannot establish which GPU ran BA. Verify against the supported installed versions, not solely the latest web documentation.
5. For external Python masking, apply the selected external device before the framework initializes it. Reuse the existing script/interpreter path; do not add Python to the native build, import reference tooling into native execution, or bundle model/framework dependencies.
6. If the selected native device has no supported external-runtime counterpart, report that explicitly and offer the existing native path or an explicit external/CPU choice where that tool supports one. Do not silently choose the first NVIDIA GPU, select CPU, or override user visibility restrictions while claiming the original GPU was honored.
7. Verify external routing separately on the supported COLMAP/Python installations: actual selected device, existing visibility filters, invalid/missing mappings, unavailable framework/backend, and child-environment isolation. No new CUDA backend validation project is part of this tail.

Reference: [COLMAP CLI documentation](https://colmap.github.io/cli.html). The installed executable's help remains authoritative for its supported flags.

## Verification plan

### What was checked while writing this plan

Only source inspection and the existing read-only inventory command were run:

```text
./build/spirula.exe sam devices
```

It reported an AMD integrated GPU and an NVIDIA GeForce RTX 3060, both usable according to NN's baseline probe. This establishes a two-device routing test opportunity, not that all models fit, that SfM arithmetic is supported, or that any new behavior has been verified. No training, model inference, build, or test suite was run for this planning task.

### Implementation checks

Build through the repository entry point, keeping the normal patent gate off:

```text
build_develop.bat -DSS_BACKEND=vulkan -DSS_BUILD_SFM=ON -DSS_BUILD_SAM=ON -DSS_ENABLE_PATENTED=OFF
```

Use the corresponding `build_develop.bash` on Linux. Main owns shared build/check execution after integration; use bounded images/scenes and cached, appropriately licensed model weights. Do not commit weights, private datasets, absolute local paths, or scratch output. Native-video coverage requires a separately opted-in build; do not change the committed default.

| Check | Required observable result |
|---|---|
| Selection without allocation | Opening the selector/listing GPUs does not initialize a logical device or lock the GUI. Empty Auto/default behavior and explicit Auto are distinct from an accidental index-zero parse. |
| Resolver edge cases | A focused native self-check covers ordinal reordering, ambiguous names, missing UUID, malformed/out-of-range input, and explicit-versus-environment precedence. Assert the selected identity/error, not parser plumbing. |
| NN failure and reuse | Failed initialization leaves no context/resources published; a subsequent valid request performs a tiny actual compute/upload/download successfully. Same-device reuse succeeds, a different live request fails, and shutdown/recreation retains the configured identity. Cover the first-use video option. |
| Classical SfM | A small SIFT extraction/matching run and a bounded mapper/BA case actually use the non-default GPU; inspect actual context UUIDs. Exercise direct BA invalid-selection handling and bottom-up/shared contexts, not only a host `--check`. |
| Learned SfM | Run ALIKED/LightGlue and LoMa extraction/matching with explicit selection conflicting with `SS_VK_DEVICE`. Both NN and any SfM GPU BA contexts report the selected identity; inspect valid reconstruction/feature output. |
| Masking | Use a small image and short frame sequence for SAM 2/SAM 3 preview, segment, track, and batch/extract. Inspect the masks/overlay and output dimensions, and verify actual NN identity. Include numeric tracking selection and preview-before-batch ordering. |
| Geometry | Run both MoGe-2 and Metric3D through preview, CLI, and dataset child. Inspect finite, correctly aligned depth/normal output and actual child UUID, including a conflicting inherited environment. |
| Session sequence | In a fresh GUI process choose the non-default GPU before preprocessing. Run masking preview, SfM, geometry, then a bounded Vulkan training/render action. Repeat a native job after NN teardown. All GPU work stays on the frozen physical identity; changing selection is disabled with an accurate restart message. |
| Failure semantics | Missing/unsupported explicit selection fails before model load/GPU work, never reroutes to another GPU, and never becomes a BA capability fallback. Legitimate same-device algorithm/CPU fallbacks remain distinguishable. |
| Native decode, opt-in | With native video built, capability display remains non-creating; real video preview/extract freezes first, decoder and masker agree, and an unsupported video codec uses the existing ffmpeg fallback without moving inference to another GPU. |
| Build guards and interface | Vulkan GUI/headless and relevant optional-module configurations retain their command availability; catalogs/help/font coverage pass. CUDA code remains source-compatible and correctly separated, without adding CUDA validation. |

Existing useful routes include `spirula sam devices`, `spirula sfm extract`, the native `sfm_sift_test`/`sfm_map_test`/`sfm_cholesky_test` device options, `sam_pipeline_test`, and the model-forward modes of `metric3d_test`/`moge_test`. Use their existing help/header syntax when executing. A large arithmetic suite or a host-only `spirula geometry --check` is not a substitute for exercising routing through the changed application path.

Keep the small regression check for the demonstrated selection/lifecycle bugs. Use throwaway scripts for end-to-end command orchestration and remove them after verification; do not add a test framework, mocked GPU-success path, or tests that only inspect command strings/source code. If runtime/hardware/model prerequisites are missing, record the exact unverified matrix rows rather than marking the native milestone complete. Desktop GUI acceptance requires launching the actual application and inspecting the selector, preview, and lock behavior; CLI success alone is insufficient.

## Execution and integration order

1. Reconfirm the implementation baseline and any stacked-PR dependency before code changes. Create the correct child branch for an actual dependency; this plan does not presume a parent PR or depend on unrelated recovery work.
2. Implement Phase A first. Before each writing wave, record branch/status/stashes and reserve all pre-existing dirty paths for Main. Use isolated writing lanes in a Git checkout and keep the parent checkout read-only while they run.
3. Dispatch B1/B2/B3 together only after their shared request/configure-without-create contract exists. Assign shared helper, NN context, `GuiApp`, common catalogs, and CMake ownership once; those files cannot be edited concurrently by sibling lanes. Each lane runs only its isolated targeted smoke check and aborts rather than claims success if it cannot pass it. No lane-wide/project-wide formatter, lint, or test sweep.
4. Integrate Phase C serially, reconcile branch/status/stashes and user work, then perform shared build/native acceptance. Confirm actual device identities, not just successful worker completion.
5. Start Phase D only after native acceptance, or keep it explicitly deferred as the separate compatibility tail. Do not label external routing complete until its own installed-tool checks pass.
