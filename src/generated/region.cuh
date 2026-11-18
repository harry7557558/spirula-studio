#pragma once

#include "generated/slang.cuh"

inline __device__ float3  max_0(float3  x_0, float3  y_0)
{
    float3  result_0;
    int i_0 = int(0);
    for(;;)
    {
        if(i_0 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_0, i_0) = (F32_max((_slang_vector_get_element(x_0, i_0)), (_slang_vector_get_element(y_0, i_0))));
        i_0 = i_0 + int(1);
    }
    return result_0;
}

inline __device__ float dot_0(float3  x_1, float3  y_1)
{
    int i_1 = int(0);
    float result_1 = 0.0f;
    for(;;)
    {
        if(i_1 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_2 = result_1 + _slang_vector_get_element(x_1, i_1) * _slang_vector_get_element(y_1, i_1);
        i_1 = i_1 + int(1);
        result_1 = result_2;
    }
    return result_1;
}

inline __device__ float dot_1(float2  x_2, float2  y_2)
{
    int i_2 = int(0);
    float result_3 = 0.0f;
    for(;;)
    {
        if(i_2 < int(2))
        {
        }
        else
        {
            break;
        }
        float result_4 = result_3 + _slang_vector_get_element(x_2, i_2) * _slang_vector_get_element(y_2, i_2);
        i_2 = i_2 + int(1);
        result_3 = result_4;
    }
    return result_3;
}

inline __device__ float aabb_d2_0(float3  p_0, float3  lo_0, float3  hi_0)
{
    float3  d_0 = max_0(max_0(lo_0 - p_0, p_0 - hi_0), make_float3 (0.0f));
    return dot_0(d_0, d_0);
}

inline __device__ float length_0(float3  x_3)
{
    return (F32_sqrt((dot_0(x_3, x_3))));
}

inline __device__ float3  normalize_0(float3  x_4)
{
    return x_4 / make_float3 (length_0(x_4));
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

inline __device__ float seed_d2_0(float3  p_1, float3  n_1, bool has_n_0, float4  seed_0, bool euclidean_0)
{
    float3  d_1 = p_1 - float3 {seed_0.x, seed_0.y, seed_0.z};
    float e2_0 = dot_0(d_1, d_1);
    uint packed_1 = (F32_asuint((seed_0.w)));
    if(euclidean_0)
    {
        return e2_0;
    }
    uint dir_bits_0 = packed_1 >> int(8);
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

inline __device__ uint label_field_query_0(float3  p_2, float3  n_2, bool has_n_1, float4  * bvh_0, float4  * seeds_0, uint num_nodes_0, bool euclidean_1, bool index_0)
{
    uint label_0;
    if(num_nodes_0 == 0U)
    {
        if(index_0)
        {
            label_0 = 4294967295U;
        }
        else
        {
            label_0 = 255U;
        }
        return label_0;
    }
    FixedArray<uint, 48>  stack_0;
    stack_0[0U] = 0U;
    if(index_0)
    {
        label_0 = 4294967295U;
    }
    else
    {
        label_0 = 255U;
    }
    float best_0 = 3.00000000549775576e+38f;
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
        uint label_1;
        uint i_3;
        uint sp_2;
        float best_1;
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
                float e2_3 = seed_d2_0(p_2, n_2, has_n_1, *_S8, euclidean_1);
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
                    if(index_0)
                    {
                        sp_2 = i_3;
                    }
                    else
                    {
                        sp_2 = (F32_asuint((s_0.w))) & 255U;
                    }
                    best_1 = e2_3;
                    label_1 = sp_2;
                }
                i_3 = i_3 + 1U;
            }
            sp_0 = sp_1;
        }
        else
        {
            uint _S10 = 2U * first_0;
            bool _S11 = (aabb_d2_0(p_2, float3 {(*(bvh_0 + _S10)).x, (*(bvh_0 + _S10)).y, (*(bvh_0 + _S10)).z}, float3 {(*(bvh_0 + (_S10 + 1U))).x, (*(bvh_0 + (_S10 + 1U))).y, (*(bvh_0 + (_S10 + 1U))).z})) <= (aabb_d2_0(p_2, float3 {(*(bvh_0 + (_S10 + 2U))).x, (*(bvh_0 + (_S10 + 2U))).y, (*(bvh_0 + (_S10 + 2U))).z}, float3 {(*(bvh_0 + (_S10 + 3U))).x, (*(bvh_0 + (_S10 + 3U))).y, (*(bvh_0 + (_S10 + 3U))).z}));
            if(_S11)
            {
                label_1 = first_0;
            }
            else
            {
                label_1 = first_0 + 1U;
            }
            if(_S11)
            {
                i_3 = first_0 + 1U;
            }
            else
            {
                i_3 = first_0;
            }
            if((sp_1 + 2U) <= 48U)
            {
                uint sp_3 = sp_1 + 1U;
                stack_0[sp_1] = i_3;
                uint _S12 = sp_3 + 1U;
                stack_0[sp_3] = label_1;
                sp_2 = _S12;
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

inline __device__ uint label_field_nearest(float3  p_3, float3  n_3, bool has_n_2, float4  * bvh_1, float4  * seeds_1, uint num_nodes_1)
{
    uint _S13 = label_field_query_0(p_3, n_3, has_n_2, bvh_1, seeds_1, num_nodes_1, false, false);
    return _S13;
}

inline __device__ uint field_seed_nearest(float3  p_4, float4  * bvh_2, float4  * seeds_2, uint num_nodes_2)
{
    int3  _S14 = make_int3 (int(0));
    float3  _S15 = make_float3 ((float)_S14.x, (float)_S14.y, (float)_S14.z);
    uint _S16 = label_field_query_0(p_4, _S15, false, bvh_2, seeds_2, num_nodes_2, true, true);
    return _S16;
}

inline __device__ bool region_contains(float3  p_5, float3  n_4, bool has_n_3, float4  * program_0, uint num_prog_0, float4  * bvh_3, float4  * seeds_3, uint num_field_nodes_0)
{
    bool _S17;
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
        uint _S18 = i_4 * 6U;
        float4  * _S19 = program_0 + _S18;
        float4  h_0 = *_S19;
        uint type_0 = uint((*_S19).x);
        bool _S20 = type_0 == 0U;
        if(_S20)
        {
            _S17 = true;
        }
        else
        {
            if(type_0 >= 10U)
            {
                _S17 = type_0 <= 12U;
            }
            else
            {
                _S17 = false;
            }
        }
        uint sp_5;
        uint i_5;
        bool b_0;
        bool a_1;
        bool v_1;
        if(_S17)
        {
            float4  half_0 = *(program_0 + (_S18 + 2U));
            float3  d_2 = p_5 - float3 {(*(program_0 + (_S18 + 1U))).x, (*(program_0 + (_S18 + 1U))).y, (*(program_0 + (_S18 + 1U))).z};
            float _S21 = dot_0(float3 {(*(program_0 + (_S18 + 3U))).x, (*(program_0 + (_S18 + 3U))).y, (*(program_0 + (_S18 + 3U))).z}, d_2);
            float _S22 = dot_0(float3 {(*(program_0 + (_S18 + 4U))).x, (*(program_0 + (_S18 + 4U))).y, (*(program_0 + (_S18 + 4U))).z}, d_2);
            float _S23 = dot_0(float3 {(*(program_0 + (_S18 + 5U))).x, (*(program_0 + (_S18 + 5U))).y, (*(program_0 + (_S18 + 5U))).z}, d_2);
            float3  q_0 = make_float3 (_S21, _S22, _S23);
            if(_S20)
            {
                if((F32_abs((_S21))) <= (half_0.x))
                {
                    b_0 = (F32_abs((_S22))) <= (half_0.y);
                }
                else
                {
                    b_0 = false;
                }
                if(b_0)
                {
                    a_1 = (F32_abs((_S23))) <= (half_0.z);
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
                        float2  u_2 = make_float2 (_S21, _S22) / float2 {half_0.x, half_0.y};
                        if((F32_abs((_S23))) <= (half_0.z))
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
                        uint _S24 = (i_4 + 1U) * 6U;
                        if((F32_abs((_S23))) <= (half_0.z))
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
                                float4  fa_0 = *(program_0 + (_S24 + i_5 / 2U));
                                float4  fb_0 = *(program_0 + (_S24 + sp_5 / 2U));
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
                                float _S25 = va_0.y;
                                float _S26 = vb_0.y;
                                if((_S25 > _S22) != (_S26 > _S22))
                                {
                                    float _S27 = va_0.x;
                                    b_0 = _S21 < ((vb_0.x - _S27) * (_S22 - _S25) / (_S26 - _S25) + _S27);
                                }
                                else
                                {
                                    b_0 = false;
                                }
                                if(b_0)
                                {
                                    v_1 = !v_1;
                                }
                                uint _S28 = i_5 + 1U;
                                sp_5 = i_5;
                                i_5 = _S28;
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
            uint _S29 = sp_5;
            sp_5 = sp_4;
            i_5 = _S29;
        }
        else
        {
            if(type_0 == 1U)
            {
                float4  * _S30 = program_0 + (_S18 + 1U);
                float3  d_3 = p_5 - float3 {(*_S30).x, (*_S30).y, (*_S30).z};
                float _S31 = (*_S30).w;
                v_1 = (dot_0(d_3, d_3)) <= (_S31 * _S31);
                sp_5 = sp_4;
            }
            else
            {
                if(type_0 == 2U)
                {
                    float4  * _S32 = program_0 + (_S18 + 1U);
                    v_1 = (dot_0(float3 {(*_S32).x, (*_S32).y, (*_S32).z}, p_5) + (*_S32).w) >= 0.0f;
                    sp_5 = sp_4;
                }
                else
                {
                    if(type_0 == 3U)
                    {
                        uint _S33 = label_field_query_0(p_5, n_4, has_n_3, bvh_3, seeds_3, num_field_nodes_0, (h_0.y) != 0.0f, false);
                        v_1 = _S33 == uint(h_0.w);
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
            uint _S34 = sp_5 + 1U;
            stack_1[sp_5] = v_1;
            sp_4 = _S34;
        }
        else
        {
            sp_4 = sp_5;
        }
        i_4 = i_5 + 1U;
    }
    if(sp_4 > 0U)
    {
        _S17 = stack_1[sp_4 - 1U];
    }
    else
    {
        _S17 = false;
    }
    return _S17;
}

inline __device__ uint region_hash_0(uint x_5)
{
    uint _S35 = (x_5 ^ (x_5 >> int(16))) * 2146121005U;
    uint _S36 = (_S35 ^ (_S35 >> int(15))) * 2221713035U;
    return _S36 ^ (_S36 >> int(16));
}

inline __device__ void quat_columns_0(float4  quat_0, float3  * c0_0, float3  * c1_0, float3  * c2_0)
{
    float w_0 = quat_0.x;
    float x_6 = quat_0.y;
    float y_3 = quat_0.z;
    float z_0 = quat_0.w;
    float inv_0 = (F32_rsqrt(((F32_max((w_0 * w_0 + x_6 * x_6 + y_3 * y_3 + z_0 * z_0), (9.99999968265522539e-21f))))));
    float w_1 = w_0 * inv_0;
    float x_7 = x_6 * inv_0;
    float y_4 = y_3 * inv_0;
    float z_1 = z_0 * inv_0;
    float _S37 = y_4 * y_4;
    float _S38 = z_1 * z_1;
    float _S39 = x_7 * y_4;
    float _S40 = z_1 * w_1;
    float _S41 = x_7 * z_1;
    float _S42 = y_4 * w_1;
    *c0_0 = make_float3 (1.0f - 2.0f * (_S37 + _S38), 2.0f * (_S39 + _S40), 2.0f * (_S41 - _S42));
    float _S43 = x_7 * x_7;
    float _S44 = y_4 * z_1;
    float _S45 = x_7 * w_1;
    *c1_0 = make_float3 (2.0f * (_S39 - _S40), 1.0f - 2.0f * (_S43 + _S38), 2.0f * (_S44 + _S45));
    *c2_0 = make_float3 (2.0f * (_S41 + _S42), 2.0f * (_S44 - _S45), 1.0f - 2.0f * (_S43 + _S37));
    return;
}

inline __device__ float3  exp_0(float3  x_8)
{
    float3  result_5;
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
        *_slang_vector_get_element_ptr(&result_5, i_6) = (F32_exp((_slang_vector_get_element(x_8, i_6))));
        i_6 = i_6 + int(1);
    }
    return result_5;
}

inline __device__ bool region_contains_splat(float3  mean_0, float4  quat_1, float3  log_scale_0, float3  n_5, bool has_n_4, float4  * pattern_0, uint num_pattern_0, uint index_1, uint seed_1, float4  * program_1, uint num_prog_1, float4  * bvh_4, float4  * seeds_4, uint num_field_nodes_1)
{
    float3  _S46 = make_float3 (0.0f);
    float3  a0_0 = _S46;
    float3  a1_0 = _S46;
    float3  a2_0 = _S46;
    uint k_0 = 0U;
    for(;;)
    {
        if(k_0 <= num_pattern_0)
        {
        }
        else
        {
            break;
        }
        if(k_0 == 1U)
        {
            uint h0_0 = region_hash_0(index_1 ^ (region_hash_0(seed_1)));
            uint h1_0 = region_hash_0(h0_0 + 2654435769U);
            float u_3 = float(h0_0 >> int(8)) * 5.9604644775390625e-08f;
            float a_2 = float(h1_0 >> int(8)) * 3.74507038714000373e-07f;
            float b_1 = float((region_hash_0(h1_0 + 2654435769U)) >> int(8)) * 3.74507038714000373e-07f;
            float ra_0 = (F32_sqrt((1.0f - u_3)));
            float rb_0 = (F32_sqrt((u_3)));
            float3  t0_0;
            float3  t1_0;
            float3  t2_0;
            quat_columns_0(make_float4 (ra_0 * (F32_sin((a_2))), ra_0 * (F32_cos((a_2))), rb_0 * (F32_sin((b_1))), rb_0 * (F32_cos((b_1)))), &t0_0, &t1_0, &t2_0);
            float3  c0_1;
            float3  c1_1;
            float3  c2_1;
            quat_columns_0(quat_1, &c0_1, &c1_1, &c2_1);
            float3  s_1 = exp_0(log_scale_0);
            float3  _S47 = c0_1 * make_float3 (s_1.x);
            c0_1 = _S47;
            float3  _S48 = c1_1 * make_float3 (s_1.y);
            c1_1 = _S48;
            float3  _S49 = c2_1 * make_float3 (s_1.z);
            c2_1 = _S49;
            float3  _S50 = _S47 * make_float3 (t1_0.x) + _S48 * make_float3 (t1_0.y) + _S49 * make_float3 (t1_0.z);
            float3  _S51 = _S47 * make_float3 (t2_0.x) + _S48 * make_float3 (t2_0.y) + _S49 * make_float3 (t2_0.z);
            a0_0 = _S47 * make_float3 (t0_0.x) + _S48 * make_float3 (t0_0.y) + _S49 * make_float3 (t0_0.z);
            a1_0 = _S50;
            a2_0 = _S51;
        }
        float3  p_6;
        if(k_0 > 0U)
        {
            float3  z_2 = float3 {(*(pattern_0 + (k_0 - 1U))).x, (*(pattern_0 + (k_0 - 1U))).y, (*(pattern_0 + (k_0 - 1U))).z};
            p_6 = mean_0 + a0_0 * make_float3 (z_2.x) + a1_0 * make_float3 (z_2.y) + a2_0 * make_float3 (z_2.z);
        }
        else
        {
            p_6 = mean_0;
        }
        bool _S52 = region_contains(p_6, n_5, has_n_4, program_1, num_prog_1, bvh_4, seeds_4, num_field_nodes_1);
        if(_S52)
        {
            return true;
        }
        k_0 = k_0 + 1U;
    }
    return false;
}

inline __device__ float3  splat_normal(float4  quat_2, float3  log_scale_1, float3  mean_1, float3  toward_0)
{
    float3  c0_2;
    float3  c1_2;
    float3  c2_2;
    quat_columns_0(quat_2, &c0_2, &c1_2, &c2_2);
    float3  _S53 = c0_2;
    float _S54 = log_scale_1.y;
    float _S55 = log_scale_1.x;
    bool _S56;
    if(_S54 < _S55)
    {
        _S56 = _S54 <= (log_scale_1.z);
    }
    else
    {
        _S56 = false;
    }
    float3  axis_0;
    if(_S56)
    {
        axis_0 = c1_2;
    }
    else
    {
        float _S57 = log_scale_1.z;
        if(_S57 < _S55)
        {
            _S56 = _S57 < _S54;
        }
        else
        {
            _S56 = false;
        }
        if(_S56)
        {
            axis_0 = c2_2;
        }
        else
        {
            axis_0 = _S53;
        }
    }
    if((dot_0(axis_0, toward_0 - mean_1)) < 0.0f)
    {
        axis_0 = - axis_0;
    }
    return axis_0;
}

