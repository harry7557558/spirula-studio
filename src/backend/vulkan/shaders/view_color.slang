#pragma once

// A splat's view-dependent colour on the Vulkan backend: `degree` SH bands,
// then `lobes` spherical-beta lobes starting at float3 coefficient `sb_base`
// of its features_sh row (core/ColorBasis.h), read in fp32 or through the
// q8/q16 value codec. The forward and the VJP every projection kernel calls.
//
// Include AFTER harmonics.slang, sh_quant.slang and sh_vjp.slang.

#include "shaders/spherical_beta.slang"

static const int kVcMaxShCoeffs = 24;
static const int kVcMaxSbCoeffs = 16;  // ColorBasis::kMaxSbLobes * 2

// Where one splat's row is read from: `fp32` already offset to the row, or
// the codec's packed cells with the row's base cell.
struct ColorRow {
    float* fp32;
    uint* packed;
    float2* bounds;
    gindex_t base;
    gindex_t bounds_stride;
    gindex_t pair_pitch;
};

[ForceInline]
float3 color_row_load<let QBITS : int>(ColorRow r, int j) {
    return _shv_C<QBITS>(r.fp32, r.packed, r.bounds, r.base, r.bounds_stride,
                         r.pair_pitch, j);
}

struct ColorRowReader<let QBITS : int> : IShCoefficientReader {
    ColorRow row;

    [ForceInline]
    float3 load(int j) {
        return color_row_load<QBITS>(row, j);
    }
}

[ForceInline]
float3 sb_row_color<let QBITS : int>(int lobes, int sb_base, float3 dir,
                                     ColorRow r) {
    float3 c = float3(0.0f);
    for (int l = 0; l < lobes; l++)
        c += sb_lobe_color(dir, color_row_load<QBITS>(r, sb_base + 2 * l),
                           color_row_load<QBITS>(r, sb_base + 2 * l + 1));
    return c;
}

// The rgb a splat shows the camera (R, t). The SH half is the same call the
// SH-only kernels make, so a row without lobes renders bit for bit as before.
[ForceInline]
float3 color_eval<let QBITS : int>(int degree, int lobes, int sb_base,
                                   float3 mean, float3x3 R, float3 t,
                                   float3 dc, ColorRow r) {
    float3 rgb;
    if (QBITS == 8 || QBITS == 16) {
        rgb = sh_to_color_qw<QBITS>(degree, mean, R, t, dc, r.packed,
                                    r.bounds, r.base, r.bounds_stride,
                                    r.pair_pitch);
    } else {
        switch (degree) {
        case 0: rgb = sh0_to_color(mean, R, t, dc, r.fp32); break;
        case 1: rgb = sh1_to_color(mean, R, t, dc, r.fp32); break;
        case 2: rgb = sh2_to_color(mean, R, t, dc, r.fp32); break;
        case 3: rgb = sh3_to_color(mean, R, t, dc, r.fp32); break;
        default: rgb = sh4_to_color(mean, R, t, dc, r.fp32); break;
        }
    }
    if (lobes > 0)
        rgb += sb_row_color<QBITS>(
            lobes, sb_base,
            normalize(mean_viewmat_to_unnormalized_viewdir(mean, R, t)), r);
    return rgb;
}

// The SH clamp excludes the lobes; both halves share the view-direction VJP.
[ForceInline]
void color_vjp<let QBITS : int>(
    int degree, int lobes, int sb_base,
    float3 mean, float3x3 R, float3 t, float3 dc, ColorRow r,
    float3 v_colors,
    inout float3 v_dc,
    inout float3 v_sh[kVcMaxShCoeffs],
    inout float3 v_sb[kVcMaxSbCoeffs],
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t) {
    ColorRowReader<QBITS> C;
    C.row = r;

    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = _shv_forward(degree, viewdir, dc, C);
    float3 v_sh_colors =
        v_colors * float3(float(rgb.x >= -0.5f), float(rgb.y >= -0.5f),
                          float(rgb.z >= -0.5f));

    float3 v_viewdir = float3(0.0f);
    _shv_dir_vjp(degree, viewdir, C, v_sh_colors, v_dc, v_sh, v_viewdir);

    if (lobes > 0) {
        float inorm = rsqrt(dot(viewdir, viewdir));
        float3 dir = viewdir * inorm;
        float3 v_dir = float3(0.0f);
        for (int l = 0; l < lobes; l++)
            sb_lobe_vjp(dir, color_row_load<QBITS>(r, sb_base + 2 * l),
                        color_row_load<QBITS>(r, sb_base + 2 * l + 1),
                        v_colors, v_sb[2 * l], v_sb[2 * l + 1], v_dir);
        v_viewdir += (v_dir - dot(v_dir, dir) * dir) * inorm;
    }

    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(mean_viewmat_to_unnormalized_viewdir)(dp_mean, dp_R, dp_t,
                                                   v_viewdir);
    v_mean += dp_mean.d;
    v_R += dp_R.d;
    v_t += dp_t.d;
}
