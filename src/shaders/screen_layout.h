#pragma once

// Float offsets inside a packed per-splat screen row, shared verbatim by the
// C++ ScreenBuffer classes (primitives/PrimitiveBase3DGS.cuh) and the Slang
// accessors (backend/vulkan/shaders/screen.slang). Plain #defines so both
// preprocessors can read the same file.
//
// The tile cull's inputs come first in each layout so they share a sector.
// A stride's byte size must be a multiple of the widest accessor alignment:
// SCR2 rows are read as float2, so SCR2_STRIDE * 4 must be a multiple of 8.

// ---- 2D (3DGS, mip): xy, conic, opac, depth, rgb --------------------------
#define SCR2_STRIDE 10
#define SCR2_XY     0
#define SCR2_CONIC  2
#define SCR2_OPAC   5
#define SCR2_DEPTH  6
#define SCR2_RGB    7

// ---- 3DGUT: conic (in the "scale" slot), opac, rgb ------------------------
#define SCRG_STRIDE 7
#define SCRG_SCALE  0
#define SCRG_OPAC   3
#define SCRG_RGB    4
