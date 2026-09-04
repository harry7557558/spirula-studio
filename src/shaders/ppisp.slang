// Based on https://github.com/nv-tlabs/ppisp/blob/main/ppisp/src

/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

struct VignettingChannelParams : IDifferentiable {
    float cx;     // Optical center X
    float cy;     // Optical center Y
    float alpha0; // r^2 coefficient
    float alpha1; // r^4 coefficient
    float alpha2; // r^6 coefficient
};

struct ColorPPISPParams : IDifferentiable {
    float2 b; // Blue latent offsets
    float2 r; // Red latent offsets
    float2 g; // Green latent offsets
    float2 n; // Neutral latent offsets
};

struct CRFPPISPChannelParams : IDifferentiable {
    float toe;      // Toe strength (before softplus)
    float shoulder; // Shoulder strength (before softplus)
    float gamma;    // Gamma (before softplus)
    float center;   // Center point (before sigmoid)
};

struct RQSCRFPPISPChannelParams : IDifferentiable {
    float g0;  // gradient at x=0 (before exp)
    float g1;  // gradient at x=1 (before exp)
    float x0;  // x0 (before sigmoid)
    float y0;  // y0 (before sigmoid)
    float gc;  // gradient at x=x0 (before exp)
};

// ----------------------------------------------------------------------------
// Exposure Compensation
// ----------------------------------------------------------------------------
[ForceInline]
[Differentiable]
float3 apply_exposure(float3 rgb_in, float exposure_param) {
    float exposure = exp2(exposure_param);
    return rgb_in * exposure;
}


// ----------------------------------------------------------------------------
// Vignetting Correction
// ----------------------------------------------------------------------------

[ForceInline]
[Differentiable]
float3 apply_vignetting(
    float3 rgb_in, VignettingChannelParams vignette_params[3],
    float2 pix_coord, float2 image_center, float2 img_size
) {
    // Normalize coordinates to [-0.5, 0.5] range based on max dimension
    float max_res = max(img_size.x, img_size.y);
    float2 uv = float2((pix_coord.x - image_center.x) / max_res,
                            (pix_coord.y - image_center.y) / max_res);
    float3 rgb_out = rgb_in;
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        const VignettingChannelParams params = vignette_params[i];
        float dx = uv.x - params.cx;
        float dy = uv.y - params.cy;
        float r2 = dx * dx + dy * dy;
        float r4 = r2 * r2;
        float r6 = r4 * r2;
        float falloff = params.alpha2 * r6 + params.alpha1 * r4 + params.alpha0 * r2 + 1.0f;
        falloff = clamp(falloff, 0.0f, 1.0f);
        rgb_out[i] *= falloff;
    }
    return rgb_out;
}


// ----------------------------------------------------------------------------
// Color Correction - PPISP (Homography)
// ----------------------------------------------------------------------------

[ForceInline]
[Differentiable]
float3x3 compute_homography(ColorPPISPParams params) {
    // Load latent offsets for control chromaticities (B, R, G, N)
    const float2 b_lat = params.b;
    const float2 r_lat = params.r;
    const float2 g_lat = params.g;
    const float2 n_lat = params.n;

    // Map latent to real offsets via ZCA 2x2 blocks (stored as constant 4-element arrays)
    const static float2x2 zca_b = float2x2(0.0480542f, -0.0043631f, -0.0043631f, 0.0481283f);
    const static float2x2 zca_r = float2x2(0.0580570f, -0.0179872f, -0.0179872f, 0.0431061f);
    const static float2x2 zca_g = float2x2(0.0433336f, -0.0180537f, -0.0180537f, 0.0580500f);
    const static float2x2 zca_n = float2x2(0.0128369f, -0.0034654f, -0.0034654f, 0.0128158f);

    float2 bd = mul(zca_b, b_lat);
    float2 rd = mul(zca_r, r_lat);
    float2 gd = mul(zca_g, g_lat);
    float2 nd = mul(zca_n, n_lat);

    // Fixed sources (r, g, intensity) + offsets = targets
    float3 t_b = float3(0.0f + bd.x, 0.0f + bd.y, 1.0f);
    float3 t_r = float3(1.0f + rd.x, 0.0f + rd.y, 1.0f);
    float3 t_g = float3(0.0f + gd.x, 1.0f + gd.y, 1.0f);
    float3 t_gray = float3(1.0f / 3.0f + nd.x, 1.0f / 3.0f + nd.y, 1.0f);

    // T has columns [t_b, t_r, t_g] (column-major stored as row-major)
    float3x3 T = float3x3(t_b.x, t_r.x, t_g.x, t_b.y, t_r.y, t_g.y, t_b.z, t_r.z, t_g.z);

    // Skew-symmetric matrix [t_gray]_x
    float3x3 skew = float3x3(0.0f, -t_gray.z, t_gray.y, t_gray.z, 0.0f, -t_gray.x, -t_gray.y,
                                  t_gray.x, 0.0f);

    // M = skew * T
    float3x3 M = mul(skew, T);

    // Nullspace vector lambda via cross product of rows
    float3 r0 = float3(M[0][0], M[0][1], M[0][2]);
    float3 r1 = float3(M[1][0], M[1][1], M[1][2]);
    float3 r2 = float3(M[2][0], M[2][1], M[2][2]);

    float3 lambda_v = cross(r0, r1);
    float n2 = dot(lambda_v, lambda_v);

    if (n2 < 1.0e-20f) {
        lambda_v = cross(r0, r2);
        n2 = dot(lambda_v, lambda_v);
        if (n2 < 1.0e-20f) {
            lambda_v = cross(r1, r2);
        }
    }

    // S_inv = [[-1,-1,1],[1,0,0],[0,1,0]]
    float3x3 S_inv = float3x3(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // D = diag(lambda)
    float3x3 D =
        float3x3(lambda_v.x, 0.0f, 0.0f, 0.0f, lambda_v.y, 0.0f, 0.0f, 0.0f, lambda_v.z);

    // H = T * D * S_inv
    float3x3 H = mul(mul(T, D), S_inv);

    // Normalize so H[2][2] = 1
    float s = H[2][2];
    if (abs(s) > 1.0e-20f) {
        float inv_s = 1.0f / s;
        H = H * (inv_s);
    }

    return H;
}

// Floor on the RGI renormalization denominator, as a fraction of |intensity|.
// The identity homography gives rgi_out.z == intensity, so this binds only on
// the pole line of an aggressive H, and caps the chroma gain at its reciprocal.
static const float kColorNormMinZ = 0.0001f;

[ForceInline]
[Differentiable]
float3 apply_color_correction_ppisp(float3 rgb_in, ColorPPISPParams color_params) {
    // Compute homography matrix from control-point parametrization
    float3x3 H = compute_homography(color_params);

    // Convert to RGI space
    float intensity = rgb_in.x + rgb_in.y + rgb_in.z;
    float3 rgi_in = float3(rgb_in.x, rgb_in.y, intensity);

    // Apply homography
    float3 rgi_out = mul(H, rgi_in);

    // Renormalize to the input intensity, off a strictly positive z.
    float z = max(rgi_out.z, kColorNormMinZ * abs(intensity) + 1.0e-8f);
    float norm_factor = intensity / z;
    float out_r = rgi_out.x * norm_factor;
    float out_g = rgi_out.y * norm_factor;

    // Third component is `intensity` by construction, so it is written back
    // rather than recomputed: intensity is preserved exactly, at every z.
    return float3(out_r, out_g, intensity - out_r - out_g);
}


// ----------------------------------------------------------------------------
// CRF - PPISP (Parametric toe-shoulder curve)
// ----------------------------------------------------------------------------

[ForceInline]
[Differentiable]
float bounded_positive_forward(float raw, float min_value = 0.1f) {
    return min_value + log(1.0f + exp(raw));
}

[ForceInline]
[Differentiable]
float clamped_forward(float raw) {
    return 1.0f / (1.0f + exp(-raw));
}

[ForceInline]
[Differentiable]
float3 apply_crf_ppisp(float3 rgb_in, CRFPPISPChannelParams crf_params[3]) {
    float3 rgb_clamped = clamp(rgb_in, 0.0f, 1.0f);
    float3 rgb_out;

    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        float x = (i == 0) ? rgb_clamped.x : (i == 1) ? rgb_clamped.y : rgb_clamped.z;
        const CRFPPISPChannelParams params = crf_params[i];

        // Transform parameters
        float toe = bounded_positive_forward(params.toe, 0.3f);
        float shoulder = bounded_positive_forward(params.shoulder, 0.3f);
        float gamma = bounded_positive_forward(params.gamma, 0.1f);
        float center = clamped_forward(params.center);

        // Compute a, b coefficients
        float lerp_val = lerp(toe, shoulder, center);
        float a = (shoulder * center) / lerp_val;
        float b = 1.0f - a;

        float y;
        // Piecewise toe-shoulder curve
        if (x <= center) {
            y = a * pow(x / center, toe);
        } else {
            y = 1.0f - b * pow((1.0f - x) / (1.0f - center), shoulder);
        }

        // Apply gamma
        rgb_out[i] = pow(max(0.0f, y), gamma);
    }

    return rgb_out;
}

[ForceInline]
[Differentiable]
float3 apply_rqs_crf_ppisp(float3 rgb_in, RQSCRFPPISPChannelParams crf_params[3]) {
    float3 rgb_clamped = clamp(rgb_in, 0.0f, 1.0f);
    float3 rgb_out;

    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        float x = (i == 0) ? rgb_clamped.x : (i == 1) ? rgb_clamped.y : rgb_clamped.z;
        const RQSCRFPPISPChannelParams params = crf_params[i];

        // Transform parameters
        float g0 = exp(params.g0);
        float g1 = exp(params.g1);
        float x0 = 1.0f / (1.0f + exp(-params.x0));
        float y0 = 1.0f / (1.0f + exp(-params.y0));
        float gc = exp(params.gc);

        // piecewise RQS
        float y;
        if (x < x0) {
            float s0 = y0 / x0;
            float t0 = x / x0;
            y = y0 * (s0 * t0 * t0 + g0 * t0 * (1.0f - t0)) /
                (s0 + (g0 + gc - 2.0f * s0) * t0 * (1.0f - t0));
        } else {
            float s1 = (1.0f - y0) / (1.0f - x0);
            float t1 = (x - x0) / (1.0f - x0);
            y = y0 + (1.0f - y0) * (s1 * t1 * t1 + gc * t1 * (1.0f - t1)) /
                (s1 + (gc + g1 - 2.0f * s1) * t1 * (1.0f - t1));
        }

        rgb_out[i] = y;
    }

    return rgb_out;
}

// Put together

struct PPISPParams : IDifferentiable {
    float exposure;
    VignettingChannelParams vignette_params[3];  // 5 x 3 params
    ColorPPISPParams color_params;  // 8 params
    CRFPPISPChannelParams crf_params[3];  // 4 x 3 params
};

static const int PPISP_NUM_PARAMS = 1 + 5 * 3 + 8 + 4 * 3;

struct PPISPParamsRQS : IDifferentiable {
    float exposure;
    VignettingChannelParams vignette_params[3]; // 5 x 3 params
    ColorPPISPParams color_params;  // 8 params
    RQSCRFPPISPChannelParams crf_params[3];  // 5 x 3 params
};

static const int PPISP_RQS_NUM_PARAMS = 1 + 5 * 3 + 8 + 5 * 3;

// "no_crf" variant: exposure + vignetting + color only, no CRF parameters.
struct PPISPParamsNoCRF : IDifferentiable {
    float exposure;
    VignettingChannelParams vignette_params[3];  // 5 x 3 params
    ColorPPISPParams color_params;  // 8 params
};

static const int PPISP_NO_CRF_NUM_PARAMS = 1 + 5 * 3 + 8;

// "no_crf_no_vig" variant: exposure + color only.
struct PPISPParamsNoCRFNoVig : IDifferentiable {
    float exposure;
    ColorPPISPParams color_params;  // 8 params
};

static const int PPISP_NO_CRF_NO_VIG_NUM_PARAMS = 1 + 8;

[ForceInline]
[Differentiable]
PPISPParams get_ppisp_params(float[PPISP_NUM_PARAMS] params) {
    PPISPParams p;
    int idx = 0;
    p.exposure = params[idx++];
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.vignette_params[i].cx = params[idx++];
        p.vignette_params[i].cy = params[idx++];
        p.vignette_params[i].alpha0 = params[idx++];
        p.vignette_params[i].alpha1 = params[idx++];
        p.vignette_params[i].alpha2 = params[idx++];
    }
    p.color_params.b.x = params[idx++];
    p.color_params.b.y = params[idx++];
    p.color_params.r.x = params[idx++];
    p.color_params.r.y = params[idx++];
    p.color_params.g.x = params[idx++];
    p.color_params.g.y = params[idx++];
    p.color_params.n.x = params[idx++];
    p.color_params.n.y = params[idx++];
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.crf_params[i].toe = params[idx++];
        p.crf_params[i].shoulder = params[idx++];
        p.crf_params[i].gamma = params[idx++];
        p.crf_params[i].center = params[idx++];
    }
    return p;
}

[ForceInline]
[Differentiable]
PPISPParamsRQS get_ppisp_rqs_params(float[PPISP_RQS_NUM_PARAMS] params) {
    PPISPParamsRQS p;
    int idx = 0;
    p.exposure = params[idx++];
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.vignette_params[i].cx = params[idx++];
        p.vignette_params[i].cy = params[idx++];
        p.vignette_params[i].alpha0 = params[idx++];
        p.vignette_params[i].alpha1 = params[idx++];
        p.vignette_params[i].alpha2 = params[idx++];
    }
    p.color_params.b.x = params[idx++];
    p.color_params.b.y = params[idx++];
    p.color_params.r.x = params[idx++];
    p.color_params.r.y = params[idx++];
    p.color_params.g.x = params[idx++];
    p.color_params.g.y = params[idx++];
    p.color_params.n.x = params[idx++];
    p.color_params.n.y = params[idx++];
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.crf_params[i].g0 = params[idx++];
        p.crf_params[i].g1 = params[idx++];
        p.crf_params[i].x0 = params[idx++];
        p.crf_params[i].y0 = params[idx++];
        p.crf_params[i].gc = params[idx++];
    }
    return p;
}

[ForceInline]
[Differentiable]
PPISPParamsNoCRF get_ppisp_no_crf_params(float[PPISP_NO_CRF_NUM_PARAMS] params) {
    PPISPParamsNoCRF p;
    int idx = 0;
    p.exposure = params[idx++];
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        p.vignette_params[i].cx = params[idx++];
        p.vignette_params[i].cy = params[idx++];
        p.vignette_params[i].alpha0 = params[idx++];
        p.vignette_params[i].alpha1 = params[idx++];
        p.vignette_params[i].alpha2 = params[idx++];
    }
    p.color_params.b.x = params[idx++];
    p.color_params.b.y = params[idx++];
    p.color_params.r.x = params[idx++];
    p.color_params.r.y = params[idx++];
    p.color_params.g.x = params[idx++];
    p.color_params.g.y = params[idx++];
    p.color_params.n.x = params[idx++];
    p.color_params.n.y = params[idx++];
    return p;
}

[ForceInline]
[Differentiable]
PPISPParamsNoCRFNoVig get_ppisp_no_crf_no_vig_params(float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] params) {
    PPISPParamsNoCRFNoVig p;
    int idx = 0;
    p.exposure = params[idx++];
    p.color_params.b.x = params[idx++];
    p.color_params.b.y = params[idx++];
    p.color_params.r.x = params[idx++];
    p.color_params.r.y = params[idx++];
    p.color_params.g.x = params[idx++];
    p.color_params.g.y = params[idx++];
    p.color_params.n.x = params[idx++];
    p.color_params.n.y = params[idx++];
    return p;
}

[ForceInline]
[Differentiable]
float[PPISP_NUM_PARAMS] from_ppisp_params(PPISPParams p) {
    float params[PPISP_NUM_PARAMS];
    int idx = 0;
    params[idx++] = p.exposure;
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        params[idx++] = p.vignette_params[i].cx;
        params[idx++] = p.vignette_params[i].cy;
        params[idx++] = p.vignette_params[i].alpha0;
        params[idx++] = p.vignette_params[i].alpha1;
        params[idx++] = p.vignette_params[i].alpha2;
    }
    params[idx++] = p.color_params.b.x;
    params[idx++] = p.color_params.b.y;
    params[idx++] = p.color_params.r.x;
    params[idx++] = p.color_params.r.y;
    params[idx++] = p.color_params.g.x;
    params[idx++] = p.color_params.g.y;
    params[idx++] = p.color_params.n.x;
    params[idx++] = p.color_params.n.y;
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        params[idx++] = p.crf_params[i].toe;
        params[idx++] = p.crf_params[i].shoulder;
        params[idx++] = p.crf_params[i].gamma;
        params[idx++] = p.crf_params[i].center;
    }
    return params;
}

[ForceInline]
[Differentiable]
float[PPISP_RQS_NUM_PARAMS] from_ppisp_rqs_params(PPISPParamsRQS p) {
    float params[PPISP_RQS_NUM_PARAMS];
    int idx = 0;
    params[idx++] = p.exposure;
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        params[idx++] = p.vignette_params[i].cx;
        params[idx++] = p.vignette_params[i].cy;
        params[idx++] = p.vignette_params[i].alpha0;
        params[idx++] = p.vignette_params[i].alpha1;
        params[idx++] = p.vignette_params[i].alpha2;
    }
    params[idx++] = p.color_params.b.x;
    params[idx++] = p.color_params.b.y;
    params[idx++] = p.color_params.r.x;
    params[idx++] = p.color_params.r.y;
    params[idx++] = p.color_params.g.x;
    params[idx++] = p.color_params.g.y;
    params[idx++] = p.color_params.n.x;
    params[idx++] = p.color_params.n.y;
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        params[idx++] = p.crf_params[i].g0;
        params[idx++] = p.crf_params[i].g1;
        params[idx++] = p.crf_params[i].x0;
        params[idx++] = p.crf_params[i].y0;
        params[idx++] = p.crf_params[i].gc;
    }
    return params;
}

[CudaDeviceExport]
[Differentiable]
float3 apply_ppisp(float3 rgb_in, no_diff float2 pix_coord, no_diff float2 image_center, no_diff float2 img_size, float[PPISP_NUM_PARAMS] params) {
    PPISPParams p = get_ppisp_params(params);
    float3 rgb = rgb_in;
    rgb = apply_exposure(rgb, p.exposure);
    rgb = apply_vignetting(rgb, p.vignette_params, pix_coord, image_center, img_size);
    rgb = apply_color_correction_ppisp(rgb, p.color_params);
    rgb = apply_crf_ppisp(rgb, p.crf_params);
    return rgb;
}

[CudaDeviceExport]
[Differentiable]
float3 apply_ppisp_rqs(float3 rgb_in, no_diff float2 pix_coord, no_diff float2 image_center, no_diff float2 img_size, float[PPISP_RQS_NUM_PARAMS] params) {
    PPISPParamsRQS p = get_ppisp_rqs_params(params);
    float3 rgb = rgb_in;
    rgb = apply_exposure(rgb, p.exposure);
    rgb = apply_vignetting(rgb, p.vignette_params, pix_coord, image_center, img_size);
    rgb = apply_color_correction_ppisp(rgb, p.color_params);
    rgb = apply_rqs_crf_ppisp(rgb, p.crf_params);
    return rgb;
}

// `clamp_output` is the mode axis the CRF-less variants carry instead of a
// tone curve: clamp()'s own derivative is what zeroes the gradient outside
// [0, 1], so no straight-through override is wanted here.
[CudaDeviceExport]
[Differentiable]
float3 apply_ppisp_no_crf(float3 rgb_in, no_diff float2 pix_coord, no_diff float2 image_center, no_diff float2 img_size, float[PPISP_NO_CRF_NUM_PARAMS] params, no_diff bool clamp_output) {
    PPISPParamsNoCRF p = get_ppisp_no_crf_params(params);
    float3 rgb = rgb_in;
    rgb = apply_exposure(rgb, p.exposure);
    rgb = apply_vignetting(rgb, p.vignette_params, pix_coord, image_center, img_size);
    rgb = apply_color_correction_ppisp(rgb, p.color_params);
    if (clamp_output)
        rgb = clamp(rgb, 0.0f, 1.0f);
    return rgb;
}

[CudaDeviceExport]
[Differentiable]
float3 apply_ppisp_no_crf_no_vig(float3 rgb_in, no_diff float2 pix_coord, no_diff float2 image_center, no_diff float2 img_size, float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] params, no_diff bool clamp_output) {
    PPISPParamsNoCRFNoVig p = get_ppisp_no_crf_no_vig_params(params);
    float3 rgb = rgb_in;
    rgb = apply_exposure(rgb, p.exposure);
    rgb = apply_color_correction_ppisp(rgb, p.color_params);
    if (clamp_output)
        rgb = clamp(rgb, 0.0f, 1.0f);
    return rgb;
}

[CudaDeviceExport]
void apply_ppisp_vjp(
    float3 rgb_in, float2 pix_coord, float2 image_center, float2 img_size,
    float[PPISP_NUM_PARAMS] params,
    float3 grad_out,
    out float3 grad_rgb_in,
    out float[PPISP_NUM_PARAMS] grad_params
) {
    DifferentialPair<float3> dp_rgb_in = diffPair(rgb_in);
    DifferentialPair<float[PPISP_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(apply_ppisp)(
        dp_rgb_in, pix_coord, image_center, img_size, dp_params,
        grad_out
    );
    grad_rgb_in = dp_rgb_in.d;
    grad_params = dp_params.d;
}

[CudaDeviceExport]
void apply_ppisp_rqs_vjp(
    float3 rgb_in, float2 pix_coord, float2 image_center, float2 img_size,
    float[PPISP_RQS_NUM_PARAMS] params,
    float3 grad_out,
    out float3 grad_rgb_in,
    out float[PPISP_RQS_NUM_PARAMS] grad_params
) {
    DifferentialPair<float3> dp_rgb_in = diffPair(rgb_in);
    DifferentialPair<float[PPISP_RQS_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(apply_ppisp_rqs)(
        dp_rgb_in, pix_coord, image_center, img_size, dp_params,
        grad_out
    );
    grad_rgb_in = dp_rgb_in.d;
    grad_params = dp_params.d;
}

[CudaDeviceExport]
void apply_ppisp_no_crf_vjp(
    float3 rgb_in, float2 pix_coord, float2 image_center, float2 img_size,
    float[PPISP_NO_CRF_NUM_PARAMS] params, bool clamp_output,
    float3 grad_out,
    out float3 grad_rgb_in,
    out float[PPISP_NO_CRF_NUM_PARAMS] grad_params
) {
    DifferentialPair<float3> dp_rgb_in = diffPair(rgb_in);
    DifferentialPair<float[PPISP_NO_CRF_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(apply_ppisp_no_crf)(
        dp_rgb_in, pix_coord, image_center, img_size, dp_params, clamp_output,
        grad_out
    );
    grad_rgb_in = dp_rgb_in.d;
    grad_params = dp_params.d;
}

[CudaDeviceExport]
void apply_ppisp_no_crf_no_vig_vjp(
    float3 rgb_in, float2 pix_coord, float2 image_center, float2 img_size,
    float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] params, bool clamp_output,
    float3 grad_out,
    out float3 grad_rgb_in,
    out float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] grad_params
) {
    DifferentialPair<float3> dp_rgb_in = diffPair(rgb_in);
    DifferentialPair<float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(apply_ppisp_no_crf_no_vig)(
        dp_rgb_in, pix_coord, image_center, img_size, dp_params, clamp_output,
        grad_out
    );
    grad_rgb_in = dp_rgb_in.d;
    grad_params = dp_params.d;
}

// Regularization

enum RawPPISPRegLossIndex {
    SumExposure,
    SumVignettingCrSquared,
    SumVignettingAlpha0Relu,
    SumVignettingAlpha1Relu,
    SumVignettingAlpha2Relu,
    SumVignettingCxChannelVariance,
    SumVignettingCyChannelVariance,
    SumVignettingAlpha0ChannelVariance,
    SumVignettingAlpha1ChannelVariance,
    SumVignettingAlpha2ChannelVariance,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    SumCRFToeChannelVariance,
    SumCRFShoulderChannelVariance,
    SumCRFGammaChannelVariance,
    SumCRFCenterChannelVariance,
    length
};

// "no_crf" raw losses: same as Original but without the CRF channel-variance entries.
enum RawPPISPRegLossIndexNoCRF {
    SumExposure,
    SumVignettingCrSquared,
    SumVignettingAlpha0Relu,
    SumVignettingAlpha1Relu,
    SumVignettingAlpha2Relu,
    SumVignettingCxChannelVariance,
    SumVignettingCyChannelVariance,
    SumVignettingAlpha0ChannelVariance,
    SumVignettingAlpha1ChannelVariance,
    SumVignettingAlpha2ChannelVariance,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    length
};

// "no_crf_no_vig" raw losses: exposure + the eight colour offsets.
enum RawPPISPRegLossIndexNoCRFNoVig {
    SumExposure,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    length
};

enum RawPPISPRegLossIndexRQS {
    SumExposure,
    SumVignettingCrSquared,
    SumVignettingAlpha0Relu,
    SumVignettingAlpha1Relu,
    SumVignettingAlpha2Relu,
    SumVignettingCxChannelVariance,
    SumVignettingCyChannelVariance,
    SumVignettingAlpha0ChannelVariance,
    SumVignettingAlpha1ChannelVariance,
    SumVignettingAlpha2ChannelVariance,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    SumCRFG0ChannelVariance,
    SumCRFG1ChannelVariance,
    SumCRFX0ChannelVariance,
    SumCRFY0ChannelVariance,
    SumCRFGcChannelVariance,
    length
};

[ForceInline]
[Differentiable]
float varp_of_3(float a, float b, float c) {
    float mean = (a + b + c) / 3.0f;
    return ((a - mean) * (a - mean) +
            (b - mean) * (b - mean) +
            (c - mean) * (c - mean)) / 3.0f;
}

[CudaDeviceExport]
[Differentiable]
float[RawPPISPRegLossIndex::length] compute_raw_ppisp_regularization_loss(
    float[PPISP_NUM_PARAMS] params
) {
    PPISPParams p = get_ppisp_params(params);
    float losses[RawPPISPRegLossIndex::length] = {0.0f};

    // Exposure
    losses[RawPPISPRegLossIndex::SumExposure] = p.exposure;

    // Vignetting
    losses[RawPPISPRegLossIndex::SumVignettingCrSquared] =
        p.vignette_params[0].cx * p.vignette_params[0].cx + p.vignette_params[0].cy * p.vignette_params[0].cy +
        p.vignette_params[1].cx * p.vignette_params[1].cx + p.vignette_params[1].cy * p.vignette_params[1].cy +
        p.vignette_params[2].cx * p.vignette_params[2].cx + p.vignette_params[2].cy * p.vignette_params[2].cy;
    losses[RawPPISPRegLossIndex::SumVignettingAlpha0Relu] =
        max(0.0f, p.vignette_params[0].alpha0) +
        max(0.0f, p.vignette_params[1].alpha0) +
        max(0.0f, p.vignette_params[2].alpha0);
    losses[RawPPISPRegLossIndex::SumVignettingAlpha1Relu] =
        max(0.0f, p.vignette_params[0].alpha1) +
        max(0.0f, p.vignette_params[1].alpha1) +
        max(0.0f, p.vignette_params[2].alpha1);
    losses[RawPPISPRegLossIndex::SumVignettingAlpha2Relu] =
        max(0.0f, p.vignette_params[0].alpha2) +
        max(0.0f, p.vignette_params[1].alpha2) +
        max(0.0f, p.vignette_params[2].alpha2);
    losses[RawPPISPRegLossIndex::SumVignettingCxChannelVariance] =
        varp_of_3(p.vignette_params[0].cx, p.vignette_params[1].cx, p.vignette_params[2].cx);
    losses[RawPPISPRegLossIndex::SumVignettingCyChannelVariance] =
        varp_of_3(p.vignette_params[0].cy, p.vignette_params[1].cy, p.vignette_params[2].cy);
    losses[RawPPISPRegLossIndex::SumVignettingAlpha0ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha0, p.vignette_params[1].alpha0, p.vignette_params[2].alpha0);
    losses[RawPPISPRegLossIndex::SumVignettingAlpha1ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha1, p.vignette_params[1].alpha1, p.vignette_params[2].alpha1);
    losses[RawPPISPRegLossIndex::SumVignettingAlpha2ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha2, p.vignette_params[1].alpha2, p.vignette_params[2].alpha2);

    // Color
    const static float2x2 zca_b = float2x2(0.0480542f, -0.0043631f, -0.0043631f, 0.0481283f);
    const static float2x2 zca_r = float2x2(0.0580570f, -0.0179872f, -0.0179872f, 0.0431061f);
    const static float2x2 zca_g = float2x2(0.0433336f, -0.0180537f, -0.0180537f, 0.0580500f);
    const static float2x2 zca_n = float2x2(0.0128369f, -0.0034654f, -0.0034654f, 0.0128158f);
    float2 bd = mul(zca_b, p.color_params.b);
    float2 rd = mul(zca_r, p.color_params.r);
    float2 gd = mul(zca_g, p.color_params.g);
    float2 nd = mul(zca_n, p.color_params.n);
    losses[RawPPISPRegLossIndex::SumColorBx] = bd.x;
    losses[RawPPISPRegLossIndex::SumColorBy] = bd.y;
    losses[RawPPISPRegLossIndex::SumColorRx] = rd.x;
    losses[RawPPISPRegLossIndex::SumColorRy] = rd.y;
    losses[RawPPISPRegLossIndex::SumColorGx] = gd.x;
    losses[RawPPISPRegLossIndex::SumColorGy] = gd.y;
    losses[RawPPISPRegLossIndex::SumColorNx] = nd.x;
    losses[RawPPISPRegLossIndex::SumColorNy] = nd.y;

    // CRF
    losses[RawPPISPRegLossIndex::SumCRFToeChannelVariance] =
        varp_of_3(p.crf_params[0].toe, p.crf_params[1].toe, p.crf_params[2].toe);
    losses[RawPPISPRegLossIndex::SumCRFShoulderChannelVariance] =
        varp_of_3(p.crf_params[0].shoulder, p.crf_params[1].shoulder, p.crf_params[2].shoulder);
    losses[RawPPISPRegLossIndex::SumCRFGammaChannelVariance] =
        varp_of_3(p.crf_params[0].gamma, p.crf_params[1].gamma, p.crf_params[2].gamma);
    losses[RawPPISPRegLossIndex::SumCRFCenterChannelVariance] =
        varp_of_3(p.crf_params[0].center, p.crf_params[1].center, p.crf_params[2].center);

    return losses;
}

[CudaDeviceExport]
[Differentiable]
float[RawPPISPRegLossIndexRQS::length] compute_raw_ppisp_rqs_regularization_loss(
    float[PPISP_RQS_NUM_PARAMS] params
) {
    PPISPParamsRQS p = get_ppisp_rqs_params(params);
    float losses[RawPPISPRegLossIndexRQS::length] = { 0.0f };
    // Exposure
    losses[RawPPISPRegLossIndexRQS::SumExposure] = p.exposure;

    // Vignetting
    losses[RawPPISPRegLossIndexRQS::SumVignettingCrSquared] =
        p.vignette_params[0].cx * p.vignette_params[0].cx + p.vignette_params[0].cy * p.vignette_params[0].cy +
        p.vignette_params[1].cx * p.vignette_params[1].cx + p.vignette_params[1].cy * p.vignette_params[1].cy +
        p.vignette_params[2].cx * p.vignette_params[2].cx + p.vignette_params[2].cy * p.vignette_params[2].cy;
    losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha0Relu] =
        max(0.0f, p.vignette_params[0].alpha0) +
        max(0.0f, p.vignette_params[1].alpha0) +
        max(0.0f, p.vignette_params[2].alpha0);
    losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha1Relu] =
        max(0.0f, p.vignette_params[0].alpha1) +
        max(0.0f, p.vignette_params[1].alpha1) +
        max(0.0f, p.vignette_params[2].alpha1);
    losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha2Relu] =
        max(0.0f, p.vignette_params[0].alpha2) +
        max(0.0f, p.vignette_params[1].alpha2) +
        max(0.0f, p.vignette_params[2].alpha2);
    losses[RawPPISPRegLossIndexRQS::SumVignettingCxChannelVariance] =
        varp_of_3(p.vignette_params[0].cx, p.vignette_params[1].cx, p.vignette_params[2].cx);
    losses[RawPPISPRegLossIndexRQS::SumVignettingCyChannelVariance] =
        varp_of_3(p.vignette_params[0].cy, p.vignette_params[1].cy, p.vignette_params[2].cy);
    losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha0ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha0, p.vignette_params[1].alpha0, p.vignette_params[2].alpha0);
    losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha1ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha1, p.vignette_params[1].alpha1, p.vignette_params[2].alpha1);
    losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha2ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha2, p.vignette_params[1].alpha2, p.vignette_params[2].alpha2);

    // Color
    const static float2x2 zca_b = float2x2(0.0480542f, -0.0043631f, -0.0043631f, 0.0481283f);
    const static float2x2 zca_r = float2x2(0.0580570f, -0.0179872f, -0.0179872f, 0.0431061f);
    const static float2x2 zca_g = float2x2(0.0433336f, -0.0180537f, -0.0180537f, 0.0580500f);
    const static float2x2 zca_n = float2x2(0.0128369f, -0.0034654f, -0.0034654f, 0.0128158f);
    float2 bd = mul(zca_b, p.color_params.b);
    float2 rd = mul(zca_r, p.color_params.r);
    float2 gd = mul(zca_g, p.color_params.g);
    float2 nd = mul(zca_n, p.color_params.n);
    losses[RawPPISPRegLossIndexRQS::SumColorBx] = bd.x;
    losses[RawPPISPRegLossIndexRQS::SumColorBy] = bd.y;
    losses[RawPPISPRegLossIndexRQS::SumColorRx] = rd.x;
    losses[RawPPISPRegLossIndexRQS::SumColorRy] = rd.y;
    losses[RawPPISPRegLossIndexRQS::SumColorGx] = gd.x;
    losses[RawPPISPRegLossIndexRQS::SumColorGy] = gd.y;
    losses[RawPPISPRegLossIndexRQS::SumColorNx] = nd.x;
    losses[RawPPISPRegLossIndexRQS::SumColorNy] = nd.y;

    // CRF
    losses[RawPPISPRegLossIndexRQS::SumCRFG0ChannelVariance] =
        varp_of_3(p.crf_params[0].g0, p.crf_params[1].g0, p.crf_params[2].g0);
    losses[RawPPISPRegLossIndexRQS::SumCRFG1ChannelVariance] =
        varp_of_3(p.crf_params[0].g1, p.crf_params[1].g1, p.crf_params[2].g1);
    losses[RawPPISPRegLossIndexRQS::SumCRFX0ChannelVariance] =
        varp_of_3(p.crf_params[0].x0, p.crf_params[1].x0, p.crf_params[2].x0);
    losses[RawPPISPRegLossIndexRQS::SumCRFY0ChannelVariance] =
        varp_of_3(p.crf_params[0].y0, p.crf_params[1].y0, p.crf_params[2].y0);
    losses[RawPPISPRegLossIndexRQS::SumCRFGcChannelVariance] =
        varp_of_3(p.crf_params[0].gc, p.crf_params[1].gc, p.crf_params[2].gc);

    return losses;
}

[CudaDeviceExport]
float[PPISP_NUM_PARAMS] compute_raw_ppisp_regularization_loss_vjp(
    float[PPISP_NUM_PARAMS] params,
    float[RawPPISPRegLossIndex::length] grad_out
) {
    DifferentialPair<float[PPISP_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(compute_raw_ppisp_regularization_loss)(
        dp_params, grad_out
    );
    return dp_params.d;
}

[CudaDeviceExport]
float[PPISP_RQS_NUM_PARAMS] compute_raw_ppisp_rqs_regularization_loss_vjp(
    float[PPISP_RQS_NUM_PARAMS] params,
    float[RawPPISPRegLossIndexRQS::length] grad_out
) {
    DifferentialPair<float[PPISP_RQS_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(compute_raw_ppisp_rqs_regularization_loss)(
        dp_params, grad_out
    );
    return dp_params.d;
}

// "no_crf" raw regularization: same as Original but no CRF terms.
[CudaDeviceExport]
[Differentiable]
float[RawPPISPRegLossIndexNoCRF::length] compute_raw_ppisp_no_crf_regularization_loss(
    float[PPISP_NO_CRF_NUM_PARAMS] params
) {
    PPISPParamsNoCRF p = get_ppisp_no_crf_params(params);
    float losses[RawPPISPRegLossIndexNoCRF::length] = { 0.0f };

    // Exposure
    losses[RawPPISPRegLossIndexNoCRF::SumExposure] = p.exposure;

    // Vignetting
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingCrSquared] =
        p.vignette_params[0].cx * p.vignette_params[0].cx + p.vignette_params[0].cy * p.vignette_params[0].cy +
        p.vignette_params[1].cx * p.vignette_params[1].cx + p.vignette_params[1].cy * p.vignette_params[1].cy +
        p.vignette_params[2].cx * p.vignette_params[2].cx + p.vignette_params[2].cy * p.vignette_params[2].cy;
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha0Relu] =
        max(0.0f, p.vignette_params[0].alpha0) +
        max(0.0f, p.vignette_params[1].alpha0) +
        max(0.0f, p.vignette_params[2].alpha0);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha1Relu] =
        max(0.0f, p.vignette_params[0].alpha1) +
        max(0.0f, p.vignette_params[1].alpha1) +
        max(0.0f, p.vignette_params[2].alpha1);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha2Relu] =
        max(0.0f, p.vignette_params[0].alpha2) +
        max(0.0f, p.vignette_params[1].alpha2) +
        max(0.0f, p.vignette_params[2].alpha2);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingCxChannelVariance] =
        varp_of_3(p.vignette_params[0].cx, p.vignette_params[1].cx, p.vignette_params[2].cx);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingCyChannelVariance] =
        varp_of_3(p.vignette_params[0].cy, p.vignette_params[1].cy, p.vignette_params[2].cy);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha0ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha0, p.vignette_params[1].alpha0, p.vignette_params[2].alpha0);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha1ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha1, p.vignette_params[1].alpha1, p.vignette_params[2].alpha1);
    losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha2ChannelVariance] =
        varp_of_3(p.vignette_params[0].alpha2, p.vignette_params[1].alpha2, p.vignette_params[2].alpha2);

    // Color
    const static float2x2 zca_b = float2x2(0.0480542f, -0.0043631f, -0.0043631f, 0.0481283f);
    const static float2x2 zca_r = float2x2(0.0580570f, -0.0179872f, -0.0179872f, 0.0431061f);
    const static float2x2 zca_g = float2x2(0.0433336f, -0.0180537f, -0.0180537f, 0.0580500f);
    const static float2x2 zca_n = float2x2(0.0128369f, -0.0034654f, -0.0034654f, 0.0128158f);
    float2 bd = mul(zca_b, p.color_params.b);
    float2 rd = mul(zca_r, p.color_params.r);
    float2 gd = mul(zca_g, p.color_params.g);
    float2 nd = mul(zca_n, p.color_params.n);
    losses[RawPPISPRegLossIndexNoCRF::SumColorBx] = bd.x;
    losses[RawPPISPRegLossIndexNoCRF::SumColorBy] = bd.y;
    losses[RawPPISPRegLossIndexNoCRF::SumColorRx] = rd.x;
    losses[RawPPISPRegLossIndexNoCRF::SumColorRy] = rd.y;
    losses[RawPPISPRegLossIndexNoCRF::SumColorGx] = gd.x;
    losses[RawPPISPRegLossIndexNoCRF::SumColorGy] = gd.y;
    losses[RawPPISPRegLossIndexNoCRF::SumColorNx] = nd.x;
    losses[RawPPISPRegLossIndexNoCRF::SumColorNy] = nd.y;

    return losses;
}

[CudaDeviceExport]
float[PPISP_NO_CRF_NUM_PARAMS] compute_raw_ppisp_no_crf_regularization_loss_vjp(
    float[PPISP_NO_CRF_NUM_PARAMS] params,
    float[RawPPISPRegLossIndexNoCRF::length] grad_out
) {
    DifferentialPair<float[PPISP_NO_CRF_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(compute_raw_ppisp_no_crf_regularization_loss)(
        dp_params, grad_out
    );
    return dp_params.d;
}

[CudaDeviceExport]
[Differentiable]
float[RawPPISPRegLossIndexNoCRFNoVig::length] compute_raw_ppisp_no_crf_no_vig_regularization_loss(
    float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] params
) {
    PPISPParamsNoCRFNoVig p = get_ppisp_no_crf_no_vig_params(params);
    float losses[RawPPISPRegLossIndexNoCRFNoVig::length] = { 0.0f };

    losses[RawPPISPRegLossIndexNoCRFNoVig::SumExposure] = p.exposure;

    const static float2x2 zca_b = float2x2(0.0480542f, -0.0043631f, -0.0043631f, 0.0481283f);
    const static float2x2 zca_r = float2x2(0.0580570f, -0.0179872f, -0.0179872f, 0.0431061f);
    const static float2x2 zca_g = float2x2(0.0433336f, -0.0180537f, -0.0180537f, 0.0580500f);
    const static float2x2 zca_n = float2x2(0.0128369f, -0.0034654f, -0.0034654f, 0.0128158f);
    float2 bd = mul(zca_b, p.color_params.b);
    float2 rd = mul(zca_r, p.color_params.r);
    float2 gd = mul(zca_g, p.color_params.g);
    float2 nd = mul(zca_n, p.color_params.n);
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorBx] = bd.x;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorBy] = bd.y;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorRx] = rd.x;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorRy] = rd.y;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorGx] = gd.x;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorGy] = gd.y;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorNx] = nd.x;
    losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorNy] = nd.y;

    return losses;
}

[CudaDeviceExport]
float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] compute_raw_ppisp_no_crf_no_vig_regularization_loss_vjp(
    float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] params,
    float[RawPPISPRegLossIndexNoCRFNoVig::length] grad_out
) {
    DifferentialPair<float[PPISP_NO_CRF_NO_VIG_NUM_PARAMS]> dp_params = diffPair(params);
    bwd_diff(compute_raw_ppisp_no_crf_no_vig_regularization_loss)(
        dp_params, grad_out
    );
    return dp_params.d;
}

enum PPISPRegLossIndex {
    ExposureMean,
    VignettingCenter,
    VignettingNonPositivity,
    VignettingChannelVariance,
    ColorMean,
    CRFChannelVariance,
    length
};

[ForceInline]
[Differentiable]
float smooth_l1_loss(float x, float beta) {
    if (abs(x) < beta) {
        return 0.5f * x * x / beta;
    } else {
        return abs(x) - 0.5f * beta;
    }
}

[CudaDeviceExport]
[Differentiable]
float[PPISPRegLossIndex::length] compute_ppisp_regularization_loss(
    float[RawPPISPRegLossIndex::length] raw_losses,
    int num_cameras,
    no_diff float[PPISPRegLossIndex::length] loss_weights
) {
    float losses[PPISPRegLossIndex::length];

    // Exposure mean
    losses[PPISPRegLossIndex::ExposureMean] =
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumExposure] / num_cameras, 0.1f);

    // Vignetting center (cx, cy close to 0)
    losses[PPISPRegLossIndex::VignettingCenter] =
        raw_losses[RawPPISPRegLossIndex::SumVignettingCrSquared] / (3.0f * num_cameras);

    // Vignetting non-positivity (alpha0, alpha1, alpha2 <= 0)
    losses[PPISPRegLossIndex::VignettingNonPositivity] = (
        raw_losses[RawPPISPRegLossIndex::SumVignettingAlpha0Relu] +
        raw_losses[RawPPISPRegLossIndex::SumVignettingAlpha1Relu] +
        raw_losses[RawPPISPRegLossIndex::SumVignettingAlpha2Relu]
    ) / (9.0f * num_cameras);

    // Vignetting channel variance
    losses[PPISPRegLossIndex::VignettingChannelVariance] = (
        raw_losses[RawPPISPRegLossIndex::SumVignettingCxChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumVignettingCyChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumVignettingAlpha0ChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumVignettingAlpha1ChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumVignettingAlpha2ChannelVariance]
    ) / (5.0f * num_cameras);

    // Color mean (latent offsets close to 0)
    losses[PPISPRegLossIndex::ColorMean] = (
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorBx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorBy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorRx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorRy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorGx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorGy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorNx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndex::SumColorNy] / num_cameras, 0.005f)
    ) / 8.0f;

    // CRF channel variance
    losses[PPISPRegLossIndex::CRFChannelVariance] = (
        raw_losses[RawPPISPRegLossIndex::SumCRFToeChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumCRFShoulderChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumCRFGammaChannelVariance] +
        raw_losses[RawPPISPRegLossIndex::SumCRFCenterChannelVariance]
    ) / (4.0f * num_cameras);

    // Apply weights
    losses[PPISPRegLossIndex::ExposureMean] *=
        loss_weights[PPISPRegLossIndex::ExposureMean];
    losses[PPISPRegLossIndex::VignettingCenter] *=
        loss_weights[PPISPRegLossIndex::VignettingCenter];
    losses[PPISPRegLossIndex::VignettingNonPositivity] *=
        loss_weights[PPISPRegLossIndex::VignettingNonPositivity];
    losses[PPISPRegLossIndex::VignettingChannelVariance] *=
        loss_weights[PPISPRegLossIndex::VignettingChannelVariance];
    losses[PPISPRegLossIndex::ColorMean] *=
        loss_weights[PPISPRegLossIndex::ColorMean];
    losses[PPISPRegLossIndex::CRFChannelVariance] *=
        loss_weights[PPISPRegLossIndex::CRFChannelVariance];

    return losses;
}

[CudaDeviceExport]
[Differentiable]
float[PPISPRegLossIndex::length] compute_ppisp_rqs_regularization_loss(
    float[RawPPISPRegLossIndexRQS::length] raw_losses,
    int num_cameras,
    no_diff float[PPISPRegLossIndex::length] loss_weights
) {
    float losses[PPISPRegLossIndex::length];

    // Exposure mean
    losses[PPISPRegLossIndex::ExposureMean] =
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumExposure] / num_cameras, 0.1f);

    // Vignetting center (cx, cy close to 0)
    losses[PPISPRegLossIndex::VignettingCenter] =
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingCrSquared] / (3.0f * num_cameras);
    // Vignetting non-positivity (alpha0, alpha1, alpha2 <= 0)
    losses[PPISPRegLossIndex::VignettingNonPositivity] = (
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha0Relu] +
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha1Relu] +
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha2Relu]
    ) / (9.0f * num_cameras);

    // Vignetting channel variance
    losses[PPISPRegLossIndex::VignettingChannelVariance] = (
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingCxChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingCyChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha0ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha1ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumVignettingAlpha2ChannelVariance]
    ) / (5.0f * num_cameras);

    // Color mean (latent offsets close to 0)
    losses[PPISPRegLossIndex::ColorMean] = (
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorBx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorBy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorRx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorRy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorGx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorGy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorNx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexRQS::SumColorNy] / num_cameras, 0.005f)
    ) / 8.0f;

    // CRF channel variance
    losses[PPISPRegLossIndex::CRFChannelVariance] = (
        raw_losses[RawPPISPRegLossIndexRQS::SumCRFG0ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumCRFG1ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumCRFX0ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumCRFY0ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexRQS::SumCRFGcChannelVariance]
    ) / (5.0f * num_cameras);

    // Apply weights
    losses[PPISPRegLossIndex::ExposureMean] *=
        loss_weights[PPISPRegLossIndex::ExposureMean];
    losses[PPISPRegLossIndex::VignettingCenter] *=
        loss_weights[PPISPRegLossIndex::VignettingCenter];
    losses[PPISPRegLossIndex::VignettingNonPositivity] *=
        loss_weights[PPISPRegLossIndex::VignettingNonPositivity];
    losses[PPISPRegLossIndex::VignettingChannelVariance] *=
        loss_weights[PPISPRegLossIndex::VignettingChannelVariance];
    losses[PPISPRegLossIndex::ColorMean] *=
        loss_weights[PPISPRegLossIndex::ColorMean];
    losses[PPISPRegLossIndex::CRFChannelVariance] *=
        loss_weights[PPISPRegLossIndex::CRFChannelVariance];

    return losses;
}

[CudaDeviceExport]
float[RawPPISPRegLossIndex::length] compute_ppisp_regularization_loss_vjp(
    float[RawPPISPRegLossIndex::length] raw_losses,
    int num_cameras,
    float[PPISPRegLossIndex::length] loss_weights,
    float[PPISPRegLossIndex::length] grad_out
) {
    DifferentialPair<float[RawPPISPRegLossIndex::length]> dp_raw_losses = diffPair(raw_losses);
    bwd_diff(compute_ppisp_regularization_loss)(
        dp_raw_losses, num_cameras, loss_weights, grad_out
    );
    return dp_raw_losses.d;
}

[CudaDeviceExport]
float[RawPPISPRegLossIndexRQS::length] compute_ppisp_rqs_regularization_loss_vjp(
    float[RawPPISPRegLossIndexRQS::length] raw_losses,
    int num_cameras,
    float[PPISPRegLossIndex::length] loss_weights,
    float[PPISPRegLossIndex::length] grad_out
) {
    DifferentialPair<float[RawPPISPRegLossIndexRQS::length]> dp_raw_losses = diffPair(raw_losses);
    bwd_diff(compute_ppisp_rqs_regularization_loss)(
        dp_raw_losses, num_cameras, loss_weights, grad_out
    );
    return dp_raw_losses.d;
}

[CudaDeviceExport]
[Differentiable]
float[PPISPRegLossIndex::length] compute_ppisp_no_crf_regularization_loss(
    float[RawPPISPRegLossIndexNoCRF::length] raw_losses,
    int num_cameras,
    no_diff float[PPISPRegLossIndex::length] loss_weights
) {
    float losses[PPISPRegLossIndex::length];

    // Exposure mean
    losses[PPISPRegLossIndex::ExposureMean] =
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumExposure] / num_cameras, 0.1f);

    // Vignetting center (cx, cy close to 0)
    losses[PPISPRegLossIndex::VignettingCenter] =
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingCrSquared] / (3.0f * num_cameras);

    // Vignetting non-positivity (alpha0, alpha1, alpha2 <= 0)
    losses[PPISPRegLossIndex::VignettingNonPositivity] = (
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha0Relu] +
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha1Relu] +
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha2Relu]
    ) / (9.0f * num_cameras);

    // Vignetting channel variance
    losses[PPISPRegLossIndex::VignettingChannelVariance] = (
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingCxChannelVariance] +
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingCyChannelVariance] +
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha0ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha1ChannelVariance] +
        raw_losses[RawPPISPRegLossIndexNoCRF::SumVignettingAlpha2ChannelVariance]
    ) / (5.0f * num_cameras);

    // Color mean (latent offsets close to 0)
    losses[PPISPRegLossIndex::ColorMean] = (
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorBx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorBy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorRx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorRy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorGx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorGy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorNx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRF::SumColorNy] / num_cameras, 0.005f)
    ) / 8.0f;

    // No CRF in this mode
    losses[PPISPRegLossIndex::CRFChannelVariance] = 0.0f;

    // Apply weights
    losses[PPISPRegLossIndex::ExposureMean] *=
        loss_weights[PPISPRegLossIndex::ExposureMean];
    losses[PPISPRegLossIndex::VignettingCenter] *=
        loss_weights[PPISPRegLossIndex::VignettingCenter];
    losses[PPISPRegLossIndex::VignettingNonPositivity] *=
        loss_weights[PPISPRegLossIndex::VignettingNonPositivity];
    losses[PPISPRegLossIndex::VignettingChannelVariance] *=
        loss_weights[PPISPRegLossIndex::VignettingChannelVariance];
    losses[PPISPRegLossIndex::ColorMean] *=
        loss_weights[PPISPRegLossIndex::ColorMean];

    return losses;
}

[CudaDeviceExport]
float[RawPPISPRegLossIndexNoCRF::length] compute_ppisp_no_crf_regularization_loss_vjp(
    float[RawPPISPRegLossIndexNoCRF::length] raw_losses,
    int num_cameras,
    float[PPISPRegLossIndex::length] loss_weights,
    float[PPISPRegLossIndex::length] grad_out
) {
    DifferentialPair<float[RawPPISPRegLossIndexNoCRF::length]> dp_raw_losses = diffPair(raw_losses);
    bwd_diff(compute_ppisp_no_crf_regularization_loss)(
        dp_raw_losses, num_cameras, loss_weights, grad_out
    );
    return dp_raw_losses.d;
}

[CudaDeviceExport]
[Differentiable]
float[PPISPRegLossIndex::length] compute_ppisp_no_crf_no_vig_regularization_loss(
    float[RawPPISPRegLossIndexNoCRFNoVig::length] raw_losses,
    int num_cameras,
    no_diff float[PPISPRegLossIndex::length] loss_weights
) {
    float losses[PPISPRegLossIndex::length];

    // Exposure mean
    losses[PPISPRegLossIndex::ExposureMean] =
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumExposure] / num_cameras, 0.1f);

    // Color mean (latent offsets close to 0)
    losses[PPISPRegLossIndex::ColorMean] = (
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorBx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorBy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorRx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorRy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorGx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorGy] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorNx] / num_cameras, 0.005f) +
        smooth_l1_loss(raw_losses[RawPPISPRegLossIndexNoCRFNoVig::SumColorNy] / num_cameras, 0.005f)
    ) / 8.0f;

    // No vignetting and no CRF in this mode
    losses[PPISPRegLossIndex::VignettingCenter] = 0.0f;
    losses[PPISPRegLossIndex::VignettingNonPositivity] = 0.0f;
    losses[PPISPRegLossIndex::VignettingChannelVariance] = 0.0f;
    losses[PPISPRegLossIndex::CRFChannelVariance] = 0.0f;

    // Apply weights
    losses[PPISPRegLossIndex::ExposureMean] *=
        loss_weights[PPISPRegLossIndex::ExposureMean];
    losses[PPISPRegLossIndex::ColorMean] *=
        loss_weights[PPISPRegLossIndex::ColorMean];

    return losses;
}

[CudaDeviceExport]
float[RawPPISPRegLossIndexNoCRFNoVig::length] compute_ppisp_no_crf_no_vig_regularization_loss_vjp(
    float[RawPPISPRegLossIndexNoCRFNoVig::length] raw_losses,
    int num_cameras,
    float[PPISPRegLossIndex::length] loss_weights,
    float[PPISPRegLossIndex::length] grad_out
) {
    DifferentialPair<float[RawPPISPRegLossIndexNoCRFNoVig::length]> dp_raw_losses = diffPair(raw_losses);
    bwd_diff(compute_ppisp_no_crf_no_vig_regularization_loss)(
        dp_raw_losses, num_cameras, loss_weights, grad_out
    );
    return dp_raw_losses.d;
}
