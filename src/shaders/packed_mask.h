#pragma once

// Geometry of the packed-projection visibility bitmask, shared verbatim by the
// CUDA kernels (kernels/projection/ProjectionPackedFwd_kernel.cuh) and the
// Slang ones (backend/vulkan/shaders/projection_fwd.slang). Plain #defines so
// both preprocessors can read the same file.
//
// The mask pass stores one BIT per (camera, gaussian) pair plus one popcount
// per workgroup; the compaction pass rebuilds an element's output index from
// the scanned workgroup counts and the bits below it. Both passes must launch
// with SS_PMASK_BLOCK threads or the bit-to-word mapping diverges.

#define SS_PMASK_BLOCK 128
#define SS_PMASK_WORDS 4  // SS_PMASK_BLOCK / 32
