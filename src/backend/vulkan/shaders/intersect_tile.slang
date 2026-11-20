// Splat-to-tile intersection (Vulkan backend), mirroring
// kernels/tile/IntersectTile.cu: a counting pass, a key-write pass (keys are
// (tile_id << 32) | ordered-float depth, sorted by the host over
// backend::sort_pairs<int64,int32>), and the per-tile offset kernel. Both
// passes take a band of tile rows, so the host can sort a large image a band
// at a time (docs/notes/vram-splat-x-img.md, "Sorting in bands").
//
// The ellipse-vs-AABB mode split is a runtime null-check on proj_screen
// (the CUDA template bool exists only to fold that check; the branch is
// uniform across the grid so the cost is nil). The count and write passes'
// params go through the params ring; the others fit the 128-byte push floor.

#define CudaDeviceExport ForceInline
#include "aabb16.slang"
#include "int64_compat.slang"
#include "int8_compat.slang"
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport

#include "dist_spec.slang"

// Micro-tile edge (raster_common.slang TILE_SIZE_X/Y). The binning tile is
// this shifted left by macro_log2, which rides in every params struct here.
static const int TILE_MICRO_X = 8;
static const int TILE_MICRO_Y = 8;

// Binning tile geometry. The reciprocals are exact -- the edge is always a
// power of two -- so `* inv_x` matches the `/ TILE_SIZE_IX` this replaced bit
// for bit, and hoists the divide out of the per-row loops below.
struct BinTile {
    int ix, iy;
    float x, y;
    float inv_x, inv_y;
};

[ForceInline] BinTile bin_tile(uint macro_log2) {
    BinTile t;
    t.ix = TILE_MICRO_X << macro_log2;
    t.iy = TILE_MICRO_Y << macro_log2;
    t.x = float(t.ix);
    t.y = float(t.iy);
    t.inv_x = 1.0f / t.x;
    t.inv_y = 1.0f / t.y;
    return t;
}

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

// ellipse_range_bound from the covariance (xx, xy, yy) of x^T cov^-1 x = 1: its
// inverse loses the tips of a thin ellipse to rounding, and whole tile rows with
// them. Empty where the rows miss the ellipse.
float2 ellipse_range_bound_cov(float3 cov, float y0, float y1) {
    float hy = sqrt(cov.z);
    if (y0 > hy || y1 < -hy)
        return float2(1e9f, -1e9f);
    // The rightmost point is at height ym, the leftmost at -ym.
    float ym = cov.y / sqrt(cov.x);
    float v0 = clamp(clamp(-ym, y0, y1), -hy, hy);
    float v1 = clamp(clamp(ym, y0, y1), -hy, hy);
    float slope = cov.y / cov.z;
    float w2 = max(cov.x - cov.y * slope, 0.0f);
    return float2(slope * v0 - sqrt(max(w2 * (1.0f - v0 * v0 / cov.z), 0.0f)),
                  slope * v1 + sqrt(max(w2 * (1.0f - v1 * v1 / cov.z), 0.0f)));
}

[ForceInline]
float2 ellipse_range(float3 m, float y0, float y1, bool by_cov) {
    if (by_cov)
        return ellipse_range_bound_cov(m, y0, y1);
    return ellipse_range_bound(m, y0, y1);
}

// Count grid cells overlapping an ellipse: inv_cov, or its covariance (by_cov).
// Port of IntersectTile.cu count_ellipse_grid_overlaps.
int count_ellipse_grid_overlaps(
    float2 xy, float3 inv_cov, bool by_cov,
    int grid_xmin, int grid_xmax, int grid_ymin, int grid_ymax, bool by_rows,
    int* tile_active, gindex_t tile_base, uint tile_width, BinTile t
) {
    int n_tiles = 0;

    if (by_rows) {
        for (int y = grid_ymin; y < grid_ymax; y++) {
            float y0 = y * t.y - xy.y;
            float y1 = y0 + t.y;
            float2 bound = ellipse_range(inv_cov, y0, y1, by_cov);
            int x0 = int(floor((bound.x + xy.x) * t.inv_x));
            int x1 = int(ceil((bound.y + xy.x) * t.inv_x));
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
            float x0 = x * t.x - xy.x;
            float x1 = x0 + t.x;
            float2 bound = ellipse_range(inv_cov, x0, x1, by_cov);
            int y0 = int(floor((bound.x + xy.y) * t.inv_y));
            int y1 = int(ceil((bound.y + xy.y) * t.inv_y));
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

// Shared tile-range setup for both passes; matches the CUDA kernel body. The
// rows are cut to the band [row0, row1) of this image, AFTER the sweep
// direction is chosen on the whole box: a banded splat keeps its tiles.
void tile_range(float4 aabb, uint tile_width, uint tile_height, BinTile t,
                int row0, int row1,
                out uint2 tile_min, out uint2 tile_max, out bool by_rows,
                out bool single) {
    tile_min.x = uint(min(max(0, int(floor((aabb.x + 0.5f) * t.inv_x))),
                          int(tile_width)));
    tile_min.y = uint(min(max(0, int(floor((aabb.y + 0.5f) * t.inv_y))),
                          int(tile_height)));
    tile_max.x = uint(min(max(0, int(ceil((aabb.z + 0.5f) * t.inv_x))),
                          int(tile_width)));
    tile_max.y = uint(min(max(0, int(ceil((aabb.w + 0.5f) * t.inv_y))),
                          int(tile_height)));
    by_rows = int(tile_max.y - tile_min.y) * t.ix <=
              int(tile_max.x - tile_min.x) * t.iy;
    single = tile_max.x - tile_min.x == 1 && tile_max.y - tile_min.y == 1;
    int y0 = clamp(row0, int(tile_min.y), int(tile_max.y));
    int y1 = clamp(row1, y0, int(tile_max.y));
    tile_min.y = uint(y0);
    tile_max.y = uint(y1);
}

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
[SpecializationConstant]
const int kFootprint = 0;  // FootprintIO present: 1 gaussian, 2 surfel
[SpecializationConstant]
const int kDistortion = 0;  // tile_quad_rays only; last, see dist_spec.slang

// ---------------------------------------------------------------------------
// Exact eval3d footprints (docs/notes/footprint.md): a tile is kept when the
// splat's footprint reaches the quad of rays that bounds it.
// ---------------------------------------------------------------------------

// FootprintView (kernels/tile/IntersectTile.cuh) plus the tiles' ray quads.
struct FootprintIO {
    float* means;       // [N, 3] world
    float* quats;       // [N, 4]
    float* scales;      // [N, 3] log
    int* gaussian_ids;  // [total] (packed) or null (world id = idx % N)
    float* viewmats;    // [I, 16] row-major
    float4* intrins;    // [I]
    float* tile_quads;  // [I, tile_h, tile_w, 4, 3], tile_quad_rays; unread when planar
    float* block_quads; // [I, block_h, block_w, 4, 3], the same per block
    // footprint_planar_cov: 1 undistorted pinhole, 2 orthographic, whose
    // footprints are ellipses; 0 any other lens.
    uint32_t planar;
    uint32_t _pad0;
};

// The fragment the rasterizers cull with, and the camera it is seen from.
struct FootprintSplat {
    float3 mean;
    float3x3 M;      // iscl_rot
    float opacity;
    float3 o;        // camera position
    float3 slab;     // slab_misses_box, from the row's conic slot
    float3x3 R;      // world to camera, and
    float4 intrin;   // the intrinsics: an undistorted pinhole needs no ray quads
};

FootprintSplat footprint_load(FootprintIO fp, float* proj_screen, int row_stride,
                              int opac_off, int conic_off, uint idx, uint N, uint iid) {
    uint wi = kPacked != 0 ? uint(fp.gaussian_ids[idx]) : idx % N;
    float4 q = float4(fp.quats[4 * wi], fp.quats[4 * wi + 1], fp.quats[4 * wi + 2],
                      fp.quats[4 * wi + 3]);
    float3 sc = float3(fp.scales[3 * wi], fp.scales[3 * wi + 1], fp.scales[3 * wi + 2]);
    float* vm = fp.viewmats + iid * 16;
    float3x3 R;
    float3 t;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        t[r] = vm[4 * r + 3];
    }
    FootprintSplat s;
    s.mean = float3(fp.means[3 * wi], fp.means[3 * wi + 1], fp.means[3 * wi + 2]);
    s.M = kFootprint == 2 ? compute_2dgs_iscl_rot(q, sc) : compute_3dgut_iscl_rot(q, sc);
    float* row = proj_screen + row_stride * int(idx);
    s.opacity = row[opac_off];
    s.slab = float3(row[conic_off], row[conic_off + 1], row[conic_off + 2]);
    s.o = transform_ray_o(R, t);
    s.R = R;
    s.intrin = fp.intrins[iid];
    return s;
}

// A planar view's footprint as the ellipse the sweeps take, where it is one:
// its centre and covariance (footprint_planar_cov).
bool footprint_load_ellipse(FootprintIO fp, float* proj_screen, int row_stride,
                            int opac_off, uint idx, uint N, uint iid,
                            out float2 xy, out float3 cov) {
    xy = float2(0.0f);
    cov = float3(0.0f);
    if (fp.planar == 0)
        return false;
    uint wi = kPacked != 0 ? uint(fp.gaussian_ids[idx]) : idx % N;
    float* vm = fp.viewmats + iid * 16;
    float3x3 R;
    float3 t;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        t[r] = vm[4 * r + 3];
    }
    return footprint_planar_cov(
        float3(fp.means[3 * wi], fp.means[3 * wi + 1], fp.means[3 * wi + 2]),
        float4(fp.quats[4 * wi], fp.quats[4 * wi + 1], fp.quats[4 * wi + 2],
               fp.quats[4 * wi + 3]),
        float3(fp.scales[3 * wi], fp.scales[3 * wi + 1], fp.scales[3 * wi + 2]),
        (proj_screen + row_stride * int(idx))[opac_off], R, t, fp.intrins[iid],
        kFootprint == 2, int(fp.planar), xy, cov);
}

// A side of the blocks tiles are tested in first: one test settles a block the
// footprint misses or covers, which is most of a large splat's box.
static const int kIsectBlock = 4;

// footprint_rays_coverage of the pixel box [lo, hi], once the slab has had its
// say on it. quad: its rays from tile_quad_rays; an undistorted pinhole (planar)
// takes the rays of its corners instead, which bound it exactly.
int footprint_box(FootprintSplat s, float* quad, bool planar, float2 lo, float2 hi) {
    if (slab_misses_box(s.slab, 0.5f * (lo + hi), 0.5f * (hi - lo)))
        return 0;
    float3 q[4];
    if (planar) {
        float2 a = (lo - s.intrin.zw) / s.intrin.xy, b = (hi - s.intrin.zw) / s.intrin.xy;
        q[0] = transform_ray_d(s.R, float3(a.x, a.y, 1.0f));
        q[1] = transform_ray_d(s.R, float3(b.x, a.y, 1.0f));
        q[2] = transform_ray_d(s.R, float3(b.x, b.y, 1.0f));
        q[3] = transform_ray_d(s.R, float3(a.x, b.y, 1.0f));
    } else {
        [ForceUnroll]
        for (int k = 0; k < 4; k++)
            q[k] = float3(quad[3 * k], quad[3 * k + 1], quad[3 * k + 2]);
    }
    return footprint_rays_coverage(s.mean, s.M, s.opacity, kFootprint == 2, s.o,
                                   q[0], q[1], q[2], q[3]);
}

int footprint_tile(FootprintSplat s, float* tile_quads, bool planar, uint tile_width,
                   BinTile bt, int x, int y) {
    float2 lo = float2(float(x) * bt.x, float(y) * bt.y);
    return footprint_box(s, tile_quads + 12 * (y * int(tile_width) + x), planar, lo,
                         lo + float2(bt.x, bt.y));
}

// The tile range cut into blocks. Both passes walk it through these, so the
// write pass emits exactly what the count pass counted.
#define SS_FOOTPRINT_BLOCKS(tile_min, tile_max)                                   \
    for (int by = int(tile_min.y) & ~(kIsectBlock - 1); by < int(tile_max.y);     \
         by += kIsectBlock)                                                       \
        for (int bx = int(tile_min.x) & ~(kIsectBlock - 1); bx < int(tile_max.x); \
             bx += kIsectBlock)

// How the footprint meets the part of block (bx, by) inside the range. single:
// its box lies in one tile, which it then covers as far as binning goes. A block
// of four tiles or fewer goes straight to its tiles.
int footprint_block(FootprintSplat s, float* block_quads, bool planar, uint tile_width,
                    BinTile bt, uint2 tile_min, uint2 tile_max, bool single, int bx, int by,
                    out int4 box) {
    box = int4(max(bx, int(tile_min.x)), max(by, int(tile_min.y)),
               min(bx + kIsectBlock, int(tile_max.x)), min(by + kIsectBlock, int(tile_max.y)));
    if (single)
        return 2;
    if ((box.z - box.x) * (box.w - box.y) <= 4)
        return 1;
    int block_width = (int(tile_width) + kIsectBlock - 1) / kIsectBlock;
    return footprint_box(
        s, block_quads + 12 * ((by / kIsectBlock) * block_width + bx / kIsectBlock), planar,
        float2(float(box.x) * bt.x, float(box.y) * bt.y),
        float2(float(box.z) * bt.x, float(box.w) * bt.y));
}

struct TileQuadsParams {  // 64 bytes: pushed directly
    float* viewmats;      // [I, 16] row-major
    float4* intrins;      // [I]
    float* dist_coeffs;   // [I, 8] or null
    float* out_quads;     // [I, tile_h, tile_w, 4, 3], then [I, block_h, block_w, 4, 3]
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t I;
    uint32_t wgs_per_row;
    uint32_t macro_log2;
    uint32_t camera_model;
    uint32_t width;
    uint32_t height;
};

#define _tq_quad(D)                                                               \
    {                                                                             \
        float dist[D.kNumParams];                                                 \
        load_dist_coeffs<D>(p.dist_coeffs, iid, dist);                            \
        ok = pixel_rect_quad<D>(int(p.camera_model), p.intrins[iid], dist, lo, hi, \
                                q[0], q[1], q[2], q[3]);                          \
    }

// World-space rays bounding every tile, then every block of kIsectBlock tiles a
// side (pixel_rect_quad), cut to the image; zero where the lens model has none.
[shader("compute")]
[numthreads(256, 1, 1)]
void tile_quad_rays(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform TileQuadsParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    uint n_tile = p.I * p.tile_width * p.tile_height;
    uint block_w = (p.tile_width + kIsectBlock - 1) / kIsectBlock;
    uint block_h = (p.tile_height + kIsectBlock - 1) / kIsectBlock;
    if (idx >= n_tile + p.I * block_w * block_h) return;
    bool block = idx >= n_tile;
    uint quads_w = block ? block_w : p.tile_width;
    uint per_image = quads_w * (block ? block_h : p.tile_height);
    uint v = block ? idx - n_tile : idx;
    uint iid = v / per_image;
    v = v % per_image;
    float* vm = p.viewmats + iid * 16;
    float3x3 R;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
    float quad_px = bin_tile(p.macro_log2).x * (block ? float(kIsectBlock) : 1.0f);
    float2 lo = float2(float(v % quads_w), float(v / quads_w)) * quad_px;
    float2 hi = min(lo + quad_px, float2(float(p.width), float(p.height)));
    float3 q[4];
    bool ok = false;
    SS_DISPATCH_DIST(_tq_quad)
    [ForceUnroll]
    for (int k = 0; k < 4; k++) {
        float3 ray = ok ? transform_ray_d(R, q[k]) : float3(0.0f);
        p.out_quads[12 * idx + 3 * k + 0] = ray.x;
        p.out_quads[12 * idx + 3 * k + 1] = ray.y;
        p.out_quads[12 * idx + 3 * k + 2] = ray.z;
    }
}

// ---------------------------------------------------------------------------
// Counting pass: tiles_per_splat[idx] = number of tiles this splat touches.
// ---------------------------------------------------------------------------

struct IsectCountParams {  // through the params ring
    uint2* aabb;               // [total] packed, aabb16.slang
    float* proj_screen;        // packed screen rows; null -> AABB mode
    int* tiles_per_splat;      // [total]
    int* image_ids;            // [total] (packed) or null (iid = idx / N)
    int* tile_active;          // [I, tile_h, tile_w] 0/1; read when kSkipTiles
    uint* isect_total;         // [2]: (low word, carry) of the exact sum
    FootprintIO fp;            // read when kFootprint
    uint32_t N;                // splats per image
    uint32_t image_width;      // the AABB's quantization extent
    uint32_t image_height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t total;            // I * N (or nnz in packed mode)
    uint32_t wgs_per_row;
    uint32_t macro_log2;       // binning tile = TILE_MICRO_X << macro_log2
    // Row layout of proj_screen; xy_off < 0 -> center from the AABB.
    int32_t row_stride, xy_off, conic_off, opac_off;
    float max_sigma;  // ProjEllipseView::max_sigma
    // Rows [band_row0, band_row1) of the I * tile_height the images stack to.
    int32_t band_row0, band_row1;
    uint32_t _pad0;
};

groupshared uint _isect_wg_total;

[shader("compute")]
[numthreads(256, 1, 1)]
void intersect_tile_count(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform IsectCountParams* pp) {
    IsectCountParams p = *pp;
    if (tid == 0) _isect_wg_total = 0u;
    GroupMemoryBarrierWithGroupSync();

    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    int n = 0;
    if (idx < p.total) {
        uint2 aabb_q = p.aabb[idx];
        if (!aabb16_is_empty(aabb_q)) {
            float4 aabb =
                aabb16_decode(aabb_q, p.image_width, p.image_height);
            BinTile bt = bin_tile(p.macro_log2);
            gindex_t iid = kPacked != 0 ? gindex_t(p.image_ids[idx])
                                        : gindex_t(idx / p.N);
            gindex_t tile_base =
                iid * gindex_t(p.tile_width) * gindex_t(p.tile_height);
            int image_row = int(iid) * int(p.tile_height);
            uint2 tile_min, tile_max;
            bool by_rows, single;
            tile_range(aabb, p.tile_width, p.tile_height, bt,
                       p.band_row0 - image_row, p.band_row1 - image_row,
                       tile_min, tile_max, by_rows, single);

            float2 xy = float2(0.0f);
            float3 conic = float3(0.0f);
            bool sweep = kEllipse != 0;
            if (kFootprint != 0) {
                sweep = footprint_load_ellipse(p.fp, p.proj_screen, p.row_stride,
                                               p.opac_off, idx, p.N, uint(iid), xy, conic);
            } else if (kEllipse != 0) {
                float* row = p.proj_screen + p.row_stride * int(idx);
                xy = kHasXy != 0
                    ? float2(row[p.xy_off], row[p.xy_off + 1])
                    : float2(0.5f * (aabb.x + aabb.z),
                             0.5f * (aabb.y + aabb.w));
                conic = float3(row[p.conic_off], row[p.conic_off + 1],
                               row[p.conic_off + 2]);
                float reach = log(row[p.opac_off] / ALPHA_THRESHOLD);
                if (p.max_sigma > 0.0f) reach = min(reach, p.max_sigma);
                conic = conic * (0.5f / reach);
            }
            if (kFootprint != 0 && !sweep) {
                FootprintSplat fs = footprint_load(
                    p.fp, p.proj_screen, p.row_stride, p.opac_off, p.conic_off, idx, p.N,
                    uint(iid));
                int block_width = (int(p.tile_width) + kIsectBlock - 1) / kIsectBlock;
                int block_height = (int(p.tile_height) + kIsectBlock - 1) / kIsectBlock;
                float* tile_quads =
                    p.fp.tile_quads + 12 * int(iid) * int(p.tile_width * p.tile_height);
                float* block_quads =
                    p.fp.block_quads + 12 * int(iid) * block_width * block_height;
                bool planar = p.fp.planar != 0;
                SS_FOOTPRINT_BLOCKS(tile_min, tile_max) {
                    int4 box;
                    int cover = footprint_block(fs, block_quads, planar, p.tile_width, bt,
                                                tile_min, tile_max, single, bx, by, box);
                    if (cover == 0) continue;
                    for (int y = box.y; y < box.w; y++)
                        for (int x = box.x; x < box.z; x++) {
                            if (kSkipTiles != 0 &&
                                p.tile_active[tile_base + gindex_t(y) * p.tile_width +
                                              gindex_t(x)] == 0)
                                continue;
                            n += (cover == 2 || footprint_tile(fs, tile_quads, planar,
                                                               p.tile_width, bt, x, y) != 0)
                                     ? 1 : 0;
                        }
                }
            } else if (sweep) {
                n = count_ellipse_grid_overlaps(
                    xy, conic, kFootprint != 0,
                    int(tile_min.x), int(tile_max.x), int(tile_min.y),
                    int(tile_max.y), by_rows, p.tile_active, tile_base,
                    p.tile_width, bt);
            } else if (kSkipTiles == 0) {
                n = max(int(tile_max.y - tile_min.y) *
                        int(tile_max.x - tile_min.x), 0);
            } else {
                for (uint i = tile_min.y; i < tile_max.y; ++i)
                    for (uint j = tile_min.x; j < tile_max.x; ++j)
                        n += p.tile_active[tile_base +
                                           gindex_t(i) * p.tile_width +
                                           gindex_t(j)] != 0 ? 1 : 0;
            }
        }
        p.tiles_per_splat[idx] = n;
    }

    // The int32 scan downstream cannot report a total it has already wrapped
    // through, so carry the exact 64-bit sum out in two words. A workgroup
    // holds at most 256 * (tile grid) tiles, which stays inside uint32.
    if (n > 0) InterlockedAdd(_isect_wg_total, uint(n));
    GroupMemoryBarrierWithGroupSync();
    if (tid == 0 && _isect_wg_total != 0u) {
        uint prev;
        InterlockedAdd(p.isect_total[0], _isect_wg_total, prev);
        if (prev + _isect_wg_total < prev)
            InterlockedAdd(p.isect_total[1], 1u);
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
    uint32_t macro_log2;
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
    BinTile bt = bin_tile(p.macro_log2);
    for (int dy = -kMaskPad; dy < bt.iy + kMaskPad && !live; ++dy) {
        int y = int(ty) * bt.iy + dy;
        if (y >= p.height) break;
        if (y < 0) continue;
        for (int dx = -kMaskPad; dx < bt.ix + kMaskPad; ++dx) {
            int x = int(tx) * bt.ix + dx;
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

struct IsectWriteParams {  // through the params ring
    int* image_ids;      // [total] (packed mode) or null (iid = idx / N)
    uint2* aabb;         // [total] packed, aabb16.slang
    float* depths;       // [total] sorting depths
    float* proj_screen;  // packed screen rows; null -> AABB mode
    int* cum_tiles;      // [total] inclusive scan of tiles_per_splat
    u64key_t* isect_ids;  // [n_isects]
    int* flatten_ids;    // [n_isects]
    int* tile_active;    // [I, tile_h, tile_w] 0/1; read when kSkipTiles
    FootprintIO fp;      // read when kFootprint
    uint32_t N;          // splats per image (non-packed iid = idx / N)
    uint32_t image_width;      // the AABB's quantization extent
    uint32_t image_height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t total;
    uint32_t wgs_per_row;
    uint32_t n_isects;   // isect_ids / flatten_ids length
    uint32_t macro_log2;
    int32_t row_stride, xy_off, conic_off, opac_off;
    float max_sigma;  // ProjEllipseView::max_sigma
    // The band's rows, as in the count pass, and the tile its keys count from.
    int32_t band_row0, band_row1;
    uint32_t key_tile_base;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void intersect_tile_write(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform IsectWriteParams* pp) {
    IsectWriteParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;

    gindex_t iid = kPacked != 0 ? gindex_t(p.image_ids[idx])
                                         : gindex_t(idx / p.N);
    // Clamped both ends: the counts are exact now, but an out-of-range
    // window here would scatter raw stores across device memory.
    gindex_t cur_idx =
        (idx == 0) ? 0 : max(gindex_t(p.cum_tiles[idx - 1]), gindex_t(0));
    gindex_t max_idx = min(gindex_t(p.cum_tiles[idx]), gindex_t(p.n_isects));

    uint2 aabb_q = p.aabb[idx];
    float4 aabb = aabb16_decode(aabb_q, p.image_width, p.image_height);
    gindex_t n_img_tiles = gindex_t(p.tile_width) * gindex_t(p.tile_height);
    gindex_t key_base = gindex_t(p.key_tile_base);
    // Where a window the count over-estimated is padded: this image's first
    // tile, or the band's when that comes later.
    gindex_t pad_tile = max(iid * n_img_tiles, key_base) - key_base;
    if (aabb16_is_empty(aabb_q)) {
        // cur_idx should equal max_idx; fill just in case floating point
        // optimization goes wild (matches the CUDA kernel).
        while (cur_idx < max_idx) {
            elem_at(p.isect_ids, cur_idx)[0] = u64key_make(pad_tile, 0xffffffffu);
            elem_at(p.flatten_ids, cur_idx)[0] = int(idx);
            ++cur_idx;
        }
        return;
    }

    BinTile bt = bin_tile(p.macro_log2);
    int image_row = int(iid) * int(p.tile_height);
    uint2 tile_min, tile_max;
    bool by_rows, single;
    tile_range(aabb, p.tile_width, p.tile_height, bt,
               p.band_row0 - image_row, p.band_row1 - image_row,
               tile_min, tile_max, by_rows, single);

    // Sorting depth -> order-preserving u32 (same bit trick as CUDA).
    float depth_f32 = abs(p.depths[idx]);
    uint depth_u32 = asuint(depth_f32);
    if ((depth_u32 >> 31) != 0)  // negative
        depth_u32 = ~depth_u32;
    else  // positive
        depth_u32 ^= (1u << 31);

    float2 xy = float2(0.0f);
    float3 conic = float3(0.0f);
    bool sweep = kEllipse != 0;
    if (kFootprint != 0) {
        sweep = footprint_load_ellipse(p.fp, p.proj_screen, p.row_stride, p.opac_off,
                                       idx, p.N, uint(iid), xy, conic);
    } else if (kEllipse != 0) {
        float* row = p.proj_screen + p.row_stride * int(idx);
        xy = kHasXy != 0
            ? float2(row[p.xy_off], row[p.xy_off + 1])
            : float2(0.5f * (aabb.x + aabb.z), 0.5f * (aabb.y + aabb.w));
        conic = float3(row[p.conic_off], row[p.conic_off + 1], row[p.conic_off + 2]);
        float reach = log(row[p.opac_off] / ALPHA_THRESHOLD);
        if (p.max_sigma > 0.0f) reach = min(reach, p.max_sigma);
        conic = conic * (0.5f / reach);
    }
    if (kFootprint != 0 && !sweep) {
        FootprintSplat fs = footprint_load(
            p.fp, p.proj_screen, p.row_stride, p.opac_off, p.conic_off, idx, p.N, uint(iid));
        int block_width = (int(p.tile_width) + kIsectBlock - 1) / kIsectBlock;
        int block_height = (int(p.tile_height) + kIsectBlock - 1) / kIsectBlock;
        float* tile_quads =
            p.fp.tile_quads + 12 * int(iid) * int(p.tile_width * p.tile_height);
        float* block_quads = p.fp.block_quads + 12 * int(iid) * block_width * block_height;
        bool planar = p.fp.planar != 0;
        SS_FOOTPRINT_BLOCKS(tile_min, tile_max) {
            int4 box;
            int cover = footprint_block(fs, block_quads, planar, p.tile_width, bt, tile_min,
                                        tile_max, single, bx, by, box);
            if (cover == 0) continue;
            for (int y = box.y; y < box.w; y++) {
                for (int x = box.x; x < box.z && cur_idx < max_idx; x++) {
                    gindex_t tile_id = iid * n_img_tiles +
                                      gindex_t(y) * gindex_t(p.tile_width) + gindex_t(x);
                    if (kSkipTiles != 0 && p.tile_active[tile_id] == 0) continue;
                    if (cover != 2 && footprint_tile(fs, tile_quads, planar, p.tile_width,
                                                     bt, x, y) == 0)
                        continue;
                    elem_at(p.isect_ids, cur_idx)[0] =
                        u64key_make(tile_id - key_base, depth_u32);
                    elem_at(p.flatten_ids, cur_idx)[0] = int(idx);
                    ++cur_idx;
                }
            }
        }
    } else if (sweep) {
        if (by_rows) {
            for (int y = int(tile_min.y); y < int(tile_max.y); y++) {
                float y0 = y * bt.y - xy.y;
                float y1 = y0 + bt.y;
                float2 bound = ellipse_range(conic, y0, y1, kFootprint != 0);
                int min_x = int(clamp(uint(floor((bound.x + xy.x) * bt.inv_x)),
                                      tile_min.x, tile_max.x));
                int max_x = int(clamp(uint(ceil((bound.y + xy.x) * bt.inv_x)),
                                      tile_min.x, tile_max.x));
                for (int x = min_x; x < max_x && cur_idx < max_idx; x++) {
                    gindex_t tile_id = iid * n_img_tiles +
                                      gindex_t(y) * gindex_t(p.tile_width) +
                                      gindex_t(x);
                    if (kSkipTiles != 0 && p.tile_active[tile_id] == 0) continue;
                    elem_at(p.isect_ids, cur_idx)[0] =
                        u64key_make(tile_id - key_base, depth_u32);
                    elem_at(p.flatten_ids, cur_idx)[0] = int(idx);
                    ++cur_idx;
                }
            }
        } else {
            conic = float3(conic.z, conic.y, conic.x);
            for (int x = int(tile_min.x); x < int(tile_max.x); x++) {
                float x0 = x * bt.x - xy.x;
                float x1 = x0 + bt.x;
                float2 bound = ellipse_range(conic, x0, x1, kFootprint != 0);
                int min_y = int(clamp(uint(floor((bound.x + xy.y) * bt.inv_y)),
                                      tile_min.y, tile_max.y));
                int max_y = int(clamp(uint(ceil((bound.y + xy.y) * bt.inv_y)),
                                      tile_min.y, tile_max.y));
                for (int y = min_y; y < max_y && cur_idx < max_idx; y++) {
                    gindex_t tile_id = iid * n_img_tiles +
                                      gindex_t(y) * gindex_t(p.tile_width) +
                                      gindex_t(x);
                    if (kSkipTiles != 0 && p.tile_active[tile_id] == 0) continue;
                    elem_at(p.isect_ids, cur_idx)[0] =
                        u64key_make(tile_id - key_base, depth_u32);
                    elem_at(p.flatten_ids, cur_idx)[0] = int(idx);
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
                elem_at(p.isect_ids, cur_idx)[0] =
                    u64key_make(tile_id - key_base, depth_u32);
                elem_at(p.flatten_ids, cur_idx)[0] = int(idx);
                ++cur_idx;
            }
        }
    }
    // Don't leave invalid IDs behind if the count under-estimated.
    while (cur_idx < max_idx) {
        elem_at(p.isect_ids, cur_idx)[0] = u64key_make(pad_tile, 0xffffffffu);
        elem_at(p.flatten_ids, cur_idx)[0] = int(idx);
        ++cur_idx;
    }
}

// ---------------------------------------------------------------------------
// Per-tile start offsets over the sorted keys.
// ---------------------------------------------------------------------------

struct IsectOffsetParams {  // 32 bytes: pushed directly
    u64key_t* isect_ids;  // [n_isects] sorted, tiles counted from the band's
    int* offsets;        // [n_offsets], the band's tiles
    uint32_t n_isects;   // 0: one thread fills every offset with isect_base
    uint32_t n_offsets;
    uint32_t wgs_per_row;
    uint32_t isect_base;  // the band's first index in the whole list
};

[shader("compute")]
[numthreads(256, 1, 1)]
void intersect_offset(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform IsectOffsetParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (p.n_isects == 0) {
        if (idx == 0)
            for (uint i = 0; i < p.n_offsets; ++i)
                p.offsets[i] = int(p.isect_base);
        return;
    }
    if (idx >= p.n_isects) return;

    gindex_t tile_id = u64key_hi(elem_at(p.isect_ids, gindex_t(idx))[0]);

    if (idx == 0) {
        // write out the offsets until the first valid tile (inclusive)
        for (uint i = 0; i < uint(tile_id) + 1; ++i)
            p.offsets[i] = int(p.isect_base + idx);
    }
    if (idx == p.n_isects - 1) {
        // write out the rest of the offsets
        for (uint i = uint(tile_id) + 1; i < p.n_offsets; ++i)
            p.offsets[i] = int(p.isect_base + p.n_isects);
    }

    if (idx > 0) {
        gindex_t tile_id_prev = u64key_hi(elem_at(p.isect_ids, gindex_t(idx) - 1)[0]);
        if (tile_id_prev == tile_id)
            return;
        // write out the offsets between the previous and current tiles
        for (uint i = uint(tile_id_prev) + 1; i < uint(tile_id) + 1; ++i)
            p.offsets[i] = int(p.isect_base + idx);
    }
}
