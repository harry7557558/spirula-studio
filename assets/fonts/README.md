# Fonts

Dear ImGui's built-in font is ASCII-only. Before this directory existed the
GUI could not draw a German umlaut, a French accent, a Turkish dotless i or a
single Cyrillic letter — which was already wrong, independently of
localization, for anyone whose dataset path was not pure ASCII.

Five faces are committed here and **all five are embedded** in the executable,
3.3 MB together. Nothing has to be downloaded for the interface to render in
any of the thirteen languages in `src/i18n/Languages.h`, and nothing has to be
downloaded for an ordinary file name to render either.

| | file | size | from |
|---|---|---|---|
| Latin / Greek / Cyrillic | `SpirulaUI-Regular.ttf` | 159 KB | Source Sans 3 |
| CJK, per region | `SpirulaCJK-{JP,SC,TC,KR}.otf` | 3.2 MB | Noto Sans CJK |

The full CJK faces — 4–8 MB each — are still **fetched at runtime** or bundled
by a regional build, but for a much smaller job than they used to do: see
*The full faces* below.

## Two jobs, two budgets

The fonts serve two kinds of text and it is worth keeping them apart.

**The interface** is text this program wrote. It is a closed set — the
catalogs under `src/i18n/catalog/` — so it can be covered exactly, and it
must be, in the right regional glyph forms, offline. That is the ~600
characters per region the subsets started out as.

**File names are not.** A dataset path, a folder name or a typed mask prompt
is user data: it can hold any character at all, and one uncovered character is
a `?` in the middle of a path. No subset of *our* strings anticipates
`D:\写真\第2回—テスト①\`. Covering that is what the rest of the 3.3 MB buys,
and it is why the character sets below are declared as blocks and national
standards rather than scraped from the catalogs.

## `SpirulaUI-Regular.ttf`

A subset of **Source Sans 3** (Adobe, SIL OFL 1.1 — `OFL-SourceSans.txt`),
431 KB upstream and 159 KB after subsetting. The ranges are listed in
`tools/make_ui_font.py`; the ones that are there for file names rather than
for the UI are worth naming:

| range | why |
|---|---|
| Latin Extended-B, Extended Additional | pinyin tone marks, Vietnamese |
| combining marks (U+0300–036F) | macOS hands out file names in NFD |
| Greek, whole Cyrillic block | not just the twelve UI locales' letters |
| General Punctuation, U+2100–214F | em dash, curly quotes, ellipsis, the numero sign |
| arrows, math, geometric, symbols, dingbats | stars and check marks name a lot of files |

Source Sans 3 declares **"Source" as a Reserved Font Name**, so a Modified
Version may not carry it. Every name-table record was rewritten accordingly —
the copyright and licence records are quoted verbatim, as the OFL requires,
and everything else says *Spirula UI*. This is a licence obligation, not a
branding decision.

## `SpirulaCJK-{JP,SC,TC,KR}.otf`

Each is **Noto Sans CJK** (Google, SIL OFL 1.1 — `OFL-NotoSansCJK.txt`) cut
down to three things:

1. that region's **national common-use standard** — GB 2312 level 1 (3755
   characters) for SC, Big5 level 1 (5401) for TC, JIS X 0208 level 1 (2965)
   for JP, the 2350 KS X 1001 hangul syllables for KR. These are the
   percentile answer to "which characters appear in a file name", chosen by
   the frequency studies behind each standard rather than by us, and they are
   decoded out of Python's own codecs, so no character list is committed here
   and none can go stale;
2. CJK punctuation, the halfwidth/fullwidth forms and the enclosed
   alphanumerics (①, Ａ, ｶ) in **every** face, because their forms are
   regional too — and kana, bopomofo, the enclosed CJK letters (㈱, ㍻) and the
   symbols that name files (★, ♪, ✓) in **one**, since all four subsets are
   merged into the same atlas and a second copy would only cost bytes. The
   symbols are named one by one rather than taken as a block: of U+2600–27BF
   Noto Sans CJK has under a fifth;
3. whatever that region's own translations use, scanned from the catalogs.

Noto Sans CJK is Source Han Sans under its other name, so it is the same
design as the Latin face and a mixed line does not visibly step.

**There are four because of Han unification**: the shared codepoints have
different default glyph forms per region, so a Japanese reader shown the
Simplified Chinese face gets kanji in Chinese forms — legible, and visibly
wrong. Two alternatives were measured and rejected:

| approach | size | cost |
|---|---|---|
| four regional cuts *(what ships)* | 3.2 MB | — |
| one shared face, single glyph forms | ~1.9 MB | every language but one reads file names in another region's forms |
| a shared base + four deltas | 2.5 MB | 18% saved for a fifth file and a merge-order hazard; of the 10 178 codepoints only 5459 have identical outlines everywhere |

Noto reserves no font name, so a subset could legally keep it; these are
renamed anyway, because a file this small is not the font it was cut from.

## What is still not covered

Hebrew, Arabic, Thai, the Indic scripts and the rest have no glyphs in any of
the five faces, and adding them would not be enough on its own: ImGui applies
no bidi and no shaping, so an Arabic path would come out unjoined and an
Indic one unreordered. A path in one of those scripts is still a row of `?`.
Combining marks are covered but not *positioned*, for the same reason — an
NFD `é` draws as an `e` with the accent alongside rather than over it, which
is worse-looking than the composed form and much better than a `?`.

`?` rather than a hollow box because ImGui picks its fallback glyph from
U+FFFD, `?`, space in that order, and neither Source Sans 3 nor Noto Sans
CJK has a U+FFFD.

## Rebuilding

```bash
pip install fonttools
python3 tools/make_ui_font.py           # all five: download, subset, rename
python3 tools/make_ui_font.py --check   # verify the committed files
python3 tools/check_font_coverage.py    # cheap: is anything missing?
```

The output is byte-reproducible, so `--check` is meaningful. The committed
files are build artifacts kept in the tree so that the build needs no network
— the same reasoning as the committed files under `src/generated/`.

**The subsets are partly derived from the catalogs.** Edit a translation and
they can go stale, and the symptom is one `?` in the middle of an
otherwise fine sentence. `tools/check_font_coverage.py` is the guard and runs
on every `build_develop.bash`; it needs neither the network nor fontTools,
because it parses `cmap` by hand.

## The full faces

Declared in `cjk_faces.txt`, which is the single source of truth: CMake
generates `app_generated/cjk_faces.h` from it and `src/app/gui/Fonts.cpp` reads
that. Nothing of them is committed here — they are downloaded, verified against
the SHA-256 in that file, and cached.

They are what covers the tail: a rare surname, a classical character, anything
outside a national common-use list. `SS_FONT_CJK=sc|tc|jp|kr|all` bundles one
or all of them into `<exe dir>/fonts/` instead, which is the first place
`Fonts.cpp` looks after `$SS_FONT_DIR`. They are bundled rather than embedded
because `ss_embed_file()` costs four characters of C source per byte, so `all`
would be a 92 MB literal.

See `docs/i18n.md` for `SS_FONT_CJK`, the download offer, and what a build with
`SS_FONT_CJK=none` does instead.
