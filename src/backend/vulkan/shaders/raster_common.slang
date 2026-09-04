// Tile-geometry constants and the ellipse/box culling test shared by the
// rasterization forward (rasterize_fwd.slang) and backward
// (rasterize_bwd.slang). Values mirror Common.cuh — binning happens at
// macro-tile granularity, kernels work on micro tiles.

static const uint TILE_SIZE_X = 8;
static const uint TILE_SIZE_Y = 8;
static const uint TILE_AREA = TILE_SIZE_X * TILE_SIZE_Y;

// Which macro tile's splat list this micro tile walks. macro_log2 rides in
// the params buffer rather than being a constant, so the binning tile is
// retunable without respecializing any pipeline (core/Common.cuh).
[ForceInline] uint macro_tile_id(uint mt_y, uint mt_x, uint tile_width,
                                 uint macro_log2) {
    return ((mt_y >> macro_log2) * tile_width) + (mt_x >> macro_log2);
}

// ALPHA_THRESHOLD comes from projection_utils.slang.
static const float DEPTH_DIST_EPS = 1e-6f;

// Test whether the unit ellipse (x^T inv_cov x = 1, centered at origin)
// overlaps an axis-aligned box [x0,x1]x[y0,y1]. Port of Common.cuh
// ellipse_box_overlap_test.
bool ellipse_box_overlap_test(float3 inv_cov, float x0, float x1, float y0,
                              float y1) {
    float a = inv_cov.x, b = inv_cov.y, c = inv_cov.z;
    // parabola vertex on the 4 edges (uses the un-doubled off-diagonal)
    float wx = -b / c, wy = -b / a;
    float u0 = min(max(x0 * wx, y0), y1);
    float u1 = min(max(x1 * wx, y0), y1);
    float v0 = min(max(y0 * wy, x0), x1);
    float v1 = min(max(y1 * wy, x0), x1);
    b *= 2.0f;
    float mx = min(a * x0 * x0 + b * x0 * u0 + c * u0 * u0,
                   a * x1 * x1 + b * x1 * u1 + c * u1 * u1);
    float my = min(a * v0 * v0 + b * v0 * y0 + c * y0 * y0,
                   a * v1 * v1 + b * v1 * y1 + c * y1 * y1);
    // box contains the ellipse center, or the box boundary meets the ellipse
    float mc = max(max(x0, -x1), max(y0, -y1));
    return min(mc, min(mx, my) - 1.0f) < 0.f;
}

