// SH colour VJP for the Vulkan backend. Coefficients are read on demand so
// the clamp evaluation and direction VJP do not retain a full decoded row.
// Include after harmonics.slang and sh_quant.slang.

#include "int64_compat.slang"

[ForceInline]
float3 _shv_C<let QBITS : int>(float* coeffs, uint* qpacked, float2* qbounds,
                               gindex_t qbase, gindex_t qstride,
                               gindex_t qpitch, int j) {
    if (QBITS == 8 || QBITS == 16)
        return _sh_load_qw<QBITS>(qpacked, qbounds, qbase, j, qstride, qpitch);
    return float3(coeffs[3 * j], coeffs[3 * j + 1], coeffs[3 * j + 2]);
}

interface IShCoefficientReader {
    float3 load(int j);
}

// Raw colour for the clamp mask; operation order matches shaders/harmonics.slang.
[ForceInline]
float3 _shv_forward<R : IShCoefficientReader>(
    const int degree, const float3 viewdir, float3 coeff_dc, R C) {
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
    colors += fTmp0A * (-y * C.load(0) + z * C.load(1) - x * C.load(2));

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
    colors += pSH4 * C.load(3) + pSH5 * C.load(4) + pSH6 * C.load(5) + pSH7 * C.load(6) +
              pSH8 * C.load(7);

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
    colors += pSH9 * C.load(8) + pSH10 * C.load(9) + pSH11 * C.load(10) + pSH12 * C.load(11) +
              pSH13 * C.load(12) + pSH14 * C.load(13) + pSH15 * C.load(14);

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

    colors += pSH16 * C.load(15) + pSH17 * C.load(16) + pSH18 * C.load(17) + pSH19 * C.load(18) +
              pSH20 * C.load(19) + pSH21 * C.load(20) + pSH22 * C.load(21) + pSH23 * C.load(22) +
              pSH24 * C.load(23);

    return colors;
}

[ForceInline]
void _shv_dir_vjp<R : IShCoefficientReader>(const int degree, const float3 dir, R C,
                  const float3 v_colors, inout float3 v_coeff_dc,
                  inout float3 v_coeffs[24], inout float3 v_dir) {
    #define sv(i, v) v_coeffs[i] += (v)

    v_coeff_dc += 0.2820947917738781f * v_colors;
    if (degree <= 0)
        return;

    float inorm = rsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    float x = dir.x * inorm;
    float y = dir.y * inorm;
    float z = dir.z * inorm;
    float v_x = 0.f, v_y = 0.f, v_z = 0.f;

    sv(0, -0.48860251190292f * y * v_colors);
    sv(1, 0.48860251190292f * z * v_colors);
    sv(2, -0.48860251190292f * x * v_colors);

    v_x += -0.48860251190292f * dot(C.load(2), v_colors);
    v_y += -0.48860251190292f * dot(C.load(0), v_colors);
    v_z += 0.48860251190292f * dot(C.load(1), v_colors);

    if (degree <= 1) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = 0.5462742152960395f * fC1;
    float pSH4 = 0.5462742152960395f * fS1;
    sv(3, pSH4 * v_colors);
    sv(4, pSH5 * v_colors);
    sv(5, pSH6 * v_colors);
    sv(6, pSH7 * v_colors);
    sv(7, pSH8 * v_colors);

    float fTmp0B_z, fC1_x, fC1_y, fS1_x, fS1_y, pSH6_z, pSH7_x, pSH7_z, pSH5_y,
        pSH5_z, pSH8_x, pSH8_y, pSH4_x, pSH4_y;

    fTmp0B_z = -1.092548430592079f;
    fC1_x = 2.f * x;
    fC1_y = -2.f * y;
    fS1_x = 2.f * y;
    fS1_y = 2.f * x;
    pSH6_z = 2.f * 0.9461746957575601f * z;
    pSH7_x = fTmp0B;
    pSH7_z = fTmp0B_z * x;
    pSH5_y = fTmp0B;
    pSH5_z = fTmp0B_z * y;
    pSH8_x = 0.5462742152960395f * fC1_x;
    pSH8_y = 0.5462742152960395f * fC1_y;
    pSH4_x = 0.5462742152960395f * fS1_x;
    pSH4_y = 0.5462742152960395f * fS1_y;

    v_x += dot(v_colors,
            pSH4_x * C.load(3) + pSH8_x * C.load(7) +
            pSH7_x * C.load(6));
    v_y += dot(v_colors,
            pSH4_y * C.load(3) + pSH8_y * C.load(7) +
            pSH5_y * C.load(4));
    v_z += dot(v_colors,
            pSH6_z * C.load(5) + pSH7_z * C.load(6) +
            pSH5_z * C.load(4));

    if (degree <= 2) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = -0.5900435899266435f * fC2;
    float pSH9 = -0.5900435899266435f * fS2;
    sv(8, pSH9 * v_colors);
    sv(9, pSH10 * v_colors);
    sv(10, pSH11 * v_colors);
    sv(11, pSH12 * v_colors);
    sv(12, pSH13 * v_colors);
    sv(13, pSH14 * v_colors);
    sv(14, pSH15 * v_colors);

    float fTmp0C_z, fTmp1B_z, fC2_x, fC2_y, fS2_x, fS2_y, pSH12_z, pSH13_x,
        pSH13_z, pSH11_y, pSH11_z, pSH14_x, pSH14_y, pSH14_z, pSH10_x, pSH10_y,
        pSH10_z, pSH15_x, pSH15_y, pSH9_x, pSH9_y;

    fTmp0C_z = -2.285228997322329f * 2.f * z;
    fTmp1B_z = 1.445305721320277f;
    fC2_x = fC1 + x * fC1_x - y * fS1_x;
    fC2_y = x * fC1_y - fS1 - y * fS1_y;
    fS2_x = fS1 + x * fS1_x + y * fC1_x;
    fS2_y = x * fS1_y + fC1 + y * fC1_y;
    pSH12_z = 3.f * 1.865881662950577f * z2 - 1.119528997770346f;
    pSH13_x = fTmp0C;
    pSH13_z = fTmp0C_z * x;
    pSH11_y = fTmp0C;
    pSH11_z = fTmp0C_z * y;
    pSH14_x = fTmp1B * fC1_x;
    pSH14_y = fTmp1B * fC1_y;
    pSH14_z = fTmp1B_z * fC1;
    pSH10_x = fTmp1B * fS1_x;
    pSH10_y = fTmp1B * fS1_y;
    pSH10_z = fTmp1B_z * fS1;
    pSH15_x = -0.5900435899266435f * fC2_x;
    pSH15_y = -0.5900435899266435f * fC2_y;
    pSH9_x = -0.5900435899266435f * fS2_x;
    pSH9_y = -0.5900435899266435f * fS2_y;

    v_x += dot(v_colors,
            pSH9_x * C.load(8) + pSH15_x * C.load(14) +
            pSH10_x * C.load(9) + pSH14_x * C.load(13) +
            pSH13_x * C.load(12));

    v_y += dot(v_colors,
            pSH9_y * C.load(8) + pSH15_y * C.load(14) +
            pSH10_y * C.load(9) + pSH14_y * C.load(13) +
            pSH11_y * C.load(10));

    v_z += dot(v_colors,
            pSH12_z * C.load(11) + pSH13_z * C.load(12) +
            pSH11_z * C.load(10) + pSH14_z * C.load(13) +
            pSH10_z * C.load(9));

    if (degree <= 3) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 + -1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;
    sv(15, pSH16 * v_colors);
    sv(16, pSH17 * v_colors);
    sv(17, pSH18 * v_colors);
    sv(18, pSH19 * v_colors);
    sv(19, pSH20 * v_colors);
    sv(20, pSH21 * v_colors);
    sv(21, pSH22 * v_colors);
    sv(22, pSH23 * v_colors);
    sv(23, pSH24 * v_colors);

    float fTmp0D_z, fTmp1C_z, fTmp2B_z, fC3_x, fC3_y, fS3_x, fS3_y, pSH20_z,
        pSH21_x, pSH21_z, pSH19_y, pSH19_z, pSH22_x, pSH22_y, pSH22_z, pSH18_x,
        pSH18_y, pSH18_z, pSH23_x, pSH23_y, pSH23_z, pSH17_x, pSH17_y, pSH17_z,
        pSH24_x, pSH24_y, pSH16_x, pSH16_y;

    fTmp0D_z = 3.f * -4.683325804901025f * z2 + 2.007139630671868f;
    fTmp1C_z = 2.f * 3.31161143515146f * z;
    fTmp2B_z = -1.770130769779931f;
    fC3_x = fC2 + x * fC2_x - y * fS2_x;
    fC3_y = x * fC2_y - fS2 - y * fS2_y;
    fS3_x = fS2 + y * fC2_x + x * fS2_x;
    fS3_y = x * fS2_y + fC2 + y * fC2_y;
    pSH20_z = 1.984313483298443f * (pSH12 + z * pSH12_z) +
                -1.006230589874905f * pSH6_z;
    pSH21_x = fTmp0D;
    pSH21_z = fTmp0D_z * x;
    pSH19_y = fTmp0D;
    pSH19_z = fTmp0D_z * y;
    pSH22_x = fTmp1C * fC1_x;
    pSH22_y = fTmp1C * fC1_y;
    pSH22_z = fTmp1C_z * fC1;
    pSH18_x = fTmp1C * fS1_x;
    pSH18_y = fTmp1C * fS1_y;
    pSH18_z = fTmp1C_z * fS1;
    pSH23_x = fTmp2B * fC2_x;
    pSH23_y = fTmp2B * fC2_y;
    pSH23_z = fTmp2B_z * fC2;
    pSH17_x = fTmp2B * fS2_x;
    pSH17_y = fTmp2B * fS2_y;
    pSH17_z = fTmp2B_z * fS2;
    pSH24_x = 0.6258357354491763f * fC3_x;
    pSH24_y = 0.6258357354491763f * fC3_y;
    pSH16_x = 0.6258357354491763f * fS3_x;
    pSH16_y = 0.6258357354491763f * fS3_y;

    v_x += dot(v_colors,
            pSH16_x * C.load(15) + pSH24_x * C.load(23) +
            pSH17_x * C.load(16) + pSH23_x * C.load(22) +
            pSH18_x * C.load(17) + pSH22_x * C.load(21) +
            pSH21_x * C.load(20));
    v_y += dot(v_colors,
            pSH16_y * C.load(15) + pSH24_y * C.load(23) +
            pSH17_y * C.load(16) + pSH23_y * C.load(22) +
            pSH18_y * C.load(17) + pSH22_y * C.load(21) +
            pSH19_y * C.load(18));
    v_z += dot(v_colors,
            pSH20_z * C.load(19) + pSH21_z * C.load(20) +
            pSH19_z * C.load(18) + pSH22_z * C.load(21) +
            pSH18_z * C.load(17) + pSH23_z * C.load(22) +
            pSH17_z * C.load(16));

    float3 dir_n = float3(x, y, z);
    float3 v_dir_n = float3(v_x, v_y, v_z);
    v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;

    #undef sv
}
