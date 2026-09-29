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

inline __device__ float aabb_d2_0(float3  p_0, float3  lo_0, float3  hi_0)
{
    float3  d_0 = max_0(max_0(lo_0 - p_0, p_0 - hi_0), make_float3 (0.0f));
    return dot_0(d_0, d_0);
}

inline __device__ float length_0(float3  x_2)
{
    return (F32_sqrt((dot_0(x_2, x_2))));
}

inline __device__ float3  normalize_0(float3  x_3)
{
    return x_3 / make_float3 (length_0(x_3));
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
        uint i_2;
        if(count_0 > 0U)
        {
            best_1 = best_0;
            label_1 = label_0;
            i_2 = first_0;
            for(;;)
            {
                if(i_2 < (first_0 + count_0))
                {
                }
                else
                {
                    break;
                }
                float4  * _S8 = seeds_0 + i_2;
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
                i_2 = i_2 + 1U;
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
                i_2 = first_0 + 1U;
            }
            else
            {
                i_2 = first_0;
            }
            uint sp_2;
            if((sp_1 + 2U) <= 48U)
            {
                uint sp_3 = sp_1 + 1U;
                stack_0[sp_1] = i_2;
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
    bool b_0;
    FixedArray<bool, 32>  stack_1;
    uint i_3 = 0U;
    uint sp_4 = 0U;
    for(;;)
    {
        if(i_3 < num_prog_0)
        {
        }
        else
        {
            break;
        }
        uint _S14 = i_3 * 6U;
        float4  * _S15 = program_0 + _S14;
        float4  h_0 = *_S15;
        uint type_0 = uint((*_S15).x);
        uint sp_5;
        bool v_1;
        if(type_0 == 0U)
        {
            float4  _S16 = *(program_0 + (_S14 + 2U));
            float3  _S17 = p_3 - float3 {(*(program_0 + (_S14 + 1U))).x, (*(program_0 + (_S14 + 1U))).y, (*(program_0 + (_S14 + 1U))).z};
            v_1 = true;
            sp_5 = 0U;
            for(;;)
            {
                if(sp_5 < 3U)
                {
                }
                else
                {
                    break;
                }
                if((F32_abs((dot_0(float3 {(*(program_0 + (_S14 + 3U + sp_5))).x, (*(program_0 + (_S14 + 3U + sp_5))).y, (*(program_0 + (_S14 + 3U + sp_5))).z}, _S17)))) > _slang_vector_get_element(_S16, sp_5))
                {
                    v_1 = false;
                }
                sp_5 = sp_5 + 1U;
            }
            sp_5 = sp_4;
        }
        else
        {
            if(type_0 == 1U)
            {
                float4  * _S18 = program_0 + (_S14 + 1U);
                float3  d_2 = p_3 - float3 {(*_S18).x, (*_S18).y, (*_S18).z};
                float _S19 = (*_S18).w;
                v_1 = (dot_0(d_2, d_2)) <= (_S19 * _S19);
                sp_5 = sp_4;
            }
            else
            {
                if(type_0 == 2U)
                {
                    float4  * _S20 = program_0 + (_S14 + 1U);
                    v_1 = (dot_0(float3 {(*_S20).x, (*_S20).y, (*_S20).z}, p_3) + (*_S20).w) >= 0.0f;
                    sp_5 = sp_4;
                }
                else
                {
                    if(type_0 == 3U)
                    {
                        uint _S21 = label_field_nearest(p_3, n_3, has_n_2, bvh_1, seeds_1, num_field_nodes_0);
                        v_1 = _S21 == uint(h_0.w);
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
                                    uint sp_8;
                                    bool a_1;
                                    if(sp_5 > 0U)
                                    {
                                        uint sp_9 = sp_5 - 1U;
                                        a_1 = stack_1[sp_9];
                                        sp_8 = sp_9;
                                    }
                                    else
                                    {
                                        a_1 = false;
                                        sp_8 = sp_5;
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
                                    sp_5 = sp_8;
                                }
                            }
                        }
                    }
                }
            }
        }
        if(sp_5 < 32U)
        {
            uint _S22 = sp_5 + 1U;
            stack_1[sp_5] = v_1;
            sp_4 = _S22;
        }
        else
        {
            sp_4 = sp_5;
        }
        i_3 = i_3 + 1U;
    }
    if(sp_4 > 0U)
    {
        b_0 = stack_1[sp_4 - 1U];
    }
    else
    {
        b_0 = false;
    }
    return b_0;
}

inline __device__ float3  splat_normal(float4  quat_0, float3  log_scale_0, float3  mean_0, float3  toward_0)
{
    float w_0 = quat_0.x;
    float x_4 = quat_0.y;
    float y_2 = quat_0.z;
    float z_0 = quat_0.w;
    float inv_0 = (F32_rsqrt(((F32_max((w_0 * w_0 + x_4 * x_4 + y_2 * y_2 + z_0 * z_0), (9.99999968265522539e-21f))))));
    float w_1 = w_0 * inv_0;
    float x_5 = x_4 * inv_0;
    float y_3 = y_2 * inv_0;
    float z_1 = z_0 * inv_0;
    float _S23 = y_3 * y_3;
    float _S24 = z_1 * z_1;
    float _S25 = x_5 * y_3;
    float _S26 = z_1 * w_1;
    float _S27 = x_5 * z_1;
    float _S28 = y_3 * w_1;
    float3  c0_0 = make_float3 (1.0f - 2.0f * (_S23 + _S24), 2.0f * (_S25 + _S26), 2.0f * (_S27 - _S28));
    float _S29 = x_5 * x_5;
    float _S30 = y_3 * z_1;
    float _S31 = x_5 * w_1;
    float3  c1_0 = make_float3 (2.0f * (_S25 - _S26), 1.0f - 2.0f * (_S29 + _S24), 2.0f * (_S30 + _S31));
    float3  c2_0 = make_float3 (2.0f * (_S27 + _S28), 2.0f * (_S30 - _S31), 1.0f - 2.0f * (_S29 + _S23));
    float _S32 = log_scale_0.y;
    float _S33 = log_scale_0.x;
    bool _S34;
    if(_S32 < _S33)
    {
        _S34 = _S32 <= (log_scale_0.z);
    }
    else
    {
        _S34 = false;
    }
    float3  axis_0;
    if(_S34)
    {
        axis_0 = c1_0;
    }
    else
    {
        float _S35 = log_scale_0.z;
        if(_S35 < _S33)
        {
            _S34 = _S35 < _S32;
        }
        else
        {
            _S34 = false;
        }
        if(_S34)
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

