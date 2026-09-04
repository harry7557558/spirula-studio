#!/bin/bash
# Fail if a source file reaches for __FILE__ instead of SS_FILE.
#
# __FILE__ is the absolute path the build system passed the compiler, so an
# error printed from a shipped binary points into a directory on the machine
# that built it. SS_FILE (src/core/SourcePath.h) trims that to "src/..." at
# compile time.
#
# Usage:  bash tools/check_file_macro.sh

cd "$(dirname "$0")/.." || exit 1

hits=$(git grep -nI --untracked '__FILE__' -- \
    '*.c' '*.cc' '*.cpp' '*.h' '*.hpp' '*.cu' '*.cuh' \
    ':!src/external/' ':!src/generated/' ':!src/core/SourcePath.h')

if [ -n "$hits" ]; then
    echo "__FILE__ used outside src/core/SourcePath.h:"
    echo ""
    echo "$hits"
    echo ""
    echo "Use SS_FILE instead -- #include \"core/SourcePath.h\"."
    exit 1
fi

echo "OK: no bare __FILE__ in the source tree."
