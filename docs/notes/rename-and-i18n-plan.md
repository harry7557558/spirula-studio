# Spirula Studio: rename, localization, and the end of the Python backend

Plan for three changes that look independent but share one keystone file:

1. **Localize** the application into 13 locales
   (en, ja, zh-Hans, zh-Hant, ko, de, fr, es, pt, it, nl, ru, tr).
2. **Rename** `spirulae-splat` → **Spirula Studio**, `SSPLAT_*` → `SS_*`, and
   give each locale an official product name.
3. **Retire the Python/PyTorch client.** Anything generated *from* Python
   becomes hand-written C++; a named subset of Python survives as reference
   for features that are not ported yet.

Status: **phases 0-4 landed; 5 is still plan.** Sections that landed are
marked and record what actually happened, which is not always what was
planned. The mechanism and the operations manual now live in
`src/i18n/README.md` and `docs/i18n.md`; what remains here is the history and
the reasoning behind the choices.

---

## 1. The keystone: `src/app/generated/cli_config.h`

All three changes converge on one generated file. It is produced by
`tools/codegen/generate_cli_config.py` from the Python config dataclasses, it
holds **192 English help strings**, and it defines `SsplatConfig` plus the
`SSPLAT_CONFIG_FIELDS` X-macro that the CLI parser and the GUI options editor
both expand.

So: it is the Python dependency (change 3), it is a third of the translation
surface (change 1), and it carries two of the renamed identifiers (change 2).
Migrating it first means doing that work once. Migrating it last means doing it
three times.

Note that `generate_cli_config.py` is the **only** one of the five generators
that reads Python *source*. The other four read `.cu`/`.cuh` and are ordinary
build tooling whose outputs are committed — they are not part of "the Python
backend" and stay. See `docs/codegen.md`.

**Phase order: 0 → 1 → 2 → 3 → 4.** Rename before localizing (renaming 1,400
catalog entries afterwards is pure waste); localize after the Python client is
gone (otherwise the CLI has two config surfaces to keep in sync).

---

## 2. Measured surface

Numbers from the tree as of this writing, to size the work honestly.

| surface | candidate literals | translate? |
|---|---|---|
| `src/app/gui/` (10.3k LOC) | ~800 | yes — the bulk of it |
| `src/app/cli/` | ~460 | yes — usage/help |
| `cli_config.h` field help | 192 | yes, last tier |
| `src/app/webviewer/` + `viewer.html` | ~40 | later |
| `viewer/js/` (standalone WebGL viewer) | ~161 | separate mechanism, out of scope here |
| `src/engine/ src/sfm/ src/sam/ src/nn/ src/mesh/ src/data/` | ~1,300 | **no** — diagnostics, stay English |

The grep counts literals with two or more words; perhaps half of the GUI's 800
are real UI copy and the rest are ImGui IDs, format strings and paths. Working
estimate: **~1,400 translatable messages**, i.e. ~16,800 strings across the
twelve non-English locales.

`SSPLAT_` appears on **601 lines in 95 files**: CMake options, 23 `getenv`
names, internal CMake variables, include guards and compile-time macros.

---

## 3. Phase 0 — config source of truth moves to C++ — **LANDED 2026-08-04**

`tools/codegen/generate_cli_config.py` and `src/app/generated/cli_config.h`
are deleted. `src/config/TrainConfig.h` is hand-written and is now the source
of truth: 190 rows of

```cpp
// X(type, member, default, group, choices, help)
#define SSPLAT_CONFIG_FIELDS(X)                                               \
    X(int, num_iterations, 30000, "trainer", "",                              \
      "Number of training iterations")                                        \
    ...
```

with `struct SsplatConfig` **expanded from the same table**, so a field cannot
exist in one and not the other — the drift the generator existed to prevent.

Four deviations from the plan as written, each for the same reason (§11: don't
edit code you are about to delete):

- **`cli_key` dropped**, derived as `#member` instead. It was identical to the
  member in all 190 rows; a derivable column is exactly the duplication this
  phase is removing. Every expansion site stringifies instead.
- **`pyname` dropped**, but it was *not* only a Python artifact: it is the key
  schema for the run's `config.json`, which `ssplat mesh` and `--resume` read
  back. It differed from the flag in exactly one row, so it became the
  `ssplat_json_key()` shim — `dm_split_batch` → `split_batch`, documented as
  compatibility rather than a mechanism.
- **`help` stays a literal**, not a `Msg` reference. `Msg` does not exist until
  Phase 3; the table gets rewritten then.
- **`SsplatConfig` keeps its name.** Renaming it to `TrainConfig` now would
  touch `bind_trainer.cpp` and `native_trainer.py`, both deleted in Phase 1,
  and would muddy the byte-identical gate below. Moved to the Phase 2
  checklist as an explicit item (the `Ssplat` → `Ss` sed does not produce
  `TrainConfig` on its own).

Verified: `train --help` byte-identical for **all 7 presets** against the last
generated build (covers every default, preset override, group ordering,
choices list and help string), a run's `config.json` byte-identical, CUDA and
Vulkan builds green, and `bind_trainer.cpp` compiles.

Not touched: `scripts/`, which Phase 0 turned out not to reach.

Follow-ups this surfaced, deliberately left alone to keep the gate clean:

- The 33 `optimizer`-group fields have **empty help strings** — `OptimizerConfig`
  never had docstrings. They are the worst entries in `--help` and should be
  written before Phase 4 translates anything.
- The `meshing` preset's help said "Use `spirulae-meshing`", a binary that no
  longer exists. *(Fixed in Phase 2: `spirula mesh`.)*

---

## 4. Phase 1 — retiring the Python client — **LANDED 2026-08-04**

Resume and its layout adaptation landed 2026-08-04 (`src/checkpoint/`, 991
lines). Two findings changed the shape of the work from what §4 assumed:

- **Most of `resume.py` was already native.** The heavy lift —
  `engine_load_checkpoint` — has been C++ all along; resume.py was config
  reconstruction, path resolution and a state.json read. The C++ version reuses
  `data/Json.h` (which already handles Python's `Infinity` extension) and
  `core/CheckpointIO.h`'s tar/npy readers, so it is 189 lines, not 218.
- **`resume_codecs.py` did not need porting at all.** Its 170 lines mirrored
  the block quantization codecs in `core/Tensor.h`, which were `__device__`-only
  behind an `#ifdef __CUDACC__`. The bodies are pure `<cmath>` math with no CUDA
  intrinsics, so the guard was widened instead and the host adaptation now calls
  the engine's own codec. A mirror that cannot drift beats a mirror with a
  parity test.

Verified against the Python implementation it replaces, on a real checkpoint
with quantized SH (16-bit values / 8-bit Adam), FPBO, a quantized bilagrid and
PPISP: cap_max shrink, SH degree shrink and grow, appearance-module drop,
no-op detection, and bilagrid resolution changes both up and down. Every
member agrees; the float grid resample is **bit-identical** to
`scipy.ndimage.zoom(order=1, mode="nearest")`, and the only nonzero residuals
are quantized buffers differing by at most ~1.5 levels because the C++ encoder
rounds in float32 where NumPy rounded in float64 — the C++ side is the more
faithful one, since float32 is what the device kernels use.

The **eval pass** landed the same day (`src/app/EvalMetrics.{h,cpp}` +
`TrainerSession::eval`). Scope was cut deliberately: l1/psnr/ssim and the
colour-corrected `cc_` variants are native; LPIPS is the one metric with a
model behind it and stays a hand-run tool
(`reference/python/eval_lpips.py`) over the PNGs `--save-eval-images` writes.
Scoring from 8-bit PNGs makes the numbers reproducible from a run directory
alone.

Verified against torchmetrics on 13 real eval pairs: l1/psnr to 1e-7, ssim to
1e-5. Two findings worth keeping:

- **torchmetrics' SSIM reflect-pads and averages over the full HxW.** It crops
  the border back off only on its `return_contrast_sensitivity` path. A
  valid-interior SSIM reads ~0.5% different — enough to invalidate a benchmark
  comparison, and the first version of this port had exactly that bug.
- **The C++ colour correction is more accurate than the Python it replaces.**
  Against a full float64 reference it is off by 3e-8; `fused_bilagrid`'s is off
  by 1.4e-3, because torch accumulates the normal equations in float32. That
  also removes `fused_bilagrid` — an undeclared external dependency — from the
  eval path.

Still open: the deletions (§4 items 3-5).

The 9.9k lines of `spirulae_splat/` are mostly the torch client path, which
`TrainerCore` already replaced. What genuinely has no C++ counterpart:

| Python | LOC | disposition |
|---|---|---|
| ~~`resume.py` + `resume_codecs.py` + `resume_adapt.py`~~ | ~~728~~ | **done** → `src/checkpoint/` (2026-08-04). |
| ~~`lpips.py`~~ | ~~530~~ | **deleted** — it was a vendored torchmetrics copy that nothing imported (`model.py` used torchmetrics directly). LPIPS is now `reference/python/eval_lpips.py`; the rest of eval is native. |
| `camera_utils.py` | 680 | **keep as reference** — already on no code path; the `orientation_method` / `center_method` reference (`docs/notes/pose-normalization.md`). |
| `enhancer.py`, `resample.py`, `edge_detector.py`, `debug_image.py` | ~700 | audit individually; most are thin wrappers over `_C` and die with the binding. |
| `model.py`, `trainer.py`, `core.py`, `dataset.py`, `datamanager.py`, `dataparser.py`, `splat/` | ~4,700 | **delete** once Phase 0 + the two ports land. |

Steps:

1. Port resume. This is the single blocker — until it lands, Python is the only
   way to continue an interrupted run.
2. Port LPIPS onto `nn/`. Cross-check against the Python implementation on a
   fixed image pair before deleting it (a parity gate in the style of
   `docs/testing.md` §4-6).
3. Move survivors to **`reference/python/`** at the repo root — outside the
   importable package, absent from `pyproject.toml`, with a README stating they
   are on no code path. The current `spirulae_splat/modules/` layout makes
   accidental imports possible; this makes them impossible and `grep`
   unambiguous.
4. Delete `spirulae_splat/`, `setup.py`, the `ss_*` console scripts, and
   `src/bindings/` (144 `m.def`s). Drop `SSPLAT_NO_TORCH` / `SSPLAT_WITH_TORCH`
   and the torch half of `cmake/SsplatBackendCuda.cmake`.
5. Update AGENTS.md §"What this project is" — the "the Python path must keep
   working" rule is retired, and `tests/python/` goes with it.

**Do not rename the Python package.** It is being deleted; renaming it first is
work spent on a corpse.

---

## 5. Phase 2 — the rename — **LANDED 2026-08-04**

What landed differs from §5.1 in one deliberate way: **the repository keeps
its name.** Renaming it moves the GitHub Pages URL that `viewer/` is published
under, and GitHub does not reliably redirect Pages project paths. The README
already carries the transition ("Formerly Spirulae-Splat"), so the cost was
all downside. Every other row of the table below landed as written.

Three findings worth keeping:

- **`SS_` did not collide with anything.** All 114 `SSPLAT_` names map onto
  free `SS_` names — the lint (`tools/check_ss_prefix.sh`, 47 declarations
  checked) exists to stop the *next* one, not to clean up this one.
- **The `getenv` consolidation was worth doing on its own.** 24 scattered
  reads became `spirula::env("SUFFIX")`, which is where the `SSPLAT_` fallback
  now lives — one function, one deletion when the shim expires. Two ad-hoc
  helpers (`nn/vk/Context.cpp`'s `envFlag`, `sfm_ba.cpp`'s `env_or`) collapsed
  into it.
- **Two on-disk directories were user data, not identifiers.** `~/.config/…`
  and the ALIKED model cache moved to `spirula-studio/`, but an existing
  `spirulae-splat/` is adopted where it is rather than migrated: no move, no
  data loss, and a repeat 100 MB+ checkpoint download avoided.

Verified: both backends build clean; `spirula train --help` byte-identical
across all 7 presets and `spirula mesh --help` byte-identical, modulo the
mechanical `ssplat`→`spirula` substitution; a stale `SSPLAT_*` CMake cache
reconfigures with deprecation warnings and the right values; `SS_X` wins over
`SSPLAT_X` and `SSPLAT_X` alone warns once; train → eval → resume on a real
scene.

### 5.1 Names

| thing | from | to |
|---|---|---|
| product | spirulae-splat | **Spirula Studio** |
| repository | `spirulae-splat` | *(unchanged — see above)* |
| executable | `ssplat` | `spirula` (`spirula train\|sfm\|sam\|mesh`; symlink `spirula-sfm`) |
| macro / option / env prefix | `SSPLAT_` | `SS_` |
| CMake modules | `cmake/Ssplat*.cmake` | `cmake/Ss*.cmake` |
| config type | `SsplatConfig` | `TrainConfig` (Phase 0) |
| C++ namespace | `ssplat` (6 files) | `spirula` |

Leave `viewer/` alone — the GitHub Pages URL depends on the directory name.
This is why the repository was not renamed: GitHub redirects git remotes and
web UI paths on rename but does not reliably redirect Pages project paths.

### 5.2 `SS_` and the avoid list

`SS_` is short enough to collide with system headers, and the two that matter
are both on our platforms:

- **`<signal.h>`** (glibc, macOS): `SS_ONSTACK`, `SS_DISABLE`.
- **`winuser.h`** (Windows SDK) — static-control styles:
  `SS_LEFT`, `SS_CENTER`, `SS_RIGHT`, `SS_ICON`, `SS_BLACKRECT`,
  `SS_GRAYRECT`, `SS_WHITERECT`, `SS_BLACKFRAME`, `SS_GRAYFRAME`,
  `SS_WHITEFRAME`, `SS_USERITEM`, `SS_SIMPLE`, `SS_LEFTNOWORDWRAP`,
  `SS_OWNERDRAW`, `SS_BITMAP`, `SS_ENHMETAFILE`, `SS_ETCHEDHORZ`,
  `SS_ETCHEDVERT`, `SS_ETCHEDFRAME`, `SS_TYPEMASK`, `SS_REALSIZECONTROL`,
  `SS_NOPREFIX`, `SS_NOTIFY`, `SS_CENTERIMAGE`, `SS_RIGHTJUST`,
  `SS_REALSIZEIMAGE`, `SS_SUNKEN`, `SS_EDITCONTROL`, `SS_ENDELLIPSIS`,
  `SS_PATHELLIPSIS`, `SS_WORDELLIPSIS`, `SS_ELLIPSISMASK`.

The GUI includes `winuser.h` transitively through GLFW, and the webviewer's
`HttpServer.cpp` includes `winsock2.h`, so both are live. Regenerate the list
against the SDK you ship with rather than trusting this one:

```bash
grep -hoE '^\s*#\s*define\s+SS_[A-Z0-9_]+' <sdk>/winuser.h | awk '{print $NF}' | sort -u
```

Enforcement: `tools/check_ss_prefix.sh`, run from `build_develop.bash`, greps
our own `#define SS_*`, `option(SS_*` and `set(SS_*` declarations against
`tools/ss_reserved_names.txt` and fails on a hit. None of the current 95
`SSPLAT_` names maps onto a reserved one, so the initial rename is clean — the
lint exists to stop the next one.

### 5.3 Compatibility shim

Two shims, both designed to be deletable in a single commit one release later.

**CMake options and cache variables** — a table-driven loop in
`cmake/SsOptions.cmake`, ahead of the `option()` declarations:

```cmake
set(SS_LEGACY_OPTIONS
    BUILD_CLI BUILD_GUI BUILD_SFM BUILD_SAM BUILD_BACKEND_TESTS
    BACKEND SEPARATE_TOOLS DEBUG_SYMBOLS ENABLE_PATENTED)
foreach(opt ${SS_LEGACY_OPTIONS})
    if(DEFINED SSPLAT_${opt} AND NOT DEFINED SS_${opt})
        message(DEPRECATION
            "SSPLAT_${opt} is deprecated; use SS_${opt}. "
            "The alias will be removed in the release after next.")
        set(SS_${opt} "${SSPLAT_${opt}}" CACHE STRING "" FORCE)
    endif()
endforeach()
```

`SSPLAT_NO_TORCH` and `SSPLAT_WITH_TORCH` get no alias — they are deleted in
Phase 1, and a deprecation warning is friendlier than silently ignoring them.

**Environment variables** — replace all 23 raw `std::getenv("SSPLAT_...")`
sites with one helper, so the deprecation lives in exactly one function:

```cpp
// src/core/Env.h
namespace spirula {
// Reads SS_<suffix>, falling back to the deprecated SSPLAT_<suffix> with a
// one-shot warning. Delete the fallback with the alias table in cmake/.
const char* env(const char* suffix);
}
```

Call sites become `spirula::env("VK_DEVICE")`. That is a strict improvement
over the status quo regardless of the rename: today the 23 names are scattered
and undocumented.

### 5.4 Execution

The identifier rename is mechanical (`sed` over `SSPLAT_` → `SS_`, `Ssplat` →
`Ss`, `ssplat` → `spirula`) but touches the committed generated trees
(`src/generated/`, `src/instantiations/`, `src/app/generated/`, 100k+ lines).
Rename the sources, re-run codegen, commit the regenerated output in the *same*
commit, and confirm the regenerated diff contains nothing but the rename.

Do this on a quiet tree, in one commit, with no other change riding along.

---

## 6. Phase 3 — the localization mechanism — **LANDED 2026-08-05**

Landed close to the design below. Read `src/i18n/README.md` for the mechanism
as built; this section keeps the reasoning and records the five places reality
differed.

**1. `Msg` carries its own name, and that name is the ImGui widget ID.**
Not in the plan, and it turned out to be load-bearing. ImGui derives a widget's
ID from its label, so a translated label is a *different widget*: switching to
Japanese would collapse every open header and reset every scroll position. The
`SS_MSG` macro stringifies `#name` into the object, and every `ui::` wrapper
renders `"text###<name>"`. Costs nothing and there is no other place the
information could have come from.

**2. The `EN`/`JA`/… tags cannot be generated, so they are guarded instead.**
A macro cannot define macros, so `BeginCatalog.h` lists the thirteen tags by
hand — a mirror of `Languages.h` that can drift. A canary message at the bottom
of that file, built from all thirteen tags and `static_assert`ed complete,
turns the drift into an error that names `BeginCatalog.h` rather than into a
baffling failure on whichever catalog entry compiled first.

**3. `SS_DEFAULT_LANG` is an identifier, not a string.** `Locale.h` writes
`Lang::SS_DEFAULT_LANG`, so `-DSS_DEFAULT_LANG=jp` (a typo for `ja`) is a
compile error naming the bad value instead of a binary that silently comes up
English. CMake reads the valid set out of `Languages.h` and checks it too, so
the failure usually arrives at configure time with the list printed.

**4. `C` and `POSIX` deliberately do not match.** They mean "the user expressed
no preference", which is *not* "the user wants English" — `parse_lang()`
returns false for them and the chain falls through to `SS_DEFAULT_LANG`. A
regional build in a bare container gets its own language, which is the case the
whole compile-time-default design exists for.

**5. The CJK faces are bundled, not embedded.** §8.2 costed
`SS_FONT_CJK=sc|tc|jp|kr|all` as an embed. It is not viable: `ss_embed_file()`
turns one byte into five characters of C source, so `all` would be a 130 MB
array literal. A regional build ships `fonts/` beside the executable instead —
the same offline behaviour, without putting a compiler through that. The
default build is still one self-contained file, because the *Latin* face is
small enough to embed and that is the half that matters: at 59 KB it is what
makes German, French, Turkish and Russian render at all.

Three further notes on the font work, which was indeed where the surprises
were (§11 predicted this correctly):

- **The per-language SubsetOTF builds, not the 16 MB regional OTFs.** Each
  carries its own language's coverage *and* its own regional glyph forms —
  which is the half that matters for Han unification — at a third of the
  download (4-8 MB).
- **Source Sans 3 reserves the font name "Source".** The 59 KB subset is a
  Modified Version under the OFL and every name-table record carrying it had to
  be rewritten, not just the filename. `tools/make_ui_font.py` does that, is
  byte-reproducible, and has a `--check` mode; the rename is a licence
  obligation, not a branding decision.
- **A face on disk is loaded whatever the UI language is.** An English UI still
  has to draw `C:\写真\` without turning it into boxes. Only the *fetch
  prompt* is tied to picking a CJK language.

Two pieces of housekeeping fell out of the work and were done rather than
worked around: `ModelDownload` became a generic `FileDownload` (the font fetch
is the same job with a different URL), and the SHA-256 that was private to
`src/aliked/model/Fetch.cpp` moved to `src/core/Sha256.h`, which removed a
duplicate rather than adding one.

### 6.0 The original design

Design goals, in priority order: **a missing translation must fail the build**;
no new dependency; catalogs modular, one per module.

The trick is to make a translation a *type* rather than a lookup key. Then
"missing translation" is a `static_assert`, not a runtime fallback.

### 6.1 The language list

One file declares the set; everything else derives from it.

```cpp
// src/i18n/Languages.h -- the ONE place the language set is written
#define SS_LANGUAGES(X)            \
    X(en,      "English")          \
    X(ja,      "日本語")            \
    X(zh_hans, "简体中文")          \
    X(zh_hant, "繁體中文")          \
    X(ko,      "한국어")            \
    X(de,      "Deutsch")          \
    X(fr,      "Français")         \
    X(es,      "Español")          \
    X(pt,      "Português")        \
    X(it,      "Italiano")         \
    X(nl,      "Nederlands")       \
    X(ru,      "Русский")          \
    X(tr,      "Türkçe")
```

Adding a locale here breaks the build on every incomplete message — which is
the point.

### 6.2 `Msg`

```cpp
// src/i18n/Message.h
namespace spirula::i18n {

enum class Lang : unsigned {
#define X(id, native) id,
    SS_LANGUAGES(X)
#undef X
};
inline constexpr unsigned kLangCount = 0
#define X(id, native) + 1
    SS_LANGUAGES(X)
#undef X
    ;

template <Lang L> struct Tr { const char* s; };

struct Msg {
    const char* v[kLangCount] = {};

    template <Lang... Ls>
    constexpr explicit Msg(Tr<Ls>... t) : v{} {
        static_assert(sizeof...(Ls) == kLangCount,
                      "i18n: wrong number of translations");
        ((v[unsigned(Ls)] = t.s), ...);            // C++17 fold
    }
    constexpr bool complete() const {
        for (unsigned i = 0; i < kLangCount; i++)
            if (!v[i] || !*v[i]) return false;     // also catches a duplicate tag
        return true;
    }
    const char* get() const { return v[unsigned(current())]; }
};

Lang current();
void set_current(Lang);

}  // namespace spirula::i18n

#define SS_MSG(name, ...)                                                     \
    inline constexpr ::spirula::i18n::Msg name{__VA_ARGS__};                  \
    static_assert(name.complete(),                                            \
                  "i18n: '" #name "' is missing a translation")
```

Properties worth noting: the tags carry their own slot, so **order does not
matter**; a duplicated tag necessarily leaves a hole and is caught by
`complete()`; a wrong count is caught by its own `static_assert`. C++17
throughout — no need to raise `CMAKE_CXX_STANDARD` from 17.

Cost: all 13 languages are always linked. At ~1,400 messages that is roughly
1 MB of `.rodata`. Acceptable, and far simpler than a resource-file scheme.

### 6.3 A catalog

One header per module, included only by that module's `.cpp` files. The short
tag macros are defined by `Message.h` and `#undef`'d by `EndCatalog.h`, so
`EN`/`JA`/… never leak into ordinary code:

```cpp
// src/i18n/catalog/Gui.h
#include "i18n/BeginCatalog.h"
namespace spirula::i18n::gui {

SS_MSG(open_dataset,
    EN("Open a Dataset..."),           JA("データセットを開く…"),
    ZH_HANS("打开数据集…"),             ZH_HANT("開啟資料集…"),
    KO("데이터셋 열기…"),                DE("Datensatz öffnen …"),
    FR("Ouvrir un jeu de données…"),   ES("Abrir un conjunto de datos…"),
    PT("Abrir um conjunto de dados…"), IT("Apri un set di dati…"),
    NL("Dataset openen…"),             RU("Открыть набор данных…"),
    TR("Veri kümesi aç…"));

}
#include "i18n/EndCatalog.h"
```

Planned catalogs: `Gui.h`, `Cli.h`, `Config.h` (the 192 field help strings),
`Errors.h`, `Brand.h` (§7).

### 6.4 The second half: unmarked strings

`Msg` cannot catch `ImGui::Button("Start")` — that compiles fine. Close it by
routing every text-rendering call through a thin wrapper whose parameters take
`const Msg&`:

```cpp
// src/app/gui/Ui.h -- the only ImGui text entry points the GUI may call
namespace ui {
inline bool Button(const i18n::Msg& m, ImVec2 sz = {}) {
    return ImGui::Button(m.get(), sz);
}
inline void Text(const i18n::Msg& m) { ImGui::TextUnformatted(m.get()); }
// Explicitly NOT translated: paths, numbers, engine log lines.
inline void TextRaw(const char* s)   { ImGui::TextUnformatted(s); }
}
```

A bare literal then does not compile. Back it with `tools/check_i18n.sh`,
which fails if `ImGui::{Text,TextWrapped,TextColored,TextUnformatted,Button,
SmallButton,Checkbox,Combo,MenuItem,BeginMenu,CollapsingHeader,SeparatorText,
RadioButton,SetTooltip,LabelText,SliderFloat,SliderInt,InputText,InputInt,
InputFloat}` appears anywhere in `src/app/gui/` outside `Ui.h`. Type system
covers "incomplete translation"; the lint covers "unmarked string".

### 6.5 Two rules to fix now

Retrofitting either of these is expensive.

- **Never concatenate sentences.** Use positional substitution —
  `format(msg, {a, b})` over `{0}` / `{1}` placeholders, ~30 lines, no
  dependency. Every one of these 13 languages reorders clauses relative to
  English; Japanese, Korean and Turkish are verb-final.
- **No plural-sensitive sentences in the catalog.** Write `Images: 5`, not
  `5 images`. Otherwise Russian needs a three-form CLDR plural rule
  (one/few/many) and every message that counts anything triples.

### 6.6 Staged rollout

1,400 messages × 12 locales will not land atomically, and an unenforced
fallback rots silently. One explicit escape hatch:

```cpp
#define SS_MSG_EN(name, s) \
    inline constexpr ::spirula::i18n::Msg name = ::spirula::i18n::en_only(s)
```

It is greppable and countable — `grep -rc SS_MSG_EN src/i18n/catalog/` **is**
the TODO list — and catalogs flip to full enforcement one at a time. Tiers:

| tier | scope | messages |
|---|---|---|
| 1 | GUI chrome, menus, errors, the ~40 basic options | ~350 |
| 2 | remaining GUI + CLI usage | ~850 |
| 3 | the 192 config help strings | ~190 |

Machine translation is a reasonable first pass for tiers 2-3. It is **not**
acceptable for the ~40 messages attached to irreversible actions (delete,
overwrite, "this will erase") — those get human review in every locale before
shipping. ja/zh/ko/de deserve review at tier 1 regardless; they are the four
where a bad string is most visible.

### 6.7 Locale resolution

In order, first hit wins:

1. `--lang <code>` on the command line
2. `SS_LANG` environment variable
3. the settings file (`%APPDATA%\Spirula Studio\settings.json`,
   `~/.config/spirula-studio/settings.json`)
4. the OS locale — `GetUserDefaultLocaleName` (Windows), `NSLocale` (macOS),
   `LC_ALL` / `LC_MESSAGES` / `LANG` (Linux)
5. **`SS_DEFAULT_LANG`, the compile-time default** (§8.3)

Chinese mapping needs care: `zh_CN`, `zh_SG`, `zh-Hans-*` → zh-Hans;
`zh_TW`, `zh_HK`, `zh_MO`, `zh-Hant-*` → zh-Hant. A bare `zh` follows
`SS_DEFAULT_LANG` if that is a Chinese locale, else zh-Hans. Unknown locales
fall to `SS_DEFAULT_LANG`, never to a hard-coded `en`.

---

---

## 6b. Phase 4 — the translations — **LANDED 2026-08-05**

**354 of 405 messages carry all thirteen languages.** The remaining 51 are the
external-COLMAP *Advanced* panel, left English on purpose: every entry names a
COLMAP parameter and is read next to COLMAP's own English-only documentation,
so a translated sentence wrapped around an untranslatable identifier helps less
there than anywhere else in the app.

The tiering in §6.6 held up, but the boundary moved: what shipped is tiers 1
and 2 complete (the whole GUI), and tier 3 — the 190 config field helps —
untouched, because those are still literals in `src/config/TrainConfig.h`
shared with `spirula train --help`. Turning them into messages is a separate
change, and the 33 `optimizer`-group fields should get their missing help text
written first (§3).

`SS_MSG_EN` earned its place: it let the whole GUI be converted and verified in
English before a single translation existed, and `tools/check_i18n.sh` counting
it down from 405 to 51 was the progress bar.

### 6b.1 Follow-up pass — **LANDED 2026-08-05**

Four things the first pass got wrong or left out, all found by looking at the
running program rather than at the code:

1. **The language menu was labelled with the word "Language", translated.** A
   user who cannot read the current interface language cannot read that menu
   either. It is now `SS_LANG_MENU_ICON` (文A, the widely used mark for this)
   followed by the current language's own name, so the menu is findable and
   also reports its state. The icon lives in `Languages.h` because
   `tools/make_ui_font.py` reads it from there — changing it regenerates the
   fonts instead of silently producing a box.

2. **The picker rendered `???` for the CJK names** with no font downloaded,
   which is the same failure one level down. Fixed by embedding four regional
   CJK faces subset to the ~600 characters per region the catalogs use: 449 KB
   for all four, against 23 MB for the full faces. See §8, rewritten.

3. **Mask prompts were being translated, and should not be.** SAM 3's text
   encoder reads English; the placeholder examples were translated, which was
   active advice to type something that works worse. The field is now
   `ui::InputTextEnglish()` (English content, translated label), a line under
   it says why in the interface language, and `src/app/gui/MaskPrompt.h` adds a
   palette of 17 common subjects — labelled in the interface language,
   inserting English — so a user who writes no English still builds a good
   prompt.

4. **Tier 2 was not as complete as it looked.** Still English were: the seven
   training presets (moved out of `TrainConfig.h` into
   `i18n/catalog/Train.h`, so the GUI and `spirula train --help` share one
   copy and `--help` is localized too), the four navigation modes, the four
   projection models, the seven lens models, and every stage name and note
   this program writes to the log (`i18n/catalog/Log.h`). Child-process output
   stays English deliberately — it is not ours to rewrite, and it is what a
   bug report is pasted from.

**Review status is recorded in `docs/i18n.md` and it matters.** These are one
author's translations, not native speakers': a solid first draft. §6.6's rule
that ja/zh/ko/de and every irreversible-action message need human review before
shipping stands, and the catalog marks the legal block with a comment saying
so. A licence summary that softens somebody else's terms is worse than no
translation at all.

---

## 7. Product names per locale

`Brand.h` holds the product name as an ordinary `SS_MSG`, so the policy is data
rather than code.

| locale | name | note |
|---|---|---|
| en, de, fr, es, pt, it, nl, tr | Spirula Studio | Latin-script markets keep the wordmark |
| zh-Hans / zh-Hant | 旋影工坊 | **all four characters are identical in Simplified and Traditional**, so one name serves both scripts — and 旋 preserves the spiral sense of *Spirula* |
| ja | スピルラ・スタジオ | pin the katakana, or users independently invent スピルーラ / スパイルラ |
| ko | 스피룰라 스튜디오 | same reasoning |
| ru | Spirula Studio (Спирула Студио) | Russian technical users keep Latin brand names; Cyrillic as a first-mention gloss only |

Recommended policy: the **Latin wordmark stays the logo in every locale**, and
the localized name is in-text copy — window titles, About, prose. This is what
Blender, Krita and Godot do, and it avoids maintaining five logo lockups. The
exception is zh, where 旋影工坊 can stand alone; CJK markets genuinely adopt
local names, which is the failure mode this whole section exists to prevent.

---

## 8. Fonts — **SUPERSEDED 2026-08-05, see below**

The design in §8.1–8.3 was written before anything was measured, and its
central assumption turned out to be wrong. It assumed the choice was between
embedding a 4–8 MB regional CJK face and fetching one at runtime. It is not:
this program writes about 600 distinct characters per region, all of them in
its own catalogs, and a face subset to exactly those is ~110 KB. All four fit.

**What shipped:** five faces embedded in every build, 507 KB together — the
58 KB Latin/Cyrillic subset of Source Sans 3 as planned, plus
`SpirulaCJK-{JP,SC,TC,KR}.otf` cut from Noto Sans CJK. Every language renders
in the right regional glyph forms with nothing to download. The full faces are
still fetched or bundled by `SS_FONT_CJK`, but for a different and much smaller
job: CJK in *user data* — file names, paths, typed prompts — which no subset of
our own strings can anticipate.

Two consequences worth recording:

- **The configure-time `SS_DEFAULT_LANG=ja` + `SS_FONT_CJK=none` check is
  gone.** It existed to stop a build shipping tofu; with the subsets embedded
  that combination renders Japanese perfectly, so the check was now asserting
  something false. `SS_FONT_CJK=none` means "no download offered", not "no
  CJK".
- **The subsets are derived from the catalogs**, so a translation edit can
  outgrow them and the symptom is one hollow box mid-sentence.
  `tools/check_font_coverage.py` guards it on every build — no network, no
  fontTools, it parses `cmap` by hand.

Alternatives measured before settling on four faces: one pan-CJK subset is
287 KB but renders 262 shared characters in Simplified Chinese forms for three
of the four languages; a shared base plus three deltas is 358 KB, because of
the characters used by more than one region only 267 have identical outlines
while 262 genuinely differ. 135 KB on a 61 MB binary is not worth either.

The sections below are kept as written, for the reasoning about Han
unification (which held up completely) and about `SS_DEFAULT_LANG` (unchanged).



**This is the unbudgeted part.** `GuiMain.cpp` contains no font code at all, so
the GUI runs on ImGui's built-in ProggyClean — a 13px ASCII-only bitmap font.
German `ö`, French `é` and Turkish `ğ` are already broken today, before any CJK
is involved.

ImGui is pinned at **v1.92.8**, which has the dynamic font atlas: glyphs
rasterize on demand and ranges need not be enumerated up front. `GuiMain.cpp`
already uses `style.FontScaleMain`, so the codebase is on the new API. What the
new system does *not* do is find a font for you — the glyphs still have to ship.

### 8.1 Font choice

Nothing with CJK coverage looks like ProggyClean; it is a pixel font and they
are outline fonts. The UI **will** change appearance, and pretending otherwise
sets up a bad surprise. The least jarring choice:

**Source Sans 3** (Latin / Greek / Cyrillic, OFL-1.1) + **Source Han Sans**
(CJK, OFL-1.1).

The reason for that specific pair over Noto Sans: Source Han Sans embeds Source
Sans as its own Latin, so the two are designed together — matching x-height,
weights and vertical metrics, which is exactly what keeps a mixed
Latin/CJK line from visibly stepping. (Noto Sans CJK and Source Han Sans are
the same typeface under two names; Noto Sans, the Latin family, is a different
design.) Turkish `ı ğ ş İ` and full Cyrillic are covered.

Mitigations for the ProggyClean → outline transition: raise the base size from
13 to 15px, and consider adding `imgui_freetype` with light hinting — the
appeal of ProggyClean is pixel-crispness, and stb_truetype at 13px is
noticeably softer. FreeType is the one dependency worth the argument here; it
is optional and gated.

An alternative was considered and rejected: keep ProggyClean for English and
switch fonts only for other locales. It preserves the current look exactly for
English users, but it means two UI appearances to maintain and screenshot, and
mixed pixel/outline glyphs on the same line look worse in French and German
than a clean switch does.

### 8.2 `SS_FONT_CJK` — embed or fetch

```cmake
set(SS_FONT_CJK "fetch" CACHE STRING
    "CJK font: fetch | none | sc | tc | jp | kr | all")
```

| value | behaviour | binary cost |
|---|---|---|
| `fetch` *(default)* | Latin/Cyrillic embedded; the matching Source Han Sans regional face is downloaded on first use of a CJK locale, through the consent-gated path already built for SAM weights in `src/app/gui/ModelCache.cpp` | ~0.4 MB |
| `none` | Latin/Cyrillic only; CJK locales render tofu | ~0.4 MB |
| `sc` / `tc` / `jp` / `kr` | embed exactly that regional face — **this is the per-region binary path** | ~+16 MB |
| `all` | embed all four | ~+65 MB |

Sizes are for the language-specific OTFs and are approximate; check against the
release you vendor, and note that the *Subset* OTFs are smaller but drop
coverage.

**Han unification matters here.** The regional faces of Source Han Sans differ
in default glyph forms for shared codepoints (直, 骨, 雪, 戸 and many more). A
`sc` binary shown to a Japanese reader renders kanji in Chinese forms — legible,
and visibly wrong. So a per-region build should pair `SS_FONT_CJK` with a
matching `SS_DEFAULT_LANG`, and the runtime should still offer to fetch the
correct regional face if the user switches away from the embedded region. Keep
the fetch path compiled in for every value except `none`.

Embedding reuses `ssplat_embed_file()` from `cmake/SsplatEmbed.cmake` — the
same mechanism as `viewer.html`. Fonts are OFL-1.1, which is
GPLv3-compatible for bundling; ship the licence text alongside, and do not
rename the font files (OFL reserved font name clause).

### 8.3 `SS_DEFAULT_LANG`

```cmake
set(SS_DEFAULT_LANG "en" CACHE STRING
    "Locale used when none is detected: en|ja|zh_hans|zh_hant|ko|de|fr|es|pt|it|nl|ru|tr")
```

The last resort in the §6.7 chain — headless runs, containers with no `LANG`,
and stripped Windows environments all land here. A regional build sets it:

```bash
# Simplified-Chinese build
cmake -DSS_DEFAULT_LANG=zh_hans -DSS_FONT_CJK=sc ...
# Japanese build
cmake -DSS_DEFAULT_LANG=ja     -DSS_FONT_CJK=jp ...
```

Add a configure-time consistency check, in the spirit of the rest of the
localization design — an inconsistent combination should fail early rather than
ship tofu:

```cmake
if(SS_DEFAULT_LANG MATCHES "^(ja|ko|zh_hans|zh_hant)$" AND SS_FONT_CJK STREQUAL "none")
    message(FATAL_ERROR
        "SS_DEFAULT_LANG=${SS_DEFAULT_LANG} needs a CJK font; "
        "set SS_FONT_CJK to fetch, sc, tc, jp, kr or all.")
endif()
```

---

## 9. Comments

Trim comments **in files you are already editing**, never as a standalone pass —
a repo-wide comment diff has no test coverage and will conflict with all three
phases above.

The rule: **keep a comment that records a non-obvious invariant or a
why-not; delete one that restates the code.** AGENTS.md's "Gotchas" section is
the right register — every entry there is something that cost someone a day.
The header comment on `ConfigUI.cpp` ("keep this file free of per-field special
cases") is a keeper. Most of the inline narration in `GuiMain.cpp` is not.

Internal comments stay in English in every file, permanently. Only catalog
strings are localized.

---

## 10. Phase checklist

- [x] **0.** `TrainConfig.h` hand-written; `generate_cli_config.py` deleted;
      `--help` diffed byte-for-byte across all 7 presets. *(2026-08-04)*
- [x] **1.** Resume + layout adaptation → `src/checkpoint/`; eval →
      `src/app/EvalMetrics.{h,cpp}`; LPIPS + the benchmark driver →
      `reference/python/` (hand-run, on no code path). `spirulae_splat/`,
      `setup.py`, `src/bindings/`, `tests/python/` and the whole Torch half of
      the build deleted — 40.8k lines out, 2.3k in. The binary links neither
      libtorch nor libpython. AGENTS.md rewritten. *(2026-08-04)*
- [x] **2.** `SSPLAT_` → `SS_` (114 names), `ssplat` → `spirula` (executable,
      namespace, entry points), `Ssplat*.cmake` → `Ss*.cmake`, CMake helpers
      and targets to `ss_*`, `SsplatConfig` → `TrainConfig` with the
      `TrainVec3*` / `train_v3*` / `train_json_key` / `kTrainPresets` helpers,
      `SPIRULAE_` guards → `SPIRULA_`. Three shims: the CMake alias loop,
      `spirula::env()`, and the `ssplat-` argv[0] prefix.
      `tools/{ss_reserved_names.txt,check_ss_prefix.sh}` wired into
      `build_develop.bash`. Config and model-cache directories move to
      `spirula-studio/`, adopting an existing `spirulae-splat/` in place.
      **The repository was deliberately not renamed** — the Pages URL under
      `viewer/` depends on the directory name, and the README's "Formerly
      Spirulae-Splat" line carries the transition instead. *(2026-08-04)*
- [x] **3.** `src/i18n/` (`Languages.h`, `Message.h`, `Begin/EndCatalog.h`,
      `Locale.{h,cpp}`), `gui/Ui.h` (55 wrappers), `tools/check_i18n.sh` wired
      into `build_develop.bash`, `SS_FONT_CJK` + `SS_DEFAULT_LANG` + the
      configure-time consistency check, the 59 KB embedded Latin/Cyrillic face
      and `tools/make_ui_font.py` that reproduces it, hash-verified CJK fetch,
      the language picker, and locale detection on all three platforms. Every
      text-bearing ImGui call in `src/app/gui/` (253 of them) now goes through
      `ui::`. *(2026-08-05)*
- [x] **4.** 458 of 509 messages translated into all thirteen languages; the 51
      left are the external-COLMAP advanced panel, deliberately. Tier 3 (the
      190 config field helps) is not started — those are still literals shared
      with `--help`, and the group structure is due a rethink first. Human
      review for ja/zh/ko/de and the legal block is **outstanding** and
      recorded in `docs/i18n.md`. *(2026-08-05)*
- [x] **4b.** Follow-up pass (§6b.1): the 文A language-menu icon, four embedded
      CJK subsets so no language renders as boxes without a download
      (507 KB total, `tools/check_font_coverage.py` keeps them in sync), the
      English-only mask prompt plus its translated common-subject palette, and
      the presets / navigation modes / projections / lens models / log lines
      that tier 2 had missed. *(2026-08-05)*
- [ ] **5.** Drop the `SSPLAT_` aliases one release later: the CMake alias loop
      in `cmake/SsOptions.cmake`, the fallback in `src/core/Env.h`, the legacy
      directory adoption in `AppPaths.cpp` and `aliked/model/Fetch.cpp`, and
      the `ssplat-` argv[0] prefix in `src/app/Main.cpp`.

## 11. Recommendation

Phase 0 first and alone — it is a few days, it unblocks everything, and it is
the one piece that is pure gain even if the rest slips.

Then Phase 1, which is gated almost entirely on the resume port. Do not start
the rename before it: renaming code you are about to delete is the largest
avoidable cost in this plan.

Phase 2 is a day of mechanical work plus a careful pass over the public option
and environment-variable names. Do it on a quiet tree, in one commit.

Phase 3's plumbing is small — `Msg` is 60 lines. Budget the time for fonts
instead; §8 is where the surprises are, and `SS_FONT_CJK=fetch` is what keeps
the default binary from growing 16 MB for a feature most users of a given build
will not use.

Phase 4 is the long tail and is the only part that can ship incrementally,
which is exactly why `SS_MSG_EN` exists.
