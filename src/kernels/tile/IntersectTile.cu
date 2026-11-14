#include "kernels/tile/IntersectTile.cuh"

#include "generated/slang.cuh"
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
#include "generated/projection_utils.cuh"
}

#include "core/AabbQuant.cuh"
#include "core/Common.cuh"
#include "core/Env.h"
#include "core/Interpolation.cuh"

#include <cooperative_groups.h>

#include <cub/cub.cuh>
#include <thrust/iterator/transform_iterator.h>


namespace cg = cooperative_groups;


// Binning tile geometry from macro_log2. The reciprocals are exact -- the
// edge is always a power of two -- so `* inv_x` is bit-for-bit the divide it
// replaced. Kept identical to intersect_tile.slang's BinTile.
struct BinTile {
    int ix, iy;
    float x, y;
    float inv_x, inv_y;
};

inline BinTile bin_tile(int macro_log2) {
    BinTile t;
    t.ix = bin_tile_x(macro_log2);
    t.iy = bin_tile_y(macro_log2);
    t.x = (float)t.ix;
    t.y = (float)t.iy;
    t.inv_x = 1.0f / t.x;
    t.inv_y = 1.0f / t.y;
    return t;
}


inline constexpr float kTransmitThreshold = 1e-4f;


// https://github.com/harry7557558/vksplat/blob/main/vksplat/shaders/utils.slang

template<typename T>
inline __device__ T clamp(T x, T a, T b)
    { return x < a ? a : x > b ? b : x; }

inline __device__ float2 ellipse_range_bound(
    float3 inv_cov, float y0, float y1
) {
    // find x_min and x_max of an ellipse clipped to y0 < y <= y1 
    // ellipse: centered at origin, defined by inv_cov and r=1

    float a = inv_cov.x, b = inv_cov.y, c = inv_cov.z;
    float ym = -b/c * sqrtf(c/(a*c-b*b));  // can be pre-computed

    float v0 = clamp(-ym, y0, y1);
    float v1 = clamp(ym, y0, y1);

    float bv0 = -b*v0, bv1 = -b*v1;

    float inv_a = 1.0f / a;
    float x0 = inv_a * (bv0 - sqrtf(bv0*bv0 - a*(c*v0*v0-1.f)));
    float x1 = inv_a * (bv1 + sqrtf(bv1*bv1 - a*(c*v1*v1-1.f)));

    return {x0, x1};
}

inline __device__ int count_ellipse_grid_overlaps(
    float2 xy,
    float3 inv_cov,
    int grid_xmin, int grid_xmax,
    int grid_ymin, int grid_ymax,
    bool by_rows,                       // sweep rows, else columns
    const int32_t* __restrict__ live,   // this image's tiles, or null
    uint32_t tile_width, BinTile t
) {
    // count the number grid cells that overlap with an ellipse

    int n_tiles = 0;

    if (by_rows) {
        for (int y = grid_ymin; y < grid_ymax; y++) {
            float y0 = y * t.y - xy.y;
            float y1 = y0 + t.y;
            float2 bound = ellipse_range_bound(inv_cov, y0, y1);
            int x0 = int(floor((bound.x + xy.x) * t.inv_x));
            int x1 = int(ceil((bound.y + xy.x) * t.inv_x));
            x0 = clamp(x0, grid_xmin, grid_xmax);
            x1 = clamp(x1, grid_xmin, grid_xmax);
            if (live == nullptr)
                n_tiles += max(x1-x0, 0);
            else
                for (int x = x0; x < x1; x++)
                    n_tiles += live[(int64_t)y * tile_width + x] ? 1 : 0;
        }
    } else {
        inv_cov = {inv_cov.z, inv_cov.y, inv_cov.x};
        for (int x = grid_xmin; x < grid_xmax; x++) {
            float x0 = x * t.x - xy.x;
            float x1 = x0 + t.x;
            float2 bound = ellipse_range_bound(inv_cov, x0, x1);
            int y0 = int(floor((bound.x + xy.y) * t.inv_y));
            int y1 = int(ceil((bound.y + xy.y) * t.inv_y));
            y0 = clamp(y0, grid_ymin, grid_ymax);
            y1 = clamp(y1, grid_ymin, grid_ymax);
            if (live == nullptr)
                n_tiles += max(y1-y0, 0);
            else
                for (int y = y0; y < y1; y++)
                    n_tiles += live[(int64_t)y * tile_width + x] ? 1 : 0;
        }
    }

    return n_tiles;
}


// DeviceReduce::Sum accumulates in the input type, so widen the int32 counts on
// the way in. thrust's iterator, not cub::TransformInputIterator: CCCL 3
// (CUDA 13) removed it.
struct WidenToI64 {
    __host__ __device__ int64_t operator()(int32_t v) const {
        return (int64_t)v;
    }
};

template<bool is_counting_pass, bool is_ellipse>
__global__ void intersect_tile_kernel(
    const uint32_t I,  // or 1 in packed mode
    const uint32_t N,  // or nnz in packed mode
    const int32_t *__restrict__ image_ids,  // [nnz], packed mode only
    const float4* __restrict__ intrins,
    const uint2 *__restrict__ aabb_buffer,  // [..., N] packed, core/AabbQuant.cuh
    const uint32_t image_width,
    const uint32_t image_height,
    const float *__restrict__ depths_buffer,  // [..., N]
    const ProjEllipseView ellipse,  // packed screen rows; used iff is_ellipse
    const int32_t *__restrict__ cum_tiles_per_splat, // [..., N], optional for counting pass
    const uint32_t tile_width,
    const uint32_t tile_height,
    // [I, tile_height, tile_width]: 0 marks a tile the loss never reads, which
    // both passes leave out so the raster gets an empty range for it.
    const int32_t *__restrict__ tile_active,
    int32_t *__restrict__ tiles_per_splat, // [..., N]
    int64_t *__restrict__ isect_ids,  // [n_isects]
    int32_t *__restrict__ flatten_ids,  // [n_isects]
    const int32_t n_isects,  // write pass: clamps the window below
    const BinTile bt,
    // Rows [band_row0, band_row1) of the I * tile_height the images stack to,
    // and the tile the write pass's keys count from.
    const int32_t band_row0,
    const int32_t band_row1,
    const int64_t key_tile_base
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= I * N) {
        return;
    }

    const uint2 aabb_q = aabb_buffer[idx];
    float4 aabb = aabb16_decode(aabb_q, image_width, image_height);
    float xmin = aabb.x, ymin = aabb.y;
    float xmax = aabb.z, ymax = aabb.w;

    if (aabb16_is_empty(aabb_q)) {
        if (is_counting_pass) {
            tiles_per_splat[idx] = 0;
        } else {
            // cur_idx should equal to max_idx
            // do this just in case compiler floating point optimization goes wild
            int64_t iid = (image_ids ? image_ids[idx] : idx / N);
            int32_t cur_idx =
                (idx == 0) ? 0 : max(cum_tiles_per_splat[idx - 1], 0);
            int32_t max_idx = min(cum_tiles_per_splat[idx], n_isects);
            while (cur_idx < max_idx) {
                int64_t tile_id =
                    max(iid * tile_width * tile_height, key_tile_base) - key_tile_base;
                isect_ids[cur_idx] = (tile_id << 32) | (int64_t)0xffffffff;
                flatten_ids[cur_idx] = static_cast<int32_t>(idx);
                ++cur_idx;
            }
        }
        return;
    }

    [[maybe_unused]] float2 xy{};
    [[maybe_unused]] float3 conic{};
    if constexpr (is_ellipse) {
        const float* row = ellipse.data + (int64_t)ellipse.stride * idx;
        xy = ellipse.xy >= 0
                 ? float2{row[ellipse.xy], row[ellipse.xy + 1]}
                 : float2{0.5f * (xmin + xmax), 0.5f * (ymin + ymax)};
        conic = float3{row[ellipse.conic], row[ellipse.conic + 1],
                       row[ellipse.conic + 2]};
        float reach = __logf(row[ellipse.opac] / ALPHA_THRESHOLD);
        if (ellipse.max_sigma > 0.0f) reach = fminf(reach, ellipse.max_sigma);
        conic = conic * (0.5f / reach);
    }

    uint2 tile_min, tile_max;
    tile_min.x = (uint32_t)min(max(0, (int)floorf((xmin + 0.5f) * bt.inv_x)), (int)tile_width);
    tile_min.y = (uint32_t)min(max(0, (int)floorf((ymin + 0.5f) * bt.inv_y)), (int)tile_height);
    tile_max.x = (uint32_t)min(max(0, (int)ceilf((xmax + 0.5f) * bt.inv_x)), (int)tile_width);
    tile_max.y = (uint32_t)min(max(0, (int)ceilf((ymax + 0.5f) * bt.inv_y)), (int)tile_height);
    const int64_t iid_c = (image_ids ? image_ids[idx] : idx / N);
    // The sweep direction is chosen on the whole box and the rows cut to the
    // band after, so a banded splat keeps the tiles the whole image gives it.
    const bool by_rows =
        (tile_max.y-tile_min.y)*bt.ix <= (tile_max.x-tile_min.x)*bt.iy;
    {
        const int image_row = (int)iid_c * (int)tile_height;
        const int y0 = clamp(band_row0 - image_row, (int)tile_min.y, (int)tile_max.y);
        const int y1 = clamp(band_row1 - image_row, y0, (int)tile_max.y);
        tile_min.y = (uint32_t)y0;
        tile_max.y = (uint32_t)y1;
    }
    const int32_t* live = tile_active
        ? tile_active + iid_c * (int64_t)tile_width * tile_height : nullptr;
    if constexpr (is_counting_pass) {
        // counting pass only writes out tiles_per_splat
        if constexpr (is_ellipse)
            tiles_per_splat[idx] = count_ellipse_grid_overlaps(
                xy, conic,
                tile_min.x, tile_max.x, tile_min.y, tile_max.y,
                by_rows, live, tile_width, bt
            );
        else if (tile_active == nullptr)
            tiles_per_splat[idx] = max(static_cast<int32_t>(
                (tile_max.y - tile_min.y) * (tile_max.x - tile_min.x)
            ), 0);
        else {
            int n = 0;
            for (uint32_t i = tile_min.y; i < tile_max.y; ++i)
                for (uint32_t j = tile_min.x; j < tile_max.x; ++j)
                    n += live[(int64_t)i * tile_width + j] ? 1 : 0;
            tiles_per_splat[idx] = n;
        }
        return;
    }

    int64_t iid = iid_c;

    float depth_f32 = depths_buffer[idx];
    depth_f32 = fabsf(depth_f32);
    uint32_t depth_u32 = __float_as_uint(depth_f32);
    if (depth_u32 >> 31)  // negative
        depth_u32 = ~depth_u32;
    else  // positive
        depth_u32 ^= (1u << 31u);
    
    // Clamped both ends: the count is exact now, but an out-of-range window
    // here would scatter raw stores across device memory.
    int32_t cur_idx = (idx == 0) ? 0 : max(cum_tiles_per_splat[idx - 1], 0);
    int32_t max_idx = min(cum_tiles_per_splat[idx], n_isects);
    if constexpr (is_ellipse) {
        if (by_rows) {
            for (int y = tile_min.y; y < tile_max.y; y++) {
                float y0 = y * bt.y - xy.y;
                float y1 = y0 + bt.y;

                float2 bound = ellipse_range_bound(conic, y0, y1);
                int min_x = clamp(
                    (unsigned)floor((bound.x + xy.x) * bt.inv_x),
                    tile_min.x, tile_max.x
                );
                int max_x = clamp(
                    (unsigned)ceil((bound.y + xy.x) * bt.inv_x),
                    tile_min.x, tile_max.x
                );
                for (int x = min_x; x < max_x && cur_idx < max_idx; x++) {
                    if (live && !live[(int64_t)y * tile_width + x]) continue;
                    int64_t tile_id = iid * tile_width * tile_height + int64_t(y) * tile_width + int64_t(x);
                    isect_ids[cur_idx] = ((tile_id - key_tile_base) << 32) | (int64_t)depth_u32;
                    flatten_ids[cur_idx] = static_cast<int32_t>(idx);
                    ++cur_idx;
                }
            }
        } else {
            conic = {conic.z, conic.y, conic.x};
            for (int x = tile_min.x; x < tile_max.x; x++) {
                float x0 = x * bt.x - xy.x;
                float x1 = x0 + bt.x;

                float2 bound = ellipse_range_bound(conic, x0, x1);
                int min_y = clamp(
                    (unsigned)floor((bound.x + xy.y) * bt.inv_y),
                    tile_min.y, tile_max.y
                );
                int max_y = clamp(
                    (unsigned)ceil((bound.y + xy.y) * bt.inv_y),
                    tile_min.y, tile_max.y
                );
                for (int y = min_y; y < max_y && cur_idx < max_idx; y++) {
                    if (live && !live[(int64_t)y * tile_width + x]) continue;
                    int64_t tile_id = iid * tile_width * tile_height + int64_t(y) * tile_width + int64_t(x);
                    isect_ids[cur_idx] = ((tile_id - key_tile_base) << 32) | (int64_t)depth_u32;
                    flatten_ids[cur_idx] = static_cast<int32_t>(idx);
                    ++cur_idx;
                }
            }
        }
    }
    else {
        for (int32_t i = tile_min.y; i < tile_max.y; ++i) {
            for (int32_t j = tile_min.x; j < tile_max.x; ++j) {
                if (cur_idx >= max_idx) break;
                if (live && !live[(int64_t)i * tile_width + j]) continue;
                int64_t tile_id = iid * tile_width * tile_height + i * tile_width + j;
                isect_ids[cur_idx] = ((tile_id - key_tile_base) << 32) | (int64_t)depth_u32;
                flatten_ids[cur_idx] = static_cast<int32_t>(idx);
                ++cur_idx;
            }
        }
    }
    // this can happen with floating point optimization, make sure it doesn't introduce invalid ID
    while (cur_idx < max_idx) {
        int64_t tile_id =
            max(iid * tile_width * tile_height, key_tile_base) - key_tile_base;
        isect_ids[cur_idx] = (tile_id << 32) | (int64_t)0xffffffff;
        flatten_ids[cur_idx] = static_cast<int32_t>(idx);
        ++cur_idx;
    }
}



// Offsets of a band's tiles, whose sorted keys count tiles from the band's
// first and whose pairs start at isect_base in the whole list. n_isects == 0:
// launched with one thread, which fills every offset with isect_base.
__global__ void intersect_offset_kernel(
    const uint32_t n_isects,
    const int64_t *__restrict__ isect_ids,
    const uint32_t n_offsets,
    const uint32_t isect_base,
    int32_t *__restrict__ offsets // [n_offsets]
) {
    uint32_t idx = cg::this_grid().thread_rank();
    if (n_isects == 0) {
        if (idx == 0)
            for (uint32_t i = 0; i < n_offsets; ++i)
                offsets[i] = static_cast<int32_t>(isect_base);
        return;
    }
    if (idx >= n_isects)
        return;

    int64_t tile_id = isect_ids[idx] >> 32;

    if (idx == 0) {
        // write out the offsets until the first valid tile (inclusive)
        for (uint32_t i = 0; i < tile_id + 1; ++i)
            offsets[i] = static_cast<int32_t>(isect_base + idx);
    }
    if (idx == n_isects - 1) {
        // write out the rest of the offsets
        for (uint32_t i = tile_id + 1; i < n_offsets; ++i)
            offsets[i] = static_cast<int32_t>(isect_base + n_isects);
    }

    if (idx > 0) {
        // visit the current and previous isect_id and check if the (bid, cid,
        // tile_id) tuple changes.
        int64_t tile_id_prev = isect_ids[idx - 1] >> 32;
        if (tile_id_prev == tile_id)
            return;

        // write out the offsets between the previous and current tiles
        for (uint32_t i = tile_id_prev + 1; i < tile_id + 1; ++i)
            offsets[i] = static_cast<int32_t>(isect_base + idx);
    }
}

// The depth -> normal stencil reads one pixel past its own: at 0 the warped
// engine_train_parity case moves, at 1 it is bit-identical.
static constexpr int kMaskPad = 1;

// One thread per tile: live when any render pixel of it (grown by kMaskPad) is
// unmasked, sampled exactly as the loss samples the mask (core/Interpolation.cuh).

__global__ void tile_active_kernel(
    const bool* __restrict__ mask,      // [I, H_mask, W_mask]
    const int I, const int H_mask, const int W_mask,
    const int width, const int height,
    const uint32_t tile_width, const uint32_t tile_height,
    const BinTile bt,
    int32_t* __restrict__ tile_active
) {
    uint32_t t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= (uint32_t)I * tile_width * tile_height) return;
    const uint32_t tx = t % tile_width;
    const uint32_t ty = (t / tile_width) % tile_height;
    const int b = (int)(t / (tile_width * tile_height));
    bool live = false;
    for (int dy = -kMaskPad; dy < bt.iy + kMaskPad && !live; ++dy) {
        const int y = (int)ty * bt.iy + dy;
        if (y >= height) break;
        if (y < 0) continue;
        for (int dx = -kMaskPad; dx < bt.ix + kMaskPad; ++dx) {
            const int x = (int)tx * bt.ix + dx;
            if (x >= width) break;
            if (x < 0) continue;
            if (nearest_sample_b(mask, b, x, y, width, height, W_mask, H_mask)) {
                live = true;
                break;
            }
        }
    }
    tile_active[t] = live ? 1 : 0;
}

// Tiles one image of this size has, i.e. its share of the live-tile map.
/*[AutoHeaderGeneratorExport]*/
int64_t intersect_tile_count(int width, int height, int macro_log2) {
    return (int64_t)_CEIL_DIV((uint32_t)width, bin_tile_x(macro_log2)) *
           _CEIL_DIV((uint32_t)height, bin_tile_y(macro_log2));
}

/*[AutoHeaderGeneratorExport]*/
void compute_tile_active(
    TorchTensorView mask,   // [I, H_mask, W_mask] bool
    int I, int width, int height,
    int macro_log2,         // binning granularity, core/Common.cuh
    int32_t* tile_active    // [I, tile_h, tile_w]
) {
    const auto& ms = std::get<2>(mask);
    const int H_mask = (int)ms[1], W_mask = (int)ms[2];
    const BinTile bt = bin_tile(macro_log2);
    const uint32_t tile_width = _CEIL_DIV((uint32_t)width, bt.ix);
    const uint32_t tile_height = _CEIL_DIV((uint32_t)height, bt.iy);
    const uint32_t n_tiles = (uint32_t)I * tile_width * tile_height;
    tile_active_kernel<<<_LAUNCH_ARGS_1D(n_tiles, 256)>>>(
        (const bool*)std::get<0>(mask), I, H_mask, W_mask, width, height,
        tile_width, tile_height, bt, tile_active);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


/*[AutoHeaderGeneratorExport]*/
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
) {
    bool packed = image_ids != nullptr;
    BinTile bt = bin_tile(macro_log2);
    // depths is [*N] float32 (numel = N or nnz); aabb is a [*N] packed box,
    // so either would do, but depths is what the non-packed shape agrees on.
    const int64_t total_count64 = depths.numel();
    if (total_count64 > 0x7fffffffLL)
        throw std::runtime_error(
            "tile intersect: " + std::to_string(total_count64) +
            " (camera, gaussian) pairs in one batch exceeds the 2^31 limit");
    const uint32_t total_count = (uint32_t)total_count64;
    const uint32_t N = packed ? total_count : total_count / I;
    const size_t count_span = 4 * (size_t)total_count;

    intersect_check_image_size(image_width, image_height);

    uint32_t tile_width = 0, tile_height = 0, n_tiles = 0;
    const int32_t* active = tile_active;
    DeviceVector<int32_t> tiles_per_splat, cum_tiles_per_splat;
    DeviceVector<int64_t> isect_ids_a, isect_ids_b;
    DeviceVector<int32_t> flatten_alt;

    // Tiles each splat touches in rows [row0, row1), and their exact total.
    auto count_rows = [&](int32_t row0, int32_t row1) -> int64_t {
        tiles_per_splat.resize(PoolSlot::IsectTilesPerSplat, total_count);
        (ellipse.data != nullptr ?
            intersect_tile_kernel<true, true> :
            intersect_tile_kernel<true, false>
        )<<<_LAUNCH_ARGS_1D(I*N, 256)>>>(
            packed ? 1 : I, N,
            // The count is per-image once tiles can be skipped: in packed mode
            // every splat would otherwise be counted against image 0's map.
            image_ids != nullptr ? image_ids->data_ptr() : nullptr,
            nullptr,  // intrins
            aabb.data_ptr(),
            image_width, image_height,
            depths.data_ptr(),
            ellipse,
            nullptr,  // cum_tiles_per_splat
            tile_width, tile_height,
            active,
            tiles_per_splat.data_ptr(),
            nullptr, nullptr, 0, bt, row0, row1, 0
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());

        /* Exact total, reduced in int64: the int32 scan below wraps silently
           past 2^31, and past 2^32 it wraps back to a plausible positive that
           would size the key buffers and send the write pass off the end. */
        int64_t n = 0;
        if (total_count > 0) {
            DeviceVector<int64_t> isect_total;
            isect_total.resize(PoolSlot::IsectTotal, 1);
            thrust::transform_iterator<WidenToI64, const int32_t*, int64_t>
                widened(tiles_per_splat.data_ptr(), WidenToI64{});
            CUB_WRAPPER(cub::DeviceReduce::Sum, widened, isect_total.data_ptr(),
                        (int)total_count);
            CHECK_DEVICE_ERROR(cudaGetLastError());
            cudaMemcpy(&n, isect_total.data_ptr(), sizeof(int64_t),
                       cudaMemcpyDeviceToHost);
        }
        return n;
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
        bt = bin_tile(macro_log2);
        tile_width = _CEIL_DIV(image_width, (uint32_t)bt.ix);
        tile_height = _CEIL_DIV(image_height, (uint32_t)bt.iy);
        n_tiles = intersect_check_tile_count(tile_width, tile_height, I);
        rows = (int32_t)(I * tile_height);

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

    // SS_TILE_SKIP_LOG=1: what the live-tile map removes from the sort and
    // the raster, which is what decides whether it pays.
    static const bool log_isects = [] {
        const char* v = spirula::env("TILE_SKIP_LOG");
        return v && *v;
    }();

    DeviceTensor3D<int32_t> offsets_out;
    offsets_out.resize(PoolSlot::IsectOffsets, I, tile_height, tile_width);
    if (n_isects == 0) {
        offsets_out.zero();
        return std::make_tuple(DeviceVector<int64_t>{}, DeviceVector<int32_t>{}, offsets_out);
    }
    if (log_isects)
        std::fprintf(stderr, "[isects] %lld pairs, %s\n", (long long)n_isects,
                     tile_active ? "masked tiles skipped" : "every tile");
    // The one list the raster keeps; every band sorts into its own stretch.
    DeviceVector<int32_t> flatten_ids;
    flatten_ids.resize(PoolSlot::IsectFlatA, n_isects);

    // Keys, sort and offsets of the pairs tiles_per_splat holds: rows [row0,
    // row1), whose tiles the keys count from tile0, into flatten_ids[base..].
    auto sort_rows = [&](int32_t row0, int32_t row1, uint32_t tile0,
                         uint32_t n_offsets, int32_t base, int32_t n) {
        /* Inclusive prefix sum -> cumulative tile counts */
        cum_tiles_per_splat.resize(PoolSlot::IsectCumTiles, total_count);
        CUB_WRAPPER(cub::DeviceScan::InclusiveSum,
            tiles_per_splat.data_ptr(), cum_tiles_per_splat.data_ptr(),
            (int)total_count);
        CHECK_DEVICE_ERROR(cudaGetLastError());
        isect_ids_a.resize(PoolSlot::IsectIdsA, n);
        isect_ids_b.resize(PoolSlot::IsectIdsB, n);
        flatten_alt.resize(PoolSlot::IsectFlatB, n);

        int tile_n_bits = 0;
        while ((1U << tile_n_bits) <= n_offsets) ++tile_n_bits;
        const int end_bit = 32 + tile_n_bits;
        // CUB swaps the buffers once per pass and its pass count follows the
        // key bits: start where the last sort of this width says the result
        // lands in flatten_ids, and copy when it did not.
        static int8_t swaps[65] = {};
        int32_t* const out = flatten_ids.data_ptr() + base;
        const bool odd = swaps[end_bit] == 1;
        cub::DoubleBuffer<int64_t> d_keys(isect_ids_a.data_ptr(), isect_ids_b.data_ptr());
        cub::DoubleBuffer<int32_t> d_values(odd ? flatten_alt.data_ptr() : out,
                                            odd ? out : flatten_alt.data_ptr());

        (ellipse.data != nullptr ?
            intersect_tile_kernel<false, true> :
            intersect_tile_kernel<false, false>
        )<<<_LAUNCH_ARGS_1D(I*N, 256)>>>(
            packed ? 1 : I, N,
            image_ids != nullptr ? image_ids->data_ptr() : nullptr,
            (const float4*)std::get<0>(intrins),
            aabb.data_ptr(),
            image_width, image_height,
            depths.data_ptr(),
            ellipse,
            cum_tiles_per_splat.data_ptr(),
            tile_width, tile_height,
            active,
            nullptr,
            d_keys.Current(),
            d_values.Current(),
            n, bt, row0, row1, (int64_t)tile0
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());

        /* Sort by (tile_id << 32 | depth) key */
        CUB_WRAPPER(cub::DeviceRadixSort::SortPairs,
            d_keys, d_values, n, 0, end_bit);
        CHECK_DEVICE_ERROR(cudaGetLastError());
        if (d_values.Current() != out) {
            cudaMemcpy(out, d_values.Current(), (size_t)n * sizeof(int32_t),
                       cudaMemcpyDeviceToDevice);
            swaps[end_bit] = odd ? 0 : 1;
        }

        /* Compute per-tile start offsets */
        intersect_offset_kernel<<<_LAUNCH_ARGS_1D(n, 256)>>>(
            n, d_keys.Current(), n_offsets, (uint32_t)base,
            offsets_out.data_ptr() + tile0
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());
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
            intersect_offset_kernel<<<1, 1>>>(
                0, nullptr, n_offsets, (uint32_t)base, offsets_out.data_ptr() + tile0);
            CHECK_DEVICE_ERROR(cudaGetLastError());
        }
        // An empty band's tiles wait for the next one, whose keys reach back.
        if (n > 0) tile0 += n_offsets;
        base += (int32_t)n;
        row += take;
        density = (double)n / (double)take;
    }
    return std::make_tuple(DeviceVector<int64_t>{}, flatten_ids, offsets_out);
}
