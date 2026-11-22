// Stochastic (sort-free) tile rasterization, docs/notes/stochastic-splats.md;
// kernels/raster/RasterizationStochastic.cu is the CUDA copy. Binned and culled
// as rasterize_fwd.slang, one 64-thread workgroup per micro tile, but a tile's
// splats are walked in list order, twice: pass 1 keeps, per sample, the nearest
// splat whose alpha accepted it; pass 2 sums what the splats in front show.

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
// 0: the 2D fragment (3dgs, mip), 1: 3dgut, 2: 2dgs.
[SpecializationConstant]
const int kStPrim = 0;
// 3dgut renders its normal too.
[SpecializationConstant]
const int kStNormal = 0;
[SpecializationConstant]
const int kRasterPacked = 0;

static const uint kMaxSpp = 8;
// Floats a pixel keeps over the frames: weighted rgb, depth, normal, the
// weight, and the share of samples that kept a splat.
static const int kStochasticSum = 9;

// Mirrors StochasticParams in backend/vulkan/kernels/RasterizeStochastic.cpp.
struct StochasticParams {
    float* means;   // [N, 3] world rows, eval3d only
    float* quats;
    float* scales;
    int* gaussian_ids;  // kRasterPacked
    float* s_screen;
    float* depths;      // the sort depths, [I * N] or [nnz]
    float* viewmats;    // [I, 16]
    float4* intrins;
    float* dist_coeffs;
    uint2* aabb;
    int* tile_offsets;
    int* flatten_ids;
    float* sum;         // [I, H, W, kStochasticSum]
    float* out_rgb;
    float* out_depth;
    float* out_normal;
    float* out_T;
    uint32_t I;
    uint32_t N;
    uint32_t n_isects;
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t macro_log2;
    uint32_t frame;     // frames already in sum
    uint32_t spp;
    uint32_t winner;    // 1: each sample shows its nearest accepted splat
    uint32_t out_depth_on;
    float kernel_cut;
    uint32_t depth_sorted;  // each tile's list is in sort-key order
    uint32_t stratified;    // a pixel's samples split one draw per splat
    uint32_t _pad0;
};

struct StFrag2d {
    float2 xy;
    float depth;
    float3 conic;
    float opac;
    float3 rgb;
};

struct StFrag3d {
    float3 mean;
    float3x3 iscl_rot;
    float opac;
    float3 rgb;
};

groupshared StFrag2d _st_2d[TILE_AREA];
groupshared StFrag3d _st_3d[TILE_AREA];
groupshared uint _st_hit[TILE_AREA];
groupshared uint _st_key[TILE_AREA];
groupshared uint _st_id[TILE_AREA];
groupshared uint _st_wi[TILE_AREA];
groupshared uint _st_zmax;

// The regular sort's order: |depth| bits, then the flatten id (its write order).
[ForceInline]
bool st_before(uint key_a, uint id_a, uint key_b, uint id_b) {
    return key_a < key_b || (key_a == key_b && id_a < id_b);
}

// Alpha of fragment t at this pixel, as rasterize_fwd.slang evaluates it; 0
// where it does not count there. The colour, depth and world normal with it.
[ForceInline]
float st_eval(uint t, float2 pxy, float3 ray_o, float3 ray_d, float cut, bool normal,
              out float3 rgb, out float depth, out float3 nrm) {
    rgb = float3(0.0f);
    depth = 0.0f;
    nrm = float3(0.0f);
    float alpha;
    if (kStPrim == 0) {
        StFrag2d f = _st_2d[t];
        float2 d = f.xy - pxy;
        float sigma = 0.5f * (f.conic.x * d.x * d.x + f.conic.z * d.y * d.y) +
                      f.conic.y * d.x * d.y;
        alpha = sigma < 0.0f ? 0.0f : min(0.999f, f.opac * kernel_vis(sigma, cut));
        rgb = f.rgb;
        depth = f.depth;
    } else {
        StFrag3d f = _st_3d[t];
        if (kStPrim == 2) {
            alpha = evaluate_2dgs(f.mean, f.iscl_rot, f.opac, f.rgb, ray_o, ray_d,
                                  rgb, depth, nrm);
        } else {
            alpha = evaluate_alpha_3dgs(f.mean, f.iscl_rot, f.opac, ray_o, ray_d);
            if (normal)
                evaluate_color_3dgs_normal(f.mean, f.iscl_rot, f.opac, f.rgb, ray_o,
                                           ray_d, rgb, depth, nrm);
            else
                evaluate_color_3dgs(f.mean, f.iscl_rot, f.opac, f.rgb, ray_o, ray_d,
                                    rgb, depth);
        }
        if (depth <= 0.0f) alpha = 0.0f;
    }
    return alpha > ALPHA_THRESHOLD ? alpha : 0.0f;
}

// The tile's farthest sample key: a splat behind it changes no pixel's samples.
[ForceInline]
uint st_tile_zmax(uint zmax, uint tid) {
    GroupMemoryBarrierWithGroupSync();
    if (tid == 0) _st_zmax = 0u;
    GroupMemoryBarrierWithGroupSync();
    InterlockedMax(_st_zmax, zmax);
    GroupMemoryBarrierWithGroupSync();
    return _st_zmax;
}

#define _st_ray(D)                                                    \
    ray_ok = pixel_ray<D>(p.dist_coeffs, image_id, kCameraModel,      \
                          float2((px - intrin.z) / intrin.x,          \
                                 (py - intrin.w) / intrin.y), raydir)

[shader("compute")]
[numthreads(64, 1, 1)]
void rasterize_stochastic(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform StochasticParams* pp) {
    StochasticParams p = *pp;
    uint image_id = wg.x;
    uint mt_y = wg.y;
    uint mt_x = wg.z;
    int tile_id = int(macro_tile_id(mt_y, mt_x, p.tile_width, p.macro_log2));
    int* tile_offsets = p.tile_offsets + image_id * p.tile_height * p.tile_width;

    uint i = mt_y * TILE_SIZE_Y + tid / TILE_SIZE_X;
    uint j = mt_x * TILE_SIZE_X + tid % TILE_SIZE_X;
    float px = float(j) + 0.5f;
    float py = float(i) + 0.5f;
    bool inside = i < p.height && j < p.width;

    float3x3 R = float3x3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    float3 ray_o = float3(0.0f), ray_d = float3(0.0f, 0.0f, 1.0f);
    if (kStPrim != 0) {
        float* vm = p.viewmats + image_id * 16;
        float3 ct;
        [ForceUnroll]
        for (int r = 0; r < 3; r++) {
            R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
            ct[r] = vm[4 * r + 3];
        }
        float4 intrin = p.intrins[image_id];
        ray_o = transform_ray_o(R, ct);
        float3 raydir = float3(0.0f);
        bool ray_ok = false;
        SS_DISPATCH_DIST(_st_ray)
        inside = inside && ray_ok;
        if (kCameraModel == 4)
            ray_o += transform_ray_d(R, float3((px - intrin.z) / intrin.x,
                                               (py - intrin.w) / intrin.y, 0.0f));
        ray_d = transform_ray_d(R, raydir);
    }
    uint pix = image_id * p.height * p.width + i * p.width + j;
    uint spp = min(p.spp, kMaxSpp);

    uint key[kMaxSpp], id[kMaxSpp];
    [ForceUnroll]
    for (uint s = 0; s < kMaxSpp; s++) {
        key[s] = 0xffffffffu;
        id[s] = 0xffffffffu;
    }
    uint zmax = inside ? 0xffffffffu : 0u;

    int range_start = tile_offsets[tile_id];
    int range_end = (image_id == p.I - 1 &&
                     tile_id == int(p.tile_width * p.tile_height) - 1)
                        ? int(p.n_isects)
                        : tile_offsets[tile_id + 1];
    uint num_batches =
        uint(max(range_end - range_start, 0) + int(TILE_AREA) - 1) / TILE_AREA;

    float mt_bx0 = float(mt_x * TILE_SIZE_X);
    float mt_by0 = float(mt_y * TILE_SIZE_Y);
    float2 mt_half = 0.5f * float2(float(TILE_SIZE_X) - 1.0f, float(TILE_SIZE_Y) - 1.0f);
    float2 mt_centre = float2(mt_bx0, mt_by0) + 0.5f + mt_half;

    float3 sum_rgb = float3(0.0f), sum_normal = float3(0.0f);
    float sum_depth = 0.0f, sum_alpha = 0.0f;
    uint tile_zmax = 0xffffffffu;

    for (uint pass = 0; pass < 2; pass++) {
        if (pass == 1) tile_zmax = st_tile_zmax(zmax, tid);
        for (uint b = 0; b < num_batches; ++b) {
            // Pass 1's samples fill as it goes; pass 2's are final.
            if (pass == 0) tile_zmax = st_tile_zmax(zmax, tid);
            else GroupMemoryBarrierWithGroupSync();

            int batch_start = range_start + int(TILE_AREA * b);
            // In key order, nothing after a splat behind every sample can count.
            if (p.depth_sorted != 0u &&
                asuint(abs(p.depths[p.flatten_ids[batch_start]])) > tile_zmax)
                break;
            int idx = batch_start + int(tid);
            bool hit = false;
            if (idx < range_end) {
                int g = p.flatten_ids[idx];
                uint wi = kRasterPacked != 0 ? uint(p.gaussian_ids[g]) : uint(g) % max(p.N, 1u);
                uint dk = asuint(abs(p.depths[g]));
                _st_key[tid] = dk;
                _st_id[tid] = uint(g);
                _st_wi[tid] = wi;
                if (dk <= tile_zmax) {
                    if (kStPrim == 0) {
                        float* r = scr2_row(p.s_screen, g);
                        StFrag2d f;
                        f.xy = scr2_xy(r);
                        f.depth = scr2_depth(r);
                        f.conic = scr2_conic(r);
                        f.opac = scr2_opac(r);
                        f.rgb = scr2_rgb(r);
                        if (f.opac > ALPHA_THRESHOLD) {
                            float kk = 0.5f / kernel_reach(f.opac, p.kernel_cut);
                            hit = ellipse_box_overlap_test(
                                f.conic * kk, mt_bx0 - f.xy.x,
                                mt_bx0 + float(TILE_SIZE_X) - f.xy.x, mt_by0 - f.xy.y,
                                mt_by0 + float(TILE_SIZE_Y) - f.xy.y);
                        }
                        _st_2d[tid] = f;
                    } else {
                        float* r = scrg_row(p.s_screen, g);
                        float opac = scrg_opac(r);
                        hit = opac > ALPHA_THRESHOLD &&
                              aabb_reaches_micro_tile(aabb16_decode(p.aabb[g], p.width, p.height),
                                                      mt_bx0, mt_by0, float(TILE_SIZE_X)) &&
                              !slab_misses_box(scrg_scale(r), mt_centre, mt_half);
                        if (hit) {
                            StFrag3d f;
                            f.mean = float3(p.means[3 * wi], p.means[3 * wi + 1], p.means[3 * wi + 2]);
                            float4 q = float4(p.quats[4 * wi], p.quats[4 * wi + 1],
                                              p.quats[4 * wi + 2], p.quats[4 * wi + 3]);
                            float3 sc = float3(p.scales[3 * wi], p.scales[3 * wi + 1],
                                               p.scales[3 * wi + 2]);
                            f.iscl_rot = kStPrim == 2 ? compute_2dgs_iscl_rot(q, sc)
                                                      : compute_3dgut_iscl_rot(q, sc);
                            f.opac = opac;
                            f.rgb = scrg_rgb(r);
                            _st_3d[tid] = f;
                        }
                    }
                }
            }
            _st_hit[tid] = hit ? 1u : 0u;
            GroupMemoryBarrierWithGroupSync();

            uint batch_size = min(TILE_AREA, uint(range_end - batch_start));
            for (uint t = 0; t < batch_size; ++t) {
                if (_st_hit[t] == 0u) continue;
                uint dk = _st_key[t];
                if (dk > zmax) continue;
                float3 c_rgb, c_normal;
                float c_depth;
                float alpha = st_eval(t, float2(px, py), ray_o, ray_d, p.kernel_cut,
                                      pass == 1 && kStNormal != 0, c_rgb, c_depth, c_normal);
                if (alpha == 0.0f) continue;
                uint g = _st_id[t];
                if (pass == 0) {
                    uint h = stochastic_hash(pix, _st_wi[t], p.frame);
                    float u0 = stochastic_uniform(h, 0u);
                    uint farthest = 0u;
                    [ForceUnroll]
                    for (uint s = 0; s < kMaxSpp; s++) {
                        if (s < spp) {
                            // Stratified, u0 + s/S: each sample's draw is still
                            // uniform and independent across splats.
                            float u = stochastic_uniform(h, s);
                            if (p.stratified != 0u) {
                                u = u0 + float(s) / float(spp);
                                if (u >= 1.0f) u -= 1.0f;
                            }
                            if (u < alpha && st_before(dk, g, key[s], id[s])) {
                                key[s] = dk;
                                id[s] = g;
                            }
                            farthest = max(farthest, key[s]);
                        }
                    }
                    zmax = farthest;
                } else {
                    // Each sample shows the splats at or in front of its own:
                    // P(nothing nearer accepted) is the transmittance there.
                    uint n = 0u;
                    [ForceUnroll]
                    for (uint s = 0; s < kMaxSpp; s++) {
                        if (s < spp) {
                            bool shown = p.winner != 0u ? g == id[s]
                                                        : !st_before(key[s], id[s], dk, g);
                            n += shown ? 1u : 0u;
                        }
                    }
                    if (n == 0u) continue;
                    float w = float(n) / float(spp);
                    if (p.winner == 0u) w *= alpha;
                    sum_rgb += w * c_rgb;
                    sum_depth += w * c_depth;
                    sum_normal += w * c_normal;
                    sum_alpha += w;
                }
            }
        }
    }

    if (inside) {
        // composited in world space, stored in camera space
        if (kStPrim != 0) sum_normal = undo_transform_ray_d(R, sum_normal);
        uint filled = 0u;
        [ForceUnroll]
        for (uint s = 0; s < kMaxSpp; s++)
            filled += (s < spp && key[s] != 0xffffffffu) ? 1u : 0u;
        float* acc = p.sum + kStochasticSum * pix;
        float v[kStochasticSum] = { sum_rgb.x, sum_rgb.y, sum_rgb.z, sum_depth,
                                    sum_normal.x, sum_normal.y, sum_normal.z, sum_alpha,
                                    float(filled) / float(spp) };
        [ForceUnroll]
        for (int c = 0; c < kStochasticSum; c++) {
            if (p.frame != 0u) v[c] += acc[c];
            acc[c] = v[c];
        }
        // The weighted sums' average, scaled to the coverage: what a frame
        // averages either way, and in the limit the sorted composite.
        float coverage = v[8] / float(p.frame + 1u);
        float scale = v[7] > 0.0f ? coverage / v[7] : 0.0f;
        p.out_rgb[3 * pix + 0] = v[0] * scale;
        p.out_rgb[3 * pix + 1] = v[1] * scale;
        p.out_rgb[3 * pix + 2] = v[2] * scale;
        if (p.out_depth_on != 0u) p.out_depth[pix] = v[7] > 0.0f ? v[3] / v[7] : 0.0f;
        if (kStPrim == 2 || kStNormal != 0) {
            p.out_normal[3 * pix + 0] = v[4] * scale;
            p.out_normal[3 * pix + 1] = v[5] * scale;
            p.out_normal[3 * pix + 2] = v[6] * scale;
        }
        p.out_T[pix] = 1.0f - coverage;
    }
}

#undef _st_ray

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
