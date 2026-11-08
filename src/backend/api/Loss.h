#pragma once
// Backend API module — GENERATED forwarder by
// tools/codegen/generate_backend_api.py. DO NOT EDIT.
//
// Fused SSIM, multi-scale per-pixel losses, per-splat losses, the local-PCA neighbourhood regularizer.
//
// Authoritative declarations live in the per-kernel headers included below
// (declaration sections generated from /*[AutoHeaderGeneratorExport]*/
// markers by generate_headers.py; CUDA-include-free via BackendTypes.h).
// Implementations: the .cu files next to each header (CUDA) and
// backend/vulkan/kernels/ (SPIR-V pipeline dispatch). See backend/README.md.

#include "kernels/loss/FusedSSIM.cuh"
#include "kernels/loss/PerPixelLoss.cuh"
#include "kernels/loss/PerSplatLoss.cuh"
#include "kernels/loss/PcaReg.cuh"
