#pragma once

#include "generated/slang.cuh"

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

inline __device__ float length_0(float3  x_2)
{
    return (F32_sqrt((dot_0(x_2, x_2))));
}

inline __device__ float length_1(float4  x_3)
{
    return (F32_sqrt((dot_1(x_3, x_3))));
}

inline __device__ float4  normalize_0(float4  x_4)
{
    return x_4 / make_float4 (length_1(x_4));
}

inline __device__ Matrix<float, 3, 3>  transpose_0(Matrix<float, 3, 3>  x_5)
{
    Matrix<float, 3, 3>  result_4;
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
            *_slang_vector_get_element_ptr(((&result_4)->rows + (r_0)), c_0) = _slang_vector_get_element(x_5.rows[c_0], r_0);
            c_0 = c_0 + int(1);
        }
        r_0 = r_0 + int(1);
    }
    return result_4;
}

inline __device__ float3  mul_0(Matrix<float, 3, 3>  left_0, float3  right_0)
{
    float3  result_5;
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
        int j_0 = int(0);
        float sum_0 = 0.0f;
        for(;;)
        {
            if(j_0 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_1 = sum_0 + _slang_vector_get_element(left_0.rows[i_2], j_0) * _slang_vector_get_element(right_0, j_0);
            j_0 = j_0 + int(1);
            sum_0 = sum_1;
        }
        *_slang_vector_get_element_ptr(&result_5, i_2) = sum_0;
        i_2 = i_2 + int(1);
    }
    return result_5;
}

inline __device__ float3  exp_0(float3  x_6)
{
    float3  result_6;
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
        *_slang_vector_get_element_ptr(&result_6, i_3) = (F32_exp((_slang_vector_get_element(x_6, i_3))));
        i_3 = i_3 + int(1);
    }
    return result_6;
}

inline __device__ float projective_depth_information(float3  anchor_0, float3  camera_0, float3  mean_0, float3  log_scale_0, float4  quat_0)
{
    float3  view_0 = mean_0 - camera_0;
    float distance_0 = length_0(view_0);
    if(!(distance_0 > 0.0f))
    {
        return 0.0f;
    }
    float3  ray_0 = view_0 / make_float3 (distance_0);
    float3  baseline_0 = camera_0 - anchor_0;
    float4  _S1 = normalize_0(quat_0);
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
    float3  local_ray_0 = mul_0(transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_0 + z2_0), 2.0f * (xy_0 + wz_0), 2.0f * (xz_0 - wy_0), 2.0f * (xy_0 - wz_0), 1.0f - 2.0f * (x2_0 + z2_0), 2.0f * (yz_0 + wx_0), 2.0f * (xz_0 + wy_0), 2.0f * (yz_0 - wx_0), 1.0f - 2.0f * (x2_0 + y2_0)))), ray_0);
    float3  variance_0 = exp_0(make_float3 (2.0f) * log_scale_0);
    float _S2 = local_ray_0.y;
    float _S3 = _S2 * _S2;
    float _S4 = local_ray_0.z;
    float _S5 = _S4 * _S4;
    float _S6 = local_ray_0.x;
    float _S7 = _S6 * _S6;
    float _S8 = dot_0(ray_0, baseline_0);
    float3  transverse_0 = baseline_0 - ray_0 * make_float3 (_S8);
    float magnification_0 = _S8 / distance_0;
    return dot_0(transverse_0, transverse_0) / (F32_max((0.5f * (variance_0.x * (_S3 + _S5) + variance_0.y * (_S7 + _S5) + variance_0.z * (_S7 + _S3))), (1.00000000317107685e-30f))) + 4.0f * magnification_0 * magnification_0;
}

inline __device__ float projective_depth_gradient(float3  anchor_1, float3  camera_1, float3  mean_1, float3  position_gradient_0, float3  scale_gradient_0, bool single_photometric_0)
{
    float gradient_0;
    if(single_photometric_0)
    {
        gradient_0 = dot_0(camera_1 - anchor_1, position_gradient_0);
    }
    else
    {
        gradient_0 = dot_0(mean_1 - anchor_1, position_gradient_0) + scale_gradient_0.x + scale_gradient_0.y + scale_gradient_0.z;
    }
    return gradient_0;
}

inline __device__ void projective_depth_update(float3  anchor_2, float3  camera_2, float lr_0, float angular_lr_0, bool single_photometric_1, bool angular_relative_0, float gradient_1, float information_0, float4  * state_0, float3  * mean_2, float3  * log_scale_1)
{
    float3  baseline_1 = camera_2 - anchor_2;
    bool _S9;
    if(single_photometric_1)
    {
        _S9 = (dot_0(baseline_1, baseline_1)) == 0.0f;
    }
    else
    {
        _S9 = false;
    }
    if(_S9)
    {
        return;
    }
    float3  view_1 = *mean_2 - camera_2;
    float distance_1 = length_0(view_1);
    if(!(distance_1 > 0.0f))
    {
        _S9 = true;
    }
    else
    {
        _S9 = !(F32_isfinite((gradient_1)));
    }
    if(_S9)
    {
        return;
    }
    float fraction_0;
    if(!angular_relative_0)
    {
        fraction_0 = angular_lr_0 / distance_1;
    }
    else
    {
        fraction_0 = angular_lr_0;
    }
    if(fraction_0 <= 0.0f)
    {
        return;
    }
    *&(state_0->y) = 0.89999997615814209f * (*state_0).y + 0.10000000149011612f * gradient_1;
    *&(state_0->z) = 0.99900001287460327f * (*state_0).z + 0.00100000004749745f * gradient_1 * gradient_1;
    *&(state_0->w) = *&(state_0->w) + 1.0f;
    float change_0 = - lr_0 * ((*state_0).y / (1.0f - (F32_pow((0.89999997615814209f), ((*state_0).w))))) / (((F32_sqrt(((*state_0).z / (1.0f - (F32_pow((0.99900001287460327f), ((*state_0).w))))))) + 1.00000000362749373e-15f) * (1.0f + information_0));
    float3  ray_1 = view_1 / make_float3 (distance_1);
    float projected_step_0 = length_0((baseline_1 - ray_1 * make_float3 (dot_0(ray_1, baseline_1))) / make_float3 (distance_1)) * change_0 / fraction_0;
    float change_1 = change_0 / (F32_sqrt((1.0f + projected_step_0 * projected_step_0)));
    if(change_1 == 0.0f)
    {
        return;
    }
    if((F32_abs((change_1))) < 0.00999999977648258f)
    {
        fraction_0 = change_1 * (1.0f + change_1 * (0.5f + change_1 * (0.1666666716337204f + change_1 * (0.0416666679084301f + change_1 / 120.0f))));
    }
    else
    {
        fraction_0 = (F32_exp((change_1))) - 1.0f;
    }
    *mean_2 = *mean_2 + make_float3 (fraction_0) * (*mean_2 - anchor_2);
    *log_scale_1 = *log_scale_1 + make_float3 (change_1);
    return;
}

