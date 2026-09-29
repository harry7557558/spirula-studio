import re
import os
from pathlib import Path


def strip_if_zero_blocks(code):
    lines = code.split('\n')
    result = []
    depth = 0  # 0 = active, >0 = inside a #if 0 dead block
    for line in lines:
        stripped = line.strip()
        if depth == 0:
            if re.match(r'#\s*if\s+0\b', stripped):
                depth = 1
            else:
                result.append(line)
        else:
            if re.match(r'#\s*if', stripped):
                depth += 1
            elif re.match(r'#\s*endif', stripped):
                depth -= 1
    return '\n'.join(result)


def extract_function_declarations(code):
    # Regex to match non-inline function declarations
    function_decl_pattern = re.compile(r"""
        \/\*\[AutoHeaderGeneratorExport\]\*\/\s*
        (.*?\))\s*\{
    """, re.MULTILINE | re.VERBOSE | re.DOTALL)
    
    matches = function_decl_pattern.findall(code)
    decls = []

    for m in matches:
        decls.append(m.strip()+';')
    
    return decls


def write_if_changed(path, new_text):
    old = None
    if os.path.exists(path):
        with open(path, "r") as f:
            old = f.read()
    if old != new_text:
        with open(path, "w") as f:
            f.write(new_text)
        return True
    return False


def generate_header(filename, sources):
    path = "src/"

    # `sources` is listed in a fixed order, so the emitted declaration order is
    # stable across machines and the committed header does not churn.
    code = ""
    for source_filename in sources:
        full = path + source_filename
        if not os.path.exists(full):
            raise FileNotFoundError(
                f"{filename}.cuh lists a missing source: {source_filename}. "
                "Update HEADER_SOURCES in generate_headers.py."
            )
        code += open(full).read()
    decls = extract_function_declarations(strip_if_zero_blocks(code))

    splitter = "/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */\n"
    include = open(path + f"{filename}.cuh").read()
    include = include.split(splitter)[0].strip()

    header = '\n\n\n'.join([include, splitter]+decls) + "\n"
    cuh_path = path + f"{filename}.cuh"
    if write_if_changed(cuh_path, header):
        print("Generated", cuh_path)
        return True
    return False


# Which .cu translation units feed each generated header.
#
# Keys and values are src-relative paths (the key without its `.cuh`). A family
# may be spread over several TUs, each named after what it *does* rather than
# after the header it feeds -- splitting oversized files is encouraged, see
# docs/codegen.md. The mapping is explicit rather than inferred from filenames
# so that a source file's name and location are free, and so a typo'd or moved
# file fails loudly instead of silently dropping declarations.
def _fam(directory, header, *sources):
    return (f"{directory}/{header}", [f"{directory}/{s}" for s in sources])


HEADER_SOURCES = dict([
    _fam("kernels/tile", "IntersectTile", "IntersectTile.cu"),
    _fam("kernels/background", "BackgroundSphericalHarmonics",
         "BackgroundSphericalHarmonics.cu"),
    _fam("kernels/loss", "PerSplatLoss", "PerSplatLoss.cu"),
    _fam("kernels/loss", "PerPixelLoss", "PerPixelLoss.cu"),
    _fam("kernels/loss", "FusedSSIM", "FusedSSIM.cu"),
    _fam("kernels/visualize", "Visualizer", "Visualizer.cu"),
    _fam("kernels/bilagrid", "BilagridUtils", "BilagridUtils.cu"),

    # Types only; the kernels live in the Fwd/Bwd headers below.
    _fam("kernels/projection", "Projection"),
    _fam("kernels/projection", "ProjectionFwd", "ProjectionFwd.cu"),
    _fam("kernels/projection", "ProjectionBwd", "ProjectionBwd.cu"),
    _fam("kernels/projection", "ProjectionBwdQuantGrad", "ProjectionBwdQuantGrad.cu"),
    _fam("kernels/projection", "ProjectionPackedFwd", "ProjectionPackedFwd.cu"),

    _fam("kernels/raster", "RasterizationFwd", "RasterizationFwd.cu"),
    _fam("kernels/raster", "RasterizationBwd", "RasterizationBwd.cu"),
    _fam("kernels/raster", "RasterizationEval3DFwd", "RasterizationEval3DFwd.cu"),
    _fam("kernels/raster", "RasterizationEval3DBwd", "RasterizationEval3DBwd.cu"),

    # Optimizer variants, split by function.
    _fam("kernels/optim", "Optimizer",
         "TensorSetZero.cu",           # set_zero_tensor
         "AdamOptim.cu",               # Adam (incl. multi/stepped/8-bit/quat)
         "NewtonOptim.cu",
         "ScaleAgnosticMeanOptim.cu",
         "FusedGeometryOptim.cu",
         "FusedAppearanceOptim.cu",
         "TrustRegion3DGS2Optim.cu",   # 3DGS^2-TR family
         "ColorOptim.cu"),             # linear-RGB / trust-region RGB + SH
    _fam("kernels/optim", "FusedProjectionBwdOptim", "FusedProjectionBwdOptim.cu"),

    # Densification, split by function.
    _fam("kernels/densify", "Densify",
         "DensifySampling.cu",     # quantile/median, indexing, scatter, sampling
         "DensifyScoring.cu",      # covariance scale init, param update
         "Relocation.cu",
         "McmcRelocation.cu",      # MCMC relocation + noise
         "DensifySplitFilter.cu",  # long-axis split, image edge filters
         "RegionWeight.cu"),       # the region test that gates the draw
])

# Image-space per-pixel operations, split by function. PixelWise.cuh is the one
# header fed from two directories: the PPISP kernels live under kernels/ppisp/
# but export into the same surface.
HEADER_SOURCES["kernels/pixelwise/PixelWise"] = [
    "kernels/pixelwise/ImageConvert.cu",      # uint8/uint16 -> float; rendered -> expected depth
    "kernels/pixelwise/ImageColorOps.cu",     # background blending, log map, overexposure reg
    "kernels/pixelwise/DepthGeometry.cu",     # depth -> points/normal, depth-normal loss,
                                              #   ray <-> linear depth
    "kernels/pixelwise/ImageDistort.cu",      # distort / undistort
    "kernels/pixelwise/ImageWarp.cu",         # wide <-> pinhole warps, incl. byte-fused
    "kernels/pixelwise/ImageRedistort.cu",    # resample from an unrepresentable source lens
    "kernels/pixelwise/GtDepthNormalWarp.cu", # GT depth/normal wide -> pinhole warps
    "kernels/ppisp/Ppisp.cu",                 # per-pixel image signal processing
]


def generate_headers():
    """Regenerate headers only if needed."""
    num_generated = 0
    for name, sources in HEADER_SOURCES.items():
        if generate_header(name, sources):
            num_generated += 1
    print(f"Generated {num_generated}/{len(HEADER_SOURCES)} new headers")


generate_headers()
