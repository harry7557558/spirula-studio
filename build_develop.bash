#!/bin/bash

# Development build for Linux and macOS. Extra arguments are passed to CMake:
#   ./build_develop.bash -DSS_BACKEND=vulkan -DSS_BUILD_GUI=OFF

# One tree per backend, so building one does not reconfigure the other. The
# arguments are read, never consumed: your own -B still reaches CMake, and the
# build step below would then target a tree it did not configure.
ss_backend=cuda
for arg in "$@"; do
    case "$arg" in
        -DSS_BACKEND=*|-DSSPLAT_BACKEND=*)
            ss_backend=$(printf '%s' "${arg#*=}" | tr 'A-Z' 'a-z') ;;
    esac
done
if [ "$(uname)" = "Darwin" ]; then
    build_dir=build     # one backend on macOS (Vulkan via MoltenVK)
else
    build_dir="build_$ss_backend"
fi

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

# The mask editor's testable half is built without SS_BUILD_SAM; an unguarded
# sam/ include there is invisible to mask_doc_test's link-level symbol gate.
bash tools/check_sam_guard.sh >/dev/null || { bash tools/check_sam_guard.sh; exit 1; }

# The mask panel has no unit seam; this pins its gates as text, and names each
# one that went missing.
bash tools/mask_editor_checks/survivors.sh >/dev/null ||
    { bash tools/mask_editor_checks/survivors.sh | command grep '^FAIL'; exit 1; }

# The licence dialog has no unit seam either; this pins that its Accept records
# through the store the CLI reads.
bash tools/license_checks/survivors.sh >/dev/null ||
    { bash tools/license_checks/survivors.sh | command grep '^FAIL'; exit 1; }

# The embedded licence texts are generated from LICENSES/; a stale copy would show
# the user terms other than the file says.
if command -v python3 >/dev/null 2>&1; then
    python3 tools/gen_license_texts.py --check ||
        { echo "src/core/LicenseTexts.cpp is stale: python3 tools/gen_license_texts.py"; exit 1; }
fi

# Comment blocks in uncommitted work must fit the AGENTS.md budget, and no
# source may name a <windows.h> macro. CMake (cmake/SsChecks.cmake) runs both
# too; running them here fails before the configure step rather than after it.
if command -v python3 >/dev/null 2>&1; then
    python3 tools/check_comment_length.py || exit 1
    python3 tools/check_winmacro.py --quiet || exit 1
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

cmake -G Ninja -B "$build_dir" "$@" || exit $?

# Repair the ninja dependency log.
if [ -f "$build_dir/.ninja_deps" ]; then
    ss_deps_broken() {
        case "$1" in
            *"premature end of file"*|*"bad deps log signature"*|\
            *"bad deps log version"*) return 0 ;;
        esac
        return 1
    }
    if ss_deps_broken "$(cmake --build "$build_dir" -- -t recompact 2>&1)"; then
        # The rewrite is what heals it, so a second pass is what confirms it.
        if ss_deps_broken "$(cmake --build "$build_dir" -- -t recompact 2>&1)"; then
            echo "$build_dir/.ninja_deps did not survive a recompact -- removing it"
            rm -f "$build_dir/.ninja_deps"
        else
            echo "repaired a corrupt $build_dir/.ninja_deps (this build recompiles everything once)"
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
    # A container's or Slurm job's cgroup limit binds before MemAvailable does:
    # the tightest over our cgroup and its ancestors, as src/core/HostMemory.h reads it.
    cg_root=/sys/fs/cgroup cg_limits="memory.max memory.high" cg_usage=memory.current cg_cache=inactive_file
    cg_path=$(awk -F: '$1 == "0" {print $3}' /proc/self/cgroup 2>/dev/null)
    cg_v1=$(awk -F: '$2 ~ /(^|,)memory(,|$)/ {print $3}' /proc/self/cgroup 2>/dev/null)
    if [ -n "$cg_v1" ]; then
        cg_root=/sys/fs/cgroup/memory cg_path=$cg_v1 cg_limits=memory.limit_in_bytes
        cg_usage=memory.usage_in_bytes cg_cache=total_inactive_file
    fi
    cg_dir=$cg_root${cg_path%/}
    [ -r "$cg_dir/$cg_usage" ] || cg_dir=$cg_root
    while :; do
        usage=$(cat "$cg_dir/$cg_usage" 2>/dev/null)
        cache=$(awk -v k="$cg_cache" '$1 == k {print $2}' "$cg_dir/memory.stat" 2>/dev/null)
        for file in $cg_limits; do
            limit=$(cat "$cg_dir/$file" 2>/dev/null)
            [[ $limit =~ ^[0-9]+$ && $usage =~ ^[0-9]+$ ]] && (( limit < 1 << 62 )) || continue
            left_mb=$(( (limit - usage + ${cache:-0}) / 1048576 ))
            (( left_mb < AVAILABLE_MB )) && AVAILABLE_MB=$(( left_mb > 0 ? left_mb : 0 ))
        done
        [ "$cg_dir" = "$cg_root" ] && break
        cg_dir=${cg_dir%/*}
    done
fi
MAX_JOBS_FROM_RAM=$(( AVAILABLE_MB / JOB_RAM_MB ))
JOBS=$(( MAX_JOBS_FROM_RAM < CPU_CORES ? MAX_JOBS_FROM_RAM : CPU_CORES ))
[ "$JOBS" -lt 1 ] && JOBS=1
echo "Build dir     : ${build_dir}"
echo "Available RAM : ${AVAILABLE_MB} MB"
echo "CPU cores     : ${CPU_CORES}"
echo "Using jobs    : ${JOBS}"
echo ""
# Propagate the build's exit status.
if ! cmake --build "$build_dir" --verbose -j"${JOBS}"; then
    echo "BUILD FAILED" >&2
    exit 1
fi

echo ""
echo "Build complete: $build_dir/spirula"
