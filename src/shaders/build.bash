#!/usr/bin/env bash
# run this from the `src` folder
# tested with slang-2026.2.1-linux-x86_64

set -euo pipefail

compile_args="-ignore-capabilities -line-directive-mode none"
# compile_args="-ignore-capabilities"  # for debugging

out_dir=generated
mkdir -p "$out_dir"

# Define source:output mappings. Add new entries here to build additional slang files.
# Format: "<source_path>:<output_path>"
shaders=(
    "shaders/.slang:${out_dir}/slang.cu"
    "shaders/projection_utils.slang:${out_dir}/projection_utils.cu"
    "shaders/camera_source.slang:${out_dir}/camera_source.cu"
    "shaders/per_pixel_losses.slang:${out_dir}/per_pixel_losses.cu"
    "shaders/per_splat_losses.slang:${out_dir}/per_splat_losses.cu"
    "shaders/pixel_wise.slang:${out_dir}/pixel_wise.cu"
    "shaders/ppisp.slang:${out_dir}/ppisp.cu"
    "shaders/densify.slang:${out_dir}/densify.cu"
    "shaders/primitive_3dgs.slang:${out_dir}/primitive_3dgs.cu"
    "shaders/harmonics.slang:${out_dir}/harmonics.cu"
    "shaders/region.slang:${out_dir}/region.cu"
)

# Files to rename from .cu -> .cuh after postprocessing
mv_targets=(
    "${out_dir}/slang.cu"
    "${out_dir}/projection_utils.cu"
    "${out_dir}/camera_source.cu"
    "${out_dir}/per_pixel_losses.cu"
    "${out_dir}/per_splat_losses.cu"
    "${out_dir}/pixel_wise.cu"
    "${out_dir}/ppisp.cu"
    "${out_dir}/densify.cu"
    "${out_dir}/primitive_3dgs.cu"
    "${out_dir}/harmonics.cu"
    "${out_dir}/region.cu"
)

declare -a pids=()
declare -a out_names=()

# Trap to ensure background jobs are killed on exit
cleanup() {
    for pid in "${pids[@]:-}"; do
        if kill -0 "$pid" >/dev/null 2>&1; then
            kill "$pid" 2>/dev/null || true
        fi
    done
}
trap cleanup EXIT

echo "Starting parallel slangc builds..."
for entry in "${shaders[@]}"; do
    src=${entry%%:*}
    out=${entry#*:}
    logfile="$out_dir/$(basename "$out").log"
    echo "  building: $src -> $out (log: $logfile)"
    slangc "$src" -target cuda -o "$out" $compile_args >"$logfile" 2>&1 &
    pid=$!
    pids+=($pid)
    out_names+=("$out")
done

echo "Waiting for builds to finish..."
failed=0
for i in "${!pids[@]}"; do
    pid=${pids[i]}
    out=${out_names[i]}
    if wait "$pid"; then
        echo "  OK: $out"
    else
        echo "  FAILED: $out (see ${out_dir}/$(basename "$out").log)" >&2
        cat "${out_dir}/$(basename "$out").log"
        failed=1
    fi
done

if [ "$failed" -ne 0 ]; then
    echo "One or more slangc builds failed; aborting." >&2
    exit 1
fi

echo ""

echo "Running postprocess step: python3 shaders/build_postprocess.py"
python3 shaders/build_postprocess.py

echo ""

safe_write() {
    local target="$1"
    local tmp
    tmp="$(mktemp)"

    # read stdin into temporary file
    cat > "$tmp"

    if [ -f "$target" ] && cmp -s "$tmp" "$target"; then
        # identical → do not overwrite → keep timestamp unchanged
        rm "$tmp"
    else
        mv "$tmp" "$target"
    fi
}

echo "Renaming generated .cu files to .cuh..."
for f in "${mv_targets[@]}"; do
    if [ -f "$f" ]; then
        target="${f%.cu}.cuh"
        if [ -f "$target" ] && cmp -s "$f" "$target"; then
            echo "  skip (unchanged): $f"
            rm "$f"
        else
            echo "  mv $f -> $target"
            mv "$f" "$target"
        fi
    else
        echo "  skip (not found): $f"
    fi
done

content=$(cat <<'EOF'
using ::FixedArray;
using ::Matrix;

using ::__ldg;
using ::atomicAdd;
using ::atomicCAS;
using ::atomicExch;

#define _IMPORT_GLOBAL_FUNC(dtype) \
    using ::make_##dtype##2; \
    using ::make_##dtype##3; \
    using ::make_##dtype##4;

_IMPORT_GLOBAL_FUNC(int)
//_IMPORT_GLOBAL_FUNC(bool)
_IMPORT_GLOBAL_FUNC(uint)
_IMPORT_GLOBAL_FUNC(short)
_IMPORT_GLOBAL_FUNC(ushort)
_IMPORT_GLOBAL_FUNC(char)
_IMPORT_GLOBAL_FUNC(uchar)
_IMPORT_GLOBAL_FUNC(longlong)
_IMPORT_GLOBAL_FUNC(ulonglong)
_IMPORT_GLOBAL_FUNC(float)
_IMPORT_GLOBAL_FUNC(double)

#undef _IMPORT_GLOBAL_FUNC
EOF
)
echo "$content" | safe_write "${out_dir}/set_namespace.cuh"

echo "Done." 
