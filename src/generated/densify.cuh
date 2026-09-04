#pragma once

#include "generated/slang.cuh"

inline __device__ bool splat_scale_is_dead(float3  log_scale_0)
{
    return !((F32_max(((F32_max((log_scale_0.x), (log_scale_0.y)))), (log_scale_0.z))) > -40.0f);
}

inline __device__ float dot_0(float3  x_0, float3  y_0)
{
    int i_0 = int(0);
    float result_0 = 0.0f;
    for(;;)
    {
        if(i_0 < int(3))
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

inline __device__ float dot_1(float4  x_1, float4  y_1)
{
    int i_1 = int(0);
    float result_2 = 0.0f;
    for(;;)
    {
        if(i_1 < int(4))
        {
        }
        else
        {
            break;
        }
        float result_3 = result_2 + _slang_vector_get_element(x_1, i_1) * _slang_vector_get_element(y_1, i_1);
        i_1 = i_1 + int(1);
        result_2 = result_3;
    }
    return result_2;
}

inline __device__ float length_0(float4  x_2)
{
    return (F32_sqrt((dot_1(x_2, x_2))));
}

inline __device__ float4  normalize_0(float4  x_3)
{
    return x_3 / make_float4 (length_0(x_3));
}

inline __device__ float3  exp_0(float3  x_4)
{
    float3  result_4;
    int i_2 = int(0);
    for(;;)
    {
        if(i_2 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_4, i_2) = (F32_exp((_slang_vector_get_element(x_4, i_2))));
        i_2 = i_2 + int(1);
    }
    return result_4;
}

inline __device__ Matrix<float, 3, 3>  transpose_0(Matrix<float, 3, 3>  x_5)
{
    Matrix<float, 3, 3>  result_5;
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
            *_slang_vector_get_element_ptr(((&result_5)->rows + (r_0)), c_0) = _slang_vector_get_element(x_5.rows[c_0], r_0);
            c_0 = c_0 + int(1);
        }
        r_0 = r_0 + int(1);
    }
    return result_5;
}

inline __device__ Matrix<float, 3, 3>  mul_0(Matrix<float, 3, 3>  left_0, Matrix<float, 3, 3>  right_0)
{
    Matrix<float, 3, 3>  result_6;
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
            int i_3 = int(0);
            float sum_0 = 0.0f;
            for(;;)
            {
                if(i_3 < int(3))
                {
                }
                else
                {
                    break;
                }
                float sum_1 = sum_0 + _slang_vector_get_element(left_0.rows[r_1], i_3) * _slang_vector_get_element(right_0.rows[i_3], c_1);
                i_3 = i_3 + int(1);
                sum_0 = sum_1;
            }
            *_slang_vector_get_element_ptr(((&result_6)->rows + (r_1)), c_1) = sum_0;
            c_1 = c_1 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_6;
}

inline __device__ float4  floor_0(float4  x_6)
{
    float4  result_7;
    int i_4 = int(0);
    for(;;)
    {
        if(i_4 < int(4))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_7, i_4) = (F32_floor((_slang_vector_get_element(x_6, i_4))));
        i_4 = i_4 + int(1);
    }
    return result_7;
}

inline __device__ float3  mul_1(Matrix<float, 3, 3>  left_1, float3  right_1)
{
    float3  result_8;
    int i_5 = int(0);
    for(;;)
    {
        if(i_5 < int(3))
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
            float sum_3 = sum_2 + _slang_vector_get_element(left_1.rows[i_5], j_0) * _slang_vector_get_element(right_1, j_0);
            j_0 = j_0 + int(1);
            sum_2 = sum_3;
        }
        *_slang_vector_get_element_ptr(&result_8, i_5) = sum_2;
        i_5 = i_5 + int(1);
    }
    return result_8;
}

inline __device__ void mcmc_add_noise_3dgs(float scaler_0, float3  * mean_0, float3  log_scale_1, float4  quat_0, float logit_opac_0)
{
    float opac_0 = 1.0f / (1.0f + (F32_exp((- logit_opac_0))));
    float4  _S1 = normalize_0(quat_0);
    float3  _S2 = exp_0(log_scale_1);
    float x_7 = _S1.y;
    float x2_0 = x_7 * x_7;
    float y2_0 = _S1.z * _S1.z;
    float z2_0 = _S1.w * _S1.w;
    float xy_0 = _S1.y * _S1.z;
    float xz_0 = _S1.y * _S1.w;
    float yz_0 = _S1.z * _S1.w;
    float wx_0 = _S1.x * _S1.y;
    float wy_0 = _S1.x * _S1.z;
    float wz_0 = _S1.x * _S1.w;
    Matrix<float, 3, 3>  M_0 = mul_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_0 + z2_0), 2.0f * (xy_0 + wz_0), 2.0f * (xz_0 - wy_0), 2.0f * (xy_0 - wz_0), 1.0f - 2.0f * (x2_0 + z2_0), 2.0f * (yz_0 + wx_0), 2.0f * (xz_0 + wy_0), 2.0f * (yz_0 - wx_0), 1.0f - 2.0f * (x2_0 + y2_0))), makeMatrix<float, 3, 3> (_S2.x, 0.0f, 0.0f, 0.0f, _S2.y, 0.0f, 0.0f, 0.0f, _S2.z));
    float4  _S3 = make_float4 (dot_0(*mean_0, *mean_0), dot_0(*mean_0, log_scale_1), dot_0(log_scale_1, log_scale_1), dot_1(quat_0, make_float4 (opac_0))) * make_float4 (0.1031000018119812f, 0.10300000011920929f, 0.09730000048875809f, 0.10989999771118164f);
    float4  _S4 = _S3 - floor_0(_S3);
    float4  _S5 = _S4 + make_float4 (dot_1(_S4, float4 {_S4.w, _S4.z, _S4.x, _S4.y} + make_float4 (33.3300018310546875f)));
    float4  _S6 = (float4 {_S5.x, _S5.x, _S5.y, _S5.z} + float4 {_S5.y, _S5.z, _S5.z, _S5.w}) * float4 {_S5.z, _S5.y, _S5.w, _S5.x};
    float4  _S7 = _S6 - floor_0(_S6);
    float2  _S8 = float2 {_S7.x, _S7.z};
    float _S9 = 6.28318548202514648f * _S8.y;
    float2  _S10 = float2 {_S7.y, _S7.w};
    float _S11 = 6.28318548202514648f * _S10.y;
    *mean_0 = *mean_0 + mul_1(mul_0(M_0, transpose_0(M_0)), make_float3 ((make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S8.x))))))) * make_float2 ((F32_cos((_S9))), (F32_sin((_S9))))).x, (make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S8.x))))))) * make_float2 ((F32_cos((_S9))), (F32_sin((_S9))))).y, (make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S10.x))))))) * make_float2 ((F32_cos((_S11))), (F32_sin((_S11))))).x) * make_float3 (scaler_0) * make_float3 (1.0f / (1.0f + (F32_exp((-100.0f * (1.0f - opac_0 - 0.99500000476837158f)))))));
    return;
}

inline __device__ float3  sqrt_0(float3  x_8)
{
    float3  result_9;
    int i_6 = int(0);
    for(;;)
    {
        if(i_6 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_9, i_6) = (F32_sqrt((_slang_vector_get_element(x_8, i_6))));
        i_6 = i_6 + int(1);
    }
    return result_9;
}

inline __device__ void revised_add_noise_3dgs(float scaler_1, float radii_0, float3  * mean_1, float3  log_scale_2, float4  quat_1, float logit_opac_1)
{
    if(radii_0 <= 0.0f)
    {
        return;
    }
    float opac_1 = 1.0f / (1.0f + (F32_exp((- logit_opac_1))));
    float4  _S12 = normalize_0(quat_1);
    float3  _S13 = sqrt_0(exp_0(log_scale_2));
    float x_9 = _S12.y;
    float x2_1 = x_9 * x_9;
    float y2_1 = _S12.z * _S12.z;
    float z2_1 = _S12.w * _S12.w;
    float xy_1 = _S12.y * _S12.z;
    float xz_1 = _S12.y * _S12.w;
    float yz_1 = _S12.z * _S12.w;
    float wx_1 = _S12.x * _S12.y;
    float wy_1 = _S12.x * _S12.z;
    float wz_1 = _S12.x * _S12.w;
    Matrix<float, 3, 3>  M_1 = mul_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_1 + z2_1), 2.0f * (xy_1 + wz_1), 2.0f * (xz_1 - wy_1), 2.0f * (xy_1 - wz_1), 1.0f - 2.0f * (x2_1 + z2_1), 2.0f * (yz_1 + wx_1), 2.0f * (xz_1 + wy_1), 2.0f * (yz_1 - wx_1), 1.0f - 2.0f * (x2_1 + y2_1))), makeMatrix<float, 3, 3> (_S13.x, 0.0f, 0.0f, 0.0f, _S13.y, 0.0f, 0.0f, 0.0f, _S13.z));
    float4  _S14 = make_float4 (dot_0(*mean_1, *mean_1), dot_0(*mean_1, log_scale_2), dot_0(log_scale_2, log_scale_2), dot_1(quat_1, make_float4 (opac_1))) * make_float4 (0.1031000018119812f, 0.10300000011920929f, 0.09730000048875809f, 0.10989999771118164f);
    float4  _S15 = _S14 - floor_0(_S14);
    float4  _S16 = _S15 + make_float4 (dot_1(_S15, float4 {_S15.w, _S15.z, _S15.x, _S15.y} + make_float4 (33.3300018310546875f)));
    float4  _S17 = (float4 {_S16.x, _S16.x, _S16.y, _S16.z} + float4 {_S16.y, _S16.z, _S16.z, _S16.w}) * float4 {_S16.z, _S16.y, _S16.w, _S16.x};
    float4  _S18 = _S17 - floor_0(_S17);
    float2  _S19 = float2 {_S18.x, _S18.z};
    float _S20 = 6.28318548202514648f * _S19.y;
    float2  _S21 = float2 {_S18.y, _S18.w};
    float _S22 = 6.28318548202514648f * _S21.y;
    *mean_1 = *mean_1 + mul_1(mul_0(M_1, transpose_0(M_1)), make_float3 ((F32_min((0.05000000074505806f * scaler_1 * (F32_sqrt((2.0f * (F32_log(((F32_max((255.0f * opac_1), (1.0f))))))))) * (F32_pow((1.0f - opac_1), (150.0f)))), (1.0f)))) * make_float3 ((make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S19.x))))))) * make_float2 ((F32_cos((_S20))), (F32_sin((_S20))))).x, (make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S19.x))))))) * make_float2 ((F32_cos((_S20))), (F32_sin((_S20))))).y, (make_float2 ((F32_sqrt((-2.0f * (F32_log((1.0f - _S21.x))))))) * make_float2 ((F32_cos((_S22))), (F32_sin((_S22))))).x));
    return;
}

inline __device__ void long_axis_split_3dgs(float k_0, float3  log_scale_3, float logit_opacity_0, float4  quat_2, float3  * new_log_scale_0, float * new_logit_opacity_0, float3  * mean_delta_0)
{
    float _S23 = log_scale_3.x;
    float _S24 = log_scale_3.y;
    float _S25 = log_scale_3.z;
    float d_0 = 0.5f * (F32_exp(((F32_max(((F32_max((_S23), (_S24)))), (_S25))))));
    *new_log_scale_0 = log_scale_3;
    *mean_delta_0 = make_float3 (0.0f, 0.0f, 0.0f);
    float kl_0 = (F32_log((0.5f)));
    float ks_0 = (F32_log((0.85000002384185791f)));
    bool _S26;
    if(_S23 > _S24)
    {
        _S26 = _S23 > _S25;
    }
    else
    {
        _S26 = false;
    }
    if(_S26)
    {
        *new_log_scale_0 = *new_log_scale_0 + make_float3 (kl_0, ks_0, ks_0);
        *&(mean_delta_0->x) = d_0;
    }
    else
    {
        if(_S24 > _S25)
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
    float4  _S27 = normalize_0(quat_2);
    float x_10 = _S27.y;
    float x2_2 = x_10 * x_10;
    float y2_2 = _S27.z * _S27.z;
    float z2_2 = _S27.w * _S27.w;
    float xy_2 = _S27.y * _S27.z;
    float xz_2 = _S27.y * _S27.w;
    float yz_2 = _S27.z * _S27.w;
    float wx_2 = _S27.x * _S27.y;
    float wy_2 = _S27.x * _S27.z;
    float wz_2 = _S27.x * _S27.w;
    *mean_delta_0 = mul_1(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_2 + z2_2), 2.0f * (xy_2 + wz_2), 2.0f * (xz_2 - wy_2), 2.0f * (xy_2 - wz_2), 1.0f - 2.0f * (x2_2 + z2_2), 2.0f * (yz_2 + wx_2), 2.0f * (xz_2 + wy_2), 2.0f * (yz_2 - wx_2), 1.0f - 2.0f * (x2_2 + y2_2))), *mean_delta_0);
    *new_logit_opacity_0 = (F32_log((k_0 / (1.0f + (F32_exp((- logit_opacity_0))) - k_0))));
    return;
}

