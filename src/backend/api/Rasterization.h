#pragma once
// Backend API module — GENERATED forwarder by
// tools/codegen/generate_backend_api.py. DO NOT EDIT.
//
// Tile-based rasterization forward/backward (2D and eval-3D/3DGUT), and the stochastic viewer forward.
//
// Authoritative declarations live in the per-kernel headers included below
// (declaration sections generated from /*[AutoHeaderGeneratorExport]*/
// markers by generate_headers.py; CUDA-include-free via BackendTypes.h).
// Implementations: the .cu files next to each header (CUDA) and
// backend/vulkan/kernels/ (SPIR-V pipeline dispatch). See backend/README.md.

#include "kernels/raster/RasterizationFwd.cuh"
#include "kernels/raster/RasterizationBwd.cuh"
#include "kernels/raster/RasterizationEval3DFwd.cuh"
#include "kernels/raster/RasterizationEval3DBwd.cuh"
#include "kernels/raster/RasterizationMomentsFwd.cuh"
#include "kernels/raster/RasterizationStochastic.cuh"
