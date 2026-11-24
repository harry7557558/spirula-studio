# AGENTS.md

## Project objectives

Spirula Studio is a native Gaussian splatting application for turning photos
and video into splats, meshes, and renders. Aim for fast end-to-end workflows,
low peak RAM and VRAM on large scenes, and reliable operation across supported
GPUs. Make the desktop and CLI easy to use with clear defaults, actionable errors,
and predictable handling of saved work.

Keep native application code in C++ and preserve compatibility with existing
datasets, presets, and checkpoints. Share implementations across entry points,
including dataset parsing, training, checkpoint resume, and evaluation.
A checkout must build with its committed generated sources without Python.
Python tools are for development and reference work. Keep PyTorch, Nerfstudio,
and gsplat out of the native build and runtime dependencies.

For performance-sensitive changes, compare wall time, peak RAM and VRAM, and
output quality on a representative workload. Count temporary allocations and
concurrent workers in resource budgets. See the [performance recorder](tools/perf/README.md).

## Repo map

```text
cmake/                 build options, targets, and shared training source list
docs/                  architecture, build, testing, and subsystem notes
src/
  app/                 CLI, desktop GUI, embedded web viewer, TrainerCore
  config/              shared training config and serialization
  core/                host and device types, memory, and utilities
  data/                dataset parsers, image loading, and scene data
  engine/  mesh/       training engine and meshing drivers
  checkpoint/          resume and checkpoint adaptation
  kernels/             CUDA launchers and device bodies
  backend/             portable API, backend runtimes, Vulkan kernels, parity tests
  shaders/             Slang device math shared by both training backends
  generated/          generated CUDA headers, alongside instantiations/ for CUDA TUs
  sfm/  nn/            reconstruction and shared inference, with their own runtimes
  i18n/                interface messages and localization
tools/                 development tools, including codegen and GUI automation
reference/             standalone reference and preprocessing tools
viewer/                standalone WebGL/WASM viewer using the native dataset parsers
```

Use `src/`-relative paths for local includes in native code. Start from the
[docs index](docs/README.md) and [architecture guide](docs/architecture.md).
Read the subsystem README before changing its internals.

## Build and verify

Use the dev scripts for regeneration, checks, and build parallelism. Select the
backend explicitly. Linux and Windows use separate backend build trees.
macOS uses Vulkan through MoltenVK and builds into `build/`.

```sh
bash build_develop.bash -DSS_BACKEND=vulkan
bash build_develop.bash -DSS_BACKEND=cuda -DSS_BUILD_BACKEND_TESTS=ON
```

On Windows, use `build_develop.bat` with the same CMake options. The main
executable is `spirula`. It opens the desktop app with no arguments when the GUI
is enabled and dispatches subcommands through `src/app/Main.cpp`.

Build and verify affected shared training and meshing paths on CUDA and Vulkan.
Include parity coverage for shared kernel changes under `src/backend/tests/`.
CUDA parity tools are opt-in and Vulkan builds them by default. See the
[test matrix](docs/testing.md#what-to-run-before-calling-a-change-done).
For training scripts that serve the viewer, use `--keep-viewer-alive 0` to exit
when training ends.

## Architecture and generated code

- Route GUI and CLI training through `TrainerCore`. Keep the engine and its
  launch declarations CUDA-free. Call `engine_reset()` between training runs
  that swap datasets because the engine is process-global.
- Keep shared device math in `src/shaders/`. CUDA launchers live in
  `src/kernels/`. Vulkan entry points and launchers live in
  `src/backend/vulkan/shaders/` and `src/backend/vulkan/kernels/`. Register
  new Vulkan training entries in `src/engine/StepTimingTable.h`. Read the
  [backend seam](src/backend/README.md) and [Vulkan guide](src/backend/vulkan/README.md).
- Keep codecs in `src/core/Tensor.h` host-callable for checkpoint adaptation.
  Quantized gradients must decode code zero to exactly `0.0`.
- Edit generator inputs and regenerate committed outputs. Keep edits above the
  auto-header splitter in `.cuh` files. Mark exported launchers with
  `/*[AutoHeaderGeneratorExport]*/`. Update `HEADER_SOURCES` when adding or
  splitting a launcher file. Reserve `_kernel.cuh` for device bodies consumed
  by the instantiation generator.
- The dev scripts regenerate launch declarations and CUDA instantiations when
  Python is available. Regenerate backend API forwarders separately when
  needed. Shared Slang edits also require regenerating the committed CUDA
  headers with `src/shaders/build.bash`. See [code generation](docs/codegen.md)
  for commands and the required Slang version.
- Keep SfM and inference on their own runtimes. They are enabled by default in
  Vulkan builds and opt-in for CUDA builds. Native GUI SfM runs in process by
  default and has a subprocess option. Keep model formats and policy in their
  model or app layers and keep `nn/` reusable. See the [SfM guide](src/sfm/README.md)
  and [inference guide](src/nn/README.md), including its macOS Metal path.
- Add training options through `SS_CONFIG_FIELDS` in `src/config/TrainConfig.h`
  and `src/i18n/catalog/TrainFields.h`. Check the Dataset and Mesh preset
  tables when changing their job settings. Update the corresponding
  `DatasetPlan` fields when a setting changes which dataset outputs can be
  reused. See [dataset reruns](docs/notes/dataset-rerun.md).

## Memory and file I/O

- Use `src/core/HostMemory.h` for host budgets so container and job limits are
  included. Account for Windows commit limits and overlapping GPU runtimes.
  See [memory forecasts](docs/notes/memory-forecast.md).
- Reuse large scratch allocations. For caller-owned `thread_local` scratch
  shared with an OpenMP team, resolve the pointer before entering the region.
  Training pool aliases are valid only in their owning phase. When changing
  allocations, keep the [phase arena](docs/notes/vram-splat-x-img.md#the-phase-arena)
  and [training forecast](docs/notes/train-forecast.md#vram) in sync.
- Use a reusable 8–32 MiB buffer for repeated sequential file I/O, with
  16 MiB as the default. Use `BufferedInputFile` and `BufferedOutputFile` from
  [BufferedFile.h](src/core/BufferedFile.h), or `kFileBufferBytes` for explicit
  chunk loops. Count each stream and worker against the memory budget. Small
  metadata transfers, mapped files, sparse seeks, and already-chunked paths
  can keep their existing buffering. Budgeted paths may use smaller buffers.
- Preserve seek and EOF behavior and timely flushing of live logs and process
  protocols. Check flush or close before publishing output. Run the affected
  format tests and `buffered_file_test` for changes to the stream helpers.

## Comments — write fewer, and shorter

Write comments for non-obvious invariants, units, ordering constraints, or
measured tradeoffs. Skip narration, changelog, and references to deleted code.
Keep long rationale in the relevant subsystem doc. Clean up stale or redundant
comments in functions you edit. Name files after their responsibility.

### Budget

File headers get at most 10 lines, other blocks 3, and inline notes 1. Section
banners, divider rules, and blank comment lines are exempt. The build checks
changed blocks with `tools/check_comment_length.py`. The dev script also
checks that files named in comments exist. See [build lints](docs/build.md#lints).

## UI text — keep it short

- Put GUI and CLI copy in `src/i18n/catalog/`. Use `ui::` wrappers for
  text-bearing ImGui calls and the `Raw` variants for paths and other dynamic
  text. Translate whole messages with placeholders and use labels for counts
  to avoid plural handling. Keep CLI flag names and stored choice values
  stable across languages.
- Aim for 1–2 rendered lines of help and cap ordinary prose at 3. Preserve
  consequences, units, and prerequisites. Lists can have up to 4 bullets,
  2 wrapped lines per bullet, and 8 lines total including the overview.
- Keep English terminal templates ASCII after catalog punctuation conversion
  and preserve formatted arguments. Match localized child output with
  `i18n::scan()` when the parent reads a message back.
- Run `python3 tools/check_ui_text.py` and review changed copy at its actual
  width across languages. See the [i18n guide](src/i18n/README.md) for
  placeholders, widget IDs, font coverage, and reviewed length exceptions.

## Repository conventions

- Use `SS_` for project macros, CMake options, and environment variables.
  Read these variables through `spirula::env("SUFFIX")`. Use `SS_FILE` for
  source locations in diagnostics. Use namespace `spirula` for namespaced C++.
- Use the existing GUI and CLI consent paths for gated model downloads.
  Keep segmentation weights out of commits and bundles. See the
  [model license registry](src/app/ModelLicenses.h).

## Do not commit

Keep local dataset paths, private capture names, and personal directory
layouts out of commits. Run `bash tools/check_private_paths.sh` before pushing.
