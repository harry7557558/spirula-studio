// Tile rasterization forward (Vulkan backend), closely following
// kernels/raster/RasterizationEval3DFwd_kernel.cuh (which also generates the 2D
// forward via IS_EVAL3D=0; here the two are separate entries because their
// groupshared fragment types differ). One workgroup per micro-tile
// (TILE_SIZE_X x TILE_SIZE_Y pixels, one thread per pixel); binning is at
// macro-tile granularity, so a workgroup reads its macro tile's gaussian
// range and culls the ones that miss its micro-tile.
//
// Variants (specialization constants, matching the CUDA template axes):
//   kCameraModel  = CameraModelType 0..3 (eval3d entry only)
//   kDistType     = 0 None | 1 D | 2 RGB_D (the instantiated subset)
//   kOutputMedian = 0 | 1
//   kDistortion   = CameraDistortionType 0..3 (eval3d entry only), last
// The primitive pixel type is RGB_D for all current primitives, so the
// render output channels are hard-wired to rgb + depth.
//
// CUDA's __syncthreads_count(done) early-out becomes a monotonic groupshared
// counter: each thread adds 1 exactly when it transitions to done, and the
// loop-top barrier (which CUDA also needs, to protect the shared batch)
// makes the count visible.
//
// The 64-thread workgroup is a multiple of every pinned subgroup size on
// known hardware (8..64; see README subgroup rules).

#define CudaDeviceExport ForceInline
#include "aabb16.slang"
#include "int64_compat.slang"
#include "screen.slang"
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport

#include "raster_common.slang"
#include "dist_spec.slang"

[SpecializationConstant]
const int kCameraModel = 0;
[SpecializationConstant]
const int kDistType = 0;
[SpecializationConstant]
const int kOutputMedian = 0;
// gaussian_ids present (packed projection); spec-folded so the null pointer
// never sits behind a runtime branch (see backend README).
[SpecializationConstant]
const int kRasterPacked = 0;

groupshared uint _hit[TILE_AREA];
groupshared uint _done_count;

// ---------------------------------------------------------------------------
// 2D splatting (Vanilla3DGS / MipSplatting share the fragment; the
// antialiasing difference lives entirely in projection).
// ---------------------------------------------------------------------------

// _BasePrimitive3DGS Screen (the 2D FragmentFwd is the screen data).
struct Frag2d {
    float2 xy;
    float depth;
    float3 conic;
    float opac;
    float3 rgb;
};

groupshared Frag2d _batch_2d[TILE_AREA];

struct RasterFwd2dParams {
    // Packed ScreenBuffer rows, indexed by the flatten id (the 2D fragment
    // never touches the world buffer).
    float* s_screen;
    int* tile_offsets;  // [I, tile_height, tile_width]
    int* flatten_ids;   // [n_isects]
    // outputs (RGB_D pixel type)
    float* out_rgb;      // [I,H,W,3]
    float* out_depth;    // [I,H,W]
    float* out_T;        // [I,H,W]
    int* out_last_ids;   // [I,H,W]
    float* dist_rgb;     // [I,H,W,3], kDistType == 2 only
    float* dist_depth;   // [I,H,W],   kDistType != 0 only
    float* out_median;   // [I,H,W],   kOutputMedian only
    uint32_t I;
    uint32_t n_isects;
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t macro_log2;
};

[shader("compute")]
[numthreads(64, 1, 1)]
void rasterize_fwd_2d(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform RasterFwd2dParams p) {
    uint image_id = wg.x;
    // one workgroup per micro-tile; recover its macro tile for binning
    uint mt_y = wg.y;
    uint mt_x = wg.z;
    int tile_id = int(macro_tile_id(mt_y, mt_x, p.tile_width, p.macro_log2));

    int* tile_offsets =
        p.tile_offsets + image_id * p.tile_height * p.tile_width;

    // this thread's pixel within the micro-tile
    uint i = mt_y * TILE_SIZE_Y + tid / TILE_SIZE_X;
    uint j = mt_x * TILE_SIZE_X + tid % TILE_SIZE_X;
    float px = float(j) + 0.5f;
    float py = float(i) + 0.5f;

    bool inside = (i < p.height && j < p.width);
    bool done = !inside;

    uint pix_id_global = image_id * p.height * p.width + i * p.width + j;
    float T = 1.0f;
    uint cur_idx = 0;

    float median_depth = 0.0f;
    float median_prev_z = 0.0f;
    float median_prev_T = 1.0f;

    // C = sum_j w_j c_j (raw) and S = sum_j w_j c_j^2 (RGB_D channels)
    float3 pix_rgb = float3(0.0f);
    float pix_depth = 0.0f;
    float3 pix2_rgb = float3(0.0f);
    float pix2_depth = 0.0f;

    // gaussians overlapping this micro-tile's macro tile
    int range_start = tile_offsets[tile_id];
    int range_end =
        (image_id == p.I - 1 &&
         tile_id == int(p.tile_width * p.tile_height) - 1)
            ? int(p.n_isects)
            : tile_offsets[tile_id + 1];
    uint num_batches =
        uint(max(range_end - range_start, 0) + int(TILE_AREA) - 1) /
        TILE_AREA;

    // micro-tile pixel box, for the footprint cull
    float mt_bx0 = float(mt_x * TILE_SIZE_X);
    float mt_by0 = float(mt_y * TILE_SIZE_Y);

    if (tid == 0)
        _done_count = 0;
    GroupMemoryBarrierWithGroupSync();
    if (done)
        InterlockedAdd(_done_count, 1);

    // collect and process batches of gaussians, one thread loads one gaussian
    for (uint b = 0; b < num_batches; ++b) {
        // resync all threads before the next batch overwrites shared, and
        // stop once every pixel in the workgroup is done (the barrier
        // publishes the done-count adds; the count is uniform so the break
        // is divergence-free)
        GroupMemoryBarrierWithGroupSync();
        if (_done_count >= TILE_AREA)
            break;

        int batch_start = range_start + int(TILE_AREA * b);
        int idx = batch_start + int(tid);
        bool hit = false;
        if (idx < range_end) {
            int g = p.flatten_ids[idx];  // flatten index in [I * N] or [nnz]
            // 2D splatting: the fragment is just the screen data, so load it
            // (cheap) and cull from its own conic / opacity.
            Frag2d f;
            float* r = scr2_row(p.s_screen, g);
            f.xy = scr2_xy(r);
            f.depth = scr2_depth(r);
            f.conic = scr2_conic(r);
            f.opac = scr2_opac(r);
            f.rgb = scr2_rgb(r);
            _batch_2d[tid] = f;
            if (f.opac > ALPHA_THRESHOLD) {
                float kk = 0.5f / log(f.opac / ALPHA_THRESHOLD);
                float3 inv_cov = f.conic * kk;
                hit = ellipse_box_overlap_test(
                    inv_cov, mt_bx0 - f.xy.x,
                    mt_bx0 + float(TILE_SIZE_X) - f.xy.x, mt_by0 - f.xy.y,
                    mt_by0 + float(TILE_SIZE_Y) - f.xy.y);
            }
        }
        _hit[tid] = hit ? 1u : 0u;

        // wait for other threads to collect the gaussians in the batch
        GroupMemoryBarrierWithGroupSync();

        // process gaussians in the current batch for this pixel
        uint batch_size = min(TILE_AREA, uint(range_end - batch_start));
        for (uint t = 0; t < batch_size; ++t) {
            if (_hit[t] == 0u)  // gaussian misses this micro-tile
                continue;
            if (done)
                continue;
            Frag2d f = _batch_2d[t];

            // FragmentFwd::evaluate_alpha(px, py)
            float2 delta = float2(f.xy.x - px, f.xy.y - py);
            float sigma = 0.5f * (f.conic.x * delta.x * delta.x +
                                  f.conic.z * delta.y * delta.y) +
                          f.conic.y * delta.x * delta.y;
            float vis0 = exp(-sigma);
            float alpha = min(0.999f, f.opac * vis0);
            if (sigma < 0.f)
                alpha = 0.f;
            if (alpha <= ALPHA_THRESHOLD)
                continue;

            // FragmentFwd::evaluate_color(px, py)
            float3 color_rgb = f.rgb;
            float color_depth = f.depth;

            float next_T = T * (1.0f - alpha);

            // median depth: detect the (unique) post-T crossing of 1/2,
            // z interpolated in ln(T)
            if (kOutputMedian != 0) {
                if (median_prev_T >= 0.5f && next_T < 0.5f) {
                    float a = log(median_prev_T);
                    float bb = log(next_T);
                    float c = -0.6931471805599453f;  // ln(1/2)
                    float d = bb - a;
                    float fr = (abs(d) > 1e-20f) ? (c - a) / d : 0.0f;
                    median_depth =
                        median_prev_z + (color_depth - median_prev_z) * fr;
                }
                median_prev_T = next_T;
                median_prev_z = color_depth;
            }

            if (next_T > 1e-4f) {
                float vis = alpha * T;
                // depth distortion accumulates in log space; the rendered
                // ray depth is recovered as exp(expected log depth) after
                // the loop
                float acc_depth = color_depth;
                if (kDistType != 0)
                    acc_depth = log(max(color_depth, DEPTH_DIST_EPS));
                if (kDistType != 0) {
                    pix2_rgb += color_rgb * color_rgb * vis;
                    pix2_depth += acc_depth * acc_depth * vis;
                }
                pix_rgb += color_rgb * vis;
                pix_depth += acc_depth * vis;
                cur_idx = uint(batch_start) + t;
                T = next_T;
            } else {
                done = true;
                InterlockedAdd(_done_count, 1);
            }
        }
    }

    if (i < p.height && j < p.width) {
        p.out_T[pix_id_global] = T;
        if (kOutputMedian != 0)
            p.out_median[pix_id_global] = median_depth;
        // distortion: closed form D = W*S - C^2, evaluated before the depth
        // channel of C is normalized below
        if (kDistType != 0) {
            float W = 1.0f - T;
            if (kDistType == 2) {
                float3 dr = pix2_rgb * W - pix_rgb * pix_rgb;
                p.dist_rgb[3 * pix_id_global + 0] = dr.x;
                p.dist_rgb[3 * pix_id_global + 1] = dr.y;
                p.dist_rgb[3 * pix_id_global + 2] = dr.z;
            }
            p.dist_depth[pix_id_global] =
                pix2_depth * W - pix_depth * pix_depth;
        }
        float invW = 1.0f / max(1.0f - T, 1e-10f);
        float out_depth = (kDistType != 0) ? exp(pix_depth * invW)
                                           : pix_depth * invW;
        p.out_rgb[3 * pix_id_global + 0] = pix_rgb.x;
        p.out_rgb[3 * pix_id_global + 1] = pix_rgb.y;
        p.out_rgb[3 * pix_id_global + 2] = pix_rgb.z;
        p.out_depth[pix_id_global] = out_depth;
        // index in bin of last gaussian in this pixel
        p.out_last_ids[pix_id_global] = int(cur_idx);
    }
}

// ---------------------------------------------------------------------------
// eval3D (Vanilla3DGUT): per-pixel rays; the projected 2D conic lives in the
// screen "scale" slot and the ellipse center is the AABB center.
// ---------------------------------------------------------------------------

// Vanilla3DGUT::FragmentFwd
struct Frag3dgut {
    float3 mean;
    float3x3 iscl_rot;
    float opacity;
    float3 rgb;
};

groupshared Frag3dgut _batch_ut[TILE_AREA];

struct RasterFwd3dgutParams {
    // WorldBuffer members used by the fragment
    float* means;   // [N, 3]
    float* quats;   // [N, 4]
    float* scales;  // [N, 3]
    int* gaussian_ids;  // [nnz] packed mode or null (world idx = g % N)
    float* s_screen;  // packed ScreenBuffer rows; "scale" slot = 2D conic
    // cameras
    float* viewmats;     // [I, 16] row-major
    float4* intrins;     // [I]
    float* dist_coeffs;  // [I, 8] or null
    uint2* aabb;         // [..] packed 2D AABB, aabb16.slang
    int* tile_offsets;   // [I, tile_height, tile_width]
    int* flatten_ids;    // [n_isects]
    // outputs (RGB_D pixel type)
    float* out_rgb;
    float* out_depth;
    float* out_T;
    int* out_last_ids;
    float* dist_rgb;
    float* dist_depth;
    float* out_median;
    uint32_t I;
    uint32_t N;  // zero if packed
    uint32_t n_isects;
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t macro_log2;
};

#define _rf_ray(D)                                                    \
    ray_ok = pixel_ray<D>(p.dist_coeffs, image_id, kCameraModel,      \
                          float2((px - intrin.z) / intrin.x,          \
                                 (py - intrin.w) / intrin.y), raydir)

[shader("compute")]
[numthreads(64, 1, 1)]
void rasterize_fwd_3dgut(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform RasterFwd3dgutParams* pp) {
    RasterFwd3dgutParams p = *pp;
    uint image_id = wg.x;
    uint mt_y = wg.y;
    uint mt_x = wg.z;
    int tile_id = int(macro_tile_id(mt_y, mt_x, p.tile_width, p.macro_log2));

    int* tile_offsets =
        p.tile_offsets + image_id * p.tile_height * p.tile_width;

    // Load camera
    float* vm = p.viewmats + image_id * 16;
    float3x3 R;
    float3 ct;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        ct[r] = vm[4 * r + 3];
    }
    float4 intrin = p.intrins[image_id];

    float3 ray_o = transform_ray_o(R, ct);

    uint i = mt_y * TILE_SIZE_Y + tid / TILE_SIZE_X;
    uint j = mt_x * TILE_SIZE_X + tid % TILE_SIZE_X;
    float px = float(j) + 0.5f;
    float py = float(i) + 0.5f;

    bool inside = (i < p.height && j < p.width);

    float3 raydir = float3(0.0f);
    bool ray_ok = false;
    SS_DISPATCH_DIST(_rf_ray)
    inside = inside && ray_ok;
    float3 ray_d = transform_ray_d(R, raydir);

    bool done = !inside;

    uint pix_id_global = image_id * p.height * p.width + i * p.width + j;
    float T = 1.0f;
    uint cur_idx = 0;

    float median_depth = 0.0f;
    float median_prev_z = 0.0f;
    float median_prev_T = 1.0f;

    float3 pix_rgb = float3(0.0f);
    float pix_depth = 0.0f;
    float3 pix2_rgb = float3(0.0f);
    float pix2_depth = 0.0f;

    int range_start = tile_offsets[tile_id];
    int range_end =
        (image_id == p.I - 1 &&
         tile_id == int(p.tile_width * p.tile_height) - 1)
            ? int(p.n_isects)
            : tile_offsets[tile_id + 1];
    uint num_batches =
        uint(max(range_end - range_start, 0) + int(TILE_AREA) - 1) /
        TILE_AREA;

    float mt_bx0 = float(mt_x * TILE_SIZE_X);
    float mt_by0 = float(mt_y * TILE_SIZE_Y);

    if (tid == 0)
        _done_count = 0;
    GroupMemoryBarrierWithGroupSync();
    if (done)
        InterlockedAdd(_done_count, 1);

    for (uint b = 0; b < num_batches; ++b) {
        GroupMemoryBarrierWithGroupSync();
        if (_done_count >= TILE_AREA)
            break;

        int batch_start = range_start + int(TILE_AREA * b);
        int idx = batch_start + int(tid);
        bool hit = false;
        if (idx < range_end) {
            int g = p.flatten_ids[idx];
            uint wi = kRasterPacked != 0 ? uint(p.gaussian_ids[g])
                                                : uint(g) % p.N;
            // 3dgut keeps the projected 2D conic in the screen "scale"
            // channel and the ellipse center is the AABB center. Cull here,
            // before the (expensive) 3D fragment load, matching the tile
            // intersector.
            float* r = scrg_row(p.s_screen, g);
            float opac = scrg_opac(r);
            if (opac > ALPHA_THRESHOLD) {
                float3 conic = scrg_scale(r);
                float2 ec = aabb16_center(p.aabb[g], p.width, p.height);
                float ecx = ec.x, ecy = ec.y;
                float kk = 0.5f / log(opac / ALPHA_THRESHOLD);
                float3 inv_cov = conic * kk;
                hit = ellipse_box_overlap_test(
                    inv_cov, mt_bx0 - ecx,
                    mt_bx0 + float(TILE_SIZE_X) - ecx, mt_by0 - ecy,
                    mt_by0 + float(TILE_SIZE_Y) - ecy);
            }
            if (hit) {
                // FragmentFwd::load
                Frag3dgut f;
                f.mean = float3(p.means[3 * wi], p.means[3 * wi + 1],
                                p.means[3 * wi + 2]);
                f.iscl_rot = compute_3dgut_iscl_rot(
                    float4(p.quats[4 * wi], p.quats[4 * wi + 1],
                           p.quats[4 * wi + 2], p.quats[4 * wi + 3]),
                    float3(p.scales[3 * wi], p.scales[3 * wi + 1],
                           p.scales[3 * wi + 2]));
                f.opacity = opac;
                f.rgb = scrg_rgb(r);
                _batch_ut[tid] = f;
            }
        }
        _hit[tid] = hit ? 1u : 0u;

        GroupMemoryBarrierWithGroupSync();

        uint batch_size = min(TILE_AREA, uint(range_end - batch_start));
        for (uint t = 0; t < batch_size; ++t) {
            if (_hit[t] == 0u)
                continue;
            if (done)
                continue;
            Frag3dgut f = _batch_ut[t];

            float alpha = evaluate_alpha_3dgs(f.mean, f.iscl_rot, f.opacity,
                                              ray_o, ray_d);
            if (alpha <= ALPHA_THRESHOLD)
                continue;

            float3 color_rgb;
            float color_depth;
            evaluate_color_3dgs(f.mean, f.iscl_rot, f.opacity, f.rgb, ray_o,
                                ray_d, color_rgb, color_depth);
            if (color_depth <= 0.0f)
                continue;

            float next_T = T * (1.0f - alpha);

            if (kOutputMedian != 0) {
                if (median_prev_T >= 0.5f && next_T < 0.5f) {
                    float a = log(median_prev_T);
                    float bb = log(next_T);
                    float c = -0.6931471805599453f;  // ln(1/2)
                    float d = bb - a;
                    float fr = (abs(d) > 1e-20f) ? (c - a) / d : 0.0f;
                    median_depth =
                        median_prev_z + (color_depth - median_prev_z) * fr;
                }
                median_prev_T = next_T;
                median_prev_z = color_depth;
            }

            if (next_T > 1e-4f) {
                float vis = alpha * T;
                float acc_depth = color_depth;
                if (kDistType != 0)
                    acc_depth = log(max(color_depth, DEPTH_DIST_EPS));
                if (kDistType != 0) {
                    pix2_rgb += color_rgb * color_rgb * vis;
                    pix2_depth += acc_depth * acc_depth * vis;
                }
                pix_rgb += color_rgb * vis;
                pix_depth += acc_depth * vis;
                cur_idx = uint(batch_start) + t;
                T = next_T;
            } else {
                done = true;
                InterlockedAdd(_done_count, 1);
            }
        }
    }

    if (i < p.height && j < p.width) {
        p.out_T[pix_id_global] = T;
        if (kOutputMedian != 0)
            p.out_median[pix_id_global] = median_depth;
        if (kDistType != 0) {
            float W = 1.0f - T;
            if (kDistType == 2) {
                float3 dr = pix2_rgb * W - pix_rgb * pix_rgb;
                p.dist_rgb[3 * pix_id_global + 0] = dr.x;
                p.dist_rgb[3 * pix_id_global + 1] = dr.y;
                p.dist_rgb[3 * pix_id_global + 2] = dr.z;
            }
            p.dist_depth[pix_id_global] =
                pix2_depth * W - pix_depth * pix_depth;
        }
        float invW = 1.0f / max(1.0f - T, 1e-10f);
        float out_depth = (kDistType != 0) ? exp(pix_depth * invW)
                                           : pix_depth * invW;
        p.out_rgb[3 * pix_id_global + 0] = pix_rgb.x;
        p.out_rgb[3 * pix_id_global + 1] = pix_rgb.y;
        p.out_rgb[3 * pix_id_global + 2] = pix_rgb.z;
        p.out_depth[pix_id_global] = out_depth;
        p.out_last_ids[pix_id_global] = int(cur_idx);
    }
}

#undef _rf_ray

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
