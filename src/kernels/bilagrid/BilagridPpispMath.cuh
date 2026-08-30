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

#pragma once

#include "backend/api/BackendTypes.h"

namespace bilagrid_ppisp {

// ============================================================================
// Note: make_float2, make_float3, make_float4 are provided by CUDA
// ============================================================================

// ============================================================================
// Matrix Types (row-major storage for efficient access)
// ============================================================================

// 2x2 matrix (4 floats, row-major)
struct float2x2 {
    float m[4];  // [m00, m01, m10, m11]

    __device__ __forceinline__ float &operator()(int row, int col) { return m[row * 2 + col]; }

    __device__ __forceinline__ const float &operator()(int row, int col) const {
        return m[row * 2 + col];
    }
};

// 3x3 matrix (9 floats, row-major)
struct float3x3 {
    float m[9];  // [m00, m01, m02, m10, m11, m12, m20, m21, m22]

    __device__ __forceinline__ float &operator()(int row, int col) { return m[row * 3 + col]; }

    __device__ __forceinline__ const float &operator()(int row, int col) const {
        return m[row * 3 + col];
    }
};

// 4x4 matrix (16 floats, row-major)
struct float4x4 {
    float m[16];  // row-major

    __device__ __forceinline__ float &operator()(int row, int col) { return m[row * 4 + col]; }

    __device__ __forceinline__ const float &operator()(int row, int col) const {
        return m[row * 4 + col];
    }
};

// Matrix constructors
__device__ __forceinline__ float2x2 make_float2x2(float m00, float m01, float m10, float m11) {
    float2x2 mat;
    mat.m[0] = m00;
    mat.m[1] = m01;
    mat.m[2] = m10;
    mat.m[3] = m11;
    return mat;
}

__device__ __forceinline__ float3x3 make_float3x3(float m00, float m01, float m02, float m10,
                                                  float m11, float m12, float m20, float m21,
                                                  float m22) {
    float3x3 mat;
    mat.m[0] = m00;
    mat.m[1] = m01;
    mat.m[2] = m02;
    mat.m[3] = m10;
    mat.m[4] = m11;
    mat.m[5] = m12;
    mat.m[6] = m20;
    mat.m[7] = m21;
    mat.m[8] = m22;
    return mat;
}

__device__ __forceinline__ float4x4 make_float4x4(float m00, float m01, float m02, float m03,
                                                  float m10, float m11, float m12, float m13,
                                                  float m20, float m21, float m22, float m23,
                                                  float m30, float m31, float m32, float m33) {
    float4x4 mat;
    mat.m[0] = m00;
    mat.m[1] = m01;
    mat.m[2] = m02;
    mat.m[3] = m03;
    mat.m[4] = m10;
    mat.m[5] = m11;
    mat.m[6] = m12;
    mat.m[7] = m13;
    mat.m[8] = m20;
    mat.m[9] = m21;
    mat.m[10] = m22;
    mat.m[11] = m23;
    mat.m[12] = m30;
    mat.m[13] = m31;
    mat.m[14] = m32;
    mat.m[15] = m33;
    return mat;
}

// ============================================================================
// Vector Operators (device-only, optimized with intrinsics)
// ============================================================================

// float2 operators
__device__ __forceinline__ float2 operator+(const float2 &a, const float2 &b) {
    return make_float2(a.x + b.x, a.y + b.y);
}

__device__ __forceinline__ float2 operator-(const float2 &a, const float2 &b) {
    return make_float2(a.x - b.x, a.y - b.y);
}

__device__ __forceinline__ float2 operator*(const float2 &a, float s) {
    return make_float2(a.x * s, a.y * s);
}

__device__ __forceinline__ float2 operator*(float s, const float2 &a) {
    return make_float2(a.x * s, a.y * s);
}

__device__ __forceinline__ float2 operator*(const float2 &a, const float2 &b) {
    return make_float2(a.x * b.x, a.y * b.y);
}

__device__ __forceinline__ float2 operator/(const float2 &a, float s) {
    float inv_s = __fdividef(1.0f, s);
    return make_float2(a.x * inv_s, a.y * inv_s);
}

// float3 operators
__device__ __forceinline__ float3 operator+(const float3 &a, const float3 &b) {
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__device__ __forceinline__ float3 operator-(const float3 &a, const float3 &b) {
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__device__ __forceinline__ float3 operator*(const float3 &a, float s) {
    return make_float3(a.x * s, a.y * s, a.z * s);
}

__device__ __forceinline__ float3 operator*(float s, const float3 &a) {
    return make_float3(a.x * s, a.y * s, a.z * s);
}

__device__ __forceinline__ float3 operator*(const float3 &a, const float3 &b) {
    return make_float3(a.x * b.x, a.y * b.y, a.z * b.z);
}

__device__ __forceinline__ float3 operator/(const float3 &a, float s) {
    float inv_s = __fdividef(1.0f, s);
    return make_float3(a.x * inv_s, a.y * inv_s, a.z * inv_s);
}

// float4 operators
__device__ __forceinline__ float4 operator+(const float4 &a, const float4 &b) {
    return make_float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

__device__ __forceinline__ float4 operator-(const float4 &a, const float4 &b) {
    return make_float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

__device__ __forceinline__ float4 operator*(const float4 &a, float s) {
    return make_float4(a.x * s, a.y * s, a.z * s, a.w * s);
}

__device__ __forceinline__ float4 operator*(float s, const float4 &a) {
    return make_float4(a.x * s, a.y * s, a.z * s, a.w * s);
}

__device__ __forceinline__ float4 operator*(const float4 &a, const float4 &b) {
    return make_float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

__device__ __forceinline__ float4 operator/(const float4 &a, float s) {
    float inv_s = __fdividef(1.0f, s);
    return make_float4(a.x * inv_s, a.y * inv_s, a.z * inv_s, a.w * inv_s);
}

// ============================================================================
// Vector Functions (optimized)
// ============================================================================

// Dot product (using FMA for precision)
__device__ __forceinline__ float dot(const float2 &a, const float2 &b) {
    return __fmaf_rn(a.x, b.x, a.y * b.y);
}

__device__ __forceinline__ float dot(const float3 &a, const float3 &b) {
    return __fmaf_rn(a.x, b.x, __fmaf_rn(a.y, b.y, a.z * b.z));
}

__device__ __forceinline__ float dot(const float4 &a, const float4 &b) {
    return __fmaf_rn(a.x, b.x, __fmaf_rn(a.y, b.y, __fmaf_rn(a.z, b.z, a.w * b.w)));
}

// Cross product
__device__ __forceinline__ float3 cross(const float3 &a, const float3 &b) {
    return make_float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// Length
__device__ __forceinline__ float length(const float2 &a) { return sqrtf(dot(a, a)); }

__device__ __forceinline__ float length(const float3 &a) { return sqrtf(dot(a, a)); }

__device__ __forceinline__ float length(const float4 &a) { return sqrtf(dot(a, a)); }

// Fast reciprocal length (less precise but faster)
__device__ __forceinline__ float rlength(const float2 &a) { return __frsqrt_rn(dot(a, a)); }

__device__ __forceinline__ float rlength(const float3 &a) { return __frsqrt_rn(dot(a, a)); }

__device__ __forceinline__ float rlength(const float4 &a) { return __frsqrt_rn(dot(a, a)); }

// Normalize
__device__ __forceinline__ float2 normalize(const float2 &a) { return a * rlength(a); }

__device__ __forceinline__ float3 normalize(const float3 &a) { return a * rlength(a); }

__device__ __forceinline__ float4 normalize(const float4 &a) { return a * rlength(a); }

// Clamp (per-component)
__device__ __forceinline__ float2 clamp(const float2 &v, float min_val, float max_val) {
    return make_float2(fminf(fmaxf(v.x, min_val), max_val), fminf(fmaxf(v.y, min_val), max_val));
}

__device__ __forceinline__ float3 clamp(const float3 &v, float min_val, float max_val) {
    return make_float3(fminf(fmaxf(v.x, min_val), max_val), fminf(fmaxf(v.y, min_val), max_val),
                       fminf(fmaxf(v.z, min_val), max_val));
}

__device__ __forceinline__ float4 clamp(const float4 &v, float min_val, float max_val) {
    return make_float4(fminf(fmaxf(v.x, min_val), max_val), fminf(fmaxf(v.y, min_val), max_val),
                       fminf(fmaxf(v.z, min_val), max_val), fminf(fmaxf(v.w, min_val), max_val));
}

// Lerp (linear interpolation) - optimized with FMA
__device__ __forceinline__ float lerp(float a, float b, float t) { return __fmaf_rn(b - a, t, a); }

__device__ __forceinline__ float2 lerp(const float2 &a, const float2 &b, float t) {
    return make_float2(lerp(a.x, b.x, t), lerp(a.y, b.y, t));
}

__device__ __forceinline__ float3 lerp(const float3 &a, const float3 &b, float t) {
    return make_float3(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

__device__ __forceinline__ float4 lerp(const float4 &a, const float4 &b, float t) {
    return make_float4(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t));
}

// Element-wise pow (use __powf for speed)
__device__ __forceinline__ float2 pow(const float2 &v, float exp) {
    return make_float2(__powf(v.x, exp), __powf(v.y, exp));
}

__device__ __forceinline__ float3 pow(const float3 &v, float exp) {
    return make_float3(__powf(v.x, exp), __powf(v.y, exp), __powf(v.z, exp));
}

__device__ __forceinline__ float3 pow(const float3 &v, const float3 &exp) {
    return make_float3(__powf(v.x, exp.x), __powf(v.y, exp.y), __powf(v.z, exp.z));
}

__device__ __forceinline__ float4 pow(const float4 &v, float exp) {
    return make_float4(__powf(v.x, exp), __powf(v.y, exp), __powf(v.z, exp), __powf(v.w, exp));
}

// Min/Max (per-component)
__device__ __forceinline__ float2 min(const float2 &a, const float2 &b) {
    return make_float2(fminf(a.x, b.x), fminf(a.y, b.y));
}

__device__ __forceinline__ float3 min(const float3 &a, const float3 &b) {
    return make_float3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z));
}

__device__ __forceinline__ float4 min(const float4 &a, const float4 &b) {
    return make_float4(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z), fminf(a.w, b.w));
}

__device__ __forceinline__ float2 max(const float2 &a, const float2 &b) {
    return make_float2(fmaxf(a.x, b.x), fmaxf(a.y, b.y));
}

__device__ __forceinline__ float3 max(const float3 &a, const float3 &b) {
    return make_float3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z));
}

__device__ __forceinline__ float4 max(const float4 &a, const float4 &b) {
    return make_float4(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z), fmaxf(a.w, b.w));
}

// ============================================================================
// Matrix-Vector Operations (optimized with FMA)
// ============================================================================

// 2x2 matrix-vector multiplication: y = A * x
__device__ __forceinline__ float2 operator*(const float2x2 &mat, const float2 &v) {
    return make_float2(__fmaf_rn(mat.m[0], v.x, mat.m[1] * v.y),
                       __fmaf_rn(mat.m[2], v.x, mat.m[3] * v.y));
}

// 3x3 matrix-vector multiplication: y = A * x
__device__ __forceinline__ float3 operator*(const float3x3 &mat, const float3 &v) {
    return make_float3(__fmaf_rn(mat.m[0], v.x, __fmaf_rn(mat.m[1], v.y, mat.m[2] * v.z)),
                       __fmaf_rn(mat.m[3], v.x, __fmaf_rn(mat.m[4], v.y, mat.m[5] * v.z)),
                       __fmaf_rn(mat.m[6], v.x, __fmaf_rn(mat.m[7], v.y, mat.m[8] * v.z)));
}

// 4x4 matrix-vector multiplication: y = A * x
__device__ __forceinline__ float4 operator*(const float4x4 &mat, const float4 &v) {
    return make_float4(
        __fmaf_rn(mat.m[0], v.x,
                  __fmaf_rn(mat.m[1], v.y, __fmaf_rn(mat.m[2], v.z, mat.m[3] * v.w))),
        __fmaf_rn(mat.m[4], v.x,
                  __fmaf_rn(mat.m[5], v.y, __fmaf_rn(mat.m[6], v.z, mat.m[7] * v.w))),
        __fmaf_rn(mat.m[8], v.x,
                  __fmaf_rn(mat.m[9], v.y, __fmaf_rn(mat.m[10], v.z, mat.m[11] * v.w))),
        __fmaf_rn(mat.m[12], v.x,
                  __fmaf_rn(mat.m[13], v.y, __fmaf_rn(mat.m[14], v.z, mat.m[15] * v.w))));
}

// ============================================================================
// Matrix-Matrix Operations (optimized with FMA and unrolling)
// ============================================================================

// 2x2 matrix multiplication: C = A * B
__device__ __forceinline__ float2x2 operator*(const float2x2 &A, const float2x2 &B) {
    float2x2 C;
#pragma unroll
    for (int i = 0; i < 2; i++) {
#pragma unroll
        for (int j = 0; j < 2; j++) {
            C(i, j) = __fmaf_rn(A(i, 0), B(0, j), A(i, 1) * B(1, j));
        }
    }
    return C;
}

// 3x3 matrix multiplication: C = A * B
__device__ __forceinline__ float3x3 operator*(const float3x3 &A, const float3x3 &B) {
    float3x3 C;
#pragma unroll
    for (int i = 0; i < 3; i++) {
#pragma unroll
        for (int j = 0; j < 3; j++) {
            C(i, j) = __fmaf_rn(A(i, 0), B(0, j), __fmaf_rn(A(i, 1), B(1, j), A(i, 2) * B(2, j)));
        }
    }
    return C;
}

// Matrix transpose
__device__ __forceinline__ float2x2 transpose(const float2x2 &A) {
    float2x2 AT;
    AT(0, 0) = A(0, 0);
    AT(0, 1) = A(1, 0);
    AT(1, 0) = A(0, 1);
    AT(1, 1) = A(1, 1);
    return AT;
}

__device__ __forceinline__ float3x3 transpose(const float3x3 &A) {
    float3x3 AT;
#pragma unroll
    for (int i = 0; i < 3; i++) {
#pragma unroll
        for (int j = 0; j < 3; j++) {
            AT(i, j) = A(j, i);
        }
    }
    return AT;
}

// ============================================================================
// Utility Functions
// ============================================================================

// Identity matrices
__device__ __forceinline__ float2x2 identity_2x2() { return make_float2x2(1.0f, 0.0f, 0.0f, 1.0f); }

__device__ __forceinline__ float3x3 identity_3x3() {
    return make_float3x3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
}

// ============================================================================
// ISP Parameter Structures
// ============================================================================

// Vignetting parameters per channel (5 params: cx, cy, alpha0, alpha1, alpha2)
struct VignettingChannelParams {
    float cx;      // Optical center X
    float cy;      // Optical center Y
    float alpha0;  // r^2 coefficient
    float alpha1;  // r^4 coefficient
    float alpha2;  // r^6 coefficient
};

// CRF parameters per channel for PPISP (4 params, pre-transformation)
struct CRFPPISPChannelParams {
    float toe;       // Toe strength (before softplus)
    float shoulder;  // Shoulder strength (before softplus)
    float gamma;     // Gamma (before softplus)
    float center;    // Center point (before sigmoid)
};

// Color correction parameters for PPISP (4 color points: BRGN, 2D offsets each)
struct ColorPPISPParams {
    float2 b;  // Blue latent offsets
    float2 r;  // Red latent offsets
    float2 g;  // Green latent offsets
    float2 n;  // Neutral latent offsets
};

// ============================================================================
// ISP-Specific Helper Functions
// ============================================================================

// Color correction pinv blocks (constant memory)
__constant__ float COLOR_PINV_BLOCKS[4][4] = {
    {0.0480542f, -0.0043631f, -0.0043631f, 0.0481283f},  // Blue
    {0.0580570f, -0.0179872f, -0.0179872f, 0.0431061f},  // Red
    {0.0433336f, -0.0180537f, -0.0180537f, 0.0580500f},  // Green
    {0.0128369f, -0.0034654f, -0.0034654f, 0.0128158f}   // Neutral
};

// Floor on the RGI renormalization denominator, as a fraction of |intensity|:
// the identity homography gives rgi_out.z == intensity, so it binds only on an
// aggressive H's pole line, capping chroma gain. Mirrors shaders/ppisp.slang.
__device__ constexpr float kColorNormMinZ = 0.05f;

// Softplus transformation for bounded positive parameters
__device__ __forceinline__ float bounded_positive_forward(float raw, float min_value = 0.1f) {
    return min_value + __logf(1.0f + __expf(raw));
}

// Sigmoid transformation for clamped parameters
__device__ __forceinline__ float clamped_forward(float raw) {
    return __fdividef(1.0f, 1.0f + __expf(-raw));
}

// Compute 3x3 homography matrix from color parameters
__device__ __forceinline__ float3x3 compute_homography(const ColorPPISPParams *params) {
    // Load latent offsets for control chromaticities (B, R, G, N)
    const float2 &b_lat = params->b;
    const float2 &r_lat = params->r;
    const float2 &g_lat = params->g;
    const float2 &n_lat = params->n;

    // Map latent to real offsets via ZCA 2x2 blocks (stored as constant 4-element arrays)
    float2x2 zca_b, zca_r, zca_g, zca_n;
    zca_b.m[0] = COLOR_PINV_BLOCKS[0][0];
    zca_b.m[1] = COLOR_PINV_BLOCKS[0][1];
    zca_b.m[2] = COLOR_PINV_BLOCKS[0][2];
    zca_b.m[3] = COLOR_PINV_BLOCKS[0][3];

    zca_r.m[0] = COLOR_PINV_BLOCKS[1][0];
    zca_r.m[1] = COLOR_PINV_BLOCKS[1][1];
    zca_r.m[2] = COLOR_PINV_BLOCKS[1][2];
    zca_r.m[3] = COLOR_PINV_BLOCKS[1][3];

    zca_g.m[0] = COLOR_PINV_BLOCKS[2][0];
    zca_g.m[1] = COLOR_PINV_BLOCKS[2][1];
    zca_g.m[2] = COLOR_PINV_BLOCKS[2][2];
    zca_g.m[3] = COLOR_PINV_BLOCKS[2][3];

    zca_n.m[0] = COLOR_PINV_BLOCKS[3][0];
    zca_n.m[1] = COLOR_PINV_BLOCKS[3][1];
    zca_n.m[2] = COLOR_PINV_BLOCKS[3][2];
    zca_n.m[3] = COLOR_PINV_BLOCKS[3][3];

    float2 bd = zca_b * b_lat;
    float2 rd = zca_r * r_lat;
    float2 gd = zca_g * g_lat;
    float2 nd = zca_n * n_lat;

    // Fixed sources (r, g, intensity) + offsets = targets
    float3 t_b = make_float3(0.0f + bd.x, 0.0f + bd.y, 1.0f);
    float3 t_r = make_float3(1.0f + rd.x, 0.0f + rd.y, 1.0f);
    float3 t_g = make_float3(0.0f + gd.x, 1.0f + gd.y, 1.0f);
    float3 t_gray = make_float3(1.0f / 3.0f + nd.x, 1.0f / 3.0f + nd.y, 1.0f);

    // T has columns [t_b, t_r, t_g] (column-major stored as row-major)
    float3x3 T = make_float3x3(t_b.x, t_r.x, t_g.x, t_b.y, t_r.y, t_g.y, t_b.z, t_r.z, t_g.z);

    // Skew-symmetric matrix [t_gray]_x
    float3x3 skew = make_float3x3(0.0f, -t_gray.z, t_gray.y, t_gray.z, 0.0f, -t_gray.x, -t_gray.y,
                                  t_gray.x, 0.0f);

    // M = skew * T
    float3x3 M = skew * T;

    // Nullspace vector lambda via cross product of rows
    float3 r0 = make_float3(M(0, 0), M(0, 1), M(0, 2));
    float3 r1 = make_float3(M(1, 0), M(1, 1), M(1, 2));
    float3 r2 = make_float3(M(2, 0), M(2, 1), M(2, 2));

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
    float3x3 S_inv = make_float3x3(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // D = diag(lambda)
    float3x3 D =
        make_float3x3(lambda_v.x, 0.0f, 0.0f, 0.0f, lambda_v.y, 0.0f, 0.0f, 0.0f, lambda_v.z);

    // H = T * D * S_inv
    float3x3 H = T * D * S_inv;

    // Normalize so H[2][2] = 1
    float s = H(2, 2);
    if (fabsf(s) > 1.0e-20f) {
        float inv_s = 1.0f / s;
#pragma unroll
        for (int i = 0; i < 9; i++) {
            H.m[i] *= inv_s;
        }
    }

    return H;
}

// ----------------------------------------------------------------------------
// 3x3 Matrix Inverse
// ----------------------------------------------------------------------------
__device__ __forceinline__ float3x3 matrix_inverse_3x3(const float3x3 &M) {
    // Compute determinant
    float det = M.m[0] * (M.m[4] * M.m[8] - M.m[5] * M.m[7]) -
                M.m[1] * (M.m[3] * M.m[8] - M.m[5] * M.m[6]) +
                M.m[2] * (M.m[3] * M.m[7] - M.m[4] * M.m[6]);

    // Add epsilon to prevent division by zero
    const float eps = 1e-8f;
    float inv_det = 1.0f / (fabsf(det) > eps ? det : (det >= 0.0f ? eps : -eps));

    // Compute cofactor matrix and transpose (adjugate)
    float3x3 inv;
    inv.m[0] = (M.m[4] * M.m[8] - M.m[5] * M.m[7]) * inv_det;
    inv.m[1] = (M.m[2] * M.m[7] - M.m[1] * M.m[8]) * inv_det;
    inv.m[2] = (M.m[1] * M.m[5] - M.m[2] * M.m[4]) * inv_det;
    inv.m[3] = (M.m[5] * M.m[6] - M.m[3] * M.m[8]) * inv_det;
    inv.m[4] = (M.m[0] * M.m[8] - M.m[2] * M.m[6]) * inv_det;
    inv.m[5] = (M.m[2] * M.m[3] - M.m[0] * M.m[5]) * inv_det;
    inv.m[6] = (M.m[3] * M.m[7] - M.m[4] * M.m[6]) * inv_det;
    inv.m[7] = (M.m[1] * M.m[6] - M.m[0] * M.m[7]) * inv_det;
    inv.m[8] = (M.m[0] * M.m[4] - M.m[1] * M.m[3]) * inv_det;

    return inv;
}

// ============================================================================
// Modular ISP Component Functions
// ============================================================================

// ----------------------------------------------------------------------------
// Exposure Compensation
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_exposure(const float3 &rgb_in, float exposure_param,
                                               float3 &rgb_out) {
    float exposure_factor = exp2f(exposure_param);
    rgb_out = rgb_in * exposure_factor;
}

// ----------------------------------------------------------------------------
// Vignetting Correction
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_vignetting(const float3 &rgb_in,
                                                 const VignettingChannelParams *vignetting_params,
                                                 const float2 &pixel_coords, float resolution_x,
                                                 float resolution_y, float3 &rgb_out) {
    // Normalize coordinates to [-0.5, 0.5] range based on max dimension
    float max_res = fmaxf(resolution_x, resolution_y);
    float2 uv = make_float2(__fdividef(pixel_coords.x - resolution_x * 0.5f, max_res),
                            __fdividef(pixel_coords.y - resolution_y * 0.5f, max_res));

    float rgb_arr[3] = {rgb_in.x, rgb_in.y, rgb_in.z};

#pragma unroll
    for (int i = 0; i < 3; i++) {
        const VignettingChannelParams &params = vignetting_params[i];

        float dx = uv.x - params.cx;
        float dy = uv.y - params.cy;
        float r2 = __fmaf_rn(dx, dx, dy * dy);
        float r4 = r2 * r2;
        float r6 = r4 * r2;

        float falloff = __fmaf_rn(params.alpha2, r6,
                                  __fmaf_rn(params.alpha1, r4, __fmaf_rn(params.alpha0, r2, 1.0f)));
        falloff = fmaxf(0.0f, fminf(1.0f, falloff));

        rgb_arr[i] *= falloff;
    }

    rgb_out = make_float3(rgb_arr[0], rgb_arr[1], rgb_arr[2]);
}

// ----------------------------------------------------------------------------
// Color Correction - PPISP (Homography)
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_color_correction_ppisp(const float3 &rgb_in,
                                                             const ColorPPISPParams *color_params,
                                                             float3 &rgb_out) {
    // Compute homography matrix from control-point parametrization
    float3x3 H = compute_homography(color_params);

    // Convert to RGI space
    float intensity = rgb_in.x + rgb_in.y + rgb_in.z;
    float3 rgi_in = make_float3(rgb_in.x, rgb_in.y, intensity);

    // Apply homography
    float3 rgi_out = H * rgi_in;

    // Renormalize to the input intensity, off a strictly positive z. Third
    // component is `intensity` by construction, so it is written back rather
    // than recomputed: intensity is preserved exactly, at every z.
    float z = fmaxf(rgi_out.z, kColorNormMinZ * fabsf(intensity) + 1.0e-8f);
    float norm_factor = __fdividef(intensity, z);
    float out_r = rgi_out.x * norm_factor;
    float out_g = rgi_out.y * norm_factor;

    rgb_out = make_float3(out_r, out_g, intensity - out_r - out_g);
}

// ----------------------------------------------------------------------------
// CRF - PPISP (Parametric toe-shoulder curve)
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_crf_ppisp(const float3 &rgb_in,
                                                const CRFPPISPChannelParams *crf_params,
                                                float3 &rgb_out) {
    float3 rgb_clamped = clamp(rgb_in, 0.0f, 1.0f);
    float out_arr[3];

#pragma unroll
    for (int i = 0; i < 3; i++) {
        float x = (i == 0) ? rgb_clamped.x : (i == 1) ? rgb_clamped.y : rgb_clamped.z;

        const CRFPPISPChannelParams &params = crf_params[i];

        // Transform parameters
        float toe = bounded_positive_forward(params.toe, 0.3f);
        float shoulder = bounded_positive_forward(params.shoulder, 0.3f);
        float gamma = bounded_positive_forward(params.gamma, 0.1f);
        float center = clamped_forward(params.center);

        // Compute a, b coefficients
        float lerp_val = __fmaf_rn(shoulder - toe, center, toe);
        float a = __fdividef(shoulder * center, lerp_val);
        float b = 1.0f - a;

        float y;

        // Piecewise toe-shoulder curve
        if (x <= center) {
            y = a * __powf(__fdividef(x, center), toe);
        } else {
            y = 1.0f - b * __powf(__fdividef(1.0f - x, 1.0f - center), shoulder);
        }

        // Apply gamma
        out_arr[i] = __powf(fmaxf(0.0f, y), gamma);
    }

    rgb_out = make_float3(out_arr[0], out_arr[1], out_arr[2]);
}
}  // namespace bilagrid_ppisp
