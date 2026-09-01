#!/bin/bash

# Development build for Linux. Extra arguments are passed to CMake, e.g.
#   ./build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON

# Regenerate headers. Skipped when python3 is unavailable -- the generated
# files are committed, so the build still works without it.
if command -v python3 >/dev/null 2>&1; then
    python3 tools/codegen/generate_headers.py
    python3 tools/codegen/generate_kernel_instantiation.py
else
    echo "python3 not found -- skipping codegen (using committed generated files)"
fi

# SS_ is a short prefix; refuse to define a name <signal.h> or winuser.h owns.
bash tools/check_ss_prefix.sh >/dev/null || exit 1

# The GUI must not hand ImGui a string that never became a translatable
# message. Prints the remaining SS_MSG_EN count, which is the Phase 4 TODO.
bash tools/check_i18n.sh || exit 1

# An error message must name a path the person reading it can open, so
# __FILE__ goes through SS_FILE (src/core/SourcePath.h).
bash tools/check_file_macro.sh >/dev/null || { bash tools/check_file_macro.sh; exit 1; }

# The embedded fonts are subset to the characters the catalogs use, so editing
# a translation can outgrow them. Cheap: no network, no fontTools.
if command -v python3 >/dev/null 2>&1; then
    python3 tools/check_font_coverage.py || exit 1
fi

# A comment must not cite a file that does not exist -- an unfollowable
# pointer reads exactly like a live one.
bash tools/check_comments.sh >/dev/null || { bash tools/check_comments.sh; exit 1; }

# Comment blocks in uncommitted work must fit the AGENTS.md budget. Also wired
# into CMake (cmake/SsChecks.cmake), which covers a bare cmake/ninja build;
# running it here fails before the configure step rather than after it.
if command -v python3 >/dev/null 2>&1; then
    python3 tools/check_comment_length.py || exit 1
fi

# Homebrew's libomp is keg-only: nothing points at it, so CMake finds no
# OpenMP and meshing, UV unwrap and metrics run serial (cmake/SsMacBundle.cmake
# then has no dylib to make static either).
if [ "$(uname)" = "Darwin" ]; then
    case " $* " in
        *" -DOpenMP_ROOT="*) ;;
        *)
            for omp_root in "$(brew --prefix libomp 2>/dev/null)" \
                            /opt/homebrew/opt/libomp /usr/local/opt/libomp; do
                [ -n "$omp_root" ] || continue
                [ -f "$omp_root/lib/libomp.a" ] ||
                    [ -f "$omp_root/lib/libomp.dylib" ] || continue
                set -- "$@" "-DOpenMP_ROOT=$omp_root"
                echo "OpenMP: passing -DOpenMP_ROOT=$omp_root (keg-only formula)"
                break
            done ;;
    esac
fi

cmake -G Ninja -B build "$@" || exit $?

# Repair the ninja dependency log.
if [ -f build/.ninja_deps ]; then
    ss_deps_broken() {
        case "$1" in
            *"premature end of file"*|*"bad deps log signature"*|\
            *"bad deps log version"*) return 0 ;;
        esac
        return 1
    }
    if ss_deps_broken "$(cmake --build build -- -t recompact 2>&1)"; then
        # The rewrite is what heals it, so a second pass is what confirms it.
        if ss_deps_broken "$(cmake --build build -- -t recompact 2>&1)"; then
            echo "build/.ninja_deps did not survive a recompact -- removing it"
            rm -f build/.ninja_deps
        else
            echo "repaired a corrupt build/.ninja_deps (this build recompiles everything once)"
        fi
    fi
fi

echo ""

JOB_RAM_MB=750   # 750MB per job
if [ "$(uname)" = "Darwin" ]; then
    # macOS has no MemAvailable; approximate with free+inactive pages.
    PAGE_SIZE=$(sysctl -n hw.pagesize)
    FREE_PAGES=$(vm_stat | awk -F'[:.]' '/Pages (free|inactive)/ {gsub(/ /,"",$2); sum+=$2} END {print sum}')
    AVAILABLE_MB=$(( FREE_PAGES * PAGE_SIZE / 1048576 ))
    CPU_CORES=$(sysctl -n hw.ncpu)
else
    AVAILABLE_KB=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
    AVAILABLE_MB=$(( AVAILABLE_KB / 1024 ))
    CPU_CORES=$(nproc)
fi
MAX_JOBS_FROM_RAM=$(( AVAILABLE_MB / JOB_RAM_MB ))
JOBS=$(( MAX_JOBS_FROM_RAM < CPU_CORES ? MAX_JOBS_FROM_RAM : CPU_CORES ))
[ "$JOBS" -lt 1 ] && JOBS=1
echo "Available RAM : ${AVAILABLE_MB} MB"
echo "CPU cores     : ${CPU_CORES}"
echo "Using jobs    : ${JOBS}"
echo ""
# Propagate the build's exit status.
if ! cmake --build build --verbose -j"${JOBS}"; then
    echo "BUILD FAILED" >&2
    exit 1
fi
