#!/usr/bin/env python3
"""Rebuild the five font files the GUI embeds.

    assets/fonts/SpirulaUI-Regular.ttf    Latin + Cyrillic, from Source Sans 3
    assets/fonts/SpirulaCJK-JP.otf   \
    assets/fonts/SpirulaCJK-SC.otf    |   from Noto Sans CJK, subset to the
    assets/fonts/SpirulaCJK-TC.otf    |   characters this program's own
    assets/fonts/SpirulaCJK-KR.otf   /    translations actually use

Together they are 3.3 MB and they cover two things with nothing to download:
this program's own text in all thirteen languages of SS_LANGUAGES, and an
ordinary FILE NAME in any of them. The second is most of the weight and the
reason the character sets below are Unicode blocks and national common-use
standards rather than a scrape of the catalogs -- a path is user data, and one
uncovered character in it is a `?` in the middle of the path.

Embedding all four full faces instead would be 23 MB, and fetching one on
demand -- what this GUI did before -- puts a wall of `?` and a download
button in front of a user at the language picker, which is the one screen
someone who cannot read the current UI language has arrived at. The full
faces are still fetched (src/app/gui/Fonts.h), now only for the tail a
common-use standard leaves out.

THE SUBSETS ARE PARTLY DERIVED FROM THE CATALOGS. Edit a translation and they
can go stale, which shows up as one `?` in the middle of a sentence.
tools/check_font_coverage.py is the guard: it runs on every build, needs no
network and no fontTools, and fails when a catalog uses a character no
committed font has.

Two more things this script exists to keep honest:

  * A subset is a "Modified Version" under the SIL Open Font License. Source
    Sans 3 reserves the name "Source", so every name-table record carrying it
    has to be rewritten -- not just the filename. Noto reserves nothing, but
    a 600-glyph file called "Noto Sans JP" would still be a lie, so it is
    renamed too. Copyright (nameID 0) and licence (13) are quoted verbatim,
    as the licence requires.
  * The committed files are build artifacts. This is their generator, in the
    same spirit as tools/codegen/ -- the output is committed so the build
    needs no network, and this script is how you check it is what it claims.

Usage:
    pip install fonttools
    python3 tools/make_ui_font.py            # rebuild all five
    python3 tools/make_ui_font.py --check    # verify the committed files
    python3 tools/make_ui_font.py --latin    # just the Latin face
"""

import argparse
import hashlib
import io
import pathlib
import re
import subprocess
import sys
import tempfile
import unicodedata
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
FONT_DIR = ROOT / "assets" / "fonts"
CATALOG_DIR = ROOT / "src" / "i18n" / "catalog"
LANGUAGES_H = ROOT / "src" / "i18n" / "Languages.h"
CJK_SPEC = FONT_DIR / "cjk_faces.txt"

SOURCE_SANS_URL = (
    "https://github.com/adobe-fonts/source-sans/releases/download/"
    "3.052R/TTF-source-sans-3.052R.zip"
)
SOURCE_SANS_MEMBER = "TTF/SourceSans3-Regular.ttf"

# What the Latin face has to draw. Not "what the twelve non-CJK locales need":
# most of this is for FILE NAMES, which are user data in any locale and where a
# single uncovered character is a `?` in the middle of a path.
UNICODES = ",".join([
    "U+0020-007E",     # Basic Latin
    "U+00A0-00FF",     # Latin-1: de/fr/es/pt/it/nl accents
    "U+0100-017F",     # Latin Extended-A: Turkish g-breve, s-cedilla, dotted I
    "U+0180-024F",     # Latin Extended-B: pinyin tone marks, Azerbaijani
    "U+0250-02FF",     # IPA and spacing modifiers -- the Hawaiian okina is here
    "U+0300-036F",     # combining marks: macOS hands out file names in NFD
    "U+0370-03FF",     # Greek
    "U+0400-04FF",     # Cyrillic, whole block -- Serbian and Macedonian too
    "U+1E00-1EFF",     # Latin Extended Additional: Vietnamese
    "U+2000-206F",     # dashes, curly quotes, ellipsis, bullet
    "U+2070-209F",     # superscripts and subscripts
    "U+20A0-20BF",     # currency: dong, won, rupee, ruble, lira
    "U+2100-214F",     # letterlike: the numero sign a Russian path uses
    "U+2150-218F",     # fractions and Roman numerals
    "U+2190-21FF",     # arrows -- the UI writes "-> outputs/scene"
    "U+2200-22FF",     # math operators
    "U+2300-23FF",     # misc technical: the macOS command and option keys
    "U+25A0-25FF",     # geometric shapes
    "U+2600-26FF",     # misc symbols: stars and music notes name a lot of files
    "U+2700-27BF",     # dingbats: check and cross marks
    "U+FB01-FB02",     # fi/fl ligatures
])
# Not U+FFFD: no Source Sans or Noto face has it, so ImGui falls back to '?'.

LAYOUT_FEATURES = "kern,liga,ccmp,locl,mark,mkmk"

# The catalog tag macro (src/i18n/BeginCatalog.h) -> the regional face whose
# glyph forms that language is read in.
TAG_REGION = {"JA": "jp", "ZH_HANS": "sc", "ZH_HANT": "tc", "KO": "kr"}

# Blocks every regional subset carries whatever its own translations use.
# Regional forms differ across all three, so each face carries its own copy.
CJK_ALWAYS = [
    (0x2460, 0x24FF),   # enclosed alphanumerics
    (0x3000, 0x303F),   # CJK symbols and punctuation
    (0xFF01, 0xFFEE),   # halfwidth and fullwidth forms
]

# What ONE face carries for all of them: the four subsets share an atlas
# (src/app/gui/Fonts.cpp), so a second copy would buy nothing but bytes.
CJK_SINGLE = {
    "jp": ([(0x3041, 0x30FF), (0x31F0, 0x31FF),     # kana
            (0x3200, 0x32FF), (0x3300, 0x33FF)],    # enclosed CJK, square forms
           # Of U+2600-27BF Noto Sans CJK has under a fifth, so these are named.
           "★☆♪♫♬♩♥♡♠♣♦"
           "◆◇○●◎△▲▽▼□■"
           "※〒℃℉♂♀✓✔✗✘"
           "☀☁☂☃☺☻❀✿❤➡"),
    "tc": ([(0x3105, 0x312F)], ""),                 # bopomofo
}

# Assigned, but Noto Sans CJK draws nothing for it, so asking would only fail
# tools/check_font_coverage.py -- correctly, since there is no glyph to embed.
NO_GLYPH = {0x332C}                                 # SQUARE PAATU

# The national common-use standard per region -- (codec, lead bytes, the block
# to keep). Decoded out of Python's own codecs so no character list has to be
# committed, downloaded, or kept in step with anything.
COMMON_USE = {
    "sc": ("gb2312", range(0xB0, 0xD8), (0x3400, 0x9FFF)),  # GB 2312 L1, 3755
    "tc": ("big5",   range(0xA4, 0xC7), (0x3400, 0x9FFF)),  # Big5 L1, 5401
    "jp": ("euc_jp", range(0xB0, 0xD0), (0x3400, 0x9FFF)),  # JIS X 0208 L1, 2965
    "kr": ("euc_kr", range(0xB0, 0xC9), (0xAC00, 0xD7A3)),  # KS X 1001, 2350
}

_TRAIL = list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF))


def common_use(region: str) -> set:
    """The region's common-use characters, decoded from its national standard."""
    codec, lead, (lo, hi) = COMMON_USE[region]
    out = set()
    for a in lead:
        for b in _TRAIL:
            try:
                c = bytes([a, b]).decode(codec)
            except UnicodeDecodeError:
                continue
            if lo <= ord(c) <= hi:
                out.add(c)
    return out


def _expand(ranges) -> set:
    """Ranges -> characters, minus the codepoints no font could draw."""
    return {chr(c) for lo, hi in ranges for c in range(lo, hi + 1)
            if c not in NO_GLYPH and unicodedata.category(chr(c)) != "Cn"}


# ---------------------------------------------------------------------------
# Scanning the catalogs
# ---------------------------------------------------------------------------

# TAG("..." "...") -- adjacent string literals, possibly across lines.
_LITERALS = r'(?:"(?:[^"\\]|\\.)*"\s*)+'
_ONE_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "0": "\0",
            '"': '"', "\\": "\\", "'": "'"}


def _unescape(blob: str) -> str:
    """Concatenated C string literals -> the text they denote."""
    out = []
    for m in _ONE_LITERAL.finditer(blob):
        s, i = m.group(1), 0
        while i < len(s):
            if s[i] == "\\" and i + 1 < len(s):
                out.append(_ESCAPES.get(s[i + 1], s[i + 1]))
                i += 2
            else:
                out.append(s[i])
                i += 1
    return "".join(out)


def _languages_h() -> list:
    """SS_LANGUAGES -> [(id, code, native, english)], the one source of truth."""
    text = LANGUAGES_H.read_text(encoding="utf-8")
    rows = re.findall(
        r'X\(\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\)', text)
    if len(rows) < 2:
        raise SystemExit(f"could not read SS_LANGUAGES out of {LANGUAGES_H}")
    return rows


def native_names() -> str:
    """The `native` column of SS_LANGUAGES -- what the picker draws."""
    return "".join(r[2] for r in _languages_h())


def menu_icon() -> str:
    """SS_LANG_MENU_ICON -- the language menu's label."""
    text = LANGUAGES_H.read_text(encoding="utf-8")
    m = re.search(r'#define\s+SS_LANG_MENU_ICON\s+"([^"]*)"', text)
    if not m:
        raise SystemExit(f"could not read SS_LANG_MENU_ICON out of {LANGUAGES_H}")
    return m.group(1)


def always(region: str) -> set:
    """What a regional subset carries regardless of its own translations."""
    ranges, named = CJK_SINGLE.get(region, ([], ""))
    return (set(menu_icon()) | set(native_names()) | common_use(region)
            | set(named) | _expand(CJK_ALWAYS + ranges))


def _tag_hits(text: str, tag: str):
    """Every TAG("...") in `text`, unescaped."""
    for m in re.finditer(tag + r'\(\s*(' + _LITERALS + r')\)', text):
        yield _unescape(m.group(1))


def _catalog_text() -> str:
    return "\n".join(p.read_text(encoding="utf-8")
                     for p in sorted(CATALOG_DIR.glob("*.h")))


def scan_catalogs() -> dict:
    """region id -> the characters that region's translations use.

    Shared with tools/check_font_coverage.py, which imports it.
    """
    per = {r: always(r) for r in TAG_REGION.values()}
    text = _catalog_text()
    for tag, region in TAG_REGION.items():
        for s in _tag_hits(text, tag):
            per[region] |= set(s)
    # ASCII is the Latin face's job; a regional face carrying its own Latin
    # would win the merge for it and quietly restyle every English string.
    return {r: {c for c in cs if ord(c) > 0x7F} for r, cs in per.items()}


def scan_all() -> set:
    """Everything the committed fonts have to cover between them.

    The catalogs of all thirteen languages plus always(), the part that is
    there for file names rather than for anything the UI writes. Tags are
    derived from SS_LANGUAGES, so a new language is scanned the moment it
    exists rather than when someone remembers this file.
    """
    chars = set().union(*(always(r) for r in TAG_REGION.values()))
    text = _catalog_text()
    for row in _languages_h():
        for s in _tag_hits(text, row[0].upper()):
            chars |= set(s)
    return chars


def read_cjk_spec() -> list:
    """assets/fonts/cjk_faces.txt -> [{id, file, url, sha256, bytes, label}]."""
    faces = []
    for line in CJK_SPEC.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split(" | ")
        if len(f) != 6:
            raise SystemExit(f"{CJK_SPEC}: expected 6 fields, got {len(f)}:\n  {line}")
        faces.append(dict(zip(("id", "file", "url", "sha256", "bytes", "label"), f)))
    return faces


# ---------------------------------------------------------------------------
# Building
# ---------------------------------------------------------------------------

def fetch(url: str, expect_sha: str = "") -> bytes:
    print(f"downloading {url}")
    with urllib.request.urlopen(url, timeout=300) as r:
        blob = r.read()
    if expect_sha:
        got = hashlib.sha256(blob).hexdigest()
        if got != expect_sha:
            raise SystemExit(f"{url}\n  expected sha256 {expect_sha}\n  got      {got}")
    return blob


def rename(font, *, family: str, ps_name: str, version: str,
           description: str, forbidden: str = "") -> None:
    """Give the subset its own identity, as the OFL requires of a modification."""
    name = font["name"]
    replacements = {
        1: family,
        2: "Regular",
        3: f"{ps_name};{version}",
        4: family,
        5: version,
        6: ps_name,
        10: description,
        16: family,
        17: "Regular",
    }
    for record in list(name.names):
        # 7 is the trademark notice, which belongs to the upstream foundry and
        # must not be reused; 8/9/11/12/19-22/25 are vendor and sample strings
        # that would all be claims about someone else.
        if record.nameID in (7, 8, 9, 11, 12, 19, 20, 21, 22, 25):
            name.names.remove(record)
            continue
        new = replacements.get(record.nameID)
        if new is not None:
            record.string = new
    if forbidden:
        for record in name.names:
            if record.nameID in (0, 13):
                continue      # copyright + licence: quoted verbatim, as required
            if forbidden in str(record):
                raise SystemExit(
                    f"nameID {record.nameID} still says '{forbidden}': {record}")


def _subset(src: pathlib.Path, dst: pathlib.Path, args: list) -> None:
    subprocess.run(
        [sys.executable, "-m", "fontTools.subset", str(src),
         "--no-hinting", "--desubroutinize", "--drop-tables+=DSIG",
         f"--output-file={dst}"] + args,
        check=True)


def build_latin() -> bytes:
    from fontTools.ttLib import TTFont

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        src, dst = tmp / "in.ttf", tmp / "out.ttf"
        with zipfile.ZipFile(io.BytesIO(fetch(SOURCE_SANS_URL))) as z:
            src.write_bytes(z.read(SOURCE_SANS_MEMBER))
        _subset(src, dst, [f"--unicodes={UNICODES}",
                           f"--layout-features={LAYOUT_FEATURES}"])
        # recalcTimestamp=False, or head.modified is stamped with the wall
        # clock on save and the committed file differs on every run -- which
        # would make --check useless.
        font = TTFont(dst, recalcTimestamp=False)
        rename(font, family="Spirula UI", ps_name="SpirulaUI-Regular",
               version="Version 3.052;Spirula subset",
               description=(
                   "Subset of Source Sans 3 (Adobe, SIL OFL 1.1) covering the "
                   "Latin and Cyrillic ranges Spirula Studio's user interface "
                   "uses. Renamed as the OFL requires of a Modified Version: "
                   "'Source' is a Reserved Font Name."),
               forbidden="Source")
        out = tmp / "final.ttf"
        font.save(out)
        return out.read_bytes()


def build_cjk(face: dict, chars: set) -> bytes:
    from fontTools.ttLib import TTFont

    region = face["id"].upper()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        src, dst = tmp / "in.otf", tmp / "out.otf"
        src.write_bytes(fetch(face["url"], face["sha256"]))

        # Which of the wanted characters this regional face even has. A
        # simplified-only character has no Japanese form to draw, so it is
        # absent here and comes from the SC subset at merge time -- see
        # Fonts.cpp. Reported rather than fatal: it is a translation smell,
        # not a build error.
        have = set(TTFont(src).getBestCmap())
        missing = {c for c in chars if ord(c) not in have}
        if missing:
            print(f"  note: {face['file']} has no glyph for "
                  f"{''.join(sorted(missing))} -- another face will supply it")

        # No layout features: CJK UI text needs no kerning or shaping, and
        # GSUB/GPOS for a CJK face is most of its weight.
        codes = ",".join(f"U+{ord(c):04X}" for c in sorted(chars - missing))
        _subset(src, dst, [f"--unicodes={codes}", "--layout-features="])
        font = TTFont(dst, recalcTimestamp=False)
        rename(font, family=f"Spirula CJK {region}",
               ps_name=f"SpirulaCJK-{region}",
               version="Version 2.004;Spirula subset",
               description=(
                   f"Subset of Noto Sans CJK {region} (Google, SIL OFL 1.1) "
                   f"covering only the characters Spirula Studio's own "
                   f"{face['label']} translations use. Renamed because a file "
                   f"this small is not the font it was cut from."))
        out = tmp / "final.otf"
        font.save(out)
        return out.read_bytes()


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def latin_coverage(blob: bytes) -> None:
    from fontTools.ttLib import TTFont

    cmap = TTFont(io.BytesIO(blob)).getBestCmap()
    samples = {
        "German": "äöüßÄÖÜ",
        "French": "éèêëçàùîïôœÉÀ«»",
        "Spanish": "ñáíóúü¿¡",
        "Portuguese": "ãõâêçÁÍ",
        "Italian": "àèéìòù",
        "Dutch": "ëïĳ",
        "Turkish": "ğĞşŞıİçÇöÖüÜ",
        "Russian": ("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
                    "абвгдеёжзийклмнопрстуфхцчшщъыьэюя"),
        "punctuation": "–—‘’“”…•·×°→←",
    }
    bad = False
    for label, chars in samples.items():
        missing = "".join(c for c in chars if ord(c) not in cmap)
        print(f"  {label:12s} {'OK' if not missing else 'MISSING ' + missing}")
        bad = bad or bool(missing)
    if bad:
        raise SystemExit("coverage check failed")


def report(path: pathlib.Path, blob: bytes, glyphs: int = 0) -> None:
    print(f"  {path.name:24s} {len(blob) / 1024:6.0f} KB"
          + (f"  {glyphs} characters" if glyphs else ""))


# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="rebuild and compare against the committed files")
    ap.add_argument("--latin", action="store_true", help="only the Latin face")
    ap.add_argument("--cjk", action="store_true", help="only the CJK subsets")
    args = ap.parse_args()
    do_latin = args.latin or not args.cjk
    do_cjk = args.cjk or not args.latin

    built = {}
    if do_latin:
        blob = build_latin()
        latin_coverage(blob)
        built[FONT_DIR / "SpirulaUI-Regular.ttf"] = blob
        report(FONT_DIR / "SpirulaUI-Regular.ttf", blob)

    if do_cjk:
        per_region = scan_catalogs()
        for face in read_cjk_spec():
            chars = per_region[face["id"]]
            blob = build_cjk(face, chars)
            path = FONT_DIR / f"SpirulaCJK-{face['id'].upper()}.otf"
            built[path] = blob
            report(path, blob, len(chars))

    total = sum(len(b) for b in built.values())
    print(f"  {'total':24s} {total / 1024:6.0f} KB")

    if args.check:
        bad = False
        for path, blob in built.items():
            want = hashlib.sha256(blob).hexdigest()
            have = (hashlib.sha256(path.read_bytes()).hexdigest()
                    if path.exists() else "(missing)")
            if have != want:
                print(f"FAIL {path.relative_to(ROOT)}\n"
                      f"  committed {have}\n  rebuilt   {want}")
                bad = True
        if bad:
            raise SystemExit("committed fonts differ from a fresh build; "
                             "re-run without --check")
        print(f"OK: {len(built)} committed font(s) match a fresh build")
        return

    FONT_DIR.mkdir(parents=True, exist_ok=True)
    for path, blob in built.items():
        path.write_bytes(blob)
        print(f"wrote {path.relative_to(ROOT)}  "
              f"sha256 {hashlib.sha256(blob).hexdigest()}")


if __name__ == "__main__":
    main()
