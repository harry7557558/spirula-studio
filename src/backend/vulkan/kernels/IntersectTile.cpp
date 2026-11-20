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
struct FootprintIO {
    uint64_t means, quats, scales, gaussian_ids, viewmats, intrins, tile_quads, block_quads;
    uint32_t planar, _pad0;
};

struct IsectCountParams {
    uint64_t aabb, proj_screen, tiles_per_splat;
    uint64_t image_ids, tile_active, isect_total;
    FootprintIO fp;
    uint32_t N, image_width, image_height, tile_width, tile_height, total,
        wgs_per_row, macro_log2;
    int32_t row_stride, xy_off, conic_off, opac_off;
    float max_sigma;
    int32_t band_row0, band_row1;
    uint32_t _pad0;
};
static_assert(sizeof(IsectCountParams) == 6 * 8 + 9 * 8 + 16 * 4,
              "params layout must match the slang struct");

struct IsectWriteParams {
    uint64_t image_ids, aabb, depths, proj_screen;
    uint64_t cum_tiles, isect_ids, flatten_ids, tile_active;
    FootprintIO fp;
    uint32_t N, image_width, image_height, tile_width, tile_height, total,
        wgs_per_row, n_isects, macro_log2;
    int32_t row_stride, xy_off, conic_off, opac_off;
    float max_sigma;
    int32_t band_row0, band_row1;
    uint32_t key_tile_base;
    uint32_t _pad0;
};
static_assert(sizeof(IsectWriteParams) == 8 * 8 + 9 * 8 + 18 * 4,
              "params layout must match the slang struct");

struct TileQuadsParams {
    uint64_t viewmats, intrins, dist_coeffs, out_quads;
    uint32_t tile_width, tile_height, I, wgs_per_row, macro_log2, camera_model, width, height;
};
static_assert(sizeof(TileQuadsParams) == 4 * 8 + 8 * 4,
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
    uint32_t n_isects, n_offsets, wgs_per_row, isect_base;
};
static_assert(sizeof(IsectOffsetParams) == 2 * 8 + 4 * 4,
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
    FootprintView footprint,      // .means null for the ellipse / AABB test
    const uint32_t I,
    TorchTensorView intrins,      // [I, 4]
    const uint32_t image_width,
    const uint32_t image_height,
    DeviceVector<int32_t>* image_ids, // null for non-packed
    const int32_t* tile_active,       // [I, tile_h, tile_w]; null = all live
    int& macro_log2                   // in: binning granularity; out: what it
                                      // coarsened to (core/Common.cuh)
) {
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
    const bool exact = footprint.means != nullptr;
    if (exact && !p_screen)
        throw std::runtime_error("tile intersect: exact footprints read the opacity from the screen rows");
    const vkk::CamDistSpec cam = exact
        ? vkk::cam_dist_spec(footprint.camera_model, footprint.distortion, true)
        : vkk::CamDistSpec{};
    DeviceVector<float> tile_quads;
    FootprintIO fio{};
    if (exact) {
        fio.means = (uint64_t)footprint.means;
        fio.quats = (uint64_t)footprint.quats;
        fio.scales = (uint64_t)footprint.scales;
        fio.gaussian_ids = vkk::or_fallback(footprint.gaussian_ids);
        fio.viewmats = (uint64_t)footprint.viewmats;
        fio.intrins = (uint64_t)std::get<0>(intrins);
        fio.planar = (uint32_t)footprint_planar(cmt(footprint.camera_model),
                                                cdt(footprint.distortion));
    }
    // Ray quads of the tile grid as it stands, then of its blocks. A planar view
    // needs none: its footprints are ellipses, or tested on rays it derives.
    auto build_tile_quads = [&](uint32_t tile_width, uint32_t tile_height, int macro_log2) {
        fio.tile_quads = fio.block_quads = vkk::null_fallback();
        if (fio.planar != 0) return;
        const int64_t n_tile = (int64_t)I * tile_width * tile_height;
        const int64_t n_block = (int64_t)I * _CEIL_DIV(tile_width, (uint32_t)kIsectBlock) *
                                _CEIL_DIV(tile_height, (uint32_t)kIsectBlock);
        tile_quads.resize(PoolSlot::IsectTileRays, 12 * (n_tile + n_block));
        TileQuadsParams qp{};
        qp.viewmats = (uint64_t)footprint.viewmats;
        qp.intrins = (uint64_t)std::get<0>(intrins);
        qp.dist_coeffs = vkk::or_fallback(footprint.dist_coeffs);
        qp.out_quads = (uint64_t)tile_quads.data_ptr();
        qp.tile_width = tile_width;
        qp.tile_height = tile_height;
        qp.I = I;
        qp.macro_log2 = (uint32_t)macro_log2;
        qp.camera_model = cam.cam;
        qp.width = image_width;
        qp.height = image_height;
        vkk::dispatch_flat("intersect_tile.tile_quad_rays",
                           {0u, 0u, 0u, 0u, 0u, cam.dist}, n_tile + n_block, 256,
                           &qp, sizeof(qp), &qp.wgs_per_row);
        fio.tile_quads = (uint64_t)tile_quads.data_ptr();
        fio.block_quads = (uint64_t)(tile_quads.data_ptr() + 12 * n_tile);
    };
    const vkk::Fold fold = vkk::fold_1d(total_count, 256);
    const size_t count_span = 4 * (size_t)total_count;

    uint32_t tile_width = 0, tile_height = 0, n_tiles = 0;
    const int32_t* active = tile_active;
    // Spec IDs: 0 = kEllipse, 1 = kHasXy, 2 = kPacked, 3 = kSkipTiles,
    // 4 = kFootprint (see intersect_tile.slang). Both passes must agree, so it
    // is settled by the count loop rather than by the caller's tile_active.
    backend::vk::SpecList mode_spec;
    DeviceVector<int32_t> tiles_per_splat, cum_tiles_per_splat;
    DeviceVector<uint32_t> isect_total;
    DeviceVector<int64_t> isect_ids_a, isect_ids_b;
    DeviceVector<int32_t> flatten_alt;

    // Tiles each splat touches in rows [row0, row1), and their exact total: a
    // wrapped int32 prefix sum would drive the write pass off its buffers.
    auto count_rows = [&](int32_t row0, int32_t row1) -> int64_t {
        tiles_per_splat.resize(PoolSlot::IsectTilesPerSplat, total_count);
        isect_total.resize(PoolSlot::IsectTotal, 2);
        isect_total.zero();
        IsectCountParams cp{};
        cp.aabb = (uint64_t)aabb.data_ptr();
        cp.proj_screen = p_screen;
        cp.tiles_per_splat = (uint64_t)tiles_per_splat.data_ptr();
        cp.image_ids = packed ? (uint64_t)image_ids->data_ptr() : 0;
        cp.tile_active = vkk::or_fallback(active);
        cp.isect_total = (uint64_t)isect_total.data_ptr();
        cp.fp = fio;
        cp.N = N;
        cp.image_width = image_width;
        cp.image_height = image_height;
        cp.tile_width = tile_width;
        cp.tile_height = tile_height;
        cp.total = total_count;
        cp.macro_log2 = (uint32_t)macro_log2;
        cp.wgs_per_row = fold.per_row;
        cp.row_stride = ellipse.stride;
        cp.xy_off = ellipse.xy;
        cp.conic_off = ellipse.conic;
        cp.opac_off = ellipse.opac;
        cp.max_sigma = ellipse.max_sigma;
        cp.band_row0 = row0;
        cp.band_row1 = row1;
        vkk::dispatch_ring("intersect_tile.intersect_tile_count", mode_spec,
                           fold.per_row, fold.rows, 1, &cp, sizeof(cp));
        uint32_t words[2] = {0, 0};
        if (total_count > 0)
            backend::memcpy_sync(words, isect_total.data_ptr(), sizeof(words),
                                 backend::MemcpyKind::DeviceToHost);
        return (int64_t)words[0] | ((int64_t)words[1] << 32);
    };

    /* Count tiles intersected per splat, coarsening the binning until the
       exact total fits int32. Re-counting costs one count pass (~0.3% of a
       step, and flat in tile size); the alternative is throwing. */
    int64_t n_isects64 = 0;
    int32_t n_isects = 0, rows = 0;
    int bands = 1;
    size_t isect_reserve = 0;
    for (;;) {
        // Opens the tile-intersect phase, whose buffers share the arena with
        // the raster backward (POOL_ALIAS_TABLE, core/PoolSlots.h). Re-opened
        // per attempt: an arena slot may be acquired only once per phase.
        pool_begin_phase(PoolPhase::TileIsect,
                         isect_reserve ? isect_reserve
                                       : DevicePool::arena_span({count_span}));
        tile_width = _CEIL_DIV(image_width, (uint32_t)bin_tile_x(macro_log2));
        tile_height = _CEIL_DIV(image_height, (uint32_t)bin_tile_y(macro_log2));
        n_tiles = intersect_check_tile_count(tile_width, tile_height, I);
        rows = (int32_t)(I * tile_height);

        mode_spec = backend::vk::SpecList{p_screen && !exact ? 1u : 0u,
                                          ellipse.xy >= 0 ? 1u : 0u,
                                          packed ? 1u : 0u,
                                          active ? 1u : 0u,
                                          !exact ? 0u : footprint.surfel ? 2u : 1u};
        if (exact) build_tile_quads(tile_width, tile_height, macro_log2);
        n_isects64 = count_rows(0, rows);
        if (intersect_should_coarsen(n_isects64, macro_log2)) {
            // tile_active was built for the old tile grid; dropping it only
            // costs the skipped-tile optimization for this step, and the next
            // one is built at the size this settles on.
            active = nullptr;
            isect_reserve = 0;
            continue;
        }
        intersect_check_isect_count(n_isects64);
        n_isects = (int32_t)n_isects64;
        bands = intersect_band_count(n_isects, total_count, rows);
        if (bands > 1) break;
        // Grown past the arena: reopen sized for it and count again, rather
        // than overflow into buffers beside the old arena (core/Tensor.h).
        const size_t isect_span = DevicePool::arena_span(
            {count_span, count_span, 8 * (size_t)n_isects, 8 * (size_t)n_isects,
             4 * (size_t)n_isects});
        if (!DevicePool::global().phase_reserve(isect_span) && isect_reserve == 0) {
            isect_reserve = isect_span;
            continue;
        }
        break;
    }

    DeviceTensor3D<int32_t> offsets_out;
    offsets_out.resize(PoolSlot::IsectOffsets, I, tile_height, tile_width);
    if (n_isects == 0) {
        offsets_out.zero();
        return std::make_tuple(DeviceVector<int64_t>{},
                               DeviceVector<int32_t>{}, offsets_out);
    }
    // The one list the raster keeps; every band sorts into its own stretch.
    DeviceVector<int32_t> flatten_ids;
    flatten_ids.resize(PoolSlot::IsectFlatA, n_isects);

    // Keys, sort and offsets of the pairs tiles_per_splat holds: rows [row0,
    // row1), whose tiles the keys count from tile0, into flatten_ids[base..].
    auto sort_rows = [&](int32_t row0, int32_t row1, uint32_t tile0,
                         uint32_t n_offsets, int32_t base, int32_t n) {
        cum_tiles_per_splat.resize(PoolSlot::IsectCumTiles, total_count);
        backend::inclusive_sum<int32_t>(tiles_per_splat.data_ptr(),
                                        cum_tiles_per_splat.data_ptr(),
                                        total_count);
        isect_ids_a.resize(PoolSlot::IsectIdsA, n);
        isect_ids_b.resize(PoolSlot::IsectIdsB, n);
        flatten_alt.resize(PoolSlot::IsectFlatB, n);

        int tile_n_bits = 0;
        while ((1U << tile_n_bits) <= n_offsets) ++tile_n_bits;
        const int end_bit = 32 + tile_n_bits;
        // One pass per 8 bits, each swapping the buffers: start in the one
        // that leaves the result in flatten_ids (backend/vulkan/SortScanVulkan.cpp).
        int32_t* const out = flatten_ids.data_ptr() + base;
        const bool odd = ((end_bit + 7) / 8) & 1;
        backend::DoubleBuffer<int64_t> d_keys(isect_ids_a.data_ptr(),
                                              isect_ids_b.data_ptr());
        backend::DoubleBuffer<int32_t> d_values(
            odd ? flatten_alt.data_ptr() : out, odd ? out : flatten_alt.data_ptr());

        IsectWriteParams wp{};
        wp.image_ids = packed ? (uint64_t)image_ids->data_ptr() : 0;
        wp.aabb = (uint64_t)aabb.data_ptr();
        wp.depths = (uint64_t)depths.data_ptr();
        wp.proj_screen = p_screen;
        wp.cum_tiles = (uint64_t)cum_tiles_per_splat.data_ptr();
        wp.isect_ids = (uint64_t)d_keys.current();
        wp.flatten_ids = (uint64_t)d_values.current();
        wp.tile_active = vkk::or_fallback(active);
        wp.fp = fio;
        wp.n_isects = (uint32_t)n;
        wp.N = N;
        wp.image_width = image_width;
        wp.image_height = image_height;
        wp.tile_width = tile_width;
        wp.tile_height = tile_height;
        wp.total = total_count;
        wp.macro_log2 = (uint32_t)macro_log2;
        wp.wgs_per_row = fold.per_row;
        wp.row_stride = ellipse.stride;
        wp.xy_off = ellipse.xy;
        wp.conic_off = ellipse.conic;
        wp.opac_off = ellipse.opac;
        wp.max_sigma = ellipse.max_sigma;
        wp.band_row0 = row0;
        wp.band_row1 = row1;
        wp.key_tile_base = tile0;
        vkk::dispatch_ring("intersect_tile.intersect_tile_write", mode_spec,
                           fold.per_row, fold.rows, 1, &wp, sizeof(wp));

        /* Sort by (tile_id << 32 | depth) key */
        backend::sort_pairs(d_keys, d_values, n, 0, end_bit);
        if (d_values.current() != out)
            throw std::runtime_error("tile intersect: sort left its result in the scratch buffer");

        /* Compute per-tile start offsets */
        vkk::Fold g = vkk::fold_1d(n, 256);
        IsectOffsetParams op{};
        op.isect_ids = (uint64_t)d_keys.current();
        op.offsets = (uint64_t)(offsets_out.data_ptr() + tile0);
        op.n_isects = (uint32_t)n;
        op.n_offsets = n_offsets;
        op.wgs_per_row = g.per_row;
        op.isect_base = (uint32_t)base;
        vkk::dispatch("intersect_tile.intersect_offset", {}, g.per_row, g.rows,
                      1, &op, sizeof(op));
    };

    if (bands == 1) {
        sort_rows(0, rows, 0, n_tiles, 0, n_isects);
        return std::make_tuple(DeviceVector<int64_t>{}, flatten_ids, offsets_out);
    }

    /* One band of rows at a time: count, sort into its stretch of the list. */
    const int64_t share = ((int64_t)n_isects + bands - 1) / bands;
    size_t reserve = DevicePool::arena_span(
        {count_span, count_span, 8 * (size_t)share, 8 * (size_t)share, 4 * (size_t)share});
    int32_t row = 0, base = 0;
    uint32_t tile0 = 0;
    double density = (double)n_isects / (double)rows;
    while (row < rows) {
        int32_t take = intersect_band_rows(share, density, rows - row);
        int64_t n = 0;
        for (bool reserved = false;;) {
            pool_begin_phase(PoolPhase::TileIsect, reserve);
            n = count_rows(row, row + take);
            // Rows are not equally full: a band well past its share is halved.
            if ((double)n > kIsectBandSlack * (double)share && take > 1) {
                take = (take + 1) / 2;
                continue;
            }
            const size_t span = DevicePool::arena_span(
                {count_span, count_span, 8 * (size_t)n, 8 * (size_t)n, 4 * (size_t)n});
            if (!DevicePool::global().phase_reserve(span) && !reserved) {
                reserve = span;
                reserved = true;
                continue;
            }
            break;
        }
        if ((int64_t)base + n > (int64_t)n_isects)
            throw std::runtime_error("tile intersect: the bands count more pairs than the image");
        const bool last = row + take >= rows;
        const uint32_t n_offsets =
            (last ? n_tiles : (uint32_t)(row + take) * tile_width) - tile0;
        if (n > 0) {
            sort_rows(row, row + take, tile0, n_offsets, base, (int32_t)n);
        } else if (last) {
            IsectOffsetParams op{};
            // Not read with no pairs, but a load llvmpipe may still speculate.
            op.isect_ids = vkk::null_fallback();
            op.offsets = (uint64_t)(offsets_out.data_ptr() + tile0);
            op.n_offsets = n_offsets;
            op.wgs_per_row = 1;
            op.isect_base = (uint32_t)base;
            vkk::dispatch("intersect_tile.intersect_offset", {}, 1, 1, 1, &op,
                          sizeof(op));
        }
        // An empty band's tiles wait for the next one, whose keys reach back.
        if (n > 0) tile0 += n_offsets;
        base += (int32_t)n;
        row += take;
        density = (double)n / (double)take;
    }
    return std::make_tuple(DeviceVector<int64_t>{}, flatten_ids, offsets_out);
}
