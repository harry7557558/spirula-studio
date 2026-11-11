#!/usr/bin/env python3
"""Fail when a comment block in an uncommitted change is over budget.

AGENTS.md ("Comments -- write fewer, and shorter") caps a file header at 10
lines and any other comment block at 3. Only blocks the working tree actually
touches are checked, so the standing debt in untouched files never blocks a
build; `--all` lists that debt for a deliberate cleanup pass.

Divider rules and section banners do not count toward a block: they are
structure, not prose.

Usage:  python3 tools/check_comment_length.py [--all | --base REV] [--quiet]

--base REV checks every block a branch touched since REV, committed or not.
"""

import os
import re
import subprocess
import sys

HEADER_MAX = 10
BLOCK_MAX = 3

C_EXTS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cu", ".cuh", ".slang"}
HASH_EXTS = {".py", ".sh", ".bash", ".cmake"}

# Vendored, generated, hand-run references, and dated design notes -- the same
# trees tools/check_comments.sh skips, for the same reasons.
EXCLUDED = (
    "src/external/",
    "src/generated/",
    "src/instantiations/",
    "reference/",
    "docs/notes/",
    "viewer/lib/",
    "build/",
)

DIVIDER = re.compile(r"^[=\-*_~#+<>.!/ ]*$")


def run(args):
    # Explicit utf-8: text=True decodes with the locale codec, and a diff
    # touching i18n/catalog/ is full of bytes cp1252 has no character for.
    p = subprocess.run(args, capture_output=True, encoding="utf-8",
                       errors="replace")
    return p.stdout if p.returncode == 0 else None


def in_scope(path):
    ext = os.path.splitext(path)[1]
    if ext not in C_EXTS and ext not in HASH_EXTS:
        return False
    return not any(path.startswith(d) for d in EXCLUDED)


def strip_c_comments(lines):
    """Classify each line as code, comment text, or both.

    Returns (kinds, texts): kinds[i] is "comment" when the line holds nothing
    but comment, "code" when it holds any code, and "" when it is blank.
    texts[i] is the comment text with its markers removed.
    """
    kinds, texts = [], []
    in_block = False
    for line in lines:
        if not in_block and '//' not in line and '/*' not in line:
            kinds.append("code" if line.strip() else "")
            texts.append("")
            continue
        code, text = [], []
        i, n = 0, len(line)
        # A bare `//` separates paragraphs of one block; it must not end it.
        marked = in_block
        while i < n:
            two = line[i:i + 2]
            if in_block:
                if two == "*/":
                    in_block = False
                    i += 2
                else:
                    text.append(line[i])
                    i += 1
            elif two == "//":
                text.append(line[i + 2:])
                marked = True
                i = n
            elif two == "/*":
                in_block = True
                marked = True
                i += 2
            elif line[i] in "\"'":
                quote = line[i]
                code.append(quote)
                i += 1
                while i < n and line[i] != quote:
                    i += 2 if line[i] == "\\" else 1
                i += 1
            elif line[i] == "R" and line[i + 1:i + 2] == '"':
                # Raw string: a // or /* inside it is payload, not a comment.
                close = line.find('"', i + 2)
                code.append(line[i:close + 1 if close >= 0 else n])
                i = close + 1 if close >= 0 else n
            else:
                code.append(line[i])
                i += 1
        text = "".join(text)
        if "".join(code).strip():
            kinds.append("code")
        elif marked:
            kinds.append("comment")
        else:
            kinds.append("")
        texts.append(text.strip(" \t*"))
    return kinds, texts


def strip_hash_comments(lines):
    kinds, texts = [], []
    for n, line in enumerate(lines):
        stripped = line.strip()
        if n == 0 and stripped.startswith("#!"):
            kinds.append("")        # a shebang is not prose
            texts.append("")
        elif stripped.startswith("#"):
            kinds.append("comment")
            texts.append(stripped.lstrip("#").strip())
        elif stripped:
            kinds.append("code")
            texts.append("")
        else:
            kinds.append("")
            texts.append("")
    return kinds, texts


def is_content(text):
    return bool(text) and not DIVIDER.match(text)


def blocks(path, source):
    """Yield (start_line, end_line, content_line_count, budget) per block.

    Line numbers are 1-based and inclusive.
    """
    lines = source.splitlines()
    ext = os.path.splitext(path)[1]
    kinds, texts = (strip_c_comments if ext in C_EXTS else strip_hash_comments)(lines)

    # The file header is the first block, and only while nothing but a
    # shebang, an include guard or `#pragma once` has come before it.
    preamble = re.compile(r"^\s*(#!|#\s*pragma\s+once|#\s*ifndef\b|#\s*define\b)")
    header_open = True

    i = 0
    while i < len(lines):
        if kinds[i] != "comment":
            if kinds[i] == "code" and not preamble.match(lines[i]):
                header_open = False
            i += 1
            continue
        start = i
        while i < len(lines) and kinds[i] == "comment":
            i += 1
        count = sum(1 for t in texts[start:i] if is_content(t))
        yield start + 1, i, count, HEADER_MAX if header_open else BLOCK_MAX
        header_open = False


def changed_lines(root, since="HEAD"):
    """Map path -> set of new-side line numbers, staged and unstaged alike."""
    head = run(["git", "-C", root, "rev-parse", "--verify", since + "^{commit}"])
    if head is None and since != "HEAD":
        return None
    base = head.strip() if head else "4b825dc642cb6eb9a060e54bf8d69288fbee4904"

    paths = [f":(glob)**/*{ext}" for ext in sorted(C_EXTS | HASH_EXTS)]
    paths += [f":(exclude){prefix}" for prefix in EXCLUDED]
    diff = run(["git", "-C", root, "diff", "--unified=0", "--no-color",
                "--diff-filter=d", base, "--", *paths])
    if diff is None:
        return None

    touched, path = {}, None
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            path = line[6:]
        elif line.startswith("@@") and path:
            m = re.search(r"\+(\d+)(?:,(\d+))?", line)
            if m:
                first = int(m.group(1))
                span = int(m.group(2)) if m.group(2) else 1
                touched.setdefault(path, set()).update(range(first, first + span))

    others = run(["git", "-C", root, "ls-files", "--others", "--exclude-standard", "--", *paths])
    for path in (others or "").splitlines():
        if path:
            touched[path] = None  # whole file
    return touched


def report(hits, root):
    print("Comment blocks over budget:\n")
    for path, start, end, count, budget in hits:
        print(f"  {path}:{start}  {count} lines of prose (budget {budget})")
        with open(os.path.join(root, path), encoding="utf-8",
                  errors="replace") as f:
            shown = f.read().splitlines()[start - 1:end][:4]
        for line in shown:
            print(f"      {line.strip()[:88]}")
        if end - start + 1 > 4:
            print(f"      ... {end - start + 1 - 4} more")
        print()


def main():
    args = sys.argv[1:]
    quiet = "--quiet" in args          # say nothing when the tree is clean
    scan_all = "--all" in args
    since = "HEAD"
    if "--base" in args:
        at = args.index("--base")
        if at + 1 >= len(args) or scan_all:
            print("check_comment_length: --base takes a revision, and not with --all")
            return 2
        since = args.pop(at + 1)
        args.pop(at)
    unknown = [a for a in args if a not in ("--all", "--quiet")]
    if unknown:
        print(f"check_comment_length: unknown argument {unknown[0]}")
        return 2

    if os.environ.get("SS_SKIP_COMMENT_CHECK"):
        return 0

    here = os.path.dirname(os.path.abspath(__file__))
    root = run(["git", "-C", here, "rev-parse", "--show-toplevel"])
    if root is None:
        return 0  # no git, or not a checkout -- nothing to compare against
    root = root.strip()

    if scan_all:
        listed = run(["git", "-C", root, "ls-files"]) or ""
        targets = {p: None for p in listed.splitlines() if in_scope(p)}
    else:
        touched = changed_lines(root, since)
        if touched is None:
            if since != "HEAD":
                print(f"check_comment_length: cannot diff against {since}")
                return 2
            return 0
        targets = {p: v for p, v in touched.items() if in_scope(p)}

    hits = []
    for path, lineset in sorted(targets.items()):
        full = os.path.join(root, path)
        if not os.path.isfile(full):
            continue
        with open(full, encoding="utf-8", errors="replace") as f:
            source = f.read()
        for start, end, count, budget in blocks(path, source):
            if count <= budget:
                continue
            if lineset is None or lineset & set(range(start, end + 1)):
                hits.append((path, start, end, count, budget))

    if not hits:
        if scan_all:
            print("OK: no comment block in the tree is over budget.")
        elif not quiet:
            scope = "the working tree" if since == "HEAD" else f"everything since {since}"
            print(f"OK: comment lengths in {scope} are within budget.")
        return 0

    report(hits, root)
    print(f"A file header gets {HEADER_MAX} lines of prose, any other comment "
          f"block {BLOCK_MAX}")
    print("(AGENTS.md, \"Comments -- write fewer, and shorter\"). Cut each block")
    print("to the why -- the measured number, the footgun, the invariant the")
    print("compiler cannot state -- or move it to docs/ and leave a pointer.")
    if not scan_all:
        print("")
        print("`python3 tools/check_comment_length.py --all` lists the rest of the")
        print("tree; SS_SKIP_COMMENT_CHECK=1 skips this check for one build.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
