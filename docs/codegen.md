# Code generation

Generated trees are marked `linguist-generated` / `linguist-vendored` in
`.gitattributes` and are **committed**. A fresh checkout therefore builds with
no Python at all — the dev build scripts run codegen when `python3` is
available and fall back to the committed files when it isn't.

Every generator reads C++/CUDA sources. **None reads Python**, and none should
be added: the training config used to be generated from the Python dataclasses
and is now hand-written in `src/config/TrainConfig.h`.

Generated locations:

```
src/generated/       device-math headers from Slang
src/backend/api/     backend module forwarders
src/instantiations/  kernel instantiation TUs
```

All generators run from the **repo root**:

```bash
python3 tools/codegen/generate_headers.py
python3 tools/codegen/generate_kernel_instantiation.py
python3 tools/codegen/generate_backend_api.py
# generate_vulkan_stubs.py takes arguments; see below
```

The one exception is `src/shaders/build.bash`, which runs from `src/` and needs
`slangc` **2026.2.1** on PATH; it refuses any other version (the slangc the
Vulkan build fetches is newer and does not reproduce the committed headers). **The dev build scripts do NOT run it.** Editing anything
under `src/shaders/` and skipping it leaves the two backends computing
different things: Vulkan compiles the Slang, CUDA compiles the committed
`src/generated/*.cuh`.

---

## `generate_headers.py` — `.cu` → `.cuh` declarations

Scans the `.cu` files listed in `HEADER_SOURCES` for functions preceded by the marker

```cpp
/*[AutoHeaderGeneratorExport]*/
void my_kernel_launcher(...) { ... }
```

and rewrites the declaration section of the matching `<Name>.cuh`.

**Invariants:**

1. Each `.cuh` has a splitter line:
   ```
   /* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */
   ```
   Everything **above** it is hand-written (includes, types, preamble) and
   preserved. Everything **below** is regenerated. Never edit below it.
2. `#if 0` blocks are stripped before scanning, so a disabled function is not
   exported.
3. **Sources are listed explicitly.** `HEADER_SOURCES` in the script maps each
   header stem to the ordered list of `.cu` files that declare into it. One
   header may be fed by many sources, and a source file's name is therefore
   free — name it after what it does. A listed file that does not exist raises
   `FileNotFoundError`, so a rename cannot silently drop declarations.
4. **A new kernel family needs an entry in `HEADER_SOURCES`.**
5. Declaration sections must stay CUDA-include-free — they must parse under
   `-DSS_BACKEND_VULKAN` with no CUDA toolkit present. Use
   `backend/api/BackendTypes.h` for vector types.

### Splitting a large `.cu` (the sanctioned workflow)

Because of invariant 3, splitting is nearly free:

```bash
# carve the sections out along the // ==== banners, naming each part after
# what it does -- not after the file it came from:
#   PixelWise.cu -> ImageConvert.cu, ImageColorOps.cu, DepthGeometry.cu,
#                   ImageDistort.cu, ImageWarp.cu, GtDepthNormalWarp.cu, Ppisp.cu
# put the shared preamble in <Family>Common.cuh and shared device helpers in a
# descriptively named header (BilinearSample.cuh), then:
#   - add the new files to HEADER_SOURCES['PixelWise']
python3 tools/codegen/generate_headers.py   # PixelWise.cuh's contents are unchanged
```

CMake picks up new kernel files through the `CONFIGURE_DEPENDS` globs in
`cmake/sources.txt`. Callers keep using the same header and declarations.

Naming rules when splitting:

- Name each part after its **function**, not its origin — `ImageWarp.cu`, not
  `PixelWise.Warp.cu`.
- `<Name>_kernel.cuh` is **reserved** for device bodies consumed by
  `generate_kernel_instantiation.py`. A plain shared-helper header gets a
  descriptive name instead.
- `<Family>Common.cuh` for the shared preamble (slang namespace includes,
  host-side view helpers).

---

## `generate_kernel_instantiation.py` — explicit template instantiations

Reads kernel declarations out of `src/kernels/**/*.cuh` and emits one small TU
per (primitive × camera model × SH degree × …) combination into
`src/instantiations/`
(currently 111 files). This keeps each nvcc edge small and parallelizable
rather than instantiating everything in one TU.

---

## The training config — *not* generated

`src/config/TrainConfig.h` is hand-written and is the training config's single
source of truth. It holds:

- `SS_CONFIG_FIELDS(X)` — one X-macro row per flag,
  `(type, member, default, section, tier, choices)`. The flag's NAME and help
  sentence are translated and live in `src/i18n/catalog/TrainFields.h`, keyed
  by member and resolved at compile time;
- `struct TrainConfig` — **expanded from that same table**, so a field cannot
  exist in one and not the other;
- `kTrainPresets` + `train_apply_preset()` — one branch per preset.

The table is expanded by the CLI's generic parser and `--help` printer, the
GUI's "All Options" editor, `TrainerCore`'s `config.json` dump and the resume
path's `config.json` reader. Add a row and the flag appears in all of them.

The CLI flag is `member` stringified (`--sh-degree` sets `sh_degree`; `-` and
`_` are interchangeable), so a flag name cannot drift from its member. The
`config.json` key is the flag, spelled the same way, at the top level: the
file is flat. It used to nest under the `group` column, which made a
presentational choice part of an on-disk format — moving a flag to another
heading moved its key, and the reader (`spirula mesh`, `--resume`) silently
fell back to the default when it could not find it. `section` and `tier` are
display metadata only and can be reshuffled freely.

It is hand-written: nothing generates it, and nothing is generated from it
except the struct and the tables `SS_CONFIG_FIELDS` expands into.

---

## `generate_backend_api.py` — backend module headers

Emits `src/backend/api/*.h` (`Projection.h`, `Rasterization.h`,
`PixelWise.h`, …) as **forwarders** that `#include` the relevant per-kernel
`.cuh` headers, grouped by subsystem.

Forwarding rather than copying declarations is deliberate: it keeps one
definition of every preamble type, so a TU may include both a module header
and an individual `.cuh` without ODR violations.

---

## `generate_vulkan_stubs.py` — link-probe for unported kernels

```bash
python3 tools/codegen/generate_vulkan_stubs.py <build-vulkan-dir> <output.cpp>
```

Link-probes `csrc_portable`'s objects against `libss_backend_vulkan.a`,
collects undefined C++ symbols, finds each function's declaration in the
per-kernel `.cuh` headers, and emits one **throwing** definition per function
(`backend/vulkan/kernels/TrainingStubs.gen.cpp`). This lets the portable
engine link today and lose stubs one module at a time as ports land.

Symbols with no header declaration (class methods, templates) are listed in
the generated file's header comment for manual stubbing in
`TrainingStubsManual.cpp`.

Rerun it whenever the engine gains a new kernel call, **or when a port lands**
— a ported symbol would otherwise be defined twice.

---

## Non-script generation done by CMake

- `viewer.html` → `app_generated/viewer_html.h` (byte array), so
  `spirula train` and the GUI serve the viewer from a self-contained binary.
  Regenerated at configure time when the HTML changes. `Viewer.cpp` has a dev
  override so HTML edits don't require a rebuild.
- Slang → SPIR-V blobs → `vk_shaders_embedded.cpp`
  (`backend/vulkan/shaders/SpirvShaders.cmake` +
  `backend/vulkan/shaders/spirv_tool.cpp`). SPIR-V is **never committed**; one `slangc` edge per blob, with capability variants
  (`.atomicadd`, `.noint64`, `.int8`) compiled per entry point.
