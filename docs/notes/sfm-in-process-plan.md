# Running structure-from-motion inside the GUI

Plan for making `src/sfm/` a library the GUI drives directly, instead of a CLI
the GUI runs as a child process and watches through the file system.

Status: **§8 steps 1–5, 7 and 9 landed, and both §10 fixes.** Steps 6, 8 and
10 are still plan. It supersedes
`sfm-port-plan.md`'s phase 3 (library-ization) and phase 5 (GUI integration),
which described a smaller version of the same work; that document keeps phases
0-2, 4, 6 and 7 and the inventory.

All three defects below are fixed: the reel lag went with the second decode
(§10), the empty hover with matches.bin arriving only at the end of its stage
(§10), and the unreliable reporting with the stdout scraping (§6).

The immediate reasons are three user-visible defects — the feature reel lags
behind extraction, hovering the match map draws no matches until `matches.bin`
lands, and stage/progress reporting is unreliable. The reason to do it as a
design change rather than three fixes is that manual tie points, manual model
alignment, and LiDAR/IMU input are all unreachable from the current shape.

---

## 1. What is actually wrong

Not the subprocess. The channel: the GUI can only learn about a run from bytes
the child has already serialized to disk.

| Symptom | Cause |
|---|---|
| The reel lags extraction | `FeatureWatcher::run` polls for each `features/*.bin` at 120 ms, then decodes the full-size image the extractor decoded seconds earlier (`app::load_rgb`) |
| Hover draws no matches | `MatchesDatabase` lives in the child's RAM for the whole match stage; `matches.bin` is written once, at the end. `PairPreview` binary-searches a `MatchesIndex` over a file that does not exist yet |
| Stage and result reporting is unreliable | `SfmRunner::note_progress` scrapes **translated** stdout — it matches `sfm::slog::prefix(Tag::Match)` and reads the first `a/b` on the line — and the outcome arrives as exit codes 3 and 4 plus a substring search for `result_not_metric` |

Everything the screen shows is therefore serialized, written, re-read and
re-decoded, at a cadence set by a poll interval rather than by the work.

The port plan already named the missing piece: **phase 3 never landed.** The
library has 116 `printf`/`fprintf` sites outside `tests/` — three quarters of
them in `map/` and `ba/` — no cancellation token, and a `--progress-dir` that
is process-global state (`sfm::progress::set_dir`). `sfm/core/Log.h` has the
right design and the CLI uses it (147 sites in `sfm_main.cpp`); the library
does not.

---

## 2. The shape to move to: `sfm::Session`

The repository has solved this once. `src/app/TrainerCore.h` is the single
training driver: phases as methods, sinks as `std::function` members, control
through public atomics, front ends owning their own UI. The CLI and the GUI
both drive it and neither reimplements it.

```cpp
namespace sfm {

struct RunContext {
    std::function<void(Tag, Level, const std::string&)> log;
    std::function<void(const Event&)>  on_event;
    std::atomic<bool>*                 cancel;
    std::string                        progress_dir;   // no longer a global
};

class Session {
public:
    SfmConfig cfg;
    Manifest  inputs;                  // section 4

    void extract(const RunContext&);   // the subcommands, unchanged
    void match  (const RunContext&);
    void map    (const RunContext&);
    void merge  (const RunContext&);

    // Read-only views for whoever is watching, under a shared lock.
    std::shared_lock<std::shared_mutex> read() const;
    const FeatureSet*     keypoints(uint32_t image) const;
    const TwoViewMatches* matches(uint32_t a, uint32_t b) const;
    const Reconstruction* model(int i) const;
};

void run_auto(Session&, const RunContext&);
}
```

`run_auto` is the load-bearing part. `cmdAuto` (`src/app/cli/sfm_main.cpp:2074`,
256 lines) is stage sequencing living in the CLI: `extractDirectory`,
`buildCameras`, the match loop, `Mapper`, `runMapper`, `assembleModels`,
orientation, model writing. While it stays there the GUI cannot run the
pipeline without a second copy of the order, which is the duplication
`AGENTS.md` forbids outright. Hoisted into the library, `cmdAuto` becomes
argv → `SfmConfig` → `run_auto`.

---

## 3. Four seams

In dependency order. Each is independently landable and none changes
reconstruction output.

**1. Log sink.** Convert the 116 sites to `slog` and make the sink settable.
The CLI installs one that prints exactly what it prints today — byte-identical
output is the regression test. What stays raw `fprintf` is what already stays
English: the `SS_SFM_MAP_PROF` diagnostics, the audit and bottom-up paths, and
`tests/`.

**2. Cancellation.** A token checked at the same points, `sfm::Cancelled`
unwinding to the job boundary, GPU work drained before the stage returns. The
`~VkContext` that frees every buffer it handed out is what makes this safe; it
is a leak fix that must not be regressed. The one `abort()`
(`src/sfm/map/Mapper.h:2949`) is behind `SFM_SCORE_CHECK` and is a developer
assertion, but it becomes a thrown error once the library shares a process with
the GUI.

**3. Typed events.** `StageBegin`/`StageEnd`, `ImageExtracted{index, name, w, h,
keypoints}`, `PairVerified{a, b, inliers}`, `ModelUpdated`, and a
`Result{registered, total, mean_reproj, metric_ok, models}` that replaces the
exit-code and substring channel. **Wire the CLI's own printed progress to the
events**, so they are exercised by the CLI before the GUI depends on them.

**4. Snapshot reads.** Push events do not answer hover: the cursor lands on an
arbitrary cell at an arbitrary time. `matches(a, b)` and `keypoints(i)` served
from the live containers under a `shared_mutex` — verification workers write,
the preview thread reads, a few reads per second — is what makes `PairPreview`
correct during matching instead of after it. `FeatureWatcher`'s poll and second
decode delete themselves at the same time.

Features do not all stay resident: 3000 images at 8k features is roughly 3 GB
of descriptors. Put a `FeatureStore` behind the accessor — RAM cache over the
same on-disk files — so a reader in the producing process skips the round trip
and everything else behaves as now, including `--keep-intermediate`.

---

## 4. The manifest, and why LiDAR/IMU and GoPro need it

Independent of everything above, and the highest-leverage single change for
what comes next.

The GUI holds a structured `std::vector<PrepInput>` — per-input camera model,
focal factor, sub-cameras, stencils, masks — and flattens it to path-prefix
strings (`SfmRunner::append_camera_overrides` emits `--camera-model
cam0=opencv-fisheye`), which `cameraPrefixMatches` re-resolves against the
filesystem in the child. The interchange format between the two halves is *a
directory of JPEGs plus prefix-matched flags*.

Nothing else fits through it. IMU gravity, GPS with covariance, LiDAR sweeps,
per-frame timestamps and rig extrinsics are not expressible as a filename
prefix, and a flag per sensor is how a 136-flag surface becomes a 200-flag one
with two parsers.

`sfm::Manifest` is the ordered image list carrying, per image: path and stem,
camera group, EXIF, timestamp, prior pose with covariance, and associated range
data; plus per-capture sensor streams and rig definitions.

- The GUI builds one from `PrepInput` and hands it over.
  `append_camera_overrides` disappears.
- The CLI builds one from argv, the directory scan and EXIF, and gains
  `--manifest x.json` — which is also how the child-process path (section 6)
  receives structured input without new flags.
- `CameraOverride` and `CameraSetupOptions` become one way to populate a
  manifest, not the interchange format.

Where the priors land, once they can arrive at all: gravity in `map/Orient.h`,
GPS/IMU adjacency in pair selection, pose priors as BA residual blocks — which
needs `sfm-port-plan.md` §9 item 3 (gauge fixing and constant-parameter masks)
first — and scale in `map/MetricGauge.h`, generalizing what `--metric-gps`
already does.

**GoPro `.360` then splits into two ordinary tasks.** Demux and EAC-packed HEVC
decode belong in `src/video/` beside the `.insv` track splitting `DatasetPrep`
already does. The GPMF telemetry track — GPS, gyro, accelerometer, gravity,
frame timing — is extracted in the same pass and goes into the manifest, which
is impossible today because the child receives only a directory. Both output
projections are available: equirectangular is carried end to end
(`ColmapParser.cpp:90` reads model 17, the projection kernels have variants,
`kSfmCameraModels` offers it), and per-face pinhole needs nothing new because
the per-folder camera mechanism already describes a rig of faces. `unproject_raydir`
in `src/shaders/pixel_wise.slang` maps `EQUIRECTANGULAR` through `equirect_unproj`,
so depth and normal supervision on equirectangular views is supported end to end.

---

## 5. Manual ties and alignment are what force the design

The feature a child process cannot deliver at any latency, and the reason to
shape the API for it now.

It needs the reconstruction to be **a document the GUI holds open**: load a
model, click correspondences across two images or drag one model onto another,
run a bounded operation, see the result in the same viewport, undo it. As a
batch job each edit is a process launch and a full model re-read.

So the session's second half is operations on an open model, not a pipeline:

```cpp
void  open(workspace);                                   // model, and features/matches if kept
TieId add_tie(img_a, px_a, img_b, px_b);
bool  register_image(uint32_t i, span<const Tie> seeds);
bool  align(int model_a, int model_b, Sim3);             // or span<PointPair>
void  refine(Scope);                                     // local BA over the touched subset
```

Build them on the mapper's existing transactional undo and de-registration
rather than a parallel edit path. `PairPreview`, `MatchMatrix` and the
`ViewportPanel` preview then become editors over the session instead of viewers
over files — a small change once section 3's accessors exist, and an
impossible one without them.

---

## 6. What the CLI keeps, and the process boundary that stays

Three properties are worth more than the I/O they cost:

- **Every stage still reads and writes the same files.** Any one stage can be
  replaced by COLMAP's equivalent to bisect a failure (`sfm_main.cpp`'s header
  says so, and it is the reason the stage graph is the CLI's shape). In-process
  only means the reader may skip the round trip.
- **The CLI stays the reference driver.** argv exists in `sfm_main.cpp` and
  nowhere else; the GUI never builds a command line for the built-in path.
- **`ColmapRunner` stays.** It is the CUDA GUI's dataset path — `SS_BUILD_SFM`
  is off there — and the fallback elsewhere.

The child process stays available as an escape hatch, for the reasons
`SfmRunner.h` gives that survive phase 3: global BA on a large model and a live
trainer must not share a VRAM budget, and a driver that resets under a long
solve takes down only the child. What goes is the parsing. **Landed:**
`--progress-dir` writes `status.bin` from the same typed event stream, so the
GUI has one reader and two producers, and `note_progress` and the exit-code
semantics are gone. `child_line_is_gpu_failure` stays — it is advice about
which flag to try next, not a fact about the model.

That path keeps one fidelity gap: a live *view* of a pair's matches comes from
a file rather than from the producer's heap (§10) -- which is what step 8 would
close, and only for the in-process transport. Acceptable for an escape hatch.
Reconsider dropping it once the in-process path has run the large-dataset
stress corpus.

---

## 7. Vulkan devices

In-process SfM adds `sfm::VkContext` to a GUI process that already creates and
destroys `nn::vk::Context` for SAM and MoGe.

Enough to ship: bracket the SfM context with the job — created at the first
stage, destroyed when the run ends. That is the pattern `nn::vk::Context`
already implements (`shutdown()` plus a `generation()` counter, because the GUI
hands the GPU back between jobs). `DatasetPrep`'s masking finishes before
extraction begins, so two devices are never concurrently live.

The real fix is `sfm-port-plan.md` phase 6, adopt-external-device. Note that
the module already churns devices *within* a run — scoped `VkContext`s per
global-BA solver and per prefilter matcher — which costs more once the host
process is the GUI.

---

## 8. Phases

Both front ends work at the end of every step.

1. **Log sink** — **landed.** `slog` gained a settable sink, a `Level` and
   `diag()`; the 116 `printf` sites outside `tests/` go through it. CLI output
   byte-identical.
2. **Cancellation** — **landed.** A token checked per image, pair,
   registration and LM iteration; `SIGINT` exits 130 mid-extract and mid-map.
   `verifyPairs` drains and joins its workers before throwing.
3. **Typed events** — **landed.** The CLI's extract and match progress lines
   are printed from the stream. `SS_SFM_EVENT_TRACE=1` shows it.
4. **`run_auto`** — **landed.** `sfm/Pipeline.{h,cpp}`; `cmdAuto` ends at
   `run_auto(cfg, in).exit_code`. `RunContext` installs and restores the sinks.
5. **`Manifest`** — **landed.** `data/Yaml.h` (a YAML subset over Json.h's
   value model, taking JSON verbatim) and `sfm/core/Manifest.{h,cpp}`; CLI
   gains `--manifest`, and `append_camera_overrides` is gone. This is the
   prerequisite for LiDAR/IMU and GoPro telemetry reaching the mapper: a new
   sensor becomes a new key rather than a new flag.
6. **Header → translation-unit split** (`sfm-port-plan.md` phase 3 item 4).
   Still plan, and now worth *less* than when this was written: step 7 landed
   without it, so the justification is build time alone. `Pipeline.cpp` is 27 s
   to compile; the GUI keeps that header out of its own translation units
   (`SfmRunner.cpp` stays at 3 s) by going through `app/gui/SfmInProcess.h`,
   whose interface names no SfM type.
7. **`SfmRunner::run` calls `run_auto`** — **landed.** `parse_auto_args` moved
   the settings list into the library, so both front ends read it with one
   parser and the GUI needs no second mapping; `cmdAuto` is ten lines. The
   child stays as the escape hatch, on a checkbox and on SS_SFM_SUBPROCESS=1,
   and both transports feed one `apply_status`.
8. **Snapshot accessors**; `FeatureWatcher` and `PairPreview` rewritten against
   the session. Still plan — §10 already gave both of them live data through
   files, so what is left is skipping that round trip. It needs the same
   refactor step 10 does: `run_auto`'s locals become a Session's members.
9. **Device bracketing** — **landed, at the level this asked for.** Every SfM
   `VkContext` is a scoped member of the stage that built it, so the GPU is
   handed back when `run_auto` returns; and the GUI genuinely sequences the two
   (`run_pending_if_stopped` fires only once `training_busy()` is false), so a
   reconstruction cannot start under a live trainer. Adopting the engine's
   device — one device, one `SS_VK_DEVICE` — remains `sfm-port-plan.md` phase 6.
10. **Interactive operations** — ties, alignment, local refine. Still plan, and
   the reason to do step 8's Session refactor rather than more file channels.

What is left (6, 8, 10) is one mechanical job and one design job: the header
split stands alone, and 8 and 10 are the same refactor twice — a Session that
owns what `run_auto` currently keeps in locals, so a front end can read a
running job and later edit a finished one.

---

## 9. Rules

To be folded into `src/sfm/README.md` as they land:

- The library never prints, never exits, never aborts.
- One stage sequence (`run_auto`), used by both front ends.
- Every stage reads and writes the same files as today.
- One `Session` per process. Mutation is single-threaded; readers take a
  shared lock.
- The GUI never builds an argv for the built-in path.

---

## 10. Available before any of this

Both **landed**, ahead of the phases above, because each fixes one of the
three defects on its own:

- **A streaming matches file.** Verification appends every verified pair to
  `live_matches.bin` as it produces it — the same VKMT layout with the pair
  count written as `kStreamingPairs`, so `indexMatches` serves both and stops
  at a torn tail. `PairPreview` reads it until `matches.bin` appears.
- **A thumbnail per extracted frame.** `thumbs/<rel_stem>.jpg`, from the copy
  the extractor has already decoded and downscaled. Measured on 25 images of
  Mip-NeRF 360 garden: extraction 0.73–0.75 s without, 0.78–0.88 s with,
  against a full decode saved per frame the reel draws.

## 11. What watching cost, once it was measured

Both of the above are written from the thread the stage runs on, and on a
larger capture that showed. Fixed, with the numbers, on 92 images of the same
capture at `--max-image-size 1600`:

- **The thumbnail's JPEG encode and file write moved to their own thread**,
  leaving only the box downscale (which has to read the caller's buffer) on
  the extractor's consumer thread — the one the GPU stage runs on. Extraction
  went from 2.65 s bare / 3.16 s watched to 2.65 / 2.86: the cost of being
  watched fell from 18 percent to 8. The queue is bounded and drops its oldest
  rather than stall the stage; a missing thumbnail makes the reel decode the
  source frame, which is what it did before thumbnails existed.
- **`live_matches.bin` is packed once and flushed on a clock**, not written
  index by index and flushed per pair while holding a global lock, and it
  stops growing past 256 MB — a capture with 700k verified pairs would
  otherwise append 1.4 GB beside the `matches.bin` that is the actual output.
- **The match stage's `Progress` event is emitted ~400 times over the stage**
  rather than once per pair, and a `PairVerified` no longer wakes the status
  writer or the GUI's fold, neither of which keys on it.

The mapping bar was the other defect: it was `registered / images` of whatever
reconstruction was in hand, and the mapper resets the model between seed
attempts while a bottom-up run numbers each atom from zero — so it ran forward
and fell back, sometimes several times. It is now `events::map_placed`, a
union over the capture's images that only rises; atoms report themselves in
database ids once done, and their private mappers report nothing (which also
stops one atom appearing on screen as "the model").
