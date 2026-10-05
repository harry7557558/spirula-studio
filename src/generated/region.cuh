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

inline __device__ float length_0(float3  x_1)
{
    return (F32_sqrt((dot_0(x_1, x_1))));
}

inline __device__ float dot_1(float2  x_2, float2  y_1)
{
    int i_1 = int(0);
    float result_2 = 0.0f;
    for(;;)
    {
        if(i_1 < int(2))
        {
        }
        else
        {
            break;
        }
        float result_3 = result_2 + _slang_vector_get_element(x_2, i_1) * _slang_vector_get_element(y_1, i_1);
        i_1 = i_1 + int(1);
        result_2 = result_3;
    }
    return result_2;
}

inline __device__ float3  normalize_0(float3  x_3)
{
    return x_3 / make_float3 (length_0(x_3));
}

inline __device__ float3  max_0(float3  x_4, float3  y_2)
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
        *_slang_vector_get_element_ptr(&result_4, i_2) = (F32_max((_slang_vector_get_element(x_4, i_2)), (_slang_vector_get_element(y_2, i_2))));
        i_2 = i_2 + int(1);
    }
    return result_4;
}

inline __device__ float aabb_d2_0(float3  p_0, float3  lo_0, float3  hi_0)
{
    float3  d_0 = max_0(max_0(lo_0 - p_0, p_0 - hi_0), make_float3 (0.0f));
    return dot_0(d_0, d_0);
}

inline __device__ float3  oct_decode_0(uint packed_0)
{
    float u_0 = float(packed_0 & 4095U) / 4095.0f * 2.0f - 1.0f;
    float v_0 = float((packed_0 >> int(12)) & 4095U) / 4095.0f * 2.0f - 1.0f;
    float _S1 = 1.0f - (F32_abs((u_0))) - (F32_abs((v_0)));
    float3  n_0 = make_float3 (u_0, v_0, _S1);
    float _S2 = (F32_max((- _S1), (0.0f)));
    float _S3;
    if(u_0 >= 0.0f)
    {
        _S3 = - _S2;
    }
    else
    {
        _S3 = _S2;
    }
    *&((&n_0)->x) = *&((&n_0)->x) + _S3;
    if((n_0.y) >= 0.0f)
    {
        _S3 = - _S2;
    }
    else
    {
        _S3 = _S2;
    }
    *&((&n_0)->y) = *&((&n_0)->y) + _S3;
    return normalize_0(n_0);
}

inline __device__ float seed_d2_0(float3  p_1, float3  n_1, bool has_n_0, float4  seed_0)
{
    float3  d_1 = p_1 - float3 {seed_0.x, seed_0.y, seed_0.z};
    float e2_0 = dot_0(d_1, d_1);
    uint dir_bits_0 = (F32_asuint((seed_0.w))) >> int(8);
    float3  dir_0;
    float e2_1;
    if(dir_bits_0 != 16777215U)
    {
        float3  dir_1 = oct_decode_0(dir_bits_0);
        float _S4 = 8.0f * (F32_max((0.0f), (- dot_0(d_1, dir_1))));
        float e2_2 = e2_0 + _S4 * _S4;
        dir_0 = dir_1;
        e2_1 = e2_2;
    }
    else
    {
        dir_0 = - d_1 * make_float3 ((F32_rsqrt(((F32_max((e2_0), (1.00000000317107685e-30f)))))));
        e2_1 = e2_0;
    }
    if(has_n_0)
    {
        float a_0 = dot_0(n_1, dir_0);
        if(a_0 < -0.5f)
        {
            return -1.0f;
        }
        e2_1 = e2_1 * (1.0f + 20.0f * (F32_max((0.0f), (- a_0))));
    }
    return e2_1;
}

inline __device__ uint label_field_nearest(float3  p_2, float3  n_2, bool has_n_1, float4  * bvh_0, float4  * seeds_0, uint num_nodes_0)
{
    if(num_nodes_0 == 0U)
    {
        return 255U;
    }
    FixedArray<uint, 48>  stack_0;
    stack_0[0U] = 0U;
    float best_0 = 3.00000000549775576e+38f;
    uint label_0 = 255U;
    uint sp_0 = 1U;
    for(;;)
    {
        if(sp_0 > 0U)
        {
        }
        else
        {
            break;
        }
        uint sp_1 = sp_0 - 1U;
        uint _S5 = 2U * stack_0[sp_1];
        float4  * _S6 = bvh_0 + _S5;
        float4  n0_0 = *_S6;
        float4  * _S7 = bvh_0 + (_S5 + 1U);
        float4  n1_0 = *_S7;
        if((aabb_d2_0(p_2, float3 {(*_S6).x, (*_S6).y, (*_S6).z}, float3 {(*_S7).x, (*_S7).y, (*_S7).z})) >= best_0)
        {
            sp_0 = sp_1;
            continue;
        }
        uint first_0 = (F32_asuint((n0_0.w)));
        uint count_0 = (F32_asuint((n1_0.w)));
        float best_1;
        uint label_1;
        uint i_3;
        if(count_0 > 0U)
        {
            best_1 = best_0;
            label_1 = label_0;
            i_3 = first_0;
            for(;;)
            {
                if(i_3 < (first_0 + count_0))
                {
                }
                else
                {
                    break;
                }
                float4  * _S8 = seeds_0 + i_3;
                float4  s_0 = *_S8;
                float e2_3 = seed_d2_0(p_2, n_2, has_n_1, *_S8);
                bool _S9;
                if(e2_3 >= 0.0f)
                {
                    _S9 = e2_3 < best_1;
                }
                else
                {
                    _S9 = false;
                }
                if(_S9)
                {
                    uint _S10 = (F32_asuint((s_0.w))) & 255U;
                    best_1 = e2_3;
                    label_1 = _S10;
                }
                i_3 = i_3 + 1U;
            }
            sp_0 = sp_1;
        }
        else
        {
            uint _S11 = 2U * first_0;
            bool _S12 = (aabb_d2_0(p_2, float3 {(*(bvh_0 + _S11)).x, (*(bvh_0 + _S11)).y, (*(bvh_0 + _S11)).z}, float3 {(*(bvh_0 + (_S11 + 1U))).x, (*(bvh_0 + (_S11 + 1U))).y, (*(bvh_0 + (_S11 + 1U))).z})) <= (aabb_d2_0(p_2, float3 {(*(bvh_0 + (_S11 + 2U))).x, (*(bvh_0 + (_S11 + 2U))).y, (*(bvh_0 + (_S11 + 2U))).z}, float3 {(*(bvh_0 + (_S11 + 3U))).x, (*(bvh_0 + (_S11 + 3U))).y, (*(bvh_0 + (_S11 + 3U))).z}));
            if(_S12)
            {
                label_1 = first_0;
            }
            else
            {
                label_1 = first_0 + 1U;
            }
            if(_S12)
            {
                i_3 = first_0 + 1U;
            }
            else
            {
                i_3 = first_0;
            }
            uint sp_2;
            if((sp_1 + 2U) <= 48U)
            {
                uint sp_3 = sp_1 + 1U;
                stack_0[sp_1] = i_3;
                uint _S13 = sp_3 + 1U;
                stack_0[sp_3] = label_1;
                sp_2 = _S13;
            }
            else
            {
                sp_2 = sp_1;
            }
            best_1 = best_0;
            label_1 = label_0;
            sp_0 = sp_2;
        }
        best_0 = best_1;
        label_0 = label_1;
    }
    return label_0;
}

inline __device__ bool region_contains(float3  p_3, float3  n_3, bool has_n_2, float4  * program_0, uint num_prog_0, float4  * bvh_1, float4  * seeds_1, uint num_field_nodes_0)
{
    bool _S14;
    FixedArray<bool, 32>  stack_1;
    stack_1[int(0)] = false;
    stack_1[int(1)] = false;
    stack_1[int(2)] = false;
    stack_1[int(3)] = false;
    stack_1[int(4)] = false;
    stack_1[int(5)] = false;
    stack_1[int(6)] = false;
    stack_1[int(7)] = false;
    stack_1[int(8)] = false;
    stack_1[int(9)] = false;
    stack_1[int(10)] = false;
    stack_1[int(11)] = false;
    stack_1[int(12)] = false;
    stack_1[int(13)] = false;
    stack_1[int(14)] = false;
    stack_1[int(15)] = false;
    stack_1[int(16)] = false;
    stack_1[int(17)] = false;
    stack_1[int(18)] = false;
    stack_1[int(19)] = false;
    stack_1[int(20)] = false;
    stack_1[int(21)] = false;
    stack_1[int(22)] = false;
    stack_1[int(23)] = false;
    stack_1[int(24)] = false;
    stack_1[int(25)] = false;
    stack_1[int(26)] = false;
    stack_1[int(27)] = false;
    stack_1[int(28)] = false;
    stack_1[int(29)] = false;
    stack_1[int(30)] = false;
    stack_1[int(31)] = false;
    uint i_4 = 0U;
    uint sp_4 = 0U;
    for(;;)
    {
        if(i_4 < num_prog_0)
        {
        }
        else
        {
            break;
        }
        uint _S15 = i_4 * 6U;
        float4  * _S16 = program_0 + _S15;
        float4  h_0 = *_S16;
        uint type_0 = uint((*_S16).x);
        bool _S17 = type_0 == 0U;
        if(_S17)
        {
            _S14 = true;
        }
        else
        {
            if(type_0 >= 10U)
            {
                _S14 = type_0 <= 12U;
            }
            else
            {
                _S14 = false;
            }
        }
        uint sp_5;
        uint i_5;
        bool b_0;
        bool a_1;
        bool v_1;
        if(_S14)
        {
            float4  half_0 = *(program_0 + (_S15 + 2U));
            float3  d_2 = p_3 - float3 {(*(program_0 + (_S15 + 1U))).x, (*(program_0 + (_S15 + 1U))).y, (*(program_0 + (_S15 + 1U))).z};
            float _S18 = dot_0(float3 {(*(program_0 + (_S15 + 3U))).x, (*(program_0 + (_S15 + 3U))).y, (*(program_0 + (_S15 + 3U))).z}, d_2);
            float _S19 = dot_0(float3 {(*(program_0 + (_S15 + 4U))).x, (*(program_0 + (_S15 + 4U))).y, (*(program_0 + (_S15 + 4U))).z}, d_2);
            float _S20 = dot_0(float3 {(*(program_0 + (_S15 + 5U))).x, (*(program_0 + (_S15 + 5U))).y, (*(program_0 + (_S15 + 5U))).z}, d_2);
            float3  q_0 = make_float3 (_S18, _S19, _S20);
            if(_S17)
            {
                if((F32_abs((_S18))) <= (half_0.x))
                {
                    b_0 = (F32_abs((_S19))) <= (half_0.y);
                }
                else
                {
                    b_0 = false;
                }
                if(b_0)
                {
                    a_1 = (F32_abs((_S20))) <= (half_0.z);
                }
                else
                {
                    a_1 = false;
                }
                v_1 = a_1;
                sp_5 = i_4;
            }
            else
            {
                if(type_0 == 10U)
                {
                    float3  u_1 = q_0 / float3 {half_0.x, half_0.y, half_0.z};
                    v_1 = (dot_0(u_1, u_1)) <= 1.0f;
                    sp_5 = i_4;
                }
                else
                {
                    if(type_0 == 11U)
                    {
                        float2  u_2 = make_float2 (_S18, _S19) / float2 {half_0.x, half_0.y};
                        if((F32_abs((_S20))) <= (half_0.z))
                        {
                            b_0 = (dot_1(u_2, u_2)) <= 1.0f;
                        }
                        else
                        {
                            b_0 = false;
                        }
                        v_1 = b_0;
                        sp_5 = i_4;
                    }
                    else
                    {
                        uint nv_0 = uint(h_0.y);
                        uint _S21 = (i_4 + 1U) * 6U;
                        if((F32_abs((_S20))) <= (half_0.z))
                        {
                            sp_5 = nv_0 - 1U;
                            v_1 = false;
                            i_5 = 0U;
                            for(;;)
                            {
                                if(i_5 < nv_0)
                                {
                                }
                                else
                                {
                                    break;
                                }
                                float4  fa_0 = *(program_0 + (_S21 + i_5 / 2U));
                                float4  fb_0 = *(program_0 + (_S21 + sp_5 / 2U));
                                float2  va_0;
                                if((i_5 & 1U) != 0U)
                                {
                                    va_0 = float2 {fa_0.z, fa_0.w};
                                }
                                else
                                {
                                    va_0 = float2 {fa_0.x, fa_0.y};
                                }
                                float2  vb_0;
                                if((sp_5 & 1U) != 0U)
                                {
                                    vb_0 = float2 {fb_0.z, fb_0.w};
                                }
                                else
                                {
                                    vb_0 = float2 {fb_0.x, fb_0.y};
                                }
                                float _S22 = va_0.y;
                                float _S23 = vb_0.y;
                                if((_S22 > _S19) != (_S23 > _S19))
                                {
                                    float _S24 = va_0.x;
                                    b_0 = _S18 < ((vb_0.x - _S24) * (_S19 - _S22) / (_S23 - _S22) + _S24);
                                }
                                else
                                {
                                    b_0 = false;
                                }
                                if(b_0)
                                {
                                    v_1 = !v_1;
                                }
                                uint _S25 = i_5 + 1U;
                                sp_5 = i_5;
                                i_5 = _S25;
                            }
                        }
                        else
                        {
                            v_1 = false;
                        }
                        sp_5 = i_4 + (2U * nv_0 + 24U - 1U) / 24U;
                    }
                }
            }
            uint _S26 = sp_5;
            sp_5 = sp_4;
            i_5 = _S26;
        }
        else
        {
            if(type_0 == 1U)
            {
                float4  * _S27 = program_0 + (_S15 + 1U);
                float3  d_3 = p_3 - float3 {(*_S27).x, (*_S27).y, (*_S27).z};
                float _S28 = (*_S27).w;
                v_1 = (dot_0(d_3, d_3)) <= (_S28 * _S28);
                sp_5 = sp_4;
            }
            else
            {
                if(type_0 == 2U)
                {
                    float4  * _S29 = program_0 + (_S15 + 1U);
                    v_1 = (dot_0(float3 {(*_S29).x, (*_S29).y, (*_S29).z}, p_3) + (*_S29).w) >= 0.0f;
                    sp_5 = sp_4;
                }
                else
                {
                    if(type_0 == 3U)
                    {
                        uint _S30 = label_field_nearest(p_3, n_3, has_n_2, bvh_1, seeds_1, num_field_nodes_0);
                        v_1 = _S30 == uint(h_0.w);
                        sp_5 = sp_4;
                    }
                    else
                    {
                        if(type_0 == 8U)
                        {
                            v_1 = true;
                            sp_5 = sp_4;
                        }
                        else
                        {
                            if(type_0 == 9U)
                            {
                                v_1 = false;
                                sp_5 = sp_4;
                            }
                            else
                            {
                                if(type_0 == 7U)
                                {
                                    if(sp_4 > 0U)
                                    {
                                        uint sp_6 = sp_4 - 1U;
                                        v_1 = !stack_1[sp_6];
                                        sp_5 = sp_6;
                                    }
                                    else
                                    {
                                        v_1 = false;
                                        sp_5 = sp_4;
                                    }
                                }
                                else
                                {
                                    if(sp_4 > 0U)
                                    {
                                        uint sp_7 = sp_4 - 1U;
                                        b_0 = stack_1[sp_7];
                                        sp_5 = sp_7;
                                    }
                                    else
                                    {
                                        b_0 = false;
                                        sp_5 = sp_4;
                                    }
                                    if(sp_5 > 0U)
                                    {
                                        uint sp_8 = sp_5 - 1U;
                                        a_1 = stack_1[sp_8];
                                        i_5 = sp_8;
                                    }
                                    else
                                    {
                                        a_1 = false;
                                        i_5 = sp_5;
                                    }
                                    if(type_0 == 4U)
                                    {
                                        if(a_1)
                                        {
                                            v_1 = true;
                                        }
                                        else
                                        {
                                            v_1 = b_0;
                                        }
                                    }
                                    else
                                    {
                                        if(type_0 == 5U)
                                        {
                                            if(a_1)
                                            {
                                                v_1 = b_0;
                                            }
                                            else
                                            {
                                                v_1 = false;
                                            }
                                        }
                                        else
                                        {
                                            if(a_1)
                                            {
                                                v_1 = !b_0;
                                            }
                                            else
                                            {
                                                v_1 = false;
                                            }
                                        }
                                    }
                                    sp_5 = i_5;
                                }
                            }
                        }
                    }
                }
            }
            i_5 = i_4;
        }
        if(sp_5 < 32U)
        {
            uint _S31 = sp_5 + 1U;
            stack_1[sp_5] = v_1;
            sp_4 = _S31;
        }
        else
        {
            sp_4 = sp_5;
        }
        i_4 = i_5 + 1U;
    }
    if(sp_4 > 0U)
    {
        _S14 = stack_1[sp_4 - 1U];
    }
    else
    {
        _S14 = false;
    }
    return _S14;
}

inline __device__ float3  splat_normal(float4  quat_0, float3  log_scale_0, float3  mean_0, float3  toward_0)
{
    float w_0 = quat_0.x;
    float x_5 = quat_0.y;
    float y_3 = quat_0.z;
    float z_0 = quat_0.w;
    float inv_0 = (F32_rsqrt(((F32_max((w_0 * w_0 + x_5 * x_5 + y_3 * y_3 + z_0 * z_0), (9.99999968265522539e-21f))))));
    float w_1 = w_0 * inv_0;
    float x_6 = x_5 * inv_0;
    float y_4 = y_3 * inv_0;
    float z_1 = z_0 * inv_0;
    float _S32 = y_4 * y_4;
    float _S33 = z_1 * z_1;
    float _S34 = x_6 * y_4;
    float _S35 = z_1 * w_1;
    float _S36 = x_6 * z_1;
    float _S37 = y_4 * w_1;
    float3  c0_0 = make_float3 (1.0f - 2.0f * (_S32 + _S33), 2.0f * (_S34 + _S35), 2.0f * (_S36 - _S37));
    float _S38 = x_6 * x_6;
    float _S39 = y_4 * z_1;
    float _S40 = x_6 * w_1;
    float3  c1_0 = make_float3 (2.0f * (_S34 - _S35), 1.0f - 2.0f * (_S38 + _S33), 2.0f * (_S39 + _S40));
    float3  c2_0 = make_float3 (2.0f * (_S36 + _S37), 2.0f * (_S39 - _S40), 1.0f - 2.0f * (_S38 + _S32));
    float _S41 = log_scale_0.y;
    float _S42 = log_scale_0.x;
    bool _S43;
    if(_S41 < _S42)
    {
        _S43 = _S41 <= (log_scale_0.z);
    }
    else
    {
        _S43 = false;
    }
    float3  axis_0;
    if(_S43)
    {
        axis_0 = c1_0;
    }
    else
    {
        float _S44 = log_scale_0.z;
        if(_S44 < _S42)
        {
            _S43 = _S44 < _S41;
        }
        else
        {
            _S43 = false;
        }
        if(_S43)
        {
            axis_0 = c2_0;
        }
        else
        {
            axis_0 = c0_0;
        }
    }
    if((dot_0(axis_0, toward_0 - mean_0)) < 0.0f)
    {
        axis_0 = - axis_0;
    }
    return axis_0;
}

