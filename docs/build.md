# Building

`CMakeLists.txt` + `cmake/` builds every artifact: the engine, both
backends, the CLI, the GUI, the meshing tool and the parity tests. There is
no second build system and nothing to keep in sync with one.

The source list lives apart from it, in `cmake/sources.txt`: a plain list of
glob patterns relative to the repo root, read line-by-line by
`cmake/SsSources.cmake`. Adding a source directory means editing that file
once.

Two files that belong to `src/app/` are in that list on purpose:
`app/webviewer/*.cpp` and `app/TrainerCore.cpp`. They live in the engine
library so every app target shares one build of them rather than each
compiling its own copy.

Always build through the dev scripts.

## CMake layout

`CMakeLists.txt` is just the running order; the logic lives in `cmake/`:

| file | what it does |
|---|---|
| `SsOptions.cmake` | options (`SS_BUILD_CLI/GUI`, `SS_DEBUG_SYMBOLS`, …), backend selection, tree paths (`SS_ROOT`, `SS_CSRC`, …) |
| `SsSources.cmake` | `ss_collect_sources()` — expands `sources.txt` with `CONFIGURE_DEPENDS` |
| `SsBackendCuda.cmake` | Torch probe, CUDA arch detection, flags, the `csrc` library, CUDA-side parity tools |
| `SsBackendVulkan.cmake` | portable engine object lib, slangc + SPIR-V embed, `ss_backend_vulkan`, the Vulkan-side tests |
| `SsSlang.cmake` | `ss_find_slangc()` — pinned version, PATH lookup, fetch on miss |
| `SsEmbed.cmake` | `ss_embed_file()` — bake a file into a byte-array header |
| `SsApps.cmake` | the `spirula` executable — every tool the build has, in one binary (backend-agnostic) |
| `SsPackage.cmake` | the `macos_app` / `macos_dmg` targets ([Packaging](#packaging)) |
| `SsChecks.cmake` | the source lints ([Lints](#lints)) — included last, so every target above depends on them |

Exactly one backend module runs. It leaves behind `SS_WITH_TORCH` and
`SS_APP_LIBS`, which is the whole contract `SsApps.cmake` depends on.

## Dev builds

```bash
# Linux
bash build_develop.bash [cmake args...]
# Windows (plain cmd prompt; locates VS via vswhere, calls vcvars64)
build_develop.bat [cmake args...]
```

Both run codegen first (skipped gracefully if `python3` is missing — the
generated files are committed), then the [lints](#lints), then configure +
build into `build/`. `build_develop.bash` additionally caps the job count by
available RAM (~750 MB/job).

## The matrix

| goal | command |
|---|---|
| CUDA CLI + GUI | `bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=cuda` |
| Vulkan CLI + GUI | `bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=vulkan` |
| parity tests | add `-DSS_BUILD_BACKEND_TESTS=ON` (Vulkan builds them unconditionally) |
| Vulkan GUI, everything on | `bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON` |

Keep the two backends in **separate build directories** so you can test both
without reconfiguring, e.g. `-B build_cuda` and `-B build`.

## Options

| option | default | effect |
|---|---|---|
| `SS_BACKEND` | `cuda` | `cuda` \| `vulkan`. `vulkan` builds the portable engine layer + `backend/vulkan/` **without the CUDA toolkit**, and forces `SS_BUILD_CLI=ON`. |
| `SS_BUILD_CLI` | `OFF` | the command-line tools (`spirula train`, `spirula mesh`) |
| `SS_BUILD_GUI` | `OFF` | the graphical application (`spirula` with no arguments); FetchContent's GLFW 3.4 + Dear ImGui v1.92.8 (needs network once) |
| `SS_SEPARATE_TOOLS` | `OFF` | *also* build `spirula-sfm` and `spirula-sam` standalone — same code, but neither links the engine (24 MB vs the combined 61 MB) |
| `SS_BUILD_BACKEND_TESTS` | `OFF` | build `backend/tests/*` (CUDA branch; Vulkan always builds them) |
| `SS_DEBUG_SYMBOLS` | `OFF` | host `-g`, CUDA cubin lineinfo, `slangc -g2`. Bloats binaries substantially — profiling/debugging only. |
| `SS_CUDA_EMBED_PTX` | `OFF` | embed PTX beside the cubin in the CUDA fatbin. Only buys JIT onto an architecture the binary was not built for, and costs about a third of every object; the build already detects the local GPU. Turn on for a redistributable binary. |
| `SS_SLANGC` | *(empty)* | path to a `slangc` to use; empty means find on PATH and fetch the pinned release on miss/mismatch |
| `SS_BUILD_SFM` | `ON` for `vulkan`, `OFF` for `cuda` | `ss_sfm` + `spirula sfm` + `sfm_*_test`. Vulkan-only; a CUDA build can opt in if the Vulkan SDK is present. |
| `SS_BUILD_SAM` | `ON` for `vulkan`, `OFF` for `cuda` | `ss_nn` + `ss_sam` + `spirula sam` + `nn_ops_test` / `sam_pipeline_test`, and the GUI's in-process masking. Same rule as SfM. |
| `SS_ENABLE_PATENTED` | `OFF` | `ss_video` — container demux + `VK_KHR_video_decode_*`. **Read the note below before turning it on.** |
| `SS_SFM_REALS` / `SS_SFM_LOSSES` | all | trim the bundle-adjustment shader variant matrix while iterating (`src/sfm/README.md`) |
| `SS_CHECK_COMMENTS` | `ON` | run the comment-length lint on every build ([Lints](#lints)) |

### `SS_ENABLE_PATENTED`

Off by default, and deliberately. This repository is GPLv3, and `src/video/` —
the H.264 / H.265 / AV1 bitstream parsers and the Vulkan Video driver — is the
one part of it carrying third-party patent exposure (H.264/H.265 via MPEG LA
and Access Advance, AV1 via the claims asserted against AOMedia). With it off,
that directory is neither compiled nor linked, and everything that wanted it
falls back to an external **ffmpeg**: `spirula sam extract` and `spirula sam
video` say so and exit, and the GUI extracts frames with ffmpeg and tells the
user why.

Turning it on buys in-process GPU decoding: roughly 15× faster frame
extraction (a 127-second 1080p30 clip in ten seconds rather than minutes),
`spirula sam extract`'s masking riding along on the same device pass, and no
ffmpeg to install. Nothing else in the build changes.

If you distribute binaries, decide for your jurisdiction and your users before
shipping one built with it on.

## Lints

Six checks guard the source. Each fails the build, and each skips itself when
its interpreter is missing — none of them is a dependency of a fresh checkout.

| check | what it refuses |
|---|---|
| `tools/check_ss_prefix.sh` | an `SS_*` name `<signal.h>` or `winuser.h` already owns (`tools/ss_reserved_names.txt`) |
| `tools/check_i18n.sh` | a text-bearing ImGui call that skipped the `ui::` wrappers |
| `tools/check_font_coverage.py` | a translation that outgrew the embedded font subsets |
| `tools/check_comments.sh` | a comment citing a file that is not in the tree |
| `tools/check_file_macro.sh` | a bare `__FILE__` where [`SS_FILE`](../src/core/SourcePath.h) belongs |
| `tools/check_comment_length.py` | a comment block over the [budget](../AGENTS.md#budget) |

The first five run from `build_develop.bash` only. The comment-length check
also runs from CMake (`cmake/SsChecks.cmake`) as the `ss_check_comment_length`
target, which every other target is made to depend on — so a bare `ninja`, a
`make`, or `cmake --build build --target spirula` fails on it just as the dev
scripts do. `build_develop.bat` runs it directly, and the standalone viewer
build includes the same module.

### The comment-length check

It reads the **uncommitted** diff — staged, unstaged and untracked alike — and
flags only the comment blocks those changes touch. Standing debt in a file you
did not open never blocks a build; `--all` lists it for a deliberate cleanup
pass. Divider rules, blank `//` lines and section banners do not count toward
a block.

```bash
python3 tools/check_comment_length.py          # what the build will say
python3 tools/check_comment_length.py --all    # the whole tree, for cleanup
SS_SKIP_COMMENT_CHECK=1 ninja -C build         # skip it for one build
cmake -B build -DSS_CHECK_COMMENTS=OFF         # or for a whole build tree
```

Neither escape hatch is a fix: the comment is still over budget, and the next
person to touch that file inherits it.

## Targets

`spirula` — one executable holding every tool this build has, dispatching on
its first argument (`src/app/Tools.h`):

| | |
|---|---|
| `spirula` | the graphical application |
| `spirula <file-or-folder>` | the same, opening what was named |
| `spirula train` | the CLI trainer |
| `spirula sfm` | structure from motion |
| `spirula sam` | segmentation, tracking and frame extraction |
| `spirula mesh` | meshing (CUDA backend only) |

A copy or symlink named `spirula-sfm` runs that tool directly, so scripts
written against the separate executables keep working; `SS_SEPARATE_TOOLS`
builds those executables for real.

Libraries: `csrc` (engine; SHARED with Torch, STATIC without), `csrc_portable`
(Vulkan branch's torch-free engine objects), `ss_backend_vulkan`,
`ss_sfm`, `ss_nn`, `ss_sam`, `ss_video`, plus one executable
per file in `backend/tests/`, `backend/tests/engine/`, `backend/vulkan/tests/`,
`sfm/tests/`, `nn/tests/` and `sam/tests/`.

The GUI runs reconstruction by re-running **itself** (`spirula sfm auto ...`) as
a child process -- see `src/app/gui/SfmRunner.h` for why that stage and only
that stage is out of process. Masking, decoding and training are in-process.

### Known: `SS_SFM_REALS=df` does not compile on Windows

slangc 2026.12.0.1 hits an internal error on all three `ba_df_*` variants under
Windows (`error[E99998]: Slang compilation aborted due to internal error`),
deterministically; `float` and `double` are fine, and all three build on Linux.
The double-float real is not the default (`--ba-real` defaults to `double`), so
the workaround costs nothing on that platform:

```bat
build_develop.bat "-DSS_SFM_REALS=float;double"
```

`spirula sfm` then reports "variant not built into this binary" if something
asks for `df`. Not yet reduced to a minimal repro or filed upstream.

### Known: `sfm_mask_test` fails on Windows

Its mask-discovery case writes both `e.jpg.jpeg` and `e.jpg.JPEG` and asserts
which one is resolved. On a case-insensitive filesystem those are the same
file, so the assertion cannot hold — and a real Windows dataset cannot contain
both spellings either, so this is the test's assumption failing, not the
lookup. Every other `sfm_*_test`, `nn_ops_test` and `sam_pipeline_test` passes
on Windows.

## Backend specifics

**CUDA.** Needs a recent CUDA toolkit (+ a compatible MSVC on Windows).
Architectures come from `nvidia-smi --query-gpu=compute_cap`; override with
`-DCMAKE_CUDA_ARCHITECTURES`.

**Vulkan.** Needs the Vulkan SDK. No Python and no CUDA toolkit required.
Slang is pinned to a specific version (`SS_SLANG_VERSION` in
`cmake/SsSlang.cmake`); if the `slangc` on PATH doesn't match, CMake fetches the
pinned release. SPIR-V blobs are **never committed** — they are compiled at
build time (one `slangc` edge per blob, see `src/backend/vulkan/shaders/SpirvShaders.cmake`) and
embedded into the binary. On an offline machine, transfer a matching `slangc`
and point `-DSS_SLANGC=` at it.

**macOS.** Vulkan backend only, through MoltenVK; `build_develop.bash` works
as on Linux. Dependencies: `brew install cmake ninja`. Four things are
macOS-only in the build: `cmake/SsVulkan.cmake` fetches a pinned universal
MoltenVK and links it *statically* (`SS_MACOS_VULKAN=static`, the default) so
the binary carries its own driver and copies to any Mac — the release tarball
supplies the Vulkan headers too, so nothing comes from Homebrew;
`cmake/SsSlang.cmake` pins a different Slang release (the one this project
pins publishes no macOS assets); `build_develop.bash` reads free memory from
`vm_stat` rather than `/proc`; and `ss_i18n` links CoreFoundation, which
`i18n/Locale.cpp` asks for the user's locale.

A static build has no loader, so it cannot load validation layers.
`-DSS_MACOS_VULKAN=loader` links the installed loader instead (needs
`brew install molten-vk vulkan-headers vulkan-loader`); the binary is then
tied to that install and is not redistributable. `-DSS_MOLTENVK_DIR=` points
at an unpacked `MoltenVK-macos` release for an offline build.

```bash
export CXXFLAGS="-nostdinc++ -isystem $(xcrun --show-sdk-path)/usr/include/c++/v1"
```

**Windows.** `build_develop.bat` always calls `vcvars64` even when `cl` is
already on PATH (an ambient `cl`/`INCLUDE` may reference an uninstalled SDK),
picks the newest installed CUDA toolkit unless `CUDA_PATH` is set, falls back
to the VS-bundled CMake/Ninja —
the torch extension build is not supported on Windows, and a broken torch
install aborts configure from inside `TorchConfig.cmake` (which `QUIET` cannot
suppress).

## Packaging

Only macOS has a packaging step, because only macOS has a form the binary is
not already in. A Linux or Windows build is one file that runs where it lands;
a Mac wants an `.app`, or the Dock shows a Terminal icon and Finder has no way
to launch it.

```bash
cmake --build build --target macos_app   # build/Spirula Studio.app
cmake --build build --target macos_dmg   # ... and the disk image around it
```

Both run `tools/package_macos.sh`, which is also usable directly
(`--build-dir`, `--out`, `--sign IDENTITY`, `--dmg`). It assembles the bundle
from the built `spirula` plus an icon resampled from `assets/icon.png` — via
`sips` and `iconutil`, which are in the base system, so packaging needs no
Xcode.

### The icon

`tools/make_icon.py` renders everything in `assets/` and is the only thing
that writes them; it needs `pip install taichi`, takes a few minutes, and
nothing in the build or in packaging imports it. Each platform takes a
different form:

| file | who reads it |
|---|---|
| `icon.png` | 1024², macOS canvas (824 of artwork, rounded). `package_macos.sh` resamples it into the bundle's `.icns` |
| `icon.ico` | compiled into the `.exe` by `src/app/app.rc`. The resource is named `GLFW_ICON`, which is what GLFW's Win32 backend looks up for the window class — so Explorer, the taskbar and the window all get it from this one line |
| `icon_128.png` | embedded and handed to `glfwSetWindowIcon` on X11 (`GuiMain.cpp`). Not called on Windows, which already has the resource, nor on macOS, where GLFW answers `GLFW_FEATURE_UNAVAILABLE` — a window there has no icon of its own |
| `banner.png` | embedded, and drawn across the top of the GUI's home screen (`GuiApp::draw_home_banner`). Same shape and camera as the icon, framed wide and close |

The derived forms are committed rather than generated, because a Windows build
must not need Python. The two desktop ones are cropped past the macOS inset,
which would otherwise leave them a tenth smaller than every neighbour in a
taskbar. The banner carries no text — the product name and tagline are drawn
over it by ImGui, so they stay translatable.

The bundle carries **one binary**. That is only honest because a default
macOS build links MoltenVK statically (`cmake/SsVulkan.cmake`), and the script
checks rather than trusts it: `otool -L` output naming anything outside
`/usr/lib` or `/System/Library` fails the packaging, since a bundle missing a
dylib works on the build machine and nowhere else.

Signing is ad-hoc (`--sign -`) by default. That is not optional decoration:
Apple silicon kills an unsigned arm64 binary on exec, and copying the
executable into the bundle invalidates whatever signature it had. It is enough
for a Mac the app is copied to directly, and *not* enough for one it is
downloaded to — the quarantine flag then wants a Developer ID signature and a
notarized bundle:

```bash
bash tools/package_macos.sh --build-dir build --dmg --sign "Developer ID Application: ..."
xcrun notarytool submit "build/Spirula Studio.dmg" --keychain-profile ... --wait
xcrun stapler staple "build/Spirula Studio.dmg"
```

Notarization needs a paid Apple Developer account, so it is not wired into the
build.

One behaviour is bundle-specific: a Finder launch inherits launchd's PATH
(`/usr/bin:/bin:/usr/sbin:/sbin`), which has no Homebrew in it, so COLMAP,
ffmpeg and python3 would be missing from an app that finds them fine when
started from a shell. `gui::add_desktop_search_paths()`
(`src/app/AppPaths.h`) appends the package managers' directories at
startup, after any PATH the process actually inherited.

## Build-time cost

The long poles are the biggest `.cu` translation units and the CUDA template
instantiation set in `src/instantiations/` (111 files). Splitting an oversized `.cu`
into function-named parts (see [codegen.md](codegen.md)) both improves
readability and parallelizes the build — no build-file edits needed, since
both builds glob `cmake/sources.txt`. Only `HEADER_SOURCES` in
`generate_headers.py` has to learn the new file.

### Re-running the build script with nothing changed must be a no-op

`build_develop.bash` / `.bat` re-run `cmake -B build` on every invocation, so
the whole configure step executes every time. The invariant that keeps
incremental builds cheap is that configure must not **touch** a file the build
reads unless its content actually changed — and `file(WRITE)` rewrites
unconditionally, giving the file a fresh mtime and rebuilding everything
downstream of it. Write generated files with `ss_write_if_different()`
(`SsOptions.cmake`) instead. It is what `ss_embed_file()`, `ss_cjk_faces()` and
the four SPIR-V `blobs.txt` list files use.

Check it after touching anything under `cmake/`: run the build script twice and
the second run must print `ninja: no work to do.`

Two failure modes look identical from the outside — a full rebuild on an
unedited tree — so `ninja -C build -d explain` is the tool that tells them
apart:

- `output ... older than most recent input <file>` — a configure-time write
  that should have gone through `ss_write_if_different()`.
- `stored deps info out of date for ...`, on *every* object, with a
  `ninja: warning: premature end of file; recovering` near the top — a corrupt
  `build/.ninja_deps`. Ninja's recovery truncates the log but not past the bad
  record, so the log never heals on its own: each build's header dependencies
  are discarded when the next build loads it back. The build scripts detect
  this and repair it with `ninja -t recompact`; by hand,
  `cmake --build build -- -t recompact` (or just delete `build/.ninja_deps`).
