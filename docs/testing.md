# Testing

The native parity tests are the suite. The Python one this file used to
describe is gone -- see §3 for what it covered and what now does not.

## 1. Native cross-backend parity tests (the important ones)

`src/backend/tests/*.cpp` — currently 22 tools covering projection (fwd, bwd,
quant-grad), rasterization bwd, tile intersect, warp, FPBO, optimizer (general
+ geometry), densify, per-pixel train, PPISP, bilagrid, multi-scale loss, the
fused appearance chain and float16 / uint8 images (`appearance_parity check`
holds the fused chain to the per-stage kernels, and `pixel_format_parity`
holds every compact-format reader and writer to float32;
`mask_loss_semantics`, `reg_loss_underflow` and `fpbo_split_parity` are
self-checking rather than dump-then-compare: the first pins what an image mask
means in the loss, including three-class masks, multiple scales, scaled masks
and ignored-color isolation; the second sweeps log
scales past every exp(scales) underflow threshold, down to -inf, and fails if
the per-splat regularizers hand the optimizer a NaN or push a splat below
kMinLogScale; the third steps FPBO and the non-fused optimizer path from the
same state and fails if they disagree, which is how the two update laws for
linear splat colour are held together),
meshing (activation, LBVH, occupancy/bisection/color, moment raster, the
per-camera samplers and the visibility cull), plus
`backend/tests/engine/` which drives the *real* engine end to end
(render parity, train parity, and two self-checking tools rather than
dump-then-compare: `engine_reset_state` trains the same scene twice across an
`engine_reset()` and the two must land in the same place, and
`gt_bilagrid_sentinels` pins the GT bilateral grids' invariants -- one depth
scalar per camera, and the no-GT sentinels passing through untouched).
`backend/vulkan/tests/` adds 3 Vulkan-only smoke tests (runtime, pipeline,
sort/scan).

**The same source builds under both backends.** Each file only touches
`backend::`, the generated launch declarations, and `Tensor.h`. The workflow
is dump-then-compare:

```bash
# on the CUDA machine
./build_cuda/projection_parity dump ref.bin
# on the target machine / device
./build_vulkan/projection_parity compare ref.bin
```

Inputs are deterministic, and comparison is tolerance-based — fast-math
exp/sqrt chains legitimately differ across compilers, and borderline-cull
flips change whole rows, so a small allowance for those is built in.

Two tests carry a **relative-RMS gate** alongside the per-element one, for the
same reason: they contain discrete per-pixel or per-splat decisions that flip
wherever an architecture's rounding differs from the reference's, so a handful
of large outliers is expected while the vector as a whole must still agree.
`msloss_parity`'s NMS / quantile / clip modes put an RX 7800 XT at 0.21% of
tight elements out of tolerance (max_abs 4.19) against 0% on NVIDIA Vulkan --
but 3.8e-4 relative RMS against 1.8e-7. A permuted or biased reference sits
orders of magnitude above that, which is what the RMS gate is there to catch.

`engine_train_parity` has two gates rather than one, because per-element
agreement is not something any implementation can hold across 12 optimizer
steps. The threshold-crossing kernels -- median depth, masked-tile skip, the
rasterize-bwd survivor batching -- flip a handful of pixels per step wherever
an architecture's rounding differs from the reference's, and Adam turns a
flipped gradient sign into a full-size parameter step, so the trajectories
separate. Measured against a CUDA reference (2026-08-25): NVIDIA Vulkan lands
0.003% of elements out of tolerance at 4.8e-7 relative RMS, while an RX 7800 XT
lands 4.4% at 1.4e-4 -- identically on amdvlk and RADV, and unchanged by
`RADV_PERFTEST=wave32`, so it is not a wave-size effect. The divergence starts
in `depth_loss` and `normal_loss` (discrete median-depth selection); `rgb_loss`,
`ssim` and `psnr` stay at 1e-7. An indexing or layout break, by contrast, puts
30%+ of elements out of tolerance at a relative RMS above 1. The RMS gate is
what keeps the test sharp; the element gate is loose enough to absorb the drift.

`msloss_parity` splits its reference into two channels. **Tight**: per-pixel
gradients (deterministic given the raw-loss sums, which enter them only through
smooth reduce math), the densification loss map in every mode, equal-shape
`v_ref_depth` / `v_ref_normal` scatters (one atomic per cell), and the quantile
outputs. **Loose**: `LossValues`, the SSIM display scalar, and scaled-GT
scatters, which accumulate atomically in a backend-specific order. Each config
runs twice and compares the second return, because the scalars come back
through a one-iteration-behind async readout on both backends. One expected
mismatch survives: the CUDA SSIM scalar sums over TILE-GRID positions, so an
image whose dims are not a multiple of the tile picks up zero-padded
out-of-image contributions that differ between the 24- and 16-wide tiles
(`ssim_cs` cfg). It is display-only; gradients and loss values are unaffected.

Several tools also take a `*_DUMP_GOT` environment variable
(`FPBO_DUMP_GOT`, `PPISP_DUMP_GOT`, `MSLOSS_DUMP_GOT`, `PWTRAIN_DUMP_GOT`,
`BILAGRID_DUMP_GOT`, `DENSIFY_DUMP_GOT`) to write the *actual* values
alongside the reference, which is how you diff a mismatch numerically instead
of guessing.

### Building them

```bash
# CUDA branch: opt-in
bash build_develop.bash -DSS_BACKEND=cuda -DSS_BUILD_BACKEND_TESTS=ON
# Vulkan branch: built unconditionally
bash build_develop.bash -DSS_BACKEND=vulkan
```

Each `.cpp` becomes an executable of the same base name in that backend's
build tree -- `build_cuda/` or `build_vulkan/` (`build/` on macOS).

### Cross-machine / cross-vendor runs

The comparison target is often a different machine (e.g. an AMD GPU box), and
often offline. The pattern that works:

1. Transfer a matching `slangc` to the target and point `-DSS_SLANGC=` at
   it — SPIR-V is compiled at build time and never committed, so the target
   needs a compiler, and the version is pinned.
2. Dump references on the CUDA host.
3. Copy the `.bin` files over and run `compare` on the target.

Keep reference dumps out of git (`parity_refs/` is gitignored).

### macOS / MoltenVK

All 17 parity tools pass against a CUDA reference on Apple silicon, at
essentially the Linux numbers (`engine_render_parity`'s blit channel: 0.157%
of bytes on macOS against 0.152% on Linux, cap 0.2%).

`engine_render_parity` used to fail here on that channel, and the failure was
worth more than its number: the viewer's grid and frustum lines came out
*fragmented on macOS only*. It was not antialiasing, which is what this
section claimed for a while. `vis_blit`'s BVH descent read the popped node
inside a two-iteration child loop, and SPIRV-Cross re-materialized that
threadgroup read once per child instead of keeping it — so the second child
was read after the first child's push had overwritten the slot, and the
descent walked into the wrong subtree. The `[ForceUnroll]` on both child
loops is what keeps the pop a value; see the first MoltenVK rule in
`src/backend/vulkan/README.md`.

Three tools -- `msloss_parity`, `optimgeo_parity`, `meshing_parity` -- pass
only because `VulkanContext::init()` turns MoltenVK's default Metal fast-math
off. `SS_VK_FAST_MATH=1` puts it back, and they fail again; that is the knob
to reach for when measuring what the setting costs.

Speed is a separate question from parity, and macOS answers it differently. Two
kernel choices that cost nothing elsewhere cost an order of magnitude here, and
both are measured at run time rather than assumed: the GEMM tiling
(`OpGemm.cpp`, `SS_NN_GEMM_KERNEL` pins it) and the matcher's dot product
(`integerDotProduct4x8BitPackedUnsignedAccelerated`, `SS_SFM_NO_DOT4` pins it).
With those, an M2 runs SAM 3 image encoding at 12x an RTX 5070 (7.4 s vs 0.64 s,
was 140x) and brute-force matching at 22x (43 vs 1.9 ms per 8192x8192 pair, was
64x); the matcher's remainder is DP4A, which Apple has no instruction for. The
third is "slangc `[unroll]`" in `src/backend/vulkan/README.md`.

### Under the Vulkan validation layer

`SS_VK_VALIDATION=1` turns on `VK_LAYER_KHRONOS_validation` in all three
Vulkan contexts (engine, SfM, inference), so one variable covers a whole
`sam extract` → `sfm auto` → `geometry` → `train` → `mesh` loop. The layer
comes with the LunarG SDK; Ubuntu's `vulkan-validationlayers` predates
extensions the inference layer uses. The inference context logs through a
debug messenger (`[vk-validation]`); the other two print the layer's own
`Validation Error:` lines, errors only unless
`VK_KHRONOS_VALIDATION_REPORT_FLAGS=error,warn,perf`.

Three things that cost time:

- The inference context turns the layer's handle wrapping off
  (`unique_handles`): NVIDIA 595 reads `vkCmdDecodeVideoKHR`'s codec `pNext`
  again at `vkQueueSubmit`, by when the layer's copy is freed, so a wrapped
  decode segfaults at its first submit.
- The deprecated `VK_KHRONOS_VALIDATION_ENABLES` / `VK_LAYER_ENABLES`
  variables make the layer ignore every new-style setting, that one included.
  Ask for Best Practices with `VK_KHRONOS_VALIDATION_VALIDATE_BEST_PRACTICES=true`.
- Synchronization validation (`VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true`)
  only sees descriptor-bound resources and copies. The engine and the
  inference layer reach memory through buffer device addresses, so a clean
  run there says nothing about their barriers; SfM binds descriptors and is
  covered.

### At wave64

AMD GCN runs every kernel 64 lanes wide, and RDNA runs the `kWave64Entries`
kernels that way. An RDNA device stands in for GCN: dump each test's
reference at `SS_VK_SUBGROUP=32`, then compare at `SS_VK_SUBGROUP=64` on the
same device, so only the width differs. On a Ryzen 7000 iGPU (RADV,
2026-10-08) every dump-compare and self-checking test passed at 64, with
errors at the level of a wave32 run compared against itself (float-atomic
order): raster_bwd_parity max_abs 9.8e-4 against 7.3e-4, engine_train_parity
loose rel_rms 1.3e-10 against 1.4e-10.

## 2. GUI / viewer checks

The web viewer can be driven headlessly over the Chrome DevTools Protocol.
Headless defaults to SwiftShader; to exercise a real GPU, run against a real
display (`DISPLAY=:0`).

A scripted run that serves the viewer needs **`--keep-viewer-alive 0`**, or
the process hangs at exit waiting on it.

## 3. What is gone

The Python suite this document used to describe -- `tests/python/`, the
dataparser and step-config goldens, the trainer and web-viewer gates -- was
deleted with the Python trainer it compared against. There is nothing to run
and nothing to regenerate.

Its job has not gone away, though, and nothing covers it today:

- **The dataset parsers** had a golden over 4 formats x 4 config variants x 2
  splits, checking the frame set, poses, intrinsics, distortion, seed cloud
  and train-frame scalars. A native replacement would generate its fixtures
  from a fixed seed, as that one did, so it needs no dataset on the machine.
  Two of its checks needed no golden at all and are worth rebuilding first:
  the train and eval splits must partition the frames (a bug dropping frames
  from *both* sides leaves each side self-consistent), and every fixture
  format must describe the same scene.
- **`build_step_config()`** had a golden over 8 config variants x 4 run states
  x 20 steps straddling every warmup and decay boundary. Drift in a ported LR
  schedule used to fail there; now it shows up as a quality regression 20k
  steps into a run.

Both are `src/backend/tests/`-shaped work: deterministic input, committed
expectation, one executable. Neither exists yet.

## What to run before calling a change done

| change | gate |
|---|---|
| any kernel | CUDA build + Vulkan build + the relevant parity test on both |
| engine logic | both builds + `engine_render_parity` + `engine_train_step`-level check |
| config field | add the row in `src/config/TrainConfig.h`; check `spirula train --help` and the GUI's All Options editor |
| training-loop logic | `TrainerCore.cpp` — `build_step_config()` is the only place it lives |
| build system | every mode in [build.md](build.md) |
| a comment you wrote | `python3 tools/check_comment_length.py` — the build runs it anyway ([lints](build.md#lints)) |
| `SS_FILE` or `SS_SOURCE_ROOT` | `source_path` on each toolchain — MSVC, GCC and nvcc spell `__FILE__` differently |
| a mesh format, or which colors it carries | `mesh_format_roundtrip` — writes every format and reads it back through the other implementation |
| the UV atlas | `uv_atlas_split` — tens of thousands of charts that fail to flatten and must split |
| a preset field, or a batch row's shape | `preset_roundtrip_test` |
| what a typed-in command line becomes, or what a message may carry into it | `command_argv_test` — the message stays one argument and stays JSON-safe |
| the home screen's recent list, or how `gui.conf` stores it | `recent_list_test` |
| a per-cell optimizer launcher (Vulkan) | `SS_OPTIM_SLICE_CELLS=2048` on `optim_parity` / `optimgeo_parity`, which forces the multi-slice path only an SH buffer past ~24M splats would otherwise take ([SH layouts](notes/sh-quant-layout.md)) |
| a launch through `dispatch_budgeted` (Vulkan), or a new one | its parity tool under `SS_SUBMIT_BUDGET_MS=0.01`, which splits every budgeted launch after its entry's first into ranges of one slice (`SS_VK_VERBOSE=1` prints the dispatch-base pipelines that ran them); compare against a dump made without it ([submit budget](notes/gpu-submit-budget.md)) |
| H.265 reference handling | `hevc_reference_retention_test` on a non-NVIDIA Vulkan video-decode device with `SS_ENABLE_PATENTED=ON` |
| H.265 encode dimensions or HEIF image correctness | `hevc_sps_crop_test` (CPU) and `heif_test` (non-NVIDIA Vulkan encode/decode); fixtures are generated at runtime |
| anything | one short training run per backend on a public scene |

## H.265 retained-reference regression

With patented decoding explicitly enabled, run `build_vulkan/hevc_reference_retention_test`
on a supported **non-NVIDIA Vulkan device** advertising H.265 Main and Main10
video decode (on Windows, use `build_vulkan\hevc_reference_retention_test.exe`).
Build with `build_develop.bat -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON`
on Windows or the corresponding `build_develop.bash` command on Linux.
The committed 320×180, 72-frame fixtures in `src/video/tests/data/` are
synthetic FFmpeg `testsrc2` clips, not private recordings. The test checks
software-decoded RGB pixels in both the full run and after seeking backward
across GOP boundaries; an exit code alone is not a pixel-correctness check.
It requires an actual video-decode-capable GPU, not a CPU-only test runner.

To regenerate either `hevc_main_retention` (8-bit `yuv420p`) or
`hevc_main10_retention` (10-bit `yuv420p10le`), substitute `<format>` and
`<name>` in these commands; FFmpeg must have `libx265`:

```sh
ffmpeg -f lavfi -i 'testsrc2=size=320x180:rate=24:duration=3' -pix_fmt <format> -c:v libx265 -preset medium -crf 20 -x265-params 'keyint=24:min-keyint=24:bframes=4:ref=4:scenecut=0:pools=2:log-level=error' <name>.mp4
ffmpeg -i <name>.mp4 -vf 'select=eq(n\,22)+eq(n\,46)' -fps_mode passthrough -frames:v 2 -pix_fmt rgb24 -f rawvideo <name>.rgb
```

At POC 24, the short-term RPS retains four pictures marked unused by the
current picture; following pictures use them. The second GOP exercises
retirement and slot reuse. The Main10 test failed before the correction
(frame 22: 10.53 dB PSNR against software) and passes with retained references;
the threshold is 28 dB to allow host/GPU color-conversion differences.

## H.265 encode cropping and HEIF round trips

These regressions generate their inputs at runtime; no downloaded images,
binary fixtures, or FFmpeg installation are needed.

```bat
build_develop.bat -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON
build_vulkan\hevc_sps_crop_test.exe
build_vulkan\heif_test.exe
```

On Linux, use `bash build_develop.bash` with the same options and run the
executables under `build_vulkan/` without `.exe`.
`hevc_sps_crop_test` is CPU-only and is also registered with CTest's
`headless` label. It checks crop arithmetic, preserved SPS syntax, sub-layer
profile/level records, emulation prevention, and rejected malformed inputs.
`heif_test` requires a supported non-NVIDIA Vulkan H.265 encode/decode device;
a skipped round trip is not GPU acceptance.

Keep the HEIF fixture's 256×160 tiles and its 32 dB PSNR threshold. On AMD
Radeon AI PRO R9700, the encoder's minimum width is 384: applying a 128-pixel
conformance crop before encoding produced a 256-wide CTB grid while the SPS
advertised 384. Encoding uncropped and setting the display crop only in the
returned SPS preserves the coded grid. The grid/rotated-tile checks measured
8.0/13.7 dB before this correction and 47.9/47.5 dB afterward. The same test
also checks grid assembly, crop, rotation, mirroring, dimensions, and EXIF.

## Profiling

`SS_PROFILE=1` enables the env-gated per-stage timing breakdown
(H2D / D2H / D2D / memset / device / host). Header-only, works on both
backends — the right first tool when a backend is unexpectedly slow rather
than wrong.

Above that table both backends print **GPU time by kernel**, so the two are
directly comparable without a profiler. Vulkan brackets each dispatch with
timestamp queries; CUDA does the same with a CUDA event pair, injected by
`-Wl,--wrap=cudaLaunchKernel` (`backend/cuda/KernelProfilerCuda.cu`) so no
launch site is instrumented by hand and CUB's kernels are covered too. Rows
aggregate over template arguments / specialization constants, which is what
makes a CUDA row and a Vulkan row the same thing.

Two caveats on reading those numbers against each other. The intervals
include the gap before each kernel starts, so their sum runs a little over
the device-wait total. And a training run is **not** reproducible: atomic
order moves the trajectory, and the rasterization and sort kernels then see a
different scene — `rasterize_fwd` has been seen to move 70% between two runs
of the same binary. The image-sized kernels (losses, bilagrid, PPISP, FPBO)
hold to ~1%, so they can be A/B'd from a training run directly; for the rest
use the benchmark tools, which fix the workload:

```bash
./build_vulkan/raster_bench [num_splats] [iters] [macro_log2]   # raster fwd/bwd, binning
./build_vulkan/fpbo_bench   [num_splats] [iters]                # fused projection bwd + optimizer
```

A run that trains also prints a VRAM breakdown after the timing table: pool
capacity per `VramCategory` (`src/core/PoolSlots.h`), the scratch buffer, the
driver's process figure, and the twelve largest buffers. The pool never
shrinks, so those are training peaks, not the numbers at exit. Both front
ends emit it — the CLI at the end of the process, the GUI when its window
closes.
