// PixelWise render-path kernels (Vulkan backend): background blends, color
// space conversion, depth->normal. Mirrors the corresponding kernels in
// kernels/pixelwise/ImageColorOps.cu via the same pixel_wise.slang device functions
// (blend_background, linear_rgb_to_srgb / rgb_to_srgb, generate_ray_d2n,
// points_to_normal). Training backwards land with the training phase.
//
// All buffers are contiguous [B, H, W, C] pool tensors, so the element-wise
// kernels run flat over B*H*W with the usual wgs_per_row grid fold. The
// depth->normal kernel keeps CUDA's 16x16 shared-memory tile but as a FLAT
// 256-thread workgroup (README subgroup rules: the X dimension must be a
// multiple of the pinned subgroup size — 16x16 is not).

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "shaders/pixel_wise.slang"
#undef CudaDeviceExport
#include "dist_spec.slang"

// Single boolean variant flag shared by the entries that need one
// (noise blend: is_linear; srgb: is_input_linear).
[SpecializationConstant]
const int kFlagLinear = 0;

// ---------------------------------------------------------------------------
// blend_background_forward
// ---------------------------------------------------------------------------

struct BlendBgParams {  // 40 bytes: pushed directly
    float* rgb;            // [B,H,W,3]
    float* transmittance;  // [B,H,W,1]
    float* background;     // [B,H,W,3]
    float* out_rgb;        // [B,H,W,3]
    uint32_t total;        // B*H*W
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void blend_background_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform BlendBgParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    float3 rgb = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1],
                        p.rgb[3 * idx + 2]);
    float T = p.transmittance[idx];
    float3 bg = float3(p.background[3 * idx], p.background[3 * idx + 1],
                       p.background[3 * idx + 2]);
    rgb = blend_background(rgb, T, bg);
    p.out_rgb[3 * idx] = rgb.x;
    p.out_rgb[3 * idx + 1] = rgb.y;
    p.out_rgb[3 * idx + 2] = rgb.z;
}

// ---------------------------------------------------------------------------
// blend_background_noise_forward. hash_uint3 is a port of the CUDA helper in
// Common.cuh (FNV-ish mix + murmur3 finalizer) — bit-exact.
// ---------------------------------------------------------------------------

[ForceInline]
uint hash_uint3(uint a, uint b, uint c) {
    uint hash = a;
    hash *= 0x01000193u;
    hash = (hash << 16) | (hash >> 16);
    hash ^= b;
    hash *= 0x01000193u;
    hash = (hash << 16) | (hash >> 16);
    hash ^= c;
    hash *= 0x01000193u;
    hash ^= hash >> 15;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16;
    return hash;
}

// Side, in pixels, of a `blocky` background tile. Per-pixel noise is averaged
// back into flat grey by SSIM's 11x11 window and by the multi-scale pyramid,
// which is where the penalty is supposed to land; a tile survives both.
static const uint kBgBlockPx = 64u;

// Murmur-style finalizer. The whole background is computed, never stored.
[ForceInline]
uint _bg_mix(uint x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// The unit sample the background is built from. `blocky` draws one of the 8 RGB
// cube corners per tile -- the extreme points of the cube, so residual
// transparency costs the most it can. Twin of kernels/pixelwise/ImageColorOps.cu.
[ForceInline]
float3 _bg_sample(uint blocky, uint seed, uint gid, uint bid, uint W) {
    if (blocky != 0u) {
        // The tile grid shifts every step, so no pixel keeps its colour and
        // the pattern cannot be baked into the splats.
        uint ox = _bg_mix(seed * 2u + 1u) % kBgBlockPx;
        uint oy = _bg_mix(seed * 2u + 7u) % kBgBlockPx;
        uint h  = _bg_mix((((gid % W) + ox) / kBgBlockPx) * 2654435761u
                          ^ (((gid / W) + oy) / kBgBlockPx) * 40503u
                          ^ (seed + bid * 0x9e3779b9u));
        return float3((h & 1u) != 0u ? 1.0f : -1.0f,
                      (h & 2u) != 0u ? 1.0f : -1.0f,
                      (h & 4u) != 0u ? 1.0f : -1.0f);
    }
    return float3(float(hash_uint3(seed + 0, gid, bid)) * exp2(-32.0f),
                  float(hash_uint3(seed + 1, gid, bid)) * exp2(-32.0f),
                  float(hash_uint3(seed + 2, gid, bid)) * exp2(-32.0f));
}

struct BlendBgNoiseParams {  // 56 bytes: pushed directly
    float* rgb;            // [B,H,W,3]
    float* transmittance;  // [B,H,W,1]
    float* out_rgb;        // [B,H,W,3]
    float randomize_weight;
    uint32_t seed;
    uint32_t HW;           // H*W (hash uses per-image pixel id + batch id)
    uint32_t total;        // B*H*W
    uint32_t wgs_per_row;
    uint32_t W;
    uint32_t blocky;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void blend_background_noise_fwd(uint3 wg: SV_GroupID,
                                uint tid: SV_GroupThreadID,
                                uniform BlendBgNoiseParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    uint bid = idx / p.HW;
    uint gid = idx % p.HW;

    float3 rgb = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1],
                        p.rgb[3 * idx + 2]);
    float T = p.transmittance[idx];

    float3 background = _bg_sample(p.blocky, p.seed, gid, bid, p.W);
    background = 0.5f + 0.5f * p.randomize_weight * background;
    if (kFlagLinear != 0) {
        background.x = srgb_to_linear_rgb(background.x);
        background.y = srgb_to_linear_rgb(background.y);
        background.z = srgb_to_linear_rgb(background.z);
    }

    rgb = blend_background(rgb, T, background);
    p.out_rgb[3 * idx] = rgb.x;
    p.out_rgb[3 * idx + 1] = rgb.y;
    p.out_rgb[3 * idx + 2] = rgb.z;
}

// ---------------------------------------------------------------------------
// rgb_to_srgb_forward (kFlagLinear = is_input_linear)
// ---------------------------------------------------------------------------

struct RgbToSrgbParams {  // 32 bytes: pushed directly
    float* rgb;           // [B,H,W,3]
    float* color_matrix;  // [3,3] row-major
    float* out_rgb;       // [B,H,W,3]
    uint32_t total;       // B*H*W
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void rgb_to_srgb_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform RgbToSrgbParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    float3 rgb = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1],
                        p.rgb[3 * idx + 2]);
    float3x3 cm;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        cm[r] = float3(p.color_matrix[3 * r], p.color_matrix[3 * r + 1],
                       p.color_matrix[3 * r + 2]);
    if (kFlagLinear != 0)
        rgb = linear_rgb_to_srgb(rgb, cm);
    else
        rgb = rgb_to_srgb(rgb, cm);
    p.out_rgb[3 * idx] = rgb.x;
    p.out_rgb[3 * idx + 1] = rgb.y;
    p.out_rgb[3 * idx + 2] = rgb.z;
}

// ---------------------------------------------------------------------------
// depth_to_normal_forward: one workgroup per 16x16 tile (flat 256 threads),
// shared (TILE+2)^2 unprojected-point apron, camera model runtime (as CUDA).
// ---------------------------------------------------------------------------

static const int D2N_TILE = 16;

struct DepthToNormalParams {  // 52 bytes: pushed directly
    float4* intrins;      // [B]
    float* dist_coeffs;   // [B,8] or null
    float* depths;        // [B,H,W,1]
    float* normals;       // [B,H,W,3]
    uint32_t W;
    uint32_t H;
    uint32_t B;
    uint32_t is_ray_depth;
    int camera_model;
};

groupshared float3 _d2n_points[(D2N_TILE + 2) * (D2N_TILE + 2)];

// Apron fill, generic over the tier so the entry folds kDistortion once
// instead of switching inside the loop.
[ForceInline]
void _d2n_fill_apron<D : ICameraDistortion>(DepthToNormalParams p, uint bid,
                                            uint2 tile0, uint tid,
                                            float4 intrin) {
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, bid, dist);
    for (uint k = tid; k < uint((D2N_TILE + 2) * (D2N_TILE + 2)); k += 256) {
        int it = int(k % (D2N_TILE + 2)), jt = int(k / (D2N_TILE + 2));
        int ig = int(tile0.x) + it - 1;
        int jg = int(tile0.y) + jt - 1;
        float depth =
            (ig >= 0 && ig < int(p.W) && jg >= 0 && jg < int(p.H))
                ? p.depths[(bid * p.H + uint(jg)) * p.W + uint(ig)]
                : 0.0f;
        float3 ray = generate_ray_d2n<D>(
            float2(float(ig) + 0.5f, float(jg) + 0.5f), intrin, dist,
            p.camera_model, p.is_ray_depth != 0);
        _d2n_points[k] = ray * depth;
    }
}

#define _d2n_call(D) \
    _d2n_fill_apron<D>(p, bid, uint2(wg.x * D2N_TILE, wg.y * D2N_TILE), tid, \
                       intrin)

[shader("compute")]
[numthreads(256, 1, 1)]
void depth_to_normal_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform DepthToNormalParams p) {
    uint bid = wg.z;
    uint tx = tid % D2N_TILE, ty = tid / D2N_TILE;
    uint i = wg.x * D2N_TILE + tx;  // pixel x
    uint j = wg.y * D2N_TILE + ty;  // pixel y

    bool inside = (bid < p.B && i < p.W && j < p.H);
    if (inside && (i == 0 || i == p.W - 1 || j == 0 || j == p.H - 1)) {
        uint o = (bid * p.H + j) * p.W + i;
        p.normals[3 * o] = 0.0f;
        p.normals[3 * o + 1] = 0.0f;
        p.normals[3 * o + 2] = 0.0f;
        inside = false;
    }

    float4 intrin = p.intrins[bid];

    // Shared apron of unprojected points (ray * depth), same layout as the
    // CUDA kernel's shared_points[TILE+2][TILE+2].
    SS_DISPATCH_DIST(_d2n_call)
    GroupMemoryBarrierWithGroupSync();
    if (!inside) return;

    uint it = tx + 1, jt = ty + 1;
    float3 points[4];
    points[0] = _d2n_points[jt * (D2N_TILE + 2) + it - 1];
    points[1] = _d2n_points[jt * (D2N_TILE + 2) + it + 1];
    points[2] = _d2n_points[(jt - 1) * (D2N_TILE + 2) + it];
    points[3] = _d2n_points[(jt + 1) * (D2N_TILE + 2) + it];
    float3 normal = points_to_normal(points);

    uint o = (bid * p.H + j) * p.W + i;
    p.normals[3 * o] = normal.x;
    p.normals[3 * o + 1] = normal.y;
    p.normals[3 * o + 2] = normal.z;
}

#undef _d2n_call

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
