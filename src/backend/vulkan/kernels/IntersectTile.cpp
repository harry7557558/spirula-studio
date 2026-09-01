// Vulkan implementation of the tile-intersection launch API
// (kernels/tile/IntersectTile.cuh). Mirrors do_intersect_tile_generic in
// IntersectTile.cu with the CUB spine swapped for backend:: SortScan:
// count -> inclusive_sum<int32> -> n_isects readback -> key write ->
// sort_pairs<int64,int32> (begin_bit 0, end_bit 32 + tile bits) -> offsets.
// Device work runs shaders/intersect_tile.slang.

#include <kernels/tile/IntersectTile.cuh>

#include "backend/common/SortScan.h"
#include "backend/vulkan/kernels/KernelCommon.h"

namespace {


// Mirror the params structs in shaders/intersect_tile.slang.
struct IsectCountParams {
    uint64_t aabb, proj_screen, tiles_per_splat;
    uint64_t image_ids, tile_active, isect_total;
    uint32_t N, image_width, image_height, tile_width, tile_height, total,
        wgs_per_row, macro_log2;
    int32_t row_stride, xy_off, conic_off, opac_off;
};
static_assert(sizeof(IsectCountParams) == 6 * 8 + 12 * 4,
              "params layout must match the slang struct");

struct IsectWriteParams {
    uint64_t image_ids, aabb, depths, proj_screen;
    uint64_t cum_tiles, isect_ids, flatten_ids, tile_active;
    uint32_t N, image_width, image_height, tile_width, tile_height, total,
        wgs_per_row, n_isects, macro_log2;
    int32_t row_stride, xy_off, conic_off, opac_off;
};
static_assert(sizeof(IsectWriteParams) == 8 * 8 + 13 * 4 + 4 /*pad*/,
              "params layout must match the slang struct");

struct TileActiveParams {
    uint64_t mask, tile_active;
    int32_t H_mask, W_mask, width, height;
    uint32_t tile_width, tile_height, total, wgs_per_row, macro_log2;
    int32_t row_stride, xy_off, conic_off, opac_off;
};
static_assert(sizeof(TileActiveParams) == 2 * 8 + 13 * 4 + 4 /*pad*/,
              "params layout must match the slang struct");

struct IsectOffsetParams {
    uint64_t isect_ids, offsets;
    uint32_t n_isects, n_offsets, wgs_per_row;
};
static_assert(sizeof(IsectOffsetParams) == 2 * 8 + 3 * 4 + 4 /*pad*/,
              "params layout must match the slang struct");

}  // namespace

/* API definition matching kernels/tile/IntersectTile.cuh */

int64_t intersect_tile_count(int width, int height, int macro_log2) {
    return (int64_t)_CEIL_DIV((uint32_t)width, bin_tile_x(macro_log2)) *
           (int64_t)_CEIL_DIV((uint32_t)height, bin_tile_y(macro_log2));
}

void compute_tile_active(
    TorchTensorView mask, int I, int width, int height, int macro_log2,
    int32_t* tile_active
) {
    const auto& ms = std::get<2>(mask);
    TileActiveParams p{};
    p.mask = vkk::or_fallback((const void*)std::get<0>(mask));
    p.tile_active = (uint64_t)tile_active;
    p.H_mask = (int32_t)ms[1];
    p.W_mask = (int32_t)ms[2];
    p.width = width;
    p.height = height;
    p.tile_width = _CEIL_DIV((uint32_t)width, bin_tile_x(macro_log2));
    p.tile_height = _CEIL_DIV((uint32_t)height, bin_tile_y(macro_log2));
    p.macro_log2 = (uint32_t)macro_log2;
    p.total = (uint32_t)I * p.tile_width * p.tile_height;
    vkk::dispatch_flat("intersect_tile.tile_active_map", {}, p.total, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

std::tuple<
    DeviceVector<int64_t>,    // isect_ids [n_isects]
    DeviceVector<int32_t>,    // flatten_ids [n_isects]
    DeviceTensor3D<int32_t>   // offsets [I, tile_h, tile_w]
> do_intersect_tile_generic(
    DeviceTensor2D<uint2> aabb,   // [*N] packed, core/AabbQuant.cuh
    DeviceTensorFloatND depths,   // [*N] float32
    ProjEllipseView ellipse,      // .data null for AABB mode
    const uint32_t I,
    TorchTensorView intrins,      // [I, 4] (unused by the kernels, as in CUDA)
    const uint32_t image_width,
    const uint32_t image_height,
    DeviceVector<int32_t>* image_ids, // null for non-packed
    const int32_t* tile_active,       // [I, tile_h, tile_w]; null = all live
    int& macro_log2                   // in: binning granularity; out: what it
                                      // coarsened to (core/Common.cuh)
) {
    (void)intrins;
    bool packed = image_ids != nullptr;
    // depths is always [*N] float32 (numel = N or nnz), while aabb is [*N, 4].
    const int64_t total_count64 = depths.numel();
    if (total_count64 > 0x7fffffffLL)
        throw std::runtime_error(
            "tile intersect: " + std::to_string(total_count64) +
            " (camera, gaussian) pairs in one batch exceeds the 2^31 limit");
    const uint32_t total_count = (uint32_t)total_count64;
    const uint32_t N = packed ? total_count : total_count / I;

    intersect_check_image_size(image_width, image_height);

    const uint64_t p_screen = (uint64_t)ellipse.data;

    /* Count tiles intersected per splat, coarsening the binning until the
       exact total fits int32. Re-counting costs one count pass (~0.3% of a
       step, and flat in tile size); the alternative is throwing. */
    uint32_t tile_width = 0, tile_height = 0, n_tiles = 0;
    int64_t n_isects64 = 0;
    const int32_t* active = tile_active;
    int32_t n_isects = 0;
    DeviceTensor3D<int32_t> offsets_out;
    DeviceVector<int32_t> cum_tiles_per_splat;
    DeviceVector<int64_t> isect_ids_a, isect_ids_b;
    DeviceVector<int32_t> flatten_ids_a, flatten_ids_b;
    // Spec IDs: 0 = kEllipse, 1 = kHasXy, 2 = kPacked, 3 = kSkipTiles
    // (see intersect_tile.slang). Both passes must agree, so it is settled
    // by the loop below rather than by the caller's tile_active.
    backend::vk::SpecList mode_spec;
    DeviceVector<int32_t> tiles_per_splat;
    DeviceVector<uint32_t> isect_total;
    for (;;) {
        // Opens the tile-intersect phase, whose buffers share the arena with
        // the raster backward (POOL_ALIAS_TABLE, core/PoolSlots.h). Re-opened
        // per attempt: an arena slot may be acquired only once per phase.
        pool_begin_phase(PoolPhase::TileIsect);
        tile_width = _CEIL_DIV(image_width, (uint32_t)bin_tile_x(macro_log2));
        tile_height = _CEIL_DIV(image_height, (uint32_t)bin_tile_y(macro_log2));
        n_tiles = intersect_check_tile_count(tile_width, tile_height, I);

        mode_spec = backend::vk::SpecList{p_screen ? 1u : 0u,
                                          ellipse.xy >= 0 ? 1u : 0u,
                                          packed ? 1u : 0u,
                                          active ? 1u : 0u};

        tiles_per_splat.resize(PoolSlot::IsectTilesPerSplat, total_count);
        isect_total.resize(PoolSlot::IsectTotal, 2);
        isect_total.zero();
        {
            vkk::Fold g = vkk::fold_1d(total_count, 256);
            IsectCountParams cp{};
            cp.aabb = (uint64_t)aabb.data_ptr();
            cp.proj_screen = p_screen;
            cp.tiles_per_splat = (uint64_t)tiles_per_splat.data_ptr();
            cp.image_ids = packed ? (uint64_t)image_ids->data_ptr() : 0;
            cp.tile_active = vkk::or_fallback(active);
            cp.isect_total = (uint64_t)isect_total.data_ptr();
            cp.N = N;
            cp.image_width = image_width;
            cp.image_height = image_height;
            cp.tile_width = tile_width;
            cp.tile_height = tile_height;
            cp.total = total_count;
            cp.macro_log2 = (uint32_t)macro_log2;
            cp.wgs_per_row = g.per_row;
            cp.row_stride = ellipse.stride;
            cp.xy_off = ellipse.xy;
            cp.conic_off = ellipse.conic;
            cp.opac_off = ellipse.opac;
            vkk::dispatch("intersect_tile.intersect_tile_count", mode_spec,
                          g.per_row, g.rows, 1, &cp, sizeof(cp));
        }

        /* Exact total, checked before the int32 scan runs over it: a wrapped
           prefix sum would drive the write pass off the end of buffers sized
           from it. */
        uint32_t total_words[2] = {0, 0};
        if (total_count > 0)
            backend::memcpy_sync(total_words, isect_total.data_ptr(),
                                 sizeof(total_words),
                                 backend::MemcpyKind::DeviceToHost);
        n_isects64 =
            (int64_t)total_words[0] | ((int64_t)total_words[1] << 32);
        if (intersect_should_coarsen(n_isects64, macro_log2)) {
            // tile_active was built for the old tile grid; dropping it only
            // costs the skipped-tile optimization for this step, and the next
            // one is built at the size this settles on.
            active = nullptr;
            continue;
        }
        intersect_check_isect_count(n_isects64);
        n_isects = (int32_t)n_isects64;

        /* Inclusive prefix sum -> cumulative tile counts */
        cum_tiles_per_splat.resize(PoolSlot::IsectCumTiles, total_count);
        backend::inclusive_sum<int32_t>(tiles_per_splat.data_ptr(),
                                        cum_tiles_per_splat.data_ptr(),
                                        total_count);
        offsets_out.resize(PoolSlot::IsectOffsets, I, tile_height, tile_width);
        if (n_isects != 0) {
            isect_ids_a.resize(PoolSlot::IsectIdsA, n_isects);
            isect_ids_b.resize(PoolSlot::IsectIdsB, n_isects);
            flatten_ids_a.resize(PoolSlot::IsectFlatA, n_isects);
            flatten_ids_b.resize(PoolSlot::IsectFlatB, n_isects);
        }
        break;
    }

    if (n_isects == 0) {
        offsets_out.zero();
        return std::make_tuple(DeviceVector<int64_t>{},
                               DeviceVector<int32_t>{}, offsets_out);
    }

    {
        vkk::Fold g = vkk::fold_1d(total_count, 256);
        IsectWriteParams wp{};
        wp.image_ids = packed ? (uint64_t)image_ids->data_ptr() : 0;
        wp.aabb = (uint64_t)aabb.data_ptr();
        wp.depths = (uint64_t)depths.data_ptr();
        wp.proj_screen = p_screen;
        wp.cum_tiles = (uint64_t)cum_tiles_per_splat.data_ptr();
        wp.isect_ids = (uint64_t)isect_ids_a.data_ptr();
        wp.flatten_ids = (uint64_t)flatten_ids_a.data_ptr();
        wp.tile_active = vkk::or_fallback(active);
        wp.n_isects = (uint32_t)n_isects;
        wp.N = N;
        wp.image_width = image_width;
        wp.image_height = image_height;
        wp.tile_width = tile_width;
        wp.tile_height = tile_height;
        wp.total = total_count;
        wp.macro_log2 = (uint32_t)macro_log2;
        wp.wgs_per_row = g.per_row;
        wp.row_stride = ellipse.stride;
        wp.xy_off = ellipse.xy;
        wp.conic_off = ellipse.conic;
        wp.opac_off = ellipse.opac;
        vkk::dispatch("intersect_tile.intersect_tile_write", mode_spec, g.per_row,
                      g.rows, 1, &wp, sizeof(wp));
    }

    /* Sort by (tile_id << 32 | depth) key */
    backend::DoubleBuffer<int64_t> d_keys(isect_ids_a.data_ptr(),
                                          isect_ids_b.data_ptr());
    backend::DoubleBuffer<int32_t> d_values(flatten_ids_a.data_ptr(),
                                            flatten_ids_b.data_ptr());
    int tile_n_bits = 0;
    while ((1U << tile_n_bits) <= n_tiles) ++tile_n_bits;
    backend::sort_pairs(d_keys, d_values, n_isects, 0, 32 + tile_n_bits);

    /* Pick whichever buffer the sort left the result in */
    DeviceVector<int64_t> isect_ids_out =
        d_keys.selector ? isect_ids_b : isect_ids_a;
    DeviceVector<int32_t> flatten_ids_out =
        d_values.selector ? flatten_ids_b : flatten_ids_a;

    /* Compute per-tile start offsets */
    {
        vkk::Fold g = vkk::fold_1d(n_isects, 256);
        IsectOffsetParams op{};
        op.isect_ids = (uint64_t)isect_ids_out.data_ptr();
        op.offsets = (uint64_t)offsets_out.data_ptr();
        op.n_isects = (uint32_t)n_isects;
        op.n_offsets = n_tiles;
        op.wgs_per_row = g.per_row;
        vkk::dispatch("intersect_tile.intersect_offset", {}, g.per_row,
                      g.rows, 1, &op, sizeof(op));
    }

    return std::make_tuple(isect_ids_out, flatten_ids_out, offsets_out);
}
