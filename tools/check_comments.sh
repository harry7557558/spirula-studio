#!/bin/bash
# Fail when a comment points at a file that is not in the tree.
#
# Comments rot silently: a path or a filename cited in prose is not compiled,
# not tested, and survives every rename. This repo has had both failure modes
# at once -- citations into a deleted Python trainer (`model.py:547-559`) that
# nobody could check, and a `scripts/mask.py` that had moved to
# `reference/scripts/` and was still printed to users in 13 languages.
#
# Two rules, both deliberately narrow so a hit is always a real defect:
#
#   1. A QUALIFIED path (one containing `/`, e.g. `src/mesh/MeshUV.cpp`) must
#      resolve to a tracked file. It resolves if it matches a tracked path
#      outright, under `src/` (the include root), or as the tail of one -- so
#      the repo's usual shorthand (`backend/vulkan/README.md`,
#      `shaders/meshing.slang`) still passes.
#   2. A BARE `<name>.py` must name some tracked file. Upstream scripts we
#      only reference are listed in EXTERNAL_PY.
#
# URLs, third-party include roots and `./`-relative forms are skipped.
# See the "Comments" section of AGENTS.md for what the rules are protecting.
#
# Usage:  bash tools/check_comments.sh

cd "$(dirname "$0")/.." || exit 1

# Python files that live in someone else's repo and are referenced as
# provenance -- upstream model definitions and the sam3.cpp converters.
EXTERNAL_PY='^(lightglue|aliked|blocks|hieradet|sam2|sam3|convert_sam2_to_ggml|convert_sam3_to_ggml|ViT_DINO_reg|RAFTDepthNormalDPTDecoder5|vision_transformer|modules|infer_panorama|dad|dedode|loma)\.py$'

# Include roots owned by a toolchain or a vendored dependency.
EXTERNAL_DIR='^(cub|vulkan|GLFW|glm|thrust|cooperative_groups|backends|misc|geogram|nets|imgui)/'

EXTS='py|cpp|cu|cuh|slang|md|bash|cmake'

# Excluded: vendored, generated, hand-run references, dated design notes (which
# record what was true when written), and the SS_FILE trim test, whose "paths"
# are synthetic inputs that name no file on purpose.
paths=(':!src/external/' ':!src/generated/' ':!src/instantiations/'
       ':!reference/' ':!docs/notes/' ':!tools/check_comments.sh'
       ':!src/core/tests/source_path.cpp')

tracked=$(git ls-files)
fail=0

resolves() {
    local p=$1
    printf '%s\n' "$tracked" | grep -qxF "$p"      && return 0
    printf '%s\n' "$tracked" | grep -qxF "src/$p"  && return 0
    printf '%s\n' "$tracked" | grep -qE "/$(printf '%s' "$p" | sed 's/[.[\*^$]/\\&/g')\$"
}

report() {
    [ $fail -eq 0 ] && echo "Comments cite files that are not in the tree:" && echo ""
    fail=1
    echo "  $1"
    git grep -nIE "(^|[^A-Za-z0-9_/.-])$(printf '%s' "$1" | sed 's/\./\\./g')" \
        -- "${paths[@]}" "${globs[@]}" \
        | grep -vE "$URL" | sed 's/^/      /' | head -3
    echo ""
}

# A URL contains slashes and a dotted tail, so it reads as a path. Drop those
# lines before extracting anything.
URL='https?://|www\.|github\.com|githubusercontent|huggingface\.co'
globs=(':(glob)**/*.'{h,cpp,cu,cuh,slang,cmake,bash,md})
lines=$(git grep -hIE "[A-Za-z0-9_.-]+\.($EXTS)\b" -- "${paths[@]}" "${globs[@]}" \
        | grep -vE "$URL")

# ---- rule 1: qualified paths -----------------------------------------------
# Scanned over every line, not just comments: the `scripts/mask.py` this was
# written for was in a user-facing string, in 13 languages.
qualified=$(printf '%s\n' "$lines" \
    | grep -oE "[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_.-]+)+\.($EXTS)\b" \
    | grep -vE "$EXTERNAL_DIR" | sort -u)

for p in $qualified; do
    resolves "$p" || report "$p"
done

# ---- rule 2: bare .py filenames --------------------------------------------
# Prose only -- comment lines in code, every line in markdown. An unqualified
# `<word>.py` in code is a member access on a struct with a `py` field
# (PreviewRenderer's `o.py`), not a citation.
prose=$( { git grep -hIE '\.py\b' -- "${paths[@]}" \
             ':(glob)**/*.'{h,cpp,cu,cuh,slang,cmake,bash} \
             | grep -E '^[[:space:]]*(//|\*|#)|//'
           git grep -hIE '\.py\b' -- "${paths[@]}" ':(glob)**/*.md'
         } | grep -vE "$URL")

bare=$(printf '%s\n' "$prose" \
    | grep -oE '(^|[^A-Za-z0-9_/.-])[A-Za-z0-9_-]+\.py\b' \
    | grep -oE '[A-Za-z0-9_-]+\.py$' | grep -vE "$EXTERNAL_PY" | sort -u)

for p in $bare; do
    printf '%s\n' "$tracked" | grep -qE "(^|/)$p\$" || report "$p"
done

if [ $fail -ne 0 ]; then
    echo "Fix the path, or delete the citation -- a pointer nobody can follow is"
    echo "worse than no pointer. Upstream files belong in EXTERNAL_PY /"
    echo "EXTERNAL_DIR in this script."
    exit 1
fi

echo "OK: every file cited in a comment exists."
