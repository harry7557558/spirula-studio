// Occupancy-moment rasterization (Vulkan backend), mirroring
// kernels/raster/RasterizationMomentsFwd.cu. Same front-to-back micro-tile
// walk as rasterize_fwd.slang's 3DGUT entry; the only difference is the
// per-pixel accumulator -- a line for sqrt(-log T) against ray depth, fitted
// to the middle half of the light the ray loses, which the meshing sampler
// turns back into occupancy at any depth (meshing_raster.slang) -- plus the
// optional full-ray DC color image.
//
// 3DGUT camera model and distortion tier are specialization constants
// (kDistortion declared last -- see dist_spec.slang).

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
// The line is fitted to the fragments that take T from kBandHi to kBandLo: the
// middle half of the light a ray loses (docs/notes/mesh-field.md).
static const float kBandHi = 0.75f;
static const float kBandLo = 0.25f;
static const float kMaxSlope = 1000.0f;   // times depth: no profile is sharper
static const float kEntrySigmas = 1.0f;
// A ray that has lost kSurfaceAlpha of its light stops at a depth gap of
// kSurfaceGap (relative): what lies beyond is another surface.
static const float kSurfaceAlpha = 0.5f;
static const float kSurfaceGap = 0.02f;

// A fragment's share of that band: 0 when it lies outside it. `T` before the
// fragment, `alpha` its opacity on the ray.
[ForceInline]
float band_share(float T, float alpha) {
    return max(min(T, kBandHi) - max(T * (1.0f - alpha), kBandLo), 0.0f);
}

// (c0, c1) through the band's mean depth with the slope its spread gives, from
// the sums of share, share * d and share * d^2 (d: depth - z0); `T` at the end.
[ForceInline]
void band_line(float sw, float sz, float szz, float z0, float T,
               out float c0, out float c1) {
    c0 = 0.0f; c1 = 0.0f;
    if (!(sw > 0.0f)) return;
    float ds = sz / sw, zs = z0 + ds;
    float range = sqrt(12.0f * max(szz / sw - ds * ds, 0.0f));
    float end = max(T, kBandLo);
    float dv = sqrt(-log(end)) - sqrt(-log(kBandHi));
    c1 = min(dv / max(range, 1e-12f), kMaxSlope / zs);
    c0 = sqrt(-log(0.5f * (kBandHi + end))) - c1 * zs;
}

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
    if (kCameraModel == 4)
        ray_o += transform_ray_d(R, float3((px-intrin.z)/intrin.x, (py-intrin.w)/intrin.y, 0));
    float3 ray_d = transform_ray_d(R, raydir);

    bool done = !inside;
    uint pix_id = image_id * p.height * p.width + i * p.width + j;

    float T = 1.0f;
    // band_share sums, depths taken from the first (z0): float holds no z^2
    float sw = 0.0f, sz = 0.0f, szz = 0.0f, z0 = 0.0f;
    float z_last = 0.0f, T_stop = -1.0f;   // the surface's last fragment; T where it stopped
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
                // where the ray enters the Gaussian: kEntrySigmas before its peak
                float3 grd = mul(f.iscl_rot, ray_d);
                float zb = max(z - kEntrySigmas * rsqrt(dot(grd, grd)), 1e-6f);
                if (T_stop < 0.0f && sw > 0.0f && 1.0f - T >= kSurfaceAlpha &&
                    zb > z_last * (1.0f + kSurfaceGap))
                    T_stop = T;
                if (T_stop < 0.0f) {
                    float w = band_share(T, alpha);
                    if (w > 0.0f) {
                        if (sw == 0.0f) z0 = zb;
                        sw += w; sz += w * (zb - z0); szz += w * (zb - z0) * (zb - z0);
                    }
                    z_last = zb;
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
        float c0, c1;
        float T_end = T_stop < 0.0f ? T : T_stop;
        band_line(sw, sz, szz, z0, T_end, c0, c1);
        p.out_moments[3 * pix_id + 0] = 1.0f - T_end;
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
