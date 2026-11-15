# `src/i18n/` — how the interface gets translated

The whole design turns on one idea: **make a translation a type, not a lookup
key.** Then "somebody added a language and forgot a string" is a compile error
instead of a runtime fallback that nobody notices for a year.

```
Languages.h        the language set. ONE file. Everything derives from it.
Message.h          Lang, Msg, SS_MSG / SS_MSG_EN, format()
BeginCatalog.h     the short tag macros: EN(...) JA(...) ZH_HANS(...) ...
EndCatalog.h       #undefs them again, so EN/TR never leak into ordinary code
Locale.cpp/.h      which language the UI is in, and how that is decided
catalog/           the messages themselves, one header per screen -- plus
                   the ones that are not screens: Cli.h (what the command-line
                   tools print around their own work), Sfm.h and Mesh.h (what
                   those two subcommands say while they run, in the GUI's
                   terminal panel as well as in yours), Data.h (what reading a
                   dataset says about it), and SfmFields.h / SfmHelp.h /
                   SamHelp.h (the two big `--help` pages)
```

## Writing a message

```cpp
#include "i18n/BeginCatalog.h"
namespace spirula { namespace i18n { namespace msg { namespace gui {

SS_MSG(start_training,
    EN("Start Training"), JA("学習を開始"), ZH_HANS("开始训练"), ...);

}}}}
#include "i18n/EndCatalog.h"
```

Each tag carries its own slot, so **the order does not matter**, a duplicated
tag necessarily leaves a hole and is caught, and a wrong count has its own
assert. Three failure modes, three compile errors:

| mistake | what you get |
|---|---|
| a language missing | `static_assert`: `'start_training' is missing a translation` |
| a language twice | the same assert (the duplicate leaves a hole) |
| too many tags | `wrong number of translations` |
| a language added to `Languages.h` but not `BeginCatalog.h` | the canary in `BeginCatalog.h` fails first, naming that file |

`SS_MSG_EN(name, "…")` is the escape hatch for a message that has not been
translated yet: every slot gets the English text, so it works, and
`bash tools/check_i18n.sh` counts what is left. There are none at the moment,
so a new one is a visible, temporary decision rather than the normal way to
add a message.

## Two rules that are expensive to retrofit

**Never build a sentence from fragments.** Every language here reorders clauses
relative to English, and Japanese, Korean and Turkish are verb-final, so
`"Stop training and " + what + "?"` cannot be translated at all. Write the
whole sentence, with `{0}` / `{1}` placeholders:

```cpp
ui::Text(msg::dataset_summary, {cameras, views, points});
```

The trainer's stop-confirmation is the worked example: it is three complete
questions, not one with a swappable tail.

**No plural-sensitive sentences.** Write `Objects: 3`, not `3 objects`.
Otherwise Russian needs a three-form CLDR plural rule and every message that
counts anything triples. Every counting message in the catalogs is labelled
rather than inflected, on purpose.

## The other half: strings that never became messages

`Msg` cannot see `ImGui::Button("Start")` — that compiles perfectly well and is
simply never translated. `src/app/gui/Ui.h` closes that from the other side:
every wrapper takes a `const Msg&`, so a bare literal does not compile, and
`tools/check_i18n.sh` fails the build if any text-bearing ImGui function is
called in `src/app/gui/` outside `Ui.h`.

The `ui::*Raw` overloads are the deliberate exception, named so that using one
is a visible decision in the diff. They are for text that must **not** be
translated: paths, filenames, device names, engine log lines, backend error
strings, and text `i18n::format()` already produced.

Between the two, "forgot to translate" has no way in:

    the type system  ->  a message with a missing language
    the lint         ->  a string that never became a message

## Widget identity

Every `ui::` wrapper with an ID renders `"text###<message name>"`. ImGui derives
a widget's ID from its label, so without this, switching to Japanese would
collapse every open header and reset every scroll position. Two call sites
sharing one message therefore share an ID too — wrap one in `ImGui::PushID()`
exactly as you would for two identical literals.

## Which language

First hit wins:

1. `--lang <code>` (consumed in `src/app/Main.cpp`, removed from `argv`, so no
   tool's own parser has to know about it)
2. the `SS_LANG` environment variable
3. the settings file — the GUI's language picker writes it there
4. the OS locale (`GetUserDefaultLocaleName`, `CFLocale`, `LC_ALL`/`LANG`)
5. `SS_DEFAULT_LANG`, the compile-time default

Step 5 is a *language*, not "English": a build shipped as
`-DSS_DEFAULT_LANG=ja` has to come up Japanese in a container with no `LANG`
set, which is exactly what a hard-coded English fallback gets wrong. It is a
CMake option that becomes an identifier, so a typo fails to compile.

`C` and `POSIX` deliberately do **not** match: they mean "no preference", not
"English", and must fall through to the next step.

Chinese needs care and gets it in `resolve_chinese()`: `zh_CN`/`zh_SG`/`zh-Hans-*`
→ Simplified; `zh_TW`/`zh_HK`/`zh_MO`/`zh-Hant-*`/`zh-CHT` → Traditional; a bare
`zh` follows `SS_DEFAULT_LANG` if that is Chinese, else Simplified.

## Adding a language

1. Add the row to `SS_LANGUAGES` in `Languages.h`.
2. Add the tag macro to `BeginCatalog.h` and the `#undef` to `EndCatalog.h`.
   (Forget this and the canary tells you, by name.)
3. Build. Every message that now has a hole is a compile error with the
   message's own name in it. That list is your work.
4. If the language needs a script the embedded fonts do not cover, add it to
   `SS_LANGUAGES_CJK`, give it a face in `assets/fonts/cjk_faces.txt`, and
   regenerate the subsets (below).
5. Rebuild the fonts: `python3 tools/make_ui_font.py`. Even for a
   Latin-script language — the subsets carry every language's native name, so
   the picker can draw the new row.

## Fonts

See `assets/fonts/README.md` and `docs/i18n.md`. Short version: five subset
faces are embedded and always available (58 KB Latin/Cyrillic + four ~110 KB
regional CJK cuts), so every language renders with nothing to download. The
full 4–8 MB CJK faces are still fetched on demand, verified against a pinned
SHA-256, for CJK in *user data* — file names, paths, typed prompts.

**The CJK subsets are generated from the catalogs.** Add a character no
translation used before and they are stale;
`python3 tools/check_font_coverage.py` runs on every build and says so.
Regenerate with `python3 tools/make_ui_font.py`.

## Reading a translated child process back

`format()` has an inverse. The GUI runs `spirula mesh` as a child process with
`--lang`, so the child prints Japanese and the GUI's progress bar has to
understand it. Matching on English fragments is the thing that breaks the
moment a tool is translated, so `scan()` matches on the MESSAGE:

```cpp
std::vector<std::string> got;
if (i18n::scan(msg::mesh::cameras_rendered, tail, got))   // "cameras rendered: 12/120"
    frac = atof(got[0].c_str()) / atof(got[1].c_str());
```

It takes the message's literal parts apart and matches them in order, handing
back what stood between them. Two rules follow for anything a parent reads
back: keep a separator between adjacent placeholders (there is nothing to split
on otherwise, and `scan()` refuses rather than guessing), and keep the numbers
in an order every translation can live with.

`src/app/gui/MeshRunner.cpp` is the worked example: it compares the stage
against the same `msg::mesh::stage_*` entries `mesh/MeshLog.h` printed, and
pulls the two counts it needs out of the bodies with `scan()`. There is no
English left anywhere in that protocol.

## Wrapping

Help text is stored one string per paragraph and wrapped by `i18n::wrap()` at
print time, never by hand. It measures in terminal columns rather than bytes,
and -- the part that matters -- it breaks BETWEEN CHARACTERS in the languages
that have no spaces to break at. Chinese and Japanese are written without them;
a `find(' ')` wrapper leaves a paragraph as one line four hundred columns wide.
`i18n::pad_to()` lays out the flag column the same way, and a flag whose
translated sentence no longer fits beside it takes its own line rather than
widening the column for everybody.

## Text that stays English on purpose

Not everything a user sees is addressed to them:

- **Mask prompts** go to SAM 3, whose text encoder reads English. The field,
  its placeholder example and the words the palette inserts are all English
  whatever the interface language is — see `src/app/gui/MaskPrompt.h`, which
  is also how a user who does not write English still builds a good prompt.
  `ui::InputTextEnglish()` is the wrapper that says so at the call site.
- **Third-party child-process output** (COLMAP, ffmpeg) is passed through
  verbatim. It is English, it is what a bug report is pasted from, and it is
  not ours to rewrite. Our own stage names and notes around it *are*
  translated — see `i18n/catalog/Log.h`.

  `spirula sfm` is **not** in that category, and this is the distinction worth
  keeping straight: it is a subcommand of this program, re-run as a child
  process, so its output is ours. Every line a default run prints comes out as
  a localized, equal-width `[tag]` followed by a localized message
  (`src/sfm/core/Log.h`, `i18n/catalog/Sfm.h`). What stays English there is
  what is addressed to whoever is debugging the pipeline rather than to the
  person waiting on it: `--help`, `SS_SFM_MAP_PROF`, the seam-test and
  focal-curve diagnostics, and the self-test binaries.
  `spirula mesh` is the same: it is ours, it runs as a child of the GUI with
  `--lang`, and every line it prints comes out as a localized `[meshing]`, a
  localized stage and a localized message (`src/mesh/MeshLog.h`,
  `i18n/catalog/Mesh.h`). Its `--help` is translated too. What stays English
  there is the moment dumps behind `SS_MESH_DEBUG_RENDER`.
- **Machine-readable output**: `spirula sam`'s detection table is a TSV a script
  reads, and the codec table beside it is the same. A script has no language.
- **`src/core/Env.h`'s deprecation notice.** That header is deliberately
  dependency-free: the standalone tool binaries include it and link neither the
  engine nor `ss_i18n`. The sentence is also about the spelling of an
  environment variable.
- **Identifiers**: preset names, camera and lens model names, config field
  names, COLMAP parameter names inside a translated sentence. The picker shows
  a translated label next to the name rather than instead of it, so a user who
  read the README still recognises the row; and `SiftMatching.max_ratio` stays
  itself in every language, because it is what the reader types into COLMAP.

The command line is **not** in this list at all -- `--help` included.
`spirula --help`, `spirula sfm --help` (its 120 flag descriptions with them),
`spirula sam --help`, `spirula train --help`, everything a training
run prints, everything a reconstruction prints, everything a meshing run
prints, and everything reading a dataset says about it are all translated --
because a terminal has no language picker to fix it with afterwards, and
because the GUI shows that terminal.

## Copy length review

See AGENTS.md's “UI text — keep it short” section for the writing policy.
`python3 tools/check_ui_text.py` reports changed logical messages, including
staged and unstaged text, against HEAD. `--all` includes existing debt;
`--base REV` compares current text with that revision (use a merge base for
branch review). `--json` includes the full text and every finding; `--limit N`
limits only the human-readable listing. Unexcluded length findings and ASCII
violations fail the check. Invalid inputs or unsupported catalog syntax also
fail. CMake runs the changed-copy check before building targets, and CI checks
the full catalog. Clean catalogs are skipped in the changed-copy check.

The initial 60-column line estimate counts Unicode wide/fullwidth characters
as two columns, ordinary letters as one, and combining/format characters as
zero. Explicit newlines count. `--columns N` allows calibration at other widths.
This is not pixel measurement: proportional glyph advances, shaping, expanded
placeholders and panel size still need visual review. The 420-scaled-pixel
hover wrapper is the first calibration target; 60 columns is provisional.

`tools/ui_text_policy.json` assigns budgets to roles. Specific message entries
override catalog roles; remaining help/hint names are inferred as tooltips and
other messages as ordinary interface text. Usage names are inferred as CLI
copy. Inference is a review aid, not proof of how a message is displayed.
The policy’s `exclusions` map records reviewed exceptions with a reason
and a usage location. Text and JSON reports show exclusions separately from
over-budget copy.

GUI and CLI prose has a three-line budget. A list may open with one prose
paragraph of up to three wrapped lines. Use up to four explicit `- ` or `• `
items, two wrapped lines per item, and eight wrapped lines total including
the overview. Write the overview on one logical line, followed immediately
by the bullets. For one follow-up, a second plain line can replace the
bullet. That line can wrap to two display lines. The checker reserves two
display columns for each bullet marker.
Verbatim legal text remains exempt.

English terminal message templates pass through the replacements in
`src/i18n/AsciiPunctuation.inc` before formatting. GUI messages and other
languages use their original strings. Formatted arguments retain their text,
including Unicode paths. Both English spellings must fit the length budget.
The checker rejects unsupported English catalog characters even in
length-excluded messages. Raw string literals in
`src/app/cli/` and `src/app/Main.cpp` must already be ASCII.
Embedded English license text uses the same conversion for terminal display,
and its characters are checked against the replacement table.
`Main.cpp` enables terminal rendering, and the GUI entry point disables it.
`i18n::scan` accepts the terminal spelling when reading child-process output.

Shared GUI/CLI messages need the GUI budget. Every explicit assignment carries
a reason; check mixed-use catalogs at call sites before adding exceptions.

Mask shrink/expansion hover help uses the same compact policy in all locales.
The percentage is relative to each detected object's size: at 5%, an object
400 pixels across gets roughly a 20-pixel margin. Smaller distant objects get
smaller margins. Shrinking removes background rims from kept subjects and does
not trim the frame boundary; expansion removes color fringes around excluded
objects. Zero preserves the detected outline.
