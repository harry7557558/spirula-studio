// Occupancy-moment rasterization (Vulkan backend), mirroring
// kernels/raster/RasterizationMomentsFwd.cu. Same front-to-back micro-tile
// walk as rasterize_fwd.slang's 3DGUT entry; the only difference is the
// per-pixel accumulator -- a two-point linear fit of
// sqrt(-log T) against the fragment depth, which the meshing sampler turns
// back into occupancy at an arbitrary depth (see MeshingRaster's
// occ_from_moment) -- plus the optional full-ray DC color image.
//
// Only 3DGUT with the three supported camera models is instantiated, matching
// the CUDA side; the camera model and distortion tier are specialization
// constants (kDistortion declared last -- see dist_spec.slang).

#define CudaDeviceExport ForceInline
#include "aabb16.slang"
#include "int64_compat.slang"
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport

#include "raster_common.slang"
#include "screen.slang"
#include "dist_spec.slang"

[SpecializationConstant]
const int kCameraModel = 0;
// gaussian_ids present (packed projection); spec-folded so the null pointer
// never sits behind a runtime branch (see backend README).
[SpecializationConstant]
const int kRasterPacked = 0;
// Write the full-ray DC color image as well as the moments.
[SpecializationConstant]
const int kOutputRgb = 0;

// Vanilla3DGUT::FragmentFwd
struct FragMoments {
    float3 mean;
    float3x3 iscl_rot;
    float opacity;
    float3 rgb;
};

groupshared FragMoments _batch_mom[TILE_AREA];
groupshared uint _hit_mom[TILE_AREA];
groupshared uint _done_count_mom;

struct RasterMomentsParams {
    // WorldBuffer members used by the fragment
    float* means;   // [N, 3]
    float* quats;   // [N, 4]
    float* scales;  // [N, 3]
    int* gaussian_ids;  // [nnz] packed mode or null (world idx = g % N)
    float* s_screen;  // packed ScreenBuffer rows; "scale" slot = 2D conic
    // cameras
    float* viewmats;     // [I, 16] row-major
    float4* intrins;     // [I]
    float* dist_coeffs;  // [I, 8] (never null: zeroed fallback)
    uint2* aabb;         // [..] packed 2D AABB, aabb16.slang
    int* tile_offsets;   // [I, tile_height, tile_width]
    int* flatten_ids;    // [n_isects]
    // outputs
    float* out_moments;  // [I,H,W,3] (m0, c0, c1)
    float* out_rgb;      // [I,H,W,3] full-ray DC color, kOutputRgb only
    uint32_t I;
    uint32_t N;  // zero if packed
    uint32_t n_isects;
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t macro_log2;
};

#define _rm_ray(D)                                                    \
    ray_ok = pixel_ray<D>(p.dist_coeffs, image_id, kCameraModel,      \
                          float2((px - intrin.z) / intrin.x,          \
                                 (py - intrin.w) / intrin.y), raydir)

[shader("compute")]
[numthreads(64, 1, 1)]
void rasterize_moments_3dgut(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform RasterMomentsParams* pp) {
    RasterMomentsParams p = *pp;
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
    SS_DISPATCH_DIST(_rm_ray)
    inside = inside && ray_ok;
    float3 ray_d = transform_ray_d(R, raydir);

    bool done = !inside;
    uint pix_id = image_id * p.height * p.width + i * p.width + j;

    float T = 1.0f;
    float z1 = 0.0f, z2 = 0.0f, v1 = 0.0f, v2 = 0.0f, c0 = 0.0f, c1 = 0.0f;
    float3 crgb = float3(0.0f);

    int range_start = tile_offsets[tile_id];
    int range_end =
        (image_id == p.I - 1 &&
         tile_id == int(p.tile_width * p.tile_height) - 1)
            ? int(p.n_isects)
            : tile_offsets[tile_id + 1];
    uint num_batches =
        uint(max(range_end - range_start, 0) + int(TILE_AREA) - 1) / TILE_AREA;

    float mt_bx0 = float(mt_x * TILE_SIZE_X);
    float mt_by0 = float(mt_y * TILE_SIZE_Y);

    if (tid == 0)
        _done_count_mom = 0;
    GroupMemoryBarrierWithGroupSync();
    if (done)
        InterlockedAdd(_done_count_mom, 1);

    for (uint b = 0; b < num_batches; ++b) {
        GroupMemoryBarrierWithGroupSync();
        if (_done_count_mom >= TILE_AREA)
            break;

        int batch_start = range_start + int(TILE_AREA * b);
        int idx = batch_start + int(tid);
        bool hit = false;
        if (idx < range_end) {
            int g = p.flatten_ids[idx];
            uint wi = kRasterPacked != 0 ? uint(p.gaussian_ids[g])
                                         : uint(g) % p.N;
            float* r = scrg_row(p.s_screen, g);
            float opac = scrg_opac(r);
            if (opac > ALPHA_THRESHOLD) {
                float3 conic = scrg_scale(r);
                float2 ec = aabb16_center(p.aabb[g], p.width, p.height);
                float ecx = ec.x, ecy = ec.y;
                float kk = 0.5f / log(opac / ALPHA_THRESHOLD);
                float3 inv_cov = conic * kk;
                hit = ellipse_box_overlap_test(
                    inv_cov, mt_bx0 - ecx, mt_bx0 + float(TILE_SIZE_X) - ecx,
                    mt_by0 - ecy, mt_by0 + float(TILE_SIZE_Y) - ecy);
            }
            if (hit) {
                FragMoments f;
                f.mean = float3(p.means[3 * wi], p.means[3 * wi + 1],
                                p.means[3 * wi + 2]);
                f.iscl_rot = compute_3dgut_iscl_rot(
                    float4(p.quats[4 * wi], p.quats[4 * wi + 1],
                           p.quats[4 * wi + 2], p.quats[4 * wi + 3]),
                    float3(p.scales[3 * wi], p.scales[3 * wi + 1],
                           p.scales[3 * wi + 2]));
                f.opacity = opac;
                f.rgb = scrg_rgb(r);
                _batch_mom[tid] = f;
            }
        }
        _hit_mom[tid] = hit ? 1u : 0u;

        GroupMemoryBarrierWithGroupSync();

        uint batch_size = min(TILE_AREA, uint(range_end - batch_start));
        for (uint t = 0; t < batch_size; ++t) {
            if (_hit_mom[t] == 0u)
                continue;
            if (done)
                continue;
            FragMoments f = _batch_mom[t];

            float alpha = evaluate_alpha_3dgs(f.mean, f.iscl_rot, f.opacity,
                                              ray_o, ray_d);
            if (alpha <= ALPHA_THRESHOLD)
                continue;

            float3 color_rgb;
            float z;
            evaluate_color_3dgs(f.mean, f.iscl_rot, f.opacity, f.rgb, ray_o,
                                ray_d, color_rgb, z);
            if (z <= 0.0f)
                continue;

            float next_T = T * (1.0f - alpha);
            if (next_T > 1e-4f) {
                float vis = alpha * T;
                float v = sqrt(max(-log(max(next_T, 1e-12f)), 1e-12f));
                if (z1 == 0.0f || (z2 == 0.0f && z <= z1)) {
                    z1 = z; v1 = v;
                } else {
                    if (z2 == 0.0f) {
                        z2 = z; v2 = v;
                    } else if (v > c0 + c1 * z) {
                        if (z < z1) { z1 = z; v1 = v; }
                        else        { z2 = z; v2 = v; }
                    }
                    if (1.0f - T <= 0.9f) {
                        c1 = (v2 - v1) / (z2 - z1);
                        c0 = v1 - c1 * z1;
                    }
                }
                if (kOutputRgb != 0)
                    crgb += color_rgb * vis;
                T = next_T;
            } else {
                done = true;
                InterlockedAdd(_done_count_mom, 1);
            }
        }
    }

    if (i < p.height && j < p.width) {
        p.out_moments[3 * pix_id + 0] = 1.0f - T;
        p.out_moments[3 * pix_id + 1] = c0;
        p.out_moments[3 * pix_id + 2] = c1;
        if (kOutputRgb != 0) {
            // normalize to full-ray color
            float inv = (T < 1.0f) ? 1.0f / (1.0f - T) : 0.0f;
            p.out_rgb[3 * pix_id + 0] = crgb.x * inv;
            p.out_rgb[3 * pix_id + 1] = crgb.y * inv;
            p.out_rgb[3 * pix_id + 2] = crgb.z * inv;
        }
    }
}

#undef _rm_ray

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
