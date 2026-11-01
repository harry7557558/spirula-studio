// Background blend, PPISP and the display encode as one kernel each way
// (Vulkan backend), mirroring kernels/pixelwise/AppearanceChain.cu. The
// backward recomputes the intermediates from the raw render.
//
// Spec constants (IDs by declaration order -- dispatches must pass ALL):
//   0 kBg: AppearanceBg      1 kPpisp: AppearancePpisp   2 kParamType
//   3 kClampOutput           4 kCsEnabled                5 kCsTransfer
//   6 kCsLinear              7 kBgTransfer               8 kBgLinear
//   9 kMatchLuma: the background's power table is bound

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "shaders/pixel_wise.slang"
#include "shaders/ppisp.slang"
#undef CudaDeviceExport
#include "atomic_float.slang"
#include "background_noise.slang"
#include "rgb_format.slang"

[SpecializationConstant] const int kBg = 0;
[SpecializationConstant] const int kPpisp = 0;
[SpecializationConstant] const int kParamType = 0;
[SpecializationConstant] const int kClampOutput = 0;
[SpecializationConstant] const int kCsEnabled = 0;
[SpecializationConstant] const int kCsTransfer = 0;
[SpecializationConstant] const int kCsLinear = 0;
[SpecializationConstant] const int kBgTransfer = 0;
[SpecializationConstant] const int kBgLinear = 0;
[SpecializationConstant] const int kMatchLuma = 0;

#include "ppisp_apply.slang"

static const int kBgColor = 1;
static const int kBgNoise = 2;
static const int kBgImage = 3;
static const int kPpispBeforeEncode = 1;
static const int kPpispAfterEncode = 2;

struct AppParams {
    uint* rgb;               // [B,H,W,3] raw render, read as rgb_fmt
    float* transmittance;    // [B,H,W,1]
    float* out_rgb;          // bwd: v_rgb, in and out
    uint* fwd_out;           // fwd: the output, written as out_fmt
    uint* raw16;             // fwd: a float16 copy of the raw render, or unbound
    float* v_transmittance;  // bwd, added into
    float* bg_image;         // kBgImage: [B,H,W,3]
    float* v_bg_image;       // kBgImage bwd: [B,H,W,3]
    float* exponent_by_cam;  // kMatchLuma
    int* cam_indices;        // [B] parameter slot per image
    float* cs_matrix;        // [3,3] row-major
    float* ppisp_params;     // [N_cam, P]
    float* v_ppisp_params;   // [N_cam, P], bwd adds
    float4* intrins;         // [B,4]
    float bg_r, bg_g, bg_b;  // kBgColor, working space
    float randomize_weight;
    uint seed, blocky, block_px;
    float oe_scale;          // bwd: 2 * weight / N
    int B, H, W;
    int has_cam_indices;     // else image b uses slot b
    uint wgs_per_row;        // bwd: pixel axis folds across (wg.x, wg.z), wg.y
                             // the batch; fwd: one flat axis over B*H*W
    uint rgb_fmt, out_fmt;   // core/PixelFormat.h
    uint raw16_on;
    uint _pad0;
};

[ForceInline]
float3 _app_ld3(float* b, uint i) {
    return float3(b[3 * i], b[3 * i + 1], b[3 * i + 2]);
}

[ForceInline]
void _app_st3(float* b, uint i, float3 v) {
    b[3 * i] = v.x;
    b[3 * i + 1] = v.y;
    b[3 * i + 2] = v.z;
}

[ForceInline]
float3x3 _app_matrix(float* m) {
    float3x3 cm;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        cm[r] = float3(m[3 * r], m[3 * r + 1], m[3 * r + 2]);
    return cm;
}

[ForceInline]
float3 _app_background(AppParams p, uint bid, uint gid, uint pix) {
    if (kBg == kBgColor) return float3(p.bg_r, p.bg_g, p.bg_b);
    if (kBg == kBgImage) return _app_ld3(p.bg_image, pix);
    return _bg_noise_color(p.blocky, p.block_px, p.seed, gid, bid, uint(p.W),
                           p.randomize_weight,
                           _bg_exponent(bid, uint(kMatchLuma), p.exponent_by_cam,
                                        p.cam_indices),
                           kBgTransfer, kBgLinear != 0);
}

[ForceInline]
float3 _app_encode(AppParams p, float3 c) {
    return working_to_display(c, _app_matrix(p.cs_matrix), kCsTransfer,
                              kCsLinear != 0);
}

[ForceInline]
void _app_load_params(AppParams p, uint bid, out int slot,
                      out float pbuf[kMaxParams]) {
    slot = p.has_cam_indices != 0 ? p.cam_indices[bid] : int(bid);
    int np = _pp_num_params();
    for (int i = 0; i < kMaxParams; i++)
        pbuf[i] = i < np ? p.ppisp_params[slot * np + i] : 0.0f;
}

[ForceInline]
float3 _app_forward(AppParams p, float3 c, uint pix, uint bid, uint gid) {
    if (kBg != 0)
        c = blend_background(c, p.transmittance[pix], _app_background(p, bid, gid, pix));
    if (kPpisp != 0) {
        int slot;
        float pbuf[kMaxParams];
        _app_load_params(p, bid, slot, pbuf);
        float4 intr = p.intrins[bid];
        float2 px = float2(float(gid % uint(p.W)), float(gid / uint(p.W)));
        float2 center = float2(intr.z, intr.w);
        float2 size = float2(float(p.W), float(p.H));
        if (kPpisp == kPpispBeforeEncode)
            c = _pp_apply(c, px, center, size, pbuf);
        if (kCsEnabled != 0)
            c = _app_encode(p, c);
        if (kPpisp == kPpispAfterEncode)
            c = _pp_apply(c, px, center, size, pbuf);
    } else if (kCsEnabled != 0) {
        c = _app_encode(p, c);
    }
    return c;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void appearance_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform AppParams* pp) {
    AppParams p = *pp;
    // Flat, so a lane's pixel parity is its lane parity (rgb_store3's pairs).
    uint hw = uint(p.H * p.W);
    uint pix = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    bool active = pix < uint(p.B) * hw;
    uint bid = active ? pix / hw : 0u;
    uint gid = active ? pix % hw : 0u;

    float3 raw = active ? rgb_load3(p.rgb, pix, p.rgb_fmt) : float3(0.0f);
    // From the copy on, the chain sees what its backward will replay from.
    if (p.raw16_on != 0) {
        raw = round_to_half3(raw);
        rgb_store3(p.raw16, pix, raw, kRgbF16, active);
    }
    float3 c = active ? _app_forward(p, raw, pix, bid, gid) : float3(0.0f);
    rgb_store3(p.fwd_out, pix, c, p.out_fmt, active);
}

static const uint kPpispReduceThreads = 256u;
#include "ppisp_reduce.slang"

[shader("compute")]
[numthreads(256, 1, 1)]
void appearance_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform AppParams* pp) {
    AppParams p = *pp;
    uint gid = (wg.z * p.wgs_per_row + wg.x) * 256 + tid;
    uint bid = wg.y;
    bool inside = bid < uint(p.B) && gid < uint(p.H * p.W);
    uint pix = inside ? bid * uint(p.H * p.W) + gid : 0u;
    uint x = inside ? gid % uint(p.W) : 0u, y = inside ? gid / uint(p.W) : 0u;

    int slot = 0;
    float pbuf[kMaxParams];
    float vbuf[kMaxParams];
    [ForceUnroll]
    for (int i = 0; i < kMaxParams; i++) { pbuf[i] = 0.0f; vbuf[i] = 0.0f; }
    if (kPpisp != 0 && bid < uint(p.B))
        _app_load_params(p, bid, slot, pbuf);

    if (inside) {
        float3 raw = rgb_load3(p.rgb, pix, p.rgb_fmt);
        float T = kBg != 0 ? p.transmittance[pix] : 0.0f;
        float3 bg = kBg != 0 ? _app_background(p, bid, gid, pix) : float3(0.0f);
        float3 c_bg = kBg != 0 ? blend_background(raw, T, bg) : raw;
        float3 v = _app_ld3(p.out_rgb, pix);

        // Every stage is linear in the incoming gradient, so a zero one -- a
        // masked pixel, mostly -- skips the recompute. Not a return: the
        // block reduction below needs every thread.
        if (any(v != float3(0.0f))) {
            float4 intr = kPpisp != 0 ? p.intrins[bid] : float4(0.0f);
            float2 px = float2(float(x), float(y));
            float2 center = float2(intr.z, intr.w);
            float2 size = float2(float(p.W), float(p.H));
            float3 c_pre = c_bg;
            if (kPpisp == kPpispBeforeEncode)
                c_pre = _pp_apply(c_bg, px, center, size, pbuf);
            if (kPpisp == kPpispAfterEncode) {
                float3 c_enc = kCsEnabled != 0 ? _app_encode(p, c_pre) : c_pre;
                float3 vc;
                float vs[kMaxParams];
                _pp_apply_vjp(c_enc, px, center, size, pbuf, v, vc, vs);
                v = vc;
                [ForceUnroll]
                for (int i = 0; i < kMaxParams; i++) vbuf[i] += vs[i];
            }
            if (kCsEnabled != 0)
                v = working_to_display_bwd(c_pre, _app_matrix(p.cs_matrix),
                                           kCsTransfer, kCsLinear != 0, v);
            if (kPpisp == kPpispBeforeEncode) {
                float3 vc;
                float vs[kMaxParams];
                _pp_apply_vjp(c_bg, px, center, size, pbuf, v, vc, vs);
                v = vc;
                [ForceUnroll]
                for (int i = 0; i < kMaxParams; i++) vbuf[i] += vs[i];
            }
        }

        if (kBg != 0) {
            float3 v_raw, v_bg;
            float v_T;
            blend_background_bwd(raw, T, bg, v, p.oe_scale, v_raw, v_T, v_bg);
            v = v_raw;
            p.v_transmittance[pix] = p.v_transmittance[pix] + v_T;
            if (kBg == kBgImage) _app_st3(p.v_bg_image, pix, v_bg);
        } else if (p.oe_scale != 0.0f) {
            v += overexposure_grad(raw, p.oe_scale);
        }
        _app_st3(p.out_rgb, pix, v);
    }

    if (kPpisp != 0) {
        int np = _pp_num_params();
        [ForceUnroll]
        for (int i = 0; i < kMaxParams; i++) {
            if (i >= np) continue;
            float g = _pp_finite(vbuf[i]) ? vbuf[i] : 0.0f;
            float sum = ppisp_reduce_sum(g, tid);
            if (tid == 0 && sum != 0.0f)
                atomic_add_f32(&p.v_ppisp_params[slot * np + i], sum);
        }
    }
}
