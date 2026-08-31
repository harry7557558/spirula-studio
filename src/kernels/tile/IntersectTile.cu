#include "kernels/tile/IntersectTile.cuh"

#include "generated/slang.cuh"
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
#include "generated/projection_utils.cuh"
}

#include "core/Common.cuh"
#include "core/Env.h"
#include "core/Interpolation.cuh"

#include <cooperative_groups.h>

#include <cub/cub.cuh>


namespace cg = cooperative_groups;


inline constexpr int TILE_SIZE_IX = TILE_SIZE_X * MACRO_TILE_SIZE_X;
inline constexpr int TILE_SIZE_IY = TILE_SIZE_Y * MACRO_TILE_SIZE_Y;


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
    const int32_t* __restrict__ live,   // this image's tiles, or null
    uint32_t tile_width
) {
    // count the number grid cells that overlap with an ellipse

    int n_tiles = 0;

    if ((grid_ymax-grid_ymin)*TILE_SIZE_IX <= (grid_xmax-grid_xmin)*TILE_SIZE_IY) {
        for (int y = grid_ymin; y < grid_ymax; y++) {
            float y0 = y * TILE_SIZE_IY - xy.y;
            float y1 = y0 + TILE_SIZE_IY;
            float2 bound = ellipse_range_bound(inv_cov, y0, y1);
            int x0 = int(floor((bound.x + xy.x) / TILE_SIZE_IX));
            int x1 = int(ceil((bound.y + xy.x) / TILE_SIZE_IX));
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
            float x0 = x * TILE_SIZE_IX - xy.x;
            float x1 = x0 + TILE_SIZE_IX;
            float2 bound = ellipse_range_bound(inv_cov, x0, x1);
            int y0 = int(floor((bound.x + xy.y) / TILE_SIZE_IY));
            int y1 = int(ceil((bound.y + xy.y) / TILE_SIZE_IY));
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


template<bool is_counting_pass, bool is_ellipse>
__global__ void intersect_tile_kernel(
    const uint32_t I,  // or 1 in packed mode
    const uint32_t N,  // or nnz in packed mode
    const int32_t *__restrict__ image_ids,  // [nnz], packed mode only
    const float4* __restrict__ intrins,
    const float4 *__restrict__ aabb_buffer,  // [..., N, 4], int32, xyxy in pixels
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
    int32_t *__restrict__ flatten_ids  // [n_isects]
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= I * N) {
        return;
    }

    float4 aabb = aabb_buffer[idx];
    float xmin = aabb.x, ymin = aabb.y;
    float xmax = aabb.z, ymax = aabb.w;

    if (xmax <= xmin || ymax <= ymin) {
        if (is_counting_pass) {
            tiles_per_splat[idx] = 0;
        } else {
            // cur_idx should equal to max_idx
            // do this just in case compiler floating point optimization goes wild
            int64_t iid = (image_ids ? image_ids[idx] : idx / N);
            int32_t cur_idx = (idx == 0) ? 0 : cum_tiles_per_splat[idx - 1];
            int32_t max_idx = cum_tiles_per_splat[idx];
            while (cur_idx < max_idx) {
                int64_t tile_id = iid * tile_width * tile_height;
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
        conic = conic * (0.5f / __logf(row[ellipse.opac] / ALPHA_THRESHOLD));
    }

    uint2 tile_min, tile_max;
    tile_min.x = (uint32_t)min(max(0, (int)floorf((xmin + 0.5f) / TILE_SIZE_IX)), (int)tile_width);
    tile_min.y = (uint32_t)min(max(0, (int)floorf((ymin + 0.5f) / TILE_SIZE_IY)), (int)tile_height);
    tile_max.x = (uint32_t)min(max(0, (int)ceilf((xmax + 0.5f) / TILE_SIZE_IX)), (int)tile_width);
    tile_max.y = (uint32_t)min(max(0, (int)ceilf((ymax + 0.5f) / TILE_SIZE_IY)), (int)tile_height);
    const int64_t iid_c = (image_ids ? image_ids[idx] : idx / N);
    const int32_t* live = tile_active
        ? tile_active + iid_c * (int64_t)tile_width * tile_height : nullptr;
    if constexpr (is_counting_pass) {
        // counting pass only writes out tiles_per_splat
        if constexpr (is_ellipse)
            tiles_per_splat[idx] = count_ellipse_grid_overlaps(
                xy, conic,
                tile_min.x, tile_max.x, tile_min.y, tile_max.y,
                live, tile_width
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
    
    int32_t cur_idx = (idx == 0) ? 0 : cum_tiles_per_splat[idx - 1];
    int32_t max_idx = cum_tiles_per_splat[idx];
    if constexpr (is_ellipse) {
        if ((tile_max.y-tile_min.y)*TILE_SIZE_IX <= (tile_max.x-tile_min.x)*TILE_SIZE_IY) {
            for (int y = tile_min.y; y < tile_max.y; y++) {
                float y0 = y * TILE_SIZE_IY - xy.y;
                float y1 = y0 + TILE_SIZE_IY;

                float2 bound = ellipse_range_bound(conic, y0, y1);
                int min_x = clamp(
                    (unsigned)floor((bound.x + xy.x) / TILE_SIZE_IX),
                    tile_min.x, tile_max.x
                );
                int max_x = clamp(
                    (unsigned)ceil((bound.y + xy.x) / TILE_SIZE_IX),
                    tile_min.x, tile_max.x
                );
                for (int x = min_x; x < max_x && cur_idx < max_idx; x++) {
                    if (live && !live[(int64_t)y * tile_width + x]) continue;
                    int64_t tile_id = iid * tile_width * tile_height + int64_t(y) * tile_width + int64_t(x);
                    isect_ids[cur_idx] = (tile_id << 32) | (int64_t)depth_u32;
                    flatten_ids[cur_idx] = static_cast<int32_t>(idx);
                    ++cur_idx;
                }
            }
        } else {
            conic = {conic.z, conic.y, conic.x};
            for (int x = tile_min.x; x < tile_max.x; x++) {
                float x0 = x * TILE_SIZE_IX - xy.x;
                float x1 = x0 + TILE_SIZE_IX;

                float2 bound = ellipse_range_bound(conic, x0, x1);
                int min_y = clamp(
                    (unsigned)floor((bound.x + xy.y) / TILE_SIZE_IY),
                    tile_min.y, tile_max.y
                );
                int max_y = clamp(
                    (unsigned)ceil((bound.y + xy.y) / TILE_SIZE_IY),
                    tile_min.y, tile_max.y
                );
                for (int y = min_y; y < max_y && cur_idx < max_idx; y++) {
                    if (live && !live[(int64_t)y * tile_width + x]) continue;
                    int64_t tile_id = iid * tile_width * tile_height + int64_t(y) * tile_width + int64_t(x);
                    isect_ids[cur_idx] = (tile_id << 32) | (int64_t)depth_u32;
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
                isect_ids[cur_idx] = (tile_id << 32) | (int64_t)depth_u32;
                flatten_ids[cur_idx] = static_cast<int32_t>(idx);
                ++cur_idx;
            }
        }
    }
    // this can happen with floating point optimization, make sure it doesn't introduce invalid ID
    while (cur_idx < max_idx) {
        int64_t tile_id = iid * tile_width * tile_height;
        isect_ids[cur_idx] = (tile_id << 32) | (int64_t)0xffffffff;
        flatten_ids[cur_idx] = static_cast<int32_t>(idx);
        ++cur_idx;
    }
}



__global__ void intersect_offset_kernel(
    const uint32_t n_isects,
    const int64_t *__restrict__ isect_ids,
    const uint32_t I,
    const uint32_t tile_width,
    const uint32_t tile_height,
    int32_t *__restrict__ offsets // [I, tile_height, tile_width]
) {
    uint32_t idx = cg::this_grid().thread_rank();
    if (idx >= n_isects)
        return;

    int64_t tile_id = isect_ids[idx] >> 32;

    if (idx == 0) {
        // write out the offsets until the first valid tile (inclusive)
        for (uint32_t i = 0; i < tile_id + 1; ++i)
            offsets[i] = static_cast<int32_t>(idx);
    }
    if (idx == n_isects - 1) {
        // write out the rest of the offsets
        for (uint32_t i = tile_id + 1; i < I * tile_width * tile_height; ++i)
            offsets[i] = static_cast<int32_t>(n_isects);
    }

    if (idx > 0) {
        // visit the current and previous isect_id and check if the (bid, cid,
        // tile_id) tuple changes.
        int64_t tile_id_prev = isect_ids[idx - 1] >> 32;
        if (tile_id_prev == tile_id)
            return;

        // write out the offsets between the previous and current tiles
        for (uint32_t i = tile_id_prev + 1; i < tile_id + 1; ++i)
            offsets[i] = static_cast<int32_t>(idx);
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
    int32_t* __restrict__ tile_active
) {
    uint32_t t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= (uint32_t)I * tile_width * tile_height) return;
    const uint32_t tx = t % tile_width;
    const uint32_t ty = (t / tile_width) % tile_height;
    const int b = (int)(t / (tile_width * tile_height));
    bool live = false;
    for (int dy = -kMaskPad; dy < TILE_SIZE_IY + kMaskPad && !live; ++dy) {
        const int y = (int)ty * TILE_SIZE_IY + dy;
        if (y >= height) break;
        if (y < 0) continue;
        for (int dx = -kMaskPad; dx < TILE_SIZE_IX + kMaskPad; ++dx) {
            const int x = (int)tx * TILE_SIZE_IX + dx;
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
int64_t intersect_tile_count(int width, int height) {
    return (int64_t)_CEIL_DIV((uint32_t)width, TILE_SIZE_IX) *
           _CEIL_DIV((uint32_t)height, TILE_SIZE_IY);
}

/*[AutoHeaderGeneratorExport]*/
void compute_tile_active(
    TorchTensorView mask,   // [I, H_mask, W_mask] bool
    int I, int width, int height,
    int32_t* tile_active    // [I, tile_h, tile_w]
) {
    const auto& ms = std::get<2>(mask);
    const int H_mask = (int)ms[1], W_mask = (int)ms[2];
    const uint32_t tile_width = _CEIL_DIV((uint32_t)width, TILE_SIZE_IX);
    const uint32_t tile_height = _CEIL_DIV((uint32_t)height, TILE_SIZE_IY);
    const uint32_t n_tiles = (uint32_t)I * tile_width * tile_height;
    tile_active_kernel<<<_LAUNCH_ARGS_1D(n_tiles, 256)>>>(
        (const bool*)std::get<0>(mask), I, H_mask, W_mask, width, height,
        tile_width, tile_height, tile_active);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


/*[AutoHeaderGeneratorExport]*/
std::tuple<
    DeviceVector<int64_t>,    // isect_ids [n_isects]
    DeviceVector<int32_t>,    // flatten_ids [n_isects]
    DeviceTensor3D<int32_t>   // offsets [I, tile_h, tile_w]
> do_intersect_tile_generic(
    DeviceTensorFloatND aabb,     // [*N, 4] float32
    DeviceTensorFloatND depths,   // [*N] float32
    ProjEllipseView ellipse,      // .data null for AABB mode
    const uint32_t I,
    TorchTensorView intrins,      // [I, 4]
    const uint32_t image_width,
    const uint32_t image_height,
    DeviceVector<int32_t>* image_ids, // null for non-packed
    const int32_t* tile_active        // [I, tile_h, tile_w]; null = all live
) {
    bool packed = image_ids != nullptr;
    // depths is always [*N] float32 (numel = N or nnz), while aabb is [*N, 4]
    // whose numel() = N*4. Use depths.numel() to get the correct splat count.
    const uint32_t total_count = (uint32_t)depths.numel();
    const uint32_t N = packed ? total_count : total_count / I;

    intersect_check_image_size(image_width, image_height);
    uint32_t tile_width = _CEIL_DIV(image_width, TILE_SIZE_IX);
    uint32_t tile_height = _CEIL_DIV(image_height, TILE_SIZE_IY);
    uint32_t n_tiles = intersect_check_tile_count(tile_width, tile_height, I);

    /* Count tiles intersected per splat */
    DeviceVector<int32_t> tiles_per_splat;
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
        reinterpret_cast<const float4*>(aabb.data_ptr()),
        depths.data_ptr(),
        ellipse,
        nullptr,  // cum_tiles_per_splat
        tile_width, tile_height,
        tile_active,
        tiles_per_splat.data_ptr(),
        nullptr, nullptr
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    /* Inclusive prefix sum -> cumulative tile counts */
    DeviceVector<int32_t> cum_tiles_per_splat;
    cum_tiles_per_splat.resize(PoolSlot::IsectCumTiles, total_count);
    CUB_WRAPPER(cub::DeviceScan::InclusiveSum,
        tiles_per_splat.data_ptr(), cum_tiles_per_splat.data_ptr(), (int)total_count);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    /* Read total intersection count from the last element */
    int32_t n_isects = 0;
    if (total_count > 0)
        cudaMemcpy(&n_isects,
                   cum_tiles_per_splat.data_ptr() + (total_count - 1),
                   sizeof(int32_t), cudaMemcpyDeviceToHost);
    intersect_check_isect_count(n_isects);

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

    /* Write isect keys into double-buffer pair */
    DeviceVector<int64_t> isect_ids_a, isect_ids_b;
    isect_ids_a.resize(PoolSlot::IsectIdsA, n_isects);
    isect_ids_b.resize(PoolSlot::IsectIdsB, n_isects);
    DeviceVector<int32_t> flatten_ids_a, flatten_ids_b;
    flatten_ids_a.resize(PoolSlot::IsectFlatA, n_isects);
    flatten_ids_b.resize(PoolSlot::IsectFlatB, n_isects);

    (ellipse.data != nullptr ?
        intersect_tile_kernel<false, true> :
        intersect_tile_kernel<false, false>
    )<<<_LAUNCH_ARGS_1D(I*N, 256)>>>(
        packed ? 1 : I, N,
        image_ids != nullptr ? image_ids->data_ptr() : nullptr,
        (const float4*)std::get<0>(intrins),
        reinterpret_cast<const float4*>(aabb.data_ptr()),
        depths.data_ptr(),
        ellipse,
        cum_tiles_per_splat.data_ptr(),
        tile_width, tile_height,
        tile_active,
        nullptr,
        isect_ids_a.data_ptr(),
        flatten_ids_a.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    if (log_isects)
        std::fprintf(stderr, "[isects] %lld pairs, %s\n", (long long)n_isects,
                     tile_active ? "masked tiles skipped" : "every tile");

    /* Sort by (tile_id << 32 | depth) key */
    cub::DoubleBuffer<int64_t> d_keys(isect_ids_a.data_ptr(), isect_ids_b.data_ptr());
    cub::DoubleBuffer<int32_t> d_values(flatten_ids_a.data_ptr(), flatten_ids_b.data_ptr());
    int tile_n_bits = 0;
    while ((1U << tile_n_bits) <= n_tiles) ++tile_n_bits;
    CUB_WRAPPER(cub::DeviceRadixSort::SortPairs,
        d_keys, d_values, n_isects, 0, 32 + tile_n_bits);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    /* Pick whichever buffer CUB left the sorted result in */
    DeviceVector<int64_t> isect_ids_out  = d_keys.selector   ? isect_ids_b  : isect_ids_a;
    DeviceVector<int32_t> flatten_ids_out = d_values.selector ? flatten_ids_b : flatten_ids_a;

    /* Compute per-tile start offsets */
    intersect_offset_kernel<<<_LAUNCH_ARGS_1D(n_isects, 256)>>>(
        n_isects,
        isect_ids_out.data_ptr(),
        I, tile_width, tile_height,
        offsets_out.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    return std::make_tuple(isect_ids_out, flatten_ids_out, offsets_out);
}
