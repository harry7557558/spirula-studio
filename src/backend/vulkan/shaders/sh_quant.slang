// SH value-quantization decode for the Vulkan backend (q8 / q16).
//
// Math is a verbatim mirror of harmonics.slang's _sh_load_q{8,16} +
// sh_coeffs_to_color_q{8,16} (same operation order, so CUDA/Vulkan parity is
// exact modulo compiler codegen). One deliberate difference: the packed
// uint8 buffer goes through int8_compat.slang (u32-word extraction on the
// baseline, native byte loads in the ".int8" blob variant) and the uint16
// buffer is always read as u32 WORDS — 16-bit storage is not in the device
// baseline. Device allocations are 16-byte rounded, so word reads past the
// last cell stay in bounds.
//
#include "int8_compat.slang"
#include "sh_addr.slang"

// Include AFTER harmonics.slang (uses mean_viewmat_to_unnormalized_viewdir).
// BITS is 8 or 16, always a fold-time constant (spec-constant switch in the
// caller), so _sh_fetch_qw's branch disappears at pipeline creation.

// Raw quantized cell fetch: cell index -> integer code (0..2^BITS-1).
[ForceInline]
uint _sh_fetch_qw<let BITS : int>(uint* packed, gindex_t cell) {
    if (BITS == 8) {
        return u8_load(packed, cell);
    } else {
        uint word = elem_at(packed, cell >> 1)[0];
        return (word >> uint(16 * uint(cell & 1))) & 0xffffu;
    }
}

// Mirrors _sh_load_q{8,16}: decode one float3 SH coefficient (basis j).
[ForceInline]
float3 _sh_load_qw<let BITS : int>(uint* packed, float2* bounds, gindex_t base,
                                   int j, gindex_t bounds_stride,
                                   gindex_t pair_pitch) {
    gindex_t cell0 = sh_cell_at_w(base, 3 * j, pair_pitch);
    gindex_t cell1 = sh_cell_at_w(base, 3 * j + 1, pair_pitch);
    gindex_t cell2 = sh_cell_at_w(base, 3 * j + 2, pair_pitch);
    // Both strides are a multiple of 256, so shifting cell and stride down
    // by 8 leaves the quotient exact and the numerator in u32; a 64-bit
    // divide is not an option, Intel/ANV hangs lowering the one slang emits.
    uint sblk = uint(bounds_stride >> 8);
    float2 mm0 = bounds[uint(cell0 >> 8) / sblk];
    float2 mm1 = bounds[uint(cell1 >> 8) / sblk];
    float2 mm2 = bounds[uint(cell2 >> 8) / sblk];
    const float inv = BITS == 8 ? (1.0f / 255.0f) : (1.0f / 65535.0f);
    return float3(
        mm0.x + (mm0.y - mm0.x) * (float(_sh_fetch_qw<BITS>(packed, cell0)) * inv),
        mm1.x + (mm1.y - mm1.x) * (float(_sh_fetch_qw<BITS>(packed, cell1)) * inv),
        mm2.x + (mm2.y - mm2.x) * (float(_sh_fetch_qw<BITS>(packed, cell2)) * inv)
    );
}

// Mirrors sh_coeffs_to_color_q{8,16} (direction form): body copied verbatim
// with the load helper swapped for the word-based one.
[ForceInline]
float3 sh_coeffs_to_color_qw<let BITS : int>(
    const int degree,
    const float3 viewdir,
    float3 coeff_dc,
    uint* packed,
    float2* bounds,
    gindex_t base,
    gindex_t bounds_stride,
    gindex_t pair_pitch
) {
    #define _C(j) _sh_load_qw<BITS>(packed, bounds, base, j, bounds_stride, pair_pitch)
    float3 colors = 0.2820947917738781f * coeff_dc;

    if (degree <= 0)
        return colors;

    float inv_norm = rsqrt(
        viewdir.x * viewdir.x + viewdir.y * viewdir.y + viewdir.z * viewdir.z
    );
    float x = viewdir.x * inv_norm;
    float y = viewdir.y * inv_norm;
    float z = viewdir.z * inv_norm;

    float fTmp0A = 0.48860251190292f;
    colors += fTmp0A *
            (-y * _C(0) +
            z * _C(1) -
            x * _C(2));

    if (degree <= 1)
        return colors;

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fTmp1A = 0.5462742152960395f;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = fTmp1A * fC1;
    float pSH4 = fTmp1A * fS1;
    colors +=
        pSH4 * _C(3) + pSH5 * _C(4) +
        pSH6 * _C(5) + pSH7 * _C(6) +
        pSH8 * _C(7);

    if (degree <= 2)
        return colors;

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fTmp2A = -0.5900435899266435f;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = fTmp2A * fC2;
    float pSH9  = fTmp2A * fS2;
    colors += pSH9  * _C(8) +
            pSH10 * _C(9) +
            pSH11 * _C(10) +
            pSH12 * _C(11) +
            pSH13 * _C(12) +
            pSH14 * _C(13) +
            pSH15 * _C(14);

    if (degree <= 3)
        return colors;

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 - 1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;

    colors += pSH16 * _C(15) +
                pSH17 * _C(16) +
                pSH18 * _C(17) +
                pSH19 * _C(18) +
                pSH20 * _C(19) +
                pSH21 * _C(20) +
                pSH22 * _C(21) +
                pSH23 * _C(22) +
                pSH24 * _C(23);

    #undef _C
    return colors;
}

// Mirrors the (mean, R, t)-form sh_coeffs_to_color_q{8,16} — i.e. what the
// CUDA projection reaches through sh{N}_to_color_q{8,16}: viewdir from the
// splat mean + camera, then the +0.5 shift and clamp.
[ForceInline]
float3 sh_to_color_qw<let BITS : int>(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint* packed, float2* bounds, gindex_t base, gindex_t bounds_stride,
    gindex_t pair_pitch
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color_qw<BITS>(degree, viewdir, coeff_dc,
                                             packed, bounds, base,
                                             bounds_stride, pair_pitch);
    return max(rgb + 0.5, 0.0);
}
