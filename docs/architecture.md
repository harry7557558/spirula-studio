# Architecture

## Layers

```
                ┌───────────────────────────────────────────────┐
  front ends    │ spirula train (CLI)      spirula (ImGui GUI)  │
                │ src/app/cli/            src/app/gui/          │
                └────────────┬──────────────────┬───────────────┘
                             │                  │
                    ┌────────┴──────────────────┴─────────────────┐
                    │  TrainerCore (src/app/)                     │
                    │  config → dataset → seeding → step loop     │
                    └───────────────────────┬─────────────────────┘
                                            │
                 ┌──────────────────────────┴────────────────────────────────┐
   engine        │  src/engine/*.cpp — CUDA-free, process-global             │
                 │  state in EngineState.h; src/data/ owns image I/O         │
                 └──────────────────────────┬────────────────────────────────┘
                                            │  launch declarations
                 ┌──────────────────────────┴────────────────────────────────┐
   backend seam  │  src/backend/api/*.h  +  BackendRuntime.h                 │
                 └───────────┬──────────────────────────────┬────────────────┘
                             │                              │
              ┌──────────────┴────────────┐    ┌────────────┴───────────────┐
   backends   │ CUDA: src/kernels/**/*.cu │    │ Vulkan: src/backend/vulkan │
              │  + src/instantiations/    │    │  launchers + shaders/      │
              └──────────────┬────────────┘    └────────────┬───────────────┘
                             └──────────┬───────────────────┘
                                        │
                    src/shaders/*.slang — shared device math
                    src/backend/common/SortScan.h — CUB / onesweep radix
```

The seam is the important part, and it is the best-documented part of the
repo: read `src/backend/README.md`, then `src/backend/vulkan/README.md`.

## Where responsibilities live

| responsibility | code |
|---|---|
| training config (source of truth) | `src/config/TrainConfig.h` — hand-written, one X-macro row per flag. The struct, the CLI parser, `--help`, the GUI editor and `config.json` all expand from it. |
| dataset parsing (native) | `src/data/parsers/{Colmap,Nerfstudio,Metashape}Parser.cpp`, `DatasetCommon.cpp`, `DatasetParser.h` |
| image cache / prefetch / warp | `src/data/DataManager.cpp` |
| training session orchestration | `src/app/TrainerCore.cpp` — the single training driver, shared by the CLI and the GUI |
| saved presets | `src/app/gui/PresetFile.{h,cpp}` — the file header, folder and probe every kind shares; `TrainPreset`, `DatasetPreset` and `MeshPreset` add one hand-written field table each |
| batch processing | `src/app/gui/BatchProcess.{h,cpp}` — the rows, the pre-flight and the plan they expand into. The driver is `GuiApp::advance_batch()`, which reuses the three live runners rather than being a fourth |
| telling somebody a queue is over | `src/app/gui/CommandRunner.{h,cpp}` runs the command, `gui::command_argv()` (Subprocess.h) builds its argv with `{message}` replaced by a summary that carries nothing a JSON payload would have to escape |
| the input list a dataset run takes | `src/app/gui/SourceList.{h,cpp}` — one answer to "which lens is this capture", shared by the New Dataset screen and a batch row |
| what a meshing run writes | `src/mesh/MeshExport.{h,cpp}` — the writers, which colors each format can carry, and the plan a request for several of them expands into. `src/app/gui/MeshJob.h` is the GUI's side of the same request |
| reading a mesh back | `src/mesh/MeshImport.cpp` (every front end but one) and `viewer/src/viewer.cpp` (the WASM viewer, whose constraints are genuinely different) |
| the actual training step | `Engine*.cpp`, entered via `engine_train_step_managed` |
| kernels | `src/kernels/**/*.cu` (CUDA) + `src/backend/vulkan/shaders/*.slang` + `src/backend/vulkan/kernels/*.cpp` |
| web viewer client | `src/app/webviewer/viewer.html` — single source, embedded into the engine library at build time; every front end serves the same bytes |
| web viewer server | `src/app/webviewer/{Viewer,HttpServer,RenderWorker}.cpp` |
| standalone WASM viewer | `viewer/` — independent, but compiles the C++ parsers in place |

Dataset parsing, the viewer server and the training driver were each once
implemented twice, in Python and in C++. Each is now one C++ implementation:
the Python halves were deleted after a parity gate proved the pairs agreed,
and the gates survive as golden-value regression tests
([testing.md](testing.md) §4-6).

Nothing is left in Python. Checkpoint resume and its layout adaptation are
native (`src/checkpoint/`), and so is the eval pass
(`src/app/EvalMetrics.{h,cpp}`), LPIPS included: AlexNet + VGG16 run on the
inference layer (`src/lpips/`). A build without that layer leaves LPIPS to a
hand-run tool over the saved eval PNGs (`reference/python/eval_lpips.py`).

## The engine

`Engine.h` is hand-maintained (not generated) and groups its API as:
lifecycle (`engine_reset`), setup (`set_data_3dgs`, `set_camera_params`,
`set_training_data[_warped]`), forward, loss+backward, optimizer step,
bilagrid (RGB/depth/normal), PPISP, background blending, color space,
densification, fused train steps, DataManager-driven train step, debug
render, state queries, and viewer blit.

Two properties to keep in mind:

- **It is a process-global singleton.** `EngineState` holds the world splats,
  camera table, forward cache, GT data, gradients, optimizer moments, bilagrid
  / PPISP / background / color-space state and the viewer state. Call
  `engine_reset()` between runs that swap datasets — `ss_benchmark`'s
  per-scene subprocess isolation exists for exactly this reason.
- **It is CUDA-free.** `Engine*.cpp` are `.cpp`, not `.cu`;
  they reach the device only through the generated launch declarations and
  `backend/api/BackendRuntime.h`. Keep it that way — it is what lets the same
  engine drive the Vulkan backend.

## Ordering rules worth knowing

Image-space corrections compose in this order:

```
render → background blending → [PPISP] → display encode → bilagrid → [PPISP] → loss
```

Only one of the two `[PPISP]` slots runs. `--apply-ppisp-before-bilagrid`
(default on) picks between bilagrid-then-PPISP and PPISP-then-bilagrid;
`--apply-ppisp-before-color-space` moves it ahead of the working→display
conversion instead, which needs the first flag on because the bilagrid is
always applied in sRGB, and does nothing when the splats have no colour space
of their own. The backward hooks in `EngineLoss.cpp` invert whichever order
the step picked. Changing the order changes results silently, so it is
asserted only by parity tests.

## Primitives

3DGS, Mip (anti-aliased 3DGS), and 3DGUT are compile-time *types*, not runtime
branches: `Primitive.cuh`, `Primitive3DGS.cuh`, `Primitive3DGUT.cuh`,
`PrimitiveBase3DGS.cuh`. Camera model, SH degree and distortion are value
parameters. This is what makes `src/instantiations/` (111 generated instantiation TUs)
necessary on the CUDA side, and what maps to Slang specialization constants on
the Vulkan side.

The view-dependent colour is an axis of its own, orthogonal to the primitive:
a `ColorBasis` (`core/ColorBasis.h`) says what one splat's colour row holds --
SH bands, then spherical-beta lobes -- and travels with the row through the
engine and the launch APIs. On Vulkan the projection family evaluates it in
one place, `backend/vulkan/shaders/view_color.slang`, with the lobe count a
specialization constant like the SH degree; the CUDA kernels take SH only and
refuse lobes. [notes/spherical-beta.md](notes/spherical-beta.md).
