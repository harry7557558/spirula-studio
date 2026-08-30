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

#include "kernels/bilagrid/BilagridPpispMath.cuh"

namespace bilagrid_ppisp {

// ============================================================================
// Backward Pass Helpers for Vector Operations
// ============================================================================

// Dot product backward: d_a, d_b given grad_output
// forward: out = dot(a, b) = a.x*b.x + a.y*b.y
// backward: d_a = grad_output * b, d_b = grad_output * a
__device__ __forceinline__ void dot_bwd(float grad_output, const float2 &a, const float2 &b,
                                        float2 &grad_a, float2 &grad_b) {
    grad_a = grad_output * b;
    grad_b = grad_output * a;
}

__device__ __forceinline__ void dot_bwd(float grad_output, const float3 &a, const float3 &b,
                                        float3 &grad_a, float3 &grad_b) {
    grad_a = grad_output * b;
    grad_b = grad_output * a;
}

__device__ __forceinline__ void dot_bwd(float grad_output, const float4 &a, const float4 &b,
                                        float4 &grad_a, float4 &grad_b) {
    grad_a = grad_output * b;
    grad_b = grad_output * a;
}

// Cross product backward
// forward: out = cross(a, b) = a x b
// backward: grad_a = b x grad_out, grad_b = grad_out x a
__device__ __forceinline__ void cross_bwd(const float3 &grad_output, const float3 &a,
                                          const float3 &b, float3 &grad_a, float3 &grad_b) {
    grad_a = cross(b, grad_output);
    grad_b = cross(grad_output, a);
}

// Length backward
// forward: out = sqrt(dot(a, a))
// backward: d_a = grad_output * a / length(a)
__device__ __forceinline__ void length_bwd(float grad_output, const float2 &a, float2 &grad_a) {
    float len = length(a);
    grad_a = (grad_output / (len + 1e-8f)) * a;
}

__device__ __forceinline__ void length_bwd(float grad_output, const float3 &a, float3 &grad_a) {
    float len = length(a);
    grad_a = (grad_output / (len + 1e-8f)) * a;
}

// Normalize backward
// forward: out = a / length(a)
// backward: d_a = (grad_out - dot(grad_out, out) * out) / length(a)
__device__ __forceinline__ void normalize_bwd(const float2 &grad_output, const float2 &a,
                                              const float2 &normalized_a, float2 &grad_a) {
    float len = length(a);
    float dot_val = dot(grad_output, normalized_a);
    grad_a = (grad_output - dot_val * normalized_a) / (len + 1e-8f);
}

__device__ __forceinline__ void normalize_bwd(const float3 &grad_output, const float3 &a,
                                              const float3 &normalized_a, float3 &grad_a) {
    float len = length(a);
    float dot_val = dot(grad_output, normalized_a);
    grad_a = (grad_output - dot_val * normalized_a) / (len + 1e-8f);
}

// ============================================================================
// Backward Pass Helpers for Matrix Operations
// ============================================================================

// Matrix-vector multiplication backward: y = A * x
// Given: grad_y (gradient w.r.t. output y)
// Compute: grad_A, grad_x
__device__ __forceinline__ void mul_bwd(const float2x2 &A, const float2 &x, const float2 &grad_y,
                                        float2x2 &grad_A, float2 &grad_x) {
    // grad_x = A^T * grad_y
    grad_x = transpose(A) * grad_y;

    // grad_A = grad_y x x^T (outer product)
#pragma unroll
    for (int i = 0; i < 2; i++) {
        float grad_y_i = (i == 0) ? grad_y.x : grad_y.y;
#pragma unroll
        for (int j = 0; j < 2; j++) {
            float x_j = (j == 0) ? x.x : x.y;
            grad_A(i, j) = grad_y_i * x_j;
        }
    }
}

__device__ __forceinline__ void mul_bwd(const float3x3 &A, const float3 &x, const float3 &grad_y,
                                        float3x3 &grad_A, float3 &grad_x) {
    // grad_x = A^T * grad_y
    grad_x = transpose(A) * grad_y;

    // grad_A = grad_y x x^T (outer product)
#pragma unroll
    for (int i = 0; i < 3; i++) {
        float grad_y_i = (i == 0) ? grad_y.x : (i == 1) ? grad_y.y : grad_y.z;
#pragma unroll
        for (int j = 0; j < 3; j++) {
            float x_j = (j == 0) ? x.x : (j == 1) ? x.y : x.z;
            grad_A(i, j) = grad_y_i * x_j;
        }
    }
}

// Matrix-matrix multiplication backward: C = A * B
// Given: grad_C (gradient w.r.t. output C)
// Compute: grad_A, grad_B
__device__ __forceinline__ void mul_mat_bwd(const float2x2 &A, const float2x2 &B,
                                            const float2x2 &grad_C, float2x2 &grad_A,
                                            float2x2 &grad_B) {
    // grad_A = grad_C * B^T
    grad_A = grad_C * transpose(B);

    // grad_B = A^T * grad_C
    grad_B = transpose(A) * grad_C;
}

__device__ __forceinline__ void mul_mat_bwd(const float3x3 &A, const float3x3 &B,
                                            const float3x3 &grad_C, float3x3 &grad_A,
                                            float3x3 &grad_B) {
    // grad_A = grad_C * B^T
    grad_A = grad_C * transpose(B);

    // grad_B = A^T * grad_C
    grad_B = transpose(A) * grad_C;
}

// Transpose backward (transpose is self-adjoint)
// Given: grad_AT (gradient w.r.t. output A^T)
// Compute: grad_A
__device__ __forceinline__ float2x2 transpose_bwd(const float2x2 &grad_AT) {
    return transpose(grad_AT);
}

__device__ __forceinline__ float3x3 transpose_bwd(const float3x3 &grad_AT) {
    return transpose(grad_AT);
}

// ============================================================================
// Backward Pass Helpers for Scalar Operations
// ============================================================================

// Clamp backward (per-component)
// forward: out = clamp(x, min, max)
// backward: d_x = grad_out if min <= x <= max else 0
__device__ __forceinline__ void clamp_bwd(const float3 &grad_output, const float3 &x, float min_val,
                                          float max_val, float3 &grad_x) {
    grad_x.x = (x.x > min_val && x.x < max_val) ? grad_output.x : 0.0f;
    grad_x.y = (x.y > min_val && x.y < max_val) ? grad_output.y : 0.0f;
    grad_x.z = (x.z > min_val && x.z < max_val) ? grad_output.z : 0.0f;
}

// Lerp backward
// forward: out = a + (b - a) * t
// backward: d_a = grad_out * (1 - t), d_b = grad_out * t, d_t = grad_out *
// (b - a)
__device__ __forceinline__ void lerp_bwd(float grad_output, float a, float b, float t,
                                         float &grad_a, float &grad_b, float &grad_t) {
    grad_a = grad_output * (1.0f - t);
    grad_b = grad_output * t;
    grad_t = grad_output * (b - a);
}

__device__ __forceinline__ void lerp_bwd(const float3 &grad_output, const float3 &a,
                                         const float3 &b, float t, float3 &grad_a, float3 &grad_b,
                                         float &grad_t) {
    grad_a = grad_output * (1.0f - t);
    grad_b = grad_output * t;
    grad_t = dot(grad_output, b - a);
}

// Pow backward (element-wise)
// forward: out = pow(x, exp)
// backward: d_x = grad_out * exp * pow(x, exp - 1), d_exp = grad_out * out *
// log(x)
__device__ __forceinline__ void pow_bwd(float grad_output, float x, float exp, float output,
                                        float &grad_x, float &grad_exp) {
    grad_x = grad_output * exp * __powf(x, exp - 1.0f);
    grad_exp = grad_output * output * __logf(x + 1e-8f);
}

__device__ __forceinline__ void pow_bwd(const float3 &grad_output, const float3 &x, float exp,
                                        const float3 &output, float3 &grad_x, float &grad_exp) {
    grad_x.x = grad_output.x * exp * __powf(x.x, exp - 1.0f);
    grad_x.y = grad_output.y * exp * __powf(x.y, exp - 1.0f);
    grad_x.z = grad_output.z * exp * __powf(x.z, exp - 1.0f);

    grad_exp = dot(grad_output * output,
                   make_float3(__logf(x.x + 1e-8f), __logf(x.y + 1e-8f), __logf(x.z + 1e-8f)));
}

__device__ __forceinline__ void pow_bwd(const float3 &grad_output, const float3 &x,
                                        const float3 &exp, const float3 &output, float3 &grad_x,
                                        float3 &grad_exp) {
    grad_x.x = grad_output.x * exp.x * __powf(x.x, exp.x - 1.0f);
    grad_x.y = grad_output.y * exp.y * __powf(x.y, exp.y - 1.0f);
    grad_x.z = grad_output.z * exp.z * __powf(x.z, exp.z - 1.0f);

    grad_exp.x = grad_output.x * output.x * __logf(x.x + 1e-8f);
    grad_exp.y = grad_output.y * output.y * __logf(x.y + 1e-8f);
    grad_exp.z = grad_output.z * output.z * __logf(x.z + 1e-8f);
}

// ============================================================================
// Backward Pass Helpers for Matrix-Vector Operations
// ============================================================================

// NOTE: Old raw-array backward functions removed (mul_2x2_bwd, mul_3x3_bwd, mul_4x4_bwd).
// Use typed matrix functions above: mul_bwd(float2x2, ...) and mul_bwd(float3x3, ...)
// float4x4 operations are not used in the ISP pipeline.

// NOTE: Old raw-array matrix-matrix backward functions removed (mul_3x3_mat_bwd).
// Use typed matrix functions above: mul_mat_bwd(float2x2, ...) and mul_mat_bwd(float3x3, ...)

// ============================================================================
// Backward Pass Helpers for Scalar Functions
// ============================================================================

// exp backward: d_x = grad_out * exp(x) = grad_out * output
__device__ __forceinline__ void exp_bwd(float grad_output, float output, float &grad_x) {
    grad_x = grad_output * output;
}

// exp2 backward: d_x = grad_out * log(2) * exp2(x) = grad_out * log(2) *
// output
__device__ __forceinline__ void exp2_bwd(float grad_output, float output, float &grad_x) {
    grad_x = grad_output * 0.69314718f * output;  // log(2)
}

// log backward: d_x = grad_out / x
__device__ __forceinline__ void log_bwd(float grad_output, float x, float &grad_x) {
    grad_x = __fdividef(grad_output, x + 1e-8f);
}

// sqrt backward: d_x = grad_out / (2 * sqrt(x)) = grad_out / (2 * output)
__device__ __forceinline__ void sqrt_bwd(float grad_output, float output, float &grad_x) {
    grad_x = __fdividef(grad_output, 2.0f * output + 1e-8f);
}

// Division backward: out = a / b
// d_a = grad_out / b, d_b = -grad_out * a / (b * b)
__device__ __forceinline__ void div_bwd(float grad_output, float a, float b, float output,
                                        float &grad_a, float &grad_b) {
    grad_a = __fdividef(grad_output, b);
    grad_b = -grad_output * __fdividef(output, b);
}

// ============================================================================
// ISP-Specific Backward Helpers
// ============================================================================

// Softplus backward (for bounded positive parameters)
// forward: out = min_value + log(1 + exp(raw))
// backward: d_raw = grad_out * sigmoid(raw)
__device__ __forceinline__ void softplus_bwd(float grad_output, float raw, float &grad_raw) {
    float sigmoid = __fdividef(1.0f, 1.0f + __expf(-raw));
    grad_raw = grad_output * sigmoid;
}

// Sigmoid backward (for clamped parameters)
// forward: out = 1 / (1 + exp(-raw))
// backward: d_raw = grad_out * out * (1 - out)
__device__ __forceinline__ void sigmoid_bwd(float grad_output, float output, float &grad_raw) {
    grad_raw = grad_output * output * (1.0f - output);
}

// Vignetting falloff backward
// forward: falloff = 1 + alpha0*r2 + alpha1*r2^2 + alpha2*r2^3
// backward: d_r2 = grad_out * (alpha0 + 2*alpha1*r2 + 3*alpha2*r2^2)
//           d_alpha0 = grad_out * r2
//           d_alpha1 = grad_out * r2^2
//           d_alpha2 = grad_out * r2^3
__device__ __forceinline__ void vignetting_bwd(float grad_output, float r2, float alpha0,
                                               float alpha1, float alpha2, float &grad_r2,
                                               float &grad_alpha0, float &grad_alpha1,
                                               float &grad_alpha2) {
    float r4 = r2 * r2;
    float r6 = r4 * r2;

    grad_r2 = grad_output * __fmaf_rn(3.0f * alpha2, r4, __fmaf_rn(2.0f * alpha1, r2, alpha0));
    grad_alpha0 = grad_output * r2;
    grad_alpha1 = grad_output * r4;
    grad_alpha2 = grad_output * r6;
}

// Linear interpolation backward (for CRF)
// forward: out = y0 * (1 - w) + y1 * w
// backward: d_y0 = grad_out * (1 - w)
//           d_y1 = grad_out * w
//           d_w = grad_out * (y1 - y0)
__device__ __forceinline__ void lerp_interp_bwd(float grad_output, float y0, float y1, float w,
                                                float &grad_y0, float &grad_y1, float &grad_w) {
    grad_y0 = grad_output * (1.0f - w);
    grad_y1 = grad_output * w;
    grad_w = grad_output * (y1 - y0);
}

// ============================================================================
// Parameter Transformation Gradients
// ============================================================================

// Gradient of bounded_positive_forward: f(x) = min_val + log(1 + exp(x))
// This is a softplus function shifted by min_val
// Derivative: f'(x) = exp(x) / (1 + exp(x)) = sigmoid(x)
__device__ __forceinline__ float bounded_positive_backward(float raw, float min_value,
                                                           float grad_output) {
    float exp_x = __expf(raw);
    float sigmoid = __fdividef(exp_x, 1.0f + exp_x);
    return grad_output * sigmoid;
}

// Gradient of clamped_forward: f(x) = 1 / (1 + exp(-x)) = sigmoid(x)
// Derivative: f'(x) = sigmoid(x) * (1 - sigmoid(x))
__device__ __forceinline__ float clamped_backward(float raw, float grad_output) {
    float exp_neg_x = __expf(-raw);
    float sigmoid = __fdividef(1.0f, 1.0f + exp_neg_x);
    return grad_output * sigmoid * (1.0f - sigmoid);
}

// ============================================================================
// Modular ISP Component Backward Functions
// ============================================================================

// ----------------------------------------------------------------------------
// Exposure Compensation Backward
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_exposure_bwd(const float3 &rgb_in, float exposure_param,
                                                   const float3 &grad_rgb_out, float3 &grad_rgb_in,
                                                   float &grad_exposure_param) {
    // Forward: rgb_out = rgb_in * exp2(exposure_param)
    float exposure_factor = exp2f(exposure_param);

    // Gradient to exposure parameter
    // IMPORTANT: Compute this FIRST before modifying grad_rgb_in,
    // since grad_rgb_out and grad_rgb_in may alias the same memory!
    // d/d_exp[rgb_in * exp2(exp)] = rgb_in * exp2(exp) * ln(2) = rgb_out * ln(2)
    // grad_exposure = grad_rgb_out * (rgb_out * ln(2))
    float3 rgb_out = rgb_in * exposure_factor;
    grad_exposure_param = dot(grad_rgb_out, rgb_out) * 0.69314718f;  // ln(2)

    // Gradient to input
    grad_rgb_in = grad_rgb_out * exposure_factor;
}

// ----------------------------------------------------------------------------
// Vignetting Correction Backward
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_vignetting_bwd(
    const float3 &rgb_in, const VignettingChannelParams *vignetting_params,
    const float2 &pixel_coords, float resolution_x, float resolution_y, const float3 &grad_rgb_out,
    float3 &grad_rgb_in, VignettingChannelParams *grad_vignetting_params) {
    // Recompute forward
    float max_res = fmaxf(resolution_x, resolution_y);
    float2 uv = make_float2(__fdividef(pixel_coords.x - resolution_x * 0.5f, max_res),
                            __fdividef(pixel_coords.y - resolution_y * 0.5f, max_res));

    float rgb_arr[3] = {rgb_in.x, rgb_in.y, rgb_in.z};
    float grad_rgb_out_arr[3] = {grad_rgb_out.x, grad_rgb_out.y, grad_rgb_out.z};
    float grad_rgb_in_arr[3] = {0, 0, 0};

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
        float falloff_clamped = fmaxf(0.0f, fminf(1.0f, falloff));

        // Gradient to rgb_in
        grad_rgb_in_arr[i] = grad_rgb_out_arr[i] * falloff_clamped;

        // Gradient to falloff
        float grad_falloff = grad_rgb_out_arr[i] * rgb_arr[i];

        // Only compute gradients if not clamped
        // Use >= and <= to include boundary cases (falloff == 0.0 or falloff == 1.0)
        if (falloff >= 0.0f && falloff <= 1.0f) {
            // Gradient to alphas
            grad_vignetting_params[i].alpha0 += grad_falloff * r2;
            grad_vignetting_params[i].alpha1 += grad_falloff * r4;
            grad_vignetting_params[i].alpha2 += grad_falloff * r6;

            // Gradient to optical center through r2
            float grad_r2 =
                grad_falloff * __fmaf_rn(3.0f * params.alpha2, r4,
                                         __fmaf_rn(2.0f * params.alpha1, r2, params.alpha0));
            grad_vignetting_params[i].cx += -grad_r2 * 2.0f * dx;
            grad_vignetting_params[i].cy += -grad_r2 * 2.0f * dy;
        }
    }

    grad_rgb_in = make_float3(grad_rgb_in_arr[0], grad_rgb_in_arr[1], grad_rgb_in_arr[2]);
}

// ----------------------------------------------------------------------------
// Homography Backward - Helper Functions
// ----------------------------------------------------------------------------

// Backward for normalization: H_normalized = H / H[2][2]
// Given grad_H_normalized, compute grad_H_unnormalized
__device__ __forceinline__ void compute_homography_normalization_bwd(const float3x3 &H_unnorm,
                                                                     const float3x3 &grad_H_norm,
                                                                     float3x3 &grad_H_unnorm) {
    float s = H_unnorm(2, 2);

    if (fabsf(s) > 1.0e-20f) {
        float inv_s = 1.0f / s;
        float inv_s2 = inv_s * inv_s;

        // grad_H_unnorm[i] = grad_H_norm[i] * inv_s
        // grad_s = -sum(grad_H_norm[i] * H_unnorm[i] * inv_s^2)
        float grad_s = 0.0f;

#pragma unroll
        for (int i = 0; i < 9; i++) {
            grad_H_unnorm.m[i] = grad_H_norm.m[i] * inv_s;
            grad_s += -grad_H_norm.m[i] * H_unnorm.m[i] * inv_s2;
        }

        // Gradient flows back to H_unnorm[2][2]
        grad_H_unnorm(2, 2) += grad_s;
    } else {
        // No normalization occurred, gradient passes through unchanged
#pragma unroll
        for (int i = 0; i < 9; i++) {
            grad_H_unnorm.m[i] = grad_H_norm.m[i];
        }
    }
}

// Backward for diagonal matrix construction: D = diag(lambda)
// Given grad_D, compute grad_lambda
__device__ __forceinline__ void compute_homography_diagonal_matrix_bwd(const float3x3 &grad_D,
                                                                       float3 &grad_lambda) {
    grad_lambda.x = grad_D(0, 0);
    grad_lambda.y = grad_D(1, 1);
    grad_lambda.z = grad_D(2, 2);
}

// Backward for skew-symmetric matrix construction from t_gray
// skew = [[0, -t_gray.z, t_gray.y], [t_gray.z, 0, -t_gray.x], [-t_gray.y, t_gray.x, 0]]
__device__ __forceinline__ void compute_homography_skew_matrix_construction_bwd(
    const float3x3 &grad_skew, float3 &grad_t_gray) {
    grad_t_gray.x = 0.0f;
    grad_t_gray.y = 0.0f;
    grad_t_gray.z = 0.0f;

    // Skew matrix elements and their dependencies:
    // skew(0,1) = -t_gray.z
    grad_t_gray.z += -grad_skew(0, 1);
    // skew(0,2) = t_gray.y
    grad_t_gray.y += grad_skew(0, 2);
    // skew(1,0) = t_gray.z
    grad_t_gray.z += grad_skew(1, 0);
    // skew(1,2) = -t_gray.x
    grad_t_gray.x += -grad_skew(1, 2);
    // skew(2,0) = -t_gray.y
    grad_t_gray.y += -grad_skew(2, 0);
    // skew(2,1) = t_gray.x
    grad_t_gray.x += grad_skew(2, 1);
}

// Backward for T matrix construction from column vectors [t_b, t_r, t_g]
__device__ __forceinline__ void compute_homography_matrix_T_construction_bwd(const float3x3 &grad_T,
                                                                             float3 &grad_t_b,
                                                                             float3 &grad_t_r,
                                                                             float3 &grad_t_g) {
    // T is column-major: T(i,j) where j selects column (t_b=0, t_r=1, t_g=2)
    grad_t_b.x = grad_T(0, 0);
    grad_t_b.y = grad_T(1, 0);
    grad_t_b.z = grad_T(2, 0);

    grad_t_r.x = grad_T(0, 1);
    grad_t_r.y = grad_T(1, 1);
    grad_t_r.z = grad_T(2, 1);

    grad_t_g.x = grad_T(0, 2);
    grad_t_g.y = grad_T(1, 2);
    grad_t_g.z = grad_T(2, 2);
}

// Backward for target point construction: t = base + make_float3(offset.x, offset.y, 0)
__device__ __forceinline__ void compute_homography_target_point_bwd(const float3 &grad_t,
                                                                    float2 &grad_offset) {
    grad_offset.x = grad_t.x;
    grad_offset.y = grad_t.y;
    // grad_t.z doesn't contribute to offset (z is constant 1.0)
}

// Backward for nullspace computation via cross products with conditionals
// This handles the conditional logic in computing lambda
__device__ __forceinline__ void compute_homography_nullspace_computation_bwd(
    const float3x3 &M, const float3 &lambda_v, const float3x3 &grad_M_in, float3x3 &grad_M) {
    // Extract rows
    float3 r0 = make_float3(M(0, 0), M(0, 1), M(0, 2));
    float3 r1 = make_float3(M(1, 0), M(1, 1), M(1, 2));
    float3 r2 = make_float3(M(2, 0), M(2, 1), M(2, 2));

    // Recompute forward conditional path
    float3 lambda_test = cross(r0, r1);
    float n2 = dot(lambda_test, lambda_test);

    float3 grad_lambda = make_float3(0.0f, 0.0f, 0.0f);

    // Accumulate gradient to lambda from grad_M_in (which comes from diagonal matrix)
    // The gradient comes through the diagonal matrix D
    grad_lambda.x = grad_M_in(0, 0);
    grad_lambda.y = grad_M_in(1, 1);
    grad_lambda.z = grad_M_in(2, 2);

    float3 grad_r0 = make_float3(0.0f, 0.0f, 0.0f);
    float3 grad_r1 = make_float3(0.0f, 0.0f, 0.0f);
    float3 grad_r2 = make_float3(0.0f, 0.0f, 0.0f);

    // Determine which branch was taken and backprop accordingly
    if (n2 < 1.0e-20f) {
        lambda_test = cross(r0, r2);
        n2 = dot(lambda_test, lambda_test);
        if (n2 < 1.0e-20f) {
            // lambda_v = cross(r1, r2)
            cross_bwd(grad_lambda, r1, r2, grad_r1, grad_r2);
        } else {
            // lambda_v = cross(r0, r2)
            cross_bwd(grad_lambda, r0, r2, grad_r0, grad_r2);
        }
    } else {
        // lambda_v = cross(r0, r1)
        cross_bwd(grad_lambda, r0, r1, grad_r0, grad_r1);
    }

    // Map row gradients back to matrix M
    grad_M = make_float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    grad_M(0, 0) = grad_r0.x;
    grad_M(0, 1) = grad_r0.y;
    grad_M(0, 2) = grad_r0.z;

    grad_M(1, 0) = grad_r1.x;
    grad_M(1, 1) = grad_r1.y;
    grad_M(1, 2) = grad_r1.z;

    grad_M(2, 0) = grad_r2.x;
    grad_M(2, 1) = grad_r2.y;
    grad_M(2, 2) = grad_r2.z;
}

// ----------------------------------------------------------------------------
// Homography Backward - Main Function
// ----------------------------------------------------------------------------
/**
 * @brief Backward pass for compute_homography
 *
 * This function computes the gradient of color_params given the gradient of H.
 * The forward pass is decomposed into subfunctions which are independently differentiated.
 */
__device__ __forceinline__ void compute_homography_bwd(const ColorPPISPParams *color_params,
                                                       const float3x3 &grad_H,
                                                       ColorPPISPParams *grad_color_params) {
    // ========================================================================
    // Step 1: Recompute forward pass to get all intermediates
    // ========================================================================

    const float2 &b_lat = color_params->b;
    const float2 &r_lat = color_params->r;
    const float2 &g_lat = color_params->g;
    const float2 &n_lat = color_params->n;

    // Map latent to real offsets via ZCA 2x2 blocks
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

    float3 t_b = make_float3(0.0f + bd.x, 0.0f + bd.y, 1.0f);
    float3 t_r = make_float3(1.0f + rd.x, 0.0f + rd.y, 1.0f);
    float3 t_g = make_float3(0.0f + gd.x, 1.0f + gd.y, 1.0f);
    float3 t_gray = make_float3(1.0f / 3.0f + nd.x, 1.0f / 3.0f + nd.y, 1.0f);

    float3x3 T = make_float3x3(t_b.x, t_r.x, t_g.x, t_b.y, t_r.y, t_g.y, t_b.z, t_r.z, t_g.z);

    float3x3 skew = make_float3x3(0.0f, -t_gray.z, t_gray.y, t_gray.z, 0.0f, -t_gray.x, -t_gray.y,
                                  t_gray.x, 0.0f);

    float3x3 M = skew * T;

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

    float3x3 S_inv = make_float3x3(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    float3x3 D =
        make_float3x3(lambda_v.x, 0.0f, 0.0f, 0.0f, lambda_v.y, 0.0f, 0.0f, 0.0f, lambda_v.z);

    float3x3 TD = T * D;
    float3x3 H_unnorm = TD * S_inv;

    // ========================================================================
    // Step 2: Backward pass through operations in reverse order
    // ========================================================================

    // Initialize gradient accumulators
    float3x3 grad_H_unnorm = make_float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // 2.1: Backprop through normalization
    compute_homography_normalization_bwd(H_unnorm, grad_H, grad_H_unnorm);

    // 2.2: Backprop through H = TD * S_inv
    // grad_TD = grad_H_unnorm * S_inv^T (since S_inv is constant)
    float3x3 grad_TD, grad_S_inv_unused;
    mul_mat_bwd(TD, S_inv, grad_H_unnorm, grad_TD, grad_S_inv_unused);

    // 2.3: Backprop through TD = T * D
    float3x3 grad_T, grad_D;
    mul_mat_bwd(T, D, grad_TD, grad_T, grad_D);

    // 2.4: Backprop through D = diag(lambda)
    float3 grad_lambda;
    compute_homography_diagonal_matrix_bwd(grad_D, grad_lambda);

    // 2.5: Backprop through lambda computation (nullspace via cross products)
    // Pass grad_lambda through a wrapper that looks like grad_D for the interface
    float3x3 grad_D_for_nullspace = make_float3x3(grad_lambda.x, 0.0f, 0.0f, 0.0f, grad_lambda.y,
                                                  0.0f, 0.0f, 0.0f, grad_lambda.z);
    float3x3 grad_M;
    compute_homography_nullspace_computation_bwd(M, lambda_v, grad_D_for_nullspace, grad_M);

    // 2.6: Backprop through M = skew * T
    float3x3 grad_skew, grad_T_from_M;
    mul_mat_bwd(skew, T, grad_M, grad_skew, grad_T_from_M);

    // Accumulate gradient to T
#pragma unroll
    for (int i = 0; i < 9; i++) {
        grad_T.m[i] += grad_T_from_M.m[i];
    }

    // 2.7: Backprop through skew matrix construction
    float3 grad_t_gray;
    compute_homography_skew_matrix_construction_bwd(grad_skew, grad_t_gray);

    // 2.8: Backprop through T matrix construction
    float3 grad_t_b, grad_t_r, grad_t_g;
    compute_homography_matrix_T_construction_bwd(grad_T, grad_t_b, grad_t_r, grad_t_g);

    // 2.9: Backprop through target point construction
    float2 grad_bd, grad_rd, grad_gd, grad_nd;
    compute_homography_target_point_bwd(grad_t_b, grad_bd);
    compute_homography_target_point_bwd(grad_t_r, grad_rd);
    compute_homography_target_point_bwd(grad_t_g, grad_gd);
    compute_homography_target_point_bwd(grad_t_gray, grad_nd);

    // 2.10: Backprop through ZCA transforms: offset = zca * latent
    float2x2 dummy_grad_zca;
    float2 grad_b_lat, grad_r_lat, grad_g_lat, grad_n_lat;

    mul_bwd(zca_b, b_lat, grad_bd, dummy_grad_zca, grad_b_lat);
    mul_bwd(zca_r, r_lat, grad_rd, dummy_grad_zca, grad_r_lat);
    mul_bwd(zca_g, g_lat, grad_gd, dummy_grad_zca, grad_g_lat);
    mul_bwd(zca_n, n_lat, grad_nd, dummy_grad_zca, grad_n_lat);

    // 2.11: Accumulate gradients to output
    grad_color_params->b.x += grad_b_lat.x;
    grad_color_params->b.y += grad_b_lat.y;
    grad_color_params->r.x += grad_r_lat.x;
    grad_color_params->r.y += grad_r_lat.y;
    grad_color_params->g.x += grad_g_lat.x;
    grad_color_params->g.y += grad_g_lat.y;
    grad_color_params->n.x += grad_n_lat.x;
    grad_color_params->n.y += grad_n_lat.y;
}

// ----------------------------------------------------------------------------
// Color Correction - PPISP Backward
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_color_correction_ppisp_bwd(
    const float3 &rgb_in, const ColorPPISPParams *color_params, const float3 &grad_rgb_out,
    float3 &grad_rgb_in, ColorPPISPParams *grad_color_params) {
    // Recompute forward
    float3x3 H = compute_homography(color_params);

    float intensity = rgb_in.x + rgb_in.y + rgb_in.z;
    float3 rgi_in = make_float3(rgb_in.x, rgb_in.y, intensity);

    float3 rgi_out = H * rgi_in;

    float floor_z = kColorNormMinZ * fabsf(intensity) + 1.0e-8f;
    float z = fmaxf(rgi_out.z, floor_z);
    float norm_factor = __fdividef(intensity, z);

    // Backward: rgb = [x * n, y * n, intensity - x * n - y * n]
    float grad_out_r = grad_rgb_out.x - grad_rgb_out.z;
    float grad_out_g = grad_rgb_out.y - grad_rgb_out.z;
    float3 grad_rgi_out =
        make_float3(norm_factor * grad_out_r, norm_factor * grad_out_g, 0.0f);
    float grad_norm_factor = rgi_out.x * grad_out_r + rgi_out.y * grad_out_g;

    // n = intensity / max(rgi_out.z, floor_z). The tie splits the incoming
    // gradient in half, and d|intensity| is 0 at 0, both matching Slang's
    // vjps -- the two backends must agree at the seam, not just away from it.
    float grad_z = -grad_norm_factor * norm_factor / z;
    float w_z = rgi_out.z > floor_z ? 1.0f : (rgi_out.z < floor_z ? 0.0f : 0.5f);
    grad_rgi_out.z = w_z * grad_z;
    float sign_i = intensity > 0.0f ? 1.0f : (intensity < 0.0f ? -1.0f : 0.0f);
    float grad_intensity = grad_rgb_out.z + grad_norm_factor / z +
                           (1.0f - w_z) * grad_z * kColorNormMinZ * sign_i;

    // Gradient through homography: rgi_out = H * rgi_in
    // Use typed matrix backward: grad_rgi_in = H^T * grad_rgi_out
    float3x3 grad_H;
    float3 grad_rgi_in;
    mul_bwd(H, rgi_in, grad_rgi_out, grad_H, grad_rgi_in);

    // Gradient through RGI construction: rgi_in = [r, g, r+g+b]
    grad_intensity += grad_rgi_in.z;
    grad_rgb_in.x = grad_rgi_in.x + grad_intensity;
    grad_rgb_in.y = grad_rgi_in.y + grad_intensity;
    grad_rgb_in.z = grad_intensity;

    // Backprop through compute_homography to get gradients for color_params
    compute_homography_bwd(color_params, grad_H, grad_color_params);
}

// ----------------------------------------------------------------------------
// CRF - PPISP Backward
// ----------------------------------------------------------------------------
__device__ __forceinline__ void apply_crf_ppisp_bwd(const float3 &rgb_in,
                                                    const CRFPPISPChannelParams *crf_params,
                                                    const float3 &grad_rgb_out, float3 &grad_rgb_in,
                                                    CRFPPISPChannelParams *grad_crf_params) {
    float3 rgb_clamped = clamp(rgb_in, 0.0f, 1.0f);
    float rgb_arr[3] = {rgb_clamped.x, rgb_clamped.y, rgb_clamped.z};
    float grad_out_arr[3] = {grad_rgb_out.x, grad_rgb_out.y, grad_rgb_out.z};
    float grad_in_arr[3] = {0, 0, 0};

#pragma unroll
    for (int i = 0; i < 3; i++) {
        const CRFPPISPChannelParams &params = crf_params[i];

        // Recompute forward
        float toe = bounded_positive_forward(params.toe, 0.3f);
        float shoulder = bounded_positive_forward(params.shoulder, 0.3f);
        float gamma = bounded_positive_forward(params.gamma, 0.1f);
        float center = clamped_forward(params.center);

        float lerp_val = __fmaf_rn(shoulder - toe, center, toe);
        float a = __fdividef(shoulder * center, lerp_val);
        float b = 1.0f - a;

        float x = rgb_arr[i];
        float y;

        if (x <= center) {
            y = a * __powf(__fdividef(x, center), toe);
        } else {
            y = 1.0f - b * __powf(__fdividef(1.0f - x, 1.0f - center), shoulder);
        }

        float output = __powf(fmaxf(0.0f, y), gamma);
        float y_clamped = fmaxf(0.0f, y);

        // Backward through gamma
        float grad_y = 0.0f;
        if (y_clamped > 0.0f) {
            grad_y = grad_out_arr[i] * gamma * __powf(y_clamped, gamma - 1.0f);
        }

        // Backward through piecewise curve
        float grad_x = 0.0f;
        if (x <= center && center > 0.0f) {
            float base = __fdividef(x, center);
            if (base > 0.0f) {
                grad_x = grad_y * a * toe * __powf(base, toe - 1.0f) / center;
            }
        } else if (x > center && center < 1.0f) {
            float base = __fdividef(1.0f - x, 1.0f - center);
            if (base > 0.0f) {
                grad_x = grad_y * b * shoulder * __powf(base, shoulder - 1.0f) / (1.0f - center);
            }
        }

        grad_in_arr[i] = grad_x;

        // Parameter gradients through transformations
        // We need to compute: grad_raw_param = grad_transformed_param *
        // d_transformed/d_raw

        // First, compute gradients to the transformed parameters (toe, shoulder,
        // gamma, center)
        float grad_toe = 0.0f;
        float grad_shoulder = 0.0f;
        float grad_gamma = 0.0f;
        float grad_center = 0.0f;

        // Gradient to gamma from output = pow(y_clamped, gamma)
        if (y_clamped > 0.0f) {
            grad_gamma = grad_out_arr[i] * output * __logf(y_clamped + 1e-8f);
        }

        // Gradients to toe, shoulder, center through the piecewise curve
        // These are complex, so we'll compute them numerically stable

        // For toe and shoulder, they affect the curve through a, b, and the power
        // terms For center, it affects both the conditional and the curve shape

        // Gradient to 'a' and 'b' from the curve
        float grad_a = 0.0f;
        float grad_b = 0.0f;

        if (x <= center && center > 0.0f) {
            // y = a * (x/center)^toe
            float base = __fdividef(x, center);
            if (base > 0.0f) {
                float powered = __powf(base, toe);
                grad_a += grad_y * powered;

                // grad_toe from the power term
                grad_toe += grad_y * a * powered * __logf(base + 1e-8f);

                // grad_center from the base (x/center)
                // dy/dcenter = dy/dbase * dbase/dcenter
                // dy/dbase = grad_y * a * toe * base^(toe-1)
                // dbase/dcenter = d(x/center)/dcenter = -x / center^2
                float grad_base = grad_y * a * toe * __powf(base, toe - 1.0f);
                grad_center += grad_base * (-x / (center * center));
            }
        } else if (x > center && center < 1.0f) {
            // y = 1 - b * ((1-x)/(1-center))^shoulder
            float base = __fdividef(1.0f - x, 1.0f - center);
            if (base > 0.0f) {
                float powered = __powf(base, shoulder);
                grad_b += -grad_y * powered;

                // grad_shoulder from the power term
                grad_shoulder += -grad_y * b * powered * __logf(base + 1e-8f);

                // grad_center from the base ((1-x)/(1-center))
                // dy/dcenter = dy/dbase * dbase/dcenter
                // dy/dbase = grad_y * (-b * shoulder * base^(shoulder-1))
                // dbase/dcenter = d((1-x)/(1-center))/dcenter = (1-x) / (1-center)^2
                float grad_base = grad_y * (-b * shoulder * __powf(base, shoulder - 1.0f));
                float dbase_dcenter = (1.0f - x) / ((1.0f - center) * (1.0f - center));
                grad_center += grad_base * dbase_dcenter;
            }
        }

        // Gradient to toe, shoulder through a and b
        // a = (shoulder * center) / lerp_val, where lerp_val = (shoulder - toe) *
        // center + toe b = 1 - a

        // NOTE: In Slang, the computation is structured as:
        //   1. Vectorially compute: float3 a = (shoulders * centers) / lerp(toes, shoulders,
        //   centers)
        //   2. Per-channel loop: float c = centers[i]; ... use c in divisions
        // The key is that 'centers' is used in BOTH places, but Slang's autodiff
        // only computes gradients through the VECTORIAL use (#1), not through
        // the per-channel extraction (#2).
        //
        // In CUDA, we're computing everything per-channel, so we need to ensure
        // gradients ONLY flow through the 'a' and 'b' computation, NOT through
        // the per-channel divisions (x/c, (1-x)/(1-c)) which we already removed.

        // CRITICAL: Since b = 1 - a, we have db/da = -1
        // So grad_a needs to accumulate -grad_b BEFORE we use it
        grad_a += -grad_b;

        float grad_lerp_val = 0.0f;
        if (fabsf(lerp_val) > 1e-8f) {
            // grad_a (now including contribution from grad_b) contributes to shoulder,
            // center, and lerp_val
            float a_over_lerp = __fdividef(shoulder * center, lerp_val);
            grad_shoulder += grad_a * center / lerp_val;
            grad_center += grad_a * shoulder / lerp_val;
            grad_lerp_val += -grad_a * a_over_lerp / lerp_val;
        }

        // Gradient through lerp_val = (shoulder - toe) * center + toe
        grad_shoulder += grad_lerp_val * center;
        grad_toe += grad_lerp_val * (1.0f - center);
        grad_center += grad_lerp_val * (shoulder - toe);

        // Now transform gradients back through the parameter transformations
        grad_crf_params[i].toe += bounded_positive_backward(params.toe, 0.3f, grad_toe);
        grad_crf_params[i].shoulder +=
            bounded_positive_backward(params.shoulder, 0.3f, grad_shoulder);
        grad_crf_params[i].gamma += bounded_positive_backward(params.gamma, 0.1f, grad_gamma);
        grad_crf_params[i].center += clamped_backward(params.center, grad_center);
    }

    grad_rgb_in = make_float3(grad_in_arr[0], grad_in_arr[1], grad_in_arr[2]);
}
}  // namespace bilagrid_ppisp
