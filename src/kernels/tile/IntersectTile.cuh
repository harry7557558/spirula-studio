#pragma once

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <core/Common.cuh>
#include <core/Tensor.h>

#include "primitives/Primitive3DGS.cuh"
#include "primitives/Primitive3DGUT.cuh"
#include "shaders/screen_layout.h"


// Ellipse-mode inputs, read out of a packed screen row. data == nullptr
// selects the conservative AABB test instead.
struct ProjEllipseView {
    const float* data = nullptr;
    int32_t stride = 0;
    int32_t xy = -1;     // -1: ellipse center is the AABB center (3DGUT)
    int32_t conic = 0;
    int32_t opac = 0;
};

// The two screen layouts as ellipse views. 3DGUT keeps its projected conic in
// the row's "scale" slot and stores no center.
inline ProjEllipseView proj_ellipse_view(const float* rows, bool eval3d) {
    if (eval3d)
        return {rows, SCRG_STRIDE, -1, SCRG_SCALE, SCRG_OPAC};
    return {rows, SCR2_STRIDE, SCR2_XY, SCR2_CONIC, SCR2_OPAC};
}

// Pixel coordinates and every tile index derived from them ride in 16 bits.
inline void intersect_check_image_size(uint32_t width, uint32_t height) {
    if (width > 32767u || height > 32767u)
        throw std::runtime_error(
            "tile intersect: image " + std::to_string(width) + "x" +
            std::to_string(height) + " exceeds the 32767-pixel limit");
}

// The sort key is (tile id << 32 | depth), so the batch's whole tile grid has
// to be addressable in the 32 bits above the depth.
inline uint32_t intersect_check_tile_count(uint32_t tile_width,
                                           uint32_t tile_height, uint32_t I) {
    const uint64_t n = (uint64_t)tile_width * tile_height * I;
    if (n > 0xffffffffull)
        throw std::runtime_error(
            "tile intersect: " + std::to_string(n) +
            " tiles in one batch exceeds the 2^32 tile-id limit");
    return (uint32_t)n;
}

// flatten_ids and the per-tile offsets are int32, so a run must stay under
// 2^31 splat-tile pairs. The count pass accumulates the exact 64-bit total
// (two words with a carry), because an int32 sum wraps before it can be read.
inline void intersect_check_isect_count(int64_t n_isects) {
    if (n_isects < 0 || n_isects > 0x7fffffffLL)
        throw std::runtime_error(
            "tile intersect: " + std::to_string(n_isects) +
            " splat-tile pairs in one batch exceeds the 2^31 limit; "
            "lower --cap-max, the sub-batch size, or the image resolution");
}


// True when the pair total overflows int32 and the binning can still coarsen;
// advances macro_log2 so the caller re-counts. A 100M-splat 7680^2 batch makes
// 4.5e9 pairs at 16 px binning and 0.58e9 at 64 px, so this beats throwing.
inline bool intersect_should_coarsen(int64_t n_isects, int& macro_log2) {
    if (n_isects >= 0 && n_isects <= 0x7fffffffLL) return false;
    if (macro_log2 >= kMacroLog2Max) return false;
    ++macro_log2;
    static int reported = -1;
    if (reported != macro_log2) {
        reported = macro_log2;
        std::fprintf(stderr,
                     "[bin-tile] %lld splat-tile pairs exceed the 2^31 limit; "
                     "binning at %d px\n",
                     (long long)n_isects, bin_tile_x(macro_log2));
    }
    return true;
}


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



int64_t intersect_tile_count(int width, int height, int macro_log2);


void compute_tile_active(
    TorchTensorView mask,   // [I, H_mask, W_mask] bool
    int I, int width, int height,
    int macro_log2,         // binning granularity, core/Common.cuh
    int32_t* tile_active    // [I, tile_h, tile_w]
);


std::tuple<
    DeviceVector<int64_t>,    // isect_ids [n_isects]
    DeviceVector<int32_t>,    // flatten_ids [n_isects]
    DeviceTensor3D<int32_t>   // offsets [I, tile_h, tile_w]
> do_intersect_tile_generic(
    DeviceTensor2D<uint2> aabb,   // [*N] packed, core/AabbQuant.cuh
    DeviceTensorFloatND depths,   // [*N] float32
    ProjEllipseView ellipse,      // .data null for AABB mode
    const uint32_t I,
    TorchTensorView intrins,      // [I, 4]
    const uint32_t image_width,
    const uint32_t image_height,
    DeviceVector<int32_t>* image_ids, // null for non-packed
    const int32_t* tile_active,       // [I, tile_h, tile_w]; null = all live
    int& macro_log2                   // in: binning granularity; out: what it
                                      // coarsened to (core/Common.cuh)
);
