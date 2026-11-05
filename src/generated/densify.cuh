#pragma once

#include "generated/slang.cuh"

inline __device__ bool splat_scale_is_dead(float3  log_scale_0)
{
    return !((F32_max(((F32_max((log_scale_0.x), (log_scale_0.y)))), (log_scale_0.z))) > -40.0f);
}

inline __device__ float3  densify_normal(uint4  key_0)
{
    uint4  _S1 = key_0 >> make_uint4 (int(8));
    float4  _S2 = make_float4 ((float)_S1.x, (float)_S1.y, (float)_S1.z, (float)_S1.w);
    float4  u_0 = _S2 * make_float4 (5.9604644775390625e-08f);
    float2  _S3 = float2 {u_0.x, u_0.z};
    float _S4 = 6.28318548202514648f * _S3.y;
    float2  _S5 = float2 {u_0.y, u_0.w};
    float _S6 = 6.28318548202514648f * _S5.y;
    return make_float3 ((make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S3.x))))))) * make_float2 ((F32_cos((_S4))), (F32_sin((_S4))))).x, (make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S3.x))))))) * make_float2 ((F32_cos((_S4))), (F32_sin((_S4))))).y, (make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S5.x))))))) * make_float2 ((F32_cos((_S6))), (F32_sin((_S6))))).x);
}

inline __device__ float dot_0(float4  x_0, float4  y_0)
{
    int i_0 = int(0);
    float result_0 = 0.0f;
    for(;;)
    {
        if(i_0 < int(4))
        {
        }
        else
        {
            break;
        }
        float result_1 = result_0 + _slang_vector_get_element(x_0, i_0) * _slang_vector_get_element(y_0, i_0);
        i_0 = i_0 + int(1);
        result_0 = result_1;
    }
    return result_0;
}

inline __device__ float length_0(float4  x_1)
{
    return (F32_sqrt((dot_0(x_1, x_1))));
}

inline __device__ float4  normalize_0(float4  x_2)
{
    return x_2 / make_float4 (length_0(x_2));
}

inline __device__ float3  exp_0(float3  x_3)
{
    float3  result_2;
    int i_1 = int(0);
    for(;;)
    {
        if(i_1 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_2, i_1) = (F32_exp((_slang_vector_get_element(x_3, i_1))));
        i_1 = i_1 + int(1);
    }
    return result_2;
}

inline __device__ Matrix<float, 3, 3>  transpose_0(Matrix<float, 3, 3>  x_4)
{
    Matrix<float, 3, 3>  result_3;
    int r_0 = int(0);
    for(;;)
    {
        if(r_0 < int(3))
        {
        }
        else
        {
            break;
        }
        int c_0 = int(0);
        for(;;)
        {
            if(c_0 < int(3))
            {
            }
            else
            {
                break;
            }
            *_slang_vector_get_element_ptr(((&result_3)->rows + (r_0)), c_0) = _slang_vector_get_element(x_4.rows[c_0], r_0);
            c_0 = c_0 + int(1);
        }
        r_0 = r_0 + int(1);
    }
    return result_3;
}

inline __device__ Matrix<float, 3, 3>  mul_0(Matrix<float, 3, 3>  left_0, Matrix<float, 3, 3>  right_0)
{
    Matrix<float, 3, 3>  result_4;
    int r_1 = int(0);
    for(;;)
    {
        if(r_1 < int(3))
        {
        }
        else
        {
            break;
        }
        int c_1 = int(0);
        for(;;)
        {
            if(c_1 < int(3))
            {
            }
            else
            {
                break;
            }
            int i_2 = int(0);
            float sum_0 = 0.0f;
            for(;;)
            {
                if(i_2 < int(3))
                {
                }
                else
                {
                    break;
                }
                float sum_1 = sum_0 + _slang_vector_get_element(left_0.rows[r_1], i_2) * _slang_vector_get_element(right_0.rows[i_2], c_1);
                i_2 = i_2 + int(1);
                sum_0 = sum_1;
            }
            *_slang_vector_get_element_ptr(((&result_4)->rows + (r_1)), c_1) = sum_0;
            c_1 = c_1 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_4;
}

inline __device__ float3  mul_1(Matrix<float, 3, 3>  left_1, float3  right_1)
{
    float3  result_5;
    int i_3 = int(0);
    for(;;)
    {
        if(i_3 < int(3))
        {
        }
        else
        {
            break;
        }
        int j_0 = int(0);
        float sum_2 = 0.0f;
        for(;;)
        {
            if(j_0 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_3 = sum_2 + _slang_vector_get_element(left_1.rows[i_3], j_0) * _slang_vector_get_element(right_1, j_0);
            j_0 = j_0 + int(1);
            sum_2 = sum_3;
        }
        *_slang_vector_get_element_ptr(&result_5, i_3) = sum_2;
        i_3 = i_3 + int(1);
    }
    return result_5;
}

inline __device__ void mcmc_add_noise_3dgs(float scaler_0, float3  * mean_0, float3  log_scale_1, float4  quat_0, float logit_opac_0, uint4  noise_key_0)
{
    float4  _S7 = normalize_0(quat_0);
    float3  _S8 = exp_0(log_scale_1);
    float x_5 = _S7.y;
    float x2_0 = x_5 * x_5;
    float y2_0 = _S7.z * _S7.z;
    float z2_0 = _S7.w * _S7.w;
    float xy_0 = _S7.y * _S7.z;
    float xz_0 = _S7.y * _S7.w;
    float yz_0 = _S7.z * _S7.w;
    float wx_0 = _S7.x * _S7.y;
    float wy_0 = _S7.x * _S7.z;
    float wz_0 = _S7.x * _S7.w;
    Matrix<float, 3, 3>  M_0 = mul_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_0 + z2_0), 2.0f * (xy_0 + wz_0), 2.0f * (xz_0 - wy_0), 2.0f * (xy_0 - wz_0), 1.0f - 2.0f * (x2_0 + z2_0), 2.0f * (yz_0 + wx_0), 2.0f * (xz_0 + wy_0), 2.0f * (yz_0 - wx_0), 1.0f - 2.0f * (x2_0 + y2_0))), makeMatrix<float, 3, 3> (_S8.x, 0.0f, 0.0f, 0.0f, _S8.y, 0.0f, 0.0f, 0.0f, _S8.z));
    *mean_0 = *mean_0 + mul_1(mul_0(M_0, transpose_0(M_0)), densify_normal(noise_key_0) * make_float3 (scaler_0) * make_float3 (1.0f / (1.0f + (F32_exp((-100.0f * (1.0f - 1.0f / (1.0f + (F32_exp((- logit_opac_0)))) - 0.99500000476837158f)))))));
    return;
}

inline __device__ float3  sqrt_0(float3  x_6)
{
    float3  result_6;
    int i_4 = int(0);
    for(;;)
    {
        if(i_4 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_6, i_4) = (F32_sqrt((_slang_vector_get_element(x_6, i_4))));
        i_4 = i_4 + int(1);
    }
    return result_6;
}

inline __device__ void revised_add_noise_3dgs(float scaler_1, float radii_0, float3  * mean_1, float3  log_scale_2, float4  quat_1, float logit_opac_1, float gate_opac_0, uint4  noise_key_1)
{
    if(radii_0 <= 0.0f)
    {
        return;
    }
    float opac_0 = 1.0f / (1.0f + (F32_exp((- logit_opac_1))));
    float4  _S9 = normalize_0(quat_1);
    float3  _S10 = sqrt_0(exp_0(log_scale_2));
    float x_7 = _S9.y;
    float x2_1 = x_7 * x_7;
    float y2_1 = _S9.z * _S9.z;
    float z2_1 = _S9.w * _S9.w;
    float xy_1 = _S9.y * _S9.z;
    float xz_1 = _S9.y * _S9.w;
    float yz_1 = _S9.z * _S9.w;
    float wx_1 = _S9.x * _S9.y;
    float wy_1 = _S9.x * _S9.z;
    float wz_1 = _S9.x * _S9.w;
    Matrix<float, 3, 3>  M_1 = mul_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_1 + z2_1), 2.0f * (xy_1 + wz_1), 2.0f * (xz_1 - wy_1), 2.0f * (xy_1 - wz_1), 1.0f - 2.0f * (x2_1 + z2_1), 2.0f * (yz_1 + wx_1), 2.0f * (xz_1 + wy_1), 2.0f * (yz_1 - wx_1), 1.0f - 2.0f * (x2_1 + y2_1))), makeMatrix<float, 3, 3> (_S10.x, 0.0f, 0.0f, 0.0f, _S10.y, 0.0f, 0.0f, 0.0f, _S10.z));
    *mean_1 = *mean_1 + mul_1(mul_0(M_1, transpose_0(M_1)), make_float3 ((F32_min((0.05000000074505806f * scaler_1 * (F32_sqrt((2.0f * (F32_log(((F32_max((255.0f * opac_0), (1.0f))))))))) * (F32_pow((1.0f - (F32_min((opac_0), (gate_opac_0)))), (150.0f)))), (1.0f)))) * densify_normal(noise_key_1));
    return;
}

inline __device__ void long_axis_split_3dgs(float k_0, float3  log_scale_3, float logit_opacity_0, float4  quat_2, float3  * new_log_scale_0, float * new_logit_opacity_0, float3  * mean_delta_0)
{
    float _S11 = log_scale_3.x;
    float _S12 = log_scale_3.y;
    float _S13 = log_scale_3.z;
    float d_0 = 0.5f * (F32_exp(((F32_max(((F32_max((_S11), (_S12)))), (_S13))))));
    *new_log_scale_0 = log_scale_3;
    *mean_delta_0 = make_float3 (0.0f, 0.0f, 0.0f);
    float kl_0 = (F32_log((0.5f)));
    float ks_0 = (F32_log((0.85000002384185791f)));
    bool _S14;
    if(_S11 > _S12)
    {
        _S14 = _S11 > _S13;
    }
    else
    {
        _S14 = false;
    }
    if(_S14)
    {
        *new_log_scale_0 = *new_log_scale_0 + make_float3 (kl_0, ks_0, ks_0);
        *&(mean_delta_0->x) = d_0;
    }
    else
    {
        if(_S12 > _S13)
        {
            *new_log_scale_0 = *new_log_scale_0 + make_float3 (ks_0, kl_0, ks_0);
            *&(mean_delta_0->y) = d_0;
        }
        else
        {
            *new_log_scale_0 = *new_log_scale_0 + make_float3 (ks_0, ks_0, kl_0);
            *&(mean_delta_0->z) = d_0;
        }
    }
    float4  _S15 = normalize_0(quat_2);
    float x_8 = _S15.y;
    float x2_2 = x_8 * x_8;
    float y2_2 = _S15.z * _S15.z;
    float z2_2 = _S15.w * _S15.w;
    float xy_2 = _S15.y * _S15.z;
    float xz_2 = _S15.y * _S15.w;
    float yz_2 = _S15.z * _S15.w;
    float wx_2 = _S15.x * _S15.y;
    float wy_2 = _S15.x * _S15.z;
    float wz_2 = _S15.x * _S15.w;
    *mean_delta_0 = mul_1(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_2 + z2_2), 2.0f * (xy_2 + wz_2), 2.0f * (xz_2 - wy_2), 2.0f * (xy_2 - wz_2), 1.0f - 2.0f * (x2_2 + z2_2), 2.0f * (yz_2 + wx_2), 2.0f * (xz_2 + wy_2), 2.0f * (yz_2 - wx_2), 1.0f - 2.0f * (x2_2 + y2_2))), *mean_delta_0);
    *new_logit_opacity_0 = (F32_log((k_0 / (1.0f + (F32_exp((- logit_opacity_0))) - k_0))));
    return;
}

