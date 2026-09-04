// Tile rasterization backward (Vulkan backend), following
// kernels/raster/RasterizationEval3DBwd_kernel.cuh (which also generates the
// 2D backward via IS_EVAL3D=0; two entries here because the fragment types
// differ). The sweep schedule, the survivor compaction and the portability
// substitutions behind them are documented in backend/vulkan/README.md.
//
// Two constraints a reader cannot recover from the code: the sweep body is an
// inlined function so its early-outs are returns and never skip the caller's
// barrier, and the SpecializationConstant declaration order below IS the spec
// ID order RasterizeBwd.cpp pushes.

#define CudaDeviceExport ForceInline
#include "aabb16.slang"
#include "int64_compat.slang"
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport

#include "raster_common.slang"
#include "screen.slang"
#include "atomic_float.slang"
#include "dist_spec.slang"

[SpecializationConstant]
const int kCameraModel = 0;   // CameraModelType; 3dgut entry only
[SpecializationConstant]
const int kDistType = 0;      // 0 None | 1 D | 2 RGB_D
[SpecializationConstant]
const int kOutputMedian = 0;
[SpecializationConstant]
const int kAccumWeight = 0;   // DensifyAccumMode
static const int kAccumMax = 1, kAccumSum = 2, kAccumAvg = 3;
[SpecializationConstant]
const int kViewmatGrad = 0;   // 3dgut entry only
[SpecializationConstant]
const int kPacked = 0;        // gaussian_ids present

static const uint TILE_SIZE_DX = 8;
static const uint TILE_SIZE_DY = 8;
static const uint RB_BLOCK = TILE_SIZE_DX * TILE_SIZE_DY;  // 64 pixels
static const uint RB_THREADS = 32;                         // = SPLAT_BATCH
// Survivors carry the screen data the cull already gathered, so the capacity
// trades against groupshared; two chunks is enough to keep the sweep fed.
static const int SURV_CAP = 2 * (int)RB_THREADS;

[ForceInline]
bool _rb_finitef(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

[ForceInline]
void _rb_atomic_add(float* addr, float v) {
    if (_rb_finitef(v)) atomic_add_f32(addr, v);
}

// ---- shared pixel state (indexed by local pixel id 0..63) ----------------
groupshared float2 _rb_Tg[RB_BLOCK];     // (T, v_T), evolves during sweep
groupshared float4 _rb_vpix[RB_BLOCK];   // v_render color (rgb, depth-pre)
groupshared float4 _rb_C[RB_BLOCK];      // distortion totals C
groupshared float4 _rb_S[RB_BLOCK];      // distortion second moments S
groupshared float4 _rb_vdist[RB_BLOCK];  // v of distortion image
groupshared float _rb_W[RB_BLOCK];       // 1 - T_final
groupshared float _rb_awmap[RB_BLOCK];
groupshared float _rb_medv[RB_BLOCK];
groupshared float _rb_medzg[RB_BLOCK];
groupshared float _rb_medzf[RB_BLOCK];
groupshared float _rb_medTf[RB_BLOCK];
groupshared int _rb_bin[RB_BLOCK];       // 2d: last_ids
groupshared float4 _rb_raybin[RB_BLOCK]; // 3dgut: (ray_d, bin as bits)
groupshared float3 _rb_vrayd[RB_BLOCK];  // 3dgut viewmat grad per pixel
groupshared int _rb_surv[SURV_CAP];      // flatten range index
groupshared int _rb_ssid[SURV_CAP];      // flatten_ids[surv]
groupshared float2 _rb_sxy[SURV_CAP];
groupshared float3 _rb_sconic[SURV_CAP];
groupshared float _rb_sopac[SURV_CAP];
groupshared uint _rb_wtot[RB_THREADS];   // per-wave compaction totals
groupshared uint _rb_binmax;             // 1 + max last_id over the tile's pixels
groupshared float _rb_red[RB_THREADS];   // viewmat-grad reduction scratch

// Load the per-pixel color/distortion/median state shared by both entries.
// `pix` is the local pixel id, `gpix` the in-image pixel id (valid only when
// inside). Returns nothing; fills the groupshared arrays.
[ForceInline]
void _rb_load_pixel_state(
    uint pix, bool inside, uint image_pix,  // image_id * H * W + gpix
    float* render_Ts_img, float* v_render_Ts_img, uint gpix,
    float* out_rgb, float* out_depth, float* dist_rgb, float* dist_depth,
    float* v_out_rgb, float* v_out_depth, float* v_dist_rgb,
    float* v_dist_depth, float* awmap, float* v_median) {
    float T = inside ? render_Ts_img[gpix] : 0.0f;
    float v_T = inside ? v_render_Ts_img[gpix] : 0.0f;
    float4 v_c = float4(0.0f);
    if (inside) {
        v_c = float4(v_out_rgb[3 * image_pix], v_out_rgb[3 * image_pix + 1],
                     v_out_rgb[3 * image_pix + 2], v_out_depth[image_pix]);
    }
    // depth channel: convert dL/d(render depth) to dL/d(accumulated C).
    {
        float inv_alpha = 1.0f / max(1.0f - T, 1e-10f);
        float exp_depth = inside ? out_depth[image_pix] : 0.0f;
        if (kDistType != 0) {
            float m = log(max(exp_depth, DEPTH_DIST_EPS));
            v_T += exp_depth * v_c.w * m * inv_alpha;
            v_c.w *= exp_depth * inv_alpha;
        } else {
            v_T += exp_depth * v_c.w * inv_alpha;
            v_c.w *= inv_alpha;
        }
    }
    _rb_Tg[pix] = float2(T, v_T);
    _rb_vpix[pix] = v_c;

    if (kDistType != 0) {
        float4 C = float4(0.0f);
        float4 Dv = float4(0.0f);
        float4 v_d = float4(0.0f);
        if (inside) {
            C = float4(out_rgb[3 * image_pix], out_rgb[3 * image_pix + 1],
                       out_rgb[3 * image_pix + 2], out_depth[image_pix]);
            Dv.w = dist_depth[image_pix];
            v_d.w = v_dist_depth[image_pix];
            if (kDistType == 2) {
                Dv.xyz = float3(dist_rgb[3 * image_pix],
                                dist_rgb[3 * image_pix + 1],
                                dist_rgb[3 * image_pix + 2]);
                v_d.xyz = float3(v_dist_rgb[3 * image_pix],
                                 v_dist_rgb[3 * image_pix + 1],
                                 v_dist_rgb[3 * image_pix + 2]);
            }
        }
        float W = max(1.0f - T, 1e-10f);
        C.w = log(max(C.w, DEPTH_DIST_EPS)) * W;   // un-normalized C_logz
        float Wr = inside ? (1.0f - T) : 0.0f;
        float invW = Wr > 1e-10f ? (1.0f / Wr) : 0.0f;
        _rb_C[pix] = C;
        _rb_S[pix] = (Dv + C * C) * invW;
        _rb_vdist[pix] = v_d;
        _rb_W[pix] = Wr;
    }
    if (kAccumWeight != 0)
        _rb_awmap[pix] = inside ? awmap[image_pix] : 0.0f;
    if (kOutputMedian != 0) {
        _rb_medv[pix] = inside ? v_median[image_pix] : 0.0f;
        _rb_medzg[pix] = 0.0f;
        _rb_medzf[pix] = 0.0f;
        _rb_medTf[pix] = 0.0f;
    }
}

// Common pixel-undo math from alpha + color to (v_alpha, v_color) plus the
// shared-state updates. Returns false when the pixel is skipped.
[ForceInline]
bool _rb_pixel_grad(int pix_id, float alpha, float4 color,
                    out float v_alpha, out float4 v_color) {
    float T1 = _rb_Tg[pix_id].x;
    float v_T1 = _rb_Tg[pix_id].y;
    float ra = 1.0f / (1.0f - alpha);
    float T0 = T1 * ra;
    float4 v_c = _rb_vpix[pix_id];

    // ---- median depth backward (see CUDA comments) ----
    float v_depth_median = 0.0f;
    if (kOutputMedian != 0) {
        const float lnhalf = -0.6931471805599453f;
        float v_med = _rb_medv[pix_id];
        if (_rb_medTf[pix_id] > 0.0f) {
            v_depth_median += _rb_medzg[pix_id];
            float a_ln = log(T1);
            float b_ln = log(_rb_medTf[pix_id]);
            float dba = b_ln - a_ln;
            float inv = (abs(dba) > 1e-20f) ? (-1.0f / dba) : 0.0f;
            float S = v_med * (_rb_medzf[pix_id] - color.w) * inv;
            v_T1 += S / T1;
            _rb_medTf[pix_id] = 0.0f;
        }
        if (T0 >= 0.5f && T1 < 0.5f) {
            float a = log(T0);
            float b = log(T1);
            float d = b - a;
            float f = (abs(d) > 1e-20f) ? (lnhalf - a) / d : 0.0f;
            v_depth_median += f * v_med;
            _rb_medzg[pix_id] = (1.0f - f) * v_med;
            _rb_medzf[pix_id] = color.w;
            _rb_medTf[pix_id] = T1;
        }
    }

    float4 acc = color;
    if (kDistType != 0) acc.w = log(max(color.w, DEPTH_DIST_EPS));

    v_alpha = T0 * dot(acc, v_c) - v_T1 * T0;
    v_color = v_c * (alpha * T0);
    float v_T0 = alpha * dot(acc, v_c) + v_T1 * (1.0f - alpha);

    if (kDistType != 0) {
        float4 v_dist = _rb_vdist[pix_id];
        float4 C = _rb_C[pix_id];
        float4 S = _rb_S[pix_id];
        float W = _rb_W[pix_id];
        float w = alpha * T0;
        v_color += (acc * W - C) * v_dist * (2.0f * w);
        float g = dot(S + acc * acc * W - acc * C * 2.0f, v_dist);
        v_alpha += T0 * g;
        v_T0 += alpha * g;
    }

    if (kDistType != 0) v_color.w *= 1.0f / max(color.w, DEPTH_DIST_EPS);
    v_color.w += v_depth_median;

    _rb_Tg[pix_id] = float2(T0, v_T0);
    return true;
}

// ---------------------------------------------------------------------------
// 2D backward (Vanilla3DGS / MipSplatting)
// ---------------------------------------------------------------------------

struct Raster2dBwdParams {  // via params ring
    float* s_screen;    // [.., SCR2_STRIDE]
    int* gaussian_ids;   // kPacked only (fallback-backed otherwise)
    int* tile_offsets;
    int* flatten_ids;
    float* render_Ts;    // [I,H,W]
    int* last_ids;       // [I,H,W]
    float* out_rgb;      // [I,H,W,3]
    float* out_depth;    // [I,H,W]
    float* dist_rgb;     // kDistType == 2
    float* dist_depth;   // kDistType != 0
    float* awmap;        // kAccumWeight
    float* v_out_rgb;
    float* v_out_depth;
    float* v_render_Ts;
    float* v_median;     // kOutputMedian
    float* v_dist_rgb;
    float* v_dist_depth;
    float* v_s_screen;  // [.., SCR2_STRIDE]
    uint* o_accum_weight;   // kAccumWeight; Max writes float bits, else float
    float* o_accum_weight_den;  // kAccumAvg lane 2
    uint32_t I;
    uint32_t N;
    uint32_t n_isects;
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t macro_log2;  // was _pad0; the struct size is unchanged
};

struct Frag2d_ {
    float2 xy;
    float depth;
    float3 conic;
    float opac;
    float3 rgb;
};

[ForceInline]
float _rb2_alpha(Frag2d_ s, float2 delta, out float sigma, out float vis) {
    sigma =
        0.5f * (s.conic.x * delta.x * delta.x +
                s.conic.z * delta.y * delta.y) +
        s.conic.y * delta.x * delta.y;
    vis = exp(-sigma);
    float alpha = min(0.999f, s.opac * vis);
    return sigma < 0.f ? 0.f : alpha;
}

// sigma / vis come from _rb2_alpha: re-deriving them here would cost a second
// exp() on every contributing pixel.
[ForceInline]
void _rb2_alpha_vjp(Frag2d_ s, float2 delta, float sigma, float vis,
                    float v_alpha, inout Frag2d_ v) {
    if (sigma >= 0.f && s.opac * vis <= 0.999f) {
        float v_sigma = -s.opac * vis * v_alpha;
        v.conic += float3(0.5f * v_sigma * delta.x * delta.x,
                          v_sigma * delta.x * delta.y,
                          0.5f * v_sigma * delta.y * delta.y);
        v.xy += float2(
            v_sigma * (s.conic.x * delta.x + s.conic.y * delta.y),
            v_sigma * (s.conic.y * delta.x + s.conic.z * delta.y));
        v.opac += vis * v_alpha;
    }
}

// One sweep step for the 2D entry; early-outs are plain returns so the
// caller's barrier is never skipped.
[ForceInline]
void _rb2_step(int pix_id, int splat_idx, bool active,
               uint wg_y, uint wg_z, Frag2d_ splat,
               inout Frag2d_ v_splat,
               inout float accum_weight, inout float accum_weight_den,
               inout bool touched) {
    if (pix_id < 0 || pix_id >= (int)RB_BLOCK || !active) return;
    if (splat_idx > _rb_bin[pix_id]) return;
    int gx = int(wg_z * TILE_SIZE_DX) + pix_id % (int)TILE_SIZE_DX;
    int gy = int(wg_y * TILE_SIZE_DY) + pix_id / (int)TILE_SIZE_DX;
    float2 delta = float2(splat.xy.x - (float(gx) + 0.5f),
                          splat.xy.y - (float(gy) + 0.5f));

    float sigma, vis;
    float alpha = _rb2_alpha(splat, delta, sigma, vis);
    if (alpha <= ALPHA_THRESHOLD) return;
    float4 color = float4(splat.rgb, splat.depth);

    float v_alpha;
    float4 v_color;
    _rb_pixel_grad(pix_id, alpha, color, v_alpha, v_color);

    _rb2_alpha_vjp(splat, delta, sigma, vis, v_alpha, v_splat);
    v_splat.rgb += v_color.xyz;
    v_splat.depth += v_color.w;
    touched = true;

    if (kAccumWeight != 0) {
        float T0 = _rb_Tg[pix_id].x;  // just updated: T0 stored in .x
        // CUDA computes the weight with the pre-update T0 = T1/(1-alpha);
        // _rb_pixel_grad stored exactly that, so reuse it.
        float w = alpha * T0;
        if (kAccumWeight == kAccumMax)
            accum_weight = max(accum_weight, _rb_awmap[pix_id] * w);
        else {
            accum_weight += _rb_awmap[pix_id] * w;
            if (kAccumWeight == kAccumAvg) accum_weight_den += w;
        }
    }
}

[shader("compute")]
[numthreads(32, 1, 1)]
void rasterize_bwd_2d(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform Raster2dBwdParams* pp) {
    Raster2dBwdParams p = *pp;
    uint image_id = wg.x;
    uint tile_id = macro_tile_id(wg.y * TILE_SIZE_DY / TILE_SIZE_Y,
                                 wg.z * TILE_SIZE_DX / TILE_SIZE_X,
                                 p.tile_width, p.macro_log2);
    uint img_off = image_id * p.height * p.width;

    // ---- load pixel state (2 pixels per thread) ----
    if (tid == 0) _rb_binmax = 0u;
    GroupMemoryBarrierWithGroupSync();
    int bin_local = -1;
    for (uint pix0 = 0; pix0 < RB_BLOCK; pix0 += RB_THREADS) {
        uint pix = pix0 + tid;
        int gx = int(wg.z * TILE_SIZE_DX) + int(pix % TILE_SIZE_DX);
        int gy = int(wg.y * TILE_SIZE_DY) + int(pix / TILE_SIZE_DX);
        uint gpix = uint(gy) * p.width + uint(gx);
        bool inside = (gx < int(p.width) && gy < int(p.height));
        int bin = inside ? p.last_ids[img_off + gpix] : -1;
        _rb_bin[pix] = bin;
        bin_local = max(bin_local, bin);
        _rb_load_pixel_state(
            pix, inside, img_off + gpix, p.render_Ts + img_off,
            p.v_render_Ts + img_off, gpix, p.out_rgb, p.out_depth,
            p.dist_rgb, p.dist_depth, p.v_out_rgb, p.v_out_depth,
            p.v_dist_rgb, p.v_dist_depth, p.awmap, p.v_median);
    }
    { uint prev; InterlockedMax(_rb_binmax, uint(bin_local + 1), prev); }
    GroupMemoryBarrierWithGroupSync();
    int bin_max = int(_rb_binmax) - 1;

    int range_start = p.tile_offsets[image_id * p.tile_height * p.tile_width +
                                     tile_id];
    int range_end =
        (image_id == p.I - 1 &&
         tile_id == p.tile_width * p.tile_height - 1)
            ? int(p.n_isects)
            : p.tile_offsets[image_id * p.tile_height * p.tile_width +
                             tile_id + 1];

    float cull_bx0 = float(wg.z * TILE_SIZE_DX);
    float cull_by0 = float(wg.y * TILE_SIZE_DY);
    uint wsize = WaveGetLaneCount();
    uint n_waves = RB_THREADS / wsize;

    // Nothing past the tile's deepest surviving fragment can contribute.
    int count = 0;
    for (int scan_end = min(range_end - 1, bin_max); scan_end >= range_start || count > 0;) {
        // ---- survivor compaction (back-to-front) ----
        int s = scan_end;
        while (s >= range_start && count <= SURV_CAP - (int)RB_THREADS) {
            int gi = s - int(tid);
            bool ok = false;
            int c_sid = 0;
            float c_opac = 0.0f;
            float3 c_conic = float3(0.0f);
            float2 c_xy = float2(0.0f);
            if (gi >= range_start) {
                c_sid = p.flatten_ids[gi];
                float* c_row = scr2_row(p.s_screen, c_sid);
                c_opac = scr2_opac(c_row);
                c_conic = scr2_conic(c_row);
                c_xy = scr2_xy(c_row);
                if (c_opac > ALPHA_THRESHOLD) {
                    float kk = 0.5f / log(c_opac / ALPHA_THRESHOLD);
                    ok = ellipse_box_overlap_test(
                        c_conic * kk, cull_bx0 - c_xy.x,
                        cull_bx0 + TILE_SIZE_DX - c_xy.x, cull_by0 - c_xy.y,
                        cull_by0 + TILE_SIZE_DY - c_xy.y);
                }
            }
            uint prefix = WavePrefixCountBits(ok);
            uint wtotal = WaveActiveCountBits(ok);
            uint base = 0;
            uint total = wtotal;
            if (n_waves > 1) {  // one wave covers the group: no roundtrip
                GroupMemoryBarrierWithGroupSync();
                if (WaveGetLaneIndex() == 0) _rb_wtot[tid / wsize] = wtotal;
                GroupMemoryBarrierWithGroupSync();
                for (uint i = 0; i < tid / wsize; i++) base += _rb_wtot[i];
                total = 0;
                for (uint i = 0; i < n_waves; i++) total += _rb_wtot[i];
            }
            if (ok) {
                int pos = count + int(base + prefix);
                _rb_surv[pos] = gi;
                _rb_ssid[pos] = c_sid;
                _rb_sxy[pos] = c_xy;
                _rb_sconic[pos] = c_conic;
                _rb_sopac[pos] = c_opac;
            }
            count += int(total);
            s -= int(RB_THREADS);
        }
        scan_end = s;
        GroupMemoryBarrierWithGroupSync();

        // ---- sweep compacted survivors in batches of RB_THREADS ----
        // A partial batch costs a whole diagonal sweep, so only the very last
        // one is allowed to be short; the tail rides into the next round.
        bool drain = scan_end < range_start;
        int num_batches = drain ? (count + (int)RB_THREADS - 1) / (int)RB_THREADS
                                : count / (int)RB_THREADS;
        for (int b = 0; b < num_batches; b++) {
            int surv_pos = b * (int)RB_THREADS + int(tid);
            bool active = surv_pos < count;
            int splat_idx = active ? _rb_surv[surv_pos] : (range_start - 1);
            int batch_size = min((int)RB_THREADS, count - b * (int)RB_THREADS);

            Frag2d_ splat = {float2(0.0f), 0.0f, float3(0.0f), 0.0f,
                             float3(0.0f)};
            uint splat_sid = 0;
            if (active) {
                // Position, conic and opacity come from the cull's gather;
                // only the two channels it had no use for are fetched again.
                splat_sid = uint(_rb_ssid[surv_pos]);
                splat.xy = _rb_sxy[surv_pos];
                splat.conic = _rb_sconic[surv_pos];
                splat.opac = _rb_sopac[surv_pos];
                float* s_row = scr2_row(p.s_screen, int(splat_sid));
                splat.depth = scr2_depth(s_row);
                splat.rgb = scr2_rgb(s_row);
            }
            Frag2d_ v_splat = {float2(0.0f), 0.0f, float3(0.0f), 0.0f,
                               float3(0.0f)};
            float accum_weight = 0.0f;
            float accum_weight_den = 0.0f;
            bool touched = false;

            for (int t = 0; t < batch_size + (int)RB_BLOCK - 1; t++) {
                _rb2_step(t - int(tid), splat_idx, active, wg.y, wg.z, splat,
                          v_splat, accum_weight, accum_weight_den, touched);
                GroupMemoryBarrierWithGroupSync();
            }

            if (active && touched) {
                uint wid = kPacked != 0 ? uint(p.gaussian_ids[splat_sid])
                                        : splat_sid % p.N;
                float* v_row = scr2_row(p.v_s_screen, int(splat_sid));
                _rb_atomic_add(v_row + SCR2_XY, v_splat.xy.x);
                _rb_atomic_add(v_row + SCR2_XY + 1, v_splat.xy.y);
                _rb_atomic_add(v_row + SCR2_CONIC, v_splat.conic.x);
                _rb_atomic_add(v_row + SCR2_CONIC + 1, v_splat.conic.y);
                _rb_atomic_add(v_row + SCR2_CONIC + 2, v_splat.conic.z);
                _rb_atomic_add(v_row + SCR2_OPAC, v_splat.opac);
                _rb_atomic_add(v_row + SCR2_DEPTH, v_splat.depth);
                _rb_atomic_add(v_row + SCR2_RGB, v_splat.rgb.x);
                _rb_atomic_add(v_row + SCR2_RGB + 1, v_splat.rgb.y);
                _rb_atomic_add(v_row + SCR2_RGB + 2, v_splat.rgb.z);
                if (kAccumWeight == kAccumMax)
                    atomic_max_f32_nonneg(p.o_accum_weight + wid, accum_weight);
                else if (kAccumWeight != 0) {
                    atomic_add_f32((float*)p.o_accum_weight + wid, accum_weight);
                    if (kAccumWeight == kAccumAvg)
                        atomic_add_f32(p.o_accum_weight_den + wid,
                                       accum_weight_den);
                }
            }
        }
        GroupMemoryBarrierWithGroupSync();  // done reading surv[]
        int rem = max(0, count - num_batches * (int)RB_THREADS);
        int t_surv = 0, t_ssid = 0;
        float2 t_xy = float2(0.0f);
        float3 t_conic = float3(0.0f);
        float t_opac = 0.0f;
        if (int(tid) < rem) {
            int src = num_batches * (int)RB_THREADS + int(tid);
            t_surv = _rb_surv[src];
            t_ssid = _rb_ssid[src];
            t_xy = _rb_sxy[src];
            t_conic = _rb_sconic[src];
            t_opac = _rb_sopac[src];
        }
        GroupMemoryBarrierWithGroupSync();
        if (int(tid) < rem) {
            _rb_surv[tid] = t_surv;
            _rb_ssid[tid] = t_ssid;
            _rb_sxy[tid] = t_xy;
            _rb_sconic[tid] = t_conic;
            _rb_sopac[tid] = t_opac;
        }
        count = rem;
        GroupMemoryBarrierWithGroupSync();
    }
}

// ---------------------------------------------------------------------------
// 3DGUT backward
// ---------------------------------------------------------------------------

struct Raster3dgutBwdParams {  // via params ring
    float* means;
    float* quats;
    float* scales;    // WORLD scales (fragment load)
    float* s_screen;    // [.., SCRG_STRIDE]; "scale" slot = 2D conic
    int* gaussian_ids;
    float* viewmats;     // [I,16]
    float4* intrins;     // [I]
    float* dist_coeffs;  // [I,8] (fallback-backed)
    uint2* aabb;         // packed, aabb16.slang
    int* tile_offsets;
    int* flatten_ids;
    float* render_Ts;
    int* last_ids;
    float* out_rgb;
    float* out_depth;
    float* dist_rgb;
    float* dist_depth;
    float* awmap;
    float* v_out_rgb;
    float* v_out_depth;
    float* v_render_Ts;
    float* v_median;
    float* v_dist_rgb;
    float* v_dist_depth;
    float* v_means;
    float* v_quats;
    float* v_scales;
    float* v_s_screen;  // [.., SCRG_STRIDE]
    uint* o_accum_weight;  // kAccumWeight; Max writes float bits, else float
    float* o_accum_weight_den;  // kAccumAvg lane 2
    float* v_viewmats;   // [I,16], kViewmatGrad
    uint32_t I;
    uint32_t N;
    uint32_t n_isects;
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t macro_log2;  // was _pad0; the struct size is unchanged
};

struct Frag3dgut_ {
    float3 mean;
    float3x3 iscl_rot;
    float opacity;
    float3 rgb;
};

struct VFrag3dgut_ {
    float3 mean;
    float3x3 iscl_rot;
    float opacity;
    float3 rgb;
};

// One sweep step for the 3dgut entry.
[ForceInline]
void _rb3_step(Raster3dgutBwdParams p, int pix_id, int splat_idx,
               bool active, float3 ray_o, Frag3dgut_ splat,
               inout VFrag3dgut_ v_splat, inout float accum_weight,
               inout float accum_weight_den,
               inout float3 total_v_ray_o) {
    if (pix_id < 0 || pix_id >= (int)RB_BLOCK || !active) return;
    float4 rb = _rb_raybin[pix_id];
    if (splat_idx > asint(rb.w)) return;
    float3 ray_d = rb.xyz;

    float alpha = evaluate_alpha_3dgs(splat.mean, splat.iscl_rot,
                                      splat.opacity, ray_o, ray_d);
    if (alpha <= ALPHA_THRESHOLD || dot(ray_d, ray_d) == 0.0f) return;

    float3 c_rgb;
    float c_depth;
    evaluate_color_3dgs(splat.mean, splat.iscl_rot, splat.opacity, splat.rgb,
                        ray_o, ray_d, c_rgb, c_depth);
    if (c_depth <= 0.0f) return;
    float4 color = float4(c_rgb, c_depth);

    float v_alpha;
    float4 v_color;
    _rb_pixel_grad(pix_id, alpha, color, v_alpha, v_color);

    float3 v_mean;
    float3x3 v_ir;
    float v_op;
    float3 v_ray_o_a, v_ray_d_a;
    evaluate_alpha_3dgs_vjp(splat.mean, splat.iscl_rot, splat.opacity, ray_o,
                            ray_d, v_alpha, v_mean, v_ir, v_op, v_ray_o_a,
                            v_ray_d_a);
    v_splat.mean += v_mean;
    v_splat.iscl_rot += v_ir;
    v_splat.opacity += v_op;

    float3 v_rgb;
    float3 v_ray_o_c, v_ray_d_c;
    evaluate_color_3dgs_vjp(splat.mean, splat.iscl_rot, splat.opacity,
                            splat.rgb, ray_o, ray_d, v_color.xyz, v_color.w,
                            v_mean, v_ir, v_op, v_rgb, v_ray_o_c, v_ray_d_c);
    v_splat.mean += v_mean;
    v_splat.iscl_rot += v_ir;
    v_splat.opacity += v_op;
    v_splat.rgb += v_rgb;

    if (kAccumWeight != 0) {
        float T0 = _rb_Tg[pix_id].x;
        float w = alpha * T0;
        if (kAccumWeight == kAccumMax)
            accum_weight = max(accum_weight, _rb_awmap[pix_id] * w);
        else {
            accum_weight += _rb_awmap[pix_id] * w;
            if (kAccumWeight == kAccumAvg) accum_weight_den += w;
        }
    }
    if (kViewmatGrad != 0) {
        total_v_ray_o += v_ray_o_a + v_ray_o_c;
        _rb_vrayd[pix_id] += v_ray_d_a + v_ray_d_c;
    }
}

// Workgroup-reduce `val` over the 32 threads and atomically add the sum to
// `dst` (thread 0 issues the add). Deterministic serial combine.
[ForceInline]
void _rb3_reduce_add(float* dst, float val, uint tid) {
    GroupMemoryBarrierWithGroupSync();
    _rb_red[tid] = _rb_finitef(val) ? val : 0.0f;
    GroupMemoryBarrierWithGroupSync();
    if (tid == 0) {
        float s = 0.0f;
        for (uint i = 0; i < RB_THREADS; i++) s += _rb_red[i];
        if (s != 0.0f) atomic_add_f32(dst, s);
    }
}

#define _rb_ray(D)                                                    \
    ray_ok = pixel_ray<D>(p.dist_coeffs, image_id, kCameraModel,      \
                          float2((px - intrin.z) / intrin.x,          \
                                 (py - intrin.w) / intrin.y), raydir)

[shader("compute")]
[numthreads(32, 1, 1)]
void rasterize_bwd_3dgut(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform Raster3dgutBwdParams* pp) {
    Raster3dgutBwdParams p = *pp;
    uint image_id = wg.x;
    uint tile_id = macro_tile_id(wg.y * TILE_SIZE_DY / TILE_SIZE_Y,
                                 wg.z * TILE_SIZE_DX / TILE_SIZE_X,
                                 p.tile_width, p.macro_log2);
    uint img_off = image_id * p.height * p.width;

    float* vm = p.viewmats + image_id * 16;
    float3x3 R;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
    float3 tvec = float3(vm[3], vm[7], vm[11]);
    float4 intrin = p.intrins[image_id];
    float3 ray_o = transform_ray_o(R, tvec);
    float3 total_v_ray_o = float3(0.0f);

    // ---- load pixel state (2 pixels per thread) ----
    if (tid == 0) _rb_binmax = 0u;
    GroupMemoryBarrierWithGroupSync();
    int bin_local = -1;
    for (uint pix0 = 0; pix0 < RB_BLOCK; pix0 += RB_THREADS) {
        uint pix = pix0 + tid;
        int gx = int(wg.z * TILE_SIZE_DX) + int(pix % TILE_SIZE_DX);
        int gy = int(wg.y * TILE_SIZE_DY) + int(pix / TILE_SIZE_DX);
        uint gpix = uint(gy) * p.width + uint(gx);
        bool inside = (gx < int(p.width) && gy < int(p.height));
        int bin_final = inside ? p.last_ids[img_off + gpix] : -1;
        bin_local = max(bin_local, bin_final);
        float px = float(gx) + 0.5f;
        float py = float(gy) + 0.5f;
        float3 raydir = float3(0.0f);
        bool ray_ok = false;
        SS_DISPATCH_DIST(_rb_ray)
        inside = inside && ray_ok;
        float3 ray_d = transform_ray_d(R, raydir);
        _rb_raybin[pix] = float4(ray_d, asfloat(bin_final));
        if (kViewmatGrad != 0) _rb_vrayd[pix] = float3(0.0f);
        _rb_load_pixel_state(
            pix, inside, img_off + gpix, p.render_Ts + img_off,
            p.v_render_Ts + img_off, gpix, p.out_rgb, p.out_depth,
            p.dist_rgb, p.dist_depth, p.v_out_rgb, p.v_out_depth,
            p.v_dist_rgb, p.v_dist_depth, p.awmap, p.v_median);
    }
    { uint prev; InterlockedMax(_rb_binmax, uint(bin_local + 1), prev); }
    GroupMemoryBarrierWithGroupSync();
    int bin_max = int(_rb_binmax) - 1;

    int range_start = p.tile_offsets[image_id * p.tile_height * p.tile_width +
                                     tile_id];
    int range_end =
        (image_id == p.I - 1 &&
         tile_id == p.tile_width * p.tile_height - 1)
            ? int(p.n_isects)
            : p.tile_offsets[image_id * p.tile_height * p.tile_width +
                             tile_id + 1];

    float cull_bx0 = float(wg.z * TILE_SIZE_DX);
    float cull_by0 = float(wg.y * TILE_SIZE_DY);
    uint wsize = WaveGetLaneCount();
    uint n_waves = RB_THREADS / wsize;

    // Nothing past the tile's deepest surviving fragment can contribute.
    int count = 0;
    for (int scan_end = min(range_end - 1, bin_max); scan_end >= range_start || count > 0;) {
        int s = scan_end;
        while (s >= range_start && count <= SURV_CAP - (int)RB_THREADS) {
            int gi = s - int(tid);
            bool ok = false;
            int c_sid = 0;
            if (gi >= range_start) {
                int sid = p.flatten_ids[gi];
                c_sid = sid;
                float* c_row = scrg_row(p.s_screen, sid);
                float c_opac = scrg_opac(c_row);
                // 3dgut keeps the projected 2D conic in the screen "scale"
                // slot; ellipse center is the AABB center.
                float3 c_conic = scrg_scale(c_row);
                float2 c_ec = aabb16_center(p.aabb[sid], p.width, p.height);
                float c_ex = c_ec.x, c_ey = c_ec.y;
                if (c_opac > ALPHA_THRESHOLD) {
                    float kk = 0.5f / log(c_opac / ALPHA_THRESHOLD);
                    float3 inv_cov = c_conic * kk;
                    ok = ellipse_box_overlap_test(
                        inv_cov, cull_bx0 - c_ex,
                        cull_bx0 + TILE_SIZE_DX - c_ex, cull_by0 - c_ey,
                        cull_by0 + TILE_SIZE_DY - c_ey);
                }
            }
            uint prefix = WavePrefixCountBits(ok);
            uint wtotal = WaveActiveCountBits(ok);
            uint base = 0;
            uint total = wtotal;
            if (n_waves > 1) {  // one wave covers the group: no roundtrip
                GroupMemoryBarrierWithGroupSync();
                if (WaveGetLaneIndex() == 0) _rb_wtot[tid / wsize] = wtotal;
                GroupMemoryBarrierWithGroupSync();
                for (uint i = 0; i < tid / wsize; i++) base += _rb_wtot[i];
                total = 0;
                for (uint i = 0; i < n_waves; i++) total += _rb_wtot[i];
            }
            if (ok) {
                int pos = count + int(base + prefix);
                _rb_surv[pos] = gi;
                _rb_ssid[pos] = c_sid;
            }
            count += int(total);
            s -= int(RB_THREADS);
        }
        scan_end = s;
        GroupMemoryBarrierWithGroupSync();

        // A partial batch costs a whole diagonal sweep, so only the very last
        // one is allowed to be short; the tail rides into the next round.
        bool drain = scan_end < range_start;
        int num_batches = drain ? (count + (int)RB_THREADS - 1) / (int)RB_THREADS
                                : count / (int)RB_THREADS;
        for (int b = 0; b < num_batches; b++) {
            int surv_pos = b * (int)RB_THREADS + int(tid);
            bool active = surv_pos < count;
            int splat_idx = active ? _rb_surv[surv_pos] : (range_start - 1);
            int batch_size = min((int)RB_THREADS, count - b * (int)RB_THREADS);

            Frag3dgut_ splat;
            splat.mean = float3(0.0f);
            splat.iscl_rot = float3x3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                      0.0f, 0.0f, 1.0f);
            splat.opacity = 0.0f;
            splat.rgb = float3(0.0f);
            uint splat_sid = 0;
            uint splat_wid = 0;
            float4 quat = float4(1, 0, 0, 0);
            float3 wscale = float3(0.0f);
            if (active) {
                splat_sid = uint(_rb_ssid[surv_pos]);
                splat_wid = kPacked != 0 ? uint(p.gaussian_ids[splat_sid])
                                         : splat_sid % p.N;
                splat.mean = float3(p.means[3 * splat_wid],
                                    p.means[3 * splat_wid + 1],
                                    p.means[3 * splat_wid + 2]);
                quat = float4(p.quats[4 * splat_wid],
                              p.quats[4 * splat_wid + 1],
                              p.quats[4 * splat_wid + 2],
                              p.quats[4 * splat_wid + 3]);
                wscale = float3(p.scales[3 * splat_wid],
                                p.scales[3 * splat_wid + 1],
                                p.scales[3 * splat_wid + 2]);
                splat.iscl_rot = compute_3dgut_iscl_rot(quat, wscale);
                float* s_row = scrg_row(p.s_screen, int(splat_sid));
                splat.opacity = scrg_opac(s_row);
                splat.rgb = scrg_rgb(s_row);
            }
            VFrag3dgut_ v_splat;
            v_splat.mean = float3(0.0f);
            v_splat.iscl_rot = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        0.0f, 0.0f, 0.0f);
            v_splat.opacity = 0.0f;
            v_splat.rgb = float3(0.0f);
            float accum_weight = 0.0f;
            float accum_weight_den = 0.0f;

            for (int t = 0; t < batch_size + (int)RB_BLOCK - 1; t++) {
                _rb3_step(p, t - int(tid), splat_idx, active, ray_o, splat,
                          v_splat, accum_weight, accum_weight_den,
                          total_v_ray_o);
                GroupMemoryBarrierWithGroupSync();
            }

            if (active) {
                float4 v_quat;
                float3 v_scale;
                compute_3dgut_iscl_rot_vjp(quat, wscale, v_splat.iscl_rot,
                                           v_quat, v_scale);
                _rb_atomic_add(p.v_means + 3 * splat_wid, v_splat.mean.x);
                _rb_atomic_add(p.v_means + 3 * splat_wid + 1, v_splat.mean.y);
                _rb_atomic_add(p.v_means + 3 * splat_wid + 2, v_splat.mean.z);
                _rb_atomic_add(p.v_quats + 4 * splat_wid, v_quat.x);
                _rb_atomic_add(p.v_quats + 4 * splat_wid + 1, v_quat.y);
                _rb_atomic_add(p.v_quats + 4 * splat_wid + 2, v_quat.z);
                _rb_atomic_add(p.v_quats + 4 * splat_wid + 3, v_quat.w);
                _rb_atomic_add(p.v_scales + 3 * splat_wid, v_scale.x);
                _rb_atomic_add(p.v_scales + 3 * splat_wid + 1, v_scale.y);
                _rb_atomic_add(p.v_scales + 3 * splat_wid + 2, v_scale.z);
                float* v_row = scrg_row(p.v_s_screen, int(splat_sid));
                _rb_atomic_add(v_row + SCRG_OPAC, v_splat.opacity);
                _rb_atomic_add(v_row + SCRG_RGB, v_splat.rgb.x);
                _rb_atomic_add(v_row + SCRG_RGB + 1, v_splat.rgb.y);
                _rb_atomic_add(v_row + SCRG_RGB + 2, v_splat.rgb.z);
                if (kAccumWeight == kAccumMax)
                    atomic_max_f32_nonneg(p.o_accum_weight + splat_wid,
                                          accum_weight);
                else if (kAccumWeight != 0) {
                    atomic_add_f32((float*)p.o_accum_weight + splat_wid,
                                   accum_weight);
                    if (kAccumWeight == kAccumAvg)
                        atomic_add_f32(p.o_accum_weight_den + splat_wid,
                                       accum_weight_den);
                }
            }
        }

        int rem = max(0, count - num_batches * (int)RB_THREADS);
        GroupMemoryBarrierWithGroupSync();
        int t_surv = 0, t_ssid = 0;
        if (int(tid) < rem) {
            int src = num_batches * (int)RB_THREADS + int(tid);
            t_surv = _rb_surv[src];
            t_ssid = _rb_ssid[src];
        }
        GroupMemoryBarrierWithGroupSync();
        if (int(tid) < rem) {
            _rb_surv[tid] = t_surv;
            _rb_ssid[tid] = t_ssid;
        }
        count = rem;
        GroupMemoryBarrierWithGroupSync();
    }

    // ---- viewmat gradient tail ----
    if (kViewmatGrad != 0) {
        float3x3 v_R;
        float3 v_t;
        transform_ray_o_vjp(R, tvec, total_v_ray_o, v_R, v_t);
        for (uint pix0 = 0; pix0 < RB_BLOCK; pix0 += RB_THREADS) {
            uint pix = pix0 + tid;
            float4 rb = _rb_raybin[pix];
            float3 raydir = undo_transform_ray_d(R, rb.xyz);
            float3x3 v_R_delta;
            float3 tmp;
            transform_ray_d_vjp(R, raydir, _rb_vrayd[pix], v_R_delta, tmp);
            v_R += v_R_delta;
        }
        float* v_viewmat = p.v_viewmats + image_id * 16;
        _rb3_reduce_add(v_viewmat + 0, v_R[0].x, tid);
        _rb3_reduce_add(v_viewmat + 1, v_R[0].y, tid);
        _rb3_reduce_add(v_viewmat + 2, v_R[0].z, tid);
        _rb3_reduce_add(v_viewmat + 3, v_t.x, tid);
        _rb3_reduce_add(v_viewmat + 4, v_R[1].x, tid);
        _rb3_reduce_add(v_viewmat + 5, v_R[1].y, tid);
        _rb3_reduce_add(v_viewmat + 6, v_R[1].z, tid);
        _rb3_reduce_add(v_viewmat + 7, v_t.y, tid);
        _rb3_reduce_add(v_viewmat + 8, v_R[2].x, tid);
        _rb3_reduce_add(v_viewmat + 9, v_R[2].y, tid);
        _rb3_reduce_add(v_viewmat + 10, v_R[2].z, tid);
        _rb3_reduce_add(v_viewmat + 11, v_t.z, tid);
    }
}

#undef _rb_ray

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
