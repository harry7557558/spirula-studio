import os
from collections import OrderedDict
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

import re
import itertools


SRC_DIR = Path("src")
DST_DIR = SRC_DIR / "instantiations"


# The (camera model, distortion tier) pairs the CUDA kernels are instantiated
# for. Must match camera_distortion_is_compiled() in src/core/CameraModel.h,
# SS_FOR_EACH_CAMERA_VARIANT in src/kernels/projection/CameraVariants.cuh and
# the export list in src/shaders/primitive_3dgs.slang.
kCameraVariants = [
    ("CameraModelType::PINHOLE",         "CameraDistortionType::None"),
    ("CameraModelType::PINHOLE",         "CameraDistortionType::OpenCV"),
    ("CameraModelType::PINHOLE",         "CameraDistortionType::ThinPrism"),
    ("CameraModelType::FISHEYE",         "CameraDistortionType::None"),
    ("CameraModelType::FISHEYE",         "CameraDistortionType::OpenCV"),
    ("CameraModelType::FISHEYE",         "CameraDistortionType::ThinPrism"),
    ("CameraModelType::EQUISOLID",       "CameraDistortionType::None"),
    ("CameraModelType::EQUISOLID",       "CameraDistortionType::OpenCV"),
    ("CameraModelType::EQUISOLID",       "CameraDistortionType::ThinPrism"),
    ("CameraModelType::EQUIRECTANGULAR", "CameraDistortionType::None"),
]

# DensifyAccumMode (primitives/Primitive.cuh): how the raster backward reduces
# the densification error map to one score per splat.
kAccumModes = [
    "DensifyAccumMode::None", "DensifyAccumMode::Max",
    "DensifyAccumMode::Sum",  "DensifyAccumMode::Avg",
]

# Interleaved [3DGS, Mip, 3DGUT] per SH degree; the `includes` lists below rely
# on that ordering to route each block to the right primitive header.
kPrimitives = [
    "Vanilla3DGS<0>", "MipSplatting<0>", "Vanilla3DGUT<0>",
    "Vanilla3DGS<1>", "MipSplatting<1>", "Vanilla3DGUT<1>",
    "Vanilla3DGS<2>", "MipSplatting<2>", "Vanilla3DGUT<2>",
    "Vanilla3DGS<3>", "MipSplatting<3>", "Vanilla3DGUT<3>",
    "Vanilla3DGS<4>", "MipSplatting<4>", "Vanilla3DGUT<4>",
]


def extract_kernel_definition(header_src: Path, kernel_name: str):
    header_src = SRC_DIR / header_src
    with open(header_src, "r", encoding="utf-8") as f:
        src = f.read()

    pattern = re.compile(
        f"""(void\\s+{kernel_name}(.*?)\\)\\s*;)""",
        re.MULTILINE | re.VERBOSE | re.DOTALL
    )

    matches = pattern.findall(src)
    assert len(matches) == 1, matches

    return matches[0][0]


# Every file emitted this run, so the stale ones can be pruned at the end. A
# leftover from a larger kCameraVariants still compiles (cmake/sources.txt
# globs the directory) and fails on the enumerator it names.
_emitted: set = set()


def write_if_changed(path, new_text):
    old = None
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            old = f.read()
    if old != new_text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(new_text)
        return True
    return False


def _short_slug(text: str, max_len: int = 8) -> str:
    """
    Turn a long identifier / filename stem into a short readable tag.
    """
    stem = Path(text).stem
    stem = stem.replace("3D", "3")

    # Prefer uppercase/digit structure if present.
    tokens = re.findall(r"[A-Z]+|\d+", stem)
    if tokens:
        slug = "".join(tok[0] for tok in tokens if tok)
    else:
        slug = re.sub(r"[^A-Za-z0-9]+", "", stem)

    slug = slug or "x"
    return slug[:max_len]


def _stable_hash(text: str, nbytes: int = 2) -> str:
    # 4 bytes -> 8 hex chars; short but usually plenty for filenames.
    import hashlib
    return hashlib.blake2s(text.encode("utf-8"), digest_size=nbytes).hexdigest()


def kernel_complexity_score(map_values, include_values):
    # Tune these weights however you like.
    # Example: headers are expensive, long template arguments are expensive.
    return (
        10.0 * len(include_values) +
        # 0.25 * sum(len(x) for x in map_values) +
        2.0 * sum("<" in x or ">" in x for x in map_values)
    )


def generate_kernel_instantiation(
    prefix: str,
    template_definition: str,
    map_header: List[Optional[str]],
    maps: List[List[str]],
    includes: List[List[str]],
    score_fn: Optional[Callable[[Sequence[str], Sequence[str]], float]] = kernel_complexity_score,
    max_file_score: float = 200.0,
    _names: Optional[set] = None,
):
    """
    Generate one or more .cu files.

    Behavior:
      1) Kernels with different include sets are always kept in different files.
      2) Kernels with the same include set are merged into the same file until
         the total score reaches max_file_score.
      3) File names are short and stable, with a hash suffix for uniqueness.

    score_fn(map_values, include_values) should return a tunable "complexity"
    score for a single kernel instantiation.
    """
    if _names is None:
        _names = set()

    assert len(maps) == len(includes), (len(maps), len(includes))
    assert len(map_header) >= 0

    if score_fn is None:
        # Conservative default; tune or replace this in the caller.
        def score_fn(map_values: Sequence[str], include_values: Sequence[str]) -> float:
            return 1.0 + 0.15 * len(include_values) + 0.01 * sum(len(x) for x in map_values)

    def instantiate_definition(map_values: Sequence[str]) -> str:
        definition = "template " + template_definition
        assert len(map_values) == len(map_header), (len(map_values), len(map_header))

        for src, dst in zip(map_header, map_values):
            if src is not None:
                definition = definition.replace(src, dst)

        template = "<\n    " + ",\n    ".join(map_values) + "\n>"
        definition = definition.replace("(", template + "(", 1)
        return definition

    def emit_file(include_key: Tuple[str, ...], batch: List[List[str]], batch_index: int) -> None:
        # Build file content first so the hash can reflect the exact output.
        content = ""
        content += "// This file is auto generated by `generate_kernel_instantiation.py`\n\n"
        content += "#define NO_TORCH\n"
        for inc in include_key:
            content += f'#include "{inc}"\n'
        content += "\n"

        for i, map_values in enumerate(batch):
            if i:
                content += "\n"
            content += instantiate_definition(map_values)
            content += "\n"

        # Slug from the basenames, so path-qualifying the includes did not
        # rename every generated file.
        include_tag = _short_slug(
            "_".join(Path(inc).name for inc in include_key), max_len=10)
        prefix_tag = _short_slug(prefix, max_len=12)
        digest = _stable_hash(prefix_tag + "|" + include_tag + "|" + str(batch_index) + "|" + content)

        # filename = f"{prefix_tag}_{include_tag}_{batch_index:02d}_{digest}.cu"
        filename = f"{prefix_tag}_{include_tag}"
        if filename in _names:
            batch_index = 1
            while f"{filename}_{batch_index}" in _names:
                batch_index += 1
            filename = f"{filename}_{batch_index}"
        assert filename not in _names, f"Duplicate file name: {filename}"
        _names.add(filename)
        filename += ".cu"

        os.makedirs(DST_DIR, exist_ok=True)
        _emitted.add(filename)
        if write_if_changed(DST_DIR / filename, content):
            print("Generated", filename)

    # Group by exact include list. This keeps kernels with different include
    # sets in separate files.
    grouped: "OrderedDict[Tuple[str, ...], List[Tuple[List[str], float]]]" = OrderedDict()
    for map_values, include_values in zip(maps, includes):
        include_key = tuple(include_values)
        grouped.setdefault(include_key, [])
        grouped[include_key].append((map_values, float(score_fn(map_values, include_values))))

    # Split each include group into multiple files if the score budget would be exceeded.
    for include_key, items in grouped.items():
        batch: List[List[str]] = []
        batch_score = 0.0
        batch_index = 0

        for map_values, score in items:
            if batch and (batch_score + score > max_file_score):
                emit_file(include_key, batch, batch_index)
                batch = []
                batch_score = 0.0
                batch_index += 1

            batch.append(map_values)
            batch_score += score

        if batch:
            emit_file(include_key, batch, batch_index)


def generate_ProjectionFwd():
    for filename, kernel_filename, prefix, wrapper_name in [
        ("kernels/projection/ProjectionFwd.cu", "kernels/projection/ProjectionFwd_kernel.cuh", "ProjectionFwd", "projection_fused_fwd_kernel_wrapper"),
        ("kernels/projection/ProjectionPackedFwd.cu", "kernels/projection/ProjectionPackedFwd_kernel.cuh", "ProjectionPackedMask", "projection_packed_mask_kernel_wrapper"),
        ("kernels/projection/ProjectionPackedFwd.cu", "kernels/projection/ProjectionPackedFwd_kernel.cuh", "ProjectionPackedFwd", "projection_packed_fwd_kernel_wrapper"),
    ]:
        definition = extract_kernel_definition(filename, wrapper_name)
        map_header = ["typename SplatPrimitive", None, None]
        map_body = [
            (prim, cam, dist)
            for prim in kPrimitives
            for cam, dist in kCameraVariants
        ]
        nv = len(kCameraVariants)
        includes = [*(
            [("primitives/Primitive3DGS.cuh", kernel_filename)] * (2 * nv) +
            [("primitives/Primitive3DGUT.cuh", kernel_filename)] * nv
        )] * 5

        generate_kernel_instantiation(prefix, definition, map_header, map_body, includes)


def _generate_projection_bwd(prefix, source, wrapper_name, kernel_filename):
    definition = extract_kernel_definition(source, wrapper_name)
    map_header = ["typename SplatPrimitive", None, None]
    map_body = [
        (prim, cam, dist)
        for prim in kPrimitives
        for cam, dist in kCameraVariants
    ]
    nv = len(kCameraVariants)
    includes = [*(
        [("primitives/Primitive3DGS.cuh", kernel_filename)] * (2 * nv) +
        [("primitives/Primitive3DGUT.cuh", kernel_filename)] * nv
    )] * 5

    generate_kernel_instantiation(prefix, definition, map_header, map_body, includes)


def generate_ProjectionBwd():
    _generate_projection_bwd(
        "ProjectionBwd", "kernels/projection/ProjectionBwd.cu",
        "projection_fused_bwd_kernel_wrapper",
        "kernels/projection/ProjectionBwd_kernel.cuh")


def generate_ProjectionBwdQuantGrad():
    _generate_projection_bwd(
        "ProjectionBwdQuantGrad", "kernels/projection/ProjectionBwdQuantGrad.cu",
        "projection_bwd_quantgrad_kernel_wrapper",
        "kernels/projection/ProjectionBwdQuantGrad_kernel.cuh")


def generate_FusedProjectionBwdOptim():
    definition = extract_kernel_definition("kernels/optim/FusedProjectionBwdOptim.cu", "fused_projection_bwd_optimizer_3dgs_kernel_wrapper")
    # Six template args: SplatPrimitive, camera_model, distortion,
    # use_scale_agnostic_mean, color_trust_linear, LEVEL.
    #
    # LEVEL collapses the prior (QUANT_BITS, VALUE_BITS) axes into 2 combos:
    #   0 = off    (32-bit value, fp32 optim state)
    #   1 = light  (16-bit value, 8-bit packed optim)
    # The wrapper derives BLOCK_SIZE / QUANT_BITS / VALUE_BITS internally via
    # constexpr, so each wrapper compiles ONE kernel specialization.
    map_header = ["typename SplatPrimitive", None, None, None, None, None]
    map_body = [
        (prim, cam, dist, sam, ctl, level)
        for prim in kPrimitives
        for cam, dist in kCameraVariants
        for sam  in ("true", "false")
        for ctl  in ("true", "false")
        for level in ("0", "1")
    ]
    # Per primitive: len(kCameraVariants) * 2 sam * 2 ctl * 2 levels.
    per_prim = len(kCameraVariants) * 8
    includes = [*(
        [("primitives/Primitive3DGS.cuh",   "kernels/optim/FusedProjectionBwdOptim_kernel.cuh")] * (per_prim * 2) +
        [("primitives/Primitive3DGUT.cuh",  "kernels/optim/FusedProjectionBwdOptim_kernel.cuh")] * per_prim
    )] * 5

    generate_kernel_instantiation("FusedProjectionBwdOptim", definition, map_header, map_body, includes)


def generate_RasterizationFwd():
    definition = extract_kernel_definition("kernels/raster/RasterizationFwd.cu", "rasterize_to_pixels_fwd_kernel_wrapper")
    map_header = ["typename SplatPrimitive", None, None]
    map_body = [*itertools.product(
        ["Vanilla3DGS<0>"],
        # distortion channel set; DN/RGB_DN (normal) added when a normal-
        # rendering primitive exists.
        ["DistortionType::None", "DistortionType::D", "DistortionType::RGB_D"],
        ["true", "false"],   # output_median
    )]
    includes = [("primitives/Primitive3DGS.cuh", "kernels/raster/RasterizationFwd_kernel.cuh")] * 6

    generate_kernel_instantiation("RasterizationFwd", definition, map_header, map_body, includes)


def generate_RasterizationBwd():
    definition = extract_kernel_definition("kernels/raster/RasterizationBwd.cu", "rasterize_to_pixels_bwd_kernel_wrapper")
    map_header = ["typename SplatPrimitive", None, None, None]
    map_body = [*itertools.product(
        ["Vanilla3DGS<0>"],
        ["DistortionType::None", "DistortionType::D", "DistortionType::RGB_D"],  # dist_type
        kAccumModes,
        ["true", "false"],   # output_median
    )]
    includes = [("primitives/Primitive3DGS.cuh", "kernels/raster/RasterizationBwd_kernel.cuh")] * len(map_body)

    generate_kernel_instantiation("RasterizationBwd", definition, map_header, map_body, includes)


def generate_RasterizationEval3DFwd():
    definition = extract_kernel_definition("kernels/raster/RasterizationEval3DFwd.cu", "rasterize_to_pixels_eval3d_fwd_kernel_wrapper")
    map_header = ["typename SplatPrimitive", None, None, None, None]
    map_body = [
        (prim, cam, dist, dt, median)
        for prim in ["Vanilla3DGUT<0>"]
        for cam, dist in kCameraVariants
        for dt in ("DistortionType::None", "DistortionType::D", "DistortionType::RGB_D")
        for median in ("true", "false")
    ]
    includes = [("primitives/Primitive3DGUT.cuh", "kernels/raster/RasterizationEval3DFwd_kernel.cuh")] * len(map_body)

    generate_kernel_instantiation("RasterizationEval3DFwd", definition, map_header, map_body, includes)


def generate_RasterizationEval3DBwd():
    definition = extract_kernel_definition("kernels/raster/RasterizationEval3DBwd.cu", "rasterize_to_pixels_eval3d_bwd_kernel_wrapper")
    map_header = ["typename SplatPrimitive", None, None, None, None, None, None]
    map_body = [
        (prim, cam, dist, dt, viewmat_grad, accum_weight, median)
        for prim in ["Vanilla3DGUT<0>"]
        for cam, dist in kCameraVariants
        for dt in ("DistortionType::None", "DistortionType::D", "DistortionType::RGB_D")
        for viewmat_grad in ("true", "false")
        for accum_weight in kAccumModes
        for median in ("true", "false")
    ]
    includes = [("primitives/Primitive3DGUT.cuh", "kernels/raster/RasterizationEval3DBwd_kernel.cuh")] * len(map_body)

    generate_kernel_instantiation("RasterizationEval3DBwd", definition, map_header, map_body, includes)


generate_ProjectionFwd()
generate_ProjectionBwd()
generate_ProjectionBwdQuantGrad()
generate_FusedProjectionBwdOptim()
generate_RasterizationFwd()
generate_RasterizationBwd()
generate_RasterizationEval3DFwd()
generate_RasterizationEval3DBwd()

for stale in sorted(DST_DIR.glob("*.cu")):
    if stale.name not in _emitted:
        stale.unlink()
        print("Removed", stale.name)
