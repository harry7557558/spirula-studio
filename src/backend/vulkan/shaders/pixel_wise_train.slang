// PixelWise TRAINING kernels (Vulkan backend): the backward passes of the
// render-path kernels in pixel_wise_render.slang plus the small loss-side
// pixel ops (overexposure regularizer, linear->ray depth remap, color-shift
// regularizer from ColorShiftReg.cu). Mirrors the corresponding kernels in
// kernels/pixelwise/ImageColorOps.cu / ColorShiftReg.cu via the same pixel_wise.slang device
// functions. Element-wise kernels run flat over B*H*W with the wgs_per_row
// fold; depth_to_normal_bwd keeps the 16x16 tile as a flat 256-thread
// workgroup like its forward.

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "shaders/pixel_wise.slang"
#undef CudaDeviceExport

#include "atomic_float.slang"
#include "dist_spec.slang"

// colorspace::Transfer and the working space's linearity, same convention as
// pixel_wise_render.slang.
[SpecializationConstant]
const int kTransfer = 0;
[SpecializationConstant]
const int kIsLinear = 0;

[ForceInline]
bool _pwt_finitef(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

// ---------------------------------------------------------------------------
// blend_background_backward
// ---------------------------------------------------------------------------

struct BlendBgBwdParams {  // 68 bytes: pushed directly
    float* rgb;            // [B,H,W,3] PRE-blend
    float* transmittance;  // [B,H,W,1]
    float* background;     // [B,H,W,3]
    float* v_out_rgb;      // [B,H,W,3]
    float* v_rgb;          // [B,H,W,3]
    float* v_transmittance;// [B,H,W,1]
    float* v_background;   // [B,H,W,3]
    float overexposure_scale;
    uint32_t total;        // B*H*W
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void blend_bg_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                  uniform BlendBgBwdParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    float3 rgb = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1],
                        p.rgb[3 * idx + 2]);
    float T = p.transmittance[idx];
    float3 bg = float3(p.background[3 * idx], p.background[3 * idx + 1],
                       p.background[3 * idx + 2]);
    float3 v_out = float3(p.v_out_rgb[3 * idx], p.v_out_rgb[3 * idx + 1],
                          p.v_out_rgb[3 * idx + 2]);
    float3 v_rgb;
    float v_T;
    float3 v_bg;
    blend_background_bwd(rgb, T, bg, v_out, p.overexposure_scale, v_rgb, v_T,
                         v_bg);
    p.v_rgb[3 * idx] = v_rgb.x;
    p.v_rgb[3 * idx + 1] = v_rgb.y;
    p.v_rgb[3 * idx + 2] = v_rgb.z;
    p.v_transmittance[idx] = v_T;
    p.v_background[3 * idx] = v_bg.x;
    p.v_background[3 * idx + 1] = v_bg.y;
    p.v_background[3 * idx + 2] = v_bg.z;
}

// ---------------------------------------------------------------------------
// blend_background_noise_backward (hash matches the noise forward)
// ---------------------------------------------------------------------------

[ForceInline]
uint _pwt_hash_uint3(uint a, uint b, uint c) {
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
    // 2u-1: without it the plain path lands in [0.5, 0.5+w/2) instead of
    // straddling 0.5, so every channel sits in the same bright half.
    return float3(float(_pwt_hash_uint3(seed + 0, gid, bid)) * exp2(-31.0f) - 1.0f,
                  float(_pwt_hash_uint3(seed + 1, gid, bid)) * exp2(-31.0f) - 1.0f,
                  float(_pwt_hash_uint3(seed + 2, gid, bid)) * exp2(-31.0f) - 1.0f);
}

struct BlendBgNoiseBwdParams {  // 72 bytes: pushed directly
    float* rgb;            // PRE-blend
    float* transmittance;
    float* v_out_rgb;
    float* v_rgb;
    float* v_transmittance;
    float overexposure_scale;
    float randomize_weight;
    uint32_t seed;
    uint32_t HW;
    uint32_t total;
    uint32_t wgs_per_row;
    uint32_t W;
    uint32_t blocky;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void blend_bg_noise_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform BlendBgNoiseBwdParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    uint bid = idx / p.HW;
    uint gid = idx % p.HW;

    float3 rgb = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1],
                        p.rgb[3 * idx + 2]);
    float T = p.transmittance[idx];

    float3 background = _bg_sample(p.blocky, p.seed, gid, bid, p.W);
    background = 0.5f + 0.5f * p.randomize_weight * background;
    background = display_to_working3(background, kTransfer, kIsLinear != 0);

    float3 v_out = float3(p.v_out_rgb[3 * idx], p.v_out_rgb[3 * idx + 1],
                          p.v_out_rgb[3 * idx + 2]);
    float3 v_rgb;
    float v_T;
    float3 v_bg;
    blend_background_bwd(rgb, T, background, v_out, p.overexposure_scale,
                         v_rgb, v_T, v_bg);
    p.v_rgb[3 * idx] = v_rgb.x;
    p.v_rgb[3 * idx + 1] = v_rgb.y;
    p.v_rgb[3 * idx + 2] = v_rgb.z;
    p.v_transmittance[idx] = v_T;
}

// ---------------------------------------------------------------------------
// working_to_display_backward (kTransfer / kIsLinear = the source encoding)
// ---------------------------------------------------------------------------

struct RgbToSrgbBwdParams {  // 40 bytes: pushed directly
    float* rgb;
    float* color_matrix;  // [3,3] row-major
    float* v_out_rgb;
    float* v_rgb;
    uint32_t total;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void working_to_display_bwd_k(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                              uniform RgbToSrgbBwdParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    float3 rgb = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1],
                        p.rgb[3 * idx + 2]);
    float3x3 cm;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        cm[r] = float3(p.color_matrix[3 * r], p.color_matrix[3 * r + 1],
                       p.color_matrix[3 * r + 2]);
    float3 v_out = float3(p.v_out_rgb[3 * idx], p.v_out_rgb[3 * idx + 1],
                          p.v_out_rgb[3 * idx + 2]);
    float3 v_rgb = working_to_display_bwd(rgb, cm, kTransfer, kIsLinear != 0,
                                          v_out);
    p.v_rgb[3 * idx] = v_rgb.x;
    p.v_rgb[3 * idx + 1] = v_rgb.y;
    p.v_rgb[3 * idx + 2] = v_rgb.z;
}

// ---------------------------------------------------------------------------
// overexposure_grad_add (in-place gradient injection)
// ---------------------------------------------------------------------------

struct OverexposureParams {  // 32 bytes: pushed directly
    float* rgb;    // [B,H,W,3]
    float* v_rgb;  // [B,H,W,3] in/out
    float scale;   // 2 * weight / (B*H*W*3)
    uint32_t total;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void overexposure_add(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform OverexposureParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    float3 c = float3(p.rgb[3 * idx], p.rgb[3 * idx + 1], p.rgb[3 * idx + 2]);
    float3 g = overexposure_grad(c, p.scale);
    p.v_rgb[3 * idx] += g.x;
    p.v_rgb[3 * idx + 1] += g.y;
    p.v_rgb[3 * idx + 2] += g.z;
}

// ---------------------------------------------------------------------------
// depth_to_normal_backward: 16x16 tile as flat 256 threads, shared apron of
// (ray, depth); each pixel scatters 4 v_depth taps to its neighbors with
// portable atomic adds (neighboring pixels share taps, as in CUDA).
// ---------------------------------------------------------------------------

static const int D2NB_TILE = 16;

struct DepthToNormalBwdParams {  // 60 bytes: pushed directly
    float4* intrins;      // [B]
    float* dist_coeffs;   // [B,8] (fallback-backed)
    float* depths;        // [B,H,W,1]
    float* v_normals;     // [B,H,W,3]
    float* v_depths;      // [B,H,W,1] accumulated in place
    uint32_t W;
    uint32_t H;
    uint32_t B;
    uint32_t is_ray_depth;
    int camera_model;
};

groupshared float4 _d2nb_points[(D2NB_TILE + 2) * (D2NB_TILE + 2)];

// Apron fill, generic over the tier so the entry folds kDistortion once
// instead of switching inside the loop.
[ForceInline]
void _d2nb_fill_apron<D : ICameraDistortion>(DepthToNormalBwdParams p,
                                             uint bid, uint2 tile0, uint tid,
                                             float4 intrin) {
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, bid, dist);
    for (uint k = tid; k < uint((D2NB_TILE + 2) * (D2NB_TILE + 2)); k += 256) {
        int it = int(k % (D2NB_TILE + 2)), jt = int(k / (D2NB_TILE + 2));
        int ig = int(tile0.x) + it - 1;
        int jg = int(tile0.y) + jt - 1;
        float depth =
            (ig >= 0 && ig < int(p.W) && jg >= 0 && jg < int(p.H))
                ? p.depths[(bid * p.H + uint(jg)) * p.W + uint(ig)]
                : 0.0f;
        float3 ray = generate_ray_d2n<D>(
            float2(float(ig) + 0.5f, float(jg) + 0.5f), intrin, dist,
            p.camera_model, p.is_ray_depth != 0);
        _d2nb_points[k] = float4(ray, depth);
    }
}

#define _d2nb_call(D)                                                       \
    _d2nb_fill_apron<D>(p, bid, uint2(wg.x * D2NB_TILE, wg.y * D2NB_TILE),  \
                        tid, intrin)

[shader("compute")]
[numthreads(256, 1, 1)]
void d2n_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
             uniform DepthToNormalBwdParams p) {
    uint bid = wg.z;
    uint tx = tid % D2NB_TILE, ty = tid / D2NB_TILE;
    uint i = wg.x * D2NB_TILE + tx;
    uint j = wg.y * D2NB_TILE + ty;

    bool inside = (bid < p.B && i < p.W && j < p.H);
    if (i == 0 || i == p.W - 1 || j == 0 || j == p.H - 1) inside = false;

    float4 intrin = p.intrins[bid];

    SS_DISPATCH_DIST(_d2nb_call)
    GroupMemoryBarrierWithGroupSync();
    if (!inside) return;

    uint o = (bid * p.H + j) * p.W + i;
    float3 v_normal = float3(p.v_normals[3 * o], p.v_normals[3 * o + 1],
                             p.v_normals[3 * o + 2]);

    uint it = tx + 1, jt = ty + 1;
    float3 rays[4];
    float3 points[4];
    float4 t;
    t = _d2nb_points[jt * (D2NB_TILE + 2) + it - 1];
    rays[0] = t.xyz; points[0] = t.xyz * t.w;
    t = _d2nb_points[jt * (D2NB_TILE + 2) + it + 1];
    rays[1] = t.xyz; points[1] = t.xyz * t.w;
    t = _d2nb_points[(jt - 1) * (D2NB_TILE + 2) + it];
    rays[2] = t.xyz; points[2] = t.xyz * t.w;
    t = _d2nb_points[(jt + 1) * (D2NB_TILE + 2) + it];
    rays[3] = t.xyz; points[3] = t.xyz * t.w;

    float3 v_points[4];
    points_to_normal_vjp(points, v_normal, v_points);

    uint row = bid * p.H + j;  // inside => 1 <= i,j <= W-2,H-2
    atomic_add_f32(p.v_depths + (row * p.W + (i - 1)), dot(v_points[0], rays[0]));
    atomic_add_f32(p.v_depths + (row * p.W + (i + 1)), dot(v_points[1], rays[1]));
    atomic_add_f32(p.v_depths + ((row - 1) * p.W + i), dot(v_points[2], rays[2]));
    atomic_add_f32(p.v_depths + ((row + 1) * p.W + i), dot(v_points[3], rays[3]));
}

// ---------------------------------------------------------------------------
// linear_depth_to_ray_depth_inplace
// ---------------------------------------------------------------------------

struct LinToRayDepthParams {  // 48 bytes: pushed directly
    float4* intrins;     // [B] at IMAGE resolution
    float* dist_coeffs;  // [B,8] (fallback-backed)
    float* depths;       // [B,Hd,Wd,1] in/out
    float sx, sy;        // depth_res / image_res
    uint32_t W, H, B;
    int camera_model;
    uint32_t _pad0;
};

[ForceInline]
float _l2r_factor<D : ICameraDistortion>(LinToRayDepthParams p, uint bid,
                                         float4 scaled, float2 pix) {
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, bid, dist);
    return ray_depth_to_linear_depth_factor<D>(pix, scaled, dist,
                                               p.camera_model);
}

#define _l2r_call(D)                                                     \
    factor = _l2r_factor<D>(p, bid, scaled,                              \
                            float2(float(i) + 0.5f, float(j) + 0.5f))

[shader("compute")]
[numthreads(128, 1, 1)]
void lin_to_ray_depth(uint3 gtid: SV_DispatchThreadID,
                      uniform LinToRayDepthParams p) {
    uint i = gtid.x, j = gtid.y, bid = gtid.z;
    if (i >= p.W || j >= p.H || bid >= p.B) return;

    float4 intrin = p.intrins[bid];
    float4 scaled = float4(intrin.x * p.sx, intrin.y * p.sy, intrin.z * p.sx,
                           intrin.w * p.sy);

    uint o = (bid * p.H + j) * p.W + i;
    float in_depth = p.depths[o];
    float factor = 0.0f;
    SS_DISPATCH_DIST(_l2r_call)
    // factor is cos(theta); 0.08715574 is cos(85 deg), past which 1/cos turns
    // a pixel of depth quantization into metres of GT.
    p.depths[o] = (factor > 0.08715574f) ? (in_depth / factor) : 0.0f;
}

// ---------------------------------------------------------------------------
// color_shift_reg_step (ColorShiftReg.cu): gradient injection + per-channel
// sign-mean reduction. CUDA's shared-memory float atomics become a
// per-thread groupshared partial + serial reduce (deterministic within a
// block) with one portable atomic add per channel per block.
// ---------------------------------------------------------------------------

struct ColorShiftInjectParams {  // 48 bytes: pushed directly
    float* v_render_rgb;  // [N] float3 in/out
    float* post_rgb;      // [N] float3
    float* pre_rgb;       // [N] float3
    float* ema;           // [3]
    float* batch_sum;     // [3] accumulated
    float reg_coef;
    uint32_t N;
    uint32_t wgs_per_row;
};

groupshared float3 _csr_partial[256];

[shader("compute")]
[numthreads(256, 1, 1)]
void color_shift_inject(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform ColorShiftInjectParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    bool inside = idx < p.N;

    float3 s = float3(0.0f);
    if (inside) {
        float3 post = float3(p.post_rgb[3 * idx], p.post_rgb[3 * idx + 1],
                             p.post_rgb[3 * idx + 2]);
        float3 pre = float3(p.pre_rgb[3 * idx], p.pre_rgb[3 * idx + 1],
                            p.pre_rgb[3 * idx + 2]);
        s.x = (post.x > pre.x ? 1.0f : 0.0f) - (post.x < pre.x ? 1.0f : 0.0f);
        s.y = (post.y > pre.y ? 1.0f : 0.0f) - (post.y < pre.y ? 1.0f : 0.0f);
        s.z = (post.z > pre.z ? 1.0f : 0.0f) - (post.z < pre.z ? 1.0f : 0.0f);

        p.v_render_rgb[3 * idx] -= p.reg_coef * p.ema[0] * s.x;
        p.v_render_rgb[3 * idx + 1] -= p.reg_coef * p.ema[1] * s.y;
        p.v_render_rgb[3 * idx + 2] -= p.reg_coef * p.ema[2] * s.z;
    }

    _csr_partial[tid] = s;
    GroupMemoryBarrierWithGroupSync();
    if (tid == 0) {
        float3 sum = float3(0.0f);
        for (uint k = 0; k < 256; k++) sum += _csr_partial[k];
        atomic_add_f32(p.batch_sum + 0, sum.x);
        atomic_add_f32(p.batch_sum + 1, sum.y);
        atomic_add_f32(p.batch_sum + 2, sum.z);
    }
}

struct ColorShiftUpdateParams {  // 24 bytes: pushed directly
    float* ema;        // [3] in/out
    float* batch_sum;  // [3] in/out (reset to 0)
    float beta;
    float inv_n_pixels;
};

[shader("compute")]
[numthreads(32, 1, 1)]
void color_shift_update(uint tid: SV_GroupThreadID,
                        uniform ColorShiftUpdateParams p) {
    if (tid < 3) {
        float batch_mean = p.batch_sum[tid] * p.inv_n_pixels;
        p.ema[tid] = p.beta * p.ema[tid] + (1.0f - p.beta) * batch_mean;
        p.batch_sum[tid] = 0.0f;
    }
}

#undef _d2nb_call
#undef _l2r_call

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
