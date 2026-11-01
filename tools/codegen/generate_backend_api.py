"""Generate backend API module headers (src/backend/api/) as forwarders.

Each module header groups the per-kernel launch headers for one subsystem.
The AUTHORITATIVE declarations (and the types they use) live in the per-kernel
.cuh headers, whose declaration sections are generated from
/*[AutoHeaderGeneratorExport]*/ markers by generate_headers.py. Those headers
are CUDA-include-free (they use backend/api/BackendTypes.h), so this whole
surface parses without the CUDA toolkit under -DSS_BACKEND_VULKAN.

Forwarding (rather than copying declarations) keeps one definition of every
preamble type: a TU may include both a module header and an individual .cuh
without ODR violations.

Run from repo root: python3 tools/codegen/generate_backend_api.py
"""

import os

SRC = "src/"
OUT_DIR = SRC + "backend/api/"

# module header -> (per-kernel headers, one-line description)
MODULES = {
    "Projection.h": (
        ["kernels/projection/ProjectionFwd.cuh", "kernels/projection/ProjectionBwd.cuh", "kernels/projection/ProjectionPackedFwd.cuh",
         "kernels/projection/ProjectionBwdQuantGrad.cuh", "kernels/optim/FusedProjectionBwdOptim.cuh"],
        "Splat projection forward/backward, packed variants, quantized-gradient"
        " backward, and the fused projection-backward + optimizer path.",
    ),
    "Rasterization.h": (
        ["kernels/raster/RasterizationFwd.cuh", "kernels/raster/RasterizationBwd.cuh",
         "kernels/raster/RasterizationEval3DFwd.cuh", "kernels/raster/RasterizationEval3DBwd.cuh",
         "kernels/raster/RasterizationMomentsFwd.cuh"],
        "Tile-based rasterization forward/backward (2D and eval-3D/3DGUT).",
    ),
    "TileIntersect.h": (
        ["kernels/tile/IntersectTile.cuh", "kernels/tile/SplatTileIntersector.cuh"],
        "Splat-to-tile intersection: key generation, radix sort, tile ranges.",
    ),
    "PixelWise.h": (
        ["kernels/pixelwise/PixelWise.cuh"],
        "Per-pixel image ops: background blend, depth/normal transforms,"
        " warps, color-space, PPISP.",
    ),
    "Optimizer.h": (
        ["kernels/optim/Optimizer.cuh", "kernels/optim/ProjectiveDepth.cuh"],
        "Fused Adam / AdaGrad / Newton optimizer steps, quantized and"
        " offloaded variants.",
    ),
    "Densify.h": (
        ["kernels/densify/Densify.cuh"],
        "Densification: MCMC and long-axis-split add/relocate, edge filters,"
        " scatter utilities.",
    ),
    "Loss.h": (
        ["kernels/loss/FusedSSIM.cuh", "kernels/loss/PerPixelLoss.cuh", "kernels/loss/PerSplatLoss.cuh"],
        "Fused SSIM, multi-scale per-pixel losses, per-splat losses.",
    ),
    "Background.h": (
        ["kernels/background/BackgroundSphericalHarmonics.cuh"],
        "Background spherical-harmonics render forward/backward.",
    ),
    "Bilagrid.h": (
        ["kernels/bilagrid/BilagridUtils.cuh", "kernels/bilagrid/BilagridBindings.h"],
        "Bilateral-grid utilities and the sample/TV-loss/fused-Adam launcher"
        " family.",
    ),
    "Visualizer.h": (
        ["kernels/visualize/Visualizer.cuh"],
        "Interactive viewer render / blit entry points.",
    ),
}

PREAMBLE = """\
#pragma once
// Backend API module — GENERATED forwarder by
// tools/codegen/generate_backend_api.py. DO NOT EDIT.
//
// {desc}
//
// Authoritative declarations live in the per-kernel headers included below
// (declaration sections generated from /*[AutoHeaderGeneratorExport]*/
// markers by generate_headers.py; CUDA-include-free via BackendTypes.h).
// Implementations: the .cu files next to each header (CUDA) and
// backend/vulkan/kernels/ (SPIR-V pipeline dispatch). See backend/README.md.

"""


def write_if_changed(path, new_text):
    old = open(path).read() if os.path.exists(path) else None
    if old != new_text:
        with open(path, "w") as f:
            f.write(new_text)
        return True
    return False


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for header, (sources, desc) in MODULES.items():
        body = PREAMBLE.format(desc=desc)
        # src/ is the include root, so forward with src-relative paths.
        body += "".join('#include "%s"\n' % s for s in sources)
        changed = write_if_changed(OUT_DIR + header, body)
        print(("wrote " if changed else "up-to-date ") + OUT_DIR + header)


if __name__ == "__main__":
    main()
