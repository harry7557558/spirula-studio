# AGENTS.md — orientation for humans and coding agents

Read this first. Detail lives in `docs/`; this file is the map and the rules.

If you are an agent writing code here, the one section you are most likely to
get wrong is **[Comments](#comments--write-fewer-and-shorter)**. Read it before
you write your first comment, not after the review.

## What this project is

**Spirula Studio**, a 3D Gaussian Splatting trainer, formerly spirulae-splat.
It began as a Nerfstudio/gsplat fork and is now a **standalone C++ codebase**:
one executable, `spirula`, with two interchangeable compute backends (CUDA and
Vulkan via Slang). There is no Python package, no pybind module and no PyTorch
anywhere — the trainer, the dataset parsers, the viewer, resume, meshing and
eval are all native. **Both backends must keep working** on every change.

The repository directory keeps the old name (the GitHub Pages URL under
`viewer/` depends on it); nothing else does.

Direction of travel, so you don't push the wrong way:

- Everything goes in C++. Don't add a Python package, a binding layer, or a
  build-time dependency on Python. The codegen tools under `tools/codegen/`
  are Python, but they are dev-time only and their output is committed, so a
  fresh checkout builds without a Python interpreter.
- Don't add a second implementation of anything. Dataset parsing, the viewer
  server, the training driver, checkpoint resume and the eval metrics each
  exist exactly once.
- `nerfstudio` / `gsplat` / `torch` are **not** dependencies and must not be
  reintroduced.
- `reference/python/` holds hand-run tools that are on no code path (LPIPS
  scoring, the benchmark driver, the pose-normalization reference). Nothing
  there may be imported by anything, and nothing in the build may need it.

## Repo map

```
CMakeLists.txt              running order only; the logic is in cmake/
cmake/                      build modules (options, backends, apps) + sources.txt,
                              the source list BOTH build systems read
build_develop.bash/.bat     the dev build entry points — USE THESE
AGENTS.md  CLAUDE.md        this file (CLAUDE.md is a pointer to it)
docs/                       architecture, build, backends, codegen, testing, notes
src/                        ALL native code (see below)
assets/fonts/               the five embedded UI faces + the full-CJK face
                              table (assets/fonts/README.md). The CJK subsets
                              are GENERATED from the catalogs -- see below
tools/codegen/              the four codegen tools (see "Codegen" below)
reference/scripts/          dataset preprocessing CLI tools (Python, standalone;
                              mask.py is embedded into the GUI binary)
reference/python/           hand-run tools on NO code path: eval_lpips.py,
                              benchmark.py, camera_utils.py (the unported
                              orientation_method / center_method reference,
                              docs/notes/pose-normalization.md)
viewer/                     standalone WebGL2 + WASM viewer, independent of training
                              (published as the online viewer -- the GitHub Pages
                               URL depends on this directory name)
                              (build-time exception: compiles src/data/parsers/*.cpp
                               in place)
```

`src/` is the include root: every local include is path-qualified relative to
it (`#include "core/Common.cuh"`, `#include "backend/api/BackendTypes.h"`), so
a file's include lines tell you which subsystems it depends on.

```
src/
├── core/                   Tensor.h, Camera.h, Common.cuh, GradQuant.cuh, …
│                             the types and device helpers everything uses
├── primitives/             Primitive*.cuh — 3DGS / Mip / 3DGUT traits
│                             (compile-time types, not runtime branches)
├── kernels/                one directory per family; each holds the launchers
│   │                         (<Name>.cu), the GENERATED declaration header
│   │                         (<Name>.cuh) and the device body (<Name>_kernel.cuh)
│   ├── projection/  raster/  tile/
│   ├── pixelwise/  ppisp/  bilagrid/     (bilagrid is 42 files, see docs/notes/)
│   ├── optim/  densify/  loss/  background/  visualize/
├── engine/                 Engine*.cpp/.h — the training engine
│                             (process-global singleton)
├── data/                   DataManager (image cache / prefetch / warp) and
│   └── parsers/              COLMAP / Nerfstudio / Metashape readers
├── mesh/                   meshing pipeline: Delaunay3D, UV, export/import, and
│                             MeshingDevice.h -- the DEVICE SEAM the portable
│                             host driver (OccupancyEvaluator.cpp,
│                             MeshingRasterHost.cpp) reaches the kernels
│                             through, so meshing runs on BOTH backends
├── sfm/                    structure from motion -- Vulkan-only, self-contained
│                             (images in, a COLMAP sparse/ model out)
│                             -- READ src/sfm/README.md
├── nn/                     the reusable GPU inference layer: its own Vulkan
│                             runtime (nn/vk/), tensor + ops + Slang kernels,
│                             host image I/O. Knows nothing about any model.
│                             -- READ src/nn/README.md
├── sam/                    SAM 2 / SAM 3 segmentation, on top of nn/
│                             -- READ src/sam/README.md
├── aliked/                 ALIKED keypoints + LightGlue, on top of nn/
│                             -- READ src/aliked/README.md
├── loma/                   LoMa: DaD keypoints + DeDoDe descriptors + the LoMa
│                             matcher, on top of nn/. The other learned SfM
│                             frontend -- READ src/loma/README.md
├── metric3d/               Metric3D v2 depth + normals, on top of nn/
│                             -- READ src/metric3d/README.md
├── moge/                   MoGe-2 point maps + normals + a sky mask, on top of
│                             nn/. The DEFAULT geometry model
│                             -- READ src/moge/README.md
├── video/                  container demux + VK_KHR_video_decode_*, on top of
│                             nn/. PATENT-GATED: compiled only with
│                             SS_ENABLE_PATENTED=ON -- READ src/video/README.md
├── backend/                the backend seam — READ backend/README.md
│   ├── api/                backend-neutral launch declarations (GENERATED forwarders)
│   ├── cuda/  common/      CUDA runtime shim, SortScan (declaration in
│   │                         common/, CUB implementation in cuda/), Profiler
│   ├── vulkan/             the whole Vulkan backend: runtime + kernels/ +
│   │                         shaders/ (its own entry points and SPIR-V build)
│   │                         — READ backend/vulkan/README.md
│   └── tests/              native cross-backend parity tests
├── shaders/                Slang device math SHARED by both backends — compiled
│                             twice, to src/generated/*.cuh and to SPIR-V
├── app/                    the ONE application, `spirula` -- every tool below in
│   │                         one executable, dispatched on argv[1]
│   ├── Tools.h  Main.cpp   the subcommand table and the only main()
│   ├── cli/                main.cpp (`spirula train`), mesh_main.cpp (mesh),
│   │                         sfm_main.cpp (sfm), sam_main.cpp (sam),
│   │                         geometry_main.cpp (depth + normals)
│   ├── FrameExtract.{h,cpp}  video -> sharp frames (`spirula sam extract` also
│   │                         masks them in the same pass; the GUI masks after)
│   ├── WriterPool.h        threads that encode/write images while the GPU runs
│   │                         the next frame; every masking loop uses it
│   ├── AppPaths.{h,cpp}    the config / cache directories and this exe's own path
│   ├── CrashLog.{h,cpp}    the stack trace every tool leaves in <config>/crash.log
│   │                         when it faults -- armed for all of them in Main.cpp
│   ├── gui/                Dear ImGui desktop app (`spirula` with no arguments)
│   ├── webviewer/          HTTP server + render worker + viewer.html (the ONE
│   │                         browser client, embedded into the engine library
│   │                         so the CLI and the GUI serve the same bytes)
│   └── TrainerCore.{h,cpp} the ONE training driver: config -> dataset ->
│                             seeding -> step loop -> eval. Both the CLI and
│                             the GUI drive this; it lives in the engine
│                             library (cmake/sources.txt), not the app targets.
├── config/                 TrainConfig.h — the training config's single source
│                             of truth: one X-macro row per flag, hand-written.
│                             TrainConfigJson.h is the one flat-JSON encoding
│                             of it, shared by config.json, --resume and the
│                             GUI's saved presets
├── i18n/                   the interface in 13 languages. A translation is a
│                             TYPE, so a missing one is a compile error, not a
│                             runtime fallback. Languages.h is the one place the
│                             language set is written -- READ src/i18n/README.md
├── app/EvalMetrics.{h,cpp} l1 / psnr / ssim (torchmetrics-compatible) and the
│                             colour correction the cc_ metrics use. LPIPS is
│                             NOT here — reference/python/eval_lpips.py
├── checkpoint/             Resume.{h,cpp} — config.json -> TrainConfig,
│                             checkpoint resolution, resumability checks;
│                             Adapt.{h,cpp} — host-side layout adaptation
│                             (the state restore itself is EngineCheckpoint.cpp)
├── generated/  instantiations/    GENERATED — do not hand-edit
└── external/               vendored (miniz, stb, npy)
```

## Building

Always use the dev scripts; they run codegen first and pick a sane job count.

```bash
# Linux
bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=cuda
bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BACKEND=vulkan   # separate build dir advised
# Windows (cmd)
build_develop.bat -DSS_BUILD_CLI=ON -DSS_BACKEND=vulkan
```

Use
`build_develop.bash`; it runs codegen first and picks a RAM-aware job count.

Everything builds into **one executable**, `build/spirula`: no arguments opens
the GUI, `spirula sfm|train|sam|mesh` are the command-line tools, and a symlink
named `spirula-sfm` runs that tool directly (`src/app/Tools.h`);
`spirula geometry` estimates depth and normals for a dataset. The GUI runs
reconstruction and that estimation by re-running itself as a child process, so
there is no sibling binary to keep next to it. `-DSS_SEPARATE_TOOLS=ON` also builds the old
per-tool executables.

Backends build into different trees; keep them separate (`-B build_cuda`,
`-B build`) so you can test both without reconfiguring. Options:
`SS_BACKEND` (`cuda`|`vulkan`), `SS_BUILD_CLI`, `SS_BUILD_GUI`,
`SS_BUILD_BACKEND_TESTS`, `SS_DEBUG_SYMBOLS`,
`SS_BUILD_SFM`, `SS_BUILD_SAM`, `SS_ENABLE_PATENTED`,
`SS_SEPARATE_TOOLS`.
Full matrix and per-platform notes: `docs/build.md`.

**`SS_ENABLE_PATENTED` is OFF by default and should stay that way in
anything you commit.** It gates `src/video/` -- the H.264 / H.265 / AV1
bitstream parsers and the VK_KHR_video_decode_* driver -- which is the only
patent-encumbered code in the tree. With it off, everything that wanted it
shells out to ffmpeg instead; no feature disappears, a subprocess appears. See
the comment on the option in `cmake/SsOptions.cmake` before changing it.

## Codegen — the invariants that bite

Generated trees are marked in `.gitattributes` (`src/generated/`,
`src/instantiations/`) and are **committed**, so a fresh checkout
builds with no Python at all. Four generators, all run from the repo root.
Every one of them reads C++/CUDA sources — **no generator reads Python**, and
none should be added:

| generator | reads | writes |
|---|---|---|
| `tools/codegen/generate_headers.py` | `/*[AutoHeaderGeneratorExport]*/` markers in `src/kernels/**/*.cu` | the declaration section of the matching `<Name>.cuh` |
| `tools/codegen/generate_kernel_instantiation.py` | kernel decls in `src/kernels/**/*_kernel.cuh` | `src/instantiations/*.cu` |
| `tools/codegen/generate_backend_api.py` | per-kernel `.cuh` headers | `src/backend/api/*.h` forwarders |
| `tools/codegen/generate_vulkan_stubs.py` | link-probes the Vulkan build | throwing stubs for unported kernels |

Rules:

1. **Never hand-edit below the `AUTO HEADER GENERATOR — DO NOT EDIT` splitter
   line** in a `.cuh`. Everything above it is yours; everything below is
   regenerated from the `.cu`.
2. To export a launch function to the engine, put
   `/*[AutoHeaderGeneratorExport]*/` immediately above its definition and
   rerun `tools/codegen/generate_headers.py`.
3. **A header can be fed by several `.cu` files**, listed explicitly in
   `HEADER_SOURCES` in `tools/codegen/generate_headers.py`. This is the sanctioned way to
   split a large `.cu`: name each part after **what it does**, not after the
   header it feeds (e.g. `ImageWarp.cu`, `DepthGeometry.cu` → `PixelWise.cuh`),
   and add it to the list. Both build systems glob via `cmake/sources.txt`, so
   no build file changes are needed. A listed file that doesn't exist is a hard error,
   so a rename can't silently drop declarations.
4. `src/config/TrainConfig.h` is the **single source of truth** for the
   training config, and it is hand-written, not generated. Adding a row to
   `SS_CONFIG_FIELDS` makes the field appear in the native CLI, `--help`,
   the GUI's "All Options" editor, the run's `config.json`, the GUI's saved
   presets and `TrainerCore` —
   the struct is expanded from the same table, so the two cannot drift. A row
   also carries a `section` (which heading it is listed under) and a `tier`
   (`basic` / `advanced` / `expert` / `stub`, which decides whether `--help`
   and the GUI's default view show it at all); both are display metadata, and
   `config.json` is flat so neither reaches disk. What the row does NOT carry
   is the flag's human name and help sentence: those are translated and live
   in `src/i18n/catalog/TrainFields.h`, resolved by member name at compile
   time, so a row without an entry there fails to build.
5. `.cuh` declaration sections must stay CUDA-include-free — they have to
   parse under `-DSS_BACKEND_VULKAN` without the CUDA toolkit.

## The Vulkan-only subsystems

`src/sfm/`, `src/nn/`, `src/sam/`, `src/aliked/`, `src/loma/`,
`src/metric3d/`, `src/moge/` and `src/video/` are **not** part of the
two-backend rule below. They are Vulkan + Slang only, carry their own Vulkan
context, share nothing with the training engine, and are absent from a CUDA
build by default (`SS_BUILD_SFM` / `SS_BUILD_SAM` default OFF there).
Nothing in them goes through `cmake/sources.txt`.

The layering runs one way and must keep doing so:

```
app/gui, app/cli ──► sam ──────┬──► nn ──► nn/vk
                 ├──► aliked ──┤
                 ├──► loma ────┤
                 ├──► metric3d ┤
                 ├──► moge ────┤
                 └──► video ───┘
                 └──► sfm  (its own vk/, independent of nn/)
```

`nn/` is the piece meant to be reused. A learned feature detector for SfM, or a
depth/geometry model, goes on top of it unchanged -- so model-specific
constants, weights formats and pipeline policy stay in `sam/` (or the next
`src/<model>/`), never in `nn/`.

## The two-backend rule

Every kernel-level change needs **three** things, or the Vulkan build breaks
or silently diverges:

1. the CUDA implementation (`src/kernels/<family>/<Kernel>.cu` + `_kernel.cuh`),
2. the Slang implementation (`cuda/slang/vulkan/*.slang`) and its launcher
   (`src/backend/vulkan/kernels/*.cpp`),
3. a parity test in `src/backend/tests/` that runs both and compares.

If the Vulkan side isn't ready, `tools/codegen/generate_vulkan_stubs.py` emits a throwing
stub so the portable engine still links — that's a deliberate TODO marker, not
a finished state. Coverage is tracked in `src/backend/vulkan/README.md`,
which is the authoritative and unusually detailed document on the Vulkan
design (device baseline, capability variants, memory model, atomics). Read it
before touching anything under `backend/vulkan/`.

## Testing

```bash
# native parity tests (CUDA build)
bash build_develop.bash -DSS_BUILD_BACKEND_TESTS=ON && ./build/<test_name>
# the Vulkan build produces the same test binaries unconditionally
```

Each `src/backend/tests/*.cpp` becomes an executable of the same name.
`backend/tests/engine/*` drive the real engine end to end. Details and the
CUDA-vs-Vulkan reference-dump workflow: `docs/testing.md`.

## Comments — write fewer, and shorter

This section exists because the codebase is 14% comment by line and a lot of
that is agent-written narration nobody needs. **A comment is a cost.** It is
read every time the code is read, it is not compiled or tested, and it rots
silently the moment the code under it changes. The default is *no comment*;
lines earn their place one at a time.

The one test: **could a competent reader recover this from the code?** If yes,
delete it. Comment the *why* — the alternative you rejected, the number you
measured, the invariant the compiler cannot state. Never the *what*.

### Do not write

- **Narration of the code below.** `// Loop over the cameras` above the loop,
  `// Constructor` above the constructor, a comment listing a class's five
  constructors when the five declarations are twenty lines down, a comment
  tabulating an enum that is defined in the header next door. The declaration
  is the documentation; keeping a second copy in prose guarantees they drift.
- **Archaeology.** This began as a Python fork; that Python is *gone*. Do not
  write "port of <that file>, lines 547-559", "mirrors <that other file>",
  "the Python path did X". A citation into a file that does not exist cannot
  be checked by anyone and reads as authority it has not got. Real
  provenance — the ONNX operator we match, torchmetrics' SSIM convention,
  COLMAP's `ba_global_images_ratio` — is fine and stays, because those
  references are live and checkable.
- **Changelog.** "Previously X, now Y", "this used to be", "was changed to".
  Git knows. When you change code, **rewrite the comment to describe what is
  true now** — never append a paragraph about what changed.
- **Deliberate omissions.** "`Cameras::__getitem__` is intentionally NOT
  ported", "rescale is omitted for the same reason". Absent code needs no
  comment; if the omission is a real design decision, it belongs in `docs/` or
  the subsystem README once, not in a header forever.
- **Meta-narration and reassurance.** "Note that", "It is important to",
  "This ensures correctness", "for clarity", "just to be safe". Either the
  point is load-bearing — then state it flatly in one line — or it is filler.
- **Tutorial voice.** No "we", "our", "let's", "you can now", "first…
  second… finally". Write the fact, not the walkthrough.
- **Essays where a doc belongs.** A thirty-line architecture lecture at the top
  of a header is a document that has escaped `docs/`. Put it in `docs/` or the
  subsystem README and leave a one-line pointer.

### Do write

Rationale that cannot be recovered any other way, and keep it dense:

- A measured number and what measured it — `src/sfm/map/Mapper.h`'s tuning
  constants are the model: each carries the dataset and the effect size that
  chose it. That is worth every line it takes.
- A footgun with teeth: the gridDim 65535 fold, `thread_local` under OpenMP,
  code 0 decoding to exactly `0.0`, an ImGui ID pinned to a message name.
- A cross-file invariant the compiler cannot enforce — "must match the layout
  in `<file>`", a Slang/CUDA ABI constraint, an ordering requirement between
  two calls.
- Units, frames and conventions on a bare number: what space, which handedness,
  whose pixel-center offset.

### Budget

Enforced, not advisory — the build fails on a block over budget:

- file header: **≤ 10 lines** — what this file is for and its one non-obvious
  constraint; everything longer goes to `docs/` or the subsystem README
- a block above a function or a constant: **≤ 3 lines**
- an inline note: **1 line**, and prefer a better name over the note

Section banners (see Conventions below) are exempt — they are structure, not
prose. So is an untitled horizontal rule used to separate top-level
definitions, which several subsystems use consistently. Neither a divider rule
nor a blank `//` counts toward a block; lines carrying prose do.

Two scripts guard this, and both fail the build:

- `tools/check_comment_length.py` counts the budget above over every block an
  **uncommitted change touches**, staged or not. The standing debt in files
  you did not open never blocks you; `--all` lists it for a deliberate cleanup
  pass. It runs from `cmake/SsChecks.cmake`, so a bare `ninja` fails the same
  way `build_develop.bash` does, and it skips itself when git or python is
  missing (neither is a build dependency). `SS_SKIP_COMMENT_CHECK=1` skips one
  build — needing it twice means the comment wanted cutting.
- `tools/check_comments.sh` enforces the other part a script can decide: every
  file a comment names must exist. A citation into a deleted file is
  unfollowable, and the reader cannot tell it from a live one.

Neither script can tell narration from rationale. A four-line block that
survives by losing its fourth line has not been fixed.

### When you edit a file

Cleaning up as you go is the whole plan; there is no separate cleanup pass
coming. If you touch a function whose comment breaks a rule above, fix the
comment in the same diff. Deleting a stale comment needs no justification and
no ceremony — do not ask, do not leave a note saying you removed it.

## Conventions

- Files are named after **what they do**, not after the file they were split
  out of, and not after the header they declare into.
- `<Name>_kernel.cuh` is reserved: it means "the device body that
  `tools/codegen/generate_kernel_instantiation.py` instantiates". Don't use the `_kernel`
  suffix for an ordinary shared-helper header — give it a descriptive name
  (e.g. `BilinearSample.cuh`).
- A `<Family>Common.cuh` holds the shared preamble when a family spans several
  translation units.
- Section banners inside long files:
  ```
  // ================
  // Section Name
  // ================
  ```
  These are load-bearing for the split workflow — keep them meaningful.
- Slang device math is shared by both backends; don't fork it per backend.
- A `.cpp` (as opposed to `.cu`) under `src/` means "portable, compiles for Vulkan
  builds too". Keep the engine layer in `.cpp` and CUDA-free.
- **Macros, CMake options and environment variables are `SS_`-prefixed.** The
  prefix is short enough to collide with `<signal.h>` and `winuser.h`, so
  `tools/check_ss_prefix.sh` (run by `build_develop.bash`) refuses any name in
  `tools/ss_reserved_names.txt`. Read environment variables through
  `spirula::env("SUFFIX")` (`src/core/Env.h`), never `getenv` directly — the
  deprecated `SSPLAT_` spelling is honoured in exactly that one function.
- **An error message that cites a source line uses `SS_FILE`, never `__FILE__`**
  (`src/core/SourcePath.h`). `__FILE__` is the absolute path on the machine that
  built the binary; `SS_FILE` trims it to `src/...` at compile time, the same on
  every toolchain. `tools/check_file_macro.sh` (run by `build_develop.bash`)
  refuses the bare macro.
- C++ code lives in `namespace spirula` where it is namespaced at all.
- **The GUI never hands ImGui a string literal.** Every text-bearing call goes
  through `ui::` (`src/app/gui/Ui.h`): `ui::Button(msg)` for interface copy,
  `ui::ButtonRaw(...)` for a path, a number or an engine log line. The `Raw` in
  the name is the point — it makes "deliberately not translated" visible in the
  diff. `tools/check_i18n.sh` (run by `build_develop.bash`) enforces it.
- **Interface copy is a `Msg`, not a literal** (`src/i18n/`, and read
  `src/i18n/README.md` before adding one). Two rules there are expensive to
  retrofit and are already followed everywhere: never build a sentence from
  fragments — use `{0}` placeholders and `i18n::format()` — and never write a
  plural-sensitive sentence ("Objects: 3", not "3 objects").
- **A training flag's name and help are interface copy too**
  (`src/i18n/catalog/TrainFields.h`, one `SS_MSG` pair per row of
  `SS_CONFIG_FIELDS`). What stays untranslated is the *identifier*:
  `--sh-degree` is `--sh-degree` in every language, and so are the `choices`
  values, which `config.json` stores and the help text quotes in backticks.
  Where those values are ordinary words the GUI shows the translation in
  front of the value rather than instead of it — "按间隔 (interval)".
  `spirula train --help` reads the same catalog, so it follows `--lang`.
- **The command line is interface copy too, and so is everything a run
  prints.** A terminal has no language picker to fix it with afterwards, and
  the GUI *shows* a terminal: `spirula sfm` and `spirula mesh` run inside it as
  child processes launched with `--lang`. `spirula --help` and the trainer's
  output are `catalog/Cli.h`; a reconstruction is `catalog/Sfm.h` through
  `src/sfm/core/Log.h`; a meshing run is `catalog/Mesh.h` through
  `src/mesh/MeshLog.h`; reading a dataset is `catalog/Data.h`. `--help` is
  included: `spirula sfm --help` alone is 120 flag descriptions
  (`catalog/SfmFields.h`) plus its command pages (`catalog/SfmHelp.h`), and
  `spirula sam --help` is `catalog/SamHelp.h`.
- **Help text is stored one string per paragraph and wrapped by
  `i18n::wrap()`, never by hand.** It measures in terminal columns rather than
  bytes, and it breaks BETWEEN CHARACTERS in the languages that have no spaces
  to break at -- without which a Chinese paragraph is one line four hundred
  columns wide. `i18n::pad_to()` lays out the flag column the same way.
- **A parent that reads a child's output matches on the MESSAGE, never on
  English.** `i18n::scan()` is the inverse of `i18n::format()`: hand it the
  same `Msg` the child printed with and it gives back what stood in the `{0}`,
  `{1}` positions. That is how `src/app/gui/MeshRunner.cpp` drives its progress
  bar off a Japanese child. Two rules follow for any message a parent reads:
  keep a separator between adjacent placeholders, and do not reorder the
  numbers past what every translation can live with.
- **A mask prompt is English, not interface copy.** SAM 3's text encoder reads
  English, so the field, its placeholder and the palette's inserted words stay
  English in every language; `ui::InputTextEnglish()` is the wrapper that says
  so, and `src/app/gui/MaskPrompt.h` is how a non-English speaker still writes
  a good prompt. The same reasoning keeps THIRD-PARTY child-process output
  (COLMAP, ffmpeg) untranslated — our stage names around it are
  `i18n/catalog/Log.h`, its own lines are not ours to rewrite. `spirula sfm`
  is ours, so it *is* translated: every line a default run prints goes through
  `src/sfm/core/Log.h`, which puts a localized, equal-width `[tag]` in front
  of a localized message (`i18n/catalog/Sfm.h`). `spirula mesh` is ours in the
  same way. What stays English in both is the deep diagnostics — `--check`,
  `--audit`, `SS_SFM_MAP_PROF`, `SS_MESH_DEBUG_RENDER` — and `spirula sam`'s
  machine-readable detection table, which a script reads.

## Gotchas worth knowing before you hit them

- **The quantized codecs in `core/Tensor.h` are host-callable.** They carry
  `__device__`, but that is an empty macro in host translation units on both
  backends, and their bodies are plain `<cmath>` math. Host-side checkpoint
  adaptation (`checkpoint/Adapt.cpp`) decodes and re-encodes through those very
  functions, so there is no host mirror to drift. If you add a codec, keep it
  free of CUDA intrinsics for the same reason.
- **Quantized gradient codecs must decode code 0 to exactly `0.0`.** Anything
  else and Adam amplifies the pseudo-gradient into visible floaters.
- **`isfinite` has no spelling that works everywhere a `core/` header goes.**
  MSVC declares `std::isfinite` as a host function template, so nvcc will not
  call it from `__device__` code; gcc's `<cmath>` leaves nothing in the global
  namespace for an unqualified call inside a template to find. `q_finite`
  (`core/Tensor.h`) is the exponent test that does. Same for `isnan`/`isinf`.
- **Never name a `thread_local` inside an `omp parallel` region.** The team
  threads are different threads, so each one resolves it to its *own* copy —
  which the calling thread never sized. Keep the storage `thread_local` if you
  want it reused, but take a raw pointer outside the region and use that
  inside; `EvalMetrics.cpp`'s SSIM slab is the worked example.
- **A `Msg` is 13 strings, and the compiler checks all 13.** Adding a language
  to `src/i18n/Languages.h` breaks the build on every incomplete message, by
  name -- that is the feature, not an accident. Add the tag macro to
  `BeginCatalog.h` in the same commit or the canary there fails first.
- **ImGui derives a widget's ID from its label**, so a translated label would
  change the ID and collapse every open header on a language switch. Every
  `ui::` wrapper renders `"text###<message name>"` to pin it. Two call sites
  sharing one message share an ID -- `ImGui::PushID()` around one of them,
  exactly as for two identical literals.
- **Swapping a font face invalidates every `ImFont*`.** `FontSet::ensure()` is
  called from `GuiMain.cpp` between frames, before `NewFrame()`, and uses
  `ImFontAtlas::ClearFonts()` (which keeps the texture) rather than `Clear()`
  (which does not, and is documented as not callable there).
- **The embedded CJK fonts are subset to the characters the catalogs use.**
  Write a translation containing a character no translation used before and
  they are stale, and the symptom is one hollow box mid-sentence.
  `tools/check_font_coverage.py` runs on every `build_develop.bash` and fails
  with the codepoint; `python3 tools/make_ui_font.py` regenerates all five
  faces. All four regional subsets are merged into the atlas with the current
  language's region FIRST -- first source wins per codepoint, which is what
  gives each language its own Han forms while the others still supply hangul
  and the simplified-only characters no single face has.
- **Big per-call scratch is a scaling bug, not just an allocation.** Anything
  over glibc's mmap threshold is faulted in and zeroed by the kernel on every
  call, and `mmap_lock` is per *process* — so several worker threads each
  allocating tens of MB serialize against each other. Reuse the buffer.
- **Scripted runs need `--keep-viewer-alive 0`**, or training hangs at exit
  waiting on the viewer.
- **A kernel with one slot per image must fold its grid.** CUDA caps
  `gridDim.y/z` at 65535 and Vulkan caps *every* dispatch dimension at 65535,
  so a per-image axis put on y/z dies somewhere between 8k and 40k images.
  Fold it across two dimensions (`vkk::fold_1d`, or `bilagrid_tv_grid` in
  `kernels/bilagrid/BilagridConfig.cuh` for the 3D case) and pass the fold
  factor to the kernel. Related: the bilagrid samplers index cells with
  int32, which `engine_init_bilagrid_*` enforces up front.
- **Some pool buffers do not own their memory.** A slot listed in
  `POOL_ALIAS_TABLE` (`core/PoolSlots.h`) is a slice of one arena that every
  `PoolPhase` reuses, so the arena costs the largest phase rather than the sum
  (626 MiB on a 15M-splat step). Acquiring one outside its phase throws;
  `SS_POOL_ALIAS_POISON=1` fills the arena at every phase switch so a read
  that outlives its phase becomes NaNs a parity test catches, and
  `SS_POOL_ALIAS=0` turns the whole thing off. Read
  `docs/notes/vram-splat-x-img.md` before adding a row.
- **`SS_PROFILE=1`** enables the per-stage backend timing breakdown
  (H2D / D2H / D2D / memset / device / host), header-only, both backends, plus
  a per-category VRAM breakdown after any run that trained. What the biggest
  category costs per element, and what has already been tried against it, is
  `docs/notes/vram-splat-x-img.md`.
- **A kernel that fills the gaps between sorted keys is a scaling trap.** One
  thread per INPUT walking `[ids[i-1]+1, ids[i]]` is fine while the ids are
  dense and quadratic when they are not: `qgrad_camera_id_bounds` measured
  478 ms/call at 20M splats with 0.8M visible, 92% of the whole step. One
  thread per OUTPUT doing a `lower_bound` is the shape that does not care.
- **The engine is a process-global singleton.** Call `engine_reset()` between
  runs that swap datasets, or the new run inherits the old splats, camera
  table, optimizer moments and color-space matrices. The one exception is
  read-only: `engine_scene_*` keeps several splat SETS resident and binds one
  per render, which is how the GUI shows four models side by side
  (`docs/notes/compare-view.md`).
- **`build/_deps` on WSL drvfs** may need `git config safe.directory` entries
  for the FetchContent'd GLFW/imgui.
- **A static library whose only content is a static initializer is not
  linked.** The generated SPIR-V blob tables are archive members that nothing
  references, so registration is an explicit call at each library's entry point
  (`NN_ENSURE_EMBEDDED_MODULES`, `src/nn/vk/EmbeddedSpirv.h`), not a
  namespace-scope constructor. A new shader directory needs its own
  declare/ensure pair or the kernels come back "no shader module".
- **Three Vulkan devices can be live in one process** -- the engine's, the SfM
  module's and the inference layer's. The GUI sequences them (mask preview,
  then reconstruction as a *child process*, then training) so no two overlap;
  do not casually make two of them concurrent. Converging is
  `docs/notes/sfm-port-plan.md` phase 6.
- **The camera models exist twice, on purpose, and only twice.** The device
  copy is `shaders/projection_utils.slang`; the host copy is
  `data/CameraMath.h`, which the GUI's frustum wireframe and `spirula
  geometry`'s resampling both call. A third copy is a bug waiting to be found
  by nobody -- `spirula geometry --check` is what tests the host one, by
  round-tripping an analytic plane through every camera model.
- **"Is this lens too wide for one pinhole?" is asked in three places and must
  get the same answer.** `spirula geometry --split auto` (CLI and GUI panel)
  splits a frame into pinhole faces and then writes RAY depth; the trainer's
  `--input-depth-is-ray-depth`, left unset, has to know that happened, and the
  two never meet. All of them call `camhost::splits_to_pinhole_faces`, which
  owns the 0.75 threshold and is deliberately blind to the distortion
  coefficients so the answer cannot move between builds. `--check` compares the
  closed form to counting the pixels.
- **Two monocular geometry models sit behind `spirula geometry`, and they
  answer different questions.** Metric3D v2 predicts depth; MoGe-2 predicts an
  affine POINT MAP that needs the camera's focal length to become one, plus a
  metric scale and a sky mask. `app/GeometryModel.h` is the one seam between
  them and `--model` is what picks; a caller that reaches past it into
  `metric3d::` or `moge::` has hard-coded a family.
- **Segmentation weights are never committed or bundled.** They are Meta's,
  under Meta's licences, and SAM 3's is not GPLv3-compatible. They are fetched
  at run time after the user has seen the terms -- `src/app/gui/ModelCache.cpp`
  is where that policy lives, and it is the only place that should grow one.
- **The inference layer's VRAM pool is process-wide and grow-only**, so
  destroying a `sam::Session` frees nothing by itself and a 2 GB checkpoint
  stays resident until the process exits. `Session::unload()` (called by the
  destructor) releases it; anything that keys a pool slot per instance must
  namespace the key per owner, as `Tracker`'s memory bank does -- two trackers
  numbering slots from zero write over each other.
- **A GUI worker that clears a `busy` flag at the end of its function will
  strand it.** Every early `return set_error(...)` skips the line, and the next
  request is refused forever. Use a scope guard (`SegmentPanel::start_job`).
- **One SAM prompt selects one object.** Points are hints about a single thing:
  a prompt holding a click on the dog and a click on the bicycle returns a mask
  that fits neither. Several objects means several instances
  (`sam::SeedPrompt::object`), unioned afterwards. And a click belongs to the
  frame it was drawn on -- reusing one across a moving capture is the bug that
  looks like a working feature.
- **Masking's remaining headroom is in staging, not arithmetic.** The memory
  bank was always there, and tensor cores now are: `gemm_coop.slang` and
  `attention_coop.slang` run on `VK_KHR_cooperative_matrix` where the device has
  it, worth ~1.4x end to end. Read `src/nn/README.md`'s "Cooperative matrix"
  before touching either -- the multiplies measure 45 TFLOP/s in isolation
  against 8.3 for the fp32 GEMM, so what decides the kernel is how its operands
  reach it, and two plausible tilings measured *slower* than the fp32 kernel
  they replace. The other thing worth taking was the host side -- PNG encoding
  and JPEG decoding are a third of a frame, and belong on `app/WriterPool.h`,
  not on the thread feeding the GPU.
- **A shader that declares a device capability needs its own module.** SPIR-V
  capabilities are per module and `vkCreateShaderModule` may reject the whole
  blob, so a cooperative-matrix kernel cannot live next to a portable one --
  `Pipelines` creates a `VkShaderModule` lazily, on first acquire of an entry
  from it, which is what keeps `CooperativeMatrixKHR` off a device that lacks
  it. Probe in `nn::vk::Context`, gate the dispatch, and leave the fp32 kernel
  as the only path everywhere else.

## Do not commit

Local dataset paths, private capture names, personal directory layouts
(`/mnt/...`, `/home/<user>/...`, `C:\Users\...`). Standard academic dataset
names (e.g. Mip-NeRF 360, ZipNeRF) are fine. Run
`bash tools/check_private_paths.sh` before pushing.
