// Splat-to-tile intersection (Vulkan backend), mirroring
// kernels/tile/IntersectTile.cu: a counting pass, a key-write pass (keys are
// (tile_id << 32) | ordered-float depth, sorted by the host over
// backend::sort_pairs<int64,int32>), and the per-tile offset kernel.
//
// The ellipse-vs-AABB mode split is a runtime null-check on proj_screen
// (the CUDA template bool exists only to fold that check; the branch is
// uniform across the grid so the cost is nil). All params fit in the
// 128-byte push floor and are pushed directly.

#include "int64_compat.slang"
#include "int8_compat.slang"

static const int TILE_SIZE_IX = 16;  // TILE_SIZE_X * MACRO_TILE_SIZE_X
static const int TILE_SIZE_IY = 16;  // TILE_SIZE_Y * MACRO_TILE_SIZE_Y

static const float ALPHA_THRESHOLD = 1.0f / 255.0f;

// Find x_min / x_max of an ellipse (centered at origin, x^T inv_cov x = 1)
// clipped to y0 < y <= y1. Port of IntersectTile.cu ellipse_range_bound.
float2 ellipse_range_bound(float3 inv_cov, float y0, float y1) {
    float a = inv_cov.x, b = inv_cov.y, c = inv_cov.z;
    float ym = -b / c * sqrt(c / (a * c - b * b));

    float v0 = clamp(-ym, y0, y1);
    float v1 = clamp(ym, y0, y1);

    float bv0 = -b * v0, bv1 = -b * v1;

    float inv_a = 1.0f / a;
    float x0 = inv_a * (bv0 - sqrt(bv0 * bv0 - a * (c * v0 * v0 - 1.f)));
    float x1 = inv_a * (bv1 + sqrt(bv1 * bv1 - a * (c * v1 * v1 - 1.f)));

    return float2(x0, x1);
}

// Count grid cells overlapping an ellipse. Port of
// IntersectTile.cu count_ellipse_grid_overlaps.
int count_ellipse_grid_overlaps(
    float2 xy, float3 inv_cov,
    int grid_xmin, int grid_xmax, int grid_ymin, int grid_ymax,
    int* tile_active, gindex_t tile_base, uint tile_width
) {
    int n_tiles = 0;

    if ((grid_ymax - grid_ymin) * TILE_SIZE_IX <=
        (grid_xmax - grid_xmin) * TILE_SIZE_IY) {
        for (int y = grid_ymin; y < grid_ymax; y++) {
            float y0 = y * TILE_SIZE_IY - xy.y;
            float y1 = y0 + TILE_SIZE_IY;
            float2 bound = ellipse_range_bound(inv_cov, y0, y1);
            int x0 = int(floor((bound.x + xy.x) / TILE_SIZE_IX));
            int x1 = int(ceil((bound.y + xy.x) / TILE_SIZE_IX));
            x0 = clamp(x0, grid_xmin, grid_xmax);
            x1 = clamp(x1, grid_xmin, grid_xmax);
            if (kSkipTiles == 0) {
                n_tiles += max(x1 - x0, 0);
            } else {
                for (int x = x0; x < x1; x++)
                    n_tiles += tile_active[tile_base + gindex_t(y) * tile_width +
                                           gindex_t(x)] != 0 ? 1 : 0;
            }
        }
    } else {
        inv_cov = float3(inv_cov.z, inv_cov.y, inv_cov.x);
        for (int x = grid_xmin; x < grid_xmax; x++) {
            float x0 = x * TILE_SIZE_IX - xy.x;
            float x1 = x0 + TILE_SIZE_IX;
            float2 bound = ellipse_range_bound(inv_cov, x0, x1);
            int y0 = int(floor((bound.x + xy.y) / TILE_SIZE_IY));
            int y1 = int(ceil((bound.y + xy.y) / TILE_SIZE_IY));
            y0 = clamp(y0, grid_ymin, grid_ymax);
            y1 = clamp(y1, grid_ymin, grid_ymax);
            if (kSkipTiles == 0) {
                n_tiles += max(y1 - y0, 0);
            } else {
                for (int y = y0; y < y1; y++)
                    n_tiles += tile_active[tile_base + gindex_t(y) * tile_width +
                                           gindex_t(x)] != 0 ? 1 : 0;
            }
        }
    }

    return n_tiles;
}

// Shared tile-range setup for both passes; matches the CUDA kernel body.
void tile_range(float4 aabb, uint tile_width, uint tile_height,
                out uint2 tile_min, out uint2 tile_max) {
    tile_min.x = uint(min(max(0, int(floor((aabb.x + 0.5f) / TILE_SIZE_IX))),
                          int(tile_width)));
    tile_min.y = uint(min(max(0, int(floor((aabb.y + 0.5f) / TILE_SIZE_IY))),
                          int(tile_height)));
    tile_max.x = uint(min(max(0, int(ceil((aabb.z + 0.5f) / TILE_SIZE_IX))),
                          int(tile_width)));
    tile_max.y = uint(min(max(0, int(ceil((aabb.w + 0.5f) / TILE_SIZE_IY))),
                          int(tile_height)));
}

// ---------------------------------------------------------------------------
// Counting pass: tiles_per_splat[idx] = number of tiles this splat touches.
// ---------------------------------------------------------------------------

struct IsectCountParams {  // pushed directly
    float4* aabb;              // [total] xyxy in pixels
    float* proj_screen;        // packed screen rows; null -> AABB mode
    int* tiles_per_splat;      // [total]
    int* image_ids;            // [total] (packed) or null (iid = idx / N)
    int* tile_active;          // [I, tile_h, tile_w] 0/1; read when kSkipTiles
    uint32_t N;                // splats per image
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t total;            // I * N (or nnz in packed mode)
    uint32_t wgs_per_row;
    // Row layout of proj_screen; xy_off < 0 -> center from the AABB.
    int32_t row_stride, xy_off, conic_off, opac_off;
};

// Mode flags as specialization constants. Spec-folded so no load ever sits
// behind a runtime branch over a possibly-null device pointer (llvmpipe's
// JIT can speculate such loads; see backend README).
[SpecializationConstant]
const int kEllipse = 0;  // proj_screen present (ellipse counting)
[SpecializationConstant]
const int kHasXy = 0;    // row carries a center (else AABB center)
[SpecializationConstant]
const int kPacked = 0;   // image_ids present (packed projection)
[SpecializationConstant]
const int kSkipTiles = 0;  // tile_active present (masked tiles left out)

[shader("compute")]
[numthreads(256, 1, 1)]
void intersect_tile_count(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform IsectCountParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;

    float4 aabb = p.aabb[idx];
    if (aabb.z <= aabb.x || aabb.w <= aabb.y) {
        p.tiles_per_splat[idx] = 0;
        return;
    }

    uint2 tile_min, tile_max;
    tile_range(aabb, p.tile_width, p.tile_height, tile_min, tile_max);

    gindex_t iid = kPacked != 0 ? gindex_t(p.image_ids[idx])
                                : gindex_t(idx / p.N);
    gindex_t tile_base = iid * gindex_t(p.tile_width) * gindex_t(p.tile_height);

    if (kEllipse != 0) {
        float* row = p.proj_screen + p.row_stride * int(idx);
        float2 xy = kHasXy != 0
            ? float2(row[p.xy_off], row[p.xy_off + 1])
            : float2(0.5f * (aabb.x + aabb.z), 0.5f * (aabb.y + aabb.w));
        float3 conic = float3(row[p.conic_off], row[p.conic_off + 1],
                              row[p.conic_off + 2]);
        conic = conic * (0.5f / log(row[p.opac_off] / ALPHA_THRESHOLD));
        p.tiles_per_splat[idx] = count_ellipse_grid_overlaps(
            xy, conic,
            int(tile_min.x), int(tile_max.x), int(tile_min.y),
            int(tile_max.y), p.tile_active, tile_base, p.tile_width);
    } else if (kSkipTiles == 0) {
        p.tiles_per_splat[idx] = max(
            int(tile_max.y - tile_min.y) * int(tile_max.x - tile_min.x), 0);
    } else {
        int n = 0;
        for (uint i = tile_min.y; i < tile_max.y; ++i)
            for (uint j = tile_min.x; j < tile_max.x; ++j)
                n += p.tile_active[tile_base + gindex_t(i) * p.tile_width +
                                   gindex_t(j)] != 0 ? 1 : 0;
        p.tiles_per_splat[idx] = n;
    }
}

// The depth -> normal stencil reads one pixel past its own; IntersectTile.cu
// kMaskPad.
static const int kMaskPad = 1;

// One thread per tile: live when any render pixel of it (grown by kMaskPad) is
// unmasked, sampled as the loss samples the mask (core/Interpolation.cuh).
struct TileActiveParams {  // 40 bytes: pushed directly
    uint* mask;          // [I, H_mask, W_mask] uint8
    int* tile_active;    // [I, tile_h, tile_w]
    int32_t H_mask, W_mask;
    int32_t width, height;
    uint32_t tile_width, tile_height;
    uint32_t total;
    uint32_t wgs_per_row;
    int32_t row_stride, xy_off, conic_off, opac_off;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void tile_active_map(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform TileActiveParams p) {
    uint t = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (t >= p.total) return;
    uint tx = t % p.tile_width;
    uint ty = (t / p.tile_width) % p.tile_height;
    gindex_t b = gindex_t(t / (p.tile_width * p.tile_height));
    bool live = false;
    for (int dy = -kMaskPad; dy < TILE_SIZE_IY + kMaskPad && !live; ++dy) {
        int y = int(ty) * TILE_SIZE_IY + dy;
        if (y >= p.height) break;
        if (y < 0) continue;
        for (int dx = -kMaskPad; dx < TILE_SIZE_IX + kMaskPad; ++dx) {
            int x = int(tx) * TILE_SIZE_IX + dx;
            if (x >= p.width) break;
            if (x < 0) continue;
            float u = (float(x) + 0.5f) * float(p.W_mask) / float(p.width) - 0.5f;
            float v = (float(y) + 0.5f) * float(p.H_mask) / float(p.height) - 0.5f;
            int mx = max(0, min(p.W_mask - 1, int(floor(u + 0.5f))));
            int my = max(0, min(p.H_mask - 1, int(floor(v + 0.5f))));
            gindex_t mi = b * gindex_t(p.H_mask) * gindex_t(p.W_mask) +
                          gindex_t(my) * gindex_t(p.W_mask) + gindex_t(mx);
            if (u8_load(p.mask, mi) != 0) { live = true; break; }
        }
    }
    p.tile_active[t] = live ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Key-write pass: scatter (tile_id << 32 | depth_u32, flat splat index)
// pairs into the [cum_tiles[idx-1], cum_tiles[idx]) range.
// ---------------------------------------------------------------------------

struct IsectWriteParams {  // pushed directly
    int* image_ids;      // [total] (packed mode) or null (iid = idx / N)
    float4* aabb;
    float* depths;       // [total] sorting depths
    float* proj_screen;  // packed screen rows; null -> AABB mode
    int* cum_tiles;      // [total] inclusive scan of tiles_per_splat
    u64key_t* isect_ids;  // [n_isects]
    int* flatten_ids;    // [n_isects]
    int* tile_active;    // [I, tile_h, tile_w] 0/1; read when kSkipTiles
    uint32_t N;          // splats per image (non-packed iid = idx / N)
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t total;
    uint32_t wgs_per_row;
    int32_t row_stride, xy_off, conic_off, opac_off;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void intersect_tile_write(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform IsectWriteParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;

    gindex_t iid = kPacked != 0 ? gindex_t(p.image_ids[idx])
                                         : gindex_t(idx / p.N);
    int cur_idx = (idx == 0) ? 0 : p.cum_tiles[idx - 1];
    int max_idx = p.cum_tiles[idx];

    float4 aabb = p.aabb[idx];
    if (aabb.z <= aabb.x || aabb.w <= aabb.y) {
        // cur_idx should equal max_idx; fill just in case floating point
        // optimization goes wild (matches the CUDA kernel).
        gindex_t n_img_tiles = gindex_t(p.tile_width) * gindex_t(p.tile_height);
        while (cur_idx < max_idx) {
            gindex_t tile_id = iid * n_img_tiles;
            p.isect_ids[cur_idx] = u64key_make(tile_id, 0xffffffffu);
            p.flatten_ids[cur_idx] = int(idx);
            ++cur_idx;
        }
        return;
    }
    gindex_t n_img_tiles = gindex_t(p.tile_width) * gindex_t(p.tile_height);

    uint2 tile_min, tile_max;
    tile_range(aabb, p.tile_width, p.tile_height, tile_min, tile_max);

    // Sorting depth -> order-preserving u32 (same bit trick as CUDA).
    float depth_f32 = abs(p.depths[idx]);
    uint depth_u32 = asuint(depth_f32);
    if ((depth_u32 >> 31) != 0)  // negative
        depth_u32 = ~depth_u32;
    else  // positive
        depth_u32 ^= (1u << 31);

    if (kEllipse != 0) {
        float* row = p.proj_screen + p.row_stride * int(idx);
        float2 xy = kHasXy != 0
            ? float2(row[p.xy_off], row[p.xy_off + 1])
            : float2(0.5f * (aabb.x + aabb.z), 0.5f * (aabb.y + aabb.w));
        float3 conic = float3(row[p.conic_off], row[p.conic_off + 1],
                              row[p.conic_off + 2]);
        conic = conic * (0.5f / log(row[p.opac_off] / ALPHA_THRESHOLD));

        if (int(tile_max.y - tile_min.y) * TILE_SIZE_IX <=
            int(tile_max.x - tile_min.x) * TILE_SIZE_IY) {
            for (int y = int(tile_min.y); y < int(tile_max.y); y++) {
                float y0 = y * TILE_SIZE_IY - xy.y;
                float y1 = y0 + TILE_SIZE_IY;
                float2 bound = ellipse_range_bound(conic, y0, y1);
                int min_x = int(clamp(uint(floor((bound.x + xy.x) / TILE_SIZE_IX)),
                                      tile_min.x, tile_max.x));
                int max_x = int(clamp(uint(ceil((bound.y + xy.x) / TILE_SIZE_IX)),
                                      tile_min.x, tile_max.x));
                for (int x = min_x; x < max_x && cur_idx < max_idx; x++) {
                    gindex_t tile_id = iid * n_img_tiles +
                                      gindex_t(y) * gindex_t(p.tile_width) +
                                      gindex_t(x);
                    if (kSkipTiles != 0 && p.tile_active[tile_id] == 0) continue;
                    p.isect_ids[cur_idx] = u64key_make(tile_id, depth_u32);
                    p.flatten_ids[cur_idx] = int(idx);
                    ++cur_idx;
                }
            }
        } else {
            conic = float3(conic.z, conic.y, conic.x);
            for (int x = int(tile_min.x); x < int(tile_max.x); x++) {
                float x0 = x * TILE_SIZE_IX - xy.x;
                float x1 = x0 + TILE_SIZE_IX;
                float2 bound = ellipse_range_bound(conic, x0, x1);
                int min_y = int(clamp(uint(floor((bound.x + xy.y) / TILE_SIZE_IY)),
                                      tile_min.y, tile_max.y));
                int max_y = int(clamp(uint(ceil((bound.y + xy.y) / TILE_SIZE_IY)),
                                      tile_min.y, tile_max.y));
                for (int y = min_y; y < max_y && cur_idx < max_idx; y++) {
                    gindex_t tile_id = iid * n_img_tiles +
                                      gindex_t(y) * gindex_t(p.tile_width) +
                                      gindex_t(x);
                    if (kSkipTiles != 0 && p.tile_active[tile_id] == 0) continue;
                    p.isect_ids[cur_idx] = u64key_make(tile_id, depth_u32);
                    p.flatten_ids[cur_idx] = int(idx);
                    ++cur_idx;
                }
            }
        }
    } else {
        for (int i = int(tile_min.y); i < int(tile_max.y); ++i) {
            for (int j = int(tile_min.x); j < int(tile_max.x); ++j) {
                if (cur_idx >= max_idx) break;
                gindex_t tile_id = iid * n_img_tiles +
                                  gindex_t(i) * gindex_t(p.tile_width) +
                                  gindex_t(j);
                if (kSkipTiles != 0 && p.tile_active[tile_id] == 0) continue;
                p.isect_ids[cur_idx] = u64key_make(tile_id, depth_u32);
                p.flatten_ids[cur_idx] = int(idx);
                ++cur_idx;
            }
        }
    }
    // Don't leave invalid IDs behind if the count under-estimated.
    while (cur_idx < max_idx) {
        gindex_t tile_id = iid * n_img_tiles;
        p.isect_ids[cur_idx] = u64key_make(tile_id, 0xffffffffu);
        p.flatten_ids[cur_idx] = int(idx);
        ++cur_idx;
    }
}

// ---------------------------------------------------------------------------
// Per-tile start offsets over the sorted keys.
// ---------------------------------------------------------------------------

struct IsectOffsetParams {  // 28 bytes: pushed directly
    u64key_t* isect_ids;  // [n_isects] sorted
    int* offsets;        // [I * tile_height * tile_width]
    uint32_t n_isects;
    uint32_t n_offsets;  // I * tile_height * tile_width
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void intersect_offset(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform IsectOffsetParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.n_isects) return;

    gindex_t tile_id = u64key_hi(p.isect_ids[idx]);

    if (idx == 0) {
        // write out the offsets until the first valid tile (inclusive)
        for (uint i = 0; i < uint(tile_id) + 1; ++i)
            p.offsets[i] = int(idx);
    }
    if (idx == p.n_isects - 1) {
        // write out the rest of the offsets
        for (uint i = uint(tile_id) + 1; i < p.n_offsets; ++i)
            p.offsets[i] = int(p.n_isects);
    }

    if (idx > 0) {
        gindex_t tile_id_prev = u64key_hi(p.isect_ids[idx - 1]);
        if (tile_id_prev == tile_id)
            return;
        // write out the offsets between the previous and current tiles
        for (uint i = uint(tile_id_prev) + 1; i < uint(tile_id) + 1; ++i)
            p.offsets[i] = int(idx);
    }
}
